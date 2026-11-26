// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Session.h
 *
 * @brief Work-session save/restore: data model, JSON file format and
 * change detection (Merge20 Phase 2 S8, design section 3.6, spec
 * sections 6 and 6.1).
 *
 * The session file is a JSON document (UTF-8, one file per folder
 * pair) holding the per-file work state that the folder compare
 * sidecar map keeps: review state, output checkbox, action category,
 * risk, counts, summary, audit memo and the per-diff merge judgments
 * (MergeResultSegment::srcPanes / state, keyed by diff index), plus
 * the folder pair, the compare options that were active, and the
 * display settings (filters, column order/widths, hidden items, view
 * position). File identity (size and modification time of every side,
 * diff counts) is stored alongside so that a file whose real content
 * changed since the save is reset to unreviewed on restore instead
 * of inheriting a stale judgment (spec section 6.1).
 *
 * This module is deliberately free of MFC and of the translation
 * macro `_()`: the same code is compiled into WinMergeU and into the
 * GoogleTest runner. The MFC side (Merge20SessionDoc) builds the
 * session from a CDirDoc and applies a loaded session back to it.
 *
 * JSON library (design T-2-1, decided in S8): no JSON parser or
 * serializer is used anywhere in Src/ (no nlohmann/json, picojson,
 * rapidjson, jsoncpp or Poco::JSON; Externals/poco and Externals/jq
 * are uninitialized submodules that nothing in the build links).
 * The session structure is fixed and small, so this module carries
 * its own minimal JSON reader/writer instead of a generic parser
 * (design section 3.6, option (b)). The XML-based ProjectFile is
 * not used: the spec requires JSON (option (c) would be a spec
 * change, and it was not adopted).
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "UnicodeString.h"
#include "Merge20Types.h"

/** @brief Format version of the Merge20 session file. */
constexpr int kMerge20SessionFormatVersion = 1;

/**
 * @brief Encode a wide string as UTF-8 (lone surrogates become U+FFFD).
 * @param [in] text Wide (UTF-16) text.
 * @return UTF-8 bytes.
 */
std::string Merge20Utf8Encode(const String& text);

/**
 * @brief Decode UTF-8 bytes into a wide string (invalid bytes become
 * U+FFFD, decoding continues after them).
 * @param [in] utf8 UTF-8 bytes.
 * @param [out] out Decoded wide text.
 * @return Always true (decoding never fails; bad input is replaced).
 */
bool Merge20Utf8Decode(const std::string& utf8, String& out);

/**
 * @brief One JSON value (minimal DOM for the fixed session format).
 *
 * Objects keep insertion order (a vector of pairs) so that a written
 * file has a stable, human-readable field order.
 */
struct Merge20JsonValue
{
	enum class Type
	{
		Null,
		Bool,
		Int,
		Text,
		Array,
		Object
	};

	Type type = Type::Null;
	bool boolean = false;
	int64_t integer = 0;
	String text;
	std::vector<Merge20JsonValue> array;
	std::vector<std::pair<String, Merge20JsonValue>> object;

	/** @brief Find a member of an object by key (nullptr when absent or not an object). */
	const Merge20JsonValue* Find(const String& key) const;
	/** @brief Value as bool (default when absent or of another type). */
	bool AsBool(bool defaultValue) const;
	/** @brief Value as integer (default when absent or of another type). */
	int64_t AsInt(int64_t defaultValue) const;
	/** @brief Value as text (default when absent or of another type). */
	String AsText(const String& defaultValue) const;
};

Merge20JsonValue Merge20JsonNull();
Merge20JsonValue Merge20JsonBool(bool value);
Merge20JsonValue Merge20JsonInt(int64_t value);
Merge20JsonValue Merge20JsonText(const String& value);
Merge20JsonValue Merge20JsonArray(std::vector<Merge20JsonValue> values);
Merge20JsonValue Merge20JsonObject(std::vector<std::pair<String, Merge20JsonValue>> members);

/**
 * @brief Parse JSON text into a DOM value.
 * @param [in] text UTF-8 JSON text (a leading UTF-8 BOM is skipped).
 * @param [out] out Parsed value.
 * @param [out] errDetail Empty on success, otherwise "offset N: ..." .
 * @return True on success.
 */
bool Merge20JsonParse(const std::string& text, Merge20JsonValue& out, std::string& errDetail);

/**
 * @brief Write a DOM value as JSON text (2-space indent, UTF-8).
 * @param [in] value Value to write.
 * @return JSON text (no BOM, no trailing newline).
 */
std::string Merge20JsonStringify(const Merge20JsonValue& value);

/** @brief Compare options active when the session was saved. */
struct Merge20SessionCompareOptions
{
	bool recursive = true;
	String fileFilter;
	int compareMethod = 0;
	int ignoreWhitespace = 1;
	bool ignoreCase = true;
	bool ignoreBlankLines = false;
	bool ignoreEol = false;
};

