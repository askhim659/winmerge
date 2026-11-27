// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Impact.h
 *
 * @brief Cross-file impact search (Merge20 best-workflow B5,
 * pillar 3, gap analysis section 4.2 B5; PR1: engine + background
 * search, no display).
 *
 * When a diff is judged, the symbols it changed (the affected
 * function names S4 extracts, Merge20ClassifyResult::functions)
 * may be used in other files of the compared folders. This module
 * is the engine that finds those uses: it reuses the S4 symbol
 * extraction as-is (no accuracy work here; that is a separate
 * discussion) and the D7 text search (Merge20GrepFindInLine for
 * the case-insensitive substring match, Merge20GrepMakeExcerpt
 * for the hit text, Merge20GrepMaxHits = 80 for the cutoff).
 *
 * Pure module (no MFC, no `_()`): the hit record, the result
 * record, the symbol list taken from a classification result,
 * the self-file exclusion and the streaming collector that the
 * background worker (Merge20ImpactSearch) feeds one line at a
 * time. The same code is compiled into WinMergeU and into the
 * GoogleTest runner.
 *
 * Search scope and precision (gap analysis B5 (c)):
 * - the search covers the whole compared folder (both panes of
 *   every file, OLD and NEW), except the file being judged
 *   itself: a use inside the judged file is the diff itself, not
 *   an impact on another file. The exclusion compares the
 *   relative path (case-insensitively, Windows paths).
 * - the search is a text search, not a reference analysis: a hit
 *   means "the symbol text occurs on this line", which can be a
 *   false positive (a comment, a different symbol with the same
 *   name) and can miss uses (a use spelled differently). Every
 *   result therefore carries isTextSearchEstimate = true, and the
 *   display (PR2) must show the estimate note next to the list.
 *   The fixed wording of the note is decided here (PR1) so the
 *   display, the tests and the PR description can never disagree:
 *   - msgid (English, rendered through `_()` by the display):
 *     "Estimated by text search; not a precise reference analysis."
 *   - Japanese rendering (the .po msgstr PR2 adds):
 *     "テキスト検索による推定であり、厳密な参照解析ではありません。"
 *   The msgid is kept as the named constant below; the Japanese
 *   text itself lives in the .po file, never in this module, so
 *   this file stays free of Japanese string literals.
 *
 * FORM comparison is out of scope: FORM is visualisation-only
 * (user decision 2026-11-26), so no impact search ever runs for
 * it. This module does not know about FORM at all; the caller
 * (PR2) simply never starts a search from a FORM view.
 */
#pragma once

#include <string>
#include <vector>
#include "Merge20/Merge20Classify.h"
#include "Merge20/Merge20Grep.h"

/** @brief Maximum impact hits in one result (D7: stops at 80). */
constexpr size_t Merge20ImpactMaxHits = static_cast<size_t>(Merge20GrepMaxHits);

/**
 * @brief English msgid of the estimate note (see the file header).
 * The display renders it through `_()`; the Japanese rendering is
 * fixed in the file header and added to the .po file by PR2.
 */
constexpr const wchar_t* Merge20ImpactEstimateNoteMsgId =
	L"Estimated by text search; not a precise reference analysis.";

/**
 * @brief One use of one changed symbol in another file.
 * The diff index is intentionally absent, exactly as in
 * Merge20GrepHit: it is resolved at click time from the opened
 * compare document (PR2 reuses the D7 jump route,
 * CDirView::Merge20OpenFileAtLine).
 */
struct Merge20ImpactHit
{
	String symbol;  /**< Changed symbol found on this line (S4 spelling). */
	String relPath; /**< File path relative to the compared root. */
	int pane = 0;   /**< 0 = OLD, 1 = NEW (2-way); 1 = middle in 3-way. */
	int line = 0;   /**< 0-based real file line of the hit. */
	String excerpt; /**< Trimmed hit-line text (at most 120 chars, D7 rule). */
};

/**
 * @brief Result of one impact search (one judged diff).
 * An empty hits list with symbols present means "no use in other
 * files": the display (PR2) shows that explicitly instead of an
 * empty list. An empty symbols list means the diff changed no
 * symbol S4 could name, so no search ran at all.
 */
