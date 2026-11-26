// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Focus.h
 *
 * @brief Focus dock judgement mapping (Merge20 Phase 4 D4, design
 * sections 3.5 and 5.2/D4, spec sections 3 (3/4) and 5.2).
 *
 * Pure module (no MFC, no `_()`): the mapping from the focus dock's
 * judgement buttons to the source panes passed to
 * `CMergeDoc::ResultChooseSources`, plus the scroll-sync clamp.
 * The same code is compiled into WinMergeU and into the GoogleTest
 * runner.
 *
 * Port of the HTML tool's `setHunk` strategies
 * (17_v17.15.html, lines 1492-1503 and 1631-1635): a deleted block
 * (old-only) offers INSERT_OLD / KEEP_NEW, a changed block offers
 * KEEP_NEW / REPLACE_WITH_OLD / BOTH_OLD_NEW, and an added block
 * offers no buttons (the new side is kept automatically). The fork
 * spells the strategies as source-pane selections:
 * - INSERT_OLD ("Insert old lines") takes the old pane {0}.
 * - KEEP_NEW on a deleted block ("Reject") takes the new pane {1},
 *   which contributes no lines, so the result holds the "<No Lines>"
 *   placeholder and saves nothing for the block. On an added block
 *   the same outcome is reached by taking the old pane {0}.
 * - KEEP_NEW on a changed block ("Use new") takes {1},
 *   REPLACE_WITH_OLD ("Use old") takes {0}, BOTH_OLD_NEW
 *   ("Use both") takes {0, 1}.
 * "Split" / "Regroup" have no source panes: D4 shows the buttons
 * only, D5 wires the split/regroup body.
 */
#pragma once

#include <vector>

/**
 * @brief Judgement actions offered by the focus dock header.
 */
enum class Merge20FocusAction
{
	InsertOld, /**< Take the old pane's lines (HTML INSERT_OLD) */
	Reject,    /**< Take the side that contributes no lines (HTML KEEP_NEW on a one-sided block) */
	ChooseNew, /**< Take the new pane (HTML KEEP_NEW on a changed block) */
	ChooseOld, /**< Take the old pane (HTML REPLACE_WITH_OLD) */
	ChooseBoth, /**< Take both panes, old first (HTML BOTH_OLD_NEW) */
	Split,     /**< Split the diff group into per-row hunks (D5) */
	Regroup,   /**< Regroup the split rows (D5) */
};

/**
 * @brief DIFFRANGE op values (Src/DiffList.h, OP_TYPE). The pure
 * module must not include DiffList.h, so the values are mirrored
 * here; they are stable upstream enum values (OP_NONE = 0,
 * OP_1STONLY = 1, OP_2NDONLY = 2, OP_3RDONLY = 3, OP_DIFF = 4,
 * OP_TRIVIAL = 5).
 */
constexpr int Merge20FocusOpNone = 0;
constexpr int Merge20FocusOpOldOnly = 1;
constexpr int Merge20FocusOpNewOnly = 2;
constexpr int Merge20FocusOpThirdOnly = 3;
constexpr int Merge20FocusOpDiff = 4;
constexpr int Merge20FocusOpTrivial = 5;

/**
 * @brief Source panes for a focus-dock judgement action.
 * @param [in] action The button pressed.
 * @param [in] nBuffers Compared panes (2 or 3); the new pane is
 *             nBuffers - 1. Below 2 there is no merge to judge.
 * @param [in] op The current diff's DIFFRANGE op (see the
 *             Merge20FocusOp* constants above).
 * @return Source panes for `CMergeDoc::ResultChooseSources`, in
 *         order; empty when the action does not apply to this diff
 *         (Split/Regroup, or a judgement shown for another diff kind).
 */
std::vector<int> Merge20FocusActionSrcPanes(Merge20FocusAction action, int nBuffers, int op);

/**
 * @brief Whether a focus-dock button is shown for the current diff.
 * @return False hides the button (HTML shows the judgement buttons
 *         contextually: deleted blocks get InsertOld/Reject, changed
 *         blocks get New/Old/Both, added blocks get none; Split and
 *         Regroup are always shown). Reject is 2-way only: a 3-way
 *         one-sided diff has two empty sides, so "reject" is ambiguous.
 */
bool Merge20FocusActionVisible(Merge20FocusAction action, int op, int nBuffers);

/**
 * @brief Clamp a focus-view top line to its buffer.
 * @param [in] nTopLine Requested top line (a scroll offset from the
 *             detail pane, already relative to the diff start).
 * @param [in] nLineCount Lines in the focus view's segment buffer.
 * @return The top line to scroll to: 0 when there is nothing to
 *         show, otherwise clamped to [0, nLineCount - 1].
 */
int Merge20FocusClampTopLine(int nTopLine, int nLineCount);
