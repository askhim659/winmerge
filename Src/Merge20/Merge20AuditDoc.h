// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20AuditDoc.h
 *
 * @brief Adapter between the folder/file compare documents
 * (CDirDoc, CMergeDoc, CDiffContext) and the UI-independent audit
 * module (Merge20Audit).
 *
 * Merge20 (best-workflow B11, gap analysis section 4.2 B11): every
 * judgment operation appends one entry to the folder pair's audit
 * log (`Merge20Audit_<hash>.json` in the session directory, same
 * folder-pair hash as the session file). The adapter owns the
 * parts the pure module cannot: the file path (from the compare
 * context), the timestamp (local time) and the operator id (the
 * learning source id `u-<GUID>`), plus the mapping from the
 * stored codes to display text for the viewer and the CSV export.
 *
 * The hooks live next to the B10 undo records, because an audit
 * entry and an undo entry describe the same operation:
 * - review-state change: CDiffContext::ApplyMerge20ReviewState
 *   (only when bRecordUndo is true: batch check and the
 *   conclusion linkage call with false, and log their own kind
 *   or nothing);
 * - output toggle: CDiffContext::SetMerge20IncludeInOutput
 *   (only when bRecordUndo is true);
 * - diff conclusion: CMergeDoc::Merge20SetDiffConclusion (only
 *   when the conclusion changed);
 * - learning accept / dismiss: CMergeDoc::OnMerge20LearnApply /
 *   OnMerge20LearnDismiss (next to their undo entries);
 * - split / regroup: CMergeDoc::Merge20SplitDiff /
 *   Merge20RegroupDiff (only when bRecordUndo is true);
 * - batch check: the S3 dialog handlers (CDirView and the
 *   file-list panel), one entry per file whose flag changed;
 * - change reset: Merge20SessionApplyToDoc (S8 change detection).
 *
 * What is never logged here: undo / redo (the undo paths use the
 * raw setters and Merge20SetDiffConclusionRestored, none of which
 * logs) and session save / restore (acceptance (e); the restore
 * only logs the files its change detection resets).
 *
 * This header is MFC-free apart from forward declarations, so the
 * pure module stays testable; all document access lives in the .cpp.
 */
#pragma once

#include <vector>
#include "UnicodeString.h"
#include "Merge20/Merge20Audit.h"
#include "Merge20Types.h"

class CDirDoc;
class CDiffContext;

/** @brief Audit log file path for the folder pair of a compare context. */
String Merge20AuditGetFilePath(const CDiffContext& ctxt);
/** @brief Audit log file path for explicit (already normalized) folder paths. */
String Merge20AuditGetFilePath(const String& leftPath, const String& rightPath, const String& thirdPath);

/**
 * @brief Append one entry to the folder pair's audit log, filling
 * in the timestamp (local time) and the operator id (learning
 * source id `u-<GUID>`).
 * @param [in] ctxt Compare context (owns the folder pair).
 * @param [in] entry Entry to append (time and operatorId are ignored: they are filled in here).
 * @return False when the append failed (the caller ignores the
 *         failure: a judgment must never be blocked by its audit
 *         record; the viewer reports a missing/unreadable log).
 */
bool Merge20AuditAppend(CDiffContext& ctxt, const Merge20AuditEntry& entry);

/**
 * @brief Log a review-state change (S2). The note records the
 * output flag the S2 linkage also changed, when it did.
 * @param [in] ctxt Compare context.
 * @param [in] relPath File (relative path).
 * @param [in] oldReview / newReview Review state before / after.
 * @param [in] oldInclude / newInclude Output flag before / after.
 * @return False when nothing changed or the append failed.
 */
bool Merge20AuditLogReviewChange(CDiffContext& ctxt, const String& relPath,
	ReviewState oldReview, ReviewState newReview, bool oldInclude, bool newInclude);

/**
 * @brief Log an output-flag change.
 * @param [in] ctxt Compare context.
 * @param [in] relPath File (relative path).
 * @param [in] oldInclude / newInclude Output flag before / after.
 * @param [in] kind IncludeInOutput for a single toggle, BatchCheck for a batch operation.
 * @return False when nothing changed or the append failed.
 */
bool Merge20AuditLogIncludeChange(CDiffContext& ctxt, const String& relPath,
	bool oldInclude, bool newInclude, Merge20AuditKind kind);

/**
 * @brief Log a per-diff conclusion (B1). The note records the
 * review state / output flag the B1 linkage also changed.
 * @param [in] ctxt Compare context.
 * @param [in] relPath File (relative path).
 * @param [in] diffIndex Diff-list index (0-based).
 * @param [in] oldConclusion / newConclusion Conclusion before / after.
 * @param [in] oldReview / newReview Review state before / after the linkage.
 * @param [in] oldInclude / newInclude Output flag before / after the linkage.
 * @return False when nothing changed or the append failed.
 */
