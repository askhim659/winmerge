// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Sniper_test.cpp
 *
 * @brief Merge20 (Phase 4 D6) unit tests for the auto sniper view
 * arithmetic.
 *
 * The expected values are the ground truth from the HTML tool's own
 * sniper behaviour (17_v17.15.html): `snZoom(d)` (line 1453) steps
 * the font by 2 within 14-28px, the diff navigation wraps around
 * both ends, and the drag/keyboard navigation moves to the
 * previous/next diff. The pixel scroll amounts (120px per line
 * step, 600px per page step) come from spec section 4(g); the
 * conversion to whole lines divides by the panel's line height
 * (at least one line for any positive amount).
 *
 * This file is ASCII; the pure module is MFC-free, so these tests
 * run in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Sniper.h"

TEST(Merge20Sniper, WrapNextMovesForward)
{
	EXPECT_EQ(Merge20SniperWrapNext(3, 0), 3);
}

TEST(Merge20Sniper, WrapNextWrapsFromLastToFirst)
{
	// No next diff (-1): wrap to the first significant diff.
	EXPECT_EQ(Merge20SniperWrapNext(-1, 0), 0);
}

TEST(Merge20Sniper, WrapNextNoDiffAtAll)
{
	EXPECT_EQ(Merge20SniperWrapNext(-1, -1), -1);
}

TEST(Merge20Sniper, WrapPrevMovesBackward)
{
	EXPECT_EQ(Merge20SniperWrapPrev(2, 5), 2);
}

TEST(Merge20Sniper, WrapPrevWrapsFromFirstToLast)
{
	// No previous diff (-1): wrap to the last significant diff.
	EXPECT_EQ(Merge20SniperWrapPrev(-1, 5), 5);
}

TEST(Merge20Sniper, WrapPrevNoDiffAtAll)
{
	EXPECT_EQ(Merge20SniperWrapPrev(-1, -1), -1);
}

TEST(Merge20Sniper, ClampFontKeepsInsideRange)
{
	EXPECT_EQ(Merge20SniperClampFont(20), 20);
	EXPECT_EQ(Merge20SniperClampFont(14), 14);
	EXPECT_EQ(Merge20SniperClampFont(28), 28);
	EXPECT_EQ(Merge20SniperClampFont(13), 14);
	EXPECT_EQ(Merge20SniperClampFont(29), 28);
	EXPECT_EQ(Merge20SniperClampFont(0), 14);
}

TEST(Merge20Sniper, StepFontStepsByTwoAndClamps)
{
	EXPECT_EQ(Merge20SniperStepFont(20, Merge20SniperFontStep), 22);
	EXPECT_EQ(Merge20SniperStepFont(20, -Merge20SniperFontStep), 18);
	// At the edges the step stops instead of passing the limit.
	EXPECT_EQ(Merge20SniperStepFont(28, Merge20SniperFontStep), 28);
	EXPECT_EQ(Merge20SniperStepFont(14, -Merge20SniperFontStep), 14);
	EXPECT_EQ(Merge20SniperStepFont(27, Merge20SniperFontStep), 28);
	EXPECT_EQ(Merge20SniperStepFont(15, -Merge20SniperFontStep), 14);
}

TEST(Merge20Sniper, ScrollLinesConvertsPixels)
{
	// 20px font: the panel line height is about 24px, so a 120px
	// line step scrolls 5 lines and a 600px page step 25 lines.
	EXPECT_EQ(Merge20SniperScrollLines(120, 24), 5);
	EXPECT_EQ(Merge20SniperScrollLines(600, 24), 25);
	// A positive amount always scrolls at least one line.
	EXPECT_EQ(Merge20SniperScrollLines(120, 200), 1);
	EXPECT_EQ(Merge20SniperScrollLines(600, 200), 3);
	// Nothing to scroll when there is no amount or no line height.
	EXPECT_EQ(Merge20SniperScrollLines(0, 24), 0);
	EXPECT_EQ(Merge20SniperScrollLines(-120, 24), 0);
	EXPECT_EQ(Merge20SniperScrollLines(120, 0), 0);
}

TEST(Merge20Sniper, DragActionNeedsFiftyPixelsHorizontal)
{
	EXPECT_EQ(Merge20SniperDragAction(-50, 0), Merge20SniperDrag::Next);
	EXPECT_EQ(Merge20SniperDragAction(50, 0), Merge20SniperDrag::Prev);
	EXPECT_EQ(Merge20SniperDragAction(-120, 10), Merge20SniperDrag::Next);
	EXPECT_EQ(Merge20SniperDragAction(120, -10), Merge20SniperDrag::Prev);
	// Shorter than 50px is not a swipe.
	EXPECT_EQ(Merge20SniperDragAction(-49, 0), Merge20SniperDrag::None);
	EXPECT_EQ(Merge20SniperDragAction(49, 0), Merge20SniperDrag::None);
	EXPECT_EQ(Merge20SniperDragAction(0, 0), Merge20SniperDrag::None);
	// A mostly-vertical drag is a scroll, not a diff move.
	EXPECT_EQ(Merge20SniperDragAction(-120, 130), Merge20SniperDrag::None);
	EXPECT_EQ(Merge20SniperDragAction(10, -120), Merge20SniperDrag::None);
}

TEST(Merge20Sniper, ToggleLayoutSwitchesBothWays)
{
	EXPECT_EQ(Merge20SniperToggleLayout(Merge20SniperLayout::Columns),
		Merge20SniperLayout::Rows);
	EXPECT_EQ(Merge20SniperToggleLayout(Merge20SniperLayout::Rows),
		Merge20SniperLayout::Columns);
}
