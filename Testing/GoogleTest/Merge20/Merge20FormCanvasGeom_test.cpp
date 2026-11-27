// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20FormCanvasGeom_test.cpp
 *
 * @brief Merge20 (Wave 5 F3) unit tests for the FORM compare
 * canvas geometry: the four zoom modes, the XML <-> screen
 * transform, the shape hit test and the text truncation (design
 * merge20-form-compare-design.md sections 3.4, 6 (6) and
 * 10 F3).
 *
 * These tests pin the acceptance conditions that do not need a
 * window: (a) the native 1:1 drawing (one XML unit is one pixel
 * at 100%, TransformOneToOne), (b) the four zoom modes
 * (Scale*), (c) the text truncation with an ellipsis
 * (Truncate*). The actual GDI drawing (colours, grid, text
 * rendering, drag scroll) is verified on the real machine, not
 * here.
 *
 * This file stays ASCII: the Japanese test text is written with \u
 * escapes, like Merge20FormList_test.cpp.
 */
#include "pch.h"
#include <cmath>
#include <gtest/gtest.h>
#include "Merge20/Merge20FormCanvasGeom.h"

namespace
{

	Merge20FormShape MakeShape(const wchar_t* key, double x, double y, double x2, double y2)
	{
		Merge20FormShape shape;
		shape.key = key;
		shape.x = x;
		shape.y = y;
		shape.x2 = x2;
		shape.y2 = y2;
		return shape;
	}

	// One changed file: a big rect, a small rect inside it, and a
	// line; only the big rect and the line are changed.
	std::vector<Merge20FormShape> SampleNewShapes()
	{
		std::vector<Merge20FormShape> shapes;
		shapes.push_back(MakeShape(L"field[name='big']", 100.0, 100.0, 300.0, 200.0));
		shapes.push_back(MakeShape(L"field[name='small']", 150.0, 120.0, 200.0, 150.0));
		Merge20FormShape line = MakeShape(L"line[name='rule']", 100.0, 250.0, 300.0, 250.0);
		line.isLine = true;
		shapes.push_back(line);
		return shapes;
	}

	std::vector<std::wstring> SampleChangedKeys()
	{
		return { L"field[name='big']", L"line[name='rule']" };
	}

} // namespace

TEST(Merge20FormCanvasGeom, ScalePercent100IsOne)
{
	EXPECT_DOUBLE_EQ(Merge20FormCanvasScaleForMode(Merge20FormZoomMode::Percent100, 100.0, 1000.0, 800.0), 1.0);
}

TEST(Merge20FormCanvasGeom, ScalePercent25IsQuarter)
{
	EXPECT_DOUBLE_EQ(Merge20FormCanvasScaleForMode(Merge20FormZoomMode::Percent25, 100.0, 1000.0, 800.0), 0.25);
}

TEST(Merge20FormCanvasGeom, ScaleSliderClampsToRange)
{
	EXPECT_DOUBLE_EQ(Merge20FormCanvasScaleForMode(Merge20FormZoomMode::Slider, 5.0, 1000.0, 800.0), 0.10);
	EXPECT_DOUBLE_EQ(Merge20FormCanvasScaleForMode(Merge20FormZoomMode::Slider, 250.0, 1000.0, 800.0), 2.00);
	EXPECT_DOUBLE_EQ(Merge20FormCanvasScaleForMode(Merge20FormZoomMode::Slider, 100.0, 1000.0, 800.0), 1.00);
	EXPECT_DOUBLE_EQ(Merge20FormCanvasScaleForMode(Merge20FormZoomMode::Slider, 137.0, 1000.0, 800.0), 1.37);
	EXPECT_DOUBLE_EQ(Merge20FormCanvasClampSliderPercent(5.0), 10.0);
	EXPECT_DOUBLE_EQ(Merge20FormCanvasClampSliderPercent(250.0), 200.0);
	EXPECT_DOUBLE_EQ(Merge20FormCanvasClampSliderPercent(100.0), 100.0);
}

TEST(Merge20FormCanvasGeom, ScaleFitShrinksToViewportWidth)
{
	// Viewport 540: availW = max(300, 540 - 40) = 500; 500 / 1000 = 0.5.
	EXPECT_DOUBLE_EQ(Merge20FormCanvasScaleForMode(Merge20FormZoomMode::Fit, 100.0, 1000.0, 540.0), 0.5);
}

