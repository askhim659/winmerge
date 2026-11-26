// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Slim_test.cpp
 *
 * @brief Merge20 (Phase 4 D9) unit tests for the slim-list helpers.
 *
 * The expected values are the ground truth from the HTML tool's
 * slim list (`tgSlim()`, 17_v17.15.html line 1390, CSS lines
 * 133-141): the visible columns are the check box, the file name
 * and the review-state tag, in that order; the review column is
 * 126px wide and the tag wraps instead of being clipped.
 *
 * This file is ASCII; the pure module is MFC-free, so these tests
 * run in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Slim.h"

TEST(Merge20Slim, VisibleColumnsAreCheckBoxFileNameAndReview)
{
	const std::vector<std::string> cols = Merge20Slim::VisibleColumns();
	ASSERT_EQ(cols.size(), 3u);
	EXPECT_EQ(cols[0], "Name");
	EXPECT_EQ(cols[1], "Soutput");
	EXPECT_EQ(cols[2], "Sreview");
}

TEST(Merge20Slim, IsVisibleColumnKeepsOnlyTheSlimSet)
{
	EXPECT_TRUE(Merge20Slim::IsVisibleColumn("Name"));
	EXPECT_TRUE(Merge20Slim::IsVisibleColumn("Soutput"));
	EXPECT_TRUE(Merge20Slim::IsVisibleColumn("Sreview"));
	// The columns the HTML slim list hides: action category, risk,
	// summary (which carries the old/changed/new counts), path,
	// status and the memo column.
	EXPECT_FALSE(Merge20Slim::IsVisibleColumn("Saction"));
	EXPECT_FALSE(Merge20Slim::IsVisibleColumn("Srisk"));
	EXPECT_FALSE(Merge20Slim::IsVisibleColumn("Ssummary"));
	EXPECT_FALSE(Merge20Slim::IsVisibleColumn("Path"));
	EXPECT_FALSE(Merge20Slim::IsVisibleColumn("Status"));
	EXPECT_FALSE(Merge20Slim::IsVisibleColumn("Smemo"));
	EXPECT_FALSE(Merge20Slim::IsVisibleColumn(""));
}

TEST(Merge20Slim, ColumnOrdersIsTheNamedFormat)
{
	// The named format DirViewColItems::LoadColumnOrders accepts:
	// an integer prefix (ignored), '|', then the visible regNames
	// in display order.
	EXPECT_EQ(Merge20Slim::ColumnOrders(), "0 | Name Soutput Sreview");
}

TEST(Merge20Slim, ReviewColumnWidthIs126)
{
	EXPECT_EQ(Merge20Slim::kReviewColumnWidth, 126);
	EXPECT_EQ(Merge20Slim::kOutputColumnWidth, 48);
}

TEST(Merge20Slim, LabelNeedsWrapAtTheColumnEdge)
{
	EXPECT_FALSE(Merge20Slim::LabelNeedsWrap(126));
	EXPECT_TRUE(Merge20Slim::LabelNeedsWrap(127));
	EXPECT_FALSE(Merge20Slim::LabelNeedsWrap(0));
}
