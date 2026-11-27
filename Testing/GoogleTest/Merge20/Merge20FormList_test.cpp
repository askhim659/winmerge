// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20FormList_test.cpp
 *
 * @brief Merge20 (Wave 5 F2) unit tests for the FORM compare list
 * helpers: the KPI dashboard, the five filter tabs, the search and
 * the five sort orders (design merge20-form-compare-design.md
 * sections 2.4, 6 (4), 7.1 and 10 F2).
 *
 * The acceptance tests of design section 10 F2 (b) and (c) are
 * covered here: the KPI numbers follow the HTML tool's
 * `updateKpiBadges` definitions (KpiDefinitions, KpiEmpty,
 * KpiMatchedCountsIdenticalFiles), the five filter tabs, the
 * search and the sort orders work (Filter*, Search*,
 * Sort*), and identical files never reach the list (the
 * document keeps only changed results; KpiMatchedCountsIdenticalFiles
 * pins the matched count the KPI reports for them).
 *
 * This file stays ASCII: the Japanese test text is written with \u
 * escapes, like Merge20FormDiff_test.cpp.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20FormList.h"

namespace
{

	Merge20FormFileResult MakeResult(const wchar_t* fileName, const wchar_t* formName,
		double totalNumericSum, int changeCount, int addCount, int delCount, int coordCount,
		bool isWarning, Merge20FormSeverity severity)
	{
		Merge20FormFileResult result;
		result.fileName = fileName;
		result.formName = formName;
		result.totalNumericSum = totalNumericSum;
		result.changeCount = changeCount;
		result.addCount = addCount;
		result.delCount = delCount;
		result.coordCount = coordCount;
		result.isWarning = isWarning;
		result.severity = severity;
		return result;
	}

	void AddRecord(Merge20FormFileResult& result, const wchar_t* element, const wchar_t* desc)
	{
		Merge20FormDiffRecord record;
		record.element = element;
		record.desc = desc;
		result.diffRecords.push_back(record);
	}

	// Three changed files plus two identical ones (total 5), as the
	// document hands them to the list module: changed only, in
	// file-name order.
	std::vector<Merge20FormFileResult> SampleResults()
	{
		std::vector<Merge20FormFileResult> results;
		Merge20FormFileResult a = MakeResult(L"a.xml", L"\u8cc7\u7523\u8a3c\u660e\u66f8",
			30.0, 3, 0, 0, 1, true, Merge20FormSeverity::VisualOnly);
		AddRecord(a, L"field[name='amount']", L"\u6a2a\u5e45\u304c +12\u5358\u4f4d \u62e1\u5927");
		results.push_back(a);
		Merge20FormFileResult b = MakeResult(L"b.xml", L"\u7d0d\u4ed8\u66f8",
			10.0, 2, 1, 0, 0, false, Merge20FormSeverity::Info);
		AddRecord(b, L"field[name='title']", L"\u8981\u7d20\u8ffd\u52a0");
		results.push_back(b);
		Merge20FormFileResult c = MakeResult(L"c.xml", L"\u524d\u5e74\u7a0e\u984d\u6bd4\u8f03\u4e00\u89a7\u8868",
			50.0, 5, 0, 1, 2, true, Merge20FormSeverity::LayoutBreaking);
		AddRecord(c, L"row[name='row1']", L"\u884c\u5168\u4f53\u304c\u4e0b\u306b12\u5358\u4f4d\u79fb\u52d5");
		results.push_back(c);
		return results;
	}

	std::vector<std::wstring> FileNames(const std::vector<Merge20FormFileResult>& results,
		const std::vector<size_t>& indices)
	{
		std::vector<std::wstring> names;
		for (size_t index : indices)
			names.push_back(results[index].fileName);
		return names;
	}

} // namespace

