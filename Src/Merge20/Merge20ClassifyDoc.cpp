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
 *
 * D10 shares the range-to-lines builder with the bulk path: the
 * builder works on Merge20BulkRange lists plus a line accessor, so
 * Merge20ClassifyFromMergeDoc (buffers) and
 * Merge20ClassifyFromRanges (in-memory line vectors read from disk)
 * produce the same diff lines from the same ranges.
 */
#include "StdAfx.h"
#include <functional>
#include "Merge20/Merge20ClassifyDoc.h"
#include "Merge20/Merge20Classify.h"
#include "Merge20/Merge20BulkDiff.h"
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

	// One line reader, by pane (0 = old, 1 = new) and real line number.
	// The merge-doc path reads from the text buffers, the bulk path
	// from the in-memory line vectors (D10); the builder below only
	// sees this accessor.
	using LineAccessor = std::function<bool(int pane, int realLine, String& text)>;

	// Find the enclosing function definition above realLine in one pane:
	// scan backwards from realLine - 1 down to (but not including)
	// stopBeforeReal, at most kMaxBackwardScanLines lines.
	// Returns the definition line text when found.
	bool FindEnclosingFunctionLine(const LineAccessor& getLine, int pane, int realLine, int stopBeforeReal, String& fnLine)
	{
		const int lowest = (std::max)(stopBeforeReal, realLine - kMaxBackwardScanLines);
		for (int r = realLine - 1; r > lowest; --r)
		{
			String text;
			if (!getLine(pane, r, text))
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

	int RangeLineCount(int begin, int end)
	{
		const int count = end - begin + 1;
		return count > 0 ? count : 0;
	}

	// Build the classifier input from diff ranges: the context entry
	// (enclosing function definition above each hunk) plus the paired
	// diff lines, exactly as the S4 merge-doc path did. OP_DIFF pairs
	// the two sides in order and keeps the longer side's leftovers as
	// one-sided OP_DIFF lines; OP_TRIVIAL lines are counted as
	// excluded lines and carry no text. A side with no lines in a
	// range uses the DIFFRANGE convention (end = begin - 1).
	std::vector<Merge20DiffLine> BuildDiffLines(const std::vector<Merge20BulkRange>& ranges, const LineAccessor& getLine)
	{
		std::vector<Merge20DiffLine> lines;
		int prevOldEnd = -1, prevNewEnd = -1;
		for (const Merge20BulkRange& range : ranges)
		{
			const int oldLines = RangeLineCount(range.beginOld, range.endOld);
			const int newLines = RangeLineCount(range.beginNew, range.endNew);

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
					found = FindEnclosingFunctionLine(getLine, 0, range.beginOld, prevOldEnd, fnLine);
				if (!found && newLines > 0)
					found = FindEnclosingFunctionLine(getLine, 1, range.beginNew, prevNewEnd, fnLine);
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
				for (int r = range.beginOld; r <= range.endOld; ++r)
				{
					Merge20DiffLine line;
					line.op = OP_1STONLY;
					getLine(0, r, line.oldText);
					lines.push_back(line);
				}
				break;
			case OP_2NDONLY:
				for (int r = range.beginNew; r <= range.endNew; ++r)
				{
					Merge20DiffLine line;
					line.op = OP_2NDONLY;
					getLine(1, r, line.newText);
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
						getLine(0, range.beginOld + k, line.oldText);
						getLine(1, range.beginNew + k, line.newText);
						lines.push_back(line);
					}
					for (int k = paired; k < oldLines; ++k)
					{
						Merge20DiffLine line;
						line.op = OP_DIFF;
						getLine(0, range.beginOld + k, line.oldText);
						lines.push_back(line);
					}
					for (int k = paired; k < newLines; ++k)
					{
						Merge20DiffLine line;
						line.op = OP_DIFF;
						getLine(1, range.beginNew + k, line.newText);
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
				prevOldEnd = (std::max)(prevOldEnd, range.endOld);
			if (newLines > 0)
				prevNewEnd = (std::max)(prevNewEnd, range.endNew);
		}
		return lines;
	}

	// Classify built diff lines (shared core; encodings are filled in
	// by the caller, because the bulk path has no buffer encodings).
	Merge20FileState ClassifyBuiltLines(const std::vector<Merge20DiffLine>& lines, const std::vector<String>& riskKeywords)
	{
		const Merge20ClassifyResult result = Merge20ClassifyDiffLines(lines, riskKeywords);
		Merge20FileState state;
		state.classified = true;
		state.action = result.action;
		state.risk = result.risk;
		state.nMissing = result.nMissing;
		state.nModified = result.nModified;
		state.nAdded = result.nAdded;
		state.nExcluded = result.nExcluded;
		state.summary = Merge20FormatSummary(result);
		return state;
	}

} // namespace

Merge20FileState Merge20ClassifyFromMergeDoc(const CMergeDoc& doc, const std::vector<String>& riskKeywords)
{
	const CDiffTextBuffer& oldBuf = *doc.m_ptBuf[0];
	const CDiffTextBuffer& newBuf = *doc.m_ptBuf[1];
	std::vector<Merge20BulkRange> ranges;
	for (int i = 0; i < doc.m_diffList.GetSize(); ++i)
	{
		const DIFFRANGE* pRange = doc.m_diffList.DiffRangeAt(i);
		if (pRange == nullptr || pRange->op == OP_NONE)
			continue;
		Merge20BulkRange range;
		range.op = pRange->op;
		range.beginOld = pRange->begin[0];
		range.endOld = pRange->end[0];
		range.beginNew = pRange->begin[1];
		range.endNew = pRange->end[1];
		ranges.push_back(range);
	}
	const LineAccessor getLine = [&](int pane, int realLine, String& text) {
		return pane == 0 ? GetBufferLine(oldBuf, realLine, text) : GetBufferLine(newBuf, realLine, text);
	};
	Merge20FileState state = ClassifyBuiltLines(BuildDiffLines(ranges, getLine), riskKeywords);
	state.leftEncoding = oldBuf.getEncoding().GetName();
	state.rightEncoding = newBuf.getEncoding().GetName();
	return state;
}

Merge20FileState Merge20ClassifyFromRanges(const std::vector<Merge20BulkRange>& ranges,
	const std::vector<String>& oldLines, const std::vector<String>& newLines,
	const std::vector<String>& riskKeywords)
{
	const LineAccessor getLine = [&](int pane, int realLine, String& text) {
		const std::vector<String>& lines = pane == 0 ? oldLines : newLines;
		if (realLine < 0 || realLine >= static_cast<int>(lines.size()))
			return false;
		text = lines[static_cast<size_t>(realLine)];
		return true;
	};
	// Encodings stay empty: the bulk path reads on-disk text without
	// the buffer encoding objects, and the encodings are display-only
	// (not shown in the list or the CSV, and not session-persisted).
	return ClassifyBuiltLines(BuildDiffLines(ranges, getLine), riskKeywords);
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
