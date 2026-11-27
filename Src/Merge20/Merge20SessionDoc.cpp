// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20SessionDoc.cpp
 *
 * @brief Adapter between the folder/file compare documents and the
 * UI-independent session module (Merge20Session). See Merge20SessionDoc.h.
 */
#include "StdAfx.h"
#include "Merge20/Merge20SessionDoc.h"
#include "Merge20/Merge20Session.h"
#include "Merge20/Merge20FocusDock.h"
#include "Merge20/Merge20Grep.h"
#include "Merge20/Merge20AuditDoc.h"
#include "Merge.h"
#include "DirDoc.h"
#include "MergeDoc.h"
#include "MergeEditView.h"
#include "MergeEditFrm.h"
#include "DiffContext.h"
#include "DiffItem.h"
#include "DirItem.h"
#include "DirView.h"
#include "DirActions.h"
#include "IMergeDoc.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "OptionsDiffOptions.h"
#include "CompareOptions.h"
#include "IniOptionsMgr.h"
#include "Environment.h"
#include "paths.h"
#include "UnicodeString.h"
#include "MergeApp.h"
#include "I18n.h"
#include <algorithm>
#include <map>

namespace
{

	// FNV-1a 64-bit hash of the session key (normalized, lowercased
	// left|right|third paths). The hash keeps the file name short and
	// free of characters illegal in file names; the stored paths are
	// still verified on load, so a hash collision can only refuse a
	// restore, never apply the wrong session.
	uint64_t Fnv1a64(const String& text)
	{
		uint64_t hash = 14695981039346656037ULL;
		for (tchar_t ch : text)
		{
			hash ^= static_cast<uint64_t>(static_cast<unsigned int>(ch));
			hash *= 1099511628211ULL;
		}
		return hash;
	}

	String FormatHex64(uint64_t value)
	{
		static const tchar_t digits[] = _T("0123456789abcdef");
		String out(16, _T('0'));
		for (int i = 15; i >= 0; --i)
		{
			out[static_cast<size_t>(i)] = digits[value & 0xF];
			value >>= 4;
		}
		return out;
	}

	String SessionKey(const String& leftPath, const String& rightPath, const String& thirdPath)
	{
		String key = strutils::makelower(leftPath);
		key += _T("|");
		key += strutils::makelower(rightPath);
		key += _T("|");
		key += strutils::makelower(thirdPath);
		return key;
	}

	// Size of one side of a DIFFITEM: -1 when the side does not exist.
	int64_t SideSize(const DIFFITEM& di, int side)
	{
		const uint64_t size = di.diffFileInfo[side].size;
		return size == DirItem::FILE_SIZE_NONE ? -1 : static_cast<int64_t>(size);
	}

	// Modification time of one side, in microseconds since the epoch:
	// -1 when the side does not exist.
	int64_t SideMtime(const DIFFITEM& di, int side)
	{
		if (di.diffFileInfo[side].size == DirItem::FILE_SIZE_NONE)
			return -1;
		return static_cast<int64_t>(di.diffFileInfo[side].mtime.epochMicroseconds());
	}

	Merge20FileIdentity IdentityOf(const DIFFITEM& di)
	{
		Merge20FileIdentity identity;
		identity.leftSize = SideSize(di, 0);
		identity.leftMtime = SideMtime(di, 0);
		identity.rightSize = SideSize(di, 1);
		identity.rightMtime = SideMtime(di, 1);
		identity.thirdSize = SideSize(di, 2);
		identity.thirdMtime = SideMtime(di, 2);
		identity.nsdiffs = di.nsdiffs;
		identity.nidiffs = di.nidiffs;
		return identity;
	}

	// Find the folder compare item of a merge doc's files (same lookup
	// as CDirDoc::SetMerge20Classification, including the swapped-paths
	// fallback).
	DIFFITEM* FindItemForMergeDoc(CDirDoc& doc, const CMergeDoc& mergeDoc)
	{
		CDiffContext& ctxt = doc.GetDiffContext();
		DIFFITEM* pItem = FindItemFromPaths(ctxt, mergeDoc.m_filePaths);
		if (pItem == nullptr && mergeDoc.m_filePaths.GetSize() >= 2)
		{
			PathContext pathsSwapped(mergeDoc.m_filePaths);
			std::swap(pathsSwapped[0], pathsSwapped[pathsSwapped.GetSize() - 1]);
			pItem = FindItemFromPaths(ctxt, pathsSwapped);
		}
		return pItem;
	}

