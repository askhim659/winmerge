// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FileListFilter.h
 *
 * @brief Visibility decision for the persistent file-list panel
 * (Panel P2, design section 12.7).
 *
 * Merge20 (Panel P2, design sections 12.3 P-R4/P-R5/P-R10 and
 * 12.7): the panel shows the folder compare's files as a flat
 * depth-first list. Search, the action-category filter tabs, the
 * risk filter (B3) and the per-directory expand/collapse state all
 * decide which rows are shown; this pure module owns that decision
 * so it can be unit-tested without MFC. The control
 * (CMerge20FileListCtrl) enumerates all rows, calls
 * Merge20ComputeFileListVisibility() once, and keeps the rows it
 * returns.
 *
 * Rules (design section 12.3, HTML tool renderTree):
 * - Search: trim surrounding whitespace, case-insensitive
 *   substring match against the whole relative path (folder part
 *   included), the same definition as the HTML tool's qFile
 *   (value.trim().toLowerCase(), path.includes(q)).
 * - Action filter: -1 shows every category; otherwise only files
 *   whose classification matches that ActionCategory. Unclassified
 *   files (never opened) match no category tab, only "all".
 *   Directory rows have no category and never match a tab by
 *   themselves.
 * - Risk filter (B3): -1 shows every risk level; otherwise only
 *   files whose S4 risk matches that RiskLevel. Unclassified files
 *   match no risk level, only -1 (same rule as the action tabs).
 *   Directory rows have no risk and never match a risk level by
 *   themselves; they stay visible when they have a visible file
 *   descendant, exactly as with the action filter.
 * - Collapse: a collapsed directory hides its descendant rows.
 *   While a search string is active the collapse state is
 *   ignored, so a matching file inside a collapsed folder is
 *   still shown (the HTML tool rebuilds its groups from the
 *   filtered files, which reads the same way).
 * - Directory rows are shown when they match the search
 *   themselves, or when they have at least one visible file
 *   descendant (so a folder that only contains a match stays
 *   visible as the path to it).
 *
 * This file stays ASCII and free of MFC and of the translation
 * macro _(): the same code is compiled into WinMergeU and into
 * the GoogleTest runner (Merge20Classify.h precedent).
 */
#pragma once

#include <set>
#include <vector>
#include "UnicodeString.h"
#include "Merge20Types.h"

/** @brief One panel row, reduced to what the visibility decision needs. */
struct Merge20FileListFilterRow
{
	String relPath; /**< Relative path (DIFFITEM::getItemRelativePath()), folders included */
	bool isDirectory = false; /**< Directory rows have no category and no output flag */
	int depth = 0; /**< DIFFITEM::GetDepth(): files directly under the root are 0 */
	bool classified = false; /**< True once S4 classification ran for this file */
	ActionCategory action = ActionCategory::NO_ACTION_IDENTICAL; /**< Meaningful only when classified */
	RiskLevel risk = RiskLevel::NONE; /**< B3: S4 risk level, meaningful only when classified */
	bool collapsed = false; /**< This directory row is folded; meaningful for directories with children */
};

/**
 * @brief Decide which panel rows are visible.
 * @param [in] rows All rows, in compare order (depth-first, as
 *             enumerated by CDiffContext).
 * @param [in] searchText Raw search text (may carry surrounding
 *             whitespace or mixed case; trimmed and lower-cased here).
 * @param [in] actionFilter -1 = all action categories, otherwise
 *             static_cast<int>(ActionCategory) of the selected tab.
 * @param [in] riskFilter -1 = all risk levels (B3), otherwise
 *             static_cast<int>(RiskLevel) of the selected risk
 *             filter. Unclassified files match only -1.
 * @return One bool per input row, in the same order.
 */
std::vector<bool> Merge20ComputeFileListVisibility(
	const std::vector<Merge20FileListFilterRow>& rows,
	const String& searchText,
	int actionFilter,
	int riskFilter);

/**
 * @brief Join the folded directories' relative paths into the one
 * string stored under OPT_MERGE20_FILELIST_COLLAPSED (Panel P4,
 * design section 12.7: the collapse state is restored at the next
 * start). One path per line (L'\n'); a relative path cannot
 * contain a line break in practice (Windows file names), so no
 * escaping is needed. The set order is the sorted order, so the
 * same set always joins to the same string. Empty set -> empty
 * string.
 */
String Merge20JoinCollapsedPaths(const std::set<String>& paths);

/**
 * @brief Split a stored collapsed-paths string back into a set.
 * Empty pieces (a leading, trailing or doubled line break) are
 * skipped, so a hand-edited or truncated value cannot create a
 * phantom empty path. Stale paths (folders that no longer exist)
 * are kept: they only match rows that exist, so they are harmless
 * until the folder comes back.
 */
std::set<String> Merge20SplitCollapsedPaths(const String& text);
