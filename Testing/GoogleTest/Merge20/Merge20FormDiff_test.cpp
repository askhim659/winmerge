/**
 * @file Merge20FormDiff_test.cpp
 *
 * @brief Merge20 (Wave 5 F1) unit tests for the FORM/XML matching
 * and classification: stable keys, move / rename detection, noise
 * filtering, grouping, severity, the large-change flag and the
 * `getDetailInterpretation` port.
 *
 * The acceptance tests of design section 10 F1 (a) are covered
 * here: section 5.1 tests 1-4 (MoveDetected, RenamedDetected,
 * RowMoveGrouped, NoFalseMoveOrRename), section 5.2 tests 1-3
 * (NoiseIgnored, NoiseBoundary, DecimalDisplay) and section 5.3
 * tests 1-3 (RowMoveGrouped, TwoRowsMoveSeparately,
 * GroupSummaryText).
 *
 * This file stays ASCII: the Japanese test text is written with \u
 * escapes, like Merge20FormXml_test.cpp.
 */
#include "pch.h"
#include <cmath>
#include <gtest/gtest.h>
#include "Merge20/Merge20FormDiff.h"

namespace
{

	Merge20FormDiffOutcome DiffPair(const wchar_t* oldXml, const wchar_t* newXml)
	{
		const std::wstring oldText(oldXml);
		const std::wstring newText(newXml);
		return Merge20DiffFormFiles(&oldText, &newText, L"test.xml");
	}

	int CountType(const Merge20FormDiffOutcome& outcome, Merge20FormDiffType type)
	{
		int count = 0;
		for (const Merge20FormDiffRecord& record : outcome.result.diffRecords)
		{
			if (record.type == type)
				++count;
		}
		return count;
	}

	const Merge20FormDiffRecord* FindRecord(const Merge20FormDiffOutcome& outcome,
		Merge20FormDiffType type)
	{
		for (const Merge20FormDiffRecord& record : outcome.result.diffRecords)
		{
			if (record.type == type)
				return &record;
		}
		return nullptr;
	}

	bool Contains(const std::wstring& text, const wchar_t* fragment)
	{
		return text.find(fragment) != std::wstring::npos;
	}

	// One row of five fields (section 5.1 test 3 / section 5.3
	// test 1). The row itself has no coordinates, so moving the
	// row means moving the five fields.
	const wchar_t* const kRowOldXml =
		L"<form>"
		L"<row name=\"row1\">"
		L"<field name=\"a\" x=\"10\" y=\"60\" width=\"100\" height=\"20\" />"
		L"<field name=\"b\" x=\"120\" y=\"60\" width=\"100\" height=\"20\" />"
		L"<field name=\"c\" x=\"230\" y=\"60\" width=\"100\" height=\"20\" />"
		L"<field name=\"d\" x=\"340\" y=\"60\" width=\"100\" height=\"20\" />"
		L"<field name=\"e\" x=\"450\" y=\"60\" width=\"100\" height=\"20\" />"
		L"</row>"
		L"</form>";

	const wchar_t* const kRowNewXml =
		L"<form>"
		L"<row name=\"row1\">"
		L"<field name=\"a\" x=\"10\" y=\"72\" width=\"100\" height=\"20\" />"
		L"<field name=\"b\" x=\"120\" y=\"72\" width=\"100\" height=\"20\" />"
		L"<field name=\"c\" x=\"230\" y=\"72\" width=\"100\" height=\"20\" />"
		L"<field name=\"d\" x=\"340\" y=\"72\" width=\"100\" height=\"20\" />"
		L"<field name=\"e\" x=\"450\" y=\"72\" width=\"100\" height=\"20\" />"
		L"</row>"
		L"</form>";

} // namespace