	// Extract the per-diff judgments of an open merge doc: one entry
	// per judged difference (skipping the equal/trivial ranges and the
	// common segments, which carry no judgment). A merge doc whose
	// result pane was never built has no judgments.
	std::vector<Merge20DiffJudgment> ExtractJudgments(const CMergeDoc& doc)
	{
		std::vector<Merge20DiffJudgment> judgments;
		if (!doc.GetMergeResultBuildState())
			return judgments;
		for (int i = 0; i < doc.m_diffList.GetSize(); ++i)
		{
			const DIFFRANGE* pRange = doc.m_diffList.DiffRangeAt(i);
			if (pRange == nullptr || pRange->op == OP_NONE || pRange->op == OP_TRIVIAL)
				continue;
			const MergeResultSegment* pSeg = doc.GetResultSegmentByDiff(i);
			if (pSeg == nullptr || pSeg->state == ResultSegmentState::Common)
				continue;
			Merge20DiffJudgment judgment;
			judgment.diffIdx = i;
			judgment.op = static_cast<int>(pRange->op);
			for (int pane = 0; pane < 3; ++pane)
			{
				judgment.begin[pane] = pRange->begin[pane];
				judgment.end[pane] = pRange->end[pane];
			}
			judgment.state = static_cast<int>(pSeg->state);
			judgment.srcPanes = pSeg->srcPanes;
			// Merge20 (best-workflow B1b): the explicit conclusion
			// rides on the judgment (it cannot be derived from the
			// segment alone: Keep leaves the segment untouched).
			judgment.conclusion = static_cast<int>(doc.Merge20GetDiffConclusion(i));
			judgments.push_back(std::move(judgment));
		}
		return judgments;
	}

	void StashJudgments(CDirDoc& doc, CMergeDoc& mergeDoc)
	{
		if (!doc.HasDiffs())
			return;
		DIFFITEM* pItem = FindItemForMergeDoc(doc, mergeDoc);
		if (pItem == nullptr)
			return;
		doc.GetDiffContext().SetMerge20DiffJudgments(pItem->getItemRelativePath(), ExtractJudgments(mergeDoc));
		// Merge20 (Phase 4 D5): stash the split groups too, so a
		// save covers the splits of open file compares as well.
		doc.GetDiffContext().SetMerge20SplitGroups(pItem->getItemRelativePath(), mergeDoc.Merge20GetSplitGroups());
	}

	String Utf8ToString(const std::string& utf8)
	{
		String out;
		Merge20Utf8Decode(utf8, out);
		return out;
	}

} // namespace

/**
 * @brief Merge20 (best-workflow B1): public wrapper for the folder
 * item lookup above, so a merge doc can reach its own folder item
 * for the conclusion linkage without duplicating the lookup
 * (including the swapped-paths fallback).
 */
DIFFITEM* Merge20FindItemForMergeDoc(CDirDoc& doc, const CMergeDoc& mergeDoc)
{
	return FindItemForMergeDoc(doc, mergeDoc);
}

String Merge20SessionGetDirectory()
{
	if (dynamic_cast<CIniOptionsMgr*>(GetOptionsMgr()) != nullptr)
		return paths::ConcatPath(env::GetProgPath(), _T("Merge20Sessions"));
	return paths::ConcatPath(env::GetAppDataPath(), _T("WinMerge\\Merge20Sessions"));
}

String Merge20SessionGetFilePath(const CDiffContext& ctxt)
{
	const int nDirs = ctxt.GetCompareDirs();
	return Merge20SessionGetFilePath(
		ctxt.GetNormalizedPath(0),
		nDirs > 1 ? ctxt.GetNormalizedPath(1) : String(),
		nDirs > 2 ? ctxt.GetNormalizedPath(2) : String());
}

String Merge20SessionGetFilePath(const String& leftPath, const String& rightPath, const String& thirdPath)
{
	const String fileName = _T("Merge20Session_") + Merge20SessionPairHashHex(leftPath, rightPath, thirdPath) + _T(".json");
	return paths::ConcatPath(Merge20SessionGetDirectory(), fileName);
}

String Merge20SessionPairHashHex(const String& leftPath, const String& rightPath, const String& thirdPath)
{
	return FormatHex64(Fnv1a64(SessionKey(leftPath, rightPath, thirdPath)));
}

