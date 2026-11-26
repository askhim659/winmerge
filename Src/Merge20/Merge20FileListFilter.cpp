// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FileListFilter.cpp
 *
 * @brief Implementation of the file-list panel visibility decision
 * (Panel P2, design section 12.7). See Merge20FileListFilter.h for
 * the rules.
 *
 * The decision is one forward pass plus a directory fix-up:
 * - The forward pass walks the depth-first rows, tracking the
 *   directory ancestors on a stack (a row belongs to the
 *   directories on the stack) and the depth below which a
 *   collapsed directory hides its descendants. A visible file
 *   marks every ancestor directory as "has a visible file".
 * - A directory row is then visible when it matched the search
 *   itself, or when it has a visible file descendant. The stack
 *   marks ancestors transitively, so a folder whose only visible
 *   file sits two levels down stays visible too.
 *
 * This file stays ASCII: no Japanese literal is needed here (all
 * user-visible strings are composed by the bar through _()).
 */
#include "pch.h"
#include "Merge20/Merge20FileListFilter.h"

std::vector<bool> Merge20ComputeFileListVisibility(
	const std::vector<Merge20FileListFilterRow>& rows,
	const String& searchText,
	int actionFilter)
{
	// Search definition (HTML tool qFile): trim, lower-case,
	// substring over the whole relative path.
	const String query = strutils::makelower(strutils::trim_ws(searchText));
	const bool searching = !query.empty();
	const size_t n = rows.size();
	std::vector<bool> visible(n, false);
	std::vector<bool> hiddenByCollapse(n, false);
	std::vector<bool> dirHasVisibleFile(n, false);
	// Indices of the ancestor directory rows of the row being
	// visited (depth-first order, so ancestors are exactly the
	// directories still on the stack).
	std::vector<size_t> dirStack;
	// Depth of the collapsed directory whose descendants are
	// currently hidden (-1 when none). Only one can be active:
	// a collapsed directory is only recorded when its own row is
	// visible, and its subtree ends exactly when a row of equal
	// or smaller depth appears.
	int hiddenBelowDepth = -1;
	for (size_t i = 0; i < n; ++i)
	{
		const Merge20FileListFilterRow& row = rows[i];
		// Leaving the subtree of the collapsed directory.
		if (hiddenBelowDepth >= 0 && row.depth <= hiddenBelowDepth)
			hiddenBelowDepth = -1;
		const bool hidden = !searching && hiddenBelowDepth >= 0 && row.depth > hiddenBelowDepth;
		hiddenByCollapse[i] = hidden;
		// Pop directories that are not ancestors of this row
		// (a row at the same depth is a sibling, not a child).
		while (!dirStack.empty() && rows[dirStack.back()].depth >= row.depth)
			dirStack.pop_back();
		const bool searchMatch = query.empty() ||
			strutils::makelower(row.relPath).find(query) != String::npos;
		if (row.isDirectory)
		{
			// Directory rows match on the search text only (they
			// have no action category); whether they stay visible
			// also depends on visible descendants (fixed up below).
			if (!hidden)
			{
				visible[i] = searchMatch;
				if (row.collapsed && !searching)
					hiddenBelowDepth = row.depth;
			}
			dirStack.push_back(i);
		}
		else
		{
			const bool actionMatch = (actionFilter < 0) ||
				(row.classified && static_cast<int>(row.action) == actionFilter);
			const bool show = searchMatch && actionMatch && !hidden;
			visible[i] = show;
			if (show)
			{
				for (size_t d : dirStack)
					dirHasVisibleFile[d] = true;
			}
		}
	}
	// Directory rows stay when they match the search themselves,
	// or when they have a visible file descendant. A row hidden
	// by a collapsed ancestor never gains visibility here: no
	// visible file can sit below it (those rows are hidden too),
	// so dirHasVisibleFile stays false for it.
	for (size_t i = 0; i < n; ++i)
	{
		if (rows[i].isDirectory && !hiddenByCollapse[i])
			visible[i] = visible[i] || dirHasVisibleFile[i];
	}
	return visible;
}
