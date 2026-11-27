// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Verify.cpp
 *
 * @brief Implementation of the output-time final verification
 * engine (Merge20 best-workflow B7).
 *
 * The four checks are independent functions over the input
 * lines; Merge20VerifyRunFile only times them and assembles the
 * result. The re-diff (i) is a real line diff (Hirschberg LCS
 * over interned normalized lines, so equal lines are matched by
 * content, not by position), and a hunk is a problem only where
 * the result is supposed to equal NEW: common text, a
 * UseNew-concluded diff, or a segment whose sources are exactly
 * the NEW pane. Everywhere else the deviation is the merge
 * decision itself (UseOld / UseBoth / Reject / Keep, an
 * auto-merge choice, a hand edit, or a conflict section), and
 * the re-diff stays silent there.
 *
 * This file stays ASCII: issues carry structured data, the
 * dialog composes the display text.
 */
#include "pch.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include "Merge20/Merge20Verify.h"
#include "Merge20/Merge20Conclusion.h"

namespace
{

using Clock = std::chrono::steady_clock;

/** @brief Lines in a range [begin, end] (inclusive), 0 when empty. */
int RangeCount(int begin, int end)
{
	const int count = end - begin + 1;
	return count > 0 ? count : 0;
}

/** @brief Whether a conclusion ordinal is a real B1 state. */
bool IsValidConclusion(int conclusion)
{
	return conclusion >= static_cast<int>(Merge20DiffConclusion::Undecided) &&
		conclusion <= static_cast<int>(Merge20DiffConclusion::Keep);
}

/**
 * @brief Strict UTF-8 decode of one line (no EOL).
 * @param [out] codePoints Decoded scalar values (only when true).
 * @param [out] badOffset Byte offset of the first invalid byte (only when false).
 * @return False on overlong forms, surrogates, values above
 *         U+10FFFF, stray continuation bytes and truncated
 *         sequences. Valid ASCII and multibyte text decodes
 *         exactly; the decoder never accepts CESU-8 surrogates.
 */
bool DecodeUtf8Strict(const std::string& line, std::vector<char32_t>& codePoints, int& badOffset)
{
	codePoints.clear();
	badOffset = -1;
	const auto* bytes = reinterpret_cast<const unsigned char*>(line.data());
	const size_t len = line.size();
	size_t i = 0;
	while (i < len)
	{
		const unsigned char b0 = bytes[i];
		if (b0 < 0x80)
		{
			codePoints.push_back(b0);
			++i;
			continue;
		}
		int extra = 0;
		char32_t cp = 0;
		char32_t minCp = 0;
		if ((b0 & 0xE0) == 0xC0)
		{
			extra = 1;
			cp = b0 & 0x1F;
			minCp = 0x80;
		}
		else if ((b0 & 0xF0) == 0xE0)
		{
			extra = 2;
			cp = b0 & 0x0F;
			minCp = 0x800;
		}
		else if ((b0 & 0xF8) == 0xF0)
		{
			extra = 3;
			cp = b0 & 0x07;
			minCp = 0x10000;
		}
		else
		{
			badOffset = static_cast<int>(i);
			return false;
		}
		if (i + static_cast<size_t>(extra) >= len)
		{
			badOffset = static_cast<int>(i);
			return false;
		}
		for (int k = 1; k <= extra; ++k)
		{
			const unsigned char bk = bytes[i + static_cast<size_t>(k)];
			if ((bk & 0xC0) != 0x80)
			{
				badOffset = static_cast<int>(i + static_cast<size_t>(k));
				return false;
			}
			cp = static_cast<char32_t>((cp << 6) | (bk & 0x3F));
		}
		if (cp < minCp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
		{
			badOffset = static_cast<int>(i);
			return false;
		}
		codePoints.push_back(cp);
		i += static_cast<size_t>(extra) + 1;
	}
	return true;
}

/**
 * @brief Comparison form of a line under the active compare
 * options: whitespace removed and ASCII case folded when the
 * options say so. Only ASCII whitespace and case are touched:
 * the compare options' ignore rules are ASCII-based, and
 * non-ASCII bytes pass through unchanged.
 */
std::string NormalizeLine(const std::string& line, bool ignoreWhitespace, bool ignoreCase)
{
	if (!ignoreWhitespace && !ignoreCase)
		return line;
	std::string out;
	out.reserve(line.size());
	for (const unsigned char ch : line)
	{
		if (ignoreWhitespace && (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\v' || ch == '\f'))
			continue;
		if (ignoreCase && ch >= 'A' && ch <= 'Z')
			out.push_back(static_cast<char>(ch + ('a' - 'A')));
		else
			out.push_back(static_cast<char>(ch));
	}
	return out;
}

/**
 * @brief Intern normalized lines to integer ids, so the LCS
 * compares ids (exact, no hash collisions) instead of strings.
 * The id map is shared by the caller across BOTH sides of one
 * diff (result and NEW): interning each side with its own map
 * would give the same line different ids on the two sides and
 * the LCS would match lines by accident of first-seen order.
 */
std::vector<int> InternLines(const std::vector<std::string>& lines, bool ignoreWhitespace, bool ignoreCase,
	std::unordered_map<std::string, int>& ids)
{
	std::vector<int> out;
	out.reserve(lines.size());
	for (const std::string& line : lines)
	{
		std::string norm = NormalizeLine(line, ignoreWhitespace, ignoreCase);
		auto it = ids.find(norm);
		if (it == ids.end())
		{
			const int id = static_cast<int>(ids.size());
			ids.emplace(std::move(norm), id);
			out.push_back(id);
		}
		else
		{
			out.push_back(it->second);
		}
	}
	return out;
}

/** @brief One differing hunk of the re-diff, half-open ranges. */
struct DiffHunk
{
	int aBegin = 0; /**< First result line (0-based) */
	int aEnd = 0;   /**< One past the last result line */
	int bBegin = 0; /**< First NEW line (0-based) */
	int bEnd = 0;   /**< One past the last NEW line */
};

/**
 * @brief Last LCS-length row for a[aBegin, aEnd) vs b[bBegin, bEnd).
 * Two rows only: O(|b|) memory.
 */
std::vector<int> LcsRowForward(const std::vector<int>& a, int aBegin, int aEnd,
	const std::vector<int>& b, int bBegin, int bEnd)
{
	const int n = bEnd - bBegin;
	std::vector<int> prev(static_cast<size_t>(n) + 1, 0), cur(static_cast<size_t>(n) + 1, 0);
	for (int i = aBegin; i < aEnd; ++i)
	{
		cur[0] = 0;
		for (int j = 0; j < n; ++j)
		{
			cur[static_cast<size_t>(j) + 1] = (a[static_cast<size_t>(i)] == b[static_cast<size_t>(bBegin + j)])
				? prev[static_cast<size_t>(j)] + 1
				: (std::max)(prev[static_cast<size_t>(j) + 1], cur[static_cast<size_t>(j)]);
		}
		prev.swap(cur);
	}
	return prev;
}

/**
 * @brief Last LCS-length row for the reversed suffixes, i.e.
 * a[aBegin, aEnd) vs b[bBegin, bEnd) read backwards. The caller
 * indexes the returned row by "lines taken from the end of b".
 */
std::vector<int> LcsRowBackward(const std::vector<int>& a, int aBegin, int aEnd,
	const std::vector<int>& b, int bBegin, int bEnd)
{
	const int n = bEnd - bBegin;
	std::vector<int> prev(static_cast<size_t>(n) + 1, 0), cur(static_cast<size_t>(n) + 1, 0);
	for (int i = aEnd - 1; i >= aBegin; --i)
	{
		cur[0] = 0;
		for (int j = 0; j < n; ++j)
		{
			cur[static_cast<size_t>(j) + 1] = (a[static_cast<size_t>(i)] == b[static_cast<size_t>(bEnd - 1 - j)])
				? prev[static_cast<size_t>(j)] + 1
				: (std::max)(prev[static_cast<size_t>(j) + 1], cur[static_cast<size_t>(j)]);
		}
		prev.swap(cur);
	}
	return prev;
}

/** @brief Hirschberg LCS diff; appends hunks in order. */
void HirschbergDiff(const std::vector<int>& a, int aBegin, int aEnd,
	const std::vector<int>& b, int bBegin, int bEnd, std::vector<DiffHunk>& hunks)
{
	if (aBegin >= aEnd)
	{
		if (bBegin < bEnd)
			hunks.push_back({ aBegin, aBegin, bBegin, bEnd });
		return;
	}
	if (bBegin >= bEnd)
	{
		hunks.push_back({ aBegin, aEnd, bBegin, bBegin });
		return;
	}
	if (aEnd - aBegin == 1)
	{
		// One result line: find it in the NEW range, if it is there.
		int found = -1;
		for (int j = bBegin; j < bEnd; ++j)
		{
			if (a[static_cast<size_t>(aBegin)] == b[static_cast<size_t>(j)])
			{
				found = j;
				break;
			}
		}
		if (found < 0)
		{
			hunks.push_back({ aBegin, aEnd, bBegin, bEnd });
			return;
		}
		if (found > bBegin)
			hunks.push_back({ aBegin, aBegin, bBegin, found });
		if (found + 1 < bEnd)
			hunks.push_back({ aBegin + 1, aBegin + 1, found + 1, bEnd });
		return;
	}
	const int aMid = aBegin + (aEnd - aBegin) / 2;
	const std::vector<int> rowF = LcsRowForward(a, aBegin, aMid, b, bBegin, bEnd);
	const std::vector<int> rowB = LcsRowBackward(a, aMid, aEnd, b, bBegin, bEnd);
	const int n = bEnd - bBegin;
	int bestK = 0;
	int bestSum = -1;
	for (int k = 0; k <= n; ++k)
	{
		const int sum = rowF[static_cast<size_t>(k)] + rowB[static_cast<size_t>(n - k)];
		if (sum > bestSum)
		{
			bestSum = sum;
			bestK = k;
		}
	}
	HirschbergDiff(a, aBegin, aMid, b, bBegin, bBegin + bestK, hunks);
	HirschbergDiff(a, aMid, aEnd, b, bBegin + bestK, bEnd, hunks);
}

/**
 * @brief Line diff between result (a) and NEW (b), as hunks.
 * Common prefix/suffix are trimmed first; when the trimmed
 * middle is too large for an exact diff, it is reported as a
 * single coarse hunk instead of spending unbounded time.
 */
std::vector<DiffHunk> ComputeRediffHunks(const std::vector<int>& aIds, const std::vector<int>& bIds)
{
	const int aSize = static_cast<int>(aIds.size());
	const int bSize = static_cast<int>(bIds.size());
	int aBegin = 0, bBegin = 0;
	while (aBegin < aSize && bBegin < bSize && aIds[static_cast<size_t>(aBegin)] == bIds[static_cast<size_t>(bBegin)])
	{
		++aBegin;
		++bBegin;
	}
	int aEnd = aSize, bEnd = bSize;
	while (aEnd > aBegin && bEnd > bBegin && aIds[static_cast<size_t>(aEnd - 1)] == bIds[static_cast<size_t>(bEnd - 1)])
	{
		--aEnd;
		--bEnd;
	}
	std::vector<DiffHunk> hunks;
	if (aBegin >= aEnd && bBegin >= bEnd)
		return hunks;
	// Exact-diff budget: 200M DP cells (a few hundred ms worst
	// case). Above it, one coarse hunk over the middle: the
	// explained-test below still localises it against the diff
	// ranges, so a huge rewrite is one row, not silence.
	const long long cells = static_cast<long long>(aEnd - aBegin) * (bEnd - bBegin);
	if (cells > 200000000LL)
	{
		hunks.push_back({ aBegin, aEnd, bBegin, bEnd });
		return hunks;
	}
	HirschbergDiff(aIds, aBegin, aEnd, bIds, bBegin, bEnd, hunks);
	return hunks;
}

/** @brief Lines of one pane's range for a source pane. */
std::vector<std::string> PaneRangeLines(const Merge20VerifyFileInput& input, int pane, int begin, int end)
{
	std::vector<std::string> lines;
	if (pane < 0 || pane >= static_cast<int>(input.panes.size()))
		return lines;
	const std::vector<std::string>& paneLines = input.panes[static_cast<size_t>(pane)];
	const int count = RangeCount(begin, end);
	if (count <= 0)
		return lines;
	for (int i = 0; i < count; ++i)
	{
		const int line = begin + i;
		if (line >= 0 && line < static_cast<int>(paneLines.size()))
			lines.push_back(paneLines[static_cast<size_t>(line)]);
	}
	return lines;
}

/** @brief Range of one pane for a source pane of a diff. */
void DiffPaneRange(const Merge20VerifyDiffInput& diff, int pane, int newPane, int& begin, int& end)
{
	if (pane == 0)
	{
		begin = diff.oldBegin;
		end = diff.oldEnd;
	}
	else if (pane == newPane)
	{
		begin = diff.newBegin;
		end = diff.newEnd;
	}
	else
	{
		begin = diff.thirdBegin;
		end = diff.thirdEnd;
	}
}

/** @brief Whether the diff's result range must equal NEW. A
 * Conflict/Unresolved segment saves the conflict section, not
 * NEW content, and an Edited segment is a deliberate hand edit:
 * neither is a claim that the result equals NEW, so the re-diff
 * stays silent there and the consistency check judges them. */
bool DiffMustEqualNew(const Merge20VerifyDiffInput& diff, int newPane)
{
	if (diff.op == Merge20ConclusionOpNone)
		return true;
	if (diff.segState == Merge20VerifySegConflict || diff.segState == Merge20VerifySegUnresolved ||
		diff.segState == Merge20VerifySegEdited)
		return false;
	if (diff.conclusion == static_cast<int>(Merge20DiffConclusion::UseNew))
		return true;
	if ((diff.segState == Merge20VerifySegAuto || diff.segState == Merge20VerifySegChosen) &&
		diff.srcPanes.size() == 1 && diff.srcPanes[0] == newPane)
		return true;
	return false;
}

/** @brief Whether a diff's deviation from NEW is explained by its conclusion/state. */
bool DiffDeviationExplained(const Merge20VerifyDiffInput& diff, int newPane)
{
	return !DiffMustEqualNew(diff, newPane);
}

/** @brief One bracket problem found by the scanner. */
struct BracketHit
{
	int line = 0;      /**< 1-based line */
	int kind = 0;      /**< 0 unmatched closer, 1 unclosed opener, 2 mismatched pair */
	char bracket = 0;  /**< The bracket character involved */
};

char MatchingOpener(char closer)
{
	switch (closer)
	{
	case ')': return '(';
	case ']': return '[';
	case '}': return '{';
	default: return 0;
	}
}

bool IsOpener(char ch)
{
	return ch == '(' || ch == '[' || ch == '{';
}

bool IsCloser(char ch)
{
	return ch == ')' || ch == ']' || ch == '}';
}

/**
 * @brief Bracket scan of one file's lines. Strings ("...",
 * '...'), // comments and block comments are skipped, so a
 * bracket inside them never counts. Byte-wise: brackets,
 * quotes and comment markers are ASCII, and UTF-8 continuation
 * bytes never equal them. Unterminated quotes end at the line
 * end (an apostrophe in prose must not swallow the file).
 */
std::vector<BracketHit> ScanBrackets(const std::vector<std::string>& lines)
{
	std::vector<BracketHit> hits;
	std::vector<std::pair<char, int>> stack; // (opener, 1-based line)
	bool inBlockComment = false;
	for (size_t li = 0; li < lines.size(); ++li)
	{
		const std::string& line = lines[li];
		const int lineNo = static_cast<int>(li) + 1;
		bool inDouble = false;
		bool inSingle = false;
		size_t i = 0;
		while (i < line.size())
		{
			const char c = line[i];
			if (inBlockComment)
			{
				if (c == '*' && i + 1 < line.size() && line[i + 1] == '/')
				{
					inBlockComment = false;
					i += 2;
				}
				else
				{
					++i;
				}
				continue;
			}
			if (inDouble)
			{
				if (c == '\\' && i + 1 < line.size())
					i += 2;
				else if (c == '"')
				{
					inDouble = false;
					++i;
				}
				else
				{
					++i;
				}
				continue;
			}
			if (inSingle)
			{
				if (c == '\\' && i + 1 < line.size())
					i += 2;
				else if (c == '\'')
				{
					inSingle = false;
					++i;
				}
				else
				{
					++i;
				}
				continue;
			}
			if (c == '/' && i + 1 < line.size() && line[i + 1] == '/')
				break;
			if (c == '/' && i + 1 < line.size() && line[i + 1] == '*')
			{
				inBlockComment = true;
				i += 2;
				continue;
			}
			if (c == '"')
			{
				inDouble = true;
				++i;
				continue;
			}
			if (c == '\'')
			{
				inSingle = true;
				++i;
				continue;
			}
			if (IsOpener(c))
			{
				stack.emplace_back(c, lineNo);
				++i;
				continue;
			}
			if (IsCloser(c))
			{
				if (stack.empty())
				{
					hits.push_back({ lineNo, 0, c });
				}
				else if (stack.back().first == MatchingOpener(c))
				{
					stack.pop_back();
				}
				else
				{
					hits.push_back({ lineNo, 2, c });
					stack.pop_back();
				}
				++i;
				continue;
			}
			++i;
		}
	}
	for (const auto& [opener, lineNo] : stack)
		hits.push_back({ lineNo, 1, opener });
	// Report in file order (unclosed openers come last by
	// construction; sort by line, keeping kind order on ties).
	std::stable_sort(hits.begin(), hits.end(),
		[](const BracketHit& a, const BracketHit& b) { return a.line < b.line; });
	return hits;
}

/** @brief Elapsed milliseconds between two time points. */
long long MsBetween(Clock::time_point from, Clock::time_point to)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count();
}

constexpr int kMaxIssuesPerCheck = 20;
constexpr int kMaxIssuesTotal = 50;

/** @brief Append an issue unless the total cap is reached. */
void AddIssue(std::vector<Merge20VerifyIssue>& issues, Merge20VerifyIssue issue)
{
	if (static_cast<int>(issues.size()) < kMaxIssuesTotal)
		issues.push_back(std::move(issue));
}

/** @brief (i) Rediff: result vs NEW, unexplained hunks only. */
std::vector<Merge20VerifyIssue> CheckRediff(const Merge20VerifyFileInput& input)
{
	std::vector<Merge20VerifyIssue> issues;
	if (input.newPane < 0 || input.newPane >= static_cast<int>(input.panes.size()))
		return issues;
	const std::vector<std::string>& newLines = input.panes[static_cast<size_t>(input.newPane)];
	// One shared id map for both sides, so equal lines get equal
	// ids (see InternLines).
	std::unordered_map<std::string, int> lineIds;
	const std::vector<int> aIds = InternLines(input.resultLines, input.ignoreWhitespace, input.ignoreCase, lineIds);
	const std::vector<int> bIds = InternLines(newLines, input.ignoreWhitespace, input.ignoreCase, lineIds);
	const std::vector<DiffHunk> hunks = ComputeRediffHunks(aIds, bIds);
	const int resultSize = static_cast<int>(input.resultLines.size());

	// Must-equal intervals in result coordinates: everything,
	// minus each diff's saved range whose deviation is explained.
	// Intervals are half-open, sorted, non-overlapping.
	std::vector<std::pair<int, int>> mustEqual;
	mustEqual.emplace_back(0, resultSize);
	for (const Merge20VerifyDiffInput& diff : input.diffs)
	{
		if (!DiffDeviationExplained(diff, input.newPane))
			continue;
		const int s = diff.savedBegin;
		const int e = diff.savedBegin + diff.savedLines;
		std::vector<std::pair<int, int>> next;
		for (const auto& [ms, me] : mustEqual)
		{
			if (e <= ms || s >= me)
			{
				next.emplace_back(ms, me);
				continue;
			}
			if (ms < s)
				next.emplace_back(ms, s);
			if (e < me)
				next.emplace_back(e, me);
		}
		mustEqual.swap(next);
	}
	auto findDiffAt = [&](int resultLine0) -> int
	{
		for (const Merge20VerifyDiffInput& diff : input.diffs)
		{
			if (resultLine0 >= diff.savedBegin && resultLine0 < diff.savedBegin + diff.savedLines)
				return diff.diffIndex;
		}
		return -1;
	};
	// The diff a missing NEW line belongs to (pure deletions
	// have no result line to attribute them by).
	auto findDiffAtNew = [&](int newLine0) -> int
	{
		for (const Merge20VerifyDiffInput& diff : input.diffs)
		{
			if (newLine0 >= diff.newBegin && newLine0 <= diff.newEnd)
				return diff.diffIndex;
		}
		return -1;
	};
	auto positionMustEqual = [&](int pos) -> bool
	{
		// The position between lines: must-equal when the line at
		// the position is, or (at the very end) the last line is.
		int line = pos;
		if (line >= resultSize)
			line = resultSize - 1;
		if (line < 0)
			return false;
		for (const auto& [ms, me] : mustEqual)
		{
			if (line >= ms && line < me)
				return true;
		}
		return false;
	};

	for (const DiffHunk& hunk : hunks)
	{
		if (static_cast<int>(issues.size()) >= kMaxIssuesPerCheck)
			break;
		if (hunk.aBegin >= hunk.aEnd)
		{
			// Pure deletion: NEW lines missing from the result.
			// The gap position must be a must-equal position.
			if (!positionMustEqual(hunk.aBegin))
				continue;
			Merge20VerifyIssue issue;
			issue.check = Merge20VerifyCheck::Rediff;
			issue.kind = Merge20VerifyIssueKind::RediffUnexplained;
			issue.diffIndex = findDiffAtNew(hunk.bBegin);
			issue.resultLine = hunk.aBegin + 1;
			issue.newLine = hunk.bBegin + 1;
			if (hunk.bBegin < static_cast<int>(newLines.size()))
				issue.text = newLines[static_cast<size_t>(hunk.bBegin)];
			AddIssue(issues, std::move(issue));
			continue;
		}
		// Intersect the hunk's result range with must-equal.
		for (const auto& [ms, me] : mustEqual)
		{
			const int s = (std::max)(hunk.aBegin, ms);
			const int e = (std::min)(hunk.aEnd, me);
			if (s >= e)
				continue;
			if (static_cast<int>(issues.size()) >= kMaxIssuesPerCheck)
				break;
			Merge20VerifyIssue issue;
			issue.check = Merge20VerifyCheck::Rediff;
			issue.kind = Merge20VerifyIssueKind::RediffUnexplained;
			issue.diffIndex = findDiffAt(s);
			issue.resultLine = s + 1;
			issue.newLine = hunk.bBegin + (s - hunk.aBegin) + 1;
			if (s < resultSize)
				issue.text = input.resultLines[static_cast<size_t>(s)];
			AddIssue(issues, std::move(issue));
		}
	}
	return issues;
}

/** @brief (ii) Line count + encoding. */
std::vector<Merge20VerifyIssue> CheckLineCountEncoding(const Merge20VerifyFileInput& input, int& expectedLineCount)
{
	std::vector<Merge20VerifyIssue> issues;
	expectedLineCount = 0;
	const bool haveNewPane = input.newPane >= 0 && input.newPane < static_cast<int>(input.panes.size());
	if (haveNewPane)
	{
		const std::vector<std::string>& newLines = input.panes[static_cast<size_t>(input.newPane)];
		expectedLineCount = static_cast<int>(newLines.size());
		for (const Merge20VerifyDiffInput& diff : input.diffs)
		{
			if (diff.op == Merge20ConclusionOpNone)
				continue;
			expectedLineCount += diff.savedLines - RangeCount(diff.newBegin, diff.newEnd);
		}
	}
	const int actualLineCount = static_cast<int>(input.resultLines.size());
	// Without a NEW pane there is no expectation to compare
	// against; the count part stays silent instead of inventing
	// a mismatch (the caller always supplies a NEW pane).
	if (haveNewPane && expectedLineCount != actualLineCount)
	{
		Merge20VerifyIssue issue;
		issue.check = Merge20VerifyCheck::LineCountEncoding;
		issue.kind = Merge20VerifyIssueKind::LineCountMismatch;
		issue.expectedCount = expectedLineCount;
		issue.actualCount = actualLineCount;
		issues.push_back(std::move(issue));
	}

	// Encoding: strict UTF-8 first, then representability of
	// every non-ASCII code point (ASCII is representable in every
	// target encoding). At most 10 lines per kind, so one broken
	// file does not flood the dialog.
	int invalidCount = 0;
	int unrepresentableCount = 0;
	for (size_t li = 0; li < input.resultLines.size(); ++li)
	{
		const std::string& line = input.resultLines[li];
		std::vector<char32_t> cps;
		int badOffset = -1;
		if (!DecodeUtf8Strict(line, cps, badOffset))
		{
			if (invalidCount < 10)
			{
				Merge20VerifyIssue issue;
				issue.check = Merge20VerifyCheck::LineCountEncoding;
				issue.kind = Merge20VerifyIssueKind::EncodingInvalidUtf8;
				issue.resultLine = static_cast<int>(li) + 1;
				issue.text = line;
				issue.byteOffset = badOffset;
				issues.push_back(std::move(issue));
			}
			++invalidCount;
			continue;
		}
		if (input.isRepresentable == nullptr)
			continue;
		for (const char32_t cp : cps)
		{
			if (cp < 0x80)
				continue;
			if (!input.isRepresentable(cp))
			{
				if (unrepresentableCount < 10)
				{
					Merge20VerifyIssue issue;
					issue.check = Merge20VerifyCheck::LineCountEncoding;
					issue.kind = Merge20VerifyIssueKind::EncodingUnrepresentable;
					issue.resultLine = static_cast<int>(li) + 1;
					issue.text = line;
					issue.codePoint = static_cast<int>(cp);
					issues.push_back(std::move(issue));
				}
				++unrepresentableCount;
				break;
			}
		}
		if (static_cast<int>(issues.size()) >= kMaxIssuesPerCheck)
			break;
	}
	return issues;
}

/** @brief (iii) Simple syntax check: brackets + invalid bytes. */
std::vector<Merge20VerifyIssue> CheckSyntax(const Merge20VerifyFileInput& input)
{
	std::vector<Merge20VerifyIssue> issues;
	// Invalid bytes are already found by (ii) decoding, but the
	// syntax check owns the wording for its own row: re-decode
	// here so disabling (ii) does not silence bad bytes.
	for (size_t li = 0; li < input.resultLines.size(); ++li)
	{
		std::vector<char32_t> cps;
		int badOffset = -1;
		if (!DecodeUtf8Strict(input.resultLines[li], cps, badOffset))
		{
			if (static_cast<int>(issues.size()) >= kMaxIssuesPerCheck)
				break;
			Merge20VerifyIssue issue;
			issue.check = Merge20VerifyCheck::Syntax;
			issue.kind = Merge20VerifyIssueKind::EncodingInvalidUtf8;
			issue.resultLine = static_cast<int>(li) + 1;
			issue.text = input.resultLines[li];
			issue.byteOffset = badOffset;
			issues.push_back(std::move(issue));
		}
	}
	// Brackets: only when both inputs balance, so an imbalance
	// the merge did not introduce is never blamed on the merge.
	if (input.panes.size() >= 2 && input.newPane >= 0 &&
		input.newPane < static_cast<int>(input.panes.size()))
	{
		const std::vector<BracketHit> oldHits = ScanBrackets(input.panes[0]);
		const std::vector<BracketHit> newHits = ScanBrackets(input.panes[static_cast<size_t>(input.newPane)]);
		if (oldHits.empty() && newHits.empty())
		{
			for (const BracketHit& hit : ScanBrackets(input.resultLines))
			{
				if (static_cast<int>(issues.size()) >= kMaxIssuesPerCheck)
					break;
				Merge20VerifyIssue issue;
				issue.check = Merge20VerifyCheck::Syntax;
				issue.kind = hit.kind == 0 ? Merge20VerifyIssueKind::SyntaxUnmatchedCloser :
					hit.kind == 1 ? Merge20VerifyIssueKind::SyntaxUnclosedOpener :
					Merge20VerifyIssueKind::SyntaxMismatchedPair;
				issue.resultLine = hit.line;
				issue.text = std::string(1, hit.bracket);
				if (hit.line >= 1 && hit.line <= static_cast<int>(input.resultLines.size()))
					issue.text += " : " + input.resultLines[static_cast<size_t>(hit.line) - 1];
				issues.push_back(std::move(issue));
			}
		}
	}
	return issues;
}

/** @brief (iv) Conclusion vs output content. */
std::vector<Merge20VerifyIssue> CheckConsistency(const Merge20VerifyFileInput& input)
{
	std::vector<Merge20VerifyIssue> issues;
	const int nBuffers = static_cast<int>(input.panes.size());
	for (const Merge20VerifyDiffInput& diff : input.diffs)
	{
		if (static_cast<int>(issues.size()) >= kMaxIssuesPerCheck)
			break;
		if (diff.op == Merge20ConclusionOpNone)
			continue;
		if (!IsValidConclusion(diff.conclusion))
			continue;
		const auto conclusion = static_cast<Merge20DiffConclusion>(diff.conclusion);
		if (conclusion == Merge20DiffConclusion::Undecided ||
			conclusion == Merge20DiffConclusion::Keep)
			continue;
		const std::vector<int> srcPanes = Merge20ConclusionSrcPanes(conclusion, nBuffers, diff.op);
		if (srcPanes.empty())
		{
			// Reject on a changed block (or 3-way): the saved
			// form must be the conflict section, not chosen
			// content. A resolved segment here means the Reject
			// did not stick.
			if (conclusion != Merge20DiffConclusion::Reject)
				continue;
			if (diff.segState == Merge20VerifySegConflict || diff.segState == Merge20VerifySegUnresolved)
			{
				if (diff.savedLines <= 0)
				{
					Merge20VerifyIssue issue;
					issue.check = Merge20VerifyCheck::Consistency;
					issue.kind = Merge20VerifyIssueKind::ConsistencyMismatch;
					issue.diffIndex = diff.diffIndex;
					issue.resultLine = diff.savedBegin + 1;
					issue.expectedCount = 1;
					issue.actualCount = 0;
					issues.push_back(std::move(issue));
				}
				continue;
			}
			Merge20VerifyIssue issue;
			issue.check = Merge20VerifyCheck::Consistency;
			issue.kind = Merge20VerifyIssueKind::ConsistencyMismatch;
			issue.diffIndex = diff.diffIndex;
			issue.resultLine = diff.savedBegin + 1;
			issue.expectedCount = 0;
			issue.actualCount = diff.savedLines;
			issues.push_back(std::move(issue));
			continue;
		}
		if (diff.segState == Merge20VerifySegConflict || diff.segState == Merge20VerifySegUnresolved)
		{
			Merge20VerifyIssue issue;
			issue.check = Merge20VerifyCheck::Consistency;
			issue.kind = Merge20VerifyIssueKind::ConsistencyUnresolved;
			issue.diffIndex = diff.diffIndex;
			issue.resultLine = diff.savedBegin + 1;
			issues.push_back(std::move(issue));
			continue;
		}
		std::vector<std::string> expected;
		for (const int pane : srcPanes)
		{
			int begin = -1, end = -2;
			DiffPaneRange(diff, pane, input.newPane, begin, end);
			std::vector<std::string> part = PaneRangeLines(input, pane, begin, end);
			expected.insert(expected.end(), part.begin(), part.end());
		}
		std::vector<std::string> actual;
		for (int i = 0; i < diff.savedLines; ++i)
		{
			const int line = diff.savedBegin + i;
			if (line >= 0 && line < static_cast<int>(input.resultLines.size()))
				actual.push_back(input.resultLines[static_cast<size_t>(line)]);
		}
		if (expected != actual)
		{
			// Compare under the active compare options: a
			// whitespace-only / case-only difference is not a
			// difference for this comparison, so it is not a
			// mismatch either.
			const auto normalize = [&](const std::vector<std::string>& lines)
			{
				std::vector<std::string> out;
				out.reserve(lines.size());
				for (const std::string& line : lines)
					out.push_back(NormalizeLine(line, input.ignoreWhitespace, input.ignoreCase));
				return out;
			};
			if (normalize(expected) == normalize(actual))
				continue;
			Merge20VerifyIssue issue;
			issue.check = Merge20VerifyCheck::Consistency;
			issue.kind = Merge20VerifyIssueKind::ConsistencyMismatch;
			issue.diffIndex = diff.diffIndex;
			issue.resultLine = diff.savedBegin + 1;
			issue.expectedCount = static_cast<int>(expected.size());
			issue.actualCount = static_cast<int>(actual.size());
			const std::vector<std::string> normExpected = normalize(expected);
			const std::vector<std::string> normActual = normalize(actual);
			const size_t firstDiff = [&]() -> size_t
			{
				const size_t n = (std::min)(normExpected.size(), normActual.size());
				for (size_t k = 0; k < n; ++k)
				{
					if (normExpected[k] != normActual[k])
						return k;
				}
				return n;
			}();
			if (firstDiff < actual.size())
				issue.text = actual[firstDiff];
			else if (firstDiff < expected.size())
				issue.text = expected[firstDiff];
			issues.push_back(std::move(issue));
		}
	}
	return issues;
}

} // namespace

Merge20VerifyResult Merge20VerifyRunFile(const Merge20VerifyFileInput& input, const Merge20VerifyOptions& options)
{
	const Clock::time_point started = Clock::now();
	Merge20VerifyResult result;
	result.resultLineCount = static_cast<int>(input.resultLines.size());
	result.expectedLineCount = result.resultLineCount;

	const auto runCheck = [&](Merge20VerifyCheck check, bool enabled,
		const std::function<std::vector<Merge20VerifyIssue>()>& fn) -> void
	{
		Merge20VerifyCheckResult cr;
		cr.check = check;
		cr.enabled = enabled;
		if (!enabled)
		{
			result.checks.push_back(cr);
			return;
		}
		const Clock::time_point checkStarted = Clock::now();
		std::vector<Merge20VerifyIssue> found = fn();
		cr.elapsedMs = MsBetween(checkStarted, Clock::now());
		cr.issueCount = static_cast<int>(found.size());
		cr.passed = found.empty();
		for (auto& issue : found)
			AddIssue(result.issues, std::move(issue));
		result.checks.push_back(cr);
	};

	int expectedLineCount = result.resultLineCount;
	runCheck(Merge20VerifyCheck::Rediff, options.rediff,
		[&]() { return CheckRediff(input); });
	runCheck(Merge20VerifyCheck::LineCountEncoding, options.lineCountEncoding,
		[&]() { return CheckLineCountEncoding(input, expectedLineCount); });
	result.expectedLineCount = expectedLineCount;
	runCheck(Merge20VerifyCheck::Syntax, options.syntax,
		[&]() { return CheckSyntax(input); });
	runCheck(Merge20VerifyCheck::Consistency, options.consistency,
		[&]() { return CheckConsistency(input); });

	result.passed = result.issues.empty();
	result.elapsedMs = MsBetween(started, Clock::now());
	return result;
}
