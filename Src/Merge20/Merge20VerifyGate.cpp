// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20VerifyGate.cpp
 *
 * @brief Implementation of the B7 verification gate adapter.
 *
 * BuildInput mirrors SaveMergeResult's saved-form expansion
 * (BuildExpandedResultText): conflict/unresolved segments
 * contribute their conflict-section lines, "<No Lines>"
 * display lines contribute nothing, everything else
 * contributes its buffer lines. The walk also records each
 * diff's saved line range, which the engine needs to attribute
 * re-diff hunks and consistency mismatches to a diff. The
 * pane lines are read by real line number (the
 * Merge20ClassifyDoc pattern: ComputeApparentLine, because
 * DIFFRANGE begin/end are real lines and the buffers hold
 * ghost lines).
 *
 * The representability predicate comes from the result
 * buffer's encoding: Unicode targets accept every scalar
 * value; a legacy codepage is asked through
 * WideCharToMultiByte with WC_NO_BEST_FIT_CHARS, so a
 * character the codepage cannot write (or would silently
 * replace) fails the check instead of being lost. An unknown
 * codepage skips the check rather than inventing problems.
 */
#include "StdAfx.h"
#include <algorithm>
#include "Merge20VerifyGate.h"
#include "MergeDoc.h"
#include "MergeResultPane.h"
#include "MergeResultView.h"
#include "MergeResultTextBuffer.h"
#include "DiffTextBuffer.h"
#include "Merge.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"
#include "Logger.h"
#include "paths.h"

// The engine mirrors ResultSegmentState by value; fail the
// build (not the user) if the mirror ever drifts.
static_assert(static_cast<int>(ResultSegmentState::Common) == Merge20VerifySegCommon,
	"Merge20VerifySegCommon drifted from ResultSegmentState::Common");
static_assert(static_cast<int>(ResultSegmentState::Auto) == Merge20VerifySegAuto,
	"Merge20VerifySegAuto drifted from ResultSegmentState::Auto");
static_assert(static_cast<int>(ResultSegmentState::Chosen) == Merge20VerifySegChosen,
	"Merge20VerifySegChosen drifted from ResultSegmentState::Chosen");
static_assert(static_cast<int>(ResultSegmentState::Unresolved) == Merge20VerifySegUnresolved,
	"Merge20VerifySegUnresolved drifted from ResultSegmentState::Unresolved");
static_assert(static_cast<int>(ResultSegmentState::Conflict) == Merge20VerifySegConflict,
	"Merge20VerifySegConflict drifted from ResultSegmentState::Conflict");
static_assert(static_cast<int>(ResultSegmentState::Edited) == Merge20VerifySegEdited,
	"Merge20VerifySegEdited drifted from ResultSegmentState::Edited");

namespace
{

// Read one real line of a text buffer (empty when unreadable).
bool GetBufferLine(const CDiffTextBuffer& buf, int realLine, String& text)
{
	if (realLine < 0)
		return false;
	return buf.GetLine(buf.ComputeApparentLine(realLine), text);
}

// Count real (non-ghost) lines of one buffer (the
// Merge20ClassifyDoc pattern: GetLineCount counts apparent
// lines, ghost lines included).
int CountRealLines(const CDiffTextBuffer& buf)
{
	int count = 0;
	const int apparent = buf.GetLineCount();
	for (int i = 0; i < apparent; ++i)
	{
		if ((buf.GetLineFlags(i) & LF_GHOST) == 0)
			++count;
	}
	return count;
}

// Whether a UTF-16 line has an unpaired surrogate (a lone high
// surrogate, or a low surrogate without a preceding high one).
// ucr::toUTF8 combines a high surrogate with whatever follows
// it, so such a line would reach the engine as valid UTF-8
// holding the wrong character; only this scan can see it.
bool HasUnpairedSurrogate(const String& text)
{
	for (size_t i = 0; i < text.length(); ++i)
	{
		const wchar_t ch = text[i];
		if (ch >= 0xD800 && ch < 0xDC00)
		{
			if (i + 1 >= text.length() || text[i + 1] < 0xDC00 || text[i + 1] > 0xDFFF)
				return true;
			++i;
		}
		else if (ch >= 0xDC00 && ch <= 0xDFFF)
		{
			return true;
		}
	}
	return false;
}

// Split a block of text (conflict section) into lines, on CRLF,
// CR or LF; a trailing EOL leaves no empty last line.
void SplitTextLines(const String& text, std::vector<String>& lines)
{
	size_t start = 0;
	while (start < text.length())
	{
		size_t end = text.find_first_of(_T("\r\n"), start);
		if (end == String::npos)
			end = text.length();
		lines.push_back(text.substr(start, end - start));
		if (end >= text.length())
			break;
		if (text[end] == _T('\r') && end + 1 < text.length() && text[end + 1] == _T('\n'))
			start = end + 2;
		else
			start = end + 1;
	}
}

} // namespace

