// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20LearnDoc.h
 *
 * @brief Adapter between the learning core (Merge20Learn, L0) and the
 * application: where the learning file lives, lazy loading, saving,
 * the ON/OFF switch and this PC's source id.
 *
 * Merge20 (Phase 3 L1, design sections 3.4, 3.5 and 5): the learning
 * data is one JSON v2 file, `Merge20Learn.json`, in its own directory
 * next to the session directory (`Merge20Learn`, not `Merge20Sessions`):
 * the learning data is shared across folder pairs, while a session is
 * bound to one folder pair, so the two must not be mixed. The file is
 * loaded lazily on first access (a missing or broken file never blocks
 * startup; the failure is remembered and reported when the learning
 * dialog is opened, L4), and saved immediately whenever the store
 * changes (L2 records, L5 import/delete/clear). Saving uses a
 * temporary file plus rename, the same way as the session save (S8).
 *
 * The ON/OFF key `OPT_MERGE20_LEARNING_ENABLED` was added in S8 but
 * had no reader until now; `Merge20LearnIsEnabled()` is its first
 * reader (L2 checks it at the start of the record and candidate
 * functions, and the compare options page binds it to a checkbox).
 * This PC's source id is kept in the new key
 * `OPT_MERGE20_LEARNING_OWNER` ("u-<GUID>", never "self": the HTML
 * tool treats "self" specially, design section 4.3).
 *
 * This header is MFC-free apart from the store type, so the pure
 * module stays testable; all options-manager and file access lives
 * in the .cpp. No automatic application is done here (spec section
 * 4(c)): loading and saving the store never touches a merge result.
 */
#pragma once

#include <vector>
#include "UnicodeString.h"
#include "Merge20/Merge20Learn.h"

class CMergeDoc;

/** @brief Directory holding the learning file. Created on demand by the save path; empty when it cannot be determined. */
String Merge20LearnGetDirectory();
/** @brief Full path of the learning file (`Merge20Learn.json`). */
String Merge20LearnGetFilePath();

/**
 * @brief Learning ON/OFF (first reader of `OPT_MERGE20_LEARNING_ENABLED`).
 * @return False while learning is off: L2 then records nothing and
 * computes no candidates (the HTML tool's OFF behaviour).
 */
bool Merge20LearnIsEnabled();

/**
 * @brief This PC's source id ("u-<GUID>").
 *
 * Read from `OPT_MERGE20_LEARNING_OWNER`; when the key is still empty
 * (first use), a GUID is generated with `CoCreateGuid`, stored in the
 * key and returned. The id is stable per PC afterwards, so judgments
 * recorded here keep the same source across restarts.
 */
String Merge20LearnGetOwnerId();

/** @brief Display name of this PC's source ("Learning on this PC (self)"). */
String Merge20LearnGetOwnerDisplayName();

/**
 * @brief The lazily loaded learning store.
 *
 * The file is read on the first call only; a missing file means an
 * empty store, and a broken file also means an empty store with the
 * failure remembered (see Merge20LearnGetLoadError). The returned
 * store is the single in-memory copy; callers that change it must
 * call Merge20LearnSaveStore afterwards.
 */
Merge20LearnStore& Merge20LearnGetStore();

/**
 * @brief True when the lazy load failed; errStr holds the reason.
 * @return False when the file loaded (or did not exist yet).
 */
bool Merge20LearnGetLoadError(String& errStr);

/**
 * @brief Save the store to `Merge20Learn.json` (temporary file plus rename).
 * @param [out] errStr Human-readable reason on failure (spec section 7:
 *   a failed save is reported, never lost silently).
 * @return False when the write failed.
 */
bool Merge20LearnSaveStore(String& errStr);

/**
 * @brief Merge20 (Phase 3 L5, design section 4.2): default file name
 * for a learning JSON export (HTML `exportLearn` / `exportUnifiedLearn`).
 * The HTML tool's file name (Japanese "Merge20 learning YYYY-MM-DD.json",
 * with a "merged" marker inserted for a merged export; see the .cpp
 * for the exact form, written with \u escapes so the source stays ASCII).
 * The save dialog decides the folder (the desktop equivalent of the
 * HTML tool's browser download folder); only the file name is fixed.
 */
String Merge20LearnDefaultExportFileName(bool merged);

/**
 * @brief Merge20 (Phase 3 L5): write the whole store as learning
 * JSON v2 to an explicit file (HTML `exportLearn` shape).
 * `sourceName` is the file's base name, `sourceId` this PC's owner
 * id. The store itself is not changed (Merge20Learn.json is not
 * touched). @param [out] errDetail ASCII reason on failure.
 */
bool Merge20LearnExportToFile(const String& filePath, std::string& errDetail);

/**
 * @brief Merge20 (Phase 3 L5): write the selected sources merged
 * into one learning JSON v2 (HTML `exportUnifiedLearn` shape).
 * Counts are summed and per-rule `sources` are dropped (L0
 * Merge20LearnMergedToJson); the root `sources` lists the merged
 * sources' display names. `ruleCount` is the number of rules written.
 */
