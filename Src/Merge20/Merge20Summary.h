// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Summary.h
 *
 * @brief End-of-session summary counts (Merge20 best-workflow B9,
 * pillar 5: "confirm the session is complete before finishing").
 *
 * Pure module (no MFC, no `_()`): the H-DASH dashboard counts for
 * the session-summary dialog, computed from one plain record per
 * file. The same code is compiled into WinMergeU and into the
 * GoogleTest runner.
 *
 * The seven H-DASH kinds (gap analysis section 4.2 B9, coverage
 * matrix H-36) are: total files, files with differences, changed
 * files, old-only files, new-only files, identical files, and files
 * needing manual work (review state SPECIAL). Two more counts ride
 * along because the dialog needs them for its warning and its
 * confirm button: unreviewed files (TODO) and output targets
 * (output checkbox on).
 *
 * The caller (CDirView::OnMerge20Summary) derives each record from
 * one DIFFITEM, the same walk S6 uses for the CSV export, so the
 * dialog and the CSV never disagree about which files exist:
 * - hasDiff is Merge20CsvHasDiff (changed, or unique on any side);
 * - isModified / isOldOnly / isNewOnly are the three disjoint
 *   diffcode cases (isResultDiff / isSideFirstOnly /
 *   isSideSecondOnly or isSideThirdOnly); a 3-way third-only file
 *   counts as new-only, because the third pane is the new side;
 * - identical is simply "not hasDiff" (identical, skipped, error
 *   and aborted files all land here, as in S6's "all diffs" range);
 * - review and includeInOutput come from the CDiffContext sidecar.
 */
#pragma once

#include <vector>
#include "Merge20Types.h"

/**
 * @brief One file's inputs to the end-of-session summary counts.
 */
struct Merge20SummaryFile
{
	bool hasDiff = false;       /**< Changed, or unique on any side (Merge20CsvHasDiff). */
	bool isModified = false;    /**< Both sides exist and differ (diffcode isResultDiff). */
	bool isOldOnly = false;     /**< Old-only file (diffcode isSideFirstOnly). */
	bool isNewOnly = false;     /**< New-only file (isSideSecondOnly, or isSideThirdOnly in 3-way). */
	ReviewState review = ReviewState::TODO; /**< Review state from the sidecar. */
	bool includeInOutput = false; /**< Output checkbox from the sidecar. */
};

/**
 * @brief The end-of-session summary counts (H-DASH, B9).
 *
 * The seven H-DASH kinds are total / diffCount / modified /
 * oldOnly / newOnly / identical / special; undecided and
 * outputTarget are the dialog's warning and confirm inputs.
 */
struct Merge20SummaryCounts
{
	int total = 0;        /**< All files in the comparison (directories excluded). */
	int diffCount = 0;    /**< Files with differences (hasDiff). */
	int modified = 0;     /**< Changed files (both sides exist and differ). */
	int oldOnly = 0;      /**< Old-only files. */
	int newOnly = 0;      /**< New-only files (third-only counts here in 3-way). */
	int identical = 0;    /**< Files without differences (!hasDiff). */
	int special = 0;      /**< Files whose review state is SPECIAL (needs manual work). */
	int undecided = 0;    /**< Unreviewed files (review state TODO). */
	int outputTarget = 0; /**< Files with the output checkbox on. */
};

/**
 * @brief Compute the summary counts for a whole comparison.
 * @param [in] files One record per file (directories excluded).
 * @return The nine counts; all zero for an empty input.
 */
Merge20SummaryCounts Merge20SummaryComputeCounts(const std::vector<Merge20SummaryFile>& files);

/**
 * @brief Whether any file is still unreviewed.
 * @param [in] counts Counts from Merge20SummaryComputeCounts.
 * @return True when counts.undecided > 0 (the dialog shows its warning).
 */
bool Merge20SummaryHasUndecided(const Merge20SummaryCounts& counts);
