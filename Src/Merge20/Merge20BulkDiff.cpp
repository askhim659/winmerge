/**
 * @file  Merge20BulkDiff.cpp
 *
 * @brief Implementation of the bulk line differ (D10).
 *
 * Port of the HTML tool's `diffFast` / `lcs` (17_v17.15.html, lines
 * 1053-1072): strip the common prefix/suffix on normalized lines,
 * run the LCS dynamic program on the remaining middle, and fall
 * back to a positional alignment when the middle satisfies
 * n*m > threshold. The walk order and the tie-break
 * (dp[i+1][j] >= dp[i][j+1] prefers a deletion) are the HTML ones,
 * so the same two files produce the same op stream as the HTML
 * tool. The only deliberate difference is the L2 version line:
 * the HTML tool treats the 2nd line specially (`__L2V__`), the
 * fork has no such concept, so it is not ported.
 *
 * Empty sides of a range use the DIFFRANGE convention (end =
 * begin - 1, so the line count is 0), never -1/-1: the shared
 * range-to-lines builder in Merge20ClassifyDoc counts a side as
 * end - begin + 1, and -1/-1 would count as 1.
 *
 * This file stays ASCII: no Japanese literals, no `_()`.
 */
#include "pch.h"
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include "Merge20/Merge20BulkDiff.h"

namespace
{

/** @brief One differ step, in file order (HTML `lcs` op). */
struct BulkOp
{
	enum class Kind { Eq, Trivial, Del, Add } kind = Kind::Eq;
	int oldIdx = -1; /**< Old line index, -1 when the step has no old line. */
	int newIdx = -1; /**< New line index, -1 when the step has no new line. */
};

/** @brief Normalize one line for comparison (HTML `normL` without the L2 special case). */
String NormalizeLine(const String& line, const Merge20BulkDiffOptions& options)
{
	String s = line;
	if (options.ignoreWhitespace)
	{
		// Collapse every whitespace run to one space and trim
		// (HTML: s.replace(/\s+/g, ' ').trim()).
		String out;
		bool inSpace = false;
		bool started = false;
		for (wchar_t ch : s)
		{
			if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n')
			{
				if (started)
					inSpace = true;
			}
			else
			{
				if (inSpace && !out.empty())
					out += L' ';
				out += ch;
				inSpace = false;
				started = true;
			}
		}
		s = out;
	}
	if (options.ignoreCase)
	{
		for (wchar_t& ch : s)
			ch = static_cast<wchar_t>(std::towlower(static_cast<wint_t>(ch)));
	}
	return s;
}

/** @brief True when the line is blank (only whitespace). */
bool IsBlankLine(const String& line)
{
	for (wchar_t ch : line)
	{
		if (ch != L' ' && ch != L'\t' && ch != L'\r' && ch != L'\n')
			return false;
	}
	return true;
}

/** @brief Group consecutive non-equal ops into ranges. */
void GroupOpsIntoRanges(const std::vector<BulkOp>& ops, const std::vector<String>& oldLines,
	const std::vector<String>& newLines, const Merge20BulkDiffOptions& options,
	std::vector<Merge20BulkRange>& ranges)
{
	// Lines consumed so far on each side (also the start of the next range).
	int oldPos = 0, newPos = 0;
	// Open diff group: del/add counts and the blank-only flag.
	int groupDel = 0, groupAdd = 0;
	bool groupAllBlank = true;
	// Open trivial run length (1:1 paired lines).
	int trivialRun = 0;

	auto flushDiffGroup = [&]() {
		if (groupDel == 0 && groupAdd == 0)
			return;
		Merge20BulkRange range;
		range.beginOld = oldPos;
		range.endOld = groupDel > 0 ? oldPos + groupDel - 1 : oldPos - 1;
		range.beginNew = newPos;
		range.endNew = groupAdd > 0 ? newPos + groupAdd - 1 : newPos - 1;
		if (options.ignoreBlankLines && groupAllBlank)
			range.op = OP_TRIVIAL;
		else if (groupDel > 0 && groupAdd > 0)
			range.op = OP_DIFF;
		else if (groupDel > 0)
			range.op = OP_1STONLY;
		else
			range.op = OP_2NDONLY;
		ranges.push_back(range);
		oldPos += groupDel;
		newPos += groupAdd;
		groupDel = 0;
		groupAdd = 0;
		groupAllBlank = true;
	};
	auto flushTrivialRun = [&]() {
		if (trivialRun == 0)
			return;
		Merge20BulkRange range;
		range.op = OP_TRIVIAL;
		range.beginOld = oldPos;
		range.endOld = oldPos + trivialRun - 1;
		range.beginNew = newPos;
		range.endNew = newPos + trivialRun - 1;
		ranges.push_back(range);
		oldPos += trivialRun;
		newPos += trivialRun;
		trivialRun = 0;
	};

	for (const BulkOp& op : ops)
	{
		switch (op.kind)
		{
		case BulkOp::Kind::Eq:
			flushDiffGroup();
			flushTrivialRun();
			++oldPos;
			++newPos;
			break;
		case BulkOp::Kind::Trivial:
			flushDiffGroup();
			++trivialRun;
			break;
		case BulkOp::Kind::Del:
			flushTrivialRun();
			if (op.oldIdx >= 0 && !IsBlankLine(oldLines[static_cast<size_t>(op.oldIdx)]))
				groupAllBlank = false;
			++groupDel;
			break;
		case BulkOp::Kind::Add:
			flushTrivialRun();
			if (op.newIdx >= 0 && !IsBlankLine(newLines[static_cast<size_t>(op.newIdx)]))
				groupAllBlank = false;
			++groupAdd;
			break;
		}
	}
	flushDiffGroup();
	flushTrivialRun();
}

} // namespace

