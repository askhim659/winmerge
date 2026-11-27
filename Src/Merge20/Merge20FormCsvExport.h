// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCsvExport.h
 *
 * @brief FORM/XML comparison (Wave 5 F7): CSV file writing
 * (design merge20-form-compare-design.md sections 9 and 10 F7).
 *
 * The row building is the pure module `Merge20FormCsv`; this
 * module only writes the built text to a file: BOM-prefixed
 * UTF-8 with CRLF line endings, the S6 `Merge20CsvExport`
 * way (UniStdioFile::OpenCreateUtf8 + WriteBom), so the CSV
 * opens directly in Excel without garbled Japanese.
 */
#pragma once

#include <vector>
#include "UnicodeString.h"
#include "Merge20/Merge20FormDiff.h"

/**
 * @brief Write the FORM compare CSV for the given files.
 * @param [in] results Changed files, in file-name order.
 * @param [in] groups F1 groups per result, aligned with results.
 * @param [in] indices Results to include, in output order.
 * @param [in] timestampText Output date/time text for the title
 *             line (already formatted by the caller).
 * @param [in] filePath Destination file (selected with the save dialog).
 * @param [out] errStr Empty on success, otherwise the error message.
 * @param [out] exportedCount Number of files written (part 1 rows).
 * @return True on success.
 */
bool Merge20FormCsvExportToFile(
	const std::vector<Merge20FormFileResult>& results,
	const std::vector<std::vector<Merge20FormDiffGroup>>& groups,
	const std::vector<size_t>& indices,
	const String& timestampText,
	const String& filePath, String& errStr, int& exportedCount);
