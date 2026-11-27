// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20VerifyReport.h
 *
 * @brief Verification report (CSV) builder (Merge20
 * best-workflow B8, gap analysis section 4.2 B8).
 *
 * Turns B7 verification results into the report that is saved
 * next to the merge output (and, on demand, on its own): the
 * verification date/time, the number of files covered, each
 * check's result, and the list of problems (when any). The
 * format follows the S6 CSV (BOM-prefixed UTF-8 with CRLF line
 * endings, text fields always double-quoted with embedded quotes
 * doubled, counts as bare numbers, so it opens directly in
 * Excel); the BOM itself is written by the caller
 * (Merge20VerifyReportWriteFile), the same UniStdioFile::WriteBom
 * call S6 uses. This builder returns the CSV text only (no BOM,
 * CRLF line endings, trailing CRLF after the last line, as in
 * the S6 files).
 *
 * This module is pure (no MFC, no `_()`): the display strings
 * (title, summary labels, section titles, column headers,
 * Passed/Failed words, check names) are supplied by the caller
 * through Merge20VerifyReportLabels, composed with `_()` there,
 * so this file stays ASCII and every display string goes
 * through Translations/WinMerge/Japanese.po (the S4/B7
 * precedent: data in, formatted text out). The issue detail
 * text is composed by the caller too
 * (Merge20VerifyFormatIssueDetail), so the report and the B7
 * problem-list dialog never disagree. The same code is compiled
 * into WinMergeU and into the GoogleTest runner.
 *
 * Layout (sections separated by one blank line):
 * - Title line (one quoted field).
 * - Summary: verification date/time (quoted), files covered,
 *   files passed, files failed, total issues (bare numbers).
 *   Only verified files count as passed/failed; a file whose
 *   merge result was never built counts in "files covered" and
 *   gets a "not verified" row in the check table instead.
 * - Check table: one row per (file, check) -- file name, check
 *   name, result (Passed / Failed / Not run when the check is
 *   switched off), issue count, elapsed ms. An unverified file
 *   gets a single row with the "not verified" text in the check
 *   column and empty result/count/elapsed cells.
 * - Issue list: one row per problem -- file name, check name,
 *   diff number (1-based, empty when the problem is not tied to
 *   a diff), result line (1-based, empty when not applicable),
 *   detail text. When there are no problems the header stands
 *   alone; the summary's "total issues: 0" states it.
 *
 * This report is evidence that one output was verified; it is
 * NOT the B11 audit log (the audit log is the history of
 * judgments, this report is the correctness of one output).
 */
#pragma once

#include <string>
#include <vector>
#include "Merge20/Merge20Verify.h"
#include "Merge20Types.h"

/**
 * @brief One problem row of the report: display strings already
 * composed by the caller (via `_()`), so the report matches the
 * B7 problem-list dialog.
 */
struct Merge20VerifyReportIssue
{
	String fileName;    /**< Display name of the file */
	String checkName;   /**< Translated check name */
	int diffIndex = -1; /**< Diff index (0-based), or -1 when not tied to a diff */
	int resultLine = -1; /**< 1-based result line, or -1 when not applicable */
	String detail;      /**< Translated detail text (includes the offending line, if any) */
};

/**
 * @brief One file's verification outcome, as the report needs it.
 */
struct Merge20VerifyReportFile
{
	String fileName;    /**< Display name of the file */
	bool verified = true; /**< False when no merge result was built: nothing was verified */
	bool passed = true; /**< True when no enabled check found a problem */
	/** @brief One entry per check, in Merge20VerifyCheck order (engine structs, reused as-is). */
	std::vector<Merge20VerifyCheckResult> checks;
	std::vector<Merge20VerifyReportIssue> issues; /**< All problems of the file */
};

/**
 * @brief All display strings of the report, composed by the
 * caller with `_()`. checkNames is indexed by the
 * Merge20VerifyCheck ordinal (Rediff, LineCountEncoding,
 * Syntax, Consistency, in that order).
 */
struct Merge20VerifyReportLabels
{
	String title;            /**< Report title line */
	String verifiedAt;       /**< "Verification date/time" summary label */
	String fileCount;        /**< "Files covered" summary label */
	String passedFiles;      /**< "Files passed" summary label */
	String failedFiles;      /**< "Files failed" summary label */
	String issueCount;       /**< "Total issues" summary label */
	String checksTitle;      /**< Check-table section title */
	String issuesTitle;      /**< Issue-list section title */
	String headerFile;       /**< Check/issue table: file-name column */
	String headerCheck;      /**< Check/issue table: check column */
	String headerResult;     /**< Check table: result column */
	String headerIssues;     /**< Check table: issue-count column */
	String headerElapsed;    /**< Check table: elapsed-ms column */
	String headerDiff;       /**< Issue table: diff-number column */
	String headerResultLine; /**< Issue table: result-line column */
	String headerDetail;     /**< Issue table: detail column */
	String passed;           /**< Check result: passed */
	String failed;           /**< Check result: failed */
	String notRun;           /**< Check result: switched off */
	String notVerified;      /**< Unverified file's check-table row */
	String checkNames[4];    /**< Check names, Merge20VerifyCheck order */
};

/**
 * @brief One report: when it was verified, and the files it covers.
 */
struct Merge20VerifyReport
{
	/** @brief Caller-formatted local date/time (e.g. "2026-11-27 21:05:33"). */
	String verifiedAt;
	std::vector<Merge20VerifyReportFile> files;
};

/**
 * @brief Build the report CSV text (CRLF line endings, trailing
 * CRLF after the last line, no BOM: the caller writes the BOM).
 * @param [in] report When the verification ran, and the files it covers.
 * @param [in] labels Display strings (see Merge20VerifyReportLabels).
 * @return The CSV text.
 */
String Merge20VerifyReportBuildCsv(const Merge20VerifyReport& report,
	const Merge20VerifyReportLabels& labels);