Merge20VerifyOptions Merge20VerifyLoadOptions()
{
	Merge20VerifyOptions options;
	options.rediff = GetOptionsMgr()->GetBool(OPT_MERGE20_VERIFY_REDIFF);
	options.lineCountEncoding = GetOptionsMgr()->GetBool(OPT_MERGE20_VERIFY_LINE_COUNT);
	options.syntax = GetOptionsMgr()->GetBool(OPT_MERGE20_VERIFY_SYNTAX);
	options.consistency = GetOptionsMgr()->GetBool(OPT_MERGE20_VERIFY_CONSISTENCY);
	return options;
}

bool Merge20VerifyBuildInput(const CMergeDoc& doc, Merge20VerifyFileInput& input,
	String& displayName, std::vector<int>& unpairedSurrogateLines)
{
	input = Merge20VerifyFileInput{};
	displayName.clear();
	unpairedSurrogateLines.clear();
	if (!doc.GetMergeResultBuildState())
		return false;
	const int nBuffers = doc.m_nBuffers;
	if (nBuffers < 2)
		return false;
	const int newPane = doc.Merge20GetNewPane();
	if (newPane < 0 || newPane >= nBuffers)
		return false;
	const CDiffTextBuffer* pResultBuf = doc.GetMergeResultBuffer();
	if (pResultBuf == nullptr || !pResultBuf->IsInitialized())
		return false;

	displayName = paths::FindFileName(doc.GetPath(newPane));

	// Compared panes, by real line number.
	for (int pane = 0; pane < nBuffers; ++pane)
	{
		const CDiffTextBuffer& buf = *doc.m_ptBuf[pane];
		std::vector<std::string> lines;
		const int realCount = CountRealLines(buf);
		for (int realLine = 0; realLine < realCount; ++realLine)
		{
			String text;
			if (!GetBufferLine(buf, realLine, text))
				text.clear();
			lines.push_back(ucr::toUTF8(text));
		}
		input.panes.push_back(std::move(lines));
	}
	input.newPane = newPane;

	// Result lines in saved form, with each diff's saved range.
	// Segments tile the buffer in start-line order; common text
	// between segments passes through unchanged.
	std::vector<const MergeResultSegment*> segments;
	for (int i = 0; i < doc.m_diffList.GetSize(); ++i)
	{
		const MergeResultSegment* pSegment = doc.GetResultSegmentByDiff(i);
		if (pSegment != nullptr)
			segments.push_back(pSegment);
	}
	std::sort(segments.begin(), segments.end(),
		[](const MergeResultSegment* a, const MergeResultSegment* b) { return a->nStartLine < b->nStartLine; });

	const int nBufLines = pResultBuf->GetLineCount();
	auto appendBufferLines = [&](int first, int count, int& savedCursor)
	{
		for (int i = 0; i < count; ++i)
		{
			const int line = first + i;
			if (line < 0 || line >= nBufLines)
				continue;
			// Mirror BuildExpandedResultText / GetResultBufferLinesText:
			// ghost lines are display alignment only and are not saved.
			if (pResultBuf->GetLineFlags(line) & LF_GHOST)
				continue;
			const int len = pResultBuf->GetLineLength(line);
			const String text(pResultBuf->GetLineChars(line), static_cast<size_t>(len));
			if (HasUnpairedSurrogate(text))
				unpairedSurrogateLines.push_back(static_cast<int>(input.resultLines.size()) + 1);
			input.resultLines.push_back(ucr::toUTF8(text));
			++savedCursor;
		}
	};
	auto appendBlockLines = [&](const String& blockText, int& savedCursor)
	{
		std::vector<String> lines;
		SplitTextLines(blockText, lines);
		for (const String& line : lines)
		{
			if (HasUnpairedSurrogate(line))
				unpairedSurrogateLines.push_back(static_cast<int>(input.resultLines.size()) + 1);
			input.resultLines.push_back(ucr::toUTF8(line));
			++savedCursor;
		}
	};

	// Saved range per diff, by diff index (segments are looked
	// up per diff so the range belongs to the right diff even
	// when a segment has no lines).
	std::vector<Merge20VerifyDiffInput> diffs(static_cast<size_t>(doc.m_diffList.GetSize()));
	int cursor = 0; // buffer line
	int savedCursor = 0; // saved-form line
	for (const MergeResultSegment* pSegment : segments)
	{
		// Common text before this segment.
		int gapCursor = savedCursor;
		appendBufferLines(cursor, pSegment->nStartLine - cursor, gapCursor);
		savedCursor = gapCursor;
		cursor = pSegment->nStartLine + pSegment->nLines;
		if (pSegment->diffIdx < 0)
			continue;
		Merge20VerifyDiffInput& diff = diffs[static_cast<size_t>(pSegment->diffIdx)];
		diff.diffIndex = pSegment->diffIdx;
		diff.savedBegin = savedCursor;
		if ((pSegment->state == ResultSegmentState::Conflict ||
			pSegment->state == ResultSegmentState::Unresolved))
		{
			// The buffer shows the compact placeholder; the file
			// gets the full conflict section.
			int before = savedCursor;
			appendBlockLines(pSegment->blockText, savedCursor);
			diff.savedLines = savedCursor - before;
		}
		else if (pSegment->bNoSrc)
		{
			// "<No Lines>" is display-only: nothing is saved.
			diff.savedLines = 0;
		}
		else
		{
			int before = savedCursor;
			appendBufferLines(pSegment->nStartLine, pSegment->nLines, savedCursor);
			diff.savedLines = savedCursor - before;
		}
	}
	// Trailing common text after the last segment.
	{
		int gapCursor = savedCursor;
		appendBufferLines(cursor, nBufLines - cursor, gapCursor);
		savedCursor = gapCursor;
	}

	// Diff records: ranges (real lines), conclusion, segment state.
	for (int i = 0; i < doc.m_diffList.GetSize(); ++i)
	{
		DIFFRANGE di;
		if (!doc.m_diffList.GetDiff(i, di))
			continue;
		Merge20VerifyDiffInput& diff = diffs[static_cast<size_t>(i)];
		diff.diffIndex = i;
		diff.op = static_cast<int>(di.op);
		diff.oldBegin = di.begin[0];
		diff.oldEnd = di.end[0];
		diff.newBegin = di.begin[newPane];
		diff.newEnd = di.end[newPane];
		if (nBuffers > 2)
		{
			diff.thirdBegin = di.begin[2];
			diff.thirdEnd = di.end[2];
		}
		diff.conclusion = static_cast<int>(doc.Merge20GetDiffConclusion(i));
		const MergeResultSegment* pSegment = doc.GetResultSegmentByDiff(i);
		if (pSegment != nullptr)
		{
			diff.segState = static_cast<int>(pSegment->state);
			for (const int pane : pSegment->srcPanes)
				diff.srcPanes.push_back(pane);
			// savedBegin/savedLines were set by the walk above;
			// a diff with no segment keeps 0 lines at 0, which
			// the engine treats as an empty contribution.
		}
	}
	input.diffs = std::move(diffs);

	input.ignoreWhitespace = GetOptionsMgr()->GetBool(OPT_CMP_IGNORE_WHITESPACE);
	input.ignoreCase = GetOptionsMgr()->GetBool(OPT_CMP_IGNORE_CASE);

	// Representability in the output encoding (S0 round-trip
	// idea, applied to the real output).
	const FileTextEncoding& encoding = pResultBuf->getEncoding();
	if (encoding.m_unicoding == ucr::NONE && encoding.m_codepage > 0)
	{
		const int codepage = encoding.m_codepage;
		input.isRepresentable = [codepage](char32_t cp) -> bool
		{
			wchar_t wide[2];
			int wideLen = 0;
			if (cp < 0x10000)
			{
				wide[0] = static_cast<wchar_t>(cp);
				wideLen = 1;
			}
			else
			{
				const char32_t v = cp - 0x10000;
				wide[0] = static_cast<wchar_t>(0xD800 + (v >> 10));
				wide[1] = static_cast<wchar_t>(0xDC00 + (v & 0x3FF));
				wideLen = 2;
			}
			BOOL usedDefault = FALSE;
			char out[4];
			const int converted = ::WideCharToMultiByte(codepage, WC_NO_BEST_FIT_CHARS,
				wide, wideLen, out, static_cast<int>(sizeof(out)), nullptr, &usedDefault);
			return converted > 0 && !usedDefault;
		};
	}
	// Unicode targets (and unknown codepages): every scalar
	// value is representable, so the predicate stays empty and
	// the engine skips that part of (ii).
	return true;
}

