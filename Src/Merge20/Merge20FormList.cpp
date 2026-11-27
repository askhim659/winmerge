// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormList.cpp
 *
 * @brief Implementation of the FORM compare list helpers (Wave 5
 * F2): KPI dashboard, filter tabs, search and sort (design
 * merge20-form-compare-design.md sections 2.4, 6 (4), 7.1 and
 * 10 F2). Pure module: no MFC, no `_()`, ASCII only.
 */
#include "pch.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include "Merge20/Merge20FormList.h"

namespace
{

	// Severity rank for the severity sort (design section 5.4):
	// layout-breaking first, then visual-only, then informational.
	int FormSeverityRank(Merge20FormSeverity severity)
	{
		switch (severity)
		{
		case Merge20FormSeverity::LayoutBreaking:
			return 0;
		case Merge20FormSeverity::VisualOnly:
			return 1;
		case Merge20FormSeverity::Info:
		default:
			return 2;
		}
	}

	// Case-insensitive comparison of two file names (towlower
	// loop, Grep precedent; only ASCII case folds in practice).
	// Returns < 0 / 0 / > 0 like wcscmp.
	int FormCompareFileName(const std::wstring& a, const std::wstring& b)
	{
		const size_t len = (std::min)(a.size(), b.size());
		for (size_t i = 0; i < len; ++i)
		{
			const wchar_t ca = std::towlower(a[i]);
			const wchar_t cb = std::towlower(b[i]);
			if (ca != cb)
				return ca < cb ? -1 : 1;
		}
		if (a.size() == b.size())
			return 0;
		return a.size() < b.size() ? -1 : 1;
	}

	// Lower-case copy of a string (the search query is lowered once
	// by the caller; this is only used for the query itself).
	std::wstring FormToLower(const std::wstring& text)
	{
		std::wstring out(text);
		for (wchar_t& ch : out)
			ch = std::towlower(ch);
		return out;
	}

} // namespace

Merge20FormKpi Merge20FormListComputeKpi(int totalFiles,
	const std::vector<Merge20FormFileResult>& changedResults)
{
	Merge20FormKpi kpi;
	kpi.total = totalFiles;
	kpi.mismatch = static_cast<int>(changedResults.size());
	kpi.matched = totalFiles - kpi.mismatch;
	if (kpi.matched < 0)
		kpi.matched = 0;
	// HTML `updateKpiBadges`: avg = round(sum of the per-file
	// rounded totals / changed files), 0 when nothing changed.
	// The same rounded average the F1 large-change flag uses
	// (Merge20ApplyLargeChangeFlag).
	double sum = 0.0;
	for (const Merge20FormFileResult& result : changedResults)
	{
		sum += std::round(result.totalNumericSum);
		if (result.isWarning)
			++kpi.warning;
		if (result.coordCount > 0)
			++kpi.coord;
		if (result.addCount > 0 || result.delCount > 0)
			++kpi.addDel;
	}
	if (kpi.mismatch > 0)
		kpi.avgNumeric = static_cast<int>(std::round(sum / kpi.mismatch));
	return kpi;
}

bool Merge20FormListPassesFilter(const Merge20FormFileResult& result, Merge20FormFilter filter)
{
	switch (filter)
	{
	case Merge20FormFilter::Warning:
		return result.isWarning;
	case Merge20FormFilter::Coord:
		return result.coordCount > 0;
	case Merge20FormFilter::AddDel:
		return result.addCount > 0 || result.delCount > 0;
	case Merge20FormFilter::LayoutBreaking:
		return result.severity == Merge20FormSeverity::LayoutBreaking;
	case Merge20FormFilter::All:
	default:
		return true;
	}
}

bool Merge20FormListContainsCaseInsensitive(const std::wstring& text, const std::wstring& queryLower)
{
	if (queryLower.empty() || text.size() < queryLower.size())
		return queryLower.empty();
	// No lowered copy of the text: compare character by character
	// with towlower (Merge20GrepFindInLine precedent).
	const size_t last = text.size() - queryLower.size();
	for (size_t i = 0; i <= last; ++i)
	{
		size_t j = 0;
		for (; j < queryLower.size(); ++j)
		{
			if (std::towlower(text[i + j]) != queryLower[j])
				break;
		}
		if (j == queryLower.size())
			return true;
	}
	return false;
}

bool Merge20FormListMatchesSearch(const Merge20FormFileResult& result, const std::wstring& queryLower)
{
	if (queryLower.empty())
		return true;
	if (Merge20FormListContainsCaseInsensitive(result.fileName, queryLower))
		return true;
	if (Merge20FormListContainsCaseInsensitive(result.formName, queryLower))
		return true;
	for (const Merge20FormDiffRecord& record : result.diffRecords)
	{
		if (Merge20FormListContainsCaseInsensitive(record.element, queryLower))
			return true;
		if (Merge20FormListContainsCaseInsensitive(record.desc, queryLower))
			return true;
	}
	return false;
}

int Merge20FormListCompare(const Merge20FormFileResult& a, const Merge20FormFileResult& b, Merge20FormSort sort)
{
	switch (sort)
	{
	case Merge20FormSort::NumericAsc:
	case Merge20FormSort::NumericDesc:
	{
		// The HTML tool rounds the per-file total before sorting.
		const double ta = std::round(a.totalNumericSum);
		const double tb = std::round(b.totalNumericSum);
		if (ta == tb)
			return 0;
		const bool aFirst = (sort == Merge20FormSort::NumericAsc) ? (ta < tb) : (ta > tb);
		return aFirst ? -1 : 1;
	}
	case Merge20FormSort::ChangeCountDesc:
		if (a.changeCount == b.changeCount)
			return 0;
		return a.changeCount > b.changeCount ? -1 : 1;
	case Merge20FormSort::Severity:
	{
		const int ra = FormSeverityRank(a.severity);
		const int rb = FormSeverityRank(b.severity);
		if (ra == rb)
			return 0;
		return ra < rb ? -1 : 1;
	}
	case Merge20FormSort::FileName:
	default:
		return FormCompareFileName(a.fileName, b.fileName);
	}
}

std::vector<size_t> Merge20FormListApplyFilterSort(const std::vector<Merge20FormFileResult>& results,
	Merge20FormFilter filter, const std::wstring& queryLower, Merge20FormSort sort)
{
	std::vector<size_t> indices;
	for (size_t i = 0; i < results.size(); ++i)
	{
		if (!Merge20FormListPassesFilter(results[i], filter))
			continue;
		if (!Merge20FormListMatchesSearch(results[i], queryLower))
			continue;
		indices.push_back(i);
	}
	// Stable sort: ties keep the input (file-name) order, like the
	// HTML tool's stable Array.sort over file-name-ordered results.
	std::stable_sort(indices.begin(), indices.end(),
		[&results, sort](size_t ia, size_t ib)
		{
			return Merge20FormListCompare(results[ia], results[ib], sort) < 0;
		});
	return indices;
}

// Lower-case helper kept for callers that pass a raw query: the
// document lowercases the search text once and passes it to
// Merge20FormListMatchesSearch / Merge20FormListApplyFilterSort.
// (Kept as a free function so the view and the tests share the
// same lowering rule.)
std::wstring Merge20FormListLowerQuery(const std::wstring& query)
{
	return FormToLower(query);
}
