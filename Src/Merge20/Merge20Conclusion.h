// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Conclusion.h
 *
 * @brief Per-diff explicit conclusion (Merge20 best-workflow B1,
 * pillar 1: "no diff left without an explicit conclusion").
 *
 * Pure module (no MFC, no `_()`): the six conclusion states of one
 * difference, their mapping to the source panes passed to
 * `CMergeDoc::ResultChooseSources`, and the single linkage rule
 * from a conclusion to the file's review state and output checkbox.
 * The same code is compiled into WinMergeU and into the GoogleTest
 * runner.
 *
 * The states are the B1 vocabulary (gap analysis section 4.2 B1):
 * Undecided / UseOld / UseNew / UseBoth / Reject / Keep. The first
 * four spell the HTML tool's `setHunk` strategies
 * (17_v17.15.html, lines 1492-1503): a deleted block offers
 * INSERT_OLD ("Use old") / KEEP_NEW ("Reject"), a changed block
 * offers KEEP_NEW ("Use new") / REPLACE_WITH_OLD ("Use old") /
 * BOTH_OLD_NEW ("Use both"). Two states are new in the fork:
 * - Reject on a changed block has no empty side (both panes have
 *   lines), so it clears the selection instead: the segment goes
 *   back to the unresolved conflict display and nothing is adopted.
 *   The file's output checkbox is turned OFF by the linkage, so
 *   nothing is emitted for that file; this is no worse than leaving
 *   the diff Unresolved, which shows and saves the same conflict
 *   block today.
 * - Keep ("keep as is") leaves the result segment untouched and
 *   only records that the person looked at the diff and decided the
 *   current result stands. It is a concluded state (it counts
 *   towards "judged X of Y diffs"), unlike Undecided.
 *
 * The linkage rule is the gap analysis B1 rule, applied in exactly
 * one place (`CMergeDoc::Merge20SetDiffConclusion`): every
 * conclusion except Reject turns the output checkbox ON (except for
 * a 2-way new-only file, S2 rule) and moves a TODO review to DONE;
 * Reject turns the output checkbox OFF and leaves the review state
 * unchanged; clearing a conclusion back to Undecided changes
 * nothing. This deliberately differs from the HTML tool in one
 * place: HTML's `setHunk` leaves the review/output untouched when
 * the strategy is KEEP_NEW, because KEEP_NEW is the HTML default
 * ("no decision yet"). In the fork, UseNew and Reject are explicit
 * conclusions distinct from Undecided, so the linkage applies to
 * them; only Undecided itself means "no decision yet".
 */
#pragma once

#include <vector>

/**
 * @brief Explicit conclusion of one difference (B1).
 */
enum class Merge20DiffConclusion
{
	Undecided = 0, /**< No conclusion yet (initial state of every diff) */
	UseOld,        /**< Adopt the old side's lines */
	UseNew,        /**< Adopt the new side's lines */
	UseBoth,       /**< Adopt both sides, old first */
	Reject,        /**< Adopt neither side's lines */
	Keep,          /**< Keep the current result as is */
};

/**
 * @brief DIFFRANGE op values (Src/DiffList.h, OP_TYPE). The pure
 * module must not include DiffList.h, so the values are mirrored
 * here; they are stable upstream enum values (OP_NONE = 0,
 * OP_1STONLY = 1, OP_2NDONLY = 2, OP_3RDONLY = 3, OP_DIFF = 4,
 * OP_TRIVIAL = 5).
 */
constexpr int Merge20ConclusionOpNone = 0;
constexpr int Merge20ConclusionOpOldOnly = 1;
constexpr int Merge20ConclusionOpNewOnly = 2;
constexpr int Merge20ConclusionOpThirdOnly = 3;
constexpr int Merge20ConclusionOpDiff = 4;
constexpr int Merge20ConclusionOpTrivial = 5;

/**
 * @brief Source panes for a conclusion, for
 * `CMergeDoc::ResultChooseSources`, in order.
 * @param [in] conclusion The conclusion being applied.
 * @param [in] nBuffers Compared panes (2 or 3); the new pane is
 *             nBuffers - 1. Below 2 there is no merge to judge.
 * @param [in] op The diff's DIFFRANGE op (see the
 *             Merge20ConclusionOp* constants above).
 * @return Source panes, in order; empty when the conclusion has no
 *         selectable side: Keep and Undecided (no result change),
 *         and Reject on a changed block or in 3-way (cleared by the
 *         caller with an empty selection instead).
 */
std::vector<int> Merge20ConclusionSrcPanes(Merge20DiffConclusion conclusion, int nBuffers, int op);

/**
 * @brief Whether applying the conclusion rewrites the result
 * segment at all.
 * @return False only for Keep (the result is left untouched).
 *         True for the other five states: UseOld/UseNew/UseBoth
 *         rewrite the segment with the selected panes, Reject
 *         rewrites it with the empty side's panes (or clears the
 *         selection on a changed block), and Undecided clears the
 *         selection so the diff goes back to unresolved.
 */
bool Merge20ConclusionChangesResult(Merge20DiffConclusion conclusion);

/**
 * @brief Outcome of the conclusion -> file linkage (B1, gap
 * analysis section 4.2 B1). Every entry point that sets a
 * conclusion applies this one rule; no other place changes the
 * review state or the output checkbox for a conclusion.
 */
struct Merge20ConclusionLinkage
{
	bool setReviewDone = false; /**< Move a TODO review to DONE */
	bool setInclude = false;    /**< Change the output checkbox */
	bool includeValue = false;  /**< New output checkbox value */
};

/**
 * @brief The linkage outcome for one conclusion.
 * @param [in] conclusion The conclusion just set.
 * @param [in] isTodo True when the file's review state is TODO now
 *             (only TODO moves to DONE).
 * @param [in] isNewOnlyFile2Way True for a 2-way new-only file
 *             (S2 rule: DONE never turns its output back ON).
 * @return Undecided: no change. Reject: output OFF, review
 *         unchanged. Any other conclusion: TODO -> DONE, and
 *         output ON unless this is a 2-way new-only file.
 */
Merge20ConclusionLinkage Merge20ConclusionLinkageFor(Merge20DiffConclusion conclusion, bool isTodo, bool isNewOnlyFile2Way);
