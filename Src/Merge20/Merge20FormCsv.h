// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCsv.h
 *
 * @brief FORM/XML comparison (Wave 5 F7): CSV/Excel export row
 * building (design merge20-form-compare-design.md sections 9
 * and 10 F7).
 *
 * Pure module (no MFC, no `_()`): the same code is compiled into
 * WinMergeU and into the GoogleTest runner. It builds the
 * two-part CSV text of the HTML tool's `exportToExcel`
 * (FROM_____1.html): part 1 is the per-form summary list, part 2
 * the per-change detail list with the impact analysis. The
 * quoting rule is S6's (`Merge20CsvEscapeField`: text fields are
 * always double-quoted, `"` doubled; numbers stay bare), and the
 * BOM-prefixed UTF-8 file writing is the S6
 * `Merge20CsvExport` way; only the row building is
 * FORM-specific (design section 3.5).
 *
 * Differences from the HTML tool (design section 9):
 * - Part 1 gains a severity column and a one-line summary column
 *   (design section 6 (4)).
 * - Part 2 gains the `Moved` / `Renamed` types, shown as
 *   "moved" / "renamed" (design section 4.2), a group column
 *   (the group summary, empty for a single change) and a minor
 *   column ("minor" for the section 5.2 minor group).
 * - Numbers are bare (S6 rule); the HTML tool quotes every cell.
 */
#pragma once

#include <string>
#include <vector>
#include "Merge20/Merge20FormDiff.h"

/** @brief Number of columns of the FORM CSV part 1 (summary list). */
constexpr int kMerge20FormCsvSummaryColumnCount = 9;
/** @brief Number of columns of the FORM CSV part 2 (detail list). */
constexpr int kMerge20FormCsvDetailColumnCount = 14;

/**
 * @brief Severity display word (design section 5.4): the same
 * Japanese words as the FORM compare list (`Layout-breaking` /
 * `Visual only` / `Information` in the .po).
 * @param [in] severity Severity of the file.
 */
std::wstring Merge20FormCsvSeverityText(Merge20FormSeverity severity);

/**
 * @brief Change-type display word (design section 4.2): the HTML
 * vocabulary (`FILE_ADD` / ... / `COORD` / `MOD`, see
 * Merge20FormTableTypeText), except `Moved` / `Renamed`, shown
 * as "moved" / "renamed".
 * @param [in] type Record type.
 */
std::wstring Merge20FormCsvTypeText(Merge20FormDiffType type);

/**
 * @brief Group column text of one record (design section 9): the
 * summary of the group the record belongs to, or empty when the
 * record is a single change.
 * @param [in] groups Groups of the file (F1).
 * @param [in] record Diff record.
 */
std::wstring Merge20FormCsvGroupText(
	const std::vector<Merge20FormDiffGroup>& groups,
	const Merge20FormDiffRecord& record);

/**
 * @brief True when the record is a minor difference (design
 * section 5.2): it belongs to the minor group.
 * @param [in] groups Groups of the file (F1).
 * @param [in] record Diff record.
 */
bool Merge20FormCsvIsMinor(
	const std::vector<Merge20FormDiffGroup>& groups,
	const Merge20FormDiffRecord& record);

/**
 * @brief Build the whole FORM compare CSV text (CRLF line endings;
 * no BOM here, the caller writes the BOM, S6 precedent).
 * @param [in] results Changed files, in file-name order.
 * @param [in] groups F1 groups per result, aligned with results
 *             (a shorter vector is treated as no groups).
 * @param [in] indices Results to include, in output order (the
 *             filtered view, or all results). Out-of-range
 *             indices are skipped.
 * @param [in] timestampText Output date/time text for the title
 *             line (already formatted by the caller).
 * @return The CSV text, every line terminated with CRLF.
 */
std::wstring Merge20FormCsvBuild(
	const std::vector<Merge20FormFileResult>& results,
	const std::vector<std::vector<Merge20FormDiffGroup>>& groups,
	const std::vector<size_t>& indices,
	const std::wstring& timestampText);
