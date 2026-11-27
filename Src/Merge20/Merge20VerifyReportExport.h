// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20VerifyReportExport.h
 *
 * @brief Verification report export (Merge20 best-workflow B8,
 * gap analysis section 4.2 B8).
 *
 * MFC adapter around the pure report builder
 * (Merge20VerifyReport): it composes the translated labels,
 * converts B7 gate results into report rows, and writes the
 * file as BOM-prefixed UTF-8 with CRLF line endings (the S6
 * pattern, so it opens directly in Excel).
 *
 * Two call sites:
 * - SaveMergeResult (via Merge20VerifySaveReportAfterOutput):
 *   after a merge result is saved, the report for that save is
 *   written next to the output file, so the evidence travels
 *   with the output. The report comes from the verification
 *   that already ran in the output gate (no second run).
 * - The folder compare context menu (standalone export):
 *   verifies the open file comparisons of this folder compare
 *   and writes one combined report to a file the user picks.
 *   S7 does not exist yet, so the report is NOT bundled into
 *   the S7 ZIP: that link is left for the S7 slice (see
 *   Merge20VerifySaveReportAfterOutput and the PR text).
 */
#pragma once

#include <vector>
#include "Merge20/Merge20Verify.h"
#include "Merge20/Merge20VerifyGate.h"
#include "Merge20/Merge20VerifyReport.h"

class CMergeDoc;

/**
 * @brief Compose the report labels (translated with `_()`).
 * Check names come from Merge20VerifyCheckDisplayName, so the
 * report and the B7 problem-list dialog use the same words.
 */
Merge20VerifyReportLabels Merge20VerifyReportMakeLabels();

/**
 * @brief Convert gate file results into a report. Check names
 * and issue details are composed with `_()` (the shared gate
 * helpers), so the report matches the B7 dialog. Files the
 * gate could not verify (no merge result built) are kept with
 * verified=false and appear as "not verified" rows.
 * @param [in] files One gate result per file (S7 order: folder-list index).
 * @param [in] verifiedAt Caller-formatted local date/time.
 */
Merge20VerifyReport Merge20VerifyReportFromFileResults(
	const std::vector<Merge20VerifyFileResult>& files, const String& verifiedAt);

/**
 * @brief Write the report as BOM-prefixed UTF-8 with CRLF line
 * endings (the S6 file creation: UniStdioFile::OpenCreateUtf8 +
 * WriteBom, so it opens directly in Excel without garbled
 * Japanese).
 * @param [out] errStr Empty on success, otherwise the error message.
 * @return True on success.
 */
bool Merge20VerifyReportWriteFile(const Merge20VerifyReport& report,
	const Merge20VerifyReportLabels& labels, const String& filePath, String& errStr);

/**
 * @brief Default report file name for a save: VerifyReport_YYYY-MM-DD.csv.
 */
String Merge20VerifyReportDefaultFileName(const CTime& now);

/**
 * @brief A non-colliding path inside directory: baseName first,
 * then VerifyReport_YYYY-MM-DD_HHMMSS.csv, then _2, _3 and so
 * on (up to 999). An existing report is never overwritten: a
 * same-day repeat save gets the time suffix instead. The final
 * fallback after 999 collisions is returned as-is (unreachable
 * in practice: it would need 999 reports in one second).
 */
String Merge20VerifyReportUniquePath(const String& directory, const String& baseName);

/**
 * @brief SaveMergeResult hook: write the report next to the
 * merge output (same folder), from the gate result of this
 * save. Nothing is written when nothing was verified (no merge
 * result built: verified=false); that is not an error.
 * @param [in] lastResult Gate result of the save (from Merge20VerifyBeforeOutput).
 * @param [in] outputPath Path the merge result was saved to.
 * @param [out] reportPath Path the report was written to (empty when skipped).
 * @param [out] errStr Empty on success, otherwise the error message.
 * @return True when the report was written or skipped; false
 *         only when the report itself could not be written. The
 *         merge result is already saved by then, so the caller
 *         only warns and never fails the save.
 */
bool Merge20VerifySaveReportAfterOutput(const Merge20VerifyFileResult& lastResult,
	const String& outputPath, String& reportPath, String& errStr);
