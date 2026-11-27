/**
 * @file Merge20FormTable_test.cpp
 *
 * @brief Merge20 (Wave 5 F5) unit tests for the structured diff
 * table rows: grouped display + expand (design section 5.3,
 * acceptance tests 1-3 of section 10 F5 (c)), the minor group
 * (design section 5.2) and the unit-suffixed values (design
 * section 5.2 test 3).
 *
 * This file stays ASCII: the Japanese test text is written with \u
 * escapes, like Merge20FormXml_test.cpp.
 */
#include "pch.h"
#include <algorithm>
#include <gtest/gtest.h>
#include "Merge20/Merge20FormDiff.h"
#include "Merge20/Merge20FormTable.h"

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

	int CountGroups(const std::vector<Merge20FormTableRow>& rows)
	{
		int count = 0;
		for (const Merge20FormTableRow& row : rows)
		{
			if (row.isGroup)
				++count;
		}
		return count;
	}

	// One row of five fields (design section 5.1 test 3 /
	// section 5.3 test 1): the row moves down 12 units.
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

	// Design section 5.3 test 1: a row move is one group row,
	// collapsed by default, and expands to the five members.
	TEST(Merge20FormTable, RowMoveGrouped)
	{
		Merge20FormDiffOutcome outcome = DiffPair(kRowOldXml, kRowNewXml);
		ASSERT_EQ(outcome.groups.size(), 1u);
		const std::set<std::wstring> collapsed;
		std::vector<Merge20FormTableRow> rows =
			Merge20BuildFormDiffTableRows(outcome.result, outcome.groups, collapsed);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_TRUE(rows[0].isGroup);
		EXPECT_FALSE(rows[0].minor);
		EXPECT_EQ(rows[0].recordIndex, -1);
		EXPECT_TRUE(Contains(rows[0].typeText, L"\u30b0\u30eb\u30fc\u30d7"));
		EXPECT_TRUE(Contains(rows[0].elementText, L"5\u9805\u76ee"));
		EXPECT_TRUE(Contains(rows[0].valueText, L"\u4e0b\u306b"));
		EXPECT_TRUE(Contains(rows[0].valueText, L"12"));
		const std::set<std::wstring> expanded{ outcome.groups[0].id };
		rows = Merge20BuildFormDiffTableRows(outcome.result, outcome.groups, expanded);
		ASSERT_EQ(rows.size(), 6u);
		EXPECT_TRUE(rows[0].isGroup);
		EXPECT_TRUE(rows[0].expanded);
		for (size_t i = 1; i < rows.size(); ++i)
		{
			EXPECT_FALSE(rows[i].isGroup);
			EXPECT_GE(rows[i].recordIndex, 0);
			EXPECT_EQ(rows[i].groupId, outcome.groups[0].id);
			// Child rows are indented.
			EXPECT_TRUE(rows[i].elementText.compare(0, 2, L"  ") == 0);
		}
	}

	// Design section 5.3 test 2: two rows moving apart stay two groups.
	TEST(Merge20FormTable, TwoRowsMoveSeparately)
	{
		const wchar_t* oldXml =
			L"<form>"
			L"<row name=\"row1\">"
			L"<field name=\"a\" x=\"10\" y=\"60\" width=\"100\" height=\"20\" />"
			L"<field name=\"b\" x=\"120\" y=\"60\" width=\"100\" height=\"20\" />"
			L"</row>"
			L"<row name=\"row2\">"
			L"<field name=\"f\" x=\"10\" y=\"100\" width=\"100\" height=\"20\" />"
			L"<field name=\"g\" x=\"120\" y=\"100\" width=\"100\" height=\"20\" />"
			L"</row>"
			L"</form>";
		const wchar_t* newXml =
			L"<form>"
			L"<row name=\"row1\">"
			L"<field name=\"a\" x=\"10\" y=\"72\" width=\"100\" height=\"20\" />"
			L"<field name=\"b\" x=\"120\" y=\"72\" width=\"100\" height=\"20\" />"
			L"</row>"
			L"<row name=\"row2\">"
			L"<field name=\"f\" x=\"10\" y=\"92\" width=\"100\" height=\"20\" />"
			L"<field name=\"g\" x=\"120\" y=\"92\" width=\"100\" height=\"20\" />"
			L"</row>"
			L"</form>";
		Merge20FormDiffOutcome outcome = DiffPair(oldXml, newXml);
		ASSERT_EQ(outcome.groups.size(), 2u);
		const std::set<std::wstring> collapsed;
		std::vector<Merge20FormTableRow> rows =
			Merge20BuildFormDiffTableRows(outcome.result, outcome.groups, collapsed);
		ASSERT_EQ(rows.size(), 2u);
		EXPECT_EQ(CountGroups(rows), 2);
	}

	// Design section 5.3 test 3: the group summary holds the
	// amount, the direction and the item count.
	TEST(Merge20FormTable, GroupSummaryText)
	{
		Merge20FormDiffOutcome outcome = DiffPair(kRowOldXml, kRowNewXml);
		ASSERT_EQ(outcome.groups.size(), 1u);
		const std::set<std::wstring> collapsed;
		std::vector<Merge20FormTableRow> rows =
			Merge20BuildFormDiffTableRows(outcome.result, outcome.groups, collapsed);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_TRUE(Contains(rows[0].descText, L"12"));
		EXPECT_TRUE(Contains(rows[0].descText, L"\u4e0b\u306b"));
		EXPECT_TRUE(Contains(rows[0].descText, L"5"));
	}

	// Design section 5.2: 0.5-1.0 changes go to the minor group,
	// collapsed by default.
	TEST(Merge20FormTable, MinorGroupCollapsedByDefault)
	{
		const wchar_t* oldXml =
			L"<form>"
			L"<field name=\"a\" x=\"10\" y=\"10\" width=\"100\" height=\"20\" />"
			L"</form>";
		const wchar_t* newXml =
			L"<form>"
			L"<field name=\"a\" x=\"10.7\" y=\"10\" width=\"100\" height=\"20\" />"
			L"</form>";
		Merge20FormDiffOutcome outcome = DiffPair(oldXml, newXml);
		ASSERT_EQ(outcome.groups.size(), 1u);
		EXPECT_TRUE(outcome.groups[0].minor);
		const std::set<std::wstring> collapsed;
		std::vector<Merge20FormTableRow> rows =
			Merge20BuildFormDiffTableRows(outcome.result, outcome.groups, collapsed);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_TRUE(rows[0].isGroup);
		EXPECT_TRUE(rows[0].minor);
		EXPECT_TRUE(Contains(rows[0].typeText, L"\u5fae\u5c0f\u306a\u5dee"));
		const std::set<std::wstring> expanded{ L"minor" };
		rows = Merge20BuildFormDiffTableRows(outcome.result, outcome.groups, expanded);
		ASSERT_EQ(rows.size(), 2u);
		EXPECT_FALSE(rows[1].isGroup);
		EXPECT_TRUE(Contains(rows[1].valueText, L"+0.7\u5358\u4f4d"));
	}

	// Design section 5.2 test 3: decimals are kept (`+12.5 units`,
	// not `+12 units`).
	TEST(Merge20FormTable, DecimalDisplay)
	{
		const wchar_t* oldXml =
			L"<form>"
			L"<field name=\"a\" x=\"100\" y=\"10\" width=\"100\" height=\"20\" />"
			L"</form>";
		const wchar_t* newXml =
			L"<form>"
			L"<field name=\"a\" x=\"112.5\" y=\"10\" width=\"100\" height=\"20\" />"
			L"</form>";
		Merge20FormDiffOutcome outcome = DiffPair(oldXml, newXml);
		ASSERT_TRUE(outcome.groups.empty());
		const std::set<std::wstring> collapsed;
		std::vector<Merge20FormTableRow> rows =
			Merge20BuildFormDiffTableRows(outcome.result, outcome.groups, collapsed);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_EQ(rows[0].typeText, L"COORD");
		EXPECT_TRUE(Contains(rows[0].elementText, L"a"));
		EXPECT_TRUE(Contains(rows[0].elementText, L"x"));
		EXPECT_TRUE(Contains(rows[0].valueText, L"100 \u2192 112.5"));
		EXPECT_TRUE(Contains(rows[0].valueText, L"+12.5\u5358\u4f4d"));
	}

	// A file-level record (FILE_ADD) is shown as a row even
	// though it has no visual (design section 2.9).
	TEST(Merge20FormTable, FileLevelRecordShown)
	{
		const std::wstring newText =
			L"<form><field name=\"a\" x=\"10\" y=\"10\" width=\"100\" height=\"20\" /></form>";
		Merge20FormDiffOutcome outcome = Merge20DiffFormFiles(nullptr, &newText, L"test.xml");
		ASSERT_EQ(outcome.result.diffRecords.size(), 1u);
		EXPECT_EQ(outcome.result.diffRecords[0].type, Merge20FormDiffType::FileAdd);
		EXPECT_FALSE(outcome.result.diffRecords[0].hasVisual);
		const std::set<std::wstring> collapsed;
		std::vector<Merge20FormTableRow> rows =
			Merge20BuildFormDiffTableRows(outcome.result, outcome.groups, collapsed);
		ASSERT_EQ(rows.size(), 1u);
		EXPECT_EQ(rows[0].typeText, L"FILE_ADD");
		EXPECT_EQ(rows[0].recordIndex, 0);
	}

	// Type codes follow the HTML vocabulary (design section 2.9).
	TEST(Merge20FormTable, TypeTexts)
	{
		EXPECT_EQ(Merge20FormTableTypeText(Merge20FormDiffType::FileAdd), L"FILE_ADD");
		EXPECT_EQ(Merge20FormTableTypeText(Merge20FormDiffType::FileDel), L"FILE_DEL");
		EXPECT_EQ(Merge20FormTableTypeText(Merge20FormDiffType::Error), L"ERROR");
		EXPECT_EQ(Merge20FormTableTypeText(Merge20FormDiffType::Add), L"ADD");
		EXPECT_EQ(Merge20FormTableTypeText(Merge20FormDiffType::Del), L"DEL");
		EXPECT_EQ(Merge20FormTableTypeText(Merge20FormDiffType::Coord), L"COORD");
		EXPECT_EQ(Merge20FormTableTypeText(Merge20FormDiffType::Mod), L"MOD");
		EXPECT_EQ(Merge20FormTableTypeText(Merge20FormDiffType::Moved), L"MOVED");
		EXPECT_EQ(Merge20FormTableTypeText(Merge20FormDiffType::Renamed), L"RENAMED");
	}

	// Display names come from the stable key: the `name`, or the
	// tag when there is none.
	TEST(Merge20FormTable, DisplayName)
	{
		EXPECT_EQ(Merge20FormTableDisplayName(L"/form/row[1]/field[1]|field|a"), L"a");
		EXPECT_EQ(Merge20FormTableDisplayName(L"/form/line[1]|line|"), L"line");
	}

	// Key contract with the canvas / spotlight (design section
	// 4.1: a shape key is the element's stable key, so the table
	// row, the spotlight and the canvas all address the same
	// element). F1 used to leave the F0 HTML key on the shapes,
	// which made every shape lookup by record key miss.
	TEST(Merge20FormTable, ShapeKeysAreStableKeys)
	{
		const wchar_t* oldXml =
			L"<form>"
			L"<field name=\"a\" x=\"10\" y=\"10\" width=\"100\" height=\"20\" />"
			L"</form>";
		const wchar_t* newXml =
			L"<form>"
			L"<field name=\"a\" x=\"150\" y=\"10\" width=\"100\" height=\"20\" />"
			L"</form>";
		Merge20FormDiffOutcome outcome = DiffPair(oldXml, newXml);
		ASSERT_EQ(outcome.result.diffRecords.size(), 1u);
		const std::wstring& element = outcome.result.diffRecords[0].element;
		bool shapeFound = false;
		for (const Merge20FormShape& shape : outcome.result.newShapes)
		{
			if (shape.key == element)
				shapeFound = true;
		}
		EXPECT_TRUE(shapeFound);
		EXPECT_TRUE(std::find(outcome.result.changedShapeKeys.begin(),
			outcome.result.changedShapeKeys.end(), element) !=
			outcome.result.changedShapeKeys.end());
		bool oldShapeFound = false;
		for (const Merge20FormShape& shape : outcome.result.oldShapes)
		{
			if (shape.key == element)
				oldShapeFound = true;
		}
		EXPECT_TRUE(oldShapeFound);
	}

} // namespace
