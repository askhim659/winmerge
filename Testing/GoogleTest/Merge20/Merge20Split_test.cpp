// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Split_test.cpp
 *
 * @brief Merge20 (Phase 4 D5) unit tests for the per-row split /
 * regroup of a diff group.
 *
 * The expected values are computed by hand from the ghost layout
 * PrimeTextBuffers() builds (MergeDoc.cpp): inside one diff, every
 * pane's real lines occupy the first lines of the apparent range
 * [dbegin, dend] and the ghost lines fill the tail, so apparent
 * line dbegin + k is real line begin[p] + k of every pane that
 * still has a line there, and a ghost line of the panes that ran
 * out. The ground truth for the HTML side is `splitHunk` /
 * `regroupHunk` (17_v17.15.html, lines 1666-1690): a group splits
 * into one hunk per row and regroups back, keeping the first row's
 * judgment (the judgment carry-over itself lives in CMergeDoc and
 * is not unit-testable here).
 *
 * This file is ASCII; the pure module is MFC-free, so these tests
 * run in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Split.h"

namespace
{

// A 2-way OP_DIFF range: 3 old lines (10-12) against 5 new lines
// (20-24), apparent lines 30-34, no ghosts on either side's own
// lines (blank -1) because both sides have lines at the start.
DIFFRANGE TwoWayDiff3v5()
{
	DIFFRANGE range;
	range.op = OP_DIFF;
	range.begin[0] = 10;
	range.end[0] = 12;
	range.begin[1] = 20;
	range.end[1] = 24;
	range.begin[2] = -1;
	range.end[2] = -1;
	range.dbegin = 30;
	range.dend = 34;
	range.blank[0] = -1;
	range.blank[1] = -1;
	range.blank[2] = -1;
	return range;
}

TEST(Merge20Split, RowCountIsLongestSide)
{
	EXPECT_EQ(Merge20SplitRowCount(TwoWayDiff3v5(), 2), 5);
	DIFFRANGE oldOnly;
	oldOnly.op = OP_1STONLY;
	oldOnly.begin[0] = 4;
	oldOnly.end[0] = 7;
	oldOnly.begin[1] = 8;
	oldOnly.end[1] = 7; // empty side: end == begin - 1
	oldOnly.begin[2] = -1;
	oldOnly.end[2] = -1;
	oldOnly.dbegin = 40;
	oldOnly.dend = 43;
	EXPECT_EQ(Merge20SplitRowCount(oldOnly, 2), 4);
	// The unused third pane (begin == end == -1) must not count.
	EXPECT_EQ(Merge20SplitRowCount(TwoWayDiff3v5(), 2), 5);
}

TEST(Merge20Split, SplittableNeedsTwoRowsAndPrimedBuffers)
{
	EXPECT_TRUE(Merge20SplitIsSplittable(TwoWayDiff3v5(), 2));
	DIFFRANGE single = TwoWayDiff3v5();
	single.end[0] = 10;
	single.end[1] = 20;
	single.dend = 30;
	EXPECT_FALSE(Merge20SplitIsSplittable(single, 2));
	DIFFRANGE trivial = TwoWayDiff3v5();
	trivial.op = OP_TRIVIAL;
	EXPECT_FALSE(Merge20SplitIsSplittable(trivial, 2));
	DIFFRANGE none = TwoWayDiff3v5();
	none.op = OP_NONE;
	EXPECT_FALSE(Merge20SplitIsSplittable(none, 2));
	// Never primed: dbegin/dend still 0 while the real ranges say 5 rows.
	DIFFRANGE unprimed = TwoWayDiff3v5();
	unprimed.dbegin = 0;
	unprimed.dend = 0;
	EXPECT_FALSE(Merge20SplitIsSplittable(unprimed, 2));
}

TEST(Merge20Split, TwoWayDiffRows)
{
	std::vector<DIFFRANGE> rows;
	ASSERT_TRUE(Merge20SplitRange(TwoWayDiff3v5(), 2, rows));
	ASSERT_EQ(rows.size(), 5u);
	// Paired rows keep OP_DIFF and tile both real ranges.
	for (int k = 0; k < 3; ++k)
	{
		EXPECT_EQ(rows[k].op, OP_DIFF);
		EXPECT_EQ(rows[k].begin[0], 10 + k);
		EXPECT_EQ(rows[k].end[0], 10 + k);
		EXPECT_EQ(rows[k].begin[1], 20 + k);
		EXPECT_EQ(rows[k].end[1], 20 + k);
		EXPECT_EQ(rows[k].dbegin, 30 + k);
		EXPECT_EQ(rows[k].dend, 30 + k);
		EXPECT_EQ(rows[k].blank[0], -1);
		EXPECT_EQ(rows[k].blank[1], -1);
	}
	// Tail rows are new-only: the old side is empty (end == begin - 1)
	// and the apparent line is a ghost line for the old pane.
	for (int k = 3; k < 5; ++k)
	{
		EXPECT_EQ(rows[k].op, OP_2NDONLY);
		EXPECT_EQ(rows[k].begin[0], 13);
		EXPECT_EQ(rows[k].end[0], 12);
		EXPECT_EQ(rows[k].begin[1], 20 + k);
		EXPECT_EQ(rows[k].end[1], 20 + k);
		EXPECT_EQ(rows[k].dbegin, 30 + k);
		EXPECT_EQ(rows[k].dend, 30 + k);
		EXPECT_EQ(rows[k].blank[0], 30 + k);
		EXPECT_EQ(rows[k].blank[1], -1);
	}
	// The rows tile the parent's apparent range contiguously.
	for (size_t k = 1; k < rows.size(); ++k)
		EXPECT_EQ(rows[k].dbegin, rows[k - 1].dend + 1);
	EXPECT_EQ(rows.front().dbegin, 30);
	EXPECT_EQ(rows.back().dend, 34);
}

TEST(Merge20Split, OldOnlyRows)
{
	DIFFRANGE range;
	range.op = OP_1STONLY;
	range.begin[0] = 4;
	range.end[0] = 7;
	range.begin[1] = 8;
	range.end[1] = 7;
	range.begin[2] = -1;
	range.end[2] = -1;
	range.dbegin = 40;
	range.dend = 43;
	range.blank[0] = -1;
	range.blank[1] = 40;
	range.blank[2] = -1;
	std::vector<DIFFRANGE> rows;
	ASSERT_TRUE(Merge20SplitRange(range, 2, rows));
	ASSERT_EQ(rows.size(), 4u);
	for (int k = 0; k < 4; ++k)
	{
		EXPECT_EQ(rows[k].op, OP_1STONLY);
		EXPECT_EQ(rows[k].begin[0], 4 + k);
		EXPECT_EQ(rows[k].end[0], 4 + k);
		EXPECT_EQ(rows[k].begin[1], 8);
		EXPECT_EQ(rows[k].end[1], 7);
		EXPECT_EQ(rows[k].dbegin, 40 + k);
		EXPECT_EQ(rows[k].dend, 40 + k);
		EXPECT_EQ(rows[k].blank[0], -1);
		EXPECT_EQ(rows[k].blank[1], 40 + k);
	}
}

TEST(Merge20Split, NewOnlyRows)
{
	DIFFRANGE range;
	range.op = OP_2NDONLY;
	range.begin[0] = 9;
	range.end[0] = 8;
	range.begin[1] = 5;
	range.end[1] = 7;
	range.begin[2] = -1;
	range.end[2] = -1;
	range.dbegin = 50;
	range.dend = 52;
	range.blank[0] = 50;
	range.blank[1] = -1;
	range.blank[2] = -1;
	std::vector<DIFFRANGE> rows;
	ASSERT_TRUE(Merge20SplitRange(range, 2, rows));
	ASSERT_EQ(rows.size(), 3u);
	for (int k = 0; k < 3; ++k)
	{
		EXPECT_EQ(rows[k].op, OP_2NDONLY);
		EXPECT_EQ(rows[k].begin[0], 9);
		EXPECT_EQ(rows[k].end[0], 8);
		EXPECT_EQ(rows[k].begin[1], 5 + k);
		EXPECT_EQ(rows[k].end[1], 5 + k);
		EXPECT_EQ(rows[k].blank[0], 50 + k);
		EXPECT_EQ(rows[k].blank[1], -1);
	}
}

TEST(Merge20Split, ThreeWayRows)
{
	// 3-way OP_DIFF: 2 / 2 / 3 lines, apparent lines 60-62.
	DIFFRANGE range;
	range.op = OP_DIFF;
	range.begin[0] = 10;
	range.end[0] = 11;
	range.begin[1] = 20;
	range.end[1] = 21;
	range.begin[2] = 30;
	range.end[2] = 32;
	range.dbegin = 60;
	range.dend = 62;
	range.blank[0] = -1;
	range.blank[1] = -1;
	range.blank[2] = -1;
	std::vector<DIFFRANGE> rows;
	ASSERT_TRUE(Merge20SplitRange(range, 3, rows));
	ASSERT_EQ(rows.size(), 3u);
	for (int k = 0; k < 2; ++k)
	{
		EXPECT_EQ(rows[k].op, OP_DIFF);
		EXPECT_EQ(rows[k].begin[0], 10 + k);
		EXPECT_EQ(rows[k].begin[1], 20 + k);
		EXPECT_EQ(rows[k].begin[2], 30 + k);
		EXPECT_EQ(rows[k].blank[0], -1);
		EXPECT_EQ(rows[k].blank[1], -1);
		EXPECT_EQ(rows[k].blank[2], -1);
	}
	// Tail row: only the third pane has a line.
	EXPECT_EQ(rows[2].op, OP_3RDONLY);
	EXPECT_EQ(rows[2].begin[0], 12);
	EXPECT_EQ(rows[2].end[0], 11);
	EXPECT_EQ(rows[2].begin[1], 22);
	EXPECT_EQ(rows[2].end[1], 21);
	EXPECT_EQ(rows[2].begin[2], 32);
	EXPECT_EQ(rows[2].end[2], 32);
	EXPECT_EQ(rows[2].blank[0], 62);
	EXPECT_EQ(rows[2].blank[1], 62);
	EXPECT_EQ(rows[2].blank[2], -1);
}

TEST(Merge20Split, SplitRangeRejectsUnsplittable)
{
	std::vector<DIFFRANGE> rows;
	DIFFRANGE single = TwoWayDiff3v5();
	single.end[0] = 10;
	single.end[1] = 20;
	single.dend = 30;
	EXPECT_FALSE(Merge20SplitRange(single, 2, rows));
	EXPECT_TRUE(rows.empty());
}

TEST(Merge20Split, GroupRoundTripAndFirstRowMatch)
{
	const DIFFRANGE parent = TwoWayDiff3v5();
	std::vector<DIFFRANGE> rows;
	ASSERT_TRUE(Merge20SplitRange(parent, 2, rows));
	const Merge20SplitGroup group = Merge20SplitGroupFromRange(parent,
		static_cast<int>(rows.size()));
	EXPECT_EQ(group.op, static_cast<int>(OP_DIFF));
	EXPECT_EQ(group.rows, 5);
	EXPECT_EQ(group.dbegin, 30);
	EXPECT_EQ(group.dend, 34);
	// The group converts back to the original range exactly.
	const DIFFRANGE restored = Merge20SplitGroupToRange(group);
	EXPECT_EQ(restored.op, parent.op);
	for (int pane = 0; pane < 3; ++pane)
	{
		EXPECT_EQ(restored.begin[pane], parent.begin[pane]);
		EXPECT_EQ(restored.end[pane], parent.end[pane]);
		EXPECT_EQ(restored.blank[pane], parent.blank[pane]);
	}
	EXPECT_EQ(restored.dbegin, parent.dbegin);
	EXPECT_EQ(restored.dend, parent.dend);
	// Only the first row matches the group (a later row does not).
	EXPECT_TRUE(Merge20SplitGroupMatchesFirstRow(group, rows[0]));
	EXPECT_FALSE(Merge20SplitGroupMatchesFirstRow(group, rows[1]));
	// The parent itself shares the first row's identity (same real
	// starts and apparent start); that is why regroup must drop the
	// group record, or the restored parent would match again.
	EXPECT_TRUE(Merge20SplitGroupMatchesFirstRow(group, parent));
}

} // namespace
