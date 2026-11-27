// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20HighRiskRemain.h
 *
 * @brief Display-update decision for the undecided high-risk
 * remainder line (Merge20 best-workflow B4, pillar 2: "make the
 * danger left in the second half visible").
 *
 * Pure module (no MFC, no `_()`): the file-list panel bar calls
 * this on every Refresh() with the fresh B2 high-risk count
 * (CDirDoc::GetMerge20CloseWarnCounts().highRisk, the same value
 * the B2 close warning reports as "of which high risk"), and this
 * module answers whether the remainder control needs touching at
 * all, and whether it shows. The same code is compiled into
 * WinMergeU and into the GoogleTest runner.
 *
 * The decision (stated here so the bar, the tests and the PR
 * description can never disagree; gap analysis section 4.2 B4):
 * - the line shows exactly while highRisk > 0. At 0 it hides and
 *   takes no vertical space, so it is never a warning display.
 * - the control is touched only when the visibility flips, or
 *   while visible and the count changed. A Refresh with an
 *   unchanged count is a no-op: no text set, no show/hide churn,
 *   no layout work beyond the Refresh the bar already does. That
 *   is what keeps the update cheap on 100-1000-file compares,
 *   where Refresh() runs on every review-state notification.
 * - the bar keeps the last shown state (visible + count); the
 *   initial state is hidden with count 0, so the first Refresh
 *   with highRisk 0 touches nothing.
 *
 * The count itself is NOT computed here: it comes from
 * Merge20CloseWarnComputeCounts (B2), so B4's N and B2's Z share
 * one definition and one value by construction.
 */
#pragma once

/**
 * @brief The outcome of one remainder-display decision (B4).
 */
struct Merge20HighRiskRemainUpdate
{
	bool changed = false; /**< Touch the control (text / show / hide). False means the Refresh leaves it alone. */
	bool visible = false; /**< Show the remainder line (true exactly while highRisk > 0). */
	int highRisk = 0;     /**< The high-risk remainder count to show when visible. */
};

/**
 * @brief Decide whether the remainder display must change.
 * @param [in] lastVisible Whether the line is currently shown.
 * @param [in] lastHighRisk The count currently shown (0 when hidden).
 * @param [in] highRisk The fresh B2 high-risk count.
 * @return The update decision; changed is false when nothing needs doing.
 */
Merge20HighRiskRemainUpdate Merge20HighRiskRemainDecideUpdate(bool lastVisible, int lastHighRisk, int highRisk);
