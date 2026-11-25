// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20CsvExport.h
 *
 * @brief Diff-list CSV export: range selection and BOM-prefixed UTF-8
 * file writing (Merge20 Phase 2 S6, design section 3.5, spec section
 * 4(f)).
 *
 * The 13 fixed columns and their order are the design section 3.5
 * list (directory, file name, full path, existence category, action
 * category, risk, review state, total diffs, missing, modified,
 * added, summary, memo). The values are the same strings the folder
 * compare list shows (see Merge20CsvGet*Text in DirViewColItems.h),
 * so the CSV and the list never disagree; the counts and the memo
 * come from the CDiffContext sidecar map.
 *
 * The existing DirCmpReport is deliberately not reused for the column
 * part: it emits the currently displayed columns (in display order)
 * plus a title line and a time line, and it quotes a value only when
 * it contains the separator without escaping embedded quotes, so it
 * cannot produce the fixed 13-column audit format and is not
 * Excel-safe for memo/summary text. Only its file creation
 * (UniStdioFile::OpenCreateUtf8) is reused, plus the WriteBom call
 * the design section 3.5 identifies as missing there. DirCmpReport
 * itself is untouched, so the existing report behaviour does not
 * change (spec section 7 compatibility).
 */
#pragma once

#include "UnicodeString.h"

class CDiffContext;
class DIFFITEM;

/**
 * @brief Export range for the Merge20 diff-list CSV, as in the HTML
 * tool's `dlCsv(onlyChk)` (17_v17.15.html).
 */
enum class Merge20CsvExportRange
{
	SelectedOnly, /**< Files with the output checkbox on (HTML f.chk). */
	AllDiffs      /**< Files with differences (HTML f.tot > 0). */
};

/**
 * @brief True when the file has differences: changed, or unique on
 * any side. Identical, skipped, error and aborted files return false
 * (the HTML tool's `tot > 0` condition).
 * @param [in] di Compared item (files only; directories are never passed here).
 * @return True when the file belongs to the "all diffs" range.
 */
bool Merge20CsvHasDiff(const DIFFITEM& di);

/**
 * @brief Count the files in each export range over the whole
 * comparison (a tree walk, so files hidden by the current filter or
 * inside a collapsed folder are included too, as in the HTML tool's
 * dlCsv which iterates all files, not the filtered view).
 * Directory rows are not counted.
 * @param [in] ctxt Compare context.
 * @param [out] selectedOnly Files with the output checkbox on.
 * @param [out] allDiffs Files with differences.
 */
void Merge20CsvCountRanges(const CDiffContext& ctxt, int& selectedOnly, int& allDiffs);

/**
 * @brief Write the fixed 13-column diff-list CSV for the given range.
 * The file is written as BOM-prefixed UTF-8 with CRLF line endings
 * (HTML dlCsv), so it opens directly in Excel without garbled
 * Japanese. The header line is the 13 column names (translated).
 * @param [in] ctxt Compare context.
 * @param [in] range Export range (selected only / all diffs).
 * @param [in] filePath Destination file (selected with the save dialog).
 * @param [out] errStr Empty on success, otherwise the error message.
 * @param [out] exportedCount Number of rows written (excluding the header).
 * @return True on success.
 */
bool Merge20CsvExportToFile(CDiffContext& ctxt, Merge20CsvExportRange range, const String& filePath, String& errStr, int& exportedCount);
