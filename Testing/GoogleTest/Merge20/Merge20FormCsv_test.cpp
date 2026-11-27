/**
 * @file Merge20FormCsv_test.cpp
 *
 * @brief Merge20 (Wave 5 F7) unit tests for the FORM compare CSV
 * export: the two-part structure and column counts (design
 * section 9, acceptance test (a)), the moved/renamed vocabulary,
 * the group and minor columns, CSV escaping (S6 rule), the
 * output-range selection, the F0-F6 end-to-end integration, and
 * the 1000-file analysis timing (design section 10 F7 (b),
 * T-F-6).
 *
 * The real-file acceptance test (F7 (b)) cannot run here: the
 * user's actual form-definition XML files are not available, so
 * synthetic XML is used and the real-file test stays "waiting
 * for the user's real files" (see the F7 PR description).
 *
 * This file stays ASCII: the Japanese test text is written with \u
 * escapes, like Merge20FormXml_test.cpp.
*/
#include "pch.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>
#include "Merge20/Merge20FormCsv.h"
#include "Merge20/Merge20FormDiff.h"
#include "Merge20/Merge20FormImpact.h"
#include "Merge20/Merge20FormMaster.h"
#include "Merge20/Merge20FormTable.h"

namespace
{

	Merge20FormDiffOutcome DiffPair(const wchar_t* oldXml, const wchar_t* newXml, const wchar_t* fileName = L"test.xml")
	{
		const std::wstring oldText(oldXml);
		const std::wstring newText(newXml);
		return Merge20DiffFormFiles(&oldText, &newText, fileName);
	}

	bool Contains(const std::wstring& text, const wchar_t* fragment)
	{
		return text.find(fragment) != std::wstring::npos;
	}

	// Split the built CSV text on CRLF (the trailing CRLF leaves
	// an empty last piece, dropped here).
	std::vector<std::wstring> SplitLines(const std::wstring& text)
	{
		std::vector<std::wstring> lines;
		size_t pos = 0;
		while (pos < text.size())
		{
			const size_t eol = text.find(L"\r\n", pos);
			if (eol == std::wstring::npos)
			{
				lines.push_back(text.substr(pos));
				break;
			}
			lines.push_back(text.substr(pos, eol - pos));
			pos = eol + 2;
		}
		return lines;
	}

	// Parse one CSV line into fields (quoted fields unescaped:
	// `""` becomes `"`). Bare numbers come back as-is.
	std::vector<std::wstring> ParseCsvLine(const std::wstring& line)
	{
		std::vector<std::wstring> fields;
		std::wstring field;
		bool inQuotes = false;
		for (size_t i = 0; i < line.size(); ++i)
		{
			const wchar_t ch = line[i];
			if (inQuotes)
			{
				if (ch == L'"')
				{
					if (i + 1 < line.size() && line[i + 1] == L'"')
					{
						field += L'"';
						++i;
					}
					else
					inQuotes = false;
				}
				else
				field += ch;
			}
			else if (ch == L'"')
				inQuotes = true;
			else if (ch == L',')
			{
				fields.push_back(field);
				field.clear();
			}
			else
			field += ch;
		}
		fields.push_back(field);
		return fields;
	}

	// One row of five fields (design section 5.3 test 1): the row
	// moves down 12 units. Shared with Merge20FormTable_test.
	const wchar_t* const kRowOldXml =
		L"<form>"
	L"<row name=\"row1\">"
	L"<field name=\"a\" x=\"10\" y=\"60\" width=\"100\" height=\"20\" />"
	L"<field name=\"b\" x=\"120\" y=\"60\" width=\"100\" height=\"20\" />"
	L"<field name=\"c\" x=\"230\" y=\"60\" width=\"100\" height=\"20\" />"
	L"<field name=\"d\" x=\"340\" y=\"60\" width=\"100\" height=\"20\" />"
	L"<field name=\"e\" x=\"450\" y=\"60\" width=\"100\" height=\"20\" />"
	L"</row>"
	L"</form>";

