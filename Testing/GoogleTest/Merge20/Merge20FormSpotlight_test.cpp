// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20FormSpotlight_test.cpp
 *
 * @brief Merge20 (Wave 5 F4) unit tests for the FORM compare
 * spotlight: focusing, previous / next stepping over the
 * records with a visual only (wrapping around), clearing, the
 * spotlight window, the scroll-to-centre offsets, the move
 * arrow head and the callout lines (design
 * merge20-form-compare-design.md sections 2.7, 6 (4) and
 * 10 F4).
 *
 * These tests pin the acceptance conditions that do not need a
 * window: (a) focusing a record selects its shape (the canvas
 * scroll and the dimming are verified on the real machine),
 * (b) the banner's old/new previews are fed from the focused
 * record (the banner itself is verified on the real machine),
 * (c) stepping cycles only over `hasVisual` records and
 * clearing returns to the whole view. The actual GDI drawing
 * (dimming, arrow, callout, banner) is verified on the real
 * machine, not here.
 *
 * This file stays ASCII: no Japanese literals are needed here.
 */
#include "pch.h"
#include <cmath>
#include <gtest/gtest.h>
#include "Merge20/Merge20FormSpotlight.h"

namespace
{

	Merge20FormDiffRecord MakeRecord(const wchar_t* element, bool hasVisual)
	{
		Merge20FormDiffRecord record;
		record.element = element;
		record.hasVisual = hasVisual;
		return record;
	}

	// Four records: visual, non-visual, visual, visual.
	std::vector<Merge20FormDiffRecord> SampleRecords()
	{
		std::vector<Merge20FormDiffRecord> records;
		records.push_back(MakeRecord(L"field[name='a']", true));
		records.push_back(MakeRecord(L"field[name='b']", false));
		records.push_back(MakeRecord(L"field[name='c']", true));
		records.push_back(MakeRecord(L"field[name='d']", true));
		return records;
	}

	Merge20FormShape MakeShape(double x, double y, double x2, double y2)
	{
		Merge20FormShape shape;
		shape.x = x;
		shape.y = y;
		shape.x2 = x2;
		shape.y2 = y2;
		return shape;
	}

} // namespace

TEST(Merge20FormSpotlight, FocusAcceptsVisualRecord)
{
	const std::vector<Merge20FormDiffRecord> records = SampleRecords();
	Merge20FormSpotlightState state;
	EXPECT_TRUE(Merge20FormSpotlightFocus(records, 2, state));
	EXPECT_TRUE(Merge20FormSpotlightIsActive(state));
	EXPECT_EQ(state.recordIndex, 2);
	EXPECT_EQ(state.shapeKey, L"field[name='c']");
}

TEST(Merge20FormSpotlight, FocusRejectsNonVisualRecord)
{
	const std::vector<Merge20FormDiffRecord> records = SampleRecords();
	Merge20FormSpotlightState state;
	EXPECT_FALSE(Merge20FormSpotlightFocus(records, 1, state));
	EXPECT_FALSE(Merge20FormSpotlightIsActive(state));
	EXPECT_EQ(state.recordIndex, -1);
	EXPECT_TRUE(state.shapeKey.empty());
}

TEST(Merge20FormSpotlight, FocusRejectsOutOfRange)
{
	const std::vector<Merge20FormDiffRecord> records = SampleRecords();
	Merge20FormSpotlightState state;
	EXPECT_FALSE(Merge20FormSpotlightFocus(records, -1, state));
	EXPECT_FALSE(Merge20FormSpotlightFocus(records, 4, state));
	EXPECT_FALSE(Merge20FormSpotlightIsActive(state));
}

TEST(Merge20FormSpotlight, FocusSameRecordRefocusesInsteadOfClearing)
{
	const std::vector<Merge20FormDiffRecord> records = SampleRecords();
	Merge20FormSpotlightState state;
	EXPECT_TRUE(Merge20FormSpotlightFocus(records, 0, state));
	// The HTML tool re-focuses on a second click of the same row.
	EXPECT_TRUE(Merge20FormSpotlightFocus(records, 0, state));
	EXPECT_TRUE(Merge20FormSpotlightIsActive(state));
	EXPECT_EQ(state.recordIndex, 0);
}

TEST(Merge20FormSpotlight, StepForwardSkipsNonVisualAndWraps)
{
	const std::vector<Merge20FormDiffRecord> records = SampleRecords();
	Merge20FormSpotlightState state;
	EXPECT_TRUE(Merge20FormSpotlightFocus(records, 0, state));
	EXPECT_EQ(Merge20FormSpotlightStep(records, state, 1), 2);
	EXPECT_TRUE(Merge20FormSpotlightFocus(records, 2, state));
	EXPECT_EQ(Merge20FormSpotlightStep(records, state, 1), 3);
	EXPECT_TRUE(Merge20FormSpotlightFocus(records, 3, state));
	// Past the last visual record it wraps to the first one.
	EXPECT_EQ(Merge20FormSpotlightStep(records, state, 1), 0);
}

