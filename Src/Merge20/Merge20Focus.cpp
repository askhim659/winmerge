// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Focus.cpp
 *
 * @brief Implementation of the focus dock judgement mapping
 * (Merge20 Phase 4 D4).
 *
 * This file stays ASCII: it computes pane selections only and never
 * formats user-visible text.
 */
#include "pch.h"
#include "Merge20/Merge20Focus.h"

namespace
{

/** @brief The "new" pane: the last compared pane. */
int FocusNewPane(int nBuffers)
{
	return nBuffers - 1;
}

} // namespace

std::vector<int> Merge20FocusActionSrcPanes(Merge20FocusAction action, int nBuffers, int op)
{
	if (nBuffers < 2)
		return {};
	const int nNew = FocusNewPane(nBuffers);
	switch (action)
	{
	case Merge20FocusAction::InsertOld:
		// Old-only diff: take the old pane's lines (HTML INSERT_OLD).
		// For any other diff kind the button is hidden, so there is
		// no selection to return.
		return op == Merge20FocusOpOldOnly ? std::vector<int>{ 0 } : std::vector<int>{};
	case Merge20FocusAction::Reject:
		// One-sided diff: take the side that contributes no lines,
		// so the result holds the "<No Lines>" placeholder and saves
		// nothing for the block (HTML KEEP_NEW on a deleted block).
		// 3-way one-sided diffs have two empty sides, so Reject is
		// not offered there (see Merge20FocusActionVisible).
		if (nBuffers != 2)
			return {};
		if (op == Merge20FocusOpOldOnly)
			return { 1 };
		if (op == Merge20FocusOpNewOnly)
			return { 0 };
		return {};
	case Merge20FocusAction::ChooseNew:
		return { nNew };
	case Merge20FocusAction::ChooseOld:
		return { 0 };
	case Merge20FocusAction::ChooseBoth:
		return { 0, nNew };
	case Merge20FocusAction::Split:
	case Merge20FocusAction::Regroup:
	default:
		return {};
	}
}

bool Merge20FocusActionVisible(Merge20FocusAction action, int op, int nBuffers)
{
	switch (action)
	{
	case Merge20FocusAction::InsertOld:
		return op == Merge20FocusOpOldOnly;
	case Merge20FocusAction::Reject:
		// 2-way one-sided diffs only (see above).
		return nBuffers == 2 &&
			(op == Merge20FocusOpOldOnly || op == Merge20FocusOpNewOnly);
	case Merge20FocusAction::ChooseNew:
	case Merge20FocusAction::ChooseOld:
	case Merge20FocusAction::ChooseBoth:
		return op == Merge20FocusOpDiff || op == Merge20FocusOpTrivial;
	case Merge20FocusAction::Split:
	case Merge20FocusAction::Regroup:
		// Always shown; D4 shows the buttons only, D5 wires the body.
		return true;
	default:
		return false;
	}
}

int Merge20FocusClampTopLine(int nTopLine, int nLineCount)
{
	if (nLineCount <= 0)
		return 0;
	if (nTopLine < 0)
		return 0;
	if (nTopLine > nLineCount - 1)
		return nLineCount - 1;
	return nTopLine;
}