TEST(Merge20FormDiff, StableKey)
{
	const Merge20FormParseResult parsed = Merge20ParseFormXml(
		L"<form><page name=\"p\"><field name=\"a\" x=\"1\" y=\"2\" /></page></form>");
	ASSERT_TRUE(parsed.ok);
	ASSERT_EQ(parsed.elements.size(), static_cast<size_t>(3));
	EXPECT_EQ(Merge20BuildStableKey(parsed.elements[2]),
		std::wstring(L"/form/page[1]/field[1]|field|a"));

	// A nameless element uses the attribute signature.
	const Merge20FormParseResult nameless = Merge20ParseFormXml(
		L"<form><box left=\"5\" top=\"120\" width=\"300\" height=\"40\" /></form>");
	ASSERT_TRUE(nameless.ok);
	EXPECT_EQ(Merge20BuildStableKey(nameless.elements[1]),
		std::wstring(L"/form/box[1]|box|height=40,left=5,top=120,width=300"));

	// The same name under two parents gives different keys.
	const Merge20FormParseResult twoParents = Merge20ParseFormXml(
		L"<form><g name=\"n\"><i name=\"x\" x=\"1\" y=\"1\" /></g>"
		L"<g name=\"m\"><i name=\"x\" x=\"2\" y=\"2\" /></g></form>");
	ASSERT_TRUE(twoParents.ok);
	EXPECT_NE(Merge20BuildStableKey(twoParents.elements[2]),
		Merge20BuildStableKey(twoParents.elements[4]));
}

TEST(Merge20FormDiff, MoveDetected)
{
	// Section 5.1 test 1: the field moves from page p1 to page p2.
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form><page name=\"p1\"><field name=\"f1\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></page>"
		L"<page name=\"p2\" /></form>",
		L"<form><page name=\"p1\" /><page name=\"p2\">"
		L"<field name=\"f1\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></page></form>");
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Moved), 1);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Add), 0);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Del), 0);
	EXPECT_EQ(outcome.result.changeCount, 1);
	const Merge20FormDiffRecord* moved = FindRecord(outcome, Merge20FormDiffType::Moved);
	ASSERT_NE(moved, nullptr);
	EXPECT_TRUE(Contains(moved->desc, L"\u79fb\u52d5\u3057\u307e\u3057\u305f"));
	EXPECT_TRUE(Contains(moved->desc, L"f1"));
	EXPECT_TRUE(moved->hasVisual);
	EXPECT_DOUBLE_EQ(moved->diffVal, 0.0); // Same coordinates.
}

TEST(Merge20FormDiff, MoveWithCoordChange)
{
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form><page name=\"p1\"><field name=\"f1\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></page>"
		L"<page name=\"p2\" /></form>",
		L"<form><page name=\"p1\" /><page name=\"p2\">"
		L"<field name=\"f1\" x=\"30\" y=\"45\" width=\"50\" height=\"10\" /></page></form>");
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Moved), 1);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Coord), 0);
	const Merge20FormDiffRecord* moved = FindRecord(outcome, Merge20FormDiffType::Moved);
	ASSERT_NE(moved, nullptr);
	EXPECT_DOUBLE_EQ(moved->diffVal, 45.0); // |dx| + |dy| = 20 + 25.
	EXPECT_TRUE(Contains(moved->desc, L"x: 10 \u2192 30"));
	EXPECT_TRUE(Contains(moved->desc, L"y: 20 \u2192 45"));
}

TEST(Merge20FormDiff, RenamedDetected)
{
	// Section 5.1 test 2: only the `name` attribute changes.
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form><field name=\"old\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></form>",
		L"<form><field name=\"new\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></form>");
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Renamed), 1);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Add), 0);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Del), 0);
	EXPECT_EQ(outcome.result.changeCount, 1);
	const Merge20FormDiffRecord* renamed = FindRecord(outcome, Merge20FormDiffType::Renamed);
	ASSERT_NE(renamed, nullptr);
	EXPECT_TRUE(Contains(renamed->desc, L"old"));
	EXPECT_TRUE(Contains(renamed->desc, L"new"));
	EXPECT_TRUE(renamed->hasVisual);
	EXPECT_DOUBLE_EQ(renamed->diffVal, 0.0); // Same coordinates.
}

