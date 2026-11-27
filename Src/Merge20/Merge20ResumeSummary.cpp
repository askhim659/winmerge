// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20ResumeSummary.cpp
 *
 * @brief Implementation of the resume summary (B13).
 *
 * The counts are a single pass over the session files: one
 * Merge20CloseWarnFile per session file is fed to the B2 counting
 * function, so the summary and the close warning cannot disagree
 * about undecided files, undecided diffs or high-risk items; the
 * output-target count is a plain count of includeInOutput. The
 * wording lines substitute the numbers into the caller-supplied
 * templates (strutils::format_strings, the same helper the B9
 * dialog uses), with diff and line numbers shown 1-based.
 *
 * This file stays ASCII: no Japanese literal is needed here (the
 * line templates come from the caller through `_()`).
 */
#include "pch.h"
#include "Merge20/Merge20ResumeSummary.h"

Merge20ResumeSummary Merge20ResumeSummaryFromSession(const Merge20Session& session)
{
	Merge20ResumeSummary summary;
	std::vector<Merge20CloseWarnFile> files;
	files.reserve(session.files.size());
	for (const Merge20SessionFile& file : session.files)
	{
		Merge20CloseWarnFile record;
		record.review = file.review;
		record.risk = file.risk;
		record.diffConclusions.reserve(file.diffs.size());
		for (const Merge20DiffJudgment& diff : file.diffs)
			record.diffConclusions.push_back(diff.conclusion);
		files.push_back(std::move(record));
		if (file.includeInOutput)
			++summary.outputFiles;
	}
	// Merge20 (best-workflow B13): the undecided counts are the B2
	// counts, computed by the B2 function itself, so the summary
	// agrees with the session contents and with the close warning
	// by construction.
	summary.warn = Merge20CloseWarnComputeCounts(files);
	return summary;
}

std::vector<String> Merge20ResumeSummaryFormatLines(const Merge20ResumeSummary& summary,
	const String& savedAt, const String& resumeRelPath, int resumeDiff, int resumeTopLine,
	const Merge20ResumeSummaryTemplates& templates)
{
	std::vector<String> lines;
	if (!savedAt.empty())
		lines.push_back(strutils::format_string1(templates.savedAt, savedAt));
	const String undecidedFiles = strutils::to_str(summary.warn.undecidedFiles);
	const String undecidedDiffs = strutils::to_str(summary.warn.undecidedDiffs);
	const String outputFiles = strutils::to_str(summary.outputFiles);
	const String totalFiles = strutils::to_str(summary.warn.total);
	const String* countArgs[] = { &undecidedFiles, &undecidedDiffs, &outputFiles, &totalFiles };
	lines.push_back(strutils::format_strings(templates.counts, countArgs, 4));
	if (resumeRelPath.empty())
	{
		lines.push_back(templates.noResume);
	}
	else if (resumeDiff >= 0)
	{
		// Diff and line numbers are 1-based on screen (the stored
		// values are 0-based); a diff of 0 is the first diff.
		const String diffNumber = strutils::to_str(resumeDiff + 1);
		const String topLine = strutils::to_str(resumeTopLine + 1);
		const String* resumeArgs[] = { &resumeRelPath, &diffNumber, &topLine };
		lines.push_back(strutils::format_strings(templates.resumeWithPosition, resumeArgs, 3));
	}
	else
	{
		lines.push_back(strutils::format_string1(templates.resumeNoPosition, resumeRelPath));
	}
	return lines;
}