/** @brief Display settings saved with the session. */
struct Merge20SessionDisplay
{
	bool showSkipped = false;
	bool showUniqueLeft = true;
	bool showUniqueMiddle = true;
	bool showUniqueRight = true;
	bool showBinaries = true;
	bool showIdentical = true;
	bool showDifferent = true;
	bool showDifferentLeftOnly = true;
	bool showDifferentMiddleOnly = true;
	bool showDifferentRightOnly = true;
	bool showMissingLeftOnly = true;
	bool showMissingMiddleOnly = true;
	bool showMissingRightOnly = true;
	bool showEmptyFolders = true;
	bool treeMode = false;
	/** @brief Review-state filter initial values (OPT_MERGE20_SHOW_REVIEW_*). */
	bool reviewFilter[5] = { true, true, true, true, true };
	/** @brief Opaque column-order string (DirViewColItems::SaveColumnOrders). */
	String columnOrders;
	/** @brief Opaque column-width string (DirViewColItems::SaveColumnWidths). */
	String columnWidths;
	/** @brief Hidden items (CDiffContext::m_vCurrentlyHiddenItems). */
	std::vector<String> hiddenItems;
	/** @brief Relative path of the focused row (best effort on restore). */
	String focusedPath;
	/** @brief First visible row index (best effort on restore). */
	int topIndex = 0;
};

/** @brief One file's saved work state. */
struct Merge20SessionFile
{
	String relPath;
	ReviewState review = ReviewState::TODO;
	bool includeInOutput = true;
	bool classified = false;
	ActionCategory action = ActionCategory::NO_ACTION_IDENTICAL;
	RiskLevel risk = RiskLevel::NONE;
	int nMissing = 0;
	int nModified = 0;
	int nAdded = 0;
	int nExcluded = 0;
	String summary;
	String memo;
	std::vector<Merge20DiffJudgment> diffs;
	/** @brief Split diff groups of the merge result pane (D5): the
	 * original ranges that were split, so a restore can re-apply
	 * the splits before restoring the per-row judgments. Absent
	 * (empty) in files saved before D5. */
	std::vector<Merge20SplitGroup> splits;
	/** @brief File size in bytes per side, -1 when the side does not exist. */
	int64_t leftSize = -1;
	int64_t rightSize = -1;
	int64_t thirdSize = -1;
	/** @brief Modification time (microseconds since the epoch) per side, -1 when the side does not exist. */
	int64_t leftMtime = -1;
	int64_t rightMtime = -1;
	int64_t thirdMtime = -1;
	/** @brief Diff counts at save time, -1 when unknown (quick compare). */
	int nsdiffs = -1;
	int nidiffs = -1;
};

/** @brief One work session (one folder pair). */
struct Merge20Session
{
	/** @brief Save time, local time as "YYYY-MM-DD HH:MM:SS" (opaque to this module). */
	String savedAt;
	String leftPath;
	String rightPath;
	String thirdPath;
	Merge20SessionCompareOptions compareOptions;
	Merge20SessionDisplay display;
	std::vector<Merge20SessionFile> files;
};

/** @brief Current on-disk identity of one compared file. */
struct Merge20FileIdentity
{
	int64_t leftSize = -1;
	int64_t leftMtime = -1;
	int64_t rightSize = -1;
	int64_t rightMtime = -1;
	int64_t thirdSize = -1;
	int64_t thirdMtime = -1;
	int nsdiffs = -1;
	int nidiffs = -1;
};

/**
 * @brief Decide whether a saved file entry is stale (spec section 6.1).
 * A file counts as changed when any side's size or modification time
 * differs, or when a diff count that was known at save time and is
 * known now differs. Unknown counts (-1) never count as a change.
 * @param [in] saved Saved entry.
 * @param [in] current Current on-disk identity.
 * @return True when the file changed since the save.
 */
bool Merge20SessionFileChanged(const Merge20SessionFile& saved, const Merge20FileIdentity& current);

/**
 * @brief Serialize a session to JSON text (UTF-8, no BOM).
 * @param [in] session Session to write.
 * @param [out] outUtf8 JSON text.
 * @return True on success (always true for a well-formed session).
 */
bool Merge20SessionToJson(const Merge20Session& session, std::string& outUtf8);

/**
 * @brief Parse JSON text into a session.
 * @param [in] utf8 UTF-8 JSON text.
 * @param [out] out Parsed session.
 * @param [out] errDetail Empty on success, otherwise the reason.
 * @param [out] skippedFiles Entries skipped for bad field types.
 * @return True on success. A wrong `app` or `version` fails the load;
 * a single bad file entry is skipped instead (and counted).
 */
bool Merge20SessionFromJson(const std::string& utf8, Merge20Session& out, std::string& errDetail, int& skippedFiles);

/**
 * @brief Write a session to a file (UTF-8 JSON, via a temporary file
 * and rename so a crash mid-write cannot leave a torn session).
 * @param [in] session Session to write.
 * @param [in] filePath Destination path.
 * @param [out] errDetail Empty on success, otherwise the reason.
 * @return True on success.
 */
bool Merge20SessionSaveToFile(const Merge20Session& session, const String& filePath, std::string& errDetail);

/**
 * @brief Read a session from a file.
 * @param [in] filePath Source path.
 * @param [out] out Parsed session.
 * @param [out] errDetail Empty on success, otherwise the reason.
 * @param [out] skippedFiles Entries skipped for bad field types.
 * @return True on success.
 */
bool Merge20SessionLoadFromFile(const String& filePath, Merge20Session& out, std::string& errDetail, int& skippedFiles);