	const wchar_t* const kRowNewXml =
		L"<form>"
	L"<row name=\"row1\">"
	L"<field name=\"a\" x=\"10\" y=\"72\" width=\"100\" height=\"20\" />"
	L"<field name=\"b\" x=\"120\" y=\"72\" width=\"100\" height=\"20\" />"
	L"<field name=\"c\" x=\"230\" y=\"72\" width=\"100\" height=\"20\" />"
	L"<field name=\"d\" x=\"340\" y=\"72\" width=\"100\" height=\"20\" />"
	L"<field name=\"e\" x=\"450\" y=\"72\" width=\"100\" height=\"20\" />"
	L"</row>"
	L"</form>";

	// One field, moved down 12 units (a single COORD record).
	const wchar_t* const kCoordOldXml =
		L"<form><field name=\"f1\" x=\"10\" y=\"60\" width=\"50\" height=\"10\" /></form>";
	const wchar_t* const kCoordNewXml =
		L"<form><field name=\"f1\" x=\"10\" y=\"72\" width=\"50\" height=\"10\" /></form>";

	// One field, moved 0.7 units (a minor difference, section 5.2).
	const wchar_t* const kMinorOldXml =
		L"<form><field name=\"f1\" x=\"10\" y=\"60\" width=\"50\" height=\"10\" /></form>";
	const wchar_t* const kMinorNewXml =
		L"<form><field name=\"f1\" x=\"10\" y=\"60.7\" width=\"50\" height=\"10\" /></form>";

	// Build a one-file CSV input from a diff outcome.
	void OneFileInput(const Merge20FormDiffOutcome& outcome,
		std::vector<Merge20FormFileResult>& results,
		std::vector<std::vector<Merge20FormDiffGroup>>& groups)
	{
		results.push_back(outcome.result);
		groups.push_back(outcome.groups);
	}

} // namespace

// Design section 9 / F7 (a): the CSV is two parts, part 1 has 9
// columns, part 2 has 14 columns, every line ends with CRLF.
TEST(Merge20FormCsv, TwoPartStructureAndColumns)
{
	Merge20FormDiffOutcome outcome = DiffPair(kCoordOldXml, kCoordNewXml);
	ASSERT_EQ(outcome.result.changeCount, 1);
	std::vector<Merge20FormFileResult> results;
	std::vector<std::vector<Merge20FormDiffGroup>> groups;
	OneFileInput(outcome, results, groups);
	const std::wstring text = Merge20FormCsvBuild(results, groups, { 0 }, L"2026-11-27 22:00:00");
	// No bare LF: every newline is part of a CRLF pair.
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == L'\n')
			ASSERT_TRUE(i > 0 && text[i - 1] == L'\r') << "bare LF at " << i;
	}
	const std::vector<std::wstring> lines = SplitLines(text);
	ASSERT_EQ(lines.size(), 10u);
	EXPECT_TRUE(Contains(lines[0], L"\u25a0 XML\u30d5\u30a9\u30fc\u30e0\u5b9a\u7fa9 \u5909\u66f4\u30fb\u5dee\u7570\u6bd4\u8f03 \u7dcf\u5408\u30ec\u30dd\u30fc\u30c8"));
	EXPECT_TRUE(Contains(lines[0], L"\u51fa\u529b\u65e5\u6642: 2026-11-27 22:00:00"));
	EXPECT_TRUE(lines[1].empty());
	EXPECT_TRUE(Contains(lines[2], L"\u5909\u66f4\u30fb\u5dee\u7570\u691c\u51fa \u5e33\u7968\u30b5\u30de\u30ea\u4e00\u89a7"));
	EXPECT_EQ(ParseCsvLine(lines[3]).size(), static_cast<size_t>(kMerge20FormCsvSummaryColumnCount));
	EXPECT_EQ(ParseCsvLine(lines[4]).size(), static_cast<size_t>(kMerge20FormCsvSummaryColumnCount));
	EXPECT_TRUE(lines[5].empty());
	EXPECT_TRUE(lines[6].empty());
	EXPECT_TRUE(Contains(lines[7], L"\u5909\u66f4\u7b87\u6240\u5225 \u8a73\u7d30\u6bd4\u8f03\u30fb\u5f71\u97ff\u5206\u6790\u4e00\u89a7"));
	EXPECT_EQ(ParseCsvLine(lines[8]).size(), static_cast<size_t>(kMerge20FormCsvDetailColumnCount));
	EXPECT_EQ(ParseCsvLine(lines[9]).size(), static_cast<size_t>(kMerge20FormCsvDetailColumnCount));
}