TEST(Merge20FormCanvasGeom, ScaleFitCapsAtOne)
{
	// Native 400 fits into viewport 1000: availW 960 / 400 = 2.4, capped at 1.0.
	EXPECT_DOUBLE_EQ(Merge20FormCanvasScaleForMode(Merge20FormZoomMode::Fit, 100.0, 400.0, 1000.0), 1.0);
}

TEST(Merge20FormCanvasGeom, ScaleFitUsesMinimumWidth)
{
	// Viewport 200: availW = max(300, 200 - 40) = 300; 300 / 1000 = 0.3.
	EXPECT_DOUBLE_EQ(Merge20FormCanvasScaleForMode(Merge20FormZoomMode::Fit, 100.0, 1000.0, 200.0), 0.3);
}

TEST(Merge20FormCanvasGeom, ScaleFitWithoutNativeWidthIsOne)
{
	EXPECT_DOUBLE_EQ(Merge20FormCanvasScaleForMode(Merge20FormZoomMode::Fit, 100.0, 0.0, 800.0), 1.0);
}

TEST(Merge20FormCanvasGeom, TransformOneToOne)
{
	// Acceptance (a): at 100% with no scroll, one XML unit is one
	// pixel, shifted by the 32-unit origin margin only.
	EXPECT_DOUBLE_EQ(Merge20FormCanvasToScreen(100.0, 1.0, 0.0), 132.0);
	EXPECT_DOUBLE_EQ(Merge20FormCanvasToScreen(0.0, 1.0, 0.0), 32.0);
	EXPECT_DOUBLE_EQ(Merge20FormCanvasToXml(132.0, 1.0, 0.0), 100.0);
	EXPECT_DOUBLE_EQ(Merge20FormCanvasToXml(32.0, 1.0, 0.0), 0.0);
}

TEST(Merge20FormCanvasGeom, TransformWithZoomAndScrollRoundTrips)
{
	const double scale = 0.25;
	const double scroll = 50.0;
	for (double xml : { 0.0, 100.0, 432.5, 1000.0 })
	{
		const double screen = Merge20FormCanvasToScreen(xml, scale, scroll);
		EXPECT_DOUBLE_EQ(Merge20FormCanvasToXml(screen, scale, scroll), xml);
	}
	EXPECT_DOUBLE_EQ(Merge20FormCanvasToScreen(100.0, 0.25, 50.0), (100.0 + 32.0) * 0.25 - 50.0);
}

TEST(Merge20FormCanvasGeom, ContentSizeIncludesOriginOffset)
{
	const std::pair<double, double> size100 = Merge20FormCanvasContentSize(1000.0, 800.0, 1.0);
	EXPECT_DOUBLE_EQ(size100.first, 1032.0);
	EXPECT_DOUBLE_EQ(size100.second, 832.0);
	const std::pair<double, double> size25 = Merge20FormCanvasContentSize(1000.0, 800.0, 0.25);
	EXPECT_DOUBLE_EQ(size25.first, 258.0);
	EXPECT_DOUBLE_EQ(size25.second, 208.0);
}

TEST(Merge20FormCanvasGeom, GridLineMajor)
{
	EXPECT_TRUE(Merge20FormCanvasGridLineIsMajor(32.0));
	EXPECT_TRUE(Merge20FormCanvasGridLineIsMajor(532.0));
	EXPECT_TRUE(Merge20FormCanvasGridLineIsMajor(1032.0));
	EXPECT_FALSE(Merge20FormCanvasGridLineIsMajor(132.0));
	EXPECT_FALSE(Merge20FormCanvasGridLineIsMajor(232.0));
}

TEST(Merge20FormCanvasGeom, HitTestSmallestAreaWins)
{
	// Click inside both the big rect and the small rect: the
	// small one wins (design section 3.4). The small rect is
	// unchanged here, so the big rect is the only candidate.
	const std::vector<Merge20FormShape> newShapes = SampleNewShapes();
	const std::vector<Merge20FormShape> oldShapes;
	EXPECT_EQ(Merge20FormCanvasHitTestKey(newShapes, oldShapes, SampleChangedKeys(), 160.0, 130.0),
		L"field[name='big']");
	// With the small rect changed too, the click picks the small one.
	const std::vector<std::wstring> bothChanged =
		{ L"field[name='big']", L"field[name='small']", L"line[name='rule']" };
	EXPECT_EQ(Merge20FormCanvasHitTestKey(newShapes, oldShapes, bothChanged, 160.0, 130.0),
		L"field[name='small']");
}

