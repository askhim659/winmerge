/**
 * @file Merge20VerifyReport_test.cpp
 *
 * @brief Merge20 (B8) unit tests for the verification report
 * (CSV) builder.
 *
 * Covered: the summary counts (files covered / passed /
 * failed / total issues, with an unverified file counting only
 * in "files covered"), the check table (Passed / Failed /
 * Not run for a switched-off check, elapsed ms as a bare
 * number), the issue list (1-based diff number, empty cells
 * when a problem has no diff or line, S6 escaping of quotes,
 * commas and newlines in the detail text), the unverified-file
 * row, and the byte form of the file (CRLF line endings, valid
 * UTF-8 for Japanese labels; the BOM itself is written by the
 * caller with the same UniStdioFile::WriteBom call S6 uses, so
 * the byte test asserts the content bytes the BOM precedes).
 *
 * This file stays ASCII: the Japanese test text is written with
 * \\u escapes, like Merge20Csv_test.cpp.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20VerifyReport.h"
#include "unicoder.h"

namespace
{

Merge20VerifyReportLabels TestLabels()
{
	Merge20VerifyReportLabels labels;
	labels.title = L"Verification report";
	labels.verifiedAt = L"Verification date/time";
	labels.fileCount = L"Files verified";
	labels.passedFiles = L"Files passed";
	labels.failedFiles = L"Files failed";
	labels.issueCount = L"Total issues";
	labels.checksTitle = L"Check results";
	labels.issuesTitle = L"Issue list";
	labels.headerFile = L"File name";
	labels.headerCheck = L"Check";
	labels.headerResult = L"Result";
	labels.headerIssues = L"Issues";
	labels.headerElapsed = L"Elapsed (ms)";
	labels.headerDiff = L"Diff no.";
	labels.headerResultLine = L"Result line";
	labels.headerDetail = L"Detail";
	labels.passed = L"Passed";
	labels.failed = L"Failed";
	labels.notRun = L"Not run";
	labels.notVerified = L"Not verified (no merge result built)";
	labels.checkNames[0] = L"Re-diff";
	labels.checkNames[1] = L"Lines / encoding";
	labels.checkNames[2] = L"Syntax (simple check)";
	labels.checkNames[3] = L"Judgment consistency";
	return labels;
}

Merge20VerifyCheckResult Check(Merge20VerifyCheck check, bool enabled,
	bool passed, int issues, long long elapsedMs)
{
	Merge20VerifyCheckResult result;
	result.check = check;
	result.enabled = enabled;
	result.passed = passed;
	result.issueCount = issues;
	result.elapsedMs = elapsedMs;
	return result;
}

Merge20VerifyReportFile CleanFile(const String& name)
{
	Merge20VerifyReportFile file;
	file.fileName = name;
	file.verified = true;
	file.passed = true;
	file.checks =
	{
		Check(Merge20VerifyCheck::Rediff, true, true, 0, 12),
		Check(Merge20VerifyCheck::LineCountEncoding, true, true, 0, 3),
		Check(Merge20VerifyCheck::Syntax, false, true, 0, 0),
		Check(Merge20VerifyCheck::Consistency, true, true, 0, 1),
	};
	return file;
}

Merge20VerifyReportFile FailedFile(const String& name)
{
	Merge20VerifyReportFile file;
	file.fileName = name;
	file.verified = true;
	file.passed = false;
	file.checks =
	{
		Check(Merge20VerifyCheck::Rediff, true, false, 2, 5),
		Check(Merge20VerifyCheck::LineCountEncoding, true, true, 0, 2),
		Check(Merge20VerifyCheck::Syntax, true, true, 0, 0),
		Check(Merge20VerifyCheck::Consistency, true, true, 0, 1),
	};
	Merge20VerifyReportIssue first;
	first.fileName = name;
	first.checkName = L"Re-diff";
	first.diffIndex = 2;
	first.resultLine = 42;
	first.detail = L"Unexplained difference from NEW (NEW line 7) : int x = 1;";
	file.issues.push_back(first);
	Merge20VerifyReportIssue second;
	second.fileName = name;
	second.checkName = L"Re-diff";
	second.diffIndex = -1;
	second.resultLine = -1;
	// Quotes, a comma and a newline inside the detail: the S6
	// escaping must keep this one field.
	second.detail = L"say \"hi\", bye\nnext";
	file.issues.push_back(second);
	return file;
}

Merge20VerifyReportFile UnverifiedFile(const String& name)
{
	Merge20VerifyReportFile file;
	file.fileName = name;
	file.verified = false;
	file.passed = true;
	return file;
}

} // namespace

TEST(Merge20VerifyReport, EmptyReport)
{
	Merge20VerifyReport report;
	report.verifiedAt = L"2026-11-27 21:05:33";
	const String csv = Merge20VerifyReportBuildCsv(report, TestLabels());
	EXPECT_EQ(csv,
		String(L"\"Verification report\"\r\n"
			L"\"Verification date/time\",\"2026-11-27 21:05:33\"\r\n"
			L"\"Files verified\",0\r\n"
			L"\"Files passed\",0\r\n"
			L"\"Files failed\",0\r\n"
			L"\"Total issues\",0\r\n"
			L"\r\n"
			L"\"Check results\"\r\n"
			L"\"File name\",\"Check\",\"Result\",\"Issues\",\"Elapsed (ms)\"\r\n"
			L"\r\n"
			L"\"Issue list\"\r\n"
			L"\"File name\",\"Check\",\"Diff no.\",\"Result line\",\"Detail\"\r\n"));
}

TEST(Merge20VerifyReport, GoldenCleanReport)
{
	Merge20VerifyReport report;
	report.verifiedAt = L"2026-11-27 21:05:33";
	report.files.push_back(CleanFile(L"Foo.cpp"));
	const String csv = Merge20VerifyReportBuildCsv(report, TestLabels());
	EXPECT_EQ(csv,
		String(L"\"Verification report\"\r\n"
			L"\"Verification date/time\",\"2026-11-27 21:05:33\"\r\n"
			L"\"Files verified\",1\r\n"
			L"\"Files passed\",1\r\n"
			L"\"Files failed\",0\r\n"
			L"\"Total issues\",0\r\n"
			L"\r\n"
			L"\"Check results\"\r\n"
			L"\"File name\",\"Check\",\"Result\",\"Issues\",\"Elapsed (ms)\"\r\n"
			L"\"Foo.cpp\",\"Re-diff\",\"Passed\",0,12\r\n"
			L"\"Foo.cpp\",\"Lines / encoding\",\"Passed\",0,3\r\n"
			L"\"Foo.cpp\",\"Syntax (simple check)\",\"Not run\",0,0\r\n"
			L"\"Foo.cpp\",\"Judgment consistency\",\"Passed\",0,1\r\n"
			L"\r\n"
			L"\"Issue list\"\r\n"
			L"\"File name\",\"Check\",\"Diff no.\",\"Result line\",\"Detail\"\r\n"));
}

TEST(Merge20VerifyReport, SummaryCounts)
{
	Merge20VerifyReport report;
	report.verifiedAt = L"2026-11-27 21:05:33";
	report.files.push_back(CleanFile(L"Foo.cpp"));
	report.files.push_back(FailedFile(L"Bar.cpp"));
	report.files.push_back(UnverifiedFile(L"Baz.cpp"));
	const String csv = Merge20VerifyReportBuildCsv(report, TestLabels());
	// 3 files covered, but only the verified ones count as passed/failed.
	EXPECT_TRUE(csv.find(L"\"Files verified\",3\r\n") != String::npos);
	EXPECT_TRUE(csv.find(L"\"Files passed\",1\r\n") != String::npos);
	EXPECT_TRUE(csv.find(L"\"Files failed\",1\r\n") != String::npos);
	EXPECT_TRUE(csv.find(L"\"Total issues\",2\r\n") != String::npos);
	// The failed check shows Failed with its issue count.
	EXPECT_TRUE(csv.find(L"\"Bar.cpp\",\"Re-diff\",\"Failed\",2,5\r\n") != String::npos);
	// The unverified file gets one row saying so, with empty cells.
	EXPECT_TRUE(csv.find(L"\"Baz.cpp\",\"Not verified (no merge result built)\",\"\",\"\",\"\"\r\n") != String::npos);
}

TEST(Merge20VerifyReport, IssueRows)
{
	Merge20VerifyReport report;
	report.verifiedAt = L"2026-11-27 21:05:33";
	report.files.push_back(FailedFile(L"Bar.cpp"));
	const String csv = Merge20VerifyReportBuildCsv(report, TestLabels());
	// Diff numbers are 1-based in the report (diffIndex 2 -> 3).
	EXPECT_TRUE(csv.find(
		L"\"Bar.cpp\",\"Re-diff\",3,42,\"Unexplained difference from NEW (NEW line 7) : int x = 1;\"\r\n")
		!= String::npos);
	// No diff / no line: empty cells; quotes doubled, comma and
	// newline kept inside the one quoted detail field.
	EXPECT_TRUE(csv.find(
		L"\"Bar.cpp\",\"Re-diff\",,,\"say \"\"hi\"\", bye\nnext\"\r\n") != String::npos);
}

TEST(Merge20VerifyReport, Utf8Bytes)
{
	Merge20VerifyReportLabels labels = TestLabels();
	// Japanese title (\\u escapes keep this file ASCII).
	labels.title = L"\u691c\u8a3c\u30ec\u30dd\u30fc\u30c8";
	Merge20VerifyReport report;
	report.verifiedAt = L"2026-11-27 21:05:33";
	report.files.push_back(CleanFile(L"Foo.cpp"));
	const std::string bytes = ucr::toUTF8(Merge20VerifyReportBuildCsv(report, labels));
	// The builder writes no BOM itself (the caller writes it with
	// WriteBom, as in S6): the file starts with the title's bytes.
	EXPECT_EQ(bytes.substr(0, 1), "\"");
	// CRLF line endings only: no bare LF anywhere.
	for (size_t i = 0; i < bytes.size(); ++i)
	{
		if (bytes[i] == '\n')
		{
			EXPECT_GT(i, 0u) << "bare LF at byte " << i;
			if (i > 0)
				EXPECT_EQ(bytes[i - 1], '\r') << "bare LF at byte " << i;
		}
	}
	// The Japanese title encodes as UTF-8: \u691c = E6 A4 9C,
	// \u8a3c = E8 A8 BC (what Excel reads after the BOM).
	EXPECT_TRUE(bytes.find("\"\xE6\xA4\x9C\xE8\xA8\xBC") != std::string::npos);
	// The documented file bytes are BOM + this content.
	const std::string fileBytes = std::string("\xEF\xBB\xBF") + bytes;
	EXPECT_EQ(static_cast<unsigned char>(fileBytes[0]), 0xEFu);
	EXPECT_EQ(static_cast<unsigned char>(fileBytes[1]), 0xBB);
	EXPECT_EQ(static_cast<unsigned char>(fileBytes[2]), 0xBF);
	EXPECT_EQ(fileBytes.substr(3), bytes);
}