bool Merge20SessionBuildFromDoc(CDirDoc& doc, Merge20Session& session)
{
	if (!doc.HasDiffs())
		return false;
	CDiffContext& ctxt = doc.GetDiffContext();

	Merge20Session fresh;
	fresh.savedAt = String(CTime::GetCurrentTime().Format(_T("%Y-%m-%d %H:%M:%S")).GetString());
	const int nDirs = ctxt.GetCompareDirs();
	fresh.leftPath = ctxt.GetNormalizedPath(0);
	fresh.rightPath = nDirs > 1 ? ctxt.GetNormalizedPath(1) : String();
	fresh.thirdPath = nDirs > 2 ? ctxt.GetNormalizedPath(2) : String();

	fresh.compareOptions.recursive = ctxt.m_bRecursive;
	fresh.compareOptions.fileFilter = GetOptionsMgr()->GetString(OPT_FILEFILTER_CURRENT);
	fresh.compareOptions.compareMethod = ctxt.GetCompareMethod();
	const DIFFOPTIONS* pOpts = ctxt.GetOptions();
	DIFFOPTIONS opts{};
	if (pOpts == nullptr)
		Options::DiffOptions::Load(GetOptionsMgr(), opts);
	else
		opts = *pOpts;
	fresh.compareOptions.ignoreWhitespace = opts.nIgnoreWhitespace;
	fresh.compareOptions.ignoreCase = opts.bIgnoreCase;
	fresh.compareOptions.ignoreBlankLines = opts.bIgnoreBlankLines;
	fresh.compareOptions.ignoreEol = opts.bIgnoreEol;

	fresh.display.hiddenItems = doc.GetHiddenItems();
	if (CDirView* pView = doc.GetMainView())
		pView->Merge20GetDisplaySettings(fresh.display);
	// Merge20 (Phase 4 D11, design section 4.1(b)): dedicated mode,
	// pin, unreviewed-only tour and dedicated zoom are global option
	// keys (D0); the slim flag is read by Merge20GetDisplaySettings
	// above, and the Grep string is the head of the search history.
	fresh.display.dedMode = GetOptionsMgr()->GetBool(OPT_MERGE20_DED_MODE);
	fresh.display.dedPinned = GetOptionsMgr()->GetBool(OPT_MERGE20_DED_PINNED);
	fresh.display.todoOnly = GetOptionsMgr()->GetBool(OPT_MERGE20_TODO_ONLY);
	fresh.display.dedZoom = GetOptionsMgr()->GetInt(OPT_MERGE20_DED_ZOOM);
	// Merge20 (best-workflow B12): remember the file compare open
	// at save time, with its current diff and first visible line,
	// so a restore can reopen it at the same position. The folder
	// item is found through the same lookup as the judgment stash
	// (including the swapped-paths fallback); a merge doc with no
	// folder item (a standalone file pair) leaves no resume target.
	// The scroll position is the active view's top (apparent) line;
	// the panes scroll together, so one pane's top line is the
	// position of the whole compare.
	if (CMergeDoc* pLast = doc.Merge20GetLastOpenMergeDoc())
	{
		if (DIFFITEM* pItem = FindItemForMergeDoc(doc, *pLast))
		{
			fresh.display.resumeRelPath = pItem->getItemRelativePath();
			fresh.display.resumeDiff = pLast->GetCurrentDiff();
			if (CMergeEditView* pView = pLast->GetActiveMergeView())
				fresh.display.resumeTopLine = pView->GetTopLine();
		}
	}
	const std::vector<String> grepHistory = Merge20GrepParseHistory(
		GetOptionsMgr()->GetString(OPT_MERGE20_GREP_HISTORY));
	fresh.display.grepText = grepHistory.empty() ? String() : grepHistory.front();

	for (DIFFITEM* pPos = ctxt.GetFirstDiffPosition(); pPos != nullptr;)
	{
		const DIFFITEM& di = ctxt.GetNextDiffPosition(pPos);
		if (di.diffcode.isDirectory())
			continue;
		Merge20SessionFile file;
		file.relPath = di.getItemRelativePath();
		const Merge20FileState& st = ctxt.GetMerge20FileStates().GetState(file.relPath);
		file.review = st.review;
		file.includeInOutput = ctxt.GetMerge20IncludeInOutput(di);
		file.classified = st.classified;
		file.action = st.action;
		file.risk = st.risk;
		file.nMissing = st.nMissing;
		file.nModified = st.nModified;
		file.nAdded = st.nAdded;
		file.nExcluded = st.nExcluded;
		file.simpleCompare = st.simpleCompare;
		file.summary = st.summary;
		file.memo = st.memo;
		file.leftSize = SideSize(di, 0);
		file.leftMtime = SideMtime(di, 0);
		file.rightSize = SideSize(di, 1);
		file.rightMtime = SideMtime(di, 1);
		file.thirdSize = SideSize(di, 2);
		file.thirdMtime = SideMtime(di, 2);
		file.nsdiffs = di.nsdiffs;
		file.nidiffs = di.nidiffs;
		file.diffs = st.diffJudgments;
		file.splits = st.splitGroups;
		fresh.files.push_back(std::move(file));
	}
	session = std::move(fresh);
	return true;
}

void Merge20SessionRefreshDiffJudgments(CDirDoc& doc)
{
	doc.RefreshMerge20DiffJudgments();
}

void Merge20SessionStashMergeDoc(CDirDoc& doc, IMergeDoc* pMergeDoc)
{
	if (pMergeDoc == nullptr)
		return;
	CMergeDoc* pDoc = dynamic_cast<CMergeDoc*>(pMergeDoc);
	if (pDoc == nullptr)
		return;
	StashJudgments(doc, *pDoc);
}