Merge20VerifyResult Merge20VerifyCheckMergeDoc(const CMergeDoc& doc)
{
	return Merge20VerifyCheckMergeDocFile(doc).result;
}

Merge20VerifyFileResult Merge20VerifyCheckMergeDocFile(const CMergeDoc& doc, int fileIndex)
{
	Merge20VerifyFileResult file;
	file.fileIndex = fileIndex;
	Merge20VerifyFileInput input;
	String displayName;
	std::vector<int> unpairedSurrogateLines;
	if (!Merge20VerifyBuildInput(doc, input, displayName, unpairedSurrogateLines))
	{
		// No merge session: nothing to verify, clean by
		// definition (the caller only gates built results).
		file.result.passed = true;
		return file;
	}
	file.fileName = displayName;
	Merge20VerifyResult result = Merge20VerifyRunFile(input, Merge20VerifyLoadOptions());
	// Unpaired surrogates the engine cannot see (see
	// BuildInput): report them as invalid bytes, unless the
	// engine already flagged the same line.
	for (const int line : unpairedSurrogateLines)
	{
		bool alreadyReported = false;
		for (const Merge20VerifyIssue& issue : result.issues)
		{
			if (issue.resultLine == line &&
				issue.kind == Merge20VerifyIssueKind::EncodingInvalidUtf8)
			{
				alreadyReported = true;
				break;
			}
		}
		if (alreadyReported)
			continue;
		Merge20VerifyIssue issue;
		issue.check = Merge20VerifyCheck::LineCountEncoding;
		issue.kind = Merge20VerifyIssueKind::EncodingInvalidUtf8;
		issue.resultLine = line;
		if (line >= 1 && line <= static_cast<int>(input.resultLines.size()))
			issue.text = input.resultLines[static_cast<size_t>(line) - 1];
		result.issues.push_back(std::move(issue));
		result.passed = false;
		for (Merge20VerifyCheckResult& cr : result.checks)
		{
			if (cr.check == Merge20VerifyCheck::LineCountEncoding)
			{
				cr.passed = false;
				++cr.issueCount;
			}
		}
	}
	file.result = std::move(result);
	return file;
}

