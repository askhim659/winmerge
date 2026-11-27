// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Verify.h
 *
 * @brief Output-time final verification engine (Merge20
 * best-workflow B7, pillar 4: "verify before output", gap
 * analysis section 4.2 B7).
 *
 * Pure module (no MFC, no `_()`): the four checks that run
 * automatically just before a merge result is written out, and
 * the issue list they produce. The same code is compiled into
 * WinMergeU and into the GoogleTest runner.
 *
 * The four checks (gap analysis section 4.2 B7):
 * - (i) Rediff: diff the merge result (saved form) against NEW
 *   again, with an independent line diff, and report differences
 *   that no conclusion or segment state explains.
 * - (ii) Line count / encoding: the saved line count must match
 *   the count implied by NEW plus the per-diff deltas, every
 *   result line must be valid UTF-8, and every character must be
 *   representable in the target encoding (S0 round-trip idea
 *   applied to the real output: the caller supplies the
 *   representability predicate, so the engine stays free of
 *   codepage tables).
 * - (iii) Syntax (simple check only): bracket pairing and
 *   invalid bytes. Deterministic and simple only; no real parsing.
 *   A bracket problem is reported only when BOTH inputs balance,
 *   so an imbalance the merge did not introduce is never blamed
 *   on the merge. The dialog must say this is a simple check, not
 *   a syntax guarantee.
 * - (iv) Consistency: for every diff with a B1 conclusion, the
 *   saved segment content must be the content that conclusion
 *   implies (Merge20ConclusionSrcPanes). Undecided diffs and Keep
 *   conclusions have no expected content and are skipped.
 *
 * The engine never decides what "NEW" or "the result" is: the
 * caller (Merge20VerifyGate, the MFC adapter) builds the input
 * from the merge doc -- pane lines by real line number, the
 * saved-form result lines (conflict sections expanded, "<No
 * Lines>" display lines dropped, exactly as SaveMergeResult
 * writes them), one record per significant diff with its saved
 * line range. The engine only judges the lines it is given.
 *
 * Issue reporting: issues carry structured data (check, kind,
 * diff index, 1-based line numbers, counts, the offending line
 * text), never translated sentences. The dialog composes the
 * Japanese text from these fields with `_()` (the S4 precedent:
 * Merge20Classify returns data, the caller formats it), so the
 * engine file stays ASCII and every display string goes through
 * Translations/WinMerge/Japanese.po.
 *
 * Known limitation of (i): a line diff aligns equal lines by
 * content, so when a diff inserts a line that also occurs on the
 * neighbouring common line (e.g. UseBoth inserting a line equal
 * to the next common line), the hunk can be attributed one line
 * into the common region. The dialog shows the exact lines, and
 * offers "Output anyway" as an explicit override, so a spurious
 * row never silently blocks a correct output.
 */
#pragma once

#include <functional>
#include <string>
#include <vector>

/**
 * @brief The four B7 verification checks, in gap-analysis order.
 */
enum class Merge20VerifyCheck
{
	Rediff = 0,          /**< (i) result vs NEW re-diff */
	LineCountEncoding,   /**< (ii) line count + encoding */
	Syntax,              /**< (iii) simple syntax check */
	Consistency,         /**< (iv) conclusion vs output */
};

/**
 * @brief Segment states, mirrored from ResultSegmentState
 * (Src/MergeResultPane.h: Common, Auto, Chosen, Unresolved,
 * Conflict, Edited). The pure module must not include that
 * header, so the values are mirrored here; the gate adapter
 * static-asserts the mirror against the real enum.
 */
constexpr int Merge20VerifySegCommon = 0;
constexpr int Merge20VerifySegAuto = 1;
constexpr int Merge20VerifySegChosen = 2;
constexpr int Merge20VerifySegUnresolved = 3;
constexpr int Merge20VerifySegConflict = 4;
constexpr int Merge20VerifySegEdited = 5;

/**
 * @brief What one issue is, so the dialog can render it.
 */
enum class Merge20VerifyIssueKind
{
	RediffUnexplained,       /**< (i) result differs from NEW where it must equal NEW */
	LineCountMismatch,       /**< (ii) saved line count differs from the expected count */
	EncodingUnrepresentable, /**< (ii) a character cannot be written in the target encoding */
	EncodingInvalidUtf8,     /**< (ii)/(iii) a result line is not valid UTF-8 */
	SyntaxUnmatchedCloser,   /**< (iii) closer without an opener */
	SyntaxUnclosedOpener,    /**< (iii) opener never closed */
	SyntaxMismatchedPair,    /**< (iii) closer of a different kind than the opener */
	ConsistencyMismatch,     /**< (iv) concluded diff's content differs from its conclusion */
	ConsistencyUnresolved,   /**< (iv) concluded diff is still Unresolved/Conflict */
};

/**
 * @brief One verification problem, as structured data.
 */
struct Merge20VerifyIssue
{
	Merge20VerifyCheck check = Merge20VerifyCheck::Rediff; /**< Which check found it */
	Merge20VerifyIssueKind kind = Merge20VerifyIssueKind::RediffUnexplained; /**< What it is */
	int diffIndex = -1;     /**< Diff index (0-based), or -1 when not tied to a diff */
	int resultLine = -1;    /**< 1-based result line, or -1 when not applicable */
	int newLine = -1;       /**< 1-based NEW line, or -1 when not applicable */
	int expectedCount = -1; /**< Line-count check: expected lines; mismatch: expected lines */
	int actualCount = -1;   /**< Line-count check: actual lines; mismatch: actual lines */
	/** @brief Offending line content (UTF-8, no EOL), a bracket
	 * character for syntax issues, or empty when not applicable.
	 * File content, not UI text: shown verbatim, never translated. */
	std::string text;
	/** @brief Code point that cannot be represented (encoding
	 * issues), or -1. The dialog renders it as U+XXXX. */
	int codePoint = -1;
	/** @brief Byte offset of the first invalid byte (invalid-UTF-8
	 * issues), or -1. */
	int byteOffset = -1;
};

