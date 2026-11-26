// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Patrol_test.cpp
 *
 * @brief Merge20 (Phase 4 D3) unit tests for the unreviewed-only
 * patrol index arithmetic.
 *
 * The expected values are the ground truth produced by running the
 * HTML tool's own `navFile` TODO-only branch (17_v17.15.html, lines
 * 1420-1429) in Node: `cand` is the list of TODO indices, forward
 * takes the first candidate after the current index (wrapping to
 * cand[0]), backward takes the last candidate before it (wrapping to
 * cand[cand.length - 1]), no candidate means no move (the HTML toast
 * "未確認のファイルはありません"), and the current row being the
 * only candidate also means no move. The C++ spelling differs in
 * two places, both deliberate: no candidate returns -1 (the callers
 * show "No unreviewed files" instead of a toast), and the current
 * row being the only candidate returns `current` (the callers treat
 * "target == current" as no move, as the HTML early return does).
 *
 * This file is ASCII; the pure module is MFC-free, so these tests
 * run in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Patrol.h"

namespace
{

using Targets = std::vector<bool>;

// [TODO, DONE, TODO, DONE]: the mixed-review fixture used by the
// Node ground truth run.
Targets Mixed()
{
	return { true, false, true, false };
}

TEST(Merge20Patrol, FindNextWrapsForward)
{
	// Last TODO (2) -> wraps to the first TODO (0).
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), 2, 1), 0);
	// Past the end (3, DONE) -> wraps to the first TODO (0).
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), 3, 1), 0);
	// From a DONE row (1) -> the next TODO (2).
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), 1, 1), 2);
	// From the first TODO (0) -> the next TODO (2).
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), 0, 1), 2);
}

TEST(Merge20Patrol, FindNextWrapsBackward)
{
	// First TODO (0) -> wraps to the last TODO (2).
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), 0, -1), 2);
	// Before the start (1, DONE) -> the previous TODO (0).
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), 1, -1), 0);
	// From the last TODO (2) -> the previous TODO (0).
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), 2, -1), 0);
	// Past the end (3, DONE) -> the last TODO (2).
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), 3, -1), 2);
}

TEST(Merge20Patrol, FindNextNoCurrentRow)
{
	// No current row: forward starts at the first TODO, backward at
	// the last TODO (HTML navFile with i == -1).
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), -1, 1), 0);
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), -1, -1), 2);
	// Out-of-range current rows are treated as no current row.
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), 99, 1), 0);
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), -5, -1), 2);
}

TEST(Merge20Patrol, FindNextNoTargetOrOnlyTarget)
{
	// No TODO row at all: -1 (the callers show "No unreviewed
	// files"; the HTML tool shows its toast here).
	EXPECT_EQ(Merge20FindNextTarget({ false, false }, 0, 1), -1);
	EXPECT_EQ(Merge20FindNextTarget({ false, false }, 0, -1), -1);
	EXPECT_EQ(Merge20FindNextTarget({}, -1, 1), -1);
	// The current row is the only TODO row: returns the current row
	// (the callers treat "target == current" as no move, as the
	// HTML early return does).
	EXPECT_EQ(Merge20FindNextTarget({ true }, 0, 1), 0);
	EXPECT_EQ(Merge20FindNextTarget({ true }, 0, -1), 0);
	// A single TODO row elsewhere is still reached.
	EXPECT_EQ(Merge20FindNextTarget({ false, true, false }, 0, 1), 1);
	EXPECT_EQ(Merge20FindNextTarget({ false, true, false }, 2, -1), 1);
}

TEST(Merge20Patrol, FindNextAllTargets)
{
	EXPECT_EQ(Merge20FindNextTarget({ true, true, true }, 1, 1), 2);
	EXPECT_EQ(Merge20FindNextTarget({ true, true, true }, 1, -1), 0);
	EXPECT_EQ(Merge20FindNextTarget({ true, true, true }, 2, 1), 0);
	EXPECT_EQ(Merge20FindNextTarget({ true, true, true }, 0, -1), 2);
}

TEST(Merge20Patrol, FindNextInvalidDirection)
{
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), 0, 0), -1);
	EXPECT_EQ(Merge20FindNextTarget(Mixed(), 0, 2), -1);
}

TEST(Merge20Patrol, HasOtherTarget)
{
	EXPECT_TRUE(Merge20HasOtherTarget(Mixed(), 0));
	EXPECT_TRUE(Merge20HasOtherTarget(Mixed(), 1));
	EXPECT_TRUE(Merge20HasOtherTarget(Mixed(), -1));
	EXPECT_TRUE(Merge20HasOtherTarget({ false, true, false }, -1));
	// The current row is the only target: no move exists.
	EXPECT_FALSE(Merge20HasOtherTarget({ true }, 0));
	// No target at all: no move exists.
	EXPECT_FALSE(Merge20HasOtherTarget({ false, false }, 0));
	EXPECT_FALSE(Merge20HasOtherTarget({}, -1));
}

TEST(Merge20Patrol, CountAndOrdinal)
{
	EXPECT_EQ(Merge20CountTargets(Mixed()), 2);
	EXPECT_EQ(Merge20CountTargets({ false, false }), 0);
	EXPECT_EQ(Merge20CountTargets({}), 0);
	EXPECT_EQ(Merge20CountTargets({ true, true, true }), 3);
	// Ordinals among the targets (the `n` of the (n/m) display).
	EXPECT_EQ(Merge20OrdinalOfTarget(Mixed(), 0), 1);
	EXPECT_EQ(Merge20OrdinalOfTarget(Mixed(), 2), 2);
	// A DONE row is not a target: 0 (the `n` of "(0/m)").
	EXPECT_EQ(Merge20OrdinalOfTarget(Mixed(), 1), 0);
	EXPECT_EQ(Merge20OrdinalOfTarget(Mixed(), 3), 0);
	EXPECT_EQ(Merge20OrdinalOfTarget(Mixed(), -1), 0);
	EXPECT_EQ(Merge20OrdinalOfTarget({ false, true, false }, 1), 1);
}

} // namespace