struct Merge20ImpactResult
{
	std::vector<String> symbols; /**< Changed symbols searched for (S4 order). */
	std::vector<Merge20ImpactHit> hits; /**< Uses found, in scan order (folder-list order, then line order). */
	bool limitReached = false; /**< True when the 80-hit cutoff stopped the scan (hits may be incomplete). */
	bool isTextSearchEstimate = true; /**< Always true: the hits are a text-search estimate, not a reference analysis (see the file header). */
	int filesScanned = 0; /**< Files fully scanned (for the progress status). */
	int filesTotal = 0;   /**< Files in the search scope. */
};

/**
 * @brief Symbols to search for, taken from an S4 classification.
 * @param [in] result Classification of the judged diff (or file).
 * @return result.functions, in first-seen order, with empty
 *         entries and exact duplicates dropped. No accuracy work
 *         is done here: the list is exactly what S4 extracted.
 */
std::vector<String> Merge20ImpactSymbolsFromClassify(const Merge20ClassifyResult& result);

/**
 * @brief Whether a scanned file is the file being judged (and so
 * must be skipped: the impact search covers other files only).
 * @param [in] relPath Relative path of the scanned file.
 * @param [in] currentRelPath Relative path of the judged file.
 * @return True when the two paths name the same file
 *         (case-insensitive compare; an empty current path never
 *         matches, so a caller without a current file searches
 *         everything).
 */
bool Merge20ImpactIsSelfFile(const String& relPath, const String& currentRelPath);

/**
 * @brief Whether the hit limit is reached: the scan stops at
 * Merge20ImpactMaxHits hits (D7 rule, shared by construction).
 */
bool Merge20ImpactReachedLimit(size_t hitCount);

/**
 * @brief Streaming collector for one impact search.
 *
 * The background worker (Merge20ImpactSearch) feeds it one file
 * line at a time, in folder-list order; the collector appends one
 * hit per (line, symbol) pair whose symbol text occurs on the
 * line, and stops appending at Merge20ImpactMaxHits. The same
 * collector is what the unit tests feed directly, so the worker
 * adds no matching logic of its own.
 */
class Merge20ImpactCollector
{
public:
	/**
	 * @brief Build a collector for one judged diff.
	 * @param [in] symbols Changed symbols (S4 order). Empty
	 *             entries and exact duplicates are dropped.
	 * @param [in] currentRelPath Relative path of the judged file
	 *             (self-file exclusion); empty means no exclusion.
	 */
	Merge20ImpactCollector(std::vector<String> symbols, String currentRelPath);
	Merge20ImpactCollector() : Merge20ImpactCollector(std::vector<String>(), String()) {}

	/** @brief Symbols actually searched for (after dedupe). */
	const std::vector<String>& GetSymbols() const { return m_symbols; }
	/** @brief Hits collected so far. */
	const std::vector<Merge20ImpactHit>& GetHits() const { return m_hits; }
	/** @brief Hits collected so far. */
	size_t GetHitCount() const { return m_hits.size(); }
	/** @brief True when the 80-hit cutoff is reached (no more hits are appended). */
	bool IsFull() const { return Merge20ImpactReachedLimit(m_hits.size()); }
	/** @brief True when the scan found nothing yet. */
	bool IsEmpty() const { return m_hits.empty(); }

	/**
	 * @brief Feed one file line to the collector.
	 * @param [in] relPath Relative path of the scanned file.
	 * @param [in] pane Pane of the scanned file (0 = OLD, 1 = NEW).
	 * @param [in] line 0-based real file line number.
	 * @param [in] text Full line text.
	 * @return Hits appended for this line (one per matching
	 *         symbol, in symbol order). Empty when the line is in
	 *         the judged file itself, when no symbol occurs on
	 *         it, or when the collector is already full.
	 */
	std::vector<Merge20ImpactHit> AddLine(const String& relPath, int pane, int line, const String& text);

	/**
	 * @brief Take the finished result.
	 * @param [in] filesScanned Files fully scanned.
	 * @param [in] filesTotal Files in the search scope.
	 * @return Result with the symbols, the hits in scan order,
	 *         limitReached set exactly when the cutoff stopped
	 *         the scan, and isTextSearchEstimate always true.
	 */
	Merge20ImpactResult TakeResult(int filesScanned, int filesTotal) const;

private:
	std::vector<String> m_symbols;      /**< Symbols searched for (deduped). */
	std::vector<String> m_symbolsLower; /**< Lower-cased queries, same order. */
	String m_currentRelPath;            /**< Judged file (self-file exclusion). */
	std::vector<Merge20ImpactHit> m_hits; /**< Hits collected so far. */
};