TEST(Merge20FormDiff, RenamedWithCoordChange)
{
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form><field name=\"old\" x=\"10\" y=\"20\" width=\"50\" height=\"10\" /></form>",
		L"<form><field name=\"new\" x=\"30\" y=\"20\" width=\"50\" height=\"10\" /></form>");
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Renamed), 1);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Coord), 0);
	const Merge20FormDiffRecord* renamed = FindRecord(outcome, Merge20FormDiffType::Renamed);
	ASSERT_NE(renamed, nullptr);
	EXPECT_DOUBLE_EQ(renamed->diffVal, 20.0);
	EXPECT_TRUE(Contains(renamed->desc, L"x: 10 \u2192 30"));
}

TEST(Merge20FormDiff, RowMoveGrouped)
{
	// Section 5.1 test 3 and section 5.3 test 1: the whole row
	// moves down 12 units; the five fields become one group.
	const Merge20FormDiffOutcome outcome = DiffPair(kRowOldXml, kRowNewXml);
	EXPECT_EQ(outcome.result.changeCount, 5);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Coord), 5);
	ASSERT_EQ(outcome.groups.size(), static_cast<size_t>(1));
	const Merge20FormDiffGroup& group = outcome.groups[0];
	EXPECT_EQ(group.id, std::wstring(L"G1"));
	EXPECT_FALSE(group.minor);
	ASSERT_EQ(group.recordIndices.size(), static_cast<size_t>(5));
	EXPECT_EQ(outcome.result.diffRecords[group.recordIndices[0]].groupId,
		std::wstring(L"G1"));
	// Section 5.3 test 3: the summary holds the amount, the
	// direction and the item count.
	EXPECT_EQ(group.summary, std::wstring(L"field 5\u9805\u76ee\u304c\u4e0b\u306b12\u5358\u4f4d\u79fb\u52d5"));
	EXPECT_EQ(outcome.result.summary, group.summary);
	EXPECT_DOUBLE_EQ(outcome.result.totalNumericSum, 60.0);
}

TEST(Merge20FormDiff, TwoRowsMoveSeparately)
{
	// Section 5.3 test 2: two rows moving in different
	// directions become two groups, not one.
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form>"
		L"<row name=\"r1\"><field name=\"a\" x=\"10\" y=\"60\" /><field name=\"b\" x=\"120\" y=\"60\" /></row>"
		L"<row name=\"r2\"><field name=\"c\" x=\"10\" y=\"100\" /><field name=\"d\" x=\"120\" y=\"100\" /></row>"
		L"</form>",
		L"<form>"
		L"<row name=\"r1\"><field name=\"a\" x=\"10\" y=\"72\" /><field name=\"b\" x=\"120\" y=\"72\" /></row>"
		L"<row name=\"r2\"><field name=\"c\" x=\"10\" y=\"95\" /><field name=\"d\" x=\"120\" y=\"95\" /></row>"
		L"</form>");
	EXPECT_EQ(outcome.result.changeCount, 4);
	ASSERT_EQ(outcome.groups.size(), static_cast<size_t>(2));
	EXPECT_EQ(outcome.groups[0].recordIndices.size(), static_cast<size_t>(2));
	EXPECT_EQ(outcome.groups[1].recordIndices.size(), static_cast<size_t>(2));
	EXPECT_NE(outcome.groups[0].summary, outcome.groups[1].summary);
	EXPECT_TRUE(Contains(outcome.groups[0].summary, L"\u4e0b\u306b12\u5358\u4f4d"));
	EXPECT_TRUE(Contains(outcome.groups[1].summary, L"\u4e0a\u306b5\u5358\u4f4d"));
}

