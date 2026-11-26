// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Focus_test.cpp
 *
 * @brief Merge20 (Phase 4 D4) unit tests for the focus dock
 * judgement mapping.
 *
 * The expected values are the ground truth of the HTML tool's
 * `setHunk` strategies (17_v17.15.html): lines 1502-1503 show
 * INSERT_OLD / KEEP_NEW for a deleted block, line 1503 shows
 * KEEP_NEW / REPLACE_WITH_OLD / BOTH_OLD_NEW for a changed block,
 * and lines 1631-1635 show the same two button sets in the focus
 * dock; an added block gets no buttons (the new side is kept
 * automatically). The C++ spelling differs in two places, both
 * deliberate: the strategies are source-pane selections for
 * `CMergeDoc::ResultChooseSources` (INSERT_OLD is {0}, KEEP_NEW on
 * a changed block is {1}, REPLACE_WITH_OLD is {0}, BOTH_OLD_NEW
 * is {0, 1}), and KEEP_NEW on a one-sided block ("Reject") is the
 * side that contributes no lines ({1} for an old-only block, {0}
 * for a new-only block), which is what makes the result hold the
 * "<No Lines>" placeholder and save nothing for the block.
 *
 * This file is ASCII; the pure module is MFC-free, so these tests
 * run in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Focus.h"

namespace
{

using Panes = std::vector<int>;

TEST(Merge20Focus, InsertOldTakesOldPaneOnly)
{
	// Old-only diff: INSERT_OLD takes the old pane.
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::InsertOld, 2, Merge20FocusOpOldOnly), (Panes{ 0 }));
	// Any other diff kind: the button is hidden, so no selection.
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::InsertOld, 2, Merge20FocusOpDiff), (Panes{}));
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::InsertOld, 2, Merge20FocusOpNewOnly), (Panes{}));
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::InsertOld, 2, Merge20FocusOpNone), (Panes{}));
}

TEST(Merge20Focus, RejectTakesTheEmptySide)
{
	// Old-only: the new pane contributes no lines.
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::Reject, 2, Merge20FocusOpOldOnly), (Panes{ 1 }));
	// New-only: the old pane contributes no lines.
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::Reject, 2, Merge20FocusOpNewOnly), (Panes{ 0 }));
	// Changed block: Reject has no meaning (both sides have lines).
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::Reject, 2, Merge20FocusOpDiff), (Panes{}));
	// 3-way one-sided: two empty sides, so no selection.
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::Reject, 3, Merge20FocusOpOldOnly), (Panes{}));
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::Reject, 3, Merge20FocusOpThirdOnly), (Panes{}));
}

TEST(Merge20Focus, ChangedBlockSelections)
{
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::ChooseNew, 2, Merge20FocusOpDiff), (Panes{ 1 }));
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::ChooseOld, 2, Merge20FocusOpDiff), (Panes{ 0 }));
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::ChooseBoth, 2, Merge20FocusOpDiff), (Panes{ 0, 1 }));
	// 3-way: the new pane is the last one, "both" is old + last.
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::ChooseNew, 3, Merge20FocusOpDiff), (Panes{ 2 }));
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::ChooseOld, 3, Merge20FocusOpDiff), (Panes{ 0 }));
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::ChooseBoth, 3, Merge20FocusOpDiff), (Panes{ 0, 2 }));
}

TEST(Merge20Focus, SplitAndRegroupHaveNoSelection)
{
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::Split, 2, Merge20FocusOpDiff), (Panes{}));
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::Regroup, 2, Merge20FocusOpDiff), (Panes{}));
}

TEST(Merge20Focus, NoBuffersMeansNoSelection)
{
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::ChooseNew, 1, Merge20FocusOpDiff), (Panes{}));
	EXPECT_EQ(Merge20FocusActionSrcPanes(Merge20FocusAction::ChooseBoth, 0, Merge20FocusOpDiff), (Panes{}));
}

