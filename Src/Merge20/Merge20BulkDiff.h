/**
 * @file  Merge20BulkDiff.h
 *
 * @brief Line differ for the S4 stage-2 bulk classification (Merge20
 * Phase 4 D10, design sections 3.11 and 5.2/D10, spec section 7).
 *
 * Port of the HTML tool's `diffFast` / `lcs` (17_v17.15.html, lines
 * 1053-1072), adapted to the fork's classifier input: instead of the
 * HTML rows it produces diff ranges (one OP_1STONLY / OP_2NDONLY /
 * OP_DIFF / OP_TRIVIAL range per hunk), which Merge20ClassifyDoc
 * turns into Merge20DiffLine entries exactly like it does for the
 * DIFFRANGE list of an opened merge doc. The pure classifier
 * (Merge20ClassifyDiffLines) therefore sees the same shape of input
 * from the bulk path and from the opened-file path.
 *
 * The HTML simple-compare fallback is ported exactly: after the
 * common prefix/suffix is stripped, when the remaining middle
 * satisfies n*m > threshold (default 2,200,000, the HTML constant,
 * option OPT_MERGE20_SIMPLE_THRESHOLD), the differ falls back to a
 * positional alignment (same line number on both sides) instead of
 * the O(n*m) LCS table, and reports simpleUsed = true. The caller
 * stores that in Merge20FileState::simpleCompare, and the folder
 * list shows the [Simple] badge for the file (spec section 7).
 *
 * Normalization follows the active compare options: lines that are
 * equal after whitespace collapsing / case folding but differ raw
 * become OP_TRIVIAL ranges (excluded lines, like diffutils trivial
 * diffs in the opened-file path). When ignore-blank-lines is on, a
 * diff range made only of blank lines becomes OP_TRIVIAL as well.
 * There is no L2 version-line handling here (the fork has none; the
 * HTML `__L2V__` special case is not ported).
 *
 * This module is deliberately free of MFC and of the translation
 * macro `_()`: the same code is compiled into WinMergeU and into
 * the GoogleTest runner. It stays ASCII-only.
 */
#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include "DiffList.h"
#include "Merge20Types.h"

/** @brief Default simple-compare threshold (HTML `lcs`: n*m > 2200000). Must match the OPT_MERGE20_SIMPLE_THRESHOLD default in OptionsInit.cpp. */
constexpr int64_t Merge20BulkSimpleThresholdDefault = 2200000;

/** @brief One diff hunk produced by the bulk differ. Line numbers are 0-based real lines; begin/end are inclusive. A side with no lines uses the DIFFRANGE convention (end = begin - 1, so the count is 0): OP_2NDONLY has no old lines, OP_1STONLY has no new lines. */
struct Merge20BulkRange
{
	OP_TYPE op = OP_NONE; /**< OP_1STONLY: old-only lines, OP_2NDONLY: new-only lines, OP_DIFF: changed lines, OP_TRIVIAL: ignored (whitespace/case/blank-only) lines. */
	int beginOld = -1; /**< First old line of the range (inclusive). */
	int endOld = -1;   /**< Last old line of the range (inclusive). */
	int beginNew = -1; /**< First new line of the range (inclusive). */
	int endNew = -1;   /**< Last new line of the range (inclusive). */
};

/** @brief Options for the bulk differ, read on the UI thread from the compare options. */
struct Merge20BulkDiffOptions
{
	bool ignoreWhitespace = true; /**< Collapse whitespace runs and trim when comparing (OPT_CMP_IGNORE_WHITESPACE). */
	bool ignoreCase = true;       /**< Compare case-insensitively (OPT_CMP_IGNORE_CASE). */
	bool ignoreBlankLines = false; /**< Blank-only diff ranges become trivial (OPT_CMP_IGNORE_BLANKLINES). */
	int64_t simpleThreshold = Merge20BulkSimpleThresholdDefault; /**< Positional fallback when trimmed-middle n*m exceeds this (OPT_MERGE20_SIMPLE_THRESHOLD; 0 disables the fallback). */
};

/** @brief Result of one bulk diff. */
struct Merge20BulkDiffResult
{
	std::vector<Merge20BulkRange> ranges; /**< Diff ranges in file order (equal lines produce no ranges). */
	bool simpleUsed = false; /**< True when the positional fallback ran (n*m > threshold). */
};

/**
 * @brief Diff two in-memory line vectors (HTML `diffFast` port).
 * @param [in] oldLines Old-side lines (0-based, in file order).
 * @param [in] newLines New-side lines (0-based, in file order).
 * @param [in] options Compare options (whitespace / case / blank lines / threshold).
 * @return Diff ranges in file order plus whether the simple fallback ran.
 */
Merge20BulkDiffResult Merge20BulkDiff(const std::vector<String>& oldLines, const std::vector<String>& newLines,
	const Merge20BulkDiffOptions& options);

/** @brief One line reader for the simple-compare check: returns the raw line text, or false when the line does not exist. */
using Merge20BulkLineReader = std::function<bool(int line, String& text)>;

/**
 * @brief True when the simple (positional) fallback would run for two line sequences (H-SIMPLE).
 * @param [in] getOldLine Reader for old-side lines (0-based).
 * @param [in] oldCount Old-side line count.
 * @param [in] getNewLine Reader for new-side lines (0-based).
 * @param [in] newCount New-side line count.
 * @param [in] options Compare options (whitespace / case / threshold; blank lines do not affect the trim).
 * @return True when the trimmed-middle line counts satisfy n*m > threshold (HTML `lcs`, threshold 0 disables).
 *
 * The trim is the HTML `diffFast` one: the common prefix/suffix on
 * normalized lines is stripped first, so files that are huge but
 * differ only in a small middle do NOT trigger the fallback. The
 * readers are only asked for prefix/suffix lines (the scan stops
 * at the first mismatch), so an opened merge doc can answer from
 * its text buffers without copying all lines into vectors. This
 * is the single place that decides the threshold, shared by the
 * bulk differ (below) and the opened-file path
 * (Merge20ClassifyIsSimpleCompare): both must agree, or the
 * [Simple] badge would flicker when a bulk-classified file is
 * opened.
 */
bool Merge20BulkIsSimple(const Merge20BulkLineReader& getOldLine, int oldCount,
	const Merge20BulkLineReader& getNewLine, int newCount,
	const Merge20BulkDiffOptions& options);
