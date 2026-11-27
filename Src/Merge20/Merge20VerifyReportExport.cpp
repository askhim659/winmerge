// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20VerifyReportExport.cpp
 *
 * @brief Implementation of the B8 verification report export.
 *
 * File writing is the S6 pattern (UniStdioFile::OpenCreateUtf8
 * + SetBom + WriteBom). The default name is
 * VerifyReport_YYYY-MM-DD.csv (gap analysis section 4.2 B8);
 * a same-day repeat save never overwrites: the time is
 * appended, then a counter.
 */
#include "StdAfx.h"
#include <vector>
#include "Merge20/Merge20VerifyReportExport.h"
#include "UniFile.h"
#include "MergeApp.h"
#include "I18n.h"

Merge20VerifyReportLabels Merge20VerifyReportMakeLabels()
{
	Merge20VerifyReportLabels labels;
	labels.title = _("Verification report");
	labels.verifiedAt = _("Verification date/time");
	labels.fileCount = _("Files verified");
	labels.passedFiles = _("Files passed");
	labels.failedFiles = _("Files failed");
	labels.issueCount = _("Total issues");
	labels.checksTitle = _("Check results");
	labels.issuesTitle = _("Issue list");
	labels.headerFile = _("File name");
	labels.headerCheck = _("Check");
	labels.headerResult = _("Result");
	labels.headerIssues = _("Issues");
	labels.headerElapsed = _("Elapsed (ms)");
	labels.headerDiff = _("Diff no.");
	labels.headerResultLine = _("Result line");
	labels.headerDetail = _("Detail");
	labels.passed = _("Passed");
	labels.failed = _("Failed");
	labels.notRun = _("Not run");
	labels.notVerified = _("Not verified (no merge result built)");
	labels.checkNames[static_cast<size_t>(Merge20VerifyCheck::Rediff)] =
		Merge20VerifyCheckDisplayName(Merge20VerifyCheck::Rediff);
	labels.checkNames[static_cast<size_t>(Merge20VerifyCheck::LineCountEncoding)] =
		Merge20VerifyCheckDisplayName(Merge20VerifyCheck::LineCountEncoding);
	labels.checkNames[static_cast<size_t>(Merge20VerifyCheck::Syntax)] =
		Merge20VerifyCheckDisplayName(Merge20VerifyCheck::Syntax);
	labels.checkNames[static_cast<size_t>(Merge20VerifyCheck::Consistency)] =
		Merge20VerifyCheckDisplayName(Merge20VerifyCheck::Consistency);
	return labels;
}

Merge20VerifyReport Merge20VerifyReportFromFileResults(
	const std::vector<Merge20VerifyFileResult>& files, const String& verifiedAt)
{
	Merge20VerifyReport report;
	report.verifiedAt = verifiedAt;
	for (const Merge20VerifyFileResult& file : files)
	{
		Merge20VerifyReportFile out;
		out.fileName = file.fileName;
		out.verified = file.verified;
		out.passed = file.result.passed;
		out.checks = file.result.checks;
		for (const Merge20VerifyIssue& issue : file.result.issues)
		{
			Merge20VerifyReportIssue row;
			row.fileName = file.fileName;
			row.checkName = Merge20VerifyCheckDisplayName(issue.check);
			row.diffIndex = issue.diffIndex;
			row.resultLine = issue.resultLine;
			row.detail = Merge20VerifyFormatIssueDetail(issue);
			out.issues.push_back(std::move(row));
		}
		report.files.push_back(std::move(out));
	}
	return report;
}

bool Merge20VerifyReportWriteFile(const Merge20VerifyReport& report,
	const Merge20VerifyReportLabels& labels, const String& filePath, String& errStr)
{
	errStr.clear();

	UniStdioFile file;
	if (!file.OpenCreateUtf8(filePath))
	{
		String errMsg = GetSysError(GetLastError());
		errStr = strutils::format_string1(_("Error creating the report:\n%1"), errMsg);
		return false;
	}
	// BOM-prefixed UTF-8 (design section 3.5 / S6): without the
	// BOM, Excel opens the Japanese text garbled.
	file.SetBom(true);
	file.WriteBom();

	file.WriteString(Merge20VerifyReportBuildCsv(report, labels));
	file.Close();
	return true;
}

String Merge20VerifyReportDefaultFileName(const CTime& now)
{
	return _T("VerifyReport_") +
		String(now.Format(_T("%Y-%m-%d")).GetString()) + _T(".csv");
}

String Merge20VerifyReportUniquePath(const String& directory, const String& baseName)
{
	String dir = directory;
	if (!dir.empty() && dir.back() != _T('\\') && dir.back() != _T('/'))
		dir += _T('\\');

	String candidate = dir + baseName;
	if (::GetFileAttributes(candidate.c_str()) == INVALID_FILE_ATTRIBUTES)
		return candidate;

	// Same-day collision: append the time (VerifyReport_YYYY-MM-DD_HHMMSS.csv).
	const size_t dot = baseName.find_last_of(_T('.'));
	const String stem = (dot == String::npos) ? baseName : baseName.substr(0, dot);
	const String ext = (dot == String::npos) ? String() : baseName.substr(dot);
	const String stamp = String(CTime::GetCurrentTime().Format(_T("%H%M%S")).GetString());
	candidate = dir + stem + _T("_") + stamp + ext;
	if (::GetFileAttributes(candidate.c_str()) == INVALID_FILE_ATTRIBUTES)
		return candidate;
	for (int n = 2; n <= 999; ++n)
	{
		candidate = dir + stem + _T("_") + stamp + _T("_") +
			strutils::to_str(n) + ext;
		if (::GetFileAttributes(candidate.c_str()) == INVALID_FILE_ATTRIBUTES)
			return candidate;
	}
	return candidate;
}

bool Merge20VerifySaveReportAfterOutput(const Merge20VerifyFileResult& lastResult,
	const String& outputPath, String& reportPath, String& errStr)
{
	reportPath.clear();
	errStr.clear();
	if (!lastResult.verified)
		return true; // Nothing was verified: no report (not an error).

	// Same folder as the merge output, so the evidence travels
	// with the output.
	String directory;
	const size_t slash = outputPath.find_last_of(_T("\\/"));
	if (slash != String::npos)
		directory = outputPath.substr(0, slash);

	const String baseName = Merge20VerifyReportDefaultFileName(CTime::GetCurrentTime());
	reportPath = Merge20VerifyReportUniquePath(directory, baseName);
	const String verifiedAt =
		String(CTime::GetCurrentTime().Format(_T("%Y-%m-%d %H:%M:%S")).GetString());
	const Merge20VerifyReport report =
		Merge20VerifyReportFromFileResults({ lastResult }, verifiedAt);
	return Merge20VerifyReportWriteFile(report, Merge20VerifyReportMakeLabels(),
		reportPath, errStr);
}
