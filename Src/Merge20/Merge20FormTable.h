// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormTable.h
 *
 * @brief FORM/XML comparison (Wave 5 F5): the structured diff
 * table rows (design merge20-form-compare-design.md sections
 * 2.9, 5.2, 5.3 and 10 F5).
 *
 * Pure module (no MFC, no `_()`): the same code is compiled into
 * WinMergeU and into the GoogleTest runner. The view
 * (CFormCompareView) owns the CListCtrl and calls this to build
 * the rows of the selected file; the grouping itself is F1
 * (`Merge20FormDiffGroup`, `record.groupId`), this module only
 * turns it into display rows.
 *
 * Columns (HTML `FROM_____1.html`, design section 2.9):
 * - type: the record type (`FILE_ADD` / `FILE_DEL` / `ERROR` /
 *   `ADD` / `DEL` / `COORD` / `MOD`, plus `MOVED` / `RENAMED`),
 *   or a group row (`Group` / `Minor differences`, with the
 *   expand marker).
 * - element: the display name of the element plus the changed
 *   attribute (records), or the group size (group rows).
 * - value: `old -> new`, with the signed delta in units for a
 *   numeric coordinate change (internal `double`, shown with
 *   one decimal place at most, design section 5.2); the group
 *   move (direction + amount) for a group row.
 * - desc: the change description plus the impact analysis
 *   (records), or the group summary (group rows).
 *
 * Grouping (design section 5.3): a group is one row, collapsed
 * by default (the minor-differences group too, design section
 * 5.2); expanding it shows the member records as child rows.
 * The group row sits at the position of its first member
 * record. Records without a group are single rows. A record
 * with `hasVisual == false` (file add/delete, XML error, an
 * element without a shape) is still shown as a row; the view
 * simply does not spotlight it.
 */
#pragma once

#include <set>
#include <string>
#include <vector>
#include "Merge20/Merge20FormDiff.h"

/** @brief One row of the structured diff table. */
struct Merge20FormTableRow
{
	bool isGroup = false; /**< True for a group summary row. */
	std::wstring groupId; /**< Group id (group rows and their children). */
	bool expanded = false; /**< Group rows only: whether the members are shown. */
	bool minor = false; /**< Group rows only: the minor-differences group (design section 5.2). */
	int recordIndex = -1; /**< Index into `Merge20FormFileResult::diffRecords` (-1 for group rows). */
	std::wstring typeText; /**< Column 0: type / group label. */
	std::wstring elementText; /**< Column 1: element / attribute (group size for group rows). */
	std::wstring valueText; /**< Column 2: old -> new (group move for group rows). */
	std::wstring descText; /**< Column 3: description + impact (group summary for group rows). */
};

/**
 * @brief Type code of a record (HTML vocabulary, design section
 * 2.9): `FILE_ADD` / `FILE_DEL` / `ERROR` / `ADD` / `DEL` /
 * `COORD` / `MOD` / `MOVED` / `RENAMED`.
 * @param [in] type Record type.
 */
std::wstring Merge20FormTableTypeText(Merge20FormDiffType type);

/**
 * @brief Display name of an element from its stable key
 * (`structPath|tag|name`, design section 5.1): the `name`, or
 * the tag when there is no name (or the name is an attribute
 * signature).
 * @param [in] stableKey Stable key of the element.
 */
std::wstring Merge20FormTableDisplayName(const std::wstring& stableKey);

/**
 * @brief Build the display rows of one file's diff table.
 * @param [in] result Per-file result (F1 output).
 * @param [in] groups Groups of the file (F1, aligned with the result).
 * @param [in] expandedGroupIds Group ids the user expanded
 *             (empty means every group is collapsed).
 * @return Rows in display order: a group row at its first
 * member's position, its members right after it when expanded,
 * ungrouped records in record order.
 */
std::vector<Merge20FormTableRow> Merge20BuildFormDiffTableRows(
	const Merge20FormFileResult& result,
	const std::vector<Merge20FormDiffGroup>& groups,
	const std::set<std::wstring>& expandedGroupIds);