std::vector<Merge20VerifyIssueRow> Merge20VerifyCollectRows(
	const std::vector<Merge20VerifyFileResult>& files)
{
	std::vector<Merge20VerifyIssueRow> rows;
	for (const Merge20VerifyFileResult& file : files)
	{
		for (const Merge20VerifyIssue& issue : file.result.issues)
		{
			Merge20VerifyIssueRow row;
			row.fileIndex = file.fileIndex;
			row.fileName = file.fileName;
			row.issue = issue;
			rows.push_back(std::move(row));
		}
	}
	return rows;
}

Merge20VerifyGateDecision Merge20VerifyShowIssuesAndDecide(CWnd* pParent,
	const std::vector<Merge20VerifyIssueRow>& rows,
	int& jumpFileIndex, int& jumpDiff, int& jumpResultLine)
{
	jumpFileIndex = -1;
	jumpDiff = -1;
	jumpResultLine = -1;
	Merge20VerifyDlg dlg(pParent, rows);
	if (dlg.DoModal() != IDOK)
		return Merge20VerifyGateDecision::Close;
	if (dlg.GetResult() == Merge20VerifyDlg::Result::Proceed)
		return Merge20VerifyGateDecision::Proceed;
	if (dlg.GetResult() == Merge20VerifyDlg::Result::Jump)
	{
		const int rowIndex = dlg.GetJumpRow();
		if (rowIndex >= 0 && rowIndex < static_cast<int>(rows.size()))
		{
			jumpFileIndex = rows[static_cast<size_t>(rowIndex)].fileIndex;
			jumpDiff = rows[static_cast<size_t>(rowIndex)].issue.diffIndex;
			jumpResultLine = rows[static_cast<size_t>(rowIndex)].issue.resultLine;
			return Merge20VerifyGateDecision::Jump;
		}
	}
	return Merge20VerifyGateDecision::Close;
}