TEST(Merge20FormDiff, NoFalseMoveOrRename)
{
	// Section 5.1 test 4: a real deletion and a real addition
	// must stay ADD / DEL even though both sit at the same
	// structural position (the coordinates differ, so no rename).
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form><field name=\"gone\" x=\"10\" y=\"20\" /><field name=\"keep\" x=\"10\" y=\"50\" /></form>",
		L"<form><field name=\"fresh\" x=\"200\" y=\"20\" /><field name=\"keep\" x=\"10\" y=\"50\" /></form>");
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Add), 1);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Del), 1);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Moved), 0);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Renamed), 0);
	EXPECT_EQ(outcome.result.changeCount, 2);
}

TEST(Merge20FormDiff, OrderOnlySwapIgnored)
{
	// Same-parent order-only swap (design section 5.1, T-F-2):
	// display order alone does not change the layout.
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form><field name=\"a\" x=\"10\" y=\"20\" /><field name=\"b\" x=\"10\" y=\"50\" /></form>",
		L"<form><field name=\"b\" x=\"10\" y=\"50\" /><field name=\"a\" x=\"10\" y=\"20\" /></form>");
	EXPECT_EQ(outcome.result.changeCount, 0);
	EXPECT_TRUE(outcome.result.diffRecords.empty());
}

TEST(Merge20FormDiff, NoiseIgnored)
{
	// Section 5.2 test 1: every coordinate shifts by 0.3, below
	// the 0.5 threshold, so there is no diff at all.
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form><field name=\"a\" x=\"10\" y=\"20\" width=\"100\" height=\"30\" />"
		L"<field name=\"b\" x=\"10\" y=\"60\" width=\"100\" height=\"30\" /></form>",
		L"<form><field name=\"a\" x=\"10.3\" y=\"20.3\" width=\"100\" height=\"30\" />"
		L"<field name=\"b\" x=\"10.3\" y=\"60.3\" width=\"100\" height=\"30\" /></form>");
	EXPECT_EQ(outcome.result.changeCount, 0);
	EXPECT_EQ(outcome.ignoredNoiseCount, 4);
	EXPECT_DOUBLE_EQ(outcome.result.totalNumericSum, 0.0);
	EXPECT_TRUE(outcome.groups.empty());
}

TEST(Merge20FormDiff, NoiseBoundary)
{
	// Section 5.2 test 2: a shift of exactly 0.5 is still a change.
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form><field name=\"a\" x=\"10\" y=\"20\" /></form>",
		L"<form><field name=\"a\" x=\"10\" y=\"20.5\" /></form>");
	EXPECT_EQ(outcome.result.changeCount, 1);
	EXPECT_EQ(outcome.ignoredNoiseCount, 0);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Coord), 1);
	const Merge20FormDiffRecord* record = FindRecord(outcome, Merge20FormDiffType::Coord);
	ASSERT_NE(record, nullptr);
	EXPECT_DOUBLE_EQ(record->diffVal, 0.5);
	EXPECT_TRUE(Contains(record->desc, L"+0.5 \u5358\u4f4d"));
}

TEST(Merge20FormDiff, DecimalDisplay)
{
	// Section 5.2 test 3: the fractional part is kept
	// (the HTML tool rounds 100 -> 112.5 to 13).
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form><field name=\"a\" x=\"100\" y=\"20\" /></form>",
		L"<form><field name=\"a\" x=\"112.5\" y=\"20\" /></form>");
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Coord), 1);
	const Merge20FormDiffRecord* record = FindRecord(outcome, Merge20FormDiffType::Coord);
	ASSERT_NE(record, nullptr);
	EXPECT_DOUBLE_EQ(record->diffVal, 12.5);
	EXPECT_TRUE(Contains(record->desc, L"+12.5 \u5358\u4f4d"));
	EXPECT_EQ(Merge20FormatFormDelta(12.5), std::wstring(L"+12.5\u5358\u4f4d"));
	EXPECT_EQ(Merge20FormatFormDelta(-3.0), std::wstring(L"-3\u5358\u4f4d"));
	EXPECT_EQ(Merge20FormatFormDelta(0.5), std::wstring(L"+0.5\u5358\u4f4d"));
}

