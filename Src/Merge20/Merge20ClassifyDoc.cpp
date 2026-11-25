/**
 * @file  Merge20ClassifyDoc.cpp
 *
 * @brief Adapter between CMergeDoc and the pure classifier (S4).
 *
 * The diff list (DIFFRANGE) is block-based: for OP_DIFF the line
 * counts follow the design's rule (block size = end - begin + 1, the
 * larger side when the two sides differ), while the insight scan
 * pairs the lines of a block in order, like the HTML tool's rows.
 * Leftover lines of the longer side of an OP_DIFF block stay OP_DIFF
 * entries with one empty side (they count as changed lines); unlike
 * the HTML tool's leftover 'del' rows they are not tested for a
 * deleted if guard, which is a known minor deviation, recorded in
 * MERGE20_NOTES.md.
 *
 * Line texts are read through ComputeApparentLine (real line number
 * from the diff range -> apparent line number in the buffer, ghost
 * lines included), so ghost padding never leaks into the texts.
 */
#include "pch.h"
#include "Merge20/Merge20ClassifyDoc.h"
#include "Merge20/Merge20Classify.h"
#include "MergeDoc.h"
#include "DiffTextBuffer.h"
#include "locality.h"
#include "I18n.h"

namespace
{

	// Maximum lines scanned backwards from a hunk when looking for
	// its enclosing function definition. The function name is only a
	// summary nicety, so the scan is capped; without a cap, a hunk
	// near the end of a very large file would scan the whole file.
	const int kMaxBackwardScanLines = 500;

	// Read one real line of a text buffer (empty when unreadable).
	bool GetBufferLine(const CDiffTextBuffer& buf, int realLine, String& text)
	{
		if (realLine < 0)
			return false;
		return buf.GetLine(buf.ComputeApparentLine(realLine), text);
	}

	// Find the enclosing function definition above realLine in buf:
	// scan backwards from realLine - 1 down to (but not including)
	// stopBeforeReal, at most kMaxBackwardScanLines lines.
	// Returns the definition line text when found.
	bool FindEnclosingFunctionLine(const CDiffTextBuffer& buf, int realLine, int stopBeforeReal, String& fnLine)
	{
		const int lowest = (std::max)(stopBeforeReal, realLine - kMaxBackwardScanLines);
		for (int r = realLine - 1; r > lowest; --r)
		{
			String text;
			if (!GetBufferLine(buf, r, text))
				continue;
			String fn;
			if (Merge20MatchFunctionName(text, fn))
			{
				fnLine = text;
				return true;
			}
		}
		return false;
	}

	int RangeLineCount(const DIFFRANGE& range, int pane)
	{
		const int count = range.end[pane] - range.begin[pane] + 1;
		return count > 0 ? count : 0;
	}

} // namespace

