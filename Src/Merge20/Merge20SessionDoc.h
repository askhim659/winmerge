// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20SessionDoc.h
 *
 * @brief Adapter between the folder/file compare documents (CDirDoc,
 * CMergeDoc) and the UI-independent session module (Merge20Session).
 *
 * Merge20 (S8, design section 3.6, spec sections 6 and 6.1): a work
 * session is a JSON file bound to one folder pair. It records the
 * compare options, the display settings, and one entry per file with
 * its review state, output checkbox, classification, audit memo and
 * per-diff judgments, plus the file size and modification time of
 * every side so a changed file can be detected on restore. The
 * session file lives in `Merge20Sessions` next to the program when
 * the options manager is the INI one (portable distribution, spec
 * section 6), otherwise under `%APPDATA%\WinMerge\Merge20Sessions`.
 * The file name carries an FNV-1a hash of the normalized, lowercased
 * folder paths, so the file for a folder pair is found without
 * scanning the directory; the stored paths are still verified on load.
 *
 * This header is MFC-free apart from forward declarations, so the
 * pure module stays testable; all document access lives in the .cpp.
 */
#pragma once

#include <vector>
#include "UnicodeString.h"
#include "Merge20/Merge20Session.h"

class CDirDoc;
class CMergeDoc;
struct IMergeDoc;
class CDiffContext;

/** @brief Outcome of applying a loaded session to a folder compare. */
struct Merge20SessionRestoreResult
{
	int restored = 0;      /**< Files whose review/output/memo/diffs were restored */
	int reset = 0;         /**< Files changed since save: reset to unreviewed */
	int skipped = 0;       /**< Session entries with no matching current file */
	int diffsRestored = 0; /**< Diff judgments stored into the sidecar for restore into merge docs */
	/** @brief Saved compare options that differ from the current ones (display names, for the restore report). */
	std::vector<String> optionsDiffer;
};

/** @brief Session file path for the folder pair of a compare context. */
String Merge20SessionGetFilePath(const CDiffContext& ctxt);
/** @brief Session file path for explicit (already normalized) folder paths. */
String Merge20SessionGetFilePath(const String& leftPath, const String& rightPath, const String& thirdPath);
/** @brief Directory holding the session files. Created on demand by the save path; empty when it cannot be determined. */
String Merge20SessionGetDirectory();

/**
 * @brief Build a session from the current document state (sidecar map
 * plus the folder view's display settings).
 * @return false when the document has no compare results yet.
 */
bool Merge20SessionBuildFromDoc(CDirDoc& doc, Merge20Session& session);
/** @brief Refresh the diff judgments of still-open merge docs into the sidecar map. */
void Merge20SessionRefreshDiffJudgments(CDirDoc& doc);
/** @brief Stash the diff judgments of one closing merge doc into the sidecar map. */
void Merge20SessionStashMergeDoc(CDirDoc& doc, IMergeDoc* pMergeDoc);
/**
 * @brief Save the current document state to its session file.
 * @param [out] filePath Session file path on success.
 * @param [out] errStr Human-readable reason on failure (spec section 7:
 *   a failed save is reported, never lost silently).
 * @return false when there is nothing to save or the write failed.
 */
bool Merge20SessionSaveDoc(CDirDoc& doc, String& filePath, String& errStr);
/** @brief True when a session file exists for this document's folder pair. */
bool Merge20SessionExistsForDoc(CDirDoc& doc, String& filePath);
/**
 * @brief Load a session file and check it belongs to this document's
 * folder pair.
 * @param [out] skippedFiles Entries skipped while loading (see
 *   Merge20SessionFromJson).
 * @return false with errStr set when the file is unusable or belongs
 *   to another folder pair.
 */
bool Merge20SessionLoadForDoc(CDirDoc& doc, const String& filePath, Merge20Session& session, String& errStr, int& skippedFiles);
/**
 * @brief Apply a loaded session: files whose size, modification time
 * or diff counts changed since the save are reset to unreviewed (their
 * memo is kept); the others get their review state, output checkbox,
 * classification, memo and diff judgments back.
 */
Merge20SessionRestoreResult Merge20SessionApplyToDoc(CDirDoc& doc, const Merge20Session& session);
/**
 * @brief Offer the restore once per document (called from
 * CDirView::OnUpdateUIMessage after a compare completes): when a session
 * file exists for the folder pair, ask the user and apply it. Silent
 * when there is no session file, the document already asked, or a
 * report is being generated. It is deliberately NOT called from
 * CDirDoc::CompareReady: CompareReady is also called from SaveModified
 * while the document is closing, where a restore offer would be wrong.
 */
void Merge20SessionOfferRestore(CDirDoc& doc);
/** @brief Human-readable report of a restore result, for the message boxes (offer + manual restore). */
String Merge20SessionFormatRestoreReport(const Merge20SessionRestoreResult& result, int skippedFiles);
/**
 * @brief Restore the saved diff judgments into a merge doc whose
 * merge session just started (called from CMergeDoc::StartMergeSession
 * after BuildMergeResult and the auto-merge). Saved split groups are
 * re-applied first, so per-row judgments match the split ranges they
 * were saved against. Judgments whose diff no longer matches are
 * skipped. No-op without a saved judgment or split group.
 */
void Merge20SessionRestoreMergeDoc(CMergeDoc& doc);