// Design section 9: part 1 row values (warning word, severity,
// one-line summary) and part 2 row values (type, element,
// old/new, integer change, group/minor empty for a single change).
TEST(Merge20FormCsv, RowValues)
{
	Merge20FormDiffOutcome outcome = DiffPair(kCoordOldXml, kCoordNewXml);
	outcome.result.formName = L"\u8cc7\u7523\u8a3c\u660e\u66f8";
	outcome.result.severity = Merge20ClassifyFormSeverity(outcome.result);
	std::vector<Merge20FormFileResult> results;
	std::vector<std::vector<Merge20FormDiffGroup>> groups;
	OneFileInput(outcome, results, groups);
	const std::wstring text = Merge20FormCsvBuild(results, groups, { 0 }, L"2026-11-27 22:00:00");
	const std::vector<std::wstring> lines = SplitLines(text);
	const std::vector<std::wstring> summary = ParseCsvLine(lines[4]);
	ASSERT_EQ(summary.size(), 9u);
	EXPECT_EQ(summary[0], L"1");
	EXPECT_EQ(summary[1], L"\u901a\u5e38");
	EXPECT_EQ(summary[2], L"test.xml");
	EXPECT_EQ(summary[3], L"\u8cc7\u7523\u8a3c\u660e\u66f8");
	EXPECT_FALSE(summary[4].empty()); // tendency
	EXPECT_EQ(summary[5], L"12"); // numeric total, integer
	EXPECT_EQ(summary[6], L"1"); // change count
	EXPECT_EQ(summary[7], L"\u898b\u305f\u76ee\u3060\u3051"); // COORD only: VisualOnly
	EXPECT_FALSE(summary[8].empty()); // one-line summary
	const std::vector<std::wstring> detail = ParseCsvLine(lines[9]);
	ASSERT_EQ(detail.size(), 14u);
	EXPECT_EQ(detail[0], L"1");
	EXPECT_EQ(detail[1], L"test.xml");
	EXPECT_EQ(detail[2], L"\u8cc7\u7523\u8a3c\u660e\u66f8");
	EXPECT_EQ(detail[3], L"\u901a\u5e38");
	EXPECT_EQ(detail[4], L"COORD");
	EXPECT_EQ(detail[5], L"f1");
	EXPECT_EQ(detail[6], L"y");
	EXPECT_EQ(detail[7], L"60");
	EXPECT_EQ(detail[8], L"72");
	EXPECT_EQ(detail[9], L"12"); // integer change
	EXPECT_FALSE(detail[10].empty()); // concrete change
	EXPECT_FALSE(detail[11].empty()); // impact analysis
	EXPECT_EQ(detail[12], L""); // no group: single change
	EXPECT_EQ(detail[13], L""); // not minor
}

// Design sections 4.2 and 9: Moved / Renamed use the words
// "\u79fb\u52d5" / "\u6539\u540d" in the CSV change-type column.
TEST(Merge20FormCsv, MoveRenameVocabulary)
{
	Merge20FormDiffOutcome moved = DiffPair(
		L"<form><page name=\"p1\"><field name=\"f1\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></page>"
		L"<page name=\"p2\" /></form>",
		L"<form><page name=\"p1\" /><page name=\"p2\">"
		L"<field name=\"f1\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></page></form>");
	ASSERT_EQ(moved.result.changeCount, 1);
	Merge20FormDiffOutcome renamed = DiffPair(
		L"<form><field name=\"old\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></form>",
		L"<form><field name=\"new\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></form>");
	ASSERT_EQ(renamed.result.changeCount, 1);
	std::vector<Merge20FormFileResult> results;
	std::vector<std::vector<Merge20FormDiffGroup>> groups;
	OneFileInput(moved, results, groups);
	OneFileInput(renamed, results, groups);
	const std::wstring text = Merge20FormCsvBuild(results, groups, { 0, 1 }, L"2026-11-27 22:00:00");
	const std::vector<std::wstring> lines = SplitLines(text);
	ASSERT_EQ(lines.size(), 12u);
	EXPECT_EQ(ParseCsvLine(lines[10])[4], L"\u79fb\u52d5");
	EXPECT_EQ(ParseCsvLine(lines[11])[4], L"\u6539\u540d");
}

