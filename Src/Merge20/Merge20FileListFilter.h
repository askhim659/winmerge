// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FileListFilter.h
 *
 * @brief Visibility decision for the persistent file-list panel
 * (Panel P2, design section 12.7).
 *
 * Merge20 (Panel P2, design sections 12.3 P-R4/P-R5/P-R10 and
 * 12.7): the panel shows the folder compare's files as a flat
 * depth-first list. Search, the action-category filter tabs and
 * the per-directory expand/collapse state all decide which rows
 * are shown; this pure module owns that decision so it can be
 * unit-tested without MFC. The control (CMerge20FileListCtrl)
 * enumerates all rows, calls Merge20ComputeFileListVisibility()
 * once, and keeps the rows it returns.
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
 * @return One bool per input row, in the same order.
 */
std::vector<bool> Merge20ComputeFileListVisibility(
	const std::vector<Merge20FileListFilterRow>& rows,
	const String& searchText,
	int actionFilter);
