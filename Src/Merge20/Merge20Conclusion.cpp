// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Conclusion.cpp
 *
 * @brief Implementation of the per-diff conclusion mapping
 * (Merge20 best-workflow B1).
 *
 * This file stays ASCII: it computes pane selections and linkage
 * outcomes only and never formats user-visible text.
 */
#include "pch.h"
#include "Merge20/Merge20Conclusion.h"

namespace
{

/** @brief The "new" pane: the last compared pane. */
int ConclusionNewPane(int nBuffers)
{
	return nBuffers - 1;
}

} // namespace

std::vector<int> Merge20ConclusionSrcPanes(Merge20DiffConclusion conclusion, int nBuffers, int op)
{
	if (nBuffers < 2)
		return {};
	const int nNew = ConclusionNewPane(nBuffers);
	switch (conclusion)
	{
	case Merge20DiffConclusion::UseOld:
		return { 0 };
	case Merge20DiffConclusion::UseNew:
		return { nNew };
	case Merge20DiffConclusion::UseBoth:
		return { 0, nNew };
	case Merge20DiffConclusion::Reject:
		// One-sided diff: take the side that contributes no lines,
		// so the result holds the "<No Lines>" placeholder and saves
		// nothing for the block (HTML KEEP_NEW on a deleted block,
		// same mapping as Merge20FocusAction::Reject). A changed
		// block has no empty side, and a 3-way one-sided diff has
		// two empty sides, so Reject is expressed there by clearing
		// the selection (the caller passes the empty list).
		if (nBuffers != 2)
			return {};
		if (op == Merge20ConclusionOpOldOnly)
			return { 1 };
		if (op == Merge20ConclusionOpNewOnly)
			return { 0 };
		return {};
	case Merge20DiffConclusion::Keep:
	case Merge20DiffConclusion::Undecided:
	default:
		return {};
	}
}

bool Merge20ConclusionChangesResult(Merge20DiffConclusion conclusion)
{
	return conclusion != Merge20DiffConclusion::Keep;
}

Merge20ConclusionLinkage Merge20ConclusionLinkageFor(Merge20DiffConclusion conclusion, bool isTodo, bool isNewOnlyFile2Way)
{
	Merge20ConclusionLinkage link;
	switch (conclusion)
	{
	case Merge20DiffConclusion::Undecided:
		// Clearing a conclusion changes nothing.
		break;
	case Merge20DiffConclusion::Reject:
		// Not adopted: the file drops out of the output, and the
		// review state is left as it was (gap analysis B1 rule).
		link.setInclude = true;
		link.includeValue = false;
		break;
	case Merge20DiffConclusion::UseOld:
	case Merge20DiffConclusion::UseNew:
	case Merge20DiffConclusion::UseBoth:
	case Merge20DiffConclusion::Keep:
	default:
		// A concluded diff: TODO becomes DONE, and the file is
		// included in the output, except that a 2-way new-only
		// file is never turned back ON (S2 rule).
		link.setReviewDone = isTodo;
		if (!isNewOnlyFile2Way)
		{
			link.setInclude = true;
			link.includeValue = true;
		}
		break;
	}
	return link;
}