bool Merge20SessionSaveDoc(CDirDoc& doc, String& filePath, String& errStr)
{
	errStr.clear();
	filePath.clear();
	if (!doc.HasDiffs())
	{
		errStr = _("There is no folder comparison to save yet.");
		return false;
	}
	// Judgments of still-open merge docs are refreshed first, so a
	// save covers work in open file compares too.
	doc.RefreshMerge20DiffJudgments();
	Merge20Session session;
	if (!Merge20SessionBuildFromDoc(doc, session))
	{
		errStr = _("There is no folder comparison to save yet.");
		return false;
	}
	const String dir = Merge20SessionGetDirectory();
	if (paths::EnsurePathExist(dir).empty())
	{
		errStr = strutils::format_string1(_("Cannot create the session folder:\n%1"), dir);
		return false;
	}
	filePath = Merge20SessionGetFilePath(doc.GetDiffContext());
	std::string errDetail;
	if (!Merge20SessionSaveToFile(session, filePath, errDetail))
	{
		errStr = strutils::format_string1(_("Cannot save the session:\n%1"), Utf8ToString(errDetail));
		return false;
	}
	return true;
}

bool Merge20SessionExistsForDoc(CDirDoc& doc, String& filePath)
{
	filePath.clear();
	if (!doc.HasDiffs())
		return false;
	filePath = Merge20SessionGetFilePath(doc.GetDiffContext());
	return paths::DoesPathExist(filePath) == paths::IS_EXISTING_FILE;
}

bool Merge20SessionLoadForDoc(CDirDoc& doc, const String& filePath, Merge20Session& session, String& errStr, int& skippedFiles)
{
	errStr.clear();
	skippedFiles = 0;
	Merge20Session loaded;
	std::string errDetail;
	if (!Merge20SessionLoadFromFile(filePath, loaded, errDetail, skippedFiles))
	{
		errStr = strutils::format_string1(_("Cannot load the session file:\n%1"), Utf8ToString(errDetail));
		return false;
	}
	if (!doc.HasDiffs())
	{
		errStr = _("There is no folder comparison to restore into yet.");
		return false;
	}
	// The file must belong to this folder pair (compared case-insensitively
	// on the normalized paths, as the file name hash does).
	const CDiffContext& ctxt = doc.GetDiffContext();
	const int nDirs = ctxt.GetCompareDirs();
	const String left = ctxt.GetNormalizedPath(0);
	const String right = nDirs > 1 ? ctxt.GetNormalizedPath(1) : String();
	const String third = nDirs > 2 ? ctxt.GetNormalizedPath(2) : String();
	if (strutils::makelower(loaded.leftPath) != strutils::makelower(left) ||
		strutils::makelower(loaded.rightPath) != strutils::makelower(right) ||
		strutils::makelower(loaded.thirdPath) != strutils::makelower(third))
	{
		errStr = _("This session file is for a different folder pair.");
		return false;
	}
	session = std::move(loaded);
	return true;
}

namespace
{

	// Merge20 (Phase 4 D11): names of the saved compare options that
	// differ from the current ones, for the restore report. The
	// display names are the S8 ones, so a report names the same
	// options before and after D11.
	void SessionOptionsDiffer(CDirDoc& doc, const Merge20Session& session, std::vector<String>& differ)
	{
		CDiffContext& ctxt = doc.GetDiffContext();
		if (session.compareOptions.recursive != ctxt.m_bRecursive)
			differ.push_back(_("Recursive"));
		if (session.compareOptions.fileFilter != GetOptionsMgr()->GetString(OPT_FILEFILTER_CURRENT))
			differ.push_back(_("File filter"));
		if (session.compareOptions.compareMethod != ctxt.GetCompareMethod())
			differ.push_back(_("Compare method"));
		if (const DIFFOPTIONS* pOpts = ctxt.GetOptions())
		{
			if (session.compareOptions.ignoreWhitespace != pOpts->nIgnoreWhitespace)
				differ.push_back(_("Ignore whitespace"));
			if (session.compareOptions.ignoreCase != pOpts->bIgnoreCase)
				differ.push_back(_("Ignore case"));
			if (session.compareOptions.ignoreBlankLines != pOpts->bIgnoreBlankLines)
				differ.push_back(_("Ignore blank lines"));
			if (session.compareOptions.ignoreEol != pOpts->bIgnoreEol)
				differ.push_back(_("Ignore EOL differences"));
		}
	}