// Design sections 5.3 and 9: a grouped change carries the group
// summary in the group column; a minor change carries "\u5fae\u5c0f" in
// the minor column.
TEST(Merge20FormCsv, GroupAndMinorColumns)
{
	Merge20FormDiffOutcome grouped = DiffPair(kRowOldXml, kRowNewXml);
	ASSERT_EQ(grouped.groups.size(), 1u);
	ASSERT_FALSE(grouped.groups[0].summary.empty());
	Merge20FormDiffOutcome minor = DiffPair(kMinorOldXml, kMinorNewXml);
	ASSERT_EQ(minor.groups.size(), 1u);
	ASSERT_TRUE(minor.groups[0].minor);
	std::vector<Merge20FormFileResult> results;
	std::vector<std::vector<Merge20FormDiffGroup>> groups;
	OneFileInput(grouped, results, groups);
	OneFileInput(minor, results, groups);
	const std::wstring text = Merge20FormCsvBuild(results, groups, { 0, 1 }, L"2026-11-27 22:00:00");
	const std::vector<std::wstring> lines = SplitLines(text);
	// Part 2: 5 grouped records + 1 minor record.
	ASSERT_EQ(lines.size(), 16u);
	const std::vector<std::wstring> groupedRow = ParseCsvLine(lines[10]);
	EXPECT_EQ(groupedRow[12], grouped.groups[0].summary);
	EXPECT_EQ(groupedRow[13], L"");
	const std::vector<std::wstring> minorRow = ParseCsvLine(lines[15]);
	EXPECT_EQ(minorRow[4], L"COORD");
	EXPECT_EQ(minorRow[12], minor.groups[0].summary);
	EXPECT_EQ(minorRow[13], L"\u5fae\u5c0f");
}

// S6 quoting rule: text is always double-quoted, `"` doubled, so
// a comma, a quote or a newline inside a value never breaks the
// column count.
TEST(Merge20FormCsv, EscapingKeepsColumnCounts)
{
	Merge20FormFileResult result;
	result.fileName = L"a\"b,c.xml";
	result.formName = L"\u540d\"\u524d";
	result.tendency = L"\u8ffd\u52a0(1)";
	result.summary = L"line1\nline2";
	result.totalNumericSum = 3.0;
	result.changeCount = 1;
	result.severity = Merge20FormSeverity::Info;
	Merge20FormDiffRecord record;
	record.type = Merge20FormDiffType::Mod;
	record.element = L"/form|field|f1";
	record.attr = L"color";
	record.oldVal = L"a\"b";
	record.newVal = L"c,d";
	record.diffVal = 0.0;
	record.desc = L"\u8272\u304c \"\u8d64\" \u304b\u3089\u5909\u308f\u3063\u305f";
	record.impact = L"\u5f71\u97ff,\u306a\u3057";
	result.diffRecords.push_back(record);
	const std::vector<Merge20FormFileResult> results = { result };
	const std::vector<std::vector<Merge20FormDiffGroup>> groups(1);
	const std::wstring text = Merge20FormCsvBuild(results, groups, { 0 }, L"2026-11-27 22:00:00");
	const std::vector<std::wstring> lines = SplitLines(text);
	// The embedded newline in the summary is a bare LF inside a
	// quoted field (SplitLines only splits on CRLF), so the row
	// stays one line; check the raw text for the quoted value.
	EXPECT_TRUE(Contains(text, L"\"a\"\"b,c.xml\""));
	EXPECT_TRUE(Contains(text, L"\"line1\nline2\""));
	const std::vector<std::wstring> detail = ParseCsvLine(lines[9]);
	ASSERT_EQ(detail.size(), 14u);
	EXPECT_EQ(detail[5], L"f1");
	EXPECT_EQ(detail[7], L"a\"b");
	EXPECT_EQ(detail[8], L"c,d");
}