Merge20BulkDiffResult Merge20BulkDiff(const std::vector<String>& oldLines, const std::vector<String>& newLines,
	const Merge20BulkDiffOptions& options)
{
	Merge20BulkDiffResult result;
	const int n = static_cast<int>(oldLines.size());
	const int m = static_cast<int>(newLines.size());

	std::vector<String> nO(static_cast<size_t>(n));
	std::vector<String> nN(static_cast<size_t>(m));
	for (int i = 0; i < n; ++i)
		nO[static_cast<size_t>(i)] = NormalizeLine(oldLines[static_cast<size_t>(i)], options);
	for (int j = 0; j < m; ++j)
		nN[static_cast<size_t>(j)] = NormalizeLine(newLines[static_cast<size_t>(j)], options);

	// Common prefix / suffix on normalized lines (HTML `diffFast`).
	int st = 0;
	while (st < n && st < m && nO[static_cast<size_t>(st)] == nN[static_cast<size_t>(st)])
		++st;
	int eO = n - 1, eN = m - 1;
	while (eO >= st && eN >= st && nO[static_cast<size_t>(eO)] == nN[static_cast<size_t>(eN)])
	{
		--eO;
		--eN;
	}
	const int nMid = eO - st + 1;
	const int mMid = eN - st + 1;

	std::vector<BulkOp> ops;
	// Prefix ops (raw-different but normalized-equal lines are trivial).
	for (int k = 0; k < st; ++k)
	{
		BulkOp op;
		op.kind = oldLines[static_cast<size_t>(k)] == newLines[static_cast<size_t>(k)] ? BulkOp::Kind::Eq : BulkOp::Kind::Trivial;
		op.oldIdx = k;
		op.newIdx = k;
		ops.push_back(op);
	}

	const bool useSimple = options.simpleThreshold > 0 &&
		static_cast<int64_t>(nMid) * static_cast<int64_t>(mMid) > options.simpleThreshold;
	if (useSimple)
	{
		// HTML `lcs` fallback: positional alignment (same index on
		// both sides), equal when the normalized lines match.
		result.simpleUsed = true;
		const int mx = (std::max)(nMid, mMid);
		for (int i = 0; i < mx; ++i)
		{
			const bool hasOld = i < nMid;
			const bool hasNew = i < mMid;
			if (hasOld && hasNew && nO[static_cast<size_t>(st + i)] == nN[static_cast<size_t>(st + i)])
			{
				BulkOp op;
				op.kind = oldLines[static_cast<size_t>(st + i)] == newLines[static_cast<size_t>(st + i)] ? BulkOp::Kind::Eq : BulkOp::Kind::Trivial;
				op.oldIdx = st + i;
				op.newIdx = st + i;
				ops.push_back(op);
			}
			else
			{
				if (hasOld)
				{
					BulkOp op;
					op.kind = BulkOp::Kind::Del;
					op.oldIdx = st + i;
					ops.push_back(op);
				}
				if (hasNew)
				{
					BulkOp op;
					op.kind = BulkOp::Kind::Add;
					op.newIdx = st + i;
					ops.push_back(op);
				}
			}
		}
	}
	else if (nMid > 0 || mMid > 0)
	{
		// HTML `lcs` dynamic program on the normalized middle.
		const int rows = nMid + 1;
		const int cols = mMid + 1;
		std::vector<int32_t> dp(static_cast<size_t>(rows) * static_cast<size_t>(cols), 0);
		auto at = [&](int i, int j) -> int32_t& { return dp[static_cast<size_t>(i) * static_cast<size_t>(cols) + static_cast<size_t>(j)]; };
		for (int i = nMid - 1; i >= 0; --i)
		{
			for (int j = mMid - 1; j >= 0; --j)
			{
				at(i, j) = nO[static_cast<size_t>(st + i)] == nN[static_cast<size_t>(st + j)]
					? 1 + at(i + 1, j + 1)
					: (std::max)(at(i + 1, j), at(i, j + 1));
			}
		}
		int i = 0, j = 0;
		while (i < nMid && j < mMid)
		{
			if (nO[static_cast<size_t>(st + i)] == nN[static_cast<size_t>(st + j)])
			{
				BulkOp op;
				op.kind = oldLines[static_cast<size_t>(st + i)] == newLines[static_cast<size_t>(st + j)] ? BulkOp::Kind::Eq : BulkOp::Kind::Trivial;
				op.oldIdx = st + i;
				op.newIdx = st + j;
				ops.push_back(op);
				++i;
				++j;
			}
			else if (at(i + 1, j) >= at(i, j + 1))
			{
				BulkOp op;
				op.kind = BulkOp::Kind::Del;
				op.oldIdx = st + i;
				ops.push_back(op);
				++i;
			}
			else
			{
				BulkOp op;
				op.kind = BulkOp::Kind::Add;
				op.newIdx = st + j;
				ops.push_back(op);
				++j;
			}
		}
		while (i < nMid)
		{
			BulkOp op;
			op.kind = BulkOp::Kind::Del;
			op.oldIdx = st + i;
			ops.push_back(op);
			++i;
		}
		while (j < mMid)
		{
			BulkOp op;
			op.kind = BulkOp::Kind::Add;
			op.newIdx = st + j;
			ops.push_back(op);
			++j;
		}
	}

	// Suffix ops.
	for (int k = 0; k < n - 1 - eO; ++k)
	{
		BulkOp op;
		const int oi = eO + 1 + k;
		const int ni = eN + 1 + k;
		op.kind = oldLines[static_cast<size_t>(oi)] == newLines[static_cast<size_t>(ni)] ? BulkOp::Kind::Eq : BulkOp::Kind::Trivial;
		op.oldIdx = oi;
		op.newIdx = ni;
		ops.push_back(op);
	}

	GroupOpsIntoRanges(ops, oldLines, newLines, options, result.ranges);
	return result;
}
