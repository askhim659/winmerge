// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20ResumeSummary.h
 *
 * @brief Resume summary for the session-restore offer (Merge20
 * best-workflow B13, pillar 7: "show where the work left off").
 *
 * Pure module (no MFC, no `_()`): the counts and the wording lines
 * of the restore offer, computed from the loaded session. The same
 * code is compiled into WinMergeU and into the GoogleTest runner.
 *
 * Counting rules (gap analysis section 4.2 B13; the summary must
 * agree with the session contents and with B2/B9):
 * - undecidedFiles / undecidedDiffs / highRisk come from
 *   Merge20CloseWarnComputeCounts (B2, the same function as the
 *   close warning), fed with one record per session file (review
 *   state, risk, one conclusion ordinal per recorded diff
 *   judgment). A file never opened in a merge session has no
 *   recorded diffs and contributes 0 diffs; its unreviewed work is
 *   represented by the file count. Each recorded diff is counted
 *   at most once, and a file is never counted as a diff, so the
 *   two numbers never double count the same item.
 * - outputFiles counts the session files with includeInOutput
 *   (the same flag B9 reports as outputTarget).
 * - total is the number of recorded session files.
 * - The counts describe the session as saved. Files changed
 *   since the save are reset to unreviewed on restore and files
 *   that no longer exist are skipped; both are reported by the
 *   existing restore report, not by this summary.
 *
 * The wording lines are built from templates the caller supplies
 * (through `_()` at the MFC call site), so this module stays
 * ASCII and the tests can pass English templates and assert the
 * exact lines. Diff and line numbers are shown 1-based (the
 * stored resumeDiff / resumeTopLine are 0-based).
 */
#pragma once

#include <vector>
#include "UnicodeString.h"
#include "Merge20/Merge20CloseWarn.h"
#include "Merge20/Merge20Session.h"

/**
 * @brief The resume summary counts (B13).
 */
struct Merge20ResumeSummary
{
	/** @brief B2 counts over the session files (total / undecided
	 * files / undecided diffs / high-risk undecided items). */
	Merge20CloseWarnCounts warn;
	/** @brief Session files with the output checkbox on. */
	int outputFiles = 0;
};

/**
 * @brief Compute the resume summary counts from a loaded session.
 * @param [in] session Loaded session (Merge20SessionFromJson).
 * @return The counts; all zero for a session with no files.
 */
Merge20ResumeSummary Merge20ResumeSummaryFromSession(const Merge20Session& session);

/**
 * @brief Templates for the summary lines (one %N placeholder per
 * value; the caller supplies the translated templates).
 */
struct Merge20ResumeSummaryTemplates
{
	/** @brief Saved-at line ("%1" = savedAt; skipped when savedAt
	 * is empty). */
	String savedAt;
	/** @brief Counts line ("%1" = undecided files, "%2" =
	 * undecided diffs, "%3" = output targets, "%4" = total files). */
	String counts;
	/** @brief Resume line with position ("%1" = relative path,
	 * "%2" = diff number, 1-based, "%3" = top line, 1-based;
	 * used when a file compare was open and it had a current
	 * diff). */
	String resumeWithPosition;
	/** @brief Resume line without a diff ("%1" = relative path;
	 * used when a file compare was open but had no current diff). */
	String resumeNoPosition;
	/** @brief No-resume line (no placeholder; used when no file
	 * compare was open at save time). */
	String noResume;
};

/**
 * @brief Build the summary lines for the restore offer, in display
 * order: saved-at line (unless savedAt is empty), counts line,
 * resume line.
 * @param [in] summary Counts from Merge20ResumeSummaryFromSession.
 * @param [in] savedAt Save time from the session ("YYYY-MM-DD
 * HH:MM:SS", opaque here).
 * @param [in] resumeRelPath Relative path of the file compare open
 * at save time (B12); empty when none was open.
 * @param [in] resumeDiff Current diff index at save time (B12),
 * -1 when there was no current diff.
 * @param [in] resumeTopLine First visible line at save time (B12).
 * @param [in] templates Line templates (see above).
 * @return Three lines (two when savedAt is empty), in order.
 */
std::vector<String> Merge20ResumeSummaryFormatLines(const Merge20ResumeSummary& summary,
	const String& savedAt, const String& resumeRelPath, int resumeDiff, int resumeTopLine,
	const Merge20ResumeSummaryTemplates& templates);