// Design section 9: the range dialog chooses the filtered view or
// all changed files; only the chosen indices reach the CSV.
TEST(Merge20FormCsv, RangeSelection)
{
	Merge20FormDiffOutcome first = DiffPair(kCoordOldXml, kCoordNewXml, L"a.xml");
	Merge20FormDiffOutcome second = DiffPair(
		L"<form><field name=\"old\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></form>",
		L"<form><field name=\"new\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></form>",
		L"b.xml");
	std::vector<Merge20FormFileResult> results;
	std::vector<std::vector<Merge20FormDiffGroup>> groups;
	OneFileInput(first, results, groups);
	OneFileInput(second, results, groups);
	// Filtered: only the second file.
	const std::wstring filtered = Merge20FormCsvBuild(results, groups, { 1 }, L"2026-11-27 22:00:00");
	const std::vector<std::wstring> filteredLines = SplitLines(filtered);
	ASSERT_EQ(filteredLines.size(), 10u);
	EXPECT_EQ(ParseCsvLine(filteredLines[4])[2], L"b.xml");
	EXPECT_EQ(ParseCsvLine(filteredLines[9])[4], L"\u6539\u540d");
	// Empty range: headers only, no rows in either part.
	const std::wstring empty = Merge20FormCsvBuild(results, groups, {}, L"2026-11-27 22:00:00");
	const std::vector<std::wstring> emptyLines = SplitLines(empty);
	ASSERT_EQ(emptyLines.size(), 8u);
}

// F0-F6 end to end (design section 10 F7): parse (F0), diff with
// grouping and move/rename (F1), the large-change flag and
// severity (F1/F5 impact), the name master (F6) and the diff
// table (F5) all feed the same CSV build.
TEST(Merge20FormCsv, IntegrationEndToEnd)
{
	std::vector<Merge20FormFileResult> results;
	std::vector<std::vector<Merge20FormDiffGroup>> groups;
	// File 1: a row move (grouped, large change).
	Merge20FormDiffOutcome rowMove = DiffPair(kRowOldXml, kRowNewXml, L"ZEB001R007.xml");
	OneFileInput(rowMove, results, groups);
	// File 2: a rename.
	Merge20FormDiffOutcome renamed = DiffPair(
		L"<form><field name=\"old\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></form>",
		L"<form><field name=\"new\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></form>",
		L"ZEB001R008.xml");
	OneFileInput(renamed, results, groups);
	// File 3: a new file (FILE_ADD).
	const std::wstring addedText =
		L"<form><field name=\"f1\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></form>";
	Merge20FormDiffOutcome added = Merge20DiffFormFiles(nullptr, &addedText, L"ZEB001R009.xml");
	OneFileInput(added, results, groups);
	ASSERT_EQ(results.size(), 3u);
	// Large-change flag + impact (F1/F5), then the name master (F6).
	Merge20ApplyLargeChangeFlag(results);
	for (Merge20FormFileResult& result : results)
		Merge20ApplyFormImpact(result, 0.0, 0.0);
	const Merge20FormMaster master = Merge20FormMasterGetDefault();
	for (Merge20FormFileResult& result : results)
		result.formName = Merge20FormMasterResolve(master, result.fileName);
	// The row move is a large change (total 60) with a group.
	EXPECT_TRUE(results[0].isWarning);
	EXPECT_EQ(results[0].severity, Merge20FormSeverity::LayoutBreaking);
	EXPECT_EQ(results[0].formName, L"\u8cc7\u7523\u8a3c\u660e\u66f8");
	EXPECT_FALSE(results[0].summary.empty());
	// F5: the diff table builds on the same result + groups.
	const std::set<std::wstring> collapsed;
	const std::vector<Merge20FormTableRow> tableRows =
		Merge20BuildFormDiffTableRows(results[0], groups[0], collapsed);
	ASSERT_EQ(tableRows.size(), 1u);
	EXPECT_TRUE(tableRows[0].isGroup);
	// The CSV over all three files.
	const std::wstring text = Merge20FormCsvBuild(results, groups, { 0, 1, 2 }, L"2026-11-27 22:00:00");
	const std::vector<std::wstring> lines = SplitLines(text);
	// Part 1: 3 rows; part 2: 5 + 1 + 1 = 7 rows.
	ASSERT_EQ(lines.size(), 18u);
	const std::vector<std::wstring> firstSummary = ParseCsvLine(lines[4]);
	EXPECT_EQ(firstSummary[1], L"\u2605\u8981\u6ce8\u610f(\u5927\u5e45\u5909\u52d5)");
	EXPECT_EQ(firstSummary[3], L"\u8cc7\u7523\u8a3c\u660e\u66f8");
	EXPECT_EQ(firstSummary[7], L"\u30ec\u30a4\u30a2\u30a6\u30c8\u7834\u58ca");
	EXPECT_FALSE(firstSummary[8].empty());
	EXPECT_EQ(ParseCsvLine(lines[11])[4], L"COORD");
	EXPECT_EQ(ParseCsvLine(lines[11])[12], groups[0][0].summary);
	EXPECT_EQ(ParseCsvLine(lines[16])[4], L"\u6539\u540d");
	EXPECT_EQ(ParseCsvLine(lines[17])[4], L"FILE_ADD");
}