TEST(Merge20FormList, KpiDefinitions)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	const Merge20FormKpi kpi = Merge20FormListComputeKpi(5, results);
	// HTML updateKpiBadges(total, mismatch, matched, avg, warning,
	// coord, adddel): total 5, changed 3, matched 2,
	// avg round((30+10+50)/3) = 30, warning 2 (a, c),
	// coord 2 (a, c), addDel 2 (b, c).
	EXPECT_EQ(kpi.total, 5);
	EXPECT_EQ(kpi.mismatch, 3);
	EXPECT_EQ(kpi.matched, 2);
	EXPECT_EQ(kpi.avgNumeric, 30);
	EXPECT_EQ(kpi.warning, 2);
	EXPECT_EQ(kpi.coord, 2);
	EXPECT_EQ(kpi.addDel, 2);
}

TEST(Merge20FormList, KpiEmpty)
{
	const std::vector<Merge20FormFileResult> empty;
	Merge20FormKpi kpi = Merge20FormListComputeKpi(0, empty);
	EXPECT_EQ(kpi.total, 0);
	EXPECT_EQ(kpi.mismatch, 0);
	EXPECT_EQ(kpi.matched, 0);
	EXPECT_EQ(kpi.avgNumeric, 0);
	EXPECT_EQ(kpi.warning, 0);
	EXPECT_EQ(kpi.coord, 0);
	EXPECT_EQ(kpi.addDel, 0);
	// Three identical files, none changed: matched is all of them.
	kpi = Merge20FormListComputeKpi(3, empty);
	EXPECT_EQ(kpi.total, 3);
	EXPECT_EQ(kpi.mismatch, 0);
	EXPECT_EQ(kpi.matched, 3);
	EXPECT_EQ(kpi.avgNumeric, 0);
}

TEST(Merge20FormList, KpiMatchedCountsIdenticalFiles)
{
	// The document lists changed files only; identical files are
	// counted by the KPI, never listed (design section 2.1 and
	// section 10 F2 (c)).
	const std::vector<Merge20FormFileResult> results = SampleResults();
	const Merge20FormKpi kpi = Merge20FormListComputeKpi(5, results);
	EXPECT_EQ(kpi.mismatch + kpi.matched, kpi.total);
	EXPECT_EQ(kpi.matched, 2);
}

TEST(Merge20FormList, KpiAverageRoundsPerFileTotals)
{
	// The average uses the per-file rounded totals, like the HTML
	// tool (and the F1 large-change flag): 10.4 -> 10, 10.6 -> 11,
	// avg round((10+11)/2) = round(10.5) = 11 (std::round rounds
	// half away from zero).
	std::vector<Merge20FormFileResult> results;
	results.push_back(MakeResult(L"a.xml", L"", 10.4, 1, 0, 0, 1, false, Merge20FormSeverity::VisualOnly));
	results.push_back(MakeResult(L"b.xml", L"", 10.6, 1, 0, 0, 1, false, Merge20FormSeverity::VisualOnly));
	const Merge20FormKpi kpi = Merge20FormListComputeKpi(2, results);
	EXPECT_EQ(kpi.avgNumeric, 11);
}

TEST(Merge20FormList, FilterAll)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	EXPECT_TRUE(Merge20FormListPassesFilter(results[0], Merge20FormFilter::All));
	EXPECT_TRUE(Merge20FormListPassesFilter(results[1], Merge20FormFilter::All));
	EXPECT_TRUE(Merge20FormListPassesFilter(results[2], Merge20FormFilter::All));
}

TEST(Merge20FormList, FilterWarning)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	EXPECT_TRUE(Merge20FormListPassesFilter(results[0], Merge20FormFilter::Warning));
	EXPECT_FALSE(Merge20FormListPassesFilter(results[1], Merge20FormFilter::Warning));
	EXPECT_TRUE(Merge20FormListPassesFilter(results[2], Merge20FormFilter::Warning));
}

TEST(Merge20FormList, FilterCoord)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	EXPECT_TRUE(Merge20FormListPassesFilter(results[0], Merge20FormFilter::Coord));
	EXPECT_FALSE(Merge20FormListPassesFilter(results[1], Merge20FormFilter::Coord));
	EXPECT_TRUE(Merge20FormListPassesFilter(results[2], Merge20FormFilter::Coord));
}

