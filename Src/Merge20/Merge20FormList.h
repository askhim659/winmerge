// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormList.h
 *
 * @brief FORM/XML comparison (Wave 5 F2): KPI dashboard, filter
 * tabs, search and sort over FORM comparison results (design
 * merge20-form-compare-design.md sections 2.4, 6 (4), 7.1 and
 * 10 F2).
 *
 * Pure module (no MFC, no `_()`): the same code is compiled into
 * WinMergeU and into the GoogleTest runner. It works on the F0 data
 * model (Merge20FormFileResult); the F1 classification (`isWarning`
 * / `severity`, set by Merge20ApplyLargeChangeFlag) must have run
 * before the KPI is computed.
 *
 * The definitions are the HTML tool's (`FROM_____1.html`,
 * `updateKpiBadges` / `applyFiltersAndRender`):
 * - total: all .xml files in the union of the two folders;
 *   mismatch: changed files; matched: total - mismatch;
 *   avg: round(sum of the per-file rounded totalNumericSum /
 *   changed files), 0 when nothing changed;
 *   warning: files with isWarning; coord: files with
 *   coordCount > 0; addDel: files with addCount > 0 or
 *   delCount > 0.
 * - Filters: all / warning (isWarning) / coord (coordCount > 0) /
 *   add_del (addCount > 0 or delCount > 0) / layout-breaking
 *   (severity == LayoutBreaking; the fifth tab, design
 *   section 6 (4)).
 * - Search: case-insensitive substring over the file name, the
 *   form name, and every diff record's element / desc.
 * - Sort: file name (case-insensitive), numeric change ascending /
 *   descending, change count descending, severity
 *   (LayoutBreaking, VisualOnly, Info). Ties keep the input
 *   order (stable sort); the document stores results in
 *   file-name order, so a tie falls back to file-name order
 *   like the HTML tool.
 */
#pragma once

#include <string>
#include <vector>
#include "Merge20/Merge20FormXml.h"

/** @brief Filter tabs of the FORM compare list (design section 7.1). */
enum class Merge20FormFilter
{
	All, /**< All changed files. */
	Warning, /**< Large changes only (isWarning). */
	Coord, /**< Files with coordinate/size changes. */
	AddDel, /**< Files with element/file additions or deletions. */
	LayoutBreaking, /**< Layout-breaking severity only (fifth tab). */
};

/** @brief Sort orders of the FORM compare list (design section 7.1). */
enum class Merge20FormSort
{
	FileName, /**< File name, ascending. */
	NumericAsc, /**< Numeric change total, ascending. */
	NumericDesc, /**< Numeric change total, descending. */
	ChangeCountDesc, /**< Change count, descending. */
	Severity, /**< Severity, layout-breaking first. */
};

/** @brief The seven KPI numbers (HTML `updateKpiBadges`). */
struct Merge20FormKpi
{
	int total = 0; /**< All .xml files in the two folders. */
	int mismatch = 0; /**< Changed files (listed). */
	int matched = 0; /**< Identical files (not listed). */
	int avgNumeric = 0; /**< Average numeric change (rounded, HTML rule). */
	int warning = 0; /**< Large-change files. */
	int coord = 0; /**< Files with coordinate changes. */
	int addDel = 0; /**< Files with element/file additions or deletions. */
};

/**
 * @brief Compute the seven KPI numbers (HTML `updateKpiBadges`).
 * @param [in] totalFiles All .xml files scanned (both folders' union).
 * @param [in] changedResults Changed files only, in file-name order
 *             (identical files are NOT in this list; the document
 *             keeps them out, design section 2.1). `isWarning` /
 *             `severity` must already be set
 *             (Merge20ApplyLargeChangeFlag).
 * @return The KPI numbers. matched = totalFiles - changedResults.size().
 */
Merge20FormKpi Merge20FormListComputeKpi(int totalFiles,
	const std::vector<Merge20FormFileResult>& changedResults);

/**
 * @brief True when one file passes the selected filter tab.
 * @param [in] result Per-file result.
 * @param [in] filter Selected filter tab.
 */
bool Merge20FormListPassesFilter(const Merge20FormFileResult& result, Merge20FormFilter filter);

/**
 * @brief Case-insensitive substring test (Grep precedent: towlower
 * comparison, no lowered copy). `queryLower` must already be
 * lowered by the caller. An empty query matches everything.
 * @param [in] text Text to search in.
 * @param [in] queryLower Lowered query.
 */
bool Merge20FormListContainsCaseInsensitive(const std::wstring& text, const std::wstring& queryLower);

/**
 * @brief True when one file matches the search query (HTML
 * `applyFiltersAndRender`): the file name, the form name, or any
 * diff record's element / desc contains the query
 * (case-insensitive). An empty query matches everything.
 * @param [in] result Per-file result.
 * @param [in] queryLower Search text, already lowered and trimmed
 *             by the caller.
 */
bool Merge20FormListMatchesSearch(const Merge20FormFileResult& result, const std::wstring& queryLower);

/**
 * @brief Lower-case a search query (towlower loop, Grep precedent).
 * The document lowercases the search text once with this and
 * passes the result to Merge20FormListMatchesSearch /
 * Merge20FormListApplyFilterSort, so the view and the tests share
 * the same lowering rule.
 * @param [in] query Raw search text.
 * @return Lower-cased query.
 */
std::wstring Merge20FormListLowerQuery(const std::wstring& query);

/**
 * @brief Compare two files for the sort order (< 0 when `a` sorts
 * before `b`, 0 on a tie, > 0 otherwise). Numeric totals are
 * compared rounded, like the HTML tool (which rounds the
 * per-file total before sorting). Severity ranks
 * LayoutBreaking (0), VisualOnly (1), Info (2). The file-name
 * comparison is case-insensitive.
 * @param [in] a First file.
 * @param [in] b Second file.
 * @param [in] sort Selected sort order.
 */
int Merge20FormListCompare(const Merge20FormFileResult& a, const Merge20FormFileResult& b, Merge20FormSort sort);

/**
 * @brief Filter + search + sort one result list (HTML
 * `applyFiltersAndRender`).
 * @param [in] results Changed files, in file-name order.
 * @param [in] filter Selected filter tab.
 * @param [in] queryLower Search text, already lowered and trimmed.
 * @param [in] sort Selected sort order.
 * @return Indices into `results`, in display order. The sort is
 * stable, so ties keep file-name order.
 */
std::vector<size_t> Merge20FormListApplyFilterSort(const std::vector<Merge20FormFileResult>& results,
	Merge20FormFilter filter, const std::wstring& queryLower, Merge20FormSort sort);
