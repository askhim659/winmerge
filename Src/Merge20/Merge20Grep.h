// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Grep.h
 *
 * @brief Cross-file search (Grep) helpers (Merge20 Phase 4 D7, design
 * sections 3.7, 4.3 and 5.2/D7, spec sections 3 (4/4) and 4(g)).
 *
 * Pure module (no MFC, no `_()`): the hit record, the case-insensitive
 * substring match, the excerpt builder and the search-history list.
 * The same code is compiled into WinMergeU and into the GoogleTest
 * runner.
 *
 * Port of the HTML tool's `runGrep` (17_v17.15.html, line 1810):
 * the query is trimmed and lower-cased, every OLD/NEW line is scanned
 * for a substring match, hits stop at 80, and a click jumps to the
 * file and line. The C++ spelling differs in three places, all
 * deliberate: the diff index is NOT computed here (the scan reads
 * on-disk text without opening a compare, so there is no diff list
 * yet; the dialog resolves it at click time from the opened
 * CMergeDoc, see Merge20GrepDlg.cpp), the excerpt is a trimmed
 * window of at most 120 characters around the hit instead of the
 * whole line, and the history (latest 10 queries, design section
 * 4.1, key OPT_MERGE20_GREP_HISTORY) is stored newline-separated.
 */
#pragma once

#include <string>
#include <vector>
#include "UnicodeString.h"

/** @brief Maximum hits (HTML `runGrep`: stops at 80). */
constexpr int Merge20GrepMaxHits = 80;
/** @brief Maximum stored history queries (design section 4.1). */
constexpr int Merge20GrepHistoryMax = 10;
/** @brief Maximum excerpt length in characters. */
constexpr size_t Merge20GrepExcerptMax = 120;

/**
 * @brief One Grep hit (design section 4.3).
 * The diff index is intentionally absent: it is resolved at click
 * time from the opened compare document.
 */
struct Merge20GrepHit
{
	String relPath; /**< File path relative to the compared root. */
	int pane = 0;   /**< 0 = OLD, 1 = NEW (2-way); 1 = middle in 3-way. */
	int line = 0;   /**< 0-based real file line of the hit. */
	String excerpt; /**< Trimmed hit-line text (at most 120 chars). */
};

/** @brief One file to scan: relative path plus one full path per pane (empty when that side does not exist). */
struct Merge20GrepFile
{
	String relPath;
	std::vector<String> paths;
};

/**
 * @brief Case-insensitive substring search (HTML: both sides lower-cased).
 * @param [in] line Line text to search (any case).
 * @param [in] queryLower Query already lower-cased by the caller.
 * @return Character index of the first match, or -1 (also for an empty query).
 */
int Merge20GrepFindInLine(const String& line, const String& queryLower);

/**
 * @brief Build the excerpt for a hit line: trimmed, at most
 * Merge20GrepExcerptMax characters, windowed around the hit when
 * the line is longer (with "..." marking a cut end).
 * @param [in] line Full hit-line text.
 * @param [in] hitPos Character index of the match (from Merge20GrepFindInLine).
 * @param [in] queryLen Length of the query in characters.
 * @return Excerpt, never longer than Merge20GrepExcerptMax + 6 (two "..." markers).
 */
String Merge20GrepMakeExcerpt(const String& line, size_t hitPos, size_t queryLen);

/**
 * @brief Parse the stored history (OPT_MERGE20_GREP_HISTORY):
 * newline-separated, most recent first.
 * @param [in] stored Raw option value (may be empty).
 * @return Queries in stored order, empty entries dropped.
 */
std::vector<String> Merge20GrepParseHistory(const String& stored);

/**
 * @brief Format a history list for storage: newline-separated,
 * most recent first.
 */
String Merge20GrepFormatHistory(const std::vector<String>& history);

/**
 * @brief Push a query to the front of a history list: the query is
 * trimmed, an empty query leaves the list unchanged, an existing
 * entry is moved to the front (not duplicated), and the list is
 * capped at Merge20GrepHistoryMax entries.
 * @param [in] history Current history, most recent first.
 * @param [in] query Query just searched.
 * @return Updated history.
 */
std::vector<String> Merge20GrepPushHistory(const std::vector<String>& history, const String& query);

/**
 * @brief Whether the hit limit is reached: the scan stops at
 * Merge20GrepMaxHits hits (design section 4.3).
 */
bool Merge20GrepReachedLimit(int hitCount);