void Merge20VerifyJumpToIssue(CMergeDoc& doc, const Merge20VerifyIssue& issue)
{
	const int newPane = doc.Merge20GetNewPane();
	if (issue.diffIndex >= 0)
	{
		doc.SetCurrentDiff(issue.diffIndex);
		if (CMergeResultView* pResultView = doc.GetMergeResultView())
		{
			if (pResultView->GetSafeHwnd() != nullptr)
				pResultView->ScrollToDiff(issue.diffIndex);
		}
		doc.MoveOnLoad(newPane, -1, true);
		return;
	}
	if (issue.resultLine >= 1)
	{
		if (CMergeResultView* pResultView = doc.GetMergeResultView())
		{
			if (pResultView->GetSafeHwnd() != nullptr)
			{
				pResultView->SetCursorPos(CEPoint(0, issue.resultLine - 1));
				pResultView->EnsureVisible(pResultView->GetCursorPos());
				pResultView->TakeFocus();
			}
		}
	}
}

bool Merge20VerifyBeforeOutput(CMergeDoc& doc)
{
	Merge20VerifyFileResult file = Merge20VerifyCheckMergeDocFile(doc);
	if (file.result.passed)
		return true;

	// No dialog in a non-interactive run: log and stop.
	if (theApp.GetNonInteractive())
	{
		const String msg = strutils::format_string1(
			_("Merge20 verification found %1 problem(s); the merge result was not saved."),
			strutils::to_str(static_cast<int>(file.result.issues.size())));
		RootLogger::Info(msg);
		return false;
	}

	// Parent window (the B14 pattern): the result view's frame,
	// else the main window.
	CWnd* pParent = nullptr;
	if (CMergeResultView* pResultView = doc.GetMergeResultView())
	{
		if (pResultView->GetSafeHwnd() != nullptr)
			pParent = static_cast<CWnd*>(pResultView->GetParentFrame());
	}
	if (pParent == nullptr)
		pParent = AfxGetMainWnd();

	const std::vector<Merge20VerifyIssueRow> rows = Merge20VerifyCollectRows({ file });

	int jumpFileIndex = -1, jumpDiff = -1, jumpResultLine = -1;
	const Merge20VerifyGateDecision decision = Merge20VerifyShowIssuesAndDecide(
		pParent, rows, jumpFileIndex, jumpDiff, jumpResultLine);
	if (decision == Merge20VerifyGateDecision::Proceed)
		return true;
	if (decision == Merge20VerifyGateDecision::Jump)
	{
		Merge20VerifyIssue issue;
		issue.diffIndex = jumpDiff;
		issue.resultLine = jumpResultLine;
		Merge20VerifyJumpToIssue(doc, issue);
	}
	return false;
}
