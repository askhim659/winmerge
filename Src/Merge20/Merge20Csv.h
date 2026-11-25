// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Csv.h
 *
 * @brief Fixed 13-column diff-list CSV: field escaping and row formatting
 * (Merge20 Phase 2 S6, design section 3.5, spec section 4(f)).
 *
 * This module is a port of the HTML tool's `dlCsv` (17_v17.15.html):
 * BOM-prefixed UTF-8, CRLF line endings, text fields always
 * double-quoted with embedded quotes doubled (""), counts written as
 * bare numbers. It is deliberately free of MFC and of the translation
 * macro `_()`: the same code is compiled into WinMergeU and into the
 * GoogleTest runner. The header line (column names) is composed by
 * the caller (Merge20CsvExport), where `_()` is available, and the
 * file itself is written there with UniStdioFile (OpenCreateUtf8 +
 * WriteBom), the same file creation DirCmpReport::GenerateReportToFile
 * uses.
 *
 * Column order (design section 3.5): directory, file name, full path,
 * existence category, action category, risk, review state, total diffs,
 * missing, modified, added, summary, memo.
 */
#pragma once

#include "Merge20Types.h"

/** @brief Number of columns in the Merge20 diff-list CSV. */
constexpr int kMerge20CsvColumnCount = 13;

/**
 * @brief One row of the Merge20 diff-list CSV (HTML dlCsv row).
 *
 * The four counts are the HTML tool's tot / dc / mc / acnt: the total
 * is the sum of missing + modified + added lines.
 */
struct Merge20CsvRow
{
	String dir;        /**< Relative directory (HTML f.dir). */
	String fileName;   /**< File name (HTML f.fn). */
	String fullPath;   /**< Relative path (HTML f.path). */
	String status;     /**< Existence category (HTML f.st). */
	String action;     /**< Action category (HTML f.sl). */
	String risk;       /**< Risk level (new in S6; the HTML CSV has no risk column). */
	String review;     /**< Review state, short form (HTML REV label). */
	int diffTotal = 0; /**< Total diff lines (HTML f.tot). */
	int missing = 0;   /**< Missing-in-new lines (HTML f.dc). */
	int modified = 0;  /**< Changed lines (HTML f.mc). */
	int added = 0;     /**< Newly added lines (HTML f.acnt). */
	String summary;    /**< Rule-based summary (new in S6; the HTML CSV has none). */
	String memo;       /**< Audit memo (HTML f.memo). */
};

/**
 * @brief Escape one text field for the CSV: always wrap it in double
 * quotes, doubling any embedded double quotes (the HTML dlCsv rule).
 * Newlines and commas inside the value are kept as-is; the quotes
 * make them safe for Excel.
 * @param [in] value Field text.
 * @return Quoted field, e.g. `"a ""b"" c"`.
 */
String Merge20CsvEscapeField(const String& value);

/**
 * @brief Format one row as a CSV line (no line ending; the caller
 * joins lines with CRLF, as in the HTML tool's `rs.join('\r\n')`).
 * Text fields are quoted, the four counts are bare numbers.
 * @param [in] row Row values.
 * @return One CSV line with exactly 13 fields.
 */
String Merge20CsvFormatRow(const Merge20CsvRow& row);
