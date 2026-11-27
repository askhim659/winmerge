// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Audit.h
 *
 * @brief Append-only audit log of judgment operations (Merge20
 * best-workflow B11, pillar 6: "who changed what, and when, can be
 * traced afterwards").
 *
 * Pure module (no MFC, no `_()`): one audit entry per judgment
 * operation, kept as one JSON object per line (JSON Lines) in a
 * file bound to one folder pair. The same code is compiled into
 * WinMergeU and into the GoogleTest runner.
 *
 * Format decision (gap analysis section 4.2 B11 leaves the format
 * to the design): JSON Lines, not a single JSON document and not
 * CSV. A single JSON document would have to be parsed and
 * rewritten whole on every judgment; a CSV would lose the
 * structured old/new values. One line per entry means appending
 * is a single write, a torn last line (crash mid-write) only
 * costs that one entry, and the log stays human-readable. The CSV
 * the user exports is derived from these entries on demand
 * (Merge20AuditBuildCsvText / Merge20AuditWriteCsvFile), with the
 * S6 escaping rules and a BOM so Excel reads it directly.
 *
 * File name (same rule as the session file, S8): the session
 * directory (`Merge20Sessions` next to the program when portable,
 * otherwise `%APPDATA%\WinMerge\Merge20Sessions`) and the same
 * folder-pair hash, but the prefix `Merge20Audit_` instead of
 * `Merge20Session_`:
 * `Merge20Audit_<FNV-1a 64-bit hex of the normalized, lowercased
 * folder paths>.json`. The session format itself is untouched
 * (B11 must not touch the S8 format); the audit log is a separate
 * file, and it is also separate from the learning JSON (learning
 * is an aggregate of patterns for suggestions, the audit log is
 * the per-judgment history; neither is derived from the other).
 *
 * One entry carries: time (local "YYYY-MM-DD HH:MM:SS", opaque
 * to this module), file (relative path), kind (operation kind),
 * old/new values (stable codes, not display text: review codes
 * TODO/DONE/HOLD/SPECIAL/KEEP, "ON"/"OFF", conclusion codes
 * Undecided/UseOld/UseNew/UseBoth/Reject/Keep, row counts for a
 * split/regroup), operator id (the learning source id `u-<GUID>`,
 * supplied by the caller so this module stays free of the
 * options manager), an optional note (codes, e.g. the output
 * flag the conclusion linkage also changed, or the learning
 * candidate's "exact"/"feat"), and the diff index (-1 when the
 * operation is not about one diff). The viewer and the CSV
 * export map the codes to display text; the log file itself stays
 * stable across languages.
 *
 * What is NOT logged, by design (see the PR body):
 * - Undo / redo (B10): rewinding a judgment is not a new
 *   judgment. The undo paths use the raw setters and the
 *   restored-conclusion entry point, none of which logs.
 * - Session save / restore (S8, acceptance (e)): restoring
 *   rewrites the states wholesale (and clears the undo history);
 *   it is not a judgment operation. The restore's change
 *   detection is the one exception: a file reset to TODO because
 *   its content changed since the save IS logged (kind
 *   ChangeReset), because that reset discards a past judgment.
 *
 * This file stays ASCII: the fixed words (kind names, JSON field
 * names) are ASCII, and Japanese text never appears here.
 */
#pragma once

#include <string>
#include <vector>
#include "UnicodeString.h"
#include "Merge20Types.h"
#include "Merge20/Merge20Conclusion.h"

/** @brief Kind of judgment operation one audit entry records. */
enum class Merge20AuditKind
{
	ReviewState,    /**< Review-state change (S2 keys 1-5, Space, panel cycling) */
	IncludeInOutput, /**< Output-checkbox toggle */
	DiffConclusion, /**< Per-diff conclusion (B1) */
	BatchCheck,     /**< Batch check (S3 dialog, per-directory check, clear-all): one entry per file whose flag changed */
	LearnAccept,    /**< Learning-candidate acceptance (L3) */
	LearnDismiss,   /**< Learning-candidate dismissal (L3) */
	SplitDiff,      /**< Per-row split of a diff group (D5) */
	RegroupDiff,    /**< Regroup of a split group back to one diff (D5) */
	ChangeReset,    /**< Change detection (S8): file changed since the save, reset to unreviewed */
};

/**
 * @brief Stable code name of a kind ("ReviewState", ...), used in
 * the JSON `kind` field. Display names live at the call sites
 * via `_()`, so this file stays ASCII-only.
 */
String Merge20AuditKindName(Merge20AuditKind kind);
/** @brief Parse a kind code name; false when the name is not one of the nine kinds. */
bool Merge20AuditKindFromName(const String& name, Merge20AuditKind& out);

/** @brief Stable code of a review state ("TODO", "DONE", "HOLD", "SPECIAL", "KEEP"). */
String Merge20AuditReviewCode(ReviewState state);
/** @brief Parse a review-state code; false when the code is not one of the five states. */
bool Merge20AuditReviewFromCode(const String& code, ReviewState& out);

/** @brief Stable code of a diff conclusion ("Undecided", "UseOld", "UseNew", "UseBoth", "Reject", "Keep"). */
String Merge20AuditConclusionCode(Merge20DiffConclusion conclusion);
/** @brief Parse a diff-conclusion code; false when the code is not one of the six conclusions. */
bool Merge20AuditConclusionFromCode(const String& code, Merge20DiffConclusion& out);

/** @brief Stable code of an output flag ("ON" / "OFF"). */
String Merge20AuditIncludeCode(bool include);

/**
 * @brief One audit-log entry: one judgment operation on one file.
 *
 * Values are stored as codes (see above) so the entry stays free
 * of display text and of document pointers. `diffIndex` is the
 * diff-list index (0-based) for the diff kinds, -1 otherwise.
 * `note` carries secondary codes (e.g. the output flag a
 * conclusion linkage also changed, "review TODO->DONE, include
 * OFF->ON", or a learning candidate's "exact"/"feat"); it may be
 * empty.
 */
struct Merge20AuditEntry
{
	String time; /**< Local time, "YYYY-MM-DD HH:MM:SS" (opaque to this module) */
	String relPath; /**< File, as DIFFITEM::getItemRelativePath() */
	Merge20AuditKind kind = Merge20AuditKind::ReviewState;
	String oldValue; /**< Old value (code) */
	String newValue; /**< New value (code) */
	String operatorId; /**< Operator id (learning source id `u-<GUID>`) */
	String note; /**< Secondary codes (may be empty) */
	int diffIndex = -1; /**< Diff-list index, or -1 when not about one diff */
};

/**
 * @brief Whether the entry changes anything at all.
 * @return False when the old and new values are equal and the
 *         kind carries no other change (a review-state entry
 *         whose review moved but whose note is empty is still a
 *         change; a dismissal is a change even when the
 *         conclusion did not move, because the candidate
 *         disappearing is the change). Callers use this to skip
 *         no-op operations (pressing '2' on a file that is
 *         already DONE writes no entry).
 */
bool Merge20AuditEntryHasChange(const Merge20AuditEntry& entry);

/**
 * @brief Serialize one entry as one JSON line (no line ending).
 *
 * Compact (single-line) JSON: the shared writer
 * (Merge20JsonStringify) pretty-prints, which would break the
 * one-line-per-entry layout, so this module carries its own
 * compact writer for this fixed 8-field shape. Field order is
 * fixed: time, file, kind, old, new, operator, note, diff.
 * Non-ASCII text is written as raw UTF-8 (like the session
 * writer); control characters are escaped.
 * @param [in] entry Entry to write.
 * @param [out] outUtf8 One JSON object as UTF-8 text (no BOM, no line ending).
 * @return Always true for a well-formed entry.
 */
bool Merge20AuditEntryToJson(const Merge20AuditEntry& entry, std::string& outUtf8);

/**
 * @brief Parse one JSON line into an entry.
 * @param [in] lineUtf8 One line of the log file (UTF-8, without the line ending).
 * @param [out] out Parsed entry.
 * @param [out] errDetail Empty on success, otherwise the reason.
 * @return False when the line is not a JSON object, when `kind`
 *         is missing or unknown, or when a field has the wrong
 *         type. Unknown fields are ignored (forward
 *         compatibility); missing text fields default to empty,
 *         a missing `diff` defaults to -1.
 */
bool Merge20AuditEntryFromJson(const std::string& lineUtf8, Merge20AuditEntry& out, std::string& errDetail);

/**
 * @brief Append one entry to the log file (one write: the JSON
 * line plus "\\n", UTF-8, no BOM).
 * @param [in] entry Entry to append.
 * @param [in] filePath Destination file (created when missing).
 * @param [out] errDetail Empty on success, otherwise the reason.
 * @return True on success. A failed append must never block the
 *         judgment itself: callers log the failure nowhere and
 *         the operation still succeeds (the viewer reports a
 *         missing/unreadable log instead).
 */
bool Merge20AuditAppendToFile(const Merge20AuditEntry& entry, const String& filePath, std::string& errDetail);

/**
 * @brief Load a log file (oldest first, file order).
 * @param [in] filePath Source file.
 * @param [out] out Entries in file order.
 * @param [out] skippedLines Lines skipped while loading (empty
 *         lines are ignored silently; unparseable lines -- e.g.
 *         a torn last line from a crash mid-write -- are counted
 *         here, not fatal).
 * @param [out] errDetail Empty on success, otherwise the reason.
 * @return False when the file cannot be opened or read (a missing
 *         file is not an error for the caller: it means no
 *         judgments were made yet; this function still returns
 *         false with errDetail set so the caller can tell the
 *         two cases apart).
 */
bool Merge20AuditLoadFromFile(const String& filePath, std::vector<Merge20AuditEntry>& out, int& skippedLines, std::string& errDetail);

/**
 * @brief One row of the audit CSV: display strings, already
 * mapped by the caller (the pure module never formats
 * user-visible text). The caller maps the kind code and the
 * old/new codes to display text via `_()`; this struct only
 * carries the results.
 */
struct Merge20AuditCsvRow
{
	String time;       /**< Date/time (as shown) */
	String file;       /**< File (relative path) */
	String kind;       /**< Operation kind (display text, with the diff number when about one diff) */
	String oldValue;   /**< Old value (display text) */
	String newValue;   /**< New value (display text) */
	String operatorId; /**< Operator id */
	String note;       /**< Secondary codes, as display text (may be empty) */
};

/**
 * @brief Format one CSV row (S6 escaping: every text field
 * double-quoted, embedded quotes doubled; no line ending).
 */
String Merge20AuditFormatCsvRow(const Merge20AuditCsvRow& row);

/**
 * @brief Build the whole CSV text: the header line (joined bare,
 * as in the S6 writer; none of the headers contains a comma, a
 * quote or a newline) followed by one row per entry, lines
 * joined with CRLF, no trailing line ending, no BOM (the BOM is
 * added by Merge20AuditWriteCsvFile).
 * @param [in] rows Rows in chronological order.
 * @param [in] headers Seven column headers, in row order.
 * @return CSV text (UTF-8 when encoded by the caller; this
 *         function works on String and adds no BOM).
 */
String Merge20AuditBuildCsvText(const std::vector<Merge20AuditCsvRow>& rows, const std::vector<String>& headers);

/**
 * @brief Write the audit CSV to a file: BOM-prefixed UTF-8 with
 * CRLF line endings (S6 rule), so it opens directly in Excel
 * without garbled Japanese.
 * @param [in] rows Rows in chronological order.
 * @param [in] headers Seven column headers, in row order.
 * @param [in] filePath Destination file.
 * @param [out] errDetail Empty on success, otherwise the reason.
 * @return True on success.
 */
bool Merge20AuditWriteCsvFile(const std::vector<Merge20AuditCsvRow>& rows, const std::vector<String>& headers, const String& filePath, std::string& errDetail);