/**
 * @brief One difference, as the engine needs it.
 *
 * Line ranges are inclusive real line numbers (DIFFRANGE
 * begin/end semantics: end - begin + 1 lines, 0 lines when
 * end < begin, e.g. -1/-2 for an empty side). savedBegin /
 * savedLines locate the diff's content in resultLines (the
 * saved form): savedBegin is 0-based, savedLines is the count
 * of saved-form lines (0 for a "<No Lines>" segment, the
 * conflict-section line count for Conflict/Unresolved).
 * diffs must be sorted by savedBegin.
 */
struct Merge20VerifyDiffInput
{
	int diffIndex = -1;     /**< Index in the diff list */
	int op = 0;             /**< DIFFRANGE op (Merge20ConclusionOp*) */
	int oldBegin = -1;      /**< First OLD line (inclusive) */
	int oldEnd = -2;        /**< Last OLD line (inclusive) */
	int newBegin = -1;      /**< First NEW line (inclusive) */
	int newEnd = -2;        /**< Last NEW line (inclusive) */
	int thirdBegin = -1;    /**< First third-pane line (3-way only) */
	int thirdEnd = -2;      /**< Last third-pane line (3-way only) */
	int conclusion = 0;     /**< Merge20DiffConclusion ordinal */
	int segState = Merge20VerifySegUnresolved; /**< ResultSegmentState mirror */
	/** @brief Source panes of the segment content (MergeResultSegment::srcPanes). */
	std::vector<int> srcPanes;
	int savedBegin = 0;     /**< First saved-form result line (0-based) */
	int savedLines = 0;     /**< Saved-form result lines for this diff */
};

/**
 * @brief One file to verify: the compared panes, the merge
 * result in saved form, and the diff records.
 */
struct Merge20VerifyFileInput
{
	/** @brief Compared panes, UTF-8 lines without EOLs, indexed
	 * by real line number (0-based). panes[newPane] is NEW. */
	std::vector<std::vector<std::string>> panes;
	int newPane = 1;        /**< Index of the NEW pane */
	/** @brief Merge result in saved form (conflict sections
	 * expanded, "<No Lines>" display lines dropped), UTF-8 lines
	 * without EOLs. This is what will be written to the file. */
	std::vector<std::string> resultLines;
	/** @brief One record per diff (sorted by savedBegin). Empty
	 * when there is no merge session: then only the rediff of
	 * result vs NEW and the line-count/encoding/syntax checks
	 * apply, and (iv) has nothing to check. */
	std::vector<Merge20VerifyDiffInput> diffs;
	/** @brief Compare options in effect for this comparison:
	 * whitespace-only / case-only differences do not count as
	 * differences in the re-diff (the user compares with
	 * whitespace and case ignored by default). */
	bool ignoreWhitespace = true;
	bool ignoreCase = true;
	/** @brief Whether one code point can be written in the target
	 * (output) encoding. Supplied by the caller, which owns the
	 * codepage tables (S0 round-trip idea); null skips the
	 * representability part of (ii). Only called with valid
	 * scalar values (never surrogates: strict UTF-8 decoding
	 * rejects those first). */
	std::function<bool(char32_t)> isRepresentable;
};

/**
 * @brief Per-check ON/OFF switches (option keys
 * Merge20/VerifyRediff etc., all default ON). The engine reads
 * the struct, never the options manager: the gate adapter loads
 * the keys, the tests pass the struct directly.
 */
struct Merge20VerifyOptions
{
	bool rediff = true;            /**< (i) */
	bool lineCountEncoding = true; /**< (ii) */
	bool syntax = true;            /**< (iii) */
	bool consistency = true;       /**< (iv) */
};

/**
 * @brief Outcome of one check.
 */
struct Merge20VerifyCheckResult
{
	Merge20VerifyCheck check = Merge20VerifyCheck::Rediff;
	bool enabled = false;   /**< False when the option switched it off */
	bool passed = true;     /**< False when the check found an issue */
	int issueCount = 0;     /**< Issues from this check */
	long long elapsedMs = 0; /**< Time spent in this check */
};

/**
 * @brief Outcome of verifying one file.
 */
struct Merge20VerifyResult
{
	bool passed = true;     /**< True when no enabled check found an issue */
	std::vector<Merge20VerifyIssue> issues; /**< All issues, check order then line order */
	std::vector<Merge20VerifyCheckResult> checks; /**< One per check, in enum order */
	long long elapsedMs = 0; /**< Total time for the file */
	int resultLineCount = 0;  /**< resultLines.size() */
	int expectedLineCount = 0; /**< (ii) expected saved line count */
};

/**
 * @brief Run the enabled checks over one file.
 * @param [in] input Compared panes, saved-form result lines and
 *             diff records (see Merge20VerifyFileInput).
 * @param [in] options Per-check ON/OFF switches.
 * @return The issues found (empty when clean) plus per-check
 *         outcomes and timings. Never throws on bad input:
 *         out-of-range indices are treated as empty ranges, so a
 *         stale segment table degrades to extra issues, not a
 *         crash.
 */
Merge20VerifyResult Merge20VerifyRunFile(const Merge20VerifyFileInput& input, const Merge20VerifyOptions& options);
