/**
 * @file Merge20FormMaster_test.cpp
 *
 * @brief Merge20 (Wave 5 F6) unit tests for the FORM name master:
 * the default data (design appendix A), the name resolution
 * (HTML `getFormName`), the text format and the validation rules
 * (design sections 2.10, 8 and 10 F6 (a)-(b)).
 *
 * This file stays ASCII: the Japanese test text is written with \u
 * escapes, like Merge20FormXml_test.cpp.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20FormMaster.h"

namespace
{

	Merge20FormMasterEntry Row(const wchar_t* id, const wchar_t* name)
	{
		return Merge20FormMasterEntry{ id, name };
	}

} // namespace

// Acceptance (a): the default data is design appendix A
// (171 rows, 142 unique ids).
TEST(Merge20FormMaster, DefaultHas171RowsAnd142UniqueIds)
{
	const Merge20FormMaster master = Merge20FormMasterGetDefault();
	EXPECT_EQ(171u, master.entries.size());
	EXPECT_EQ(142, Merge20FormMasterCountUniqueIds(master));
}

// Acceptance (a): spot checks of the default data (first row,
// a multi-name id, the last row).
TEST(Merge20FormMaster, DefaultSpotChecks)
{
	const Merge20FormMaster master = Merge20FormMasterGetDefault();
	ASSERT_FALSE(master.entries.empty());
	EXPECT_EQ(L"ZEB001R007", master.entries.front().id);
	EXPECT_EQ(L"\u8cc7\u7523\u8a3c\u660e\u66f8", master.entries.front().name);
	EXPECT_EQ(L"ZEB905R002", master.entries.back().id);
	EXPECT_EQ(L"\u6708\u5225\u96c6\u8a08\u8868\uff08\u8abf\u5b9a\uff09", master.entries.back().name);
	int zeb302Count = 0;
	for (const Merge20FormMasterEntry& entry : master.entries)
	{
		if (entry.id == L"ZEB302R001")
			++zeb302Count;
	}
	EXPECT_EQ(10, zeb302Count);
}

// Acceptance (a): an exact base-name match resolves to the name.
TEST(Merge20FormMaster, ResolveExactMatch)
{
	const Merge20FormMaster master = Merge20FormMasterGetDefault();
	EXPECT_EQ(L"\u8cc7\u7523\u8a3c\u660e\u66f8", Merge20FormMasterResolve(master, L"ZEB001R007.xml"));
	EXPECT_EQ(L"\u524d\u5e74\u7a0e\u984d\u6bd4\u8f03\u4e00\u89a7\u8868",
		Merge20FormMasterResolve(master, L"sub/ZEB327R001.xml"));
	EXPECT_EQ(L"\u8cc7\u7523\u8a3c\u660e\u66f8",
		Merge20FormMasterResolve(master, L"C:\\forms\\ZEB001R007.xml"));
}

// Acceptance (a): no exact match falls back to the first id
// contained in the base name (HTML `getFormName`).
TEST(Merge20FormMaster, ResolveContainsId)
{
	const Merge20FormMaster master = Merge20FormMasterGetDefault();
	EXPECT_EQ(L"\u8cc7\u7523\u8a3c\u660e\u66f8",
		Merge20FormMasterResolve(master, L"ZEB001R007_2024.xml"));
}

// Acceptance (a): up to 3 names are joined with " / ".
TEST(Merge20FormMaster, ResolveUpToThreeNamesJoined)
{
	const Merge20FormMaster master = Merge20FormMasterGetDefault();
	EXPECT_EQ(L"\u540d\u5bc4\u5e33 \uff0f \u540d\u5bc4\u5e33\uff08\u7269\u4ef6\u9078\u629e\uff09 \uff0f \u540d\u5bc4\u5e33\uff08\u5c65\u6b74\uff09",
		Merge20FormMasterResolve(master, L"ZEB001R006.xml"));
	EXPECT_EQ(L"\u571f\u5730\uff08\u88dc\u5145\uff09\u8ab2\u7a0e\u53f0\u5e33\uff08\u95b2\u89a7\u7528\uff09 \uff0f \u571f\u5730\uff08\u88dc\u5145\uff09\u8ab2\u7a0e\u53f0\u5e33\uff08\u5185\u90e8\u7528\uff09",
		Merge20FormMasterResolve(master, L"ZEB001R013.xml"));
}

// Acceptance (a): 4 or more names show "first name + N more kinds".
TEST(Merge20FormMaster, ResolveFourOrMoreNamesUsesEtc)
{
	const Merge20FormMaster master = Merge20FormMasterGetDefault();
	EXPECT_EQ(L"\u56fa\u5b9a\u8cc7\u7523\u7a0e\uff3f\u30d6\u30c3\u30ad\u30f3\u30b0\u7d0d\u4ed8\u66f8\uff08\u53e3\u5ea7\u5916\uff09 \u307b\u304b\u51684\u7a2e",
		Merge20FormMasterResolve(master, L"ZEB204R003.xml"));
	EXPECT_EQ(L"\u7d0d\u7a0e\u901a\u77e5\u66f8\uff08\u4e00\u822c\uff09 \u307b\u304b\u51684\u7a2e",
		Merge20FormMasterResolve(master, L"ZEB204R009.xml"));
	EXPECT_EQ(L"\u4e00\u62ec\u540d\u5bc4\u5e33(1\uff5e20000) \u307b\u304b\u516810\u7a2e",
		Merge20FormMasterResolve(master, L"ZEB302R001.xml"));
}

// Acceptance (a): an unregistered id shows "(unregistered)".
TEST(Merge20FormMaster, ResolveUnregistered)
{
	const Merge20FormMaster master = Merge20FormMasterGetDefault();
	EXPECT_EQ(L"\uff08\u540d\u79f0\u672a\u767b\u9332\uff09", Merge20FormMasterResolve(master, L"UNKNOWN001.xml"));
	EXPECT_EQ(L"\uff08\u540d\u79f0\u672a\u767b\u9332\uff09", Merge20FormMasterResolve(master, L"ZEB999R999.xml"));
}

// The text format is `ID<TAB>name` per line; blank lines are
// ignored (design section 8.2).
TEST(Merge20FormMaster, ParseIgnoresBlankLines)
{
	Merge20FormMaster master;
	int badLine = -1;
	ASSERT_TRUE(Merge20FormMasterParseText(
		L"ZEB001R007\t\u8cc7\u7523\u8a3c\u660e\u66f8\r\n\r\n   \r\nZEB001R008\t\u516c\u8ab2\u8a3c\u660e\u66f8\uff08\u511f\u5374\u8cc7\u7523\uff09\r\n",
		master, badLine));
	EXPECT_EQ(0, badLine);
	ASSERT_EQ(2u, master.entries.size());
	EXPECT_EQ(L"ZEB001R007", master.entries[0].id);
	EXPECT_EQ(L"\u516c\u8ab2\u8a3c\u660e\u66f8\uff08\u511f\u5374\u8cc7\u7523\uff09", master.entries[1].name);
}

// Acceptance (b): a line without a tab warns and aborts the save
// (the parse fails and reports the 1-based line number).
TEST(Merge20FormMaster, ParseRejectsTablessLine)
{
	Merge20FormMaster master;
	int badLine = -1;
	EXPECT_FALSE(Merge20FormMasterParseText(
		L"ZEB001R007 \u8cc7\u7523\u8a3c\u660e\u66f8\r\n", master, badLine));
	EXPECT_EQ(1, badLine);
	EXPECT_TRUE(master.entries.empty());
	EXPECT_FALSE(Merge20FormMasterParseText(
		L"ZEB001R007\t\u8cc7\u7523\u8a3c\u660e\u66f8\r\nZEB001R008 \u516c\u8ab2\u8a3c\u660e\u66f8\r\n", master, badLine));
	EXPECT_EQ(2, badLine);
}

// Acceptance (b): a line with an empty id or an empty name is rejected.
TEST(Merge20FormMaster, ParseRejectsEmptyIdOrName)
{
	Merge20FormMaster master;
	int badLine = -1;
	EXPECT_FALSE(Merge20FormMasterParseText(L"\t\u8cc7\u7523\u8a3c\u660e\u66f8\r\n", master, badLine));
	EXPECT_EQ(1, badLine);
	EXPECT_FALSE(Merge20FormMasterParseText(L"ZEB001R007\t\r\n", master, badLine));
	EXPECT_EQ(1, badLine);
}

// Acceptance (b): a duplicated (id, name) row is removed on
// parse; a different name for the same id is not a duplicate.
TEST(Merge20FormMaster, ParseDedupsIdenticalRows)
{
	Merge20FormMaster master;
	int badLine = -1;
	ASSERT_TRUE(Merge20FormMasterParseText(
		L"ZEB001R007\t\u8cc7\u7523\u8a3c\u660e\u66f8\r\nZEB001R007\t\u8cc7\u7523\u8a3c\u660e\u66f8\r\n"
		L"ZEB001R007\t\u8cc7\u7523\u8a3c\u660e\u66f8\uff08\u5199\uff09\r\n",
		master, badLine));
	ASSERT_EQ(2u, master.entries.size());
	EXPECT_EQ(L"\u8cc7\u7523\u8a3c\u660e\u66f8", master.entries[0].name);
	EXPECT_EQ(L"\u8cc7\u7523\u8a3c\u660e\u66f8\uff08\u5199\uff09", master.entries[1].name);
}

// The serialization is the tab-separated format, and a
// serialize/parse round trip keeps the default data.
TEST(Merge20FormMaster, ToTextFormatAndRoundTrip)
{
	Merge20FormMaster master;
	master.entries.push_back(Row(L"A001", L"\u5e33\u7968\u3042"));
	master.entries.push_back(Row(L"A002", L"\u5e33\u7968\u3044"));
	EXPECT_EQ(L"A001\t\u5e33\u7968\u3042\r\nA002\t\u5e33\u7968\u3044\r\n", Merge20FormMasterToText(master));

	const Merge20FormMaster def = Merge20FormMasterGetDefault();
	Merge20FormMaster parsed;
	int badLine = -1;
	ASSERT_TRUE(Merge20FormMasterParseText(Merge20FormMasterToText(def), parsed, badLine));
	ASSERT_EQ(def.entries.size(), parsed.entries.size());
	for (size_t i = 0; i < def.entries.size(); ++i)
	{
		EXPECT_EQ(def.entries[i].id, parsed.entries[i].id);
		EXPECT_EQ(def.entries[i].name, parsed.entries[i].name);
	}
}

// The count badge counts unique ids, not rows.
TEST(Merge20FormMaster, CountUniqueIds)
{
	Merge20FormMaster master;
	master.entries.push_back(Row(L"A001", L"\u5e33\u7968\u3042"));
	master.entries.push_back(Row(L"A001", L"\u5e33\u7968\u30422"));
	master.entries.push_back(Row(L"A002", L"\u5e33\u7968\u3044"));
	EXPECT_EQ(2, Merge20FormMasterCountUniqueIds(master));
	EXPECT_EQ(0, Merge20FormMasterCountUniqueIds(Merge20FormMaster()));
}