Merge20FileState Merge20ClassifyFromMergeDoc(const CMergeDoc& doc, const std::vector<String>& riskKeywords)
{
	Merge20FileState state;
	const CDiffTextBuffer& oldBuf = *doc.m_ptBuf[0];
	const CDiffTextBuffer& newBuf = *doc.m_ptBuf[1];
	std::vector<Merge20DiffLine> lines;
	int prevOldEnd = -1, prevNewEnd = -1;

	for (int i = 0; i < doc.m_diffList.GetSize(); ++i)
	{
		const DIFFRANGE* pRange = doc.m_diffList.DiffRangeAt(i);
		if (pRange == nullptr || pRange->op == OP_NONE)
			continue;
		const DIFFRANGE& range = *pRange;
		const int oldLines = RangeLineCount(range, 0);
		const int newLines = RangeLineCount(range, 1);

		// Context entry: the enclosing function definition above this
		// hunk, so the affected function can be named even when the
		// definition itself did not change (HTML analyzeInsights
		// tracks the current function across equal rows the same way).
		// The scan stops at the previous hunk, because the current
		// function is carried over from there.
		if (range.op != OP_TRIVIAL)
		{
			String fnLine;
			bool found = false;
			if (oldLines > 0)
				found = FindEnclosingFunctionLine(oldBuf, range.begin[0], prevOldEnd, fnLine);
			if (!found && newLines > 0)
				found = FindEnclosingFunctionLine(newBuf, range.begin[1], prevNewEnd, fnLine);
			if (found)
			{
				Merge20DiffLine ctx;
				ctx.op = OP_NONE;
				ctx.oldText = fnLine;
				ctx.newText = fnLine;
				lines.push_back(ctx);
			}
		}

		switch (range.op)
		{
		case OP_1STONLY:
			for (int r = range.begin[0]; r <= range.end[0]; ++r)
			{
				Merge20DiffLine line;
				line.op = OP_1STONLY;
				GetBufferLine(oldBuf, r, line.oldText);
				lines.push_back(line);
			}
			break;
		case OP_2NDONLY:
			for (int r = range.begin[1]; r <= range.end[1]; ++r)
			{
				Merge20DiffLine line;
				line.op = OP_2NDONLY;
				GetBufferLine(newBuf, r, line.newText);
				lines.push_back(line);
			}
			break;
		case OP_DIFF:
			{
				const int paired = (std::min)(oldLines, newLines);
				for (int k = 0; k < paired; ++k)
				{
					Merge20DiffLine line;
					line.op = OP_DIFF;
					GetBufferLine(oldBuf, range.begin[0] + k, line.oldText);
					GetBufferLine(newBuf, range.begin[1] + k, line.newText);
					lines.push_back(line);
				}
				for (int k = paired; k < oldLines; ++k)
				{
					Merge20DiffLine line;
					line.op = OP_DIFF;
					GetBufferLine(oldBuf, range.begin[0] + k, line.oldText);
					lines.push_back(line);
				}
				for (int k = paired; k < newLines; ++k)
				{
					Merge20DiffLine line;
					line.op = OP_DIFF;
					GetBufferLine(newBuf, range.begin[1] + k, line.newText);
					lines.push_back(line);
				}
			}
			break;
		case OP_TRIVIAL:
			{
				// Counted as excluded lines; the texts are not needed.
				const int count = (std::max)(oldLines, newLines);
				for (int k = 0; k < count; ++k)
				{
					Merge20DiffLine line;
					line.op = OP_TRIVIAL;
					lines.push_back(line);
				}
			}
			break;
		default:
			break;
		}
		if (oldLines > 0)
			prevOldEnd = (std::max)(prevOldEnd, range.end[0]);
		if (newLines > 0)
			prevNewEnd = (std::max)(prevNewEnd, range.end[1]);
	}

	const Merge20ClassifyResult result = Merge20ClassifyDiffLines(lines, riskKeywords);
	state.classified = true;
	state.action = result.action;
	state.risk = result.risk;
	state.nMissing = result.nMissing;
	state.nModified = result.nModified;
	state.nAdded = result.nAdded;
	state.nExcluded = result.nExcluded;
	state.summary = Merge20FormatSummary(result);
	state.leftEncoding = oldBuf.getEncoding().GetName();
	state.rightEncoding = newBuf.getEncoding().GetName();
	return state;
}

String Merge20FormatSummary(const Merge20ClassifyResult& result)
{
	// Warning sign, as in the HTML tool's insight strings. Written as
	// an escape: this file stays ASCII, like the other Merge20 files.
	const String warn = _T("\x26a0 ");
	String summary = strutils::format_string3(
		_("Missing: %1 lines / Changed: %2 lines / Added: %3 lines"),
		locality::NumToLocaleStr(result.nMissing),
		locality::NumToLocaleStr(result.nModified),
		locality::NumToLocaleStr(result.nAdded));
	if (result.nExcluded > 0)
	{
		summary += _T(" / ");
		summary += strutils::format_string1(_("Excluded: %1 lines"), locality::NumToLocaleStr(result.nExcluded));
	}
	for (Merge20Insight insight : result.insights)
	{
		summary += _T(" / ");
		summary += warn;
		switch (insight)
		{
		case Merge20Insight::ROUNDING:
			summary += _("Rounding change");
			break;
		case Merge20Insight::SQL:
			summary += _("SQL change");
			break;
		case Merge20Insight::IF_GUARD:
			summary += _("Deleted if guard (old-only logic lost)");
			break;
		case Merge20Insight::CONDITION:
			summary += _("Condition/boundary change");
			break;
		case Merge20Insight::CONSTANT:
			if (!result.constantFrom.empty() && !result.constantTo.empty())
			{
				// ASCII "->" on purpose: _() msgids are narrow strings,
				// so a \x2192 escape would not survive as an arrow.
				// The HTML tool shows a real arrow here.
				summary += strutils::format_string2(_("Constant change (%1 -> %2)"),
					result.constantFrom, result.constantTo);
			}
			else
			{
				summary += _("Constant change");
			}
			break;
		}
	}
	if (result.hasRiskKeyword)
	{
		summary += _T(" / ");
		summary += warn;
		summary += _("Includes important keywords");
	}
	if (!result.functions.empty())
	{
		summary += _T(" / ");
		summary += strutils::format_string1(_("Affected functions: %1"),
			strutils::join(result.functions.begin(), result.functions.end(), String(_T(", "))));
	}
	return summary;
}
