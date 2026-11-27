// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Audit_test.cpp
 *
 * @brief Merge20 (best-workflow B11) unit tests for the append-only
 * audit log.
 *
 * The log is a pure module, so these tests run in the UnitTests
 * runner on CI. What is pinned here:
 * - one entry serializes to exactly one JSON line, and parses
 *   back to the same entry (all fields: time, file, kind, old,
 *   new, operator, note, diff index);
 * - the kind / review / conclusion code names round-trip, and an
 *   unknown kind name is rejected (a bad line is skipped, never
 *   fatal);
 * - appending two entries and loading the file back yields both
 *   entries in order; a malformed line in the middle is skipped
 *   and counted, and the entries around it still load (a torn
 *   last line from a crash mid-write costs one entry only);
 * - a no-op entry (old == new, empty note) is not a change,
 *   except for a learning dismissal, which is a change even
 *   when the conclusion did not move (the candidate disappearing
 *   is the change);
 * - the CSV text is CRLF-joined, every text field is quoted
 *   with embedded quotes doubled (S6 rule), and the CSV file
 *   starts with the UTF-8 BOM bytes (acceptance (d) at the byte
 *   level; reading it in Excel is a real-machine check).
 *
 * This file is ASCII; Japanese text is written with \\u escapes.
 */
#include "pch.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <tchar.h>
#include "Merge20/Merge20Audit.h"