	// Merge20 (Phase 4 D11): write the saved compare options back
	// to the global options (and the global file filter) so the
	// rescan's InitDiffContext picks them up. The recursive flag is
	// refreshed through RefreshOptions (InitDiffContext does not set
	// m_bRecursive; the compare thread reads it from the context).
	// The saved file filter is the mask/expression the Filters dialog
	// stores (see CMainFrame::OnToolsFilters); SetMaskOrExpression
	// round-trips it, including a named filter (pf:...) whose file is
	// gone (then it is treated as a literal mask).
	void ApplySessionCompareOptions(CDirDoc& doc, const Merge20Session& session)
	{
		COptionsMgr* pOptions = GetOptionsMgr();
		CDiffContext& ctxt = doc.GetDiffContext();
		if (session.compareOptions.recursive != ctxt.m_bRecursive)
			pOptions->SaveOption(OPT_CMP_INCLUDE_SUBDIRS, session.compareOptions.recursive);
		if (session.compareOptions.fileFilter != pOptions->GetString(OPT_FILEFILTER_CURRENT))
		{
			theApp.GetGlobalFileFilter()->SetMaskOrExpression(session.compareOptions.fileFilter);
			pOptions->SaveOption(OPT_FILEFILTER_CURRENT, session.compareOptions.fileFilter);
		}
		if (session.compareOptions.compareMethod != ctxt.GetCompareMethod())
			pOptions->SaveOption(OPT_CMP_METHOD, session.compareOptions.compareMethod);
		DIFFOPTIONS opts{};
		if (const DIFFOPTIONS* pOpts = ctxt.GetOptions())
			opts = *pOpts;
		else
			Options::DiffOptions::Load(pOptions, opts);
		opts.nIgnoreWhitespace = session.compareOptions.ignoreWhitespace;
		opts.bIgnoreCase = session.compareOptions.ignoreCase;
		opts.bIgnoreBlankLines = session.compareOptions.ignoreBlankLines;
		opts.bIgnoreEol = session.compareOptions.ignoreEol;
		Options::DiffOptions::Save(pOptions, opts);
		doc.RefreshOptions();
	}

	// Merge20 (S8, D11): apply the per-file states and the display
	// settings of a session to the current (already compared) list.
	// This is the synchronous half of Merge20SessionApplyToDoc; the
	// deferred half (Merge20SessionApplyPending) calls it after the
	// rescan that applied the saved compare options.
	void ApplySessionStates(CDirDoc& doc, const Merge20Session& session, Merge20SessionRestoreResult& result)
	{
		CDiffContext& ctxt = doc.GetDiffContext();
		// Merge20 (best-workflow B10): a restore rewrites the review
		// states, output flags and diff judgments wholesale, so every
		// undo entry predates the restored states. The history is
		// cleared here (both the synchronous and the deferred paths
		// run through ApplySessionStates); saving a session does not
		// clear it. The manual states this (section 17).
		ctxt.Merge20ClearUndoHistory();
		// Index the current files by relative path.
		std::map<String, const DIFFITEM*> byRelPath;
	for (DIFFITEM* pPos = ctxt.GetFirstDiffPosition(); pPos != nullptr;)
	{
		const DIFFITEM& di = ctxt.GetNextDiffPosition(pPos);
		if (di.diffcode.isDirectory())
			continue;
		byRelPath[di.getItemRelativePath()] = &di;
	}

	for (const Merge20SessionFile& file : session.files)
	{
		auto it = byRelPath.find(file.relPath);
		if (it == byRelPath.end())
		{
			++result.skipped;
			continue;
		}
		const DIFFITEM& di = *it->second;
		if (Merge20SessionFileChanged(file, IdentityOf(di)))
		{
			// Spec section 6.1: a changed file does not inherit its
			// review state. The memo is kept (see ResetMerge20ChangedFile).
			// Merge20 (best-workflow B11): a reset discards a past
			// judgment, so it is logged (the one restore action that
			// writes the audit log; the restore itself does not).
			// Both the synchronous and the deferred restore run
			// through here. A file whose saved review was already
			// TODO logs nothing (the reset changes nothing).
			Merge20AuditLogChangeReset(doc, file.relPath, file.review);
			ctxt.ResetMerge20ChangedFile(di);
			++result.reset;
			continue;
		}
		// Restore the stored state verbatim. The stored output flag is
		// authoritative, so this deliberately does NOT use
		// ApplyMerge20ReviewState (which would recompute the flag).
		Merge20FileState st;
		st.review = file.review;
		st.includeInOutput = file.includeInOutput;
		st.classified = file.classified;
		st.action = file.action;
		st.risk = file.risk;
		st.nMissing = file.nMissing;
		st.nModified = file.nModified;
		st.nAdded = file.nAdded;
		st.nExcluded = file.nExcluded;
		st.simpleCompare = file.simpleCompare;
		st.summary = file.summary;
		st.memo = file.memo;
		st.diffJudgments = file.diffs;
		st.splitGroups = file.splits;
		ctxt.SetMerge20SessionState(di.getItemRelativePath(), st);
		++result.restored;
		result.diffsRestored += static_cast<int>(file.diffs.size());
		// Merge20 (best-workflow B12): the resume target is only
		// carried over when this file was restored unchanged. A
		// file that was reset (changed since the save) is not
		// reopened: its diff indices and scroll lines may have
		// shifted, so reopening at the saved position would mislead.
		// A missing file never reaches here (skipped above).
		if (file.relPath == session.display.resumeRelPath)
		{
			result.resumeRelPath = file.relPath;
			result.resumeDiff = session.display.resumeDiff;
			result.resumeTopLine = session.display.resumeTopLine;
		}
	}

	// Display settings: hidden items first (they affect the list the
	// view rebuilds), then the view's filters, columns and position.
	doc.SetHiddenItems(session.display.hiddenItems);
	if (CDirView* pView = doc.GetMainView())
	{
		pView->Merge20SetDisplayFilter(session.display);
		pView->Merge20ApplyColumnSettings(session.display.columnOrders, session.display.columnWidths);
		pView->Merge20RestoreViewPosition(session.display.focusedPath, session.display.topIndex);
		// Merge20 (Phase 4 D11, design section 4.1(b)): the Grep
		// search string goes back to the head of the search history
		// (and into the open Grep dialog, if any).
		pView->Merge20RestoreGrepText(session.display.grepText);
	}
	// Merge20 (Phase 4 D11, design section 4.1(b)): dedicated mode,
	// pin, unreviewed-only tour and dedicated zoom are global option
	// keys; restoring them makes the next file compare open the way
	// the saved session left it. Open file compares are not forced
	// into dedicated mode by a restore.
	GetOptionsMgr()->SaveOption(OPT_MERGE20_DED_MODE, session.display.dedMode);
	// Merge20 (best-workflow B12, NOTES §53.4 hole): do not write
	// the pin back while a dedicated-mode frame is open. Its
	// IDT_DEDHIDE timer reads the key every 500ms and hides the bar
	// once the flash grace is over and the cursor has been away for
	// 3s, so writing back a saved (or default false) pin would hide
	// the bar of a frame the user is working in. The saved value
	// stays in the session file for the next restore; entering
	// dedicated mode pins on entry anyway (PR #46).
	if (!CMergeEditFrame::HasOpenDedicatedModeFrame())
		GetOptionsMgr()->SaveOption(OPT_MERGE20_DED_PINNED, session.display.dedPinned);
	GetOptionsMgr()->SaveOption(OPT_MERGE20_TODO_ONLY, session.display.todoOnly);
	if (session.display.dedZoom > 0)
		GetOptionsMgr()->SaveOption(OPT_MERGE20_DED_ZOOM, session.display.dedZoom);

	// Merge20 (Panel P1, design section 12.5): the per-file setters
	// above (SetMerge20SessionState / ResetMerge20ChangedFile) do
	// not notify, so a restore would leave every open file-list
	// panel stale until the next activation. Notify once here,
	// after the whole restore, so all open panels refresh with the
	// restored review states, output flags and classifications.
	ctxt.NotifyMerge20FileListChanged();
}

} // namespace