TEST(Merge20FormCanvasGeom, HitTestIgnoresUnchangedShapes)
{
	// A shape that is not in the changed set is never a hit,
	// even when the click is right inside it.
	std::vector<Merge20FormShape> newShapes;
	newShapes.push_back(MakeShape(L"field[name='kept']", 500.0, 500.0, 600.0, 550.0));
	const std::vector<Merge20FormShape> oldShapes;
	EXPECT_TRUE(Merge20FormCanvasHitTestKey(newShapes, oldShapes, SampleChangedKeys(), 550.0, 525.0).empty());
	// And far away from every changed shape: no hit either.
	const std::vector<Merge20FormShape> sample = SampleNewShapes();
	EXPECT_TRUE(Merge20FormCanvasHitTestKey(sample, oldShapes, SampleChangedKeys(), 900.0, 700.0).empty());
}

TEST(Merge20FormCanvasGeom, HitTestToleranceExpandsRect)
{
	const std::vector<Merge20FormShape> newShapes = SampleNewShapes();
	const std::vector<Merge20FormShape> oldShapes;
	// 8 units left of the big rect (x = 100): still a hit.
	EXPECT_EQ(Merge20FormCanvasHitTestKey(newShapes, oldShapes, SampleChangedKeys(), 92.0, 150.0),
		L"field[name='big']");
	// 9 units left: a miss.
	EXPECT_TRUE(Merge20FormCanvasHitTestKey(newShapes, oldShapes, SampleChangedKeys(), 91.0, 150.0).empty());
}

TEST(Merge20FormCanvasGeom, HitTestLineUsesTolerance)
{
	const std::vector<Merge20FormShape> newShapes = SampleNewShapes();
	const std::vector<Merge20FormShape> oldShapes;
	// The line is y = 250, x 100-300: a click 5 units above it hits.
	EXPECT_EQ(Merge20FormCanvasHitTestKey(newShapes, oldShapes, SampleChangedKeys(), 200.0, 245.0),
		L"line[name='rule']");
}

TEST(Merge20FormCanvasGeom, HitTestNewWinsTie)
{
	// Same rect on both sides with the same area: the new shape
	// (first in order) wins.
	const std::vector<Merge20FormShape> newShapes = SampleNewShapes();
	std::vector<Merge20FormShape> oldShapes;
	oldShapes.push_back(MakeShape(L"field[name='big']", 100.0, 100.0, 300.0, 200.0));
	const std::wstring hit = Merge20FormCanvasHitTestKey(newShapes, oldShapes, SampleChangedKeys(), 120.0, 110.0);
	EXPECT_EQ(hit, L"field[name='big']");
}

TEST(Merge20FormCanvasGeom, TruncateShortTextUnchanged)
{
	EXPECT_EQ(Merge20FormCanvasTruncateText(L"abc", 5), L"abc");
	EXPECT_EQ(Merge20FormCanvasTruncateText(L"abc", 3), L"abc");
	EXPECT_EQ(Merge20FormCanvasTruncateText(L"", 5), L"");
}

TEST(Merge20FormCanvasGeom, TruncateLongTextAddsEllipsis)
{
	// 10 chars, max 5: 4 chars + ellipsis = 5 chars total.
	EXPECT_EQ(Merge20FormCanvasTruncateText(L"0123456789", 5), L"0123\u2026");
	EXPECT_EQ(Merge20FormCanvasTruncateText(L"\u8cc7\u7523\u8a3c\u660e\u66f8\u306e\u984d", 5), L"\u8cc7\u7523\u8a3c\u660e\u2026");
}

TEST(Merge20FormCanvasGeom, TruncateZeroOrOneMaxChars)
{
	EXPECT_EQ(Merge20FormCanvasTruncateText(L"abcdef", 0), L"");
	EXPECT_EQ(Merge20FormCanvasTruncateText(L"abcdef", 1), L"\u2026");
}

TEST(Merge20FormCanvasGeom, PointToPixels)
{
	// 12pt at 96 dpi = 16 px; 10pt at 96 dpi = 13.33... px.
	EXPECT_DOUBLE_EQ(Merge20FormCanvasPointToPixels(12.0, 96), 16.0);
	EXPECT_NEAR(Merge20FormCanvasPointToPixels(10.0, 96), 13.333333333333334, 1e-9);
	// Missing / invalid input falls back to 10pt at 96 dpi.
	EXPECT_NEAR(Merge20FormCanvasPointToPixels(std::nan(""), 96), 13.333333333333334, 1e-9);
	EXPECT_NEAR(Merge20FormCanvasPointToPixels(0.0, 96), 13.333333333333334, 1e-9);
	EXPECT_NEAR(Merge20FormCanvasPointToPixels(12.0, 0), 16.0);
}
