// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20LearnDoc.cpp
 *
 * @brief Adapter between the learning core (Merge20Learn, L0) and the
 * application. See Merge20LearnDoc.h.
 */
#include "StdAfx.h"
#include "Merge20/Merge20LearnDoc.h"
#include "Merge20/Merge20Learn.h"
#include "Merge20/Merge20Session.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "IniOptionsMgr.h"
#include "Environment.h"
#include "paths.h"
#include "UnicodeString.h"
#include "MergeApp.h"
#include "I18n.h"
#include "MergeDoc.h"
#include "MergeResultView.h"
#include "DirDoc.h"
#include "DiffContext.h"
#include "MainFrm.h"
#include "IMergeDoc.h"
#include <combaseapi.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace
{

	// Lazily loaded learning store (see Merge20LearnGetStore): the
	// single in-memory copy, whether it has been loaded yet, and the
	// failure of the load (if any) for the learning dialog (L4).
	Merge20LearnStore g_learnStore;
	bool g_learnStoreLoaded = false;
	String g_learnStoreLoadError;

	// File name of the learning data (fixed: the data is not bound
	// to a folder pair, so no hash is needed, unlike the session file).
	const String kLearnFileName = _T("Merge20Learn.json");

	String Utf8ToString(const std::string& utf8)
	{
		String out;
		Merge20Utf8Decode(utf8, out);
		return out;
	}

	// This PC's source id ("u-<GUID>") from a GUID. StringFromGUID2
	// returns "{XXXXXXXX-...}" in upper case; the braces are dropped
	// and the hex is lowercased so the id looks like the HTML tool's
	// `u-<UUID>` (lower-case hex, no braces).
	String OwnerIdFromGuid(const GUID& guid)
	{
		wchar_t buf[39] = {};
		if (::StringFromGUID2(guid, buf, 39) == 0)
			return String();
		String text(buf);
		if (text.size() >= 2 && text.front() == _T('{') && text.back() == _T('}'))
			text = text.substr(1, text.size() - 2);
		return _T("u-") + strutils::makelower(text);
	}

	// Read the learning file into a fresh store. A missing file is
	// not an error (first run): the caller treats it as an empty
	// store. Returns false with errDetail set when the file exists
	// but cannot be read or parsed.
	bool LoadStoreFromFile(const String& filePath, Merge20LearnStore& out, std::string& errDetail, int& skippedRules)
	{
		skippedRules = 0;
		errDetail.clear();
		std::ifstream in(std::filesystem::path(filePath), std::ios::binary);
		if (!in)
		{
			errDetail = "cannot open learning file";
			return false;
		}
		std::string utf8((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (in.bad())
		{
			errDetail = "failed to read learning file";
			return false;
		}
		Merge20LearnFileHeader header;
		return Merge20LearnFromJson(utf8, kLearnFileName, out, header, errDetail, skippedRules);
	}

	// Write the store as learning JSON v2 (temporary file plus
	// rename, the same way as Merge20SessionSaveToFile in S8).
	bool SaveStoreToFile(const Merge20LearnStore& store, const String& filePath, std::string& errDetail)
	{
		errDetail.clear();
		const String exportedAt = String(CTime::GetCurrentTime().Format(_T("%Y-%m-%dT%H:%M:%S")).GetString());
		std::string utf8;
		if (!Merge20LearnToJson(store, store.GetOwnerId(), kLearnFileName, exportedAt, utf8))
		{
			errDetail = "failed to serialize learning data";
			return false;
		}
		std::filesystem::path target(filePath);
		// The ".tmp" suffix is ASCII, so building the temp path from
		// the String name is safe on both Windows (native wide path)
		// and POSIX (codecvt conversion of an ASCII name).
		String tmpName = filePath;
		tmpName += L".tmp";
		std::filesystem::path tmp(tmpName);
		{
			std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
			if (!out)
			{
				errDetail = "cannot create temporary file";
				return false;
			}
			out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
			out.flush();
			if (!out)
			{
				errDetail = "failed to write learning file";
				std::error_code ec;
				std::filesystem::remove(tmp, ec);
				return false;
			}
		}
		std::error_code ec;
		std::filesystem::rename(tmp, target, ec);
		if (ec)
		{
			// Rename failed (e.g. a locked target): fall back to a
			// direct write so the save still has a chance to succeed.
			std::ofstream out(target, std::ios::binary | std::ios::trunc);
			if (!out)
			{
				errDetail = "cannot create learning file";
				std::filesystem::remove(tmp, ec);
				return false;
			}
			out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
			out.flush();
			if (!out)
			{
				errDetail = "failed to write learning file";
				std::filesystem::remove(tmp, ec);
				return false;
			}
			std::filesystem::remove(tmp, ec);
		}
		return true;
	}

	// Merge20 (Phase 3 L2): helpers for the record / candidate
	// adapter below. They mirror Merge20ClassifyDoc.cpp (same
	// ghost-safe line reading, same per-range pairing).

	// Read one real line of a text buffer (empty when unreadable).
	bool GetBufferLine(const CDiffTextBuffer& buf, int realLine, String& text)
	{
		if (realLine < 0)
			return false;
		return buf.GetLine(buf.ComputeApparentLine(realLine), text);
	}

	int RangeLineCount(const DIFFRANGE& range, int pane)
	{
		const int count = range.end[pane] - range.begin[pane] + 1;
		return count > 0 ? count : 0;
	}

	// Design section 2.2: a strategy maps to source panes. INSERT_OLD
	// and REPLACE_WITH_OLD both resolve to the old pane only ({0});
	// they stay distinct in the learning counts (HTML JSON
	// compatibility) even though the drawn result is the same.
	// KEEP_NEW is the new pane only ({1}): on a deleted-only diff
	// the new side has no lines, so {1} becomes the display-only
	// "<No Lines>" row that is left out when the result is saved
	// (i.e. "not adopted"); the same holds for {0} on an added-only
	// diff. BOTH_OLD_NEW takes both panes ({0, 1}).
	std::vector<int> SrcPanesFromStrategy(Merge20LearnStrategy strategy)
	{
		switch (strategy)
		{
		case Merge20LearnStrategy::InsertOld:
		case Merge20LearnStrategy::ReplaceWithOld:
			return { 0 };
		case Merge20LearnStrategy::KeepNew:
			return { 1 };
		case Merge20LearnStrategy::BothOldNew:
			return { 0, 1 };
		}
		return { 1 };
	}

	// Reverse of SrcPanesFromStrategy (for recording): the chosen
	// source panes plus the diff op give back the strategy. {0} on
	// a deleted-only diff is INSERT_OLD (the HTML tool's default for
	// deletions); {0} on a changed or added-only diff is
	// REPLACE_WITH_OLD. {1} is KEEP_NEW for every op, {0, 1} is
	// BOTH_OLD_NEW. An empty selection is not a judgment (false).
	bool StrategyFromSrcPanes(OP_TYPE op, const std::vector<int>& srcPanes,
		Merge20LearnStrategy& strategyOut)
	{
		const bool hasOld = std::find(srcPanes.begin(), srcPanes.end(), 0) != srcPanes.end();
		const bool hasNew = std::find(srcPanes.begin(), srcPanes.end(), 1) != srcPanes.end();
		if (hasOld && hasNew)
		{
			strategyOut = Merge20LearnStrategy::BothOldNew;
			return true;
		}
		if (hasOld)
		{
			strategyOut = (op == OP_1STONLY) ?
				Merge20LearnStrategy::InsertOld : Merge20LearnStrategy::ReplaceWithOld;
			return true;
		}
		if (hasNew)
		{
			strategyOut = Merge20LearnStrategy::KeepNew;
			return true;
		}
		return false;
	}

	// Identity of a diff for the dismissed set (design section 2.3):
	// op and the real line ranges, so a rescan that moves the diff
	// shows the candidate again, while the same diff stays dismissed.
	String DismissedKey(const DIFFRANGE& range)
	{
		return strutils::format(_T("%d:%d-%d:%d-%d"),
			static_cast<int>(range.op),
			range.begin[0], range.end[0], range.begin[1], range.end[1]);
	}

	// Example file path stored with a recorded judgment (HTML `ex`):
	// the folder-relative path when the file came from a folder
	// compare (the same relative path the session file uses),
	// otherwise the file name. Display-only metadata.
	String ExamplePath(const CMergeDoc& doc)
	{
		const String filePath = doc.GetPath(1);
		if (const CDirDoc* pDirDoc = dynamic_cast<const CDirDoc*>(doc.GetDirDoc()))
		{
			const CDiffContext& ctxt = pDirDoc->GetDiffContext();
			if (ctxt.GetCompareDirs() > 1)
			{
				const String root = ctxt.GetNormalizedPath(1);
				if (!root.empty())
				{
					const String lowerPath = strutils::makelower(filePath);
					String lowerRoot = strutils::makelower(root);
					if (!lowerRoot.empty() && lowerRoot.back() != _T('\\') && lowerRoot.back() != _T('/'))
						lowerRoot += _T('\\');
					if (lowerPath.rfind(lowerRoot, 0) == 0)
						return filePath.substr(lowerRoot.length());
				}
			}
		}
		return paths::FindFileName(filePath);
	}

} // namespace

String Merge20LearnGetDirectory()
{
	if (dynamic_cast<CIniOptionsMgr*>(GetOptionsMgr()) != nullptr)
		return paths::ConcatPath(env::GetProgPath(), _T("Merge20Learn"));
	return paths::ConcatPath(env::GetAppDataPath(), _T("WinMerge\\Merge20Learn"));
}

String Merge20LearnGetFilePath()
{
	return paths::ConcatPath(Merge20LearnGetDirectory(), kLearnFileName);
}

bool Merge20LearnIsEnabled()
{
	return GetOptionsMgr()->GetBool(OPT_MERGE20_LEARNING_ENABLED);
}

String Merge20LearnGetOwnerId()
{
	String ownerId = GetOptionsMgr()->GetString(OPT_MERGE20_LEARNING_OWNER);
	if (!ownerId.empty())
		return ownerId;
	GUID guid{};
	if (::CoCreateGuid(&guid) == S_OK)
		ownerId = OwnerIdFromGuid(guid);
	if (ownerId.empty())
	{
		// CoCreateGuid essentially never fails; fall back to a
		// time-based id so learning still has a stable source id
		// for this run (it is saved like the GUID one, so a later
		// GUID is only generated when the key is still empty).
		ownerId = _T("u-local-") + String(CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S")).GetString());
	}
	GetOptionsMgr()->SaveOption(OPT_MERGE20_LEARNING_OWNER, ownerId);
	return ownerId;
}

String Merge20LearnGetOwnerDisplayName()
{
	return _("Learning on this PC (self)");
}

Merge20LearnStore& Merge20LearnGetStore()
{
	if (!g_learnStoreLoaded)
	{
		g_learnStoreLoaded = true;
		g_learnStore.SetOwnerId(Merge20LearnGetOwnerId());
		const String filePath = Merge20LearnGetFilePath();
		if (paths::DoesPathExist(filePath) == paths::IS_EXISTING_FILE)
		{
			std::string errDetail;
			int skippedRules = 0;
			Merge20LearnStore fresh;
			fresh.SetOwnerId(g_learnStore.GetOwnerId());
			if (!LoadStoreFromFile(filePath, fresh, errDetail, skippedRules))
			{
				// A broken file never blocks startup: keep the empty
				// store and remember the failure for the learning
				// dialog (L4), which reports it when opened.
				g_learnStoreLoadError = Utf8ToString(errDetail);
			}
			else
			{
				g_learnStore = std::move(fresh);
			}
		}
	}
	return g_learnStore;
}

bool Merge20LearnGetLoadError(String& errStr)
{
	// Touch the store so the lazy load has run before reporting.
	(void)Merge20LearnGetStore();
	errStr = g_learnStoreLoadError;
	return !errStr.empty();
}

bool Merge20LearnSaveStore(String& errStr)
{
	errStr.clear();
	const String dir = Merge20LearnGetDirectory();
	if (paths::EnsurePathExist(dir).empty())
	{
		errStr = strutils::format_string1(_("Cannot create the learning folder:\n%1"), dir);
		return false;
	}
	Merge20LearnStore& store = Merge20LearnGetStore();
	const String filePath = Merge20LearnGetFilePath();
	std::string errDetail;
	if (!SaveStoreToFile(store, filePath, errDetail))
	{
		errStr = strutils::format_string1(_("Cannot save the learning data:\n%1"), Utf8ToString(errDetail));
		return false;
	}
	return true;
}

std::vector<Merge20DiffLine> Merge20LearnLinesForDiff(const CMergeDoc& doc, int nDiff)
{
	std::vector<Merge20DiffLine> lines;
	if (doc.m_nBuffers != 2 || nDiff < 0 || nDiff >= doc.m_diffList.GetSize())
		return lines;
	const DIFFRANGE* pRange = doc.m_diffList.DiffRangeAt(nDiff);
	// Context (OP_NONE) and ignored (OP_TRIVIAL) ranges carry no
	// judgment text: trivial diffs have no line texts at all (see
	// Merge20ClassifyDoc), and the record hook only runs for
	// significant diffs, so they never reach the learning keys.
	if (pRange == nullptr || pRange->op == OP_NONE || pRange->op == OP_TRIVIAL)
		return lines;
	const DIFFRANGE& range = *pRange;
	const CDiffTextBuffer& oldBuf = *doc.m_ptBuf[0];
	const CDiffTextBuffer& newBuf = *doc.m_ptBuf[1];
	const int oldLines = RangeLineCount(range, 0);
	const int newLines = RangeLineCount(range, 1);
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
			// Pair the lines in order, like Merge20ClassifyDoc:
			// the leftover lines of the longer side stay OP_DIFF
			// rows with one empty side.
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
	default:
		break;
	}
	return lines;
}

bool Merge20LearnRecordDiffJudgment(CMergeDoc& doc, int nDiff)
{
	// OFF, 3-way, and not-yet-built result panes record nothing.
	// 3-way has no strategy mapping (design section 2.2 is 2-way
	// only), and an unbuilt pane has no segment to judge.
	if (!Merge20LearnIsEnabled() || doc.m_nBuffers != 2 || !doc.GetMergeResultBuildState())
		return false;
	if (nDiff < 0 || nDiff >= doc.m_diffList.GetSize() ||
		!doc.m_diffList.IsDiffSignificant(nDiff))
		return false;
	const DIFFRANGE* pRange = doc.m_diffList.DiffRangeAt(nDiff);
	if (pRange == nullptr)
		return false;
	const MergeResultSegment* pSegment = doc.GetResultSegmentByDiff(nDiff);
	// Only a Chosen segment is an explicit judgment (design section
	// 2.2): an empty selection goes back to Unresolved/Conflict and
	// must not be recorded as "not adopted", and an Auto segment was
	// resolved by the machine, not by the person.
	if (pSegment == nullptr || pSegment->state != ResultSegmentState::Chosen)
		return false;
	Merge20LearnStrategy strategy;
	if (!StrategyFromSrcPanes(pRange->op, pSegment->srcPanes, strategy))
		return false;
	std::vector<Merge20DiffLine> lines = Merge20LearnLinesForDiff(doc, nDiff);
	if (lines.empty())
		return false;
	// The candidate shown for this diff (if any) is the previous
	// learned judgment: choosing a different strategy corrects it.
	const auto& candidates = doc.GetMerge20LearnCandidates();
	const auto it = candidates.find(nDiff);
	const Merge20LearnStrategy* prevLearned = nullptr;
	Merge20LearnStrategy prevStrategy;
	if (it != candidates.end())
	{
		prevStrategy = it->second.strategy;
		prevLearned = &prevStrategy;
	}
	Merge20LearnStore& store = Merge20LearnGetStore();
	const String today = String(CTime::GetCurrentTime().Format(_T("%Y-%m-%d")).GetString());
	Merge20LearnRecordJudgment(store, lines, strategy, prevLearned,
		Merge20LearnGetOwnerId(), Merge20LearnGetOwnerDisplayName(),
		ExamplePath(doc), today);
	return true;
}

void Merge20LearnRefreshCandidates(CMergeDoc& doc)
{
	std::map<int, Merge20LearnCandidate>& candidates = doc.GetMerge20LearnCandidates();
	const bool hadCandidates = !candidates.empty();
	// OFF clears the map (design section 3.5); 3-way docs and docs
	// without a built result pane never get candidates. The result
	// view is invalidated below when the map changed, so the cleared
	// state is visible at once.
	if (!Merge20LearnIsEnabled() || doc.m_nBuffers != 2 || !doc.GetMergeResultBuildState())
	{
		candidates.clear();
		if (hadCandidates && doc.GetMergeResultView() != nullptr &&
			doc.GetMergeResultView()->GetSafeHwnd() != nullptr)
			doc.GetMergeResultView()->Invalidate();
		return;
	}
	const Merge20LearnStore& store = Merge20LearnGetStore();
	const std::set<String>& dismissed = doc.GetMerge20LearnDismissed();
	std::map<int, Merge20LearnCandidate> fresh;
	for (int nDiff = 0; nDiff < doc.m_diffList.GetSize(); ++nDiff)
	{
		const DIFFRANGE* pRange = doc.m_diffList.DiffRangeAt(nDiff);
		if (pRange == nullptr || pRange->op == OP_NONE || pRange->op == OP_TRIVIAL)
			continue;
		// Only diffs still waiting for a decision get a candidate:
		// resolved (Chosen/Auto) and common segments are skipped,
		// so a candidate can never change the result (spec 4(c)).
		const MergeResultSegment* pSegment = doc.GetResultSegmentByDiff(nDiff);
		if (pSegment == nullptr ||
			(pSegment->state != ResultSegmentState::Unresolved &&
			 pSegment->state != ResultSegmentState::Conflict))
			continue;
		// A dismissed diff stays dismissed across rescans.
		if (dismissed.find(DismissedKey(*pRange)) != dismissed.end())
			continue;
		std::vector<Merge20DiffLine> lines = Merge20LearnLinesForDiff(doc, nDiff);
		if (lines.empty())
			continue;
		Merge20LearnCandidate candidate;
		if (!store.Match(lines, candidate))
			continue;
		candidate.diffIdx = nDiff;
		candidate.srcPanes = SrcPanesFromStrategy(candidate.strategy);
		fresh.emplace(nDiff, std::move(candidate));
	}
	// Merge20LearnCandidate has no operator==, so compare the map
	// entry by entry (diffIdx always equals the key).
	bool changed = (fresh.size() != candidates.size());
	if (!changed)
	{
		for (const auto& kv : fresh)
		{
			const auto it = candidates.find(kv.first);
			if (it == candidates.end() ||
				it->second.strategy != kv.second.strategy ||
				it->second.via != kv.second.via ||
				it->second.n != kv.second.n ||
				it->second.srcPanes != kv.second.srcPanes)
			{
				changed = true;
				break;
			}
		}
	}
	candidates = std::move(fresh);
	if (changed && doc.GetMergeResultView() != nullptr &&
		doc.GetMergeResultView()->GetSafeHwnd() != nullptr)
		doc.GetMergeResultView()->Invalidate();
}

void Merge20LearnRefreshAllOpenDocs()
{
	CMainFrame* pMainFrame = dynamic_cast<CMainFrame*>(AfxGetMainWnd());
	if (pMainFrame == nullptr)
		return;
	for (IMergeDoc* pDoc : pMainFrame->GetAllMergeDocuments())
	{
		if (CMergeDoc* pMergeDoc = dynamic_cast<CMergeDoc*>(pDoc))
			Merge20LearnRefreshCandidates(*pMergeDoc);
	}
}
