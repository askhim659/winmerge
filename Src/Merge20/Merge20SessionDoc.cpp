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
#include "DirDoc.h"
#include "MergeDoc.h"
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
#include "MergeApp.h"
#include "I18n.h"
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
		String key = makelower(leftPath);
		key += _T("|");
		key += makelower(rightPath);
		key += _T("|");
		key += makelower(thirdPath);
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
	}

	String Utf8ToString(const std::string& utf8)
	{
		String out;
		Merge20Utf8Decode(utf8, out);
		return out;
	}

} // namespace

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
	const String fileName = _T("Merge20Session_") + FormatHex64(Fnv1a64(SessionKey(leftPath, rightPath, thirdPath))) + _T(".json");
	return paths::ConcatPath(Merge20SessionGetDirectory(), fileName);
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
	if (makelower(loaded.leftPath) != makelower(left) ||
		makelower(loaded.rightPath) != makelower(right) ||
		makelower(loaded.thirdPath) != makelower(third))
	{
		errStr = _("This session file is for a different folder pair.");
		return false;
	}
	session = std::move(loaded);
	return true;
}

Merge20SessionRestoreResult Merge20SessionApplyToDoc(CDirDoc& doc, const Merge20Session& session)
{
	Merge20SessionRestoreResult result;
	if (!doc.HasDiffs())
		return result;
	CDiffContext& ctxt = doc.GetDiffContext();

	// Compare options are saved but NOT applied in v1: applying them
	// needs a rescan. Differing options are listed in the result so
	// the restore report can name them (design section 3.6).
	if (session.compareOptions.recursive != ctxt.m_bRecursive)
		result.optionsDiffer.push_back(_("Recursive"));
	if (session.compareOptions.fileFilter != GetOptionsMgr()->GetString(OPT_FILEFILTER_CURRENT))
		result.optionsDiffer.push_back(_("File filter"));
	if (session.compareOptions.compareMethod != ctxt.GetCompareMethod())
		result.optionsDiffer.push_back(_("Compare method"));
	if (const DIFFOPTIONS* pOpts = ctxt.GetOptions())
	{
		if (session.compareOptions.ignoreWhitespace != pOpts->nIgnoreWhitespace)
			result.optionsDiffer.push_back(_("Ignore whitespace"));
		if (session.compareOptions.ignoreCase != pOpts->bIgnoreCase)
			result.optionsDiffer.push_back(_("Ignore case"));
		if (session.compareOptions.ignoreBlankLines != pOpts->bIgnoreBlankLines)
			result.optionsDiffer.push_back(_("Ignore blank lines"));
		if (session.compareOptions.ignoreEol != pOpts->bIgnoreEol)
			result.optionsDiffer.push_back(_("Ignore EOL differences"));
	}

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
		st.summary = file.summary;
		st.memo = file.memo;
		st.diffJudgments = file.diffs;
		ctxt.SetMerge20SessionState(di.getItemRelativePath(), st);
		++result.restored;
		result.diffsRestored += static_cast<int>(file.diffs.size());
	}

	// Display settings: hidden items first (they affect the list the
	// view rebuilds), then the view's filters, columns and position.
	doc.SetHiddenItems(session.display.hiddenItems);
	if (CDirView* pView = doc.GetMainView())
	{
		pView->Merge20SetDisplayFilter(session.display);
		pView->Merge20ApplyColumnSettings(session.display.columnOrders, session.display.columnWidths);
		pView->Merge20RestoreViewPosition(session.display.focusedPath, session.display.topIndex);
	}
	return result;
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
	if (AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;
	Merge20SessionRestoreResult result = Merge20SessionApplyToDoc(doc, session);
	AfxMessageBox(Merge20SessionFormatRestoreReport(result, skippedFiles).c_str(), MB_ICONINFORMATION);
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
	if (!result.optionsDiffer.empty())
	{
		const String names = strutils::join(result.optionsDiffer.begin(), result.optionsDiffer.end(), String(_T(", ")));
		report += _T("\n") + strutils::format_string1(_("Note: the saved compare options differ from the current ones and were not applied: %1."), names);
	}
	return report;
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
	}
}