Merge20SessionRestoreResult Merge20SessionApplyToDoc(CDirDoc& doc, const Merge20Session& session, int skippedFiles)
{
	Merge20SessionRestoreResult result;
	if (!doc.HasDiffs())
		return result;
	// Merge20 (Phase 4 D11, design section 5.2/D11): differing
	// compare options are applied now (S8 only noted them). Applying
	// them needs a rescan, so the file states wait until that rescan
	// completes (Merge20SessionApplyPending); the caller shows the
	// interim message until then.
	SessionOptionsDiffer(doc, session, result.optionsDiffer);
	if (!result.optionsDiffer.empty() &&
		doc.m_diffThread.GetThreadState() != CDiffThread::THREAD_COMPARING)
	{
		result.optionsApplied = result.optionsDiffer;
		result.optionsDiffer.clear();
		result.deferred = true;
		ApplySessionCompareOptions(doc, session);
		doc.Merge20SetPendingSession(session, skippedFiles, result.optionsApplied);
		doc.Rescan();
		return result;
	}
	// No option change, or a compare is already running (then the S8
	// behaviour is kept: the states are applied against the current
	// list and the differing options are only named in the report).
	ApplySessionStates(doc, session, result);
	return result;
}

bool Merge20SessionApplyPending(CDirDoc& doc, Merge20SessionRestoreResult& result, int& skippedFiles)
{
	result = Merge20SessionRestoreResult();
	skippedFiles = 0;
	Merge20Session session;
	std::vector<String> optionsApplied;
	if (!doc.Merge20TakePendingSession(session, skippedFiles, optionsApplied))
		return false;
	if (!doc.HasDiffs())
		return false;
	// The folders may have changed while the rescan ran (a header-bar
	// folder switch starts a new compare); never apply a session to
	// another folder pair.
	const CDiffContext& ctxt = doc.GetDiffContext();
	const int nDirs = ctxt.GetCompareDirs();
	const String left = ctxt.GetNormalizedPath(0);
	const String right = nDirs > 1 ? ctxt.GetNormalizedPath(1) : String();
	const String third = nDirs > 2 ? ctxt.GetNormalizedPath(2) : String();
	if (strutils::makelower(session.leftPath) != strutils::makelower(left) ||
		strutils::makelower(session.rightPath) != strutils::makelower(right) ||
		strutils::makelower(session.thirdPath) != strutils::makelower(third))
		return false;
	result.optionsApplied = optionsApplied;
	ApplySessionStates(doc, session, result);
	return true;
}