TEST(Merge20Focus, VisibleOldOnlyBlock)
{
	EXPECT_TRUE(Merge20FocusActionVisible(Merge20FocusAction::InsertOld, Merge20FocusOpOldOnly, 2));
	EXPECT_TRUE(Merge20FocusActionVisible(Merge20FocusAction::Reject, Merge20FocusOpOldOnly, 2));
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::ChooseNew, Merge20FocusOpOldOnly, 2));
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::ChooseOld, Merge20FocusOpOldOnly, 2));
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::ChooseBoth, Merge20FocusOpOldOnly, 2));
}

TEST(Merge20Focus, VisibleNewOnlyBlock)
{
	// Added block: only Reject (drop the new lines); no New/Old/Both.
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::InsertOld, Merge20FocusOpNewOnly, 2));
	EXPECT_TRUE(Merge20FocusActionVisible(Merge20FocusAction::Reject, Merge20FocusOpNewOnly, 2));
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::ChooseNew, Merge20FocusOpNewOnly, 2));
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::ChooseOld, Merge20FocusOpNewOnly, 2));
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::ChooseBoth, Merge20FocusOpNewOnly, 2));
}

TEST(Merge20Focus, VisibleChangedBlock)
{
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::InsertOld, Merge20FocusOpDiff, 2));
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::Reject, Merge20FocusOpDiff, 2));
	EXPECT_TRUE(Merge20FocusActionVisible(Merge20FocusAction::ChooseNew, Merge20FocusOpDiff, 2));
	EXPECT_TRUE(Merge20FocusActionVisible(Merge20FocusAction::ChooseOld, Merge20FocusOpDiff, 2));
	EXPECT_TRUE(Merge20FocusActionVisible(Merge20FocusAction::ChooseBoth, Merge20FocusOpDiff, 2));
	// Trivial diffs behave like changed blocks.
	EXPECT_TRUE(Merge20FocusActionVisible(Merge20FocusAction::ChooseNew, Merge20FocusOpTrivial, 2));
}

TEST(Merge20Focus, VisibleThirdOnlyAndNone)
{
	// 3-way one-sided: no judgement button (Reject is ambiguous).
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::InsertOld, Merge20FocusOpThirdOnly, 3));
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::Reject, Merge20FocusOpThirdOnly, 3));
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::ChooseNew, Merge20FocusOpThirdOnly, 3));
	// No diff: no judgement button.
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::InsertOld, Merge20FocusOpNone, 2));
	EXPECT_FALSE(Merge20FocusActionVisible(Merge20FocusAction::ChooseNew, Merge20FocusOpNone, 2));
}

TEST(Merge20Focus, SplitAndRegroupAlwaysVisible)
{
	EXPECT_TRUE(Merge20FocusActionVisible(Merge20FocusAction::Split, Merge20FocusOpDiff, 2));
	EXPECT_TRUE(Merge20FocusActionVisible(Merge20FocusAction::Regroup, Merge20FocusOpNone, 2));
	EXPECT_TRUE(Merge20FocusActionVisible(Merge20FocusAction::Split, Merge20FocusOpThirdOnly, 3));
}

TEST(Merge20Focus, ClampTopLine)
{
	EXPECT_EQ(Merge20FocusClampTopLine(0, 0), 0);
	EXPECT_EQ(Merge20FocusClampTopLine(5, 0), 0);
	EXPECT_EQ(Merge20FocusClampTopLine(-1, 10), 0);
	EXPECT_EQ(Merge20FocusClampTopLine(0, 10), 0);
	EXPECT_EQ(Merge20FocusClampTopLine(4, 10), 4);
	EXPECT_EQ(Merge20FocusClampTopLine(9, 10), 9);
	EXPECT_EQ(Merge20FocusClampTopLine(10, 10), 9);
	EXPECT_EQ(Merge20FocusClampTopLine(99, 10), 9);
}

} // namespace