bool Merge20LearnExportMergedToFile(const std::vector<String>& srcIds,
	const String& filePath, std::string& errDetail, int& ruleCount);

/**
 * @brief Merge20 (Phase 3 L5): import a learning JSON file,
 * accumulating (HTML `importLearn`).
 * Reads the file and calls L0 Merge20LearnImportJson: the file's
 * source replaces its previous contribution (no double count), and
 * per-rule `sources` in the file are flattened under that one source.
 * The store is changed in memory only; the caller saves it
 * (Merge20LearnSaveStore), like the L4 delete handler.
 * @param [out] errDetail ASCII reason on failure (see
 * Merge20LearnFromJson: a wrong `app` / `version` rejects the file).
 */
bool Merge20LearnImportFromFile(const String& filePath,
	Merge20LearnFileHeader& header, int& skippedRules, int& addedRules,
	int& mergedRules, std::string& errDetail);

/**
 * @brief Merge20 (Phase 3 L2, design section 3.2): diff lines of one
 * diff (one DIFFRANGE), built like Merge20ClassifyDoc but for a
 * single diff only and without OP_NONE context lines (the HTML tool's
 * hunkKeyExact / hunkFeatKey are built from diff rows only).
 * Line texts are read through ComputeApparentLine, so ghost padding
 * never leaks into the texts. Only 2-way docs have lines; other docs
 * return an empty vector.
 */
std::vector<Merge20DiffLine> Merge20LearnLinesForDiff(const CMergeDoc& doc, int nDiff);

/**
 * @brief Merge20 (Phase 3 L2, design section 3.1): record the current
 * judgment of one diff (HTML `learnFromHunk`).
 *
 * The only record entry point: called from CMergeDoc::OnMergeChooseSource
 * (the explicit source-choice command) and nowhere else, so batch
 * approval (S5), auto-merge and session restore (S8) can never mix
 * into the learning data. Only a `Chosen` segment of a 2-way doc is
 * recorded, and only while learning is enabled. The candidate shown
 * for this diff (if any) is passed as `prevLearned`, so choosing a
 * different strategy corrects the old one (-1) before the new one
 * gains (+1), like the HTML tool. The store is changed in memory
 * only; the caller saves it (Merge20LearnSaveStore) once per command.
 * @return True when a judgment was recorded.
 */
bool Merge20LearnRecordDiffJudgment(CMergeDoc& doc, int nDiff);

/**
 * @brief Merge20 (Phase 3 L3): display name of a judgment strategy
 * ("Insert old" / "Keep new" / "Replace with old" / "Both old and
 * new", translated through _()).
 */
String Merge20LearnStrategyDisplayName(Merge20LearnStrategy strategy);

/**
 * @brief Merge20 (Phase 3 L3): display name of a candidate's match
 * ("Exact match" / "Feature match", translated through _()).
 * The candidate's `via` is "exact" or "feat" (Merge20Learn.h).
 */
String Merge20LearnViaDisplayName(const String& via);

/**
 * @brief Merge20 (Phase 3 L3): dismiss the candidate of one diff.
 *
 * Adds the diff identity to the doc's dismissed set (so a recompute
 * never shows the candidate again) and erases the candidate from the
 * map. Records nothing in the learning data (a dismissal is not a
 * judgment) and changes no review state. The caller invalidates the
 * result view. @return True when a candidate was dismissed.
 */
bool Merge20LearnDismissCandidate(CMergeDoc& doc, int nDiff);

/**
 * @brief Merge20 (Phase 3 L2, design section 3.2): recompute the
 * learning candidates of one doc into its candidate map
 * (CMergeDoc::GetMerge20LearnCandidates()).
 *
 * Only 2-way docs with a built result pane get candidates, and only
 * for diffs whose segment is still Unresolved/Conflict: a candidate
 * never touches `m_resultSegments` and never resolves anything
 * (spec section 4(c)). Dismissed diffs (by identity) get no
 * candidate. When learning is OFF, the map is cleared instead and
 * nothing is computed. The result view is invalidated when the map
 * changed, so a cleared map is visible at once (design section 3.5).
 * Runs on the UI thread only (called from Rescan / StartMergeSession
 * tails, like the S4 classification hook).
 */
void Merge20LearnRefreshCandidates(CMergeDoc& doc);

/**
 * @brief Merge20 (Phase 3 L4, design section 3.3): number of learning
 * candidates currently shown in all open text merge docs (the
 * "Suggestions shown: K" figure of the learning dialog). The HTML
 * tool's "auto-judged in this analysis: N" statistic is dropped
 * (there is no automatic application in the fork, spec section 4(c));
 * the live candidate count takes its place. Counts the candidate map
 * of each open CMergeDoc (binary / image / web page docs are not
 * CMergeDoc and have no candidates).
 */
int Merge20LearnCountOpenCandidates();

/**
 * @brief Merge20 (Phase 3 L2, design section 3.2 (c)): recompute the
 * learning candidates of every open text merge doc (used after the
 * learning data changed: recording, and the ON/OFF toggle in the
 * options dialog). Binary / image / web page docs are skipped
 * (they are not CMergeDoc).
 */
void Merge20LearnRefreshAllOpenDocs();