String Merge20SessionFormatDeferredMessage(const Merge20SessionRestoreResult& result)
{
	const String names = strutils::join(result.optionsApplied.begin(), result.optionsApplied.end(), String(_T(", ")));
	return strutils::format_string1(
		_("The saved compare options were applied and the folders are being compared again: %1.\n\nThe review states, output checks, memos and merge judgments will be restored when the compare finishes."),
		names);
}

void Merge20SessionOfferRestore(CDirDoc& doc)
{
	if (doc.Merge20SessionAsked() || doc.GetGeneratingReport() || !doc.GetReportFile().empty() || !doc.HasDiffs())
		return;
	doc.SetMerge20SessionAsked();

	String filePath;
	if (!Merge20SessionExistsForDoc(doc, filePath))
		return;
	Merge20Session session;
	String errStr;
	int skippedFiles = 0;
	if (!Merge20SessionLoadForDoc(doc, filePath, session, errStr, skippedFiles))
	{
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
		return;
	}
	String msg = strutils::format_string2(
		_("A session saved on %1 was found for these folders (%2 files recorded).\n\nRestore the review states, output checks, memos and merge judgments?"),
		session.savedAt, strutils::to_str(static_cast<int>(session.files.size())));
	if (skippedFiles > 0)
		msg += _T("\n") + strutils::format_string1(_("%1 saved entries could not be read and will be ignored."), strutils::to_str(skippedFiles));
	// Merge20 (best-workflow B12): say that the last open file
	// compare will be opened too, so the user can decline the whole
	// restore when that is not wanted. There is no separate
	// checkbox: the offer is a plain message box, and reopening at
	// the saved position is the point of the restore (a changed
	// target is simply not reopened; see ApplySessionStates).
	if (!session.display.resumeRelPath.empty())
		msg += _T("\n") + strutils::format_string1(_("The file compare \"%1\" that was open last time will also be opened."), session.display.resumeRelPath);
	if (AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;
	Merge20SessionRestoreResult result = Merge20SessionApplyToDoc(doc, session, skippedFiles);
	if (result.deferred)
	{
		// Merge20 (Phase 4 D11): the saved compare options were
		// applied and a rescan started; the final report comes after
		// the pending apply (CDirView::OnUpdateUIMessage).
		AfxMessageBox(Merge20SessionFormatDeferredMessage(result).c_str(), MB_ICONINFORMATION);
		return;
	}
	AfxMessageBox(Merge20SessionFormatRestoreReport(result, skippedFiles).c_str(), MB_ICONINFORMATION);
	// Merge20 (best-workflow B12): reopen the file compare that was
	// open at save time, at its saved diff and scroll position. The
	// report above is shown first, so the reopen does not hide it.
	Merge20SessionResumeLastFileCompare(doc, result);
}

String Merge20SessionFormatRestoreReport(const Merge20SessionRestoreResult& result, int skippedFiles)
{
	String report = strutils::format_string2(
		_("Session restored: %1 files restored, %2 files changed since the save and reset to unreviewed."),
		strutils::to_str(result.restored), strutils::to_str(result.reset));
	if (skippedFiles > 0)
		report += _T("\n") + strutils::format_string1(_("%1 saved entries could not be read and will be ignored."), strutils::to_str(skippedFiles));
	if (result.skipped > 0)
		report += _T("\n") + strutils::format_string1(_("%1 saved files no longer exist in this comparison."), strutils::to_str(result.skipped));
	if (!result.optionsApplied.empty())
	{
		const String names = strutils::join(result.optionsApplied.begin(), result.optionsApplied.end(), String(_T(", ")));
		report += _T("\n") + strutils::format_string1(_("Note: the saved compare options were applied and the folders were compared again: %1."), names);
	}
	if (!result.optionsDiffer.empty())
	{
		const String names = strutils::join(result.optionsDiffer.begin(), result.optionsDiffer.end(), String(_T(", ")));
		report += _T("\n") + strutils::format_string1(_("Note: the saved compare options differ from the current ones and were not applied: %1."), names);
	}
	return report;
}

void Merge20SessionResumeLastFileCompare(CDirDoc& doc, const Merge20SessionRestoreResult& result)
{
	if (result.resumeRelPath.empty())
		return;
	// Already open: activate its frame instead of opening a
	// duplicate (same frame activation as CDirDoc::Merge20MoveToFile).
	CMergeDoc* pMergeDoc = doc.Merge20FindOpenMergeDoc(result.resumeRelPath);
	if (pMergeDoc != nullptr)
	{
		if (POSITION pos = pMergeDoc->GetFirstViewPosition())
		{
			if (CView* pView = pMergeDoc->GetNextView(pos))
			{
				if (CFrameWnd* pFrame = pView->GetParentFrame())
					pFrame->ActivateFrame();
			}
		}
	}
	else
	{
		// Not open: open it quietly through the folder view. A file
		// hidden by the current filter cannot be opened, and a
		// failed open leaves no doc behind; both fall back to the
		// folder list alone, with no error (acceptance (d)).
		const DIFFITEM* pItem = doc.Merge20FindItemByRelPath(result.resumeRelPath);
		if (pItem == nullptr)
			return;
		CDirView* pDirView = doc.GetMainView();
		if (pDirView == nullptr || !pDirView->Merge20OpenFileQuiet(*pItem))
			return;
		pMergeDoc = doc.Merge20FindOpenMergeDoc(result.resumeRelPath);
		if (pMergeDoc == nullptr)
			return;
	}
	// Put the reopened compare back where it was: the saved diff
	// first (SelectDiff sets the current diff and scrolls to it),
	// then the exact saved top line. Both are clamped to the
	// reopened compare, never forced: the diff count and the line
	// count come from the live document.
	CMergeEditView* pView = pMergeDoc->GetActiveMergeView();
	if (pView == nullptr)
		return;
	if (result.resumeDiff >= 0 && result.resumeDiff < pMergeDoc->m_diffList.GetSize())
		pView->SelectDiff(result.resumeDiff, true, false);
	const int nLines = pView->GetLineCount();
	if (nLines > 0)
	{
		const int nTop = std::clamp(result.resumeTopLine, 0, nLines - 1);
		pView->ScrollToSubLine(pView->GetSubLineIndex(nTop));
		pView->UpdateSiblingScrollPos(false);
	}
}

void Merge20SessionRestoreMergeDoc(CMergeDoc& doc)
{
	if (!doc.GetMergeResultBuildState())
		return;
	CDirDoc* pFolderDoc = dynamic_cast<CDirDoc*>(doc.GetDirDoc());
	if (pFolderDoc == nullptr || !pFolderDoc->HasDiffs())
		return;
	DIFFITEM* pItem = FindItemForMergeDoc(*pFolderDoc, doc);
	if (pItem == nullptr)
		return;
	const Merge20FileState& st = pFolderDoc->GetDiffContext().GetMerge20FileStates().GetState(pItem->getItemRelativePath());
	// Merge20 (Phase 4 D5): re-apply the saved split groups first, so
	// the saved per-row judgments below match the split ranges they
	// were saved against. A group whose parent diff no longer matches
	// is skipped; its per-row judgments are then skipped the same way
	// any changed diff is (never forced).
	doc.Merge20ReapplySplitGroups(st.splitGroups);
	if (st.diffJudgments.empty())
		return;
	for (const Merge20DiffJudgment& judgment : st.diffJudgments)
	{
		const int nDiff = judgment.diffIdx;
		if (nDiff < 0 || nDiff >= doc.m_diffList.GetSize())
			continue;
		const DIFFRANGE* pRange = doc.m_diffList.DiffRangeAt(nDiff);
		if (pRange == nullptr || static_cast<int>(pRange->op) != judgment.op)
			continue;
		// The judgment belongs to this diff only when its line ranges
		// still match; a moved or edited diff is skipped, never forced.
		bool matches = true;
		for (int pane = 0; pane < 3; ++pane)
		{
			if (pRange->begin[pane] != judgment.begin[pane] || pRange->end[pane] != judgment.end[pane])
				matches = false;
		}
		if (!matches)
			continue;
		if (judgment.state == static_cast<int>(ResultSegmentState::Auto))
		{
			doc.ResultChooseSources(nDiff, judgment.srcPanes);
			// ResultChooseSources marks the segment Chosen; put the
			// saved Auto state back (same content, same state).
			doc.Merge20SetResultSegmentState(nDiff, ResultSegmentState::Auto);
		}
		else if (judgment.state == static_cast<int>(ResultSegmentState::Chosen))
		{
			doc.ResultChooseSources(nDiff, judgment.srcPanes);
		}
		else if (judgment.state == static_cast<int>(ResultSegmentState::Unresolved) ||
			judgment.state == static_cast<int>(ResultSegmentState::Conflict))
		{
			// The user deliberately left this diff undecided; force
			// it back to unresolved even when the auto-merge would
			// have resolved it.
			doc.ResultChooseSources(nDiff, {});
		}
		// Common and Edited are skipped: Common carries no judgment,
		// and a hand edit cannot be reconstructed from the sources.
		// Merge20 (best-workflow B1b): put the saved conclusion back.
		// The result content above already matches the saved
		// judgment; this only restores the conclusion itself (no
		// linkage: the saved review state and output checkbox are
		// restored separately by Merge20SessionApplyToDoc).
		doc.Merge20SetDiffConclusionRestored(nDiff,
			static_cast<Merge20DiffConclusion>(judgment.conclusion));
	}
	// Merge20 (best-workflow B1b): the restored conclusions change
	// the header progress, so refresh the dock even when no
	// judgment rewrote a segment (e.g. every conclusion is Keep).
	if (CMerge20FocusDock* pDock = doc.GetMerge20FocusDock(); pDock != nullptr)
		pDock->ShowDiff(doc.GetCurrentDiff());
}