TEST(Merge20FormDiff, MinorGroup)
{
	// Section 5.2: a 0.7 change is kept but collected into the
	// minor group.
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form><field name=\"a\" x=\"10\" y=\"20\" /></form>",
		L"<form><field name=\"a\" x=\"10\" y=\"20.7\" /></form>");
	EXPECT_EQ(outcome.result.changeCount, 1);
	ASSERT_EQ(outcome.groups.size(), static_cast<size_t>(1));
	EXPECT_EQ(outcome.groups[0].id, std::wstring(L"minor"));
	EXPECT_TRUE(outcome.groups[0].minor);
	EXPECT_TRUE(Contains(outcome.groups[0].summary, L"\u5fae\u5c0f\u306a\u5dee 1\u4ef6"));
	EXPECT_EQ(outcome.result.diffRecords[0].groupId, std::wstring(L"minor"));
}

TEST(Merge20FormDiff, Interpretation)
{
	// `getDetailInterpretation` port (design section 10 F1 (c)):
	// the wording is Japanese and matches the HTML tool.
	Merge20FormInterpretation interp =
		Merge20InterpretFormAttribute(L"width", L"100", L"112", true, true);
	EXPECT_TRUE(interp.isCoord);
	EXPECT_DOUBLE_EQ(interp.numericDiff, 12.0);
	EXPECT_EQ(interp.desc, std::wstring(L"\u6a2a\u5e45\u304c +12 \u5358\u4f4d \u62e1\u5927"));
	EXPECT_TRUE(Contains(interp.impact, L"\u53f3\u3078\u30b7\u30d5\u30c8"));

	interp = Merge20InterpretFormAttribute(L"x", L"100", L"88", true, true);
	EXPECT_TRUE(interp.isCoord);
	EXPECT_EQ(interp.desc, std::wstring(L"\u5de6\u65b9\u5411\u3078 -12 \u5358\u4f4d \u79fb\u52d5"));
	EXPECT_TRUE(Contains(interp.impact, L"\u5b9f\u6a5f\u5370\u5b57\u78ba\u8a8d\u3092\u63a8\u5968"));

	interp = Merge20InterpretFormAttribute(L"height", L"30", L"20", true, true);
	EXPECT_EQ(interp.desc, std::wstring(L"\u7e26\u5e45\u304c -10 \u5358\u4f4d \u7e2e\u5c0f"));

	interp = Merge20InterpretFormAttribute(L"y", L"20", L"32", true, true);
	EXPECT_EQ(interp.desc, std::wstring(L"\u4e0b\u65b9\u5411\u3078 +12 \u5358\u4f4d \u79fb\u52d5"));
	EXPECT_TRUE(Contains(interp.impact, L"\u5e72\u6e09\u30ea\u30b9\u30af\u3042\u308a"));

	interp = Merge20InterpretFormAttribute(L"point", L"12", L"14", true, true);
	EXPECT_FALSE(interp.isCoord);
	EXPECT_DOUBLE_EQ(interp.numericDiff, 2.0);
	EXPECT_EQ(interp.desc, std::wstring(L"\u30d5\u30a9\u30f3\u30c8\u30b5\u30a4\u30ba\u304c 12pt \u2794 14pt \u3078\u5909\u66f4"));
	EXPECT_TRUE(Contains(interp.impact, L"\u6587\u5b57\u62e1\u5927\u306b\u4f34\u3044"));

	interp = Merge20InterpretFormAttribute(L"fontsize", L"14", L"12", true, true);
	EXPECT_TRUE(Contains(interp.impact, L"\u6587\u5b57\u7e2e\u5c0f\u306b\u3088\u308a"));

	interp = Merge20InterpretFormAttribute(L"version", L"1.0", L"2.0", true, true);
	EXPECT_DOUBLE_EQ(interp.numericDiff, 1.0);
	EXPECT_EQ(interp.desc, std::wstring(L"\u30d5\u30a9\u30fc\u30e0\u5b9a\u7fa9\u30d0\u30fc\u30b8\u30e7\u30f3\u304c 1.0 \u2794 2.0 \u3078\u66f4\u65b0"));

	interp = Merge20InterpretFormAttribute(L"color", L"red", L"blue", true, true);
	EXPECT_FALSE(interp.isCoord);
	EXPECT_EQ(interp.desc, std::wstring(L"\u5c5e\u6027\u5024\u300ccolor\u300d\u304c red \u2794 blue \u306b\u5909\u66f4"));

	// A coordinate change with one decimal place.
	interp = Merge20InterpretFormAttribute(L"x", L"100", L"112.5", true, true);
	EXPECT_TRUE(interp.isCoord);
	EXPECT_DOUBLE_EQ(interp.numericDiff, 12.5);
	EXPECT_TRUE(Contains(interp.desc, L"+12.5 \u5358\u4f4d"));

	// A non-numeric coordinate value falls through to the generic
	// wording (the HTML tool leaves `desc` empty there).
	interp = Merge20InterpretFormAttribute(L"x", L"auto", L"100", true, true);
	EXPECT_FALSE(interp.isCoord);
	EXPECT_FALSE(interp.desc.empty());
	EXPECT_FALSE(interp.impact.empty());
}