namespace
{

Merge20AuditEntry MakeEntry(Merge20AuditKind kind = Merge20AuditKind::ReviewState)
{
	Merge20AuditEntry entry;
	entry.time = L"2026-11-28 00:21:00";
	entry.relPath = L"src\\foo.cpp";
	entry.kind = kind;
	entry.oldValue = L"TODO";
	entry.newValue = L"DONE";
	entry.operatorId = L"u-01234567-89ab-cdef-0123-456789abcdef";
	entry.note = L"include OFF->ON";
	entry.diffIndex = -1;
	return entry;
}

// Unique temp path for one test (removed by the test when done).
std::filesystem::path TempAuditPath(const char* name)
{
	return std::filesystem::temp_directory_path() / name;
}

String TempAuditPathString(const char* name)
{
	return TempAuditPath(name).wstring();
}

void RemoveTempFile(const char* name)
{
	std::error_code ec;
	std::filesystem::remove(TempAuditPath(name), ec);
}

std::vector<Merge20AuditCsvRow> MakeCsvRows()
{
	return {
		{ L"2026-11-28 00:21:00", L"src\\foo.cpp", L"ReviewState", L"TODO", L"DONE", L"u-1", L"include OFF->ON" },
		{ L"2026-11-28 00:22:00", L"src\\a,\"b\".cpp", L"BatchCheck", L"OFF", L"ON", L"u-1", L"" },
	};
}

std::vector<String> CsvHeaders()
{
	return { L"Date/time", L"File", L"Operation", L"Old value", L"New value", L"Operator", L"Note" };
}

TEST(Merge20Audit, EntryJsonRoundTrip)
{
	const Merge20AuditEntry entry = MakeEntry(Merge20AuditKind::DiffConclusion);
	Merge20AuditEntry parsed;
	// A diff conclusion carries the diff index; the review-state
	// linkage that also moved is in the note.
	Merge20AuditEntry withDiff = entry;
	withDiff.diffIndex = 2;
	withDiff.oldValue = L"Undecided";
	withDiff.newValue = L"UseOld";
	std::string json;
	ASSERT_TRUE(Merge20AuditEntryToJson(withDiff, json));
	// Exactly one line: no embedded newline.
	EXPECT_EQ(json.find('\n'), std::string::npos);
	std::string errDetail;
	ASSERT_TRUE(Merge20AuditEntryFromJson(json, parsed, errDetail)) << errDetail;
	EXPECT_EQ(parsed.time, withDiff.time);
	EXPECT_EQ(parsed.relPath, withDiff.relPath);
	EXPECT_EQ(parsed.kind, Merge20AuditKind::DiffConclusion);
	EXPECT_EQ(parsed.oldValue, L"Undecided");
	EXPECT_EQ(parsed.newValue, L"UseOld");
	EXPECT_EQ(parsed.operatorId, withDiff.operatorId);
	EXPECT_EQ(parsed.note, withDiff.note);
	EXPECT_EQ(parsed.diffIndex, 2);
}

TEST(Merge20Audit, EntryJsonEscapesSpecialCharacters)
{
	Merge20AuditEntry entry = MakeEntry();
	entry.relPath = L"src\\a\"b\nc.cpp";
	entry.note = L"tab\there";
	std::string json;
	ASSERT_TRUE(Merge20AuditEntryToJson(entry, json));
	Merge20AuditEntry parsed;
	std::string errDetail;
	ASSERT_TRUE(Merge20AuditEntryFromJson(json, parsed, errDetail)) << errDetail;
	EXPECT_EQ(parsed.relPath, entry.relPath);
	EXPECT_EQ(parsed.note, entry.note);
}

TEST(Merge20Audit, KindNamesRoundTrip)
{
	const Merge20AuditKind kinds[] = {
		Merge20AuditKind::ReviewState, Merge20AuditKind::IncludeInOutput,
		Merge20AuditKind::DiffConclusion, Merge20AuditKind::BatchCheck,
		Merge20AuditKind::LearnAccept, Merge20AuditKind::LearnDismiss,
		Merge20AuditKind::SplitDiff, Merge20AuditKind::RegroupDiff,
		Merge20AuditKind::ChangeReset,
	};
	for (Merge20AuditKind kind : kinds)
	{
		Merge20AuditKind parsed = Merge20AuditKind::ReviewState;
		ASSERT_TRUE(Merge20AuditKindFromName(Merge20AuditKindName(kind), parsed));
		EXPECT_EQ(parsed, kind);
	}
	Merge20AuditKind parsed = Merge20AuditKind::ReviewState;
	EXPECT_FALSE(Merge20AuditKindFromName(L"NotAKind", parsed));
}

TEST(Merge20Audit, ReviewAndConclusionCodesRoundTrip)
{
	for (ReviewState state : { ReviewState::TODO, ReviewState::DONE, ReviewState::HOLD,
		ReviewState::SPECIAL, ReviewState::KEEP })
	{
		ReviewState parsed = ReviewState::TODO;
		ASSERT_TRUE(Merge20AuditReviewFromCode(Merge20AuditReviewCode(state), parsed));
		EXPECT_EQ(parsed, state);
	}
	ReviewState badReview = ReviewState::TODO;
	EXPECT_FALSE(Merge20AuditReviewFromCode(L"MAYBE", badReview));
	for (Merge20DiffConclusion conclusion : { Merge20DiffConclusion::Undecided,
		Merge20DiffConclusion::UseOld, Merge20DiffConclusion::UseNew,
		Merge20DiffConclusion::UseBoth, Merge20DiffConclusion::Reject,
		Merge20DiffConclusion::Keep })
	{
		Merge20DiffConclusion parsed = Merge20DiffConclusion::Undecided;
		ASSERT_TRUE(Merge20AuditConclusionFromCode(Merge20AuditConclusionCode(conclusion), parsed));
		EXPECT_EQ(parsed, conclusion);
	}
	EXPECT_EQ(Merge20AuditIncludeCode(true), L"ON");
	EXPECT_EQ(Merge20AuditIncludeCode(false), L"OFF");
}

TEST(Merge20Audit, EntryFromJsonRejectsUnknownKind)
{
	Merge20AuditEntry parsed;
	std::string errDetail;
	EXPECT_FALSE(Merge20AuditEntryFromJson(
		"{\"time\":\"2026-11-28 00:21:00\",\"file\":\"a.cpp\",\"kind\":\"Nope\",\"old\":\"\",\"new\":\"\",\"operator\":\"u-1\",\"note\":\"\",\"diff\":-1}",
		parsed, errDetail));
	EXPECT_FALSE(errDetail.empty());
	EXPECT_FALSE(Merge20AuditEntryFromJson("not json at all", parsed, errDetail));
}

TEST(Merge20Audit, EntryFromJsonToleratesMissingOptionalFields)
{
	Merge20AuditEntry parsed;
	std::string errDetail;
	ASSERT_TRUE(Merge20AuditEntryFromJson(
		"{\"time\":\"2026-11-28 00:21:00\",\"file\":\"a.cpp\",\"kind\":\"BatchCheck\",\"old\":\"OFF\",\"new\":\"ON\",\"operator\":\"u-1\"}",
		parsed, errDetail)) << errDetail;
	EXPECT_EQ(parsed.note, L"");
	EXPECT_EQ(parsed.diffIndex, -1);
	// Unknown fields are ignored (forward compatibility).
	ASSERT_TRUE(Merge20AuditEntryFromJson(
		"{\"time\":\"2026-11-28 00:21:00\",\"file\":\"a.cpp\",\"kind\":\"BatchCheck\",\"old\":\"OFF\",\"new\":\"ON\",\"operator\":\"u-1\",\"note\":\"\",\"diff\":-1,\"future\":123}",
		parsed, errDetail)) << errDetail;
	EXPECT_EQ(parsed.kind, Merge20AuditKind::BatchCheck);
}

TEST(Merge20Audit, NoopEntriesAreNotChangesExceptDismissal)
{
	Merge20AuditEntry noop = MakeEntry();
	noop.newValue = noop.oldValue;
	noop.note.clear();
	EXPECT_FALSE(Merge20AuditEntryHasChange(noop));
	// A review change that also moved the output flag is a
	// change even when the review itself did not move.
	Merge20AuditEntry linked = MakeEntry();
	linked.newValue = linked.oldValue;
	linked.note = L"include OFF->ON";
	EXPECT_TRUE(Merge20AuditEntryHasChange(linked));
	// A dismissal is a change even when the conclusion did not
	// move: the candidate disappearing is the change.
	Merge20AuditEntry dismiss = MakeEntry(Merge20AuditKind::LearnDismiss);
	dismiss.newValue = dismiss.oldValue;
	dismiss.note.clear();
	EXPECT_TRUE(Merge20AuditEntryHasChange(dismiss));
}

TEST(Merge20Audit, AppendAndLoadKeepsOrder)
{
	const char* name = "Merge20Audit_test_append.json";
	RemoveTempFile(name);
	const String path = TempAuditPathString(name);
	std::string errDetail;
	ASSERT_TRUE(Merge20AuditAppendToFile(MakeEntry(), path, errDetail)) << errDetail;
	Merge20AuditEntry second = MakeEntry(Merge20AuditKind::BatchCheck);
	second.time = L"2026-11-28 00:22:00";
	second.oldValue = L"OFF";
	second.newValue = L"ON";
	ASSERT_TRUE(Merge20AuditAppendToFile(second, path, errDetail)) << errDetail;
	std::vector<Merge20AuditEntry> loaded;
	int skipped = -1;
	ASSERT_TRUE(Merge20AuditLoadFromFile(path, loaded, skipped, errDetail)) << errDetail;
	EXPECT_EQ(skipped, 0);
	ASSERT_EQ(loaded.size(), 2u);
	EXPECT_EQ(loaded[0].kind, Merge20AuditKind::ReviewState);
	EXPECT_EQ(loaded[0].time, L"2026-11-28 00:21:00");
	EXPECT_EQ(loaded[1].kind, Merge20AuditKind::BatchCheck);
	EXPECT_EQ(loaded[1].time, L"2026-11-28 00:22:00");
	EXPECT_EQ(loaded[1].oldValue, L"OFF");
	RemoveTempFile(name);
}

TEST(Merge20Audit, LoadSkipsBadLineAndKeepsTheRest)
{
	const char* name = "Merge20Audit_test_badline.json";
	RemoveTempFile(name);
	const String path = TempAuditPathString(name);
	std::string errDetail;
	ASSERT_TRUE(Merge20AuditAppendToFile(MakeEntry(), path, errDetail)) << errDetail;
	{
		// A torn line (crash mid-write) and a hand-edited bad
		// line: both are skipped, the entries around them load.
		std::ofstream out(TempAuditPath(name), std::ios::binary | std::ios::app);
		out << "{\"time\":\"2026-11-28 00:22:00\",\"file\":\"torn";
		out << "\nnot json\n";
	}
	Merge20AuditEntry second = MakeEntry(Merge20AuditKind::ChangeReset);
	second.time = L"2026-11-28 00:23:00";
	ASSERT_TRUE(Merge20AuditAppendToFile(second, path, errDetail)) << errDetail;
	std::vector<Merge20AuditEntry> loaded;
	int skipped = -1;
	ASSERT_TRUE(Merge20AuditLoadFromFile(path, loaded, skipped, errDetail)) << errDetail;
	EXPECT_EQ(skipped, 2);
	ASSERT_EQ(loaded.size(), 2u);
	EXPECT_EQ(loaded[0].kind, Merge20AuditKind::ReviewState);
	EXPECT_EQ(loaded[1].kind, Merge20AuditKind::ChangeReset);
	RemoveTempFile(name);
}

TEST(Merge20Audit, LoadMissingFileFails)
{
	const char* name = "Merge20Audit_test_missing.json";
	RemoveTempFile(name);
	std::vector<Merge20AuditEntry> loaded;
	int skipped = -1;
	std::string errDetail;
	EXPECT_FALSE(Merge20AuditLoadFromFile(TempAuditPathString(name), loaded, skipped, errDetail));
	EXPECT_FALSE(errDetail.empty());
	EXPECT_TRUE(loaded.empty());
}

TEST(Merge20Audit, CsvTextIsCrlfJoinedAndQuoted)
{
	const String text = Merge20AuditBuildCsvText(MakeCsvRows(), CsvHeaders());
	// Header first, then one CRLF-joined row per entry, no
	// trailing line ending.
	EXPECT_EQ(text.find(L"Date/time,File,Operation,Old value,New value,Operator,Note"), 0u);
	// Every line ending is CRLF: no bare LF anywhere.
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == L'\n')
			ASSERT_GT(i, 0u) << "bare LF at offset " << i;
		if (text[i] == L'\n')
			EXPECT_EQ(text[i - 1], L'\r');
	}
	// The comma and the quote inside the file name are quoted
	// (S6 rule: every text field double-quoted, quotes doubled).
	EXPECT_NE(text.find(L"\"src\\a,\"\"b\"\".cpp\""), String::npos);
	// Two rows: exactly two CRLFs (header + row 1 separator,
	// row 1 + row 2 separator).
	size_t crlf = 0, pos = 0;
	while ((pos = text.find(L"\r\n", pos)) != String::npos)
	{
		++crlf;
		pos += 2;
	}
	EXPECT_EQ(crlf, 2u);
}

TEST(Merge20Audit, CsvFileStartsWithBom)
{
	const char* name = "Merge20Audit_test_export.csv";
	RemoveTempFile(name);
	const String path = TempAuditPathString(name);
	std::string errDetail;
	ASSERT_TRUE(Merge20AuditWriteCsvFile(MakeCsvRows(), CsvHeaders(), path, errDetail)) << errDetail;
	std::ifstream in(TempAuditPath(name), std::ios::binary);
	ASSERT_TRUE(in.good());
	std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	ASSERT_GE(bytes.size(), 3u);
	// UTF-8 BOM, then the header line.
	EXPECT_EQ(static_cast<unsigned char>(bytes[0]), 0xEFu);
	EXPECT_EQ(static_cast<unsigned char>(bytes[1]), 0xBBu);
	EXPECT_EQ(static_cast<unsigned char>(bytes[2]), 0xBFu);
	EXPECT_NE(bytes.find("Date/time,File,Operation,Old value,New value,Operator,Note"), std::string::npos);
	EXPECT_NE(bytes.find("\r\n"), std::string::npos);
	RemoveTempFile(name);
}

} // namespace
