// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20ImpactView.h
 *
 * @brief Display-row assembly for the cross-file impact list
 * (Merge20 best-workflow B5, PR2: display + jump wiring).
 *
 * The impact list in the focus dock (D4) and the sniper overlay
 * (D6) shows one row per Merge20ImpactHit. The row texts and the
 * jump target are assembled here, in a pure module (no MFC, no
 * `_()`), so the GoogleTest runner can fix them: the row keeps the
 * hit's symbol, relative path, pane and excerpt, and shows the
 * line 1-based (the hit stores the 0-based real file line, the
 * list shows the line the user sees in the editor). The jump
 * target is the D7 record (Merge20GrepHit): the same fields minus
 * the symbol, resolved to a diff at click time by
 * CDirView::Merge20OpenFileAtLine, exactly like a Grep hit.
 *
 * The same code is compiled into WinMergeU and into the
 * GoogleTest runner.
 */
#pragma once

#include "Merge20/Merge20Grep.h"
#include "Merge20/Merge20Impact.h"

/**
 * @brief One row of the "uses in other files" list (display data
 * only: the list control renders these strings as columns).
 */
struct Merge20ImpactViewRow
{
	String symbol;     /**< Changed symbol found on this line (S4 spelling). */
	String relPath;    /**< File path relative to the compared root. */
	int pane = 0;      /**< 0 = OLD, 1 = NEW (2-way); 1 = middle in 3-way. */
	String lineNumber; /**< 1-based line number, as display text. */
	String excerpt;    /**< Trimmed hit-line text (at most 120 chars, D7 rule). */
};

/**
 * @brief Build the list row for one impact hit.
 * @param [in] hit One use of a changed symbol in another file.
 * @return Row with the hit's symbol, path, pane and excerpt, and
 *         the line shown 1-based (hit.line + 1).
 */
Merge20ImpactViewRow Merge20ImpactViewRowFromHit(const Merge20ImpactHit& hit);

/**
 * @brief Build the D7 jump target for one impact hit.
 * @param [in] hit One use of a changed symbol in another file.
 * @return Merge20GrepHit with the hit's relative path, pane, line
 *         (0-based, as the jump takes it) and excerpt. The symbol
 *         is not part of the jump: the diff index is resolved at
 *         click time from the opened compare document.
 */
Merge20GrepHit Merge20ImpactHitToGrepHit(const Merge20ImpactHit& hit);
