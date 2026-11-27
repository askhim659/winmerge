// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Conclusion_test.cpp
 *
 * @brief Merge20 (best-workflow B1) unit tests for the per-diff
 * conclusion mapping and linkage rule.
 *
 * The expected pane selections are the ground truth of the HTML
 * tool's `setHunk` strategies (17_v17.15.html, lines 1492-1503):
 * INSERT_OLD is {0}, KEEP_NEW on a changed block is {1},
 * REPLACE_WITH_OLD is {0}, BOTH_OLD_NEW is {0, 1}, and KEEP_NEW
 * on a one-sided block ("Reject") is the side that contributes no
 * lines ({1} for an old-only block, {0} for a new-only block).
 * The linkage expectations are the B1 rule of gap analysis
 * section 4.2 B1: every conclusion except Reject turns the output
 * ON (except for a 2-way new-only file) and moves TODO to DONE;
 * Reject turns the output OFF and leaves the review unchanged;
 * Undecided changes nothing.
 *
 * This file is ASCII; the pure module is MFC-free, so these tests
 * run in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Conclusion.h"

namespace
{

using Panes = std::vector<int>;

TEST(Merge20Conclusion, UseOldTakesOldPane)
{
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::UseOld, 2, Merge20ConclusionOpDiff), (Panes{ 0 }));
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::UseOld, 2, Merge20ConclusionOpOldOnly), (Panes{ 0 }));
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::UseOld, 3, Merge20ConclusionOpDiff), (Panes{ 0 }));
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::UseOld, 1, Merge20ConclusionOpDiff), (Panes{}));
}

TEST(Merge20Conclusion, UseNewTakesNewPane)
{
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::UseNew, 2, Merge20ConclusionOpDiff), (Panes{ 1 }));
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::UseNew, 2, Merge20ConclusionOpNewOnly), (Panes{ 1 }));
	// 3-way: the new pane is the last one.
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::UseNew, 3, Merge20ConclusionOpDiff), (Panes{ 2 }));
}

TEST(Merge20Conclusion, UseBothTakesOldThenNew)
{
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::UseBoth, 2, Merge20ConclusionOpDiff), (Panes{ 0, 1 }));
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::UseBoth, 3, Merge20ConclusionOpDiff), (Panes{ 0, 2 }));
}

TEST(Merge20Conclusion, RejectTakesTheEmptySide)
{
	// Old-only: the new pane contributes no lines.
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::Reject, 2, Merge20ConclusionOpOldOnly), (Panes{ 1 }));
	// New-only: the old pane contributes no lines.
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::Reject, 2, Merge20ConclusionOpNewOnly), (Panes{ 0 }));
	// Changed block: no empty side, so the selection is cleared.
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::Reject, 2, Merge20ConclusionOpDiff), (Panes{}));
	// 3-way one-sided: two empty sides, so no selection.
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::Reject, 3, Merge20ConclusionOpOldOnly), (Panes{}));
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::Reject, 3, Merge20ConclusionOpThirdOnly), (Panes{}));
}

TEST(Merge20Conclusion, KeepAndUndecidedHaveNoSelection)
{
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::Keep, 2, Merge20ConclusionOpDiff), (Panes{}));
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::Keep, 2, Merge20ConclusionOpOldOnly), (Panes{}));
	EXPECT_EQ(Merge20ConclusionSrcPanes(Merge20DiffConclusion::Undecided, 2, Merge20ConclusionOpDiff), (Panes{}));
}

TEST(Merge20Conclusion, ChangesResultOnlyForKeep)
{
	// Only Keep leaves the result segment untouched.
	EXPECT_FALSE(Merge20ConclusionChangesResult(Merge20DiffConclusion::Keep));
	EXPECT_TRUE(Merge20ConclusionChangesResult(Merge20DiffConclusion::UseOld));
	EXPECT_TRUE(Merge20ConclusionChangesResult(Merge20DiffConclusion::UseNew));
	EXPECT_TRUE(Merge20ConclusionChangesResult(Merge20DiffConclusion::UseBoth));
	EXPECT_TRUE(Merge20ConclusionChangesResult(Merge20DiffConclusion::Reject));
	EXPECT_TRUE(Merge20ConclusionChangesResult(Merge20DiffConclusion::Undecided));
}

TEST(Merge20Conclusion, LinkageAdoptMovesTodoToDoneAndOutputOn)
{
	// TODO file, ordinary file: review DONE, output ON.
	const Merge20ConclusionLinkage link =
		Merge20ConclusionLinkageFor(Merge20DiffConclusion::UseOld, true, false);
	EXPECT_TRUE(link.setReviewDone);
	EXPECT_TRUE(link.setInclude);
	EXPECT_TRUE(link.includeValue);
	// The same holds for the other adopting conclusions.
	for (Merge20DiffConclusion c : { Merge20DiffConclusion::UseNew,
		Merge20DiffConclusion::UseBoth, Merge20DiffConclusion::Keep })
	{
		const Merge20ConclusionLinkage l = Merge20ConclusionLinkageFor(c, true, false);
		EXPECT_TRUE(l.setReviewDone);
		EXPECT_TRUE(l.setInclude);
		EXPECT_TRUE(l.includeValue);
	}
}

TEST(Merge20Conclusion, LinkageAdoptKeepsNonTodoReview)
{
	// A file already HOLD stays HOLD; only the output turns ON.
	const Merge20ConclusionLinkage link =
		Merge20ConclusionLinkageFor(Merge20DiffConclusion::UseNew, false, false);
	EXPECT_FALSE(link.setReviewDone);
	EXPECT_TRUE(link.setInclude);
	EXPECT_TRUE(link.includeValue);
}

TEST(Merge20Conclusion, LinkageAdoptNeverTurnsOnNewOnlyFile)
{
	// 2-way new-only file (S2 rule): the review still moves to DONE,
	// but the output checkbox is left alone.
	const Merge20ConclusionLinkage link =
		Merge20ConclusionLinkageFor(Merge20DiffConclusion::UseBoth, true, true);
	EXPECT_TRUE(link.setReviewDone);
	EXPECT_FALSE(link.setInclude);
}

TEST(Merge20Conclusion, LinkageRejectTurnsOutputOffOnly)
{
	const Merge20ConclusionLinkage link =
		Merge20ConclusionLinkageFor(Merge20DiffConclusion::Reject, true, false);
	EXPECT_FALSE(link.setReviewDone);
	EXPECT_TRUE(link.setInclude);
	EXPECT_FALSE(link.includeValue);
	// The new-only exception does not matter for Reject: OFF is OFF.
	const Merge20ConclusionLinkage linkNewOnly =
		Merge20ConclusionLinkageFor(Merge20DiffConclusion::Reject, false, true);
	EXPECT_FALSE(linkNewOnly.setReviewDone);
	EXPECT_TRUE(linkNewOnly.setInclude);
	EXPECT_FALSE(linkNewOnly.includeValue);
}

TEST(Merge20Conclusion, LinkageUndecidedChangesNothing)
{
	const Merge20ConclusionLinkage link =
		Merge20ConclusionLinkageFor(Merge20DiffConclusion::Undecided, true, false);
	EXPECT_FALSE(link.setReviewDone);
	EXPECT_FALSE(link.setInclude);
}

} // namespace