TEST(Merge20FormDiff, TextChange)
{
	// The HTML tool ignores element text; F1 detects it (F0
	// keeps `text` for this comparison).
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form><field name=\"a\" x=\"1\" y=\"2\">hello</field></form>",
		L"<form><field name=\"a\" x=\"1\" y=\"2\">world</field></form>");
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Mod), 1);
	const Merge20FormDiffRecord* record = FindRecord(outcome, Merge20FormDiffType::Mod);
	ASSERT_NE(record, nullptr);
	EXPECT_EQ(record->attr, std::wstring(L"\u30c6\u30ad\u30b9\u30c8"));
	EXPECT_TRUE(Contains(record->desc, L"hello"));
	EXPECT_TRUE(Contains(record->desc, L"world"));
}

TEST(Merge20FormDiff, NamelessElementAttrChange)
{
	// A nameless element whose attributes change: the attribute
	// signature is part of its stable key, so the element is
	// paired by structural path plus tag (stage 3.5) instead of
	// being reported as deleted plus added.
	const Merge20FormDiffOutcome outcome = DiffPair(
		L"<form version=\"1.0\"><field name=\"a\" x=\"1\" y=\"2\" /></form>",
		L"<form version=\"2.0\"><field name=\"a\" x=\"1\" y=\"2\" /></form>");
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Mod), 1);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Add), 0);
	EXPECT_EQ(CountType(outcome, Merge20FormDiffType::Del), 0);
	const Merge20FormDiffRecord* record = FindRecord(outcome, Merge20FormDiffType::Mod);
	ASSERT_NE(record, nullptr);
	EXPECT_EQ(record->attr, std::wstring(L"version"));
}

TEST(Merge20FormDiff, FileAddDelError)
{
	const std::wstring xml = L"<form><field name=\"a\" x=\"1\" y=\"2\" /></form>";
	const Merge20FormDiffOutcome added = Merge20DiffFormFiles(nullptr, &xml, L"new.xml");
	ASSERT_EQ(added.result.diffRecords.size(), static_cast<size_t>(1));
	EXPECT_EQ(added.result.diffRecords[0].type, Merge20FormDiffType::FileAdd);
	EXPECT_EQ(added.result.addCount, 1);
	EXPECT_DOUBLE_EQ(added.result.totalNumericSum, 1.0);
	EXPECT_EQ(added.result.tendency, std::wstring(L"\u65b0\u898f\u30d5\u30a1\u30a4\u30eb\u8ffd\u52a0"));
	EXPECT_EQ(added.result.severity, Merge20FormSeverity::LayoutBreaking);
	EXPECT_FALSE(added.result.diffRecords[0].hasVisual);

	const Merge20FormDiffOutcome deleted = Merge20DiffFormFiles(&xml, nullptr, L"old.xml");
	ASSERT_EQ(deleted.result.diffRecords.size(), static_cast<size_t>(1));
	EXPECT_EQ(deleted.result.diffRecords[0].type, Merge20FormDiffType::FileDel);
	EXPECT_EQ(deleted.result.delCount, 1);
	EXPECT_EQ(deleted.result.tendency, std::wstring(L"\u65e2\u5b58\u30d5\u30a1\u30a4\u30eb\u524a\u9664"));
	EXPECT_EQ(deleted.result.severity, Merge20FormSeverity::LayoutBreaking);

	const std::wstring broken = L"<a><b></a></b>";
	const Merge20FormDiffOutcome error = Merge20DiffFormFiles(&xml, &broken, L"bad.xml");
	ASSERT_EQ(error.result.diffRecords.size(), static_cast<size_t>(1));
	EXPECT_EQ(error.result.diffRecords[0].type, Merge20FormDiffType::Error);
	EXPECT_EQ(error.result.tendency, std::wstring(L"XML\u69cb\u6587\u30a8\u30e9\u30fc"));
	EXPECT_EQ(error.result.severity, Merge20FormSeverity::LayoutBreaking);
}

