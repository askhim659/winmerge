/**
 * @file Merge20FormImpact_test.cpp
 *
 * @brief Merge20 (Wave 5 F5) unit tests for the FORM/XML impact
 * analysis: overlap, text overflow, font ripple and page
 * overflow (design section 6 (5), acceptance tests 1-4 of
 * section 10 F5).
 *
 * This file stays ASCII: the Japanese test text is written with \u
 * escapes, like Merge20FormXml_test.cpp.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20FormDiff.h"
#include "Merge20/Merge20FormImpact.h"

namespace
{

	Merge20FormDiffOutcome DiffPair(const wchar_t* oldXml, const wchar_t* newXml)
	{
		const std::wstring oldText(oldXml);
		const std::wstring newText(newXml);
		return Merge20DiffFormFiles(&oldText, &newText, L"test.xml");
	}

	bool Contains(const std::wstring& text, const wchar_t* fragment)
	{
		return text.find(fragment) != std::wstring::npos;
	}

	bool AnyImpactContains(const Merge20FormFileResult& result, const wchar_t* fragment)
	{
		for (const Merge20FormDiffRecord& record : result.diffRecords)
		{
			if (Contains(record.impact, fragment))
				return true;
		}
		return false;
	}

	int CountKind(const Merge20FormImpactOutcome& outcome, Merge20FormImpactKind kind)
	{
		int count = 0;
		for (const Merge20FormImpactWarning& warning : outcome.warnings)
		{
			if (warning.kind == kind)
				++count;
		}
		return count;
	}

	// Two fields side by side (design section 6 (5) test 1).
	const wchar_t* const kOverlapOldXml =
		L"<form>"
		L"<field name=\"a\" x=\"10\" y=\"10\" width=\"100\" height=\"20\" />"
		L"<field name=\"b\" x=\"200\" y=\"10\" width=\"100\" height=\"20\" />"
		L"</form>";

	// `a` moves right onto `b` (150-250 overlaps 200-300).
	const wchar_t* const kOverlapNewXml =
		L"<form>"
		L"<field name=\"a\" x=\"150\" y=\"10\" width=\"100\" height=\"20\" />"
		L"<field name=\"b\" x=\"200\" y=\"10\" width=\"100\" height=\"20\" />"
		L"</form>";

	// One field with five full-width characters (design section
	// 6 (5) test 2: estimated width 5 * 10 * 1.0 = 50 units).
	const wchar_t* const kOverflowOldXml =
		L"<form>"
		L"<field name=\"a\" x=\"10\" y=\"10\" width=\"100\" height=\"20\" point=\"10\">\u3042\u3044\u3046\u3048\u304a</field>"
		L"</form>";

	// The width shrinks to 40 units: the text no longer fits.
	const wchar_t* const kOverflowNewXml =
		L"<form>"
		L"<field name=\"a\" x=\"10\" y=\"10\" width=\"40\" height=\"20\" point=\"10\">\u3042\u3044\u3046\u3048\u304a</field>"
		L"</form>";

	// Font growth (design section 6 (5) test 3): `a` grows from
	// 10pt to 20pt (estimated width 50 -> 100 units) while `b`
	// sits 10 units to its right.
	const wchar_t* const kFontOldXml =
		L"<form>"
		L"<field name=\"a\" x=\"10\" y=\"10\" width=\"100\" height=\"20\" point=\"10\">\u3042\u3044\u3046\u3048\u304a</field>"
		L"<field name=\"b\" x=\"120\" y=\"10\" width=\"100\" height=\"20\" point=\"10\">b</field>"
		L"</form>";

	const wchar_t* const kFontNewXml =
		L"<form>"
		L"<field name=\"a\" x=\"10\" y=\"10\" width=\"100\" height=\"20\" point=\"20\">\u3042\u3044\u3046\u3048\u304a</field>"
		L"<field name=\"b\" x=\"120\" y=\"10\" width=\"100\" height=\"20\" point=\"10\">b</field>"
		L"</form>";

	// A clean move (design section 6 (5) test 4): no overlap,
	// no overflow.
	const wchar_t* const kCleanOldXml =
		L"<form>"
		L"<field name=\"a\" x=\"10\" y=\"10\" width=\"100\" height=\"20\" />"
		L"<field name=\"b\" x=\"200\" y=\"10\" width=\"100\" height=\"20\" />"
		L"</form>";

	const wchar_t* const kCleanNewXml =
		L"<form>"
		L"<field name=\"a\" x=\"10\" y=\"30\" width=\"100\" height=\"20\" />"
		L"<field name=\"b\" x=\"200\" y=\"10\" width=\"100\" height=\"20\" />"
		L"</form>";

	Merge20FormShape MakeShape(const wchar_t* key, const wchar_t* name,
		double x, double y, double x2, double y2, double point = 10.0)
	{
		Merge20FormShape shape;
		shape.key = key;
		shape.tag = L"field";
		shape.name = name;
		shape.x = x;
		shape.y = y;
		shape.x2 = x2;
		shape.y2 = y2;
		shape.point = point;
		return shape;
	}

	TEST(Merge20FormImpact, EstimateTextWidth)
	{
		EXPECT_DOUBLE_EQ(Merge20FormImpactEstimateTextWidth(L"ab", 10.0), 12.0);
		EXPECT_DOUBLE_EQ(Merge20FormImpactEstimateTextWidth(L"\u3042", 10.0), 10.0);
		EXPECT_DOUBLE_EQ(Merge20FormImpactEstimateTextWidth(L"\u3042\u3044\u3046\u3048\u304a", 10.0), 50.0);
		EXPECT_DOUBLE_EQ(Merge20FormImpactEstimateTextWidth(L"", 10.0), 0.0);
		EXPECT_TRUE(Merge20FormImpactIsWideChar(L'\u3042'));
		EXPECT_FALSE(Merge20FormImpactIsWideChar(L'a'));
	}

	TEST(Merge20FormImpact, OverlapArea)
	{
		const Merge20FormShape a = MakeShape(L"a", L"a", 150.0, 10.0, 250.0, 30.0);
		const Merge20FormShape b = MakeShape(L"b", L"b", 200.0, 10.0, 300.0, 30.0);
		EXPECT_DOUBLE_EQ(Merge20FormImpactOverlapArea(a, b), 1000.0);
		const Merge20FormShape c = MakeShape(L"c", L"c", 300.0, 10.0, 400.0, 30.0);
		EXPECT_DOUBLE_EQ(Merge20FormImpactOverlapArea(a, c), 0.0);
		// Touching edges is not an overlap.
		const Merge20FormShape d = MakeShape(L"d", L"d", 250.0, 10.0, 350.0, 30.0);
		EXPECT_DOUBLE_EQ(Merge20FormImpactOverlapArea(a, d), 0.0);
	}

	// Design section 6 (5) test 1: overlap.
	TEST(Merge20FormImpact, OverlapDetected)
	{
		Merge20FormDiffOutcome outcome = DiffPair(kOverlapOldXml, kOverlapNewXml);
		Merge20FormImpactOutcome impact = Merge20ApplyFormImpact(outcome.result, 0.0, 0.0);
		EXPECT_EQ(CountKind(impact, Merge20FormImpactKind::Overlap), 1);
		EXPECT_TRUE(impact.hasLayoutBreaking);
		ASSERT_EQ(impact.warnings.size(), 1u);
		// The warning names both items.
		EXPECT_TRUE(Contains(impact.warnings[0].text, L"a"));
		EXPECT_TRUE(Contains(impact.warnings[0].text, L"b"));
		EXPECT_TRUE(Contains(impact.warnings[0].text, L"\u91cd\u306a\u3063\u3066\u3044\u307e\u3059"));
		EXPECT_NEAR(impact.warnings[0].overlapRatio, 0.5, 0.001);
		// Attached to the moved record, and the severity rises.
		EXPECT_TRUE(AnyImpactContains(outcome.result, L"\u91cd\u306a\u3063\u3066\u3044\u307e\u3059"));
		EXPECT_EQ(outcome.result.severity, Merge20FormSeverity::LayoutBreaking);
	}

	// Design section 6 (5) test 2: text overflow.
	TEST(Merge20FormImpact, TextOverflowDetected)
	{
		Merge20FormDiffOutcome outcome = DiffPair(kOverflowOldXml, kOverflowNewXml);
		Merge20FormImpactOutcome impact = Merge20ApplyFormImpact(outcome.result, 0.0, 0.0);
		EXPECT_EQ(CountKind(impact, Merge20FormImpactKind::TextOverflow), 1);
		EXPECT_TRUE(impact.hasLayoutBreaking);
		ASSERT_EQ(impact.warnings.size(), 1u);
		// "Possibility" wording, and the estimate is marked approximate.
		EXPECT_TRUE(Contains(impact.warnings[0].text, L"\u53ef\u80fd\u6027\u3042\u308a"));
		EXPECT_TRUE(Contains(impact.warnings[0].text, L"\u8fd1\u4f3c"));
		EXPECT_TRUE(AnyImpactContains(outcome.result, L"\u53ef\u80fd\u6027\u3042\u308a"));
		EXPECT_EQ(outcome.result.severity, Merge20FormSeverity::LayoutBreaking);
	}

	// Design section 6 (5) test 3: font-size ripple.
	TEST(Merge20FormImpact, FontRippleDetected)
	{
		Merge20FormDiffOutcome outcome = DiffPair(kFontOldXml, kFontNewXml);
		Merge20FormImpactOutcome impact = Merge20ApplyFormImpact(outcome.result, 0.0, 0.0);
		EXPECT_EQ(CountKind(impact, Merge20FormImpactKind::FontRipple), 1);
		ASSERT_EQ(impact.warnings.size(), 1u);
		EXPECT_TRUE(Contains(impact.warnings[0].text, L"\u53ef\u80fd\u6027\u3042\u308a"));
		EXPECT_TRUE(Contains(impact.warnings[0].text, L"a"));
		EXPECT_TRUE(Contains(impact.warnings[0].text, L"b"));
		EXPECT_TRUE(AnyImpactContains(outcome.result, L"\u6ce2\u53ca\u3059\u308b\u53ef\u80fd\u6027\u3042\u308a"));
	}

	// Design section 6 (5) test 4: no false positives on a clean change.
	TEST(Merge20FormImpact, NoFalsePositivesOnCleanChange)
	{
		Merge20FormDiffOutcome outcome = DiffPair(kCleanOldXml, kCleanNewXml);
		Merge20FormImpactOutcome impact = Merge20ApplyFormImpact(outcome.result, 0.0, 0.0);
		EXPECT_TRUE(impact.warnings.empty());
		EXPECT_FALSE(impact.hasLayoutBreaking);
		EXPECT_EQ(outcome.result.severity, Merge20FormSeverity::VisualOnly);
	}

	// Design section 6 (5) test 4: two rules overlapping is normal.
	TEST(Merge20FormImpact, LineLineOverlapIgnored)
	{
		const wchar_t* oldXml =
			L"<form>"
			L"<line name=\"l1\" x=\"10\" y=\"10\" width=\"100\" height=\"2\" />"
			L"<line name=\"l2\" x=\"200\" y=\"10\" width=\"100\" height=\"2\" />"
			L"</form>";
		const wchar_t* newXml =
			L"<form>"
			L"<line name=\"l1\" x=\"150\" y=\"10\" width=\"100\" height=\"2\" />"
			L"<line name=\"l2\" x=\"200\" y=\"10\" width=\"100\" height=\"2\" />"
			L"</form>";
		Merge20FormDiffOutcome outcome = DiffPair(oldXml, newXml);
		Merge20FormImpactOutcome impact = Merge20ApplyFormImpact(outcome.result, 0.0, 0.0);
		EXPECT_TRUE(impact.warnings.empty());
	}

	// Below 10% is a "minor overlap" note, not a warning.
	TEST(Merge20FormImpact, MinorOverlapIsNoteOnly)
	{
		const wchar_t* newXml =
			L"<form>"
			L"<field name=\"a\" x=\"295\" y=\"10\" width=\"100\" height=\"20\" />"
			L"<field name=\"b\" x=\"200\" y=\"10\" width=\"100\" height=\"20\" />"
			L"</form>";
		Merge20FormDiffOutcome outcome = DiffPair(kOverlapOldXml, newXml);
		Merge20FormImpactOutcome impact = Merge20ApplyFormImpact(outcome.result, 0.0, 0.0);
		EXPECT_EQ(CountKind(impact, Merge20FormImpactKind::MinorOverlap), 1);
		EXPECT_EQ(CountKind(impact, Merge20FormImpactKind::Overlap), 0);
		EXPECT_FALSE(impact.hasLayoutBreaking);
		ASSERT_EQ(impact.warnings.size(), 1u);
		EXPECT_TRUE(Contains(impact.warnings[0].text, L"\u8efd\u5fae\u306a\u91cd\u306a\u308a"));
		EXPECT_NEAR(impact.warnings[0].overlapRatio, 0.05, 0.001);
		EXPECT_EQ(outcome.result.severity, Merge20FormSeverity::VisualOnly);
	}

	// A rule overlapping an item still warns (only rule-rule is exempt).
	TEST(Merge20FormImpact, LineItemOverlapWarns)
	{
		const wchar_t* oldXml =
			L"<form>"
			L"<line name=\"l1\" x=\"10\" y=\"10\" width=\"100\" height=\"2\" />"
			L"<field name=\"b\" x=\"200\" y=\"10\" width=\"100\" height=\"20\" />"
			L"</form>";
		const wchar_t* newXml =
			L"<form>"
			L"<line name=\"l1\" x=\"150\" y=\"10\" width=\"100\" height=\"2\" />"
			L"<field name=\"b\" x=\"200\" y=\"10\" width=\"100\" height=\"20\" />"
			L"</form>";
		Merge20FormDiffOutcome outcome = DiffPair(oldXml, newXml);
		Merge20FormImpactOutcome impact = Merge20ApplyFormImpact(outcome.result, 0.0, 0.0);
		EXPECT_EQ(CountKind(impact, Merge20FormImpactKind::Overlap), 1);
	}

	// Page overflow against a known paper size, and against the
	// canvas size when the paper size is unknown.
	TEST(Merge20FormImpact, PageOverflow)
	{
		const Merge20FormShape oldShape = MakeShape(L"a", L"a", 10.0, 10.0, 110.0, 30.0);
		Merge20FormShape newShape = MakeShape(L"a", L"a", 10.0, 10.0, 210.0, 30.0);
		const std::vector<Merge20FormShape> oldShapes{ oldShape };
		const std::vector<Merge20FormShape> newShapes{ newShape };
		// Paper 150 wide: the shape exceeds it.
		Merge20FormImpactOutcome withPaper = Merge20AnalyzeFormImpact(
			oldShapes, newShapes, 1000.0, 800.0, 150.0, 100.0);
		EXPECT_EQ(CountKind(withPaper, Merge20FormImpactKind::PageOverflow), 1);
		EXPECT_TRUE(withPaper.hasLayoutBreaking);
		ASSERT_EQ(withPaper.warnings.size(), 1u);
		EXPECT_TRUE(Contains(withPaper.warnings[0].text, L"\u7528\u7d19\u306e\u7bc4\u56f2\u3092\u8d85\u3048\u3066\u3044\u307e\u3059"));
		// Paper unknown, canvas 1000 wide: inside the canvas.
		Merge20FormImpactOutcome noPaper = Merge20AnalyzeFormImpact(
			oldShapes, newShapes, 1000.0, 800.0, 0.0, 0.0);
		EXPECT_TRUE(noPaper.warnings.empty());
		// Paper unknown, canvas 150 wide: exceeds the canvas.
		Merge20FormImpactOutcome smallCanvas = Merge20AnalyzeFormImpact(
			oldShapes, newShapes, 150.0, 100.0, 0.0, 0.0);
		EXPECT_EQ(CountKind(smallCanvas, Merge20FormImpactKind::PageOverflow), 1);
		ASSERT_EQ(smallCanvas.warnings.size(), 1u);
		EXPECT_TRUE(Contains(smallCanvas.warnings[0].text, L"\u30ad\u30e3\u30f3\u30d0\u30b9\u306e\u7bc4\u56f2\u3092\u8d85\u3048\u3066\u3044\u307e\u3059"));
	}

	// A shape that already exceeded the limit on the old side
	// does not warn again.
	TEST(Merge20FormImpact, PreExistingPageOverflowIgnored)
	{
		const Merge20FormShape oldShape = MakeShape(L"a", L"a", 10.0, 10.0, 210.0, 30.0);
		const Merge20FormShape newShape = MakeShape(L"a", L"a", 10.0, 10.0, 210.0, 30.0);
		const std::vector<Merge20FormShape> oldShapes{ oldShape };
		const std::vector<Merge20FormShape> newShapes{ newShape };
		Merge20FormImpactOutcome outcome = Merge20AnalyzeFormImpact(
			oldShapes, newShapes, 1000.0, 800.0, 150.0, 100.0);
		EXPECT_TRUE(outcome.warnings.empty());
	}

	// Estimated rectangles are excluded from every check.
	TEST(Merge20FormImpact, EstimatedShapesExcluded)
	{
		Merge20FormShape oldShape = MakeShape(L"a", L"a", 10.0, 10.0, 110.0, 30.0);
		Merge20FormShape newShape = MakeShape(L"a", L"a", 150.0, 10.0, 250.0, 30.0);
		newShape.isEstimated = true;
		const Merge20FormShape b = MakeShape(L"b", L"b", 200.0, 10.0, 300.0, 30.0);
		const std::vector<Merge20FormShape> oldShapes{ oldShape, b };
		const std::vector<Merge20FormShape> newShapes{ newShape, b };
		Merge20FormImpactOutcome outcome = Merge20AnalyzeFormImpact(
			oldShapes, newShapes, 1000.0, 800.0, 0.0, 0.0);
		EXPECT_TRUE(outcome.warnings.empty());
	}

	// No old side (a file added): nothing to compare against.
	TEST(Merge20FormImpact, NoOldSideNoWarnings)
	{
		const Merge20FormShape b = MakeShape(L"b", L"b", 200.0, 10.0, 300.0, 30.0);
		const std::vector<Merge20FormShape> oldShapes;
		const std::vector<Merge20FormShape> newShapes{ b };
		Merge20FormImpactOutcome outcome = Merge20AnalyzeFormImpact(
			oldShapes, newShapes, 1000.0, 800.0, 0.0, 0.0);
		EXPECT_TRUE(outcome.warnings.empty());
	}

} // namespace
