// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20VerifyReport.cpp
 *
 * @brief Implementation of the B8 verification report builder.
 *
 * The CSV rules are the S6 ones (Merge20CsvEscapeField: every
 * text field double-quoted, embedded quotes doubled; counts as
 * bare numbers; CRLF line endings). This file stays ASCII: all
 * Japanese text arrives through the labels (composed by the
 * caller with `_()`).
 */
#include "pch.h"
#include <string>
#include "Merge20/Merge20VerifyReport.h"
#include "Merge20/Merge20Csv.h"

namespace
{

// One bare number (counts, elapsed ms): no quotes, so Excel
// reads it as a number, as in the S6 CSV.
String BareNumber(long long value)
{
	return std::to_wstring(value);
}

// One CSV line from already-escaped fields (no line ending;
// the caller joins lines with CRLF, as in the S6 export).
String JoinFields(const std::vector<String>& fields)
{
	String line;
	for (size_t i = 0; i < fields.size(); ++i)
	{
		if (i > 0)
			line += L',';
		line += fields[i];
	}
	return line;
}

} // namespace

String Merge20VerifyReportBuildCsv(const Merge20VerifyReport& report,
	const Merge20VerifyReportLabels& labels)
{
	std::vector<String> lines;

	// Title.
	lines.push_back(Merge20CsvEscapeField(labels.title));

	// Summary: date/time, files covered, passed, failed, issues.
	int passedFiles = 0;
	int failedFiles = 0;
	int totalIssues = 0;
	for (const Merge20VerifyReportFile& file : report.files)
	{
		if (!file.verified)
			continue;
		if (file.passed)
			++passedFiles;
		else
			++failedFiles;
		totalIssues += static_cast<int>(file.issues.size());
	}
	lines.push_back(JoinFields({ Merge20CsvEscapeField(labels.verifiedAt),
		Merge20CsvEscapeField(report.verifiedAt) }));
	lines.push_back(JoinFields({ Merge20CsvEscapeField(labels.fileCount),
		BareNumber(static_cast<long long>(report.files.size())) }));
	lines.push_back(JoinFields({ Merge20CsvEscapeField(labels.passedFiles),
		BareNumber(passedFiles) }));
	lines.push_back(JoinFields({ Merge20CsvEscapeField(labels.failedFiles),
		BareNumber(failedFiles) }));
	lines.push_back(JoinFields({ Merge20CsvEscapeField(labels.issueCount),
		BareNumber(totalIssues) }));

	lines.push_back(String());

	// Check table: one row per (file, check).
	lines.push_back(Merge20CsvEscapeField(labels.checksTitle));
	lines.push_back(JoinFields({ Merge20CsvEscapeField(labels.headerFile),
		Merge20CsvEscapeField(labels.headerCheck),
		Merge20CsvEscapeField(labels.headerResult),
		Merge20CsvEscapeField(labels.headerIssues),
		Merge20CsvEscapeField(labels.headerElapsed) }));
	for (const Merge20VerifyReportFile& file : report.files)
	{
		if (!file.verified)
		{
			// No merge result was built: there is nothing to
			// tabulate for this file, so the row says so and
			// leaves the other cells empty.
			lines.push_back(JoinFields({ Merge20CsvEscapeField(file.fileName),
				Merge20CsvEscapeField(labels.notVerified),
				Merge20CsvEscapeField(String()),
				Merge20CsvEscapeField(String()),
				Merge20CsvEscapeField(String()) }));
			continue;
		}
		for (const Merge20VerifyCheckResult& check : file.checks)
		{
			String result;
			if (!check.enabled)
				result = labels.notRun;
			else if (check.passed)
				result = labels.passed;
			else
				result = labels.failed;
			const size_t checkIndex = static_cast<size_t>(check.check);
			const String checkName = (checkIndex < 4) ? labels.checkNames[checkIndex] : String();
			lines.push_back(JoinFields({ Merge20CsvEscapeField(file.fileName),
				Merge20CsvEscapeField(checkName),
				Merge20CsvEscapeField(result),
				BareNumber(check.issueCount),
				BareNumber(check.elapsedMs) }));
		}
	}

	lines.push_back(String());

	// Issue list: one row per problem (header only when clean).
	lines.push_back(Merge20CsvEscapeField(labels.issuesTitle));
	lines.push_back(JoinFields({ Merge20CsvEscapeField(labels.headerFile),
		Merge20CsvEscapeField(labels.headerCheck),
		Merge20CsvEscapeField(labels.headerDiff),
		Merge20CsvEscapeField(labels.headerResultLine),
		Merge20CsvEscapeField(labels.headerDetail) }));
	for (const Merge20VerifyReportFile& file : report.files)
	{
		for (const Merge20VerifyReportIssue& issue : file.issues)
		{
			const String diff = (issue.diffIndex >= 0)
				? BareNumber(static_cast<long long>(issue.diffIndex) + 1) : String();
			const String resultLine = (issue.resultLine >= 1)
				? BareNumber(issue.resultLine) : String();
			lines.push_back(JoinFields({ Merge20CsvEscapeField(issue.fileName),
				Merge20CsvEscapeField(issue.checkName),
				diff,
				resultLine,
				Merge20CsvEscapeField(issue.detail) }));
		}
	}

	// CRLF after every line, including the last (the S6 files
	// end the same way).
	String out;
	for (const String& line : lines)
	{
		out += line;
		out += L"\r\n";
	}
	return out;
}