// Design section 10 F7 (b) / T-F-6: 1000-file analysis timing.
// Synthetic data (the real files are not available): 1000 file
// pairs, 20 fields each, one coordinate change per file. The
// elapsed time is printed to the test log for the record; the
// bound only guards against a pathological slowdown.
TEST(Merge20FormCsv, ThousandFileAnalysisTiming)
{
	const auto start = std::chrono::steady_clock::now();
	std::vector<Merge20FormFileResult> results;
	std::vector<std::vector<Merge20FormDiffGroup>> groups;
	results.reserve(1000);
	groups.reserve(1000);
	for (int file = 0; file < 1000; ++file)
	{
		std::wstring oldXml = L"<form>";
		std::wstring newXml = L"<form>";
		for (int field = 0; field < 20; ++field)
		{
			oldXml += L"<field name=\"f" + std::to_wstring(field) +
				L"\" x=\"" + std::to_wstring(field * 60) +
				L"\" y=\"60\" width=\"50\" height=\"10\" />";
			// Field 0 moves down 12 units; the rest are identical.
			const int y = (field == 0) ? 72 : 60;
			newXml += L"<field name=\"f" + std::to_wstring(field) +
				L"\" x=\"" + std::to_wstring(field * 60) +
				L"\" y=\"" + std::to_wstring(y) +
				L"\" width=\"50\" height=\"10\" />";
		}
		oldXml += L"</form>";
		newXml += L"</form>";
		Merge20FormDiffOutcome outcome = Merge20DiffFormFiles(
			&oldXml, &newXml, L"form" + std::to_wstring(file) + L".xml");
		ASSERT_EQ(outcome.result.changeCount, 1) << "file " << file;
		results.push_back(outcome.result);
		groups.push_back(outcome.groups);
	}
	Merge20ApplyLargeChangeFlag(results);
	std::vector<size_t> indices(results.size());
	for (size_t i = 0; i < indices.size(); ++i)
		indices[i] = i;
	const std::wstring text = Merge20FormCsvBuild(results, groups, indices, L"2026-11-27 22:00:00");
	const auto end = std::chrono::steady_clock::now();
	const long long elapsedMs =
		std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	std::printf("[ FORM 1000-file] analysis + CSV of 1000 files: %lld ms\n", elapsedMs);
	EXPECT_FALSE(text.empty());
	EXPECT_EQ(results.size(), 1000u);
	EXPECT_LT(elapsedMs, 120000);
}