bool Merge20AuditLogConclusion(CDiffContext& ctxt, const String& relPath, int diffIndex,
	Merge20DiffConclusion oldConclusion, Merge20DiffConclusion newConclusion,
	ReviewState oldReview, ReviewState newReview, bool oldInclude, bool newInclude);

/**
 * @brief Log a learning-candidate acceptance or dismissal (L3).
 * @param [in] ctxt Compare context.
 * @param [in] relPath File (relative path).
 * @param [in] diffIndex Diff-list index (0-based).
 * @param [in] kind LearnAccept or LearnDismiss.
 * @param [in] oldConclusion / newConclusion Conclusion before / after (equal for a dismissal).
 * @param [in] oldReview / newReview Review state before / after the linkage.
 * @param [in] oldInclude / newInclude Output flag before / after the linkage.
 * @param [in] note Secondary codes: the candidate's "via exact" /
 *             "via feat" (acceptance), or "candidate <strategy>" (dismissal).
 * @return False when the append failed (a dismissal is always a change).
 */
bool Merge20AuditLogLearn(CDiffContext& ctxt, const String& relPath, int diffIndex,
	Merge20AuditKind kind,
	Merge20DiffConclusion oldConclusion, Merge20DiffConclusion newConclusion,
	ReviewState oldReview, ReviewState newReview, bool oldInclude, bool newInclude,
	const String& note);

/**
 * @brief Log a per-row split or a regroup (D5).
 * @param [in] ctxt Compare context.
 * @param [in] relPath File (relative path).
 * @param [in] diffIndex Diff-list index (0-based) of the split parent / regrouped diff.
 * @param [in] kind SplitDiff or RegroupDiff.
 * @param [in] rows Row count of the split group (>= 2).
 * @return False when the append failed.
 */
bool Merge20AuditLogSplit(CDiffContext& ctxt, const String& relPath, int diffIndex,
	Merge20AuditKind kind, int rows);

/**
 * @brief Log a change-detection reset (S8): the file changed
 * since the session was saved, so its saved review state is
 * discarded and it goes back to unreviewed.
 * @param [in] doc Folder compare document (owns the folder pair).
 * @param [in] relPath File (relative path).
 * @param [in] savedReview Review state the session had saved for the file.
 * @return False when the append failed.
 */
bool Merge20AuditLogChangeReset(CDirDoc& doc, const String& relPath, ReviewState savedReview);

/**
 * @brief Load the audit log of this document's folder pair.
 * @param [in] doc Folder compare document.
 * @param [out] entries Entries in file order (oldest first).
 * @param [out] skippedLines Lines skipped while loading (see Merge20AuditLoadFromFile).
 * @param [out] filePath Audit log file path (set even when the file is missing).
 * @return False when the file is missing or unreadable.
 */
bool Merge20AuditLoadForDoc(CDirDoc& doc, std::vector<Merge20AuditEntry>& entries, int& skippedLines, String& filePath);

/**
 * @brief Write the audit CSV for the given entries (BOM-prefixed
 * UTF-8, CRLF, S6 escaping; see Merge20AuditWriteCsvFile).
 * @param [in] entries Entries in chronological order.
 * @param [in] filePath Destination file (selected with the save dialog).
 * @param [out] errStr Empty on success, otherwise the error message.
 * @param [out] exportedCount Number of rows written (excluding the header).
 * @return True on success.
 */
bool Merge20AuditExportCsv(const std::vector<Merge20AuditEntry>& entries, const String& filePath, String& errStr, int& exportedCount);

/** @brief Display name of a kind (translated); the diff number is appended when diffIndex >= 0. */
String Merge20AuditKindDisplayName(Merge20AuditKind kind, int diffIndex = -1);
/** @brief Display name of a review state (translated; the S1/S2 short forms). */
String Merge20AuditReviewDisplayName(ReviewState state);
/** @brief Display name of a diff conclusion (translated; the B1 words). */
String Merge20AuditConclusionDisplayName(Merge20DiffConclusion conclusion);
/** @brief Display text of an output flag ("ON" / "OFF", untranslated codes). */
String Merge20AuditIncludeDisplayName(bool include);
/** @brief Display text of a stored note (translated where the note carries codes). */
String Merge20AuditNoteDisplayName(const String& note);

/** @brief Display strings of one entry, for the viewer and the CSV (same mapping, so they never disagree). */
Merge20AuditCsvRow Merge20AuditBuildDisplayRow(const Merge20AuditEntry& entry);
/** @brief Display rows of all entries, in file order. */
std::vector<Merge20AuditCsvRow> Merge20AuditBuildDisplayRows(const std::vector<Merge20AuditEntry>& entries);
/** @brief CSV column headers (translated), in row order. */
std::vector<String> Merge20AuditCsvHeaders();