TEST(Merge20FormDiff, LargeChangeFlag)
{
	// Design section 4.4: total > average * 1.5 and total > 20.
	std::vector<Merge20FormFileResult> results;
	{
		const Merge20FormDiffOutcome a = DiffPair(
			L"<form><field name=\"a\" x=\"0\" y=\"0\" /></form>",
			L"<form><field name=\"a\" x=\"100\" y=\"0\" /></form>");
		results.push_back(a.result);
	}
	{
		const Merge20FormDiffOutcome b = DiffPair(
			L"<form><field name=\"a\" x=\"0\" y=\"0\" /></form>",
			L"<form><field name=\"a\" x=\"10\" y=\"0\" /></form>");
		results.push_back(b.result);
	}
	{
		const Merge20FormDiffOutcome c = DiffPair(
			L"<form><field name=\"a\" x=\"0\" y=\"0\" /></form>",
			L"<form><field name=\"a\" x=\"10\" y=\"0\" /></form>");
		results.push_back(c.result);
	}
	EXPECT_DOUBLE_EQ(results[0].totalNumericSum, 100.0);
	Merge20ApplyLargeChangeFlag(results);
	EXPECT_TRUE(results[0].isWarning); // 100 > 40 * 1.5 and > 20.
	EXPECT_FALSE(results[1].isWarning);
	EXPECT_FALSE(results[2].isWarning);
	EXPECT_EQ(results[0].severity, Merge20FormSeverity::LayoutBreaking);
	EXPECT_EQ(results[1].severity, Merge20FormSeverity::VisualOnly);
}

TEST(Merge20FormDiff, Severity)
{
	// Coordinate-only change: visual only.
	const Merge20FormDiffOutcome coord = DiffPair(
		L"<form><field name=\"a\" x=\"0\" y=\"0\" /></form>",
		L"<form><field name=\"a\" x=\"5\" y=\"0\" /></form>");
	EXPECT_EQ(coord.result.severity, Merge20FormSeverity::VisualOnly);

	// Element addition only: informational.
	const Merge20FormDiffOutcome added = DiffPair(
		L"<form><field name=\"a\" x=\"0\" y=\"0\" /></form>",
		L"<form><field name=\"a\" x=\"0\" y=\"0\" /><field name=\"b\" x=\"50\" y=\"0\" /></form>");
	EXPECT_EQ(added.result.severity, Merge20FormSeverity::Info);
	EXPECT_EQ(CountType(added, Merge20FormDiffType::Add), 1);

	// No change at all.
	const Merge20FormDiffOutcome same = DiffPair(
		L"<form><field name=\"a\" x=\"0\" y=\"0\" /></form>",
		L"<form><field name=\"a\" x=\"0\" y=\"0\" /></form>");
	EXPECT_EQ(same.result.changeCount, 0);
	EXPECT_EQ(same.result.severity, Merge20FormSeverity::Info);
}