TEST(Merge20FormSpotlight, StepBackwardSkipsNonVisualAndWraps)
{
	const std::vector<Merge20FormDiffRecord> records = SampleRecords();
	Merge20FormSpotlightState state;
	EXPECT_TRUE(Merge20FormSpotlightFocus(records, 0, state));
	// Before the first visual record it wraps to the last one.
	EXPECT_EQ(Merge20FormSpotlightStep(records, state, -1), 3);
	EXPECT_TRUE(Merge20FormSpotlightFocus(records, 2, state));
	EXPECT_EQ(Merge20FormSpotlightStep(records, state, -1), 0);
}

TEST(Merge20FormSpotlight, StepWithoutFocusStartsAtFirstOrLastVisual)
{
	const std::vector<Merge20FormDiffRecord> records = SampleRecords();
	Merge20FormSpotlightState state;
	EXPECT_EQ(Merge20FormSpotlightStep(records, state, 1), 0);
	EXPECT_EQ(Merge20FormSpotlightStep(records, state, -1), 3);
}

TEST(Merge20FormSpotlight, StepReturnsMinusOneWhenNothingHasVisual)
{
	std::vector<Merge20FormDiffRecord> records;
	records.push_back(MakeRecord(L"field[name='a']", false));
	records.push_back(MakeRecord(L"field[name='b']", false));
	Merge20FormSpotlightState state;
	EXPECT_EQ(Merge20FormSpotlightStep(records, state, 1), -1);
	EXPECT_EQ(Merge20FormSpotlightStep(records, state, -1), -1);
	EXPECT_EQ(Merge20FormSpotlightFirstVisual(records), -1);
	EXPECT_TRUE(Merge20FormSpotlightVisualIndices(records).empty());
}

TEST(Merge20FormSpotlight, ClearReturnsToWholeView)
{
	const std::vector<Merge20FormDiffRecord> records = SampleRecords();
	Merge20FormSpotlightState state;
	EXPECT_TRUE(Merge20FormSpotlightFocus(records, 3, state));
	Merge20FormSpotlightClear(state);
	EXPECT_FALSE(Merge20FormSpotlightIsActive(state));
	EXPECT_EQ(state.recordIndex, -1);
	EXPECT_TRUE(state.shapeKey.empty());
}

TEST(Merge20FormSpotlight, VisualIndicesListOnlyVisualRecords)
{
	const std::vector<Merge20FormDiffRecord> records = SampleRecords();
	EXPECT_EQ(Merge20FormSpotlightFirstVisual(records), 0);
	const std::vector<int> indices = Merge20FormSpotlightVisualIndices(records);
	ASSERT_EQ(indices.size(), 3u);
	EXPECT_EQ(indices[0], 0);
	EXPECT_EQ(indices[1], 2);
	EXPECT_EQ(indices[2], 3);
}

TEST(Merge20FormSpotlight, FindShapeIndex)
{
	std::vector<Merge20FormShape> shapes;
	shapes.push_back(MakeShape(0.0, 0.0, 10.0, 10.0));
	shapes[0].key = L"field[name='a']";
	shapes.push_back(MakeShape(20.0, 20.0, 30.0, 30.0));
	shapes[1].key = L"field[name='b']";
	EXPECT_EQ(Merge20FormSpotlightFindShapeIndex(shapes, L"field[name='b']"), 1);
	EXPECT_EQ(Merge20FormSpotlightFindShapeIndex(shapes, L"field[name='missing']"), -1);
}

TEST(Merge20FormSpotlight, WindowRectCoversBothSidesWithPadding)
{
	const Merge20FormShape oldShape = MakeShape(100.0, 100.0, 200.0, 150.0);
	const Merge20FormShape newShape = MakeShape(112.0, 100.0, 212.0, 150.0);
	const Merge20FormSpotlightWindow window =
		Merge20FormSpotlightWindowRect(&oldShape, &newShape);
	EXPECT_DOUBLE_EQ(window.x, 64.0);
	EXPECT_DOUBLE_EQ(window.y, 52.0);
	EXPECT_DOUBLE_EQ(window.width, 184.0);
	EXPECT_DOUBLE_EQ(window.height, 146.0);
}

TEST(Merge20FormSpotlight, WindowRectSingleSide)
{
	const Merge20FormShape newShape = MakeShape(100.0, 100.0, 200.0, 150.0);
	const Merge20FormSpotlightWindow window =
		Merge20FormSpotlightWindowRect(nullptr, &newShape);
	EXPECT_DOUBLE_EQ(window.x, 64.0);
	EXPECT_DOUBLE_EQ(window.y, 52.0);
	EXPECT_DOUBLE_EQ(window.width, 172.0);
	EXPECT_DOUBLE_EQ(window.height, 146.0);
}

