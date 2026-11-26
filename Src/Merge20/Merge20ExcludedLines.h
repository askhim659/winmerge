/**
 * @file  Merge20ExcludedLines.h
 *
 * @brief Count the lines a prediffer blanked (Merge20 S0-F1,
 * coverage matrix S0-F1/H-08/H-28).
 *
 * The folder-compare "Ignored diffs" column (regName `Sexcluded`)
 * used to show DIFFITEM::nidiffs, i.e. a count of trivial
 * (ignored) diffs, not a line count (S0 plan 2, MERGE20_NOTES.md
 * section 8.2). The true excluded-line count is the number of
 * lines the prediffer turned blank before diffing: the
 * Merge20Exclude prediffer (Plugins/dlls/Merge20Exclude.sct)
 * replaces every excluded line by an empty line and keeps the
 * line count, so a line that was non-blank before the prediffer
 * and blank afterwards is exactly one excluded line (the HTML
 * tool counts the same rows as `f.ex`).
 *
 * This module is deliberately free of MFC and of the translation
 * macro `_()`: the same code is compiled into WinMergeU and into
 * the GoogleTest runner. It stays ASCII only.
 */
#pragma once

#include <string_view>

/**
 * @brief Count lines blanked between two texts.
 * @param [in] before Text before the prediffer ran.
 * @param [in] after Text after the prediffer ran.
 * @return Number of line positions where `before` has a
 * non-blank line (more than whitespace) and `after` has a
 * blank (empty or whitespace-only) line, or no line at all
 * (the prediffer deleted lines). Lines that were already
 * blank before the prediffer are never counted, and lines
 * the prediffer added (more lines after than before) are
 * never counted either: only lines that disappeared from
 * the diff because the prediffer blanked them count.
 * Line breaks are CRLF, LF or CR. A trailing line break
 * does not create a phantom extra line.
 */
int Merge20CountExcludedLines(std::wstring_view before, std::wstring_view after);
