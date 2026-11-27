// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20ImpactView.cpp
 *
 * @brief Implementation of the impact-list display assembly
 * (Merge20 best-workflow B5, PR2).
 *
 * The row is a straight copy of the hit, except the line number:
 * the hit stores the 0-based real file line (the jump takes it
 * as-is), the list shows it 1-based (the line the user sees in
 * the editor, the same convention the D7 Grep list uses). The
 * jump target drops only the symbol: the D7 jump resolves the
 * diff index from the opened document, so it needs the path, the
 * pane, the line and the excerpt, and nothing else.
 *
 * This file stays ASCII: no Japanese literal is needed here (the
 * column headers and the status texts are rendered through `_()`
 * by the list control, CMerge20ImpactList).
 */
#include "pch.h"
#include "Merge20/Merge20ImpactView.h"

Merge20ImpactViewRow Merge20ImpactViewRowFromHit(const Merge20ImpactHit& hit)
{
	Merge20ImpactViewRow row;
	row.symbol = hit.symbol;
	row.relPath = hit.relPath;
	row.pane = hit.pane;
	// 1-based for display (the hit stores the 0-based real line).
	row.lineNumber = strutils::to_str(hit.line + 1);
	row.excerpt = hit.excerpt;
	return row;
}

Merge20GrepHit Merge20ImpactHitToGrepHit(const Merge20ImpactHit& hit)
{
	Merge20GrepHit target;
	target.relPath = hit.relPath;
	target.pane = hit.pane;
	target.line = hit.line;
	target.excerpt = hit.excerpt;
	return target;
}
