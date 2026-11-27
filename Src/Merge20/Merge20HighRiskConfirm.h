// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20HighRiskConfirm.h
 *
 * @brief Whether a judgment on a high-risk diff or file needs a
 * confirmation dialog (Merge20 best-workflow B14, pillar 8:
 * "keyboard speed, with care for dangerous operations").
 *
 * Pure module (no MFC, no `_()`): the decision only. The same code
 * is compiled into WinMergeU and into the GoogleTest runner. The
 * dialog itself (Merge20HighRiskConfirmDlg) only displays what the
 * callers built from this decision.
 *
 * Rules (gap analysis section 4.2 B14, section 3.1, section 3.2):
 * - A judgment is destructive when it throws one side away: UseOld
 *   (the new side is dropped), UseNew (the old side is dropped)
 *   and Reject (neither side is adopted). UseBoth adopts both
 *   sides, Keep leaves the result untouched, and Undecided clears
 *   the conclusion, so none of the three needs a confirmation.
 * - The confirmation is shown exactly when the diff's risk is HIGH
 *   and the judgment is destructive. The risk axis and the learning
 *   axis are independent (section 3.1): a learning candidate on
 *   the diff never suppresses the confirmation; the candidate text
 *   is only shown inside the dialog.
 * - One dialog per judgment (acceptance (a)-(f)): a range operation
 *   (context menu over several diffs, learning acceptance over
 *   several diffs) collects every diff that needs confirmation
 *   and shows a single dialog for the whole range, never one
 *   dialog per diff. Re-applying the same conclusion to a diff
 *   that already holds it is not a new judgment, so it does not
 *   prompt again ("the same diff's consecutive operation is not
 *   repeated").
 * - Space advance (S2, folder list and file-list panel): Space
 *   cycles the file's review state blindly, so the one step that
 *   declares a high-risk file done (TODO -> DONE, which also
 *   turns its output checkbox ON) needs the confirmation. The
 *   other steps of the cycle (DONE -> HOLD, HOLD -> SPECIAL,
 *   SPECIAL -> KEEP, KEEP -> TODO) never declare a file done, so
 *   they keep the old one-key behaviour. Keys 1-5 stay
 *   unguarded: pressing '2' is an explicit choice of DONE, unlike
 *   Space, which cycles without showing which state comes next.
 */
#pragma once

#include <vector>
#include "Merge20Types.h"
#include "Merge20Conclusion.h"

/**
 * @brief Whether the conclusion throws one side away.
 * @return True for UseOld, UseNew and Reject; false for UseBoth,
 *         Keep and Undecided.
 */
bool Merge20HighRiskConfirmIsDestructive(Merge20DiffConclusion conclusion);

/**
 * @brief Whether one diff judgment needs the confirmation dialog.
 * @param [in] risk Risk of the diff (S4 per-diff risk,
 *             Merge20ClassifyDiffRisk).
 * @param [in] conclusion The conclusion about to be applied.
 * @param [in] alreadyDecided True when the diff already holds
 *             this conclusion (re-applying it is not a new
 *             judgment, so no dialog).
 * @return True exactly when risk is HIGH, the conclusion is
 *         destructive, and the diff does not already hold it.
 */
bool Merge20HighRiskConfirmNeedsConfirm(RiskLevel risk, Merge20DiffConclusion conclusion, bool alreadyDecided);

/**
 * @brief One diff targeted by a judgment operation, for the
 * once-per-judgment collection.
 */
struct Merge20HighRiskConfirmTarget
{
	int diffIndex = -1; /**< Index in the doc's diff list. */
	RiskLevel risk = RiskLevel::NONE; /**< S4 per-diff risk of the diff. */
	Merge20DiffConclusion conclusion = Merge20DiffConclusion::Undecided; /**< Conclusion about to be applied. */
	bool alreadyDecided = false; /**< True when the diff already holds this conclusion. */
};

/**
 * @brief Diff indices of the targets that need one confirmation
 * dialog, in target order.
 * @param [in] targets Every diff the operation is about to judge
 *             (already filtered to significant diffs by the
 *             caller; non-diff entries are ignored by the risk
 *             rule, not by this function).
 * @return The diff indices that need confirmation, each once, in
 *         the order the targets were given. Empty when nothing
 *         needs confirmation.
 */
std::vector<int> Merge20HighRiskConfirmCollect(const std::vector<Merge20HighRiskConfirmTarget>& targets);

/**
 * @brief Whether one Space-cycle step on a file needs the
 * confirmation dialog (S2 Space advance, B14 acceptance (f)).
 * @param [in] risk S4 risk of the file (NONE when unclassified).
 * @param [in] fromState The file's review state now.
 * @param [in] toState The state Space would move it to.
 * @return True exactly when the file is HIGH risk and the step
 *         declares it done (TODO -> DONE). Every other step
 *         proceeds without a dialog.
 */
bool Merge20HighRiskConfirmFileCycleNeedsConfirm(RiskLevel risk, ReviewState fromState, ReviewState toState);