TEST(Merge20FormSpotlight, WindowRectClampsToOriginAndMinimumSize)
{
	const Merge20FormShape shape = MakeShape(10.0, 10.0, 20.0, 20.0);
	const Merge20FormSpotlightWindow window =
		Merge20FormSpotlightWindowRect(&shape, nullptr);
	EXPECT_DOUBLE_EQ(window.x, 0.0);
	EXPECT_DOUBLE_EQ(window.y, 0.0);
	EXPECT_DOUBLE_EQ(window.width, 80.0);
	EXPECT_DOUBLE_EQ(window.height, 68.0);
}

TEST(Merge20FormSpotlight, WindowRectEmptyWithoutShapes)
{
	const Merge20FormSpotlightWindow window =
		Merge20FormSpotlightWindowRect(nullptr, nullptr);
	EXPECT_DOUBLE_EQ(window.width, 0.0);
	EXPECT_DOUBLE_EQ(window.height, 0.0);
}

TEST(Merge20FormSpotlight, ScrollToCenterPutsShapeCentreInViewportCentre)
{
	// Centre (1000, 800) at 100%, viewport 800x600:
	// x = (1000 + 32) - 400 = 632, y = (800 + 32) - 300 = 532.
	const std::pair<int, int> scroll =
		Merge20FormSpotlightScrollToCenter(1000.0, 800.0, 1.0, 800, 600);
	EXPECT_EQ(scroll.first, 632);
	EXPECT_EQ(scroll.second, 532);
	// At 50% the same centre needs (1032 * 0.5) - 400 = 116 on both axes.
	const std::pair<int, int> half =
		Merge20FormSpotlightScrollToCenter(1000.0, 800.0, 0.5, 800, 600);
	EXPECT_EQ(half.first, 116);
	EXPECT_EQ(half.second, 116);
	// A centre near the origin never scrolls below zero.
	const std::pair<int, int> origin =
		Merge20FormSpotlightScrollToCenter(150.0, 125.0, 1.0, 800, 600);
	EXPECT_EQ(origin.first, 0);
	EXPECT_EQ(origin.second, 0);
}

TEST(Merge20FormSpotlight, ArrowHeadPointsAlongShaft)
{
	// A horizontal shaft to the right: the head is 12 units long,
	// its base corners 30 degrees off the shaft.
	const Merge20FormSpotlightArrowHead head =
		Merge20FormSpotlightMakeArrowHead(0.0, 0.0, 100.0, 0.0);
	EXPECT_DOUBLE_EQ(head.tipX, 100.0);
	EXPECT_DOUBLE_EQ(head.tipY, 0.0);
	// The head is 12 units long, its base corners 30 degrees off the shaft.
	const double cos30 = std::cos(3.14159265358979323846 / 6.0);
	const double sin30 = std::sin(3.14159265358979323846 / 6.0);
	EXPECT_NEAR(head.base1X, 100.0 - 12.0 * cos30, 1e-9);
	EXPECT_NEAR(head.base1Y, 0.0 + 12.0 * sin30, 1e-9);
	EXPECT_NEAR(head.base2X, 100.0 - 12.0 * cos30, 1e-9);
	EXPECT_NEAR(head.base2Y, 0.0 - 12.0 * sin30, 1e-9);
}

TEST(Merge20FormSpotlight, ArrowHeadDegenerateWhenPointsCoincide)
{
	const Merge20FormSpotlightArrowHead head =
		Merge20FormSpotlightMakeArrowHead(50.0, 50.0, 50.0, 50.0);
	EXPECT_DOUBLE_EQ(head.tipX, 50.0);
	EXPECT_DOUBLE_EQ(head.tipY, 50.0);
	EXPECT_DOUBLE_EQ(head.base1X, 50.0);
	EXPECT_DOUBLE_EQ(head.base1Y, 50.0);
	EXPECT_DOUBLE_EQ(head.base2X, 50.0);
	EXPECT_DOUBLE_EQ(head.base2Y, 50.0);
}

TEST(Merge20FormSpotlight, CalloutLinesListAttrOldNewAndDesc)
{
	std::vector<Merge20FormDiffRecord> records;
	Merge20FormDiffRecord x = MakeRecord(L"field[name='a']", true);
	x.attr = L"x";
	x.oldVal = L"100";
	x.newVal = L"112";
	x.desc = L"desc-x";
	records.push_back(x);
	Merge20FormDiffRecord width = MakeRecord(L"field[name='a']", true);
	width.attr = L"width";
	width.oldVal = L"200";
	width.newVal = L"210";
	width.desc = L"desc-w";
	records.push_back(width);
	const std::vector<std::wstring> lines = Merge20FormSpotlightCalloutLines(records);
	ASSERT_EQ(lines.size(), 2u);
	EXPECT_EQ(lines[0], L"\u30fbx: 100 \u2192 112 (desc-x)");
	EXPECT_EQ(lines[1], L"\u30fbwidth: 200 \u2192 210 (desc-w)");
}