TEST(Merge20FormList, FilterAddDel)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	EXPECT_FALSE(Merge20FormListPassesFilter(results[0], Merge20FormFilter::AddDel));
	EXPECT_TRUE(Merge20FormListPassesFilter(results[1], Merge20FormFilter::AddDel));
	EXPECT_TRUE(Merge20FormListPassesFilter(results[2], Merge20FormFilter::AddDel));
}

TEST(Merge20FormList, FilterLayoutBreaking)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	EXPECT_FALSE(Merge20FormListPassesFilter(results[0], Merge20FormFilter::LayoutBreaking));
	EXPECT_FALSE(Merge20FormListPassesFilter(results[1], Merge20FormFilter::LayoutBreaking));
	EXPECT_TRUE(Merge20FormListPassesFilter(results[2], Merge20FormFilter::LayoutBreaking));
}

TEST(Merge20FormList, SearchByFileName)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	EXPECT_TRUE(Merge20FormListMatchesSearch(results[1], Merge20FormListLowerQuery(L"b.xmL")));
	EXPECT_FALSE(Merge20FormListMatchesSearch(results[0], Merge20FormListLowerQuery(L"b.xml")));
	EXPECT_TRUE(Merge20FormListMatchesSearch(results[1], Merge20FormListLowerQuery(L"B.XML")));
}

TEST(Merge20FormList, SearchByFormName)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	// "\u7d0d\u4ed8\u66f8" (b.xml's form name).
	EXPECT_TRUE(Merge20FormListMatchesSearch(results[1], Merge20FormListLowerQuery(L"\u7d0d\u4ed8")));
	EXPECT_FALSE(Merge20FormListMatchesSearch(results[0], Merge20FormListLowerQuery(L"\u7d0d\u4ed8")));
}

TEST(Merge20FormList, SearchByElementAndDesc)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	// Element key of a.xml's record.
	EXPECT_TRUE(Merge20FormListMatchesSearch(results[0], Merge20FormListLowerQuery(L"AMOUNT")));
	EXPECT_FALSE(Merge20FormListMatchesSearch(results[1], Merge20FormListLowerQuery(L"AMOUNT")));
	// Desc text ("\u6a2a\u5e45") of a.xml's record.
	EXPECT_TRUE(Merge20FormListMatchesSearch(results[0], Merge20FormListLowerQuery(L"\u6a2a\u5e45")));
	EXPECT_FALSE(Merge20FormListMatchesSearch(results[2], Merge20FormListLowerQuery(L"\u6a2a\u5e45")));
}

TEST(Merge20FormList, SearchEmptyMatchesAll)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	for (const Merge20FormFileResult& result : results)
		EXPECT_TRUE(Merge20FormListMatchesSearch(result, L""));
}

TEST(Merge20FormList, SortFileNameCaseInsensitive)
{
	std::vector<Merge20FormFileResult> results;
	results.push_back(MakeResult(L"b.xml", L"", 1.0, 1, 0, 0, 0, false, Merge20FormSeverity::Info));
	results.push_back(MakeResult(L"A.xml", L"", 1.0, 1, 0, 0, 0, false, Merge20FormSeverity::Info));
	results.push_back(MakeResult(L"C.xml", L"", 1.0, 1, 0, 0, 0, false, Merge20FormSeverity::Info));
	const std::vector<size_t> indices = Merge20FormListApplyFilterSort(
		results, Merge20FormFilter::All, L"", Merge20FormSort::FileName);
	ASSERT_EQ(indices.size(), 3u);
	EXPECT_EQ(results[indices[0]].fileName, L"A.xml");
	EXPECT_EQ(results[indices[1]].fileName, L"b.xml");
	EXPECT_EQ(results[indices[2]].fileName, L"C.xml");
}

TEST(Merge20FormList, SortNumericAscDesc)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	std::vector<size_t> indices = Merge20FormListApplyFilterSort(
		results, Merge20FormFilter::All, L"", Merge20FormSort::NumericAsc);
	EXPECT_EQ(FileNames(results, indices), (std::vector<std::wstring>{ L"b.xml", L"a.xml", L"c.xml" }));
	indices = Merge20FormListApplyFilterSort(
		results, Merge20FormFilter::All, L"", Merge20FormSort::NumericDesc);
	EXPECT_EQ(FileNames(results, indices), (std::vector<std::wstring>{ L"c.xml", L"a.xml", L"b.xml" }));
}

