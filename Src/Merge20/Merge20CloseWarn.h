// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20CloseWarn.h
 *
 * @brief Undecided-items warning counts (Merge20 best-workflow B2
 * stage 1, pillar 1: "no diff left without an explicit conclusion").
 *
 * Pure module (no MFC, no `_()`): the counts behind the warning
 * shown when a folder compare document is about to close with
 * undecided items left. The same code is compiled into WinMergeU
 * and into the GoogleTest runner.
 *
 * Counting rules (stated here so the dialog, the tests and the PR
 * description can never disagree; gap analysis section 4.2 B2):
 * - undecidedFiles counts the files whose review state is TODO
 *   (the S2/S3 file-level confirmation state; the same count B9
 *   reports as "undecided").
 * - undecidedDiffs counts the recorded per-diff judgments whose
 *   B1 conclusion is Undecided. Only files opened in a merge
 *   session have recorded judgments (Merge20SessionDoc
 *   ExtractJudgments writes one entry per diff of an open merge
 *   doc); a file that was never opened contributes 0 diffs here,
 *   and its unreviewed work is represented by the file count
 *   above. Each recorded diff is counted at most once, and a file
 *   is never counted as a diff, so the two numbers never double
 *   count the same item.
 * - highRisk counts the undecided items that live in a HIGH-risk
 *   file (the S4 classification, same definition as everywhere
 *   else): TODO files with risk HIGH, plus Undecided diffs in
 *   HIGH-risk files. It is always <= undecidedFiles +
 *   undecidedDiffs.
 * - The warning is shown exactly when undecidedFiles +
 *   undecidedDiffs > 0.
 *
 * Stage 2 hook (gap analysis section 5, two-stage B2): the S7
 * unified-ZIP output gate and the S5 bulk-approve gate reuse this
 * same counting function and the same dialog (with their own
 * proceed label); only the call site changes, not the counts.
 */
#pragma once

#include <vector>
#include "Merge20Types.h"
#include "Merge20Conclusion.h"

/**
 * @brief One file's inputs to the close-warning counts.
 */
struct Merge20CloseWarnFile
{
	ReviewState review = ReviewState::TODO; /**< Review state from the sidecar. */
	RiskLevel risk = RiskLevel::NONE;       /**< S4 risk of the file (NONE when unclassified). */
	/** @brief One B1 conclusion ordinal per recorded diff judgment
	 * (Merge20DiffJudgment::conclusion); empty when the file was
	 * never opened in a merge session. */
	std::vector<int> diffConclusions;
};

/**
 * @brief The close-warning counts (B2 stage 1).
 */
struct Merge20CloseWarnCounts
{
	int total = 0;           /**< All files in the comparison (directories excluded). */
	int undecidedFiles = 0;  /**< Files whose review state is TODO. */
	int undecidedDiffs = 0;  /**< Recorded diff judgments with conclusion Undecided. */
	int highRisk = 0;        /**< Undecided items (files + diffs) in HIGH-risk files. */
};

/**
 * @brief Compute the close-warning counts for a whole comparison.
 * @param [in] files One record per file (directories excluded).
 * @return The four counts; all zero for an empty input.
 */
Merge20CloseWarnCounts Merge20CloseWarnComputeCounts(const std::vector<Merge20CloseWarnFile>& files);

/**
 * @brief Whether the close warning must be shown.
 * @param [in] counts Counts from Merge20CloseWarnComputeCounts.
 * @return True when any undecided file or diff remains.
 */
bool Merge20CloseWarnHasUndecided(const Merge20CloseWarnCounts& counts);