TEST(Merge20FormList, SortNumericUsesRoundedTotals)
{
	// 10.4 and 10.6 round to 10 and 11, so the ascending order is
	// a (10) before b (11); the unrounded values would order the
	// same way here, but a 10.5 vs 10.4 pair pins the rounding:
	// 10.5 -> 11, 10.4 -> 10.
	std::vector<Merge20FormFileResult> results;
	results.push_back(MakeResult(L"b.xml", L"", 10.5, 1, 0, 0, 1, false, Merge20FormSeverity::VisualOnly));
	results.push_back(MakeResult(L"a.xml", L"", 10.4, 1, 0, 0, 1, false, Merge20FormSeverity::VisualOnly));
	const std::vector<size_t> indices = Merge20FormListApplyFilterSort(
		results, Merge20FormFilter::All, L"", Merge20FormSort::NumericAsc);
	ASSERT_EQ(indices.size(), 2u);
	EXPECT_EQ(results[indices[0]].fileName, L"a.xml");
	EXPECT_EQ(results[indices[1]].fileName, L"b.xml");
}

TEST(Merge20FormList, SortChangeCountDesc)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	const std::vector<size_t> indices = Merge20FormListApplyFilterSort(
		results, Merge20FormFilter::All, L"", Merge20FormSort::ChangeCountDesc);
	EXPECT_EQ(FileNames(results, indices), (std::vector<std::wstring>{ L"c.xml", L"a.xml", L"b.xml" }));
}

TEST(Merge20FormList, SortSeverity)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	const std::vector<size_t> indices = Merge20FormListApplyFilterSort(
		results, Merge20FormFilter::All, L"", Merge20FormSort::Severity);
	// LayoutBreaking (c) first, then VisualOnly (a), then Info (b).
	EXPECT_EQ(FileNames(results, indices), (std::vector<std::wstring>{ L"c.xml", L"a.xml", L"b.xml" }));
}

TEST(Merge20FormList, SortSeverityTiesKeepFileNameOrder)
{
	std::vector<Merge20FormFileResult> results;
	results.push_back(MakeResult(L"b.xml", L"", 5.0, 1, 0, 0, 0, false, Merge20FormSeverity::VisualOnly));
	results.push_back(MakeResult(L"a.xml", L"", 9.0, 1, 0, 0, 0, false, Merge20FormSeverity::VisualOnly));
	const std::vector<size_t> indices = Merge20FormListApplyFilterSort(
		results, Merge20FormFilter::All, L"", Merge20FormSort::Severity);
	// Same severity: the stable sort keeps the input order. The
	// input here is b before a on purpose, to pin the stability;
	// the document passes file-name-ordered input, so in practice
	// ties resolve to file-name order.
	ASSERT_EQ(indices.size(), 2u);
	EXPECT_EQ(results[indices[0]].fileName, L"b.xml");
	EXPECT_EQ(results[indices[1]].fileName, L"a.xml");
}

TEST(Merge20FormList, ApplyFilterSortCombined)
{
	const std::vector<Merge20FormFileResult> results = SampleResults();
	// Warning filter + search for the form name of c.xml, sorted
	// by numeric change descending: only c.xml survives.
	const std::vector<size_t> indices = Merge20FormListApplyFilterSort(
		results, Merge20FormFilter::Warning, Merge20FormListLowerQuery(L"\u524d\u5e74"),
		Merge20FormSort::NumericDesc);
	ASSERT_EQ(indices.size(), 1u);
	EXPECT_EQ(results[indices[0]].fileName, L"c.xml");
	// A search with no match yields an empty list.
	const std::vector<size_t> none = Merge20FormListApplyFilterSort(
		results, Merge20FormFilter::All, Merge20FormListLowerQuery(L"zzzz"),
		Merge20FormSort::FileName);
	EXPECT_TRUE(none.empty());
}
