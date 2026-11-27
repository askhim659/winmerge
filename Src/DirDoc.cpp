/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  DirDoc.cpp
 *
 * @brief Implementation file for CDirDoc
 *
 */

#include "StdAfx.h"
#include "DirDoc.h"
#if !defined(__cppcheck__)
#include <boost/range/mfc.hpp>
#endif
#include "Merge.h"
#include "IMergeDoc.h"
#include "CompareOptions.h"
#include "UnicodeString.h"
#include "CompareStats.h"
#include "FilterList.h"
#include "DirView.h"
#include "DirFrame.h"
#include "MainFrm.h"
#include "MergeDoc.h"
#include "paths.h"
#include "locality.h"
#include "7zCommon.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "OptionsDiffOptions.h"
#include "LineFiltersList.h"
#include "SubstitutionFiltersList.h"
#include "FileFilterHelper.h"
#include "FilterExpression.h"
#include "FilterErrorMessages.h"
#include "DirActions.h"
#include "DirScan.h"
#include "Merge20/Merge20CloseWarn.h"
#include "Merge20/Merge20CloseWarnDlg.h"
#include "Merge20/Merge20Measure.h"
#include "Merge20/Merge20SessionDoc.h"
#include "MessageBoxDialog.h"
#include "DirCmpReport.h"
#include "DiffWrapper.h"
#include "FolderCmp.h"
#include "DirViewColItems.h"
#include "RenameMoveDetection.h"
#include "Shell.h"
#include "MergeLogger.h"
#include "MergeTextFormatter.h"
#include <Poco/Semaphore.h>
#include <set>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using boost::begin;
using boost::end;

int CDirDoc::m_nDirsTemp = 2;
// If a folder comparison takes longer than this threshold,
// display a confirmation message when closing the folder comparison window.
static constexpr int COMPARISON_TIME_THRESHOLD_SECONDS = 30;

/////////////////////////////////////////////////////////////////////////////
// CDirDoc

IMPLEMENT_DYNCREATE(CDirDoc, CDocument)

/**
 * @brief Constructor.
 */
CDirDoc::CDirDoc()
: m_pCtxt(nullptr)
, m_pDirView(nullptr)
, m_pCompareStats(nullptr)
, m_bMarkedRescan(false)
, m_pTempPathContext(nullptr)
, m_bGeneratingReport(false)
, m_pReport(nullptr)
, m_compareStart(0)
, m_elapsed(0)
{
	m_nDirs = m_nDirsTemp;

	m_bRO[0] = false;
	m_bRO[1] = false;
	m_bRO[2] = false;
}

/**
 * @brief Destructor.
 *
 * Clears document list and deleted possible archive-temp files.
 */
CDirDoc::~CDirDoc()
{
}

/**
 * @brief Called when new dirdoc is created.
 */
BOOL CDirDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	return TRUE;
}

/**
 * @brief Called when dirdoc is closed.
 */
void CDirDoc::OnCloseDocument()
{
	// Merge20 (S8, design section 3.6): auto-save the session on close
	// when enabled. Merge docs are still open here, so their judgments
	// are included through the refresh in the save. Skipped while a
	// compare is running (the sidecar is incomplete then) and when
	// nothing was compared. A failed save is reported (spec section 7),
	// never lost silently.
	if (m_pCtxt != nullptr &&
		m_diffThread.GetThreadState() != CDiffThread::THREAD_COMPARING &&
		GetOptionsMgr()->GetBool(OPT_MERGE20_SESSION_AUTOSAVE))
	{
		String filePath, errStr;
		if (!Merge20SessionSaveDoc(*this, filePath, errStr))
			AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
	}
	if (m_pDirView)
	{
		if (CDirFrame* pf = m_pDirView->GetParentFrame())
		{
			if (auto* pHeaderBar = pf->GetHeaderInterface())
			{
				pHeaderBar->SetOnSetFocusCallback(nullptr);
				pHeaderBar->SetOnCaptionChangedCallback(nullptr);
				pHeaderBar->SetOnFolderSelectedCallback(nullptr);
				pHeaderBar->SetOnGetRecentItemsCallback(nullptr);
				pHeaderBar->SetOnGetClipboardHistoryCallback(nullptr);
			}
		}
	}
	// Inform all of our merge docs that we're closing
	for (auto pMergeDoc : m_MergeDocs)
		pMergeDoc->DirDocClosing(this);
	// Delete all temporary folders belonging to this document
	while (m_pTempPathContext != nullptr)
		m_pTempPathContext = m_pTempPathContext->DeleteHead();
	__super::OnCloseDocument();
}

BEGIN_MESSAGE_MAP(CDirDoc, CDocument)
	//{{AFX_MSG_MAP(CDirDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	// Progress dialog
	ON_BN_CLICKED(IDC_COMPARISON_STOP, OnBnClickedComparisonStop)
	ON_BN_CLICKED(IDC_COMPARISON_PAUSE, OnBnClickedComparisonPause)
	ON_BN_CLICKED(IDC_COMPARISON_CONTINUE, OnBnClickedComparisonContinue)
	ON_CBN_SELCHANGE(IDC_COMPARISON_CPUCORES, OnCbnSelChangeCPUCores)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CDirDoc serialization

void CDirDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CDirDoc commands

/**
 * @brief Initialise directory compare for given paths.
 *
 * Initialises directory compare with paths given and recursive choice.
 * Previous compare context is first free'd.
 * @param [in] paths Paths to compare
 * @param [in] bRecursive If `true` subdirectories are included to compare.
 */
void CDirDoc::InitCompare(const PathContext & paths, bool bRecursive, CTempPathContext *pTempPathContext)
{
	// Abort previous comparing
	while (m_diffThread.GetThreadState() == CDiffThread::THREAD_COMPARING)
	{
		m_diffThread.Abort();
		Sleep(50);
	}

	m_pDirView->DeleteAllDisplayItems();
	// Anything that can go wrong here will yield an exception.
	// Default implementation of operator new() never returns `nullptr`.
	
	if (m_pCompareStats == nullptr)
		m_pCompareStats.reset(new CompareStats(m_nDirs));

	m_pCtxt.reset(new CDiffContext(paths,
			GetOptionsMgr()->GetInt(OPT_CMP_METHOD)));
	m_pCtxt->m_bRecursive = bRecursive;

	if (pTempPathContext != nullptr)
	{
		int nIndex;
		for (nIndex = 0; nIndex < m_nDirs; nIndex++)
			ApplyDisplayRoot(nIndex, pTempPathContext->m_strDisplayRoot[nIndex]);
		pTempPathContext->m_pParent = m_pTempPathContext;
		m_pTempPathContext = pTempPathContext;
		for (nIndex = 0; nIndex < m_nDirs; nIndex++)
			m_pTempPathContext->m_strRoot[nIndex] = m_pCtxt->GetNormalizedPath(nIndex);
	}

	MergeLogger::LogComparisonStart(paths, m_strDesc, nullptr, nullptr);
}


/**
 * @brief Load line filters to the compare context.
 * Loads linefilters, converts them to UTF-8 and sets them for compare context.
 */
void CDirDoc::LoadLineFilterList(CDiffContext *pCtxt)
{
	ASSERT(pCtxt != nullptr);
	
	bool bFilters = GetOptionsMgr()->GetBool(OPT_LINEFILTER_ENABLED);
	auto filters = theApp.m_pLineFilters->MakeFilterList(false);
	if (!bFilters || !filters->HasRegExps())
	{
		pCtxt->m_pFilterList.reset();
		return;
	}
	pCtxt->m_pFilterList = filters;
}

void CDirDoc::LoadSubstitutionFiltersList(CDiffContext* pCtxt)
{
	ASSERT(pCtxt != nullptr);

	bool SubstitutionFiltersEnabled = theApp.m_pSubstitutionFiltersList->GetEnabled();
	if (!SubstitutionFiltersEnabled || theApp.m_pSubstitutionFiltersList->GetCount() == 0)
	{
		pCtxt->m_pSubstitutionList.reset();
		return;
	}

	pCtxt->m_pSubstitutionList = theApp.m_pSubstitutionFiltersList->MakeSubstitutionList();
}

void CDirDoc::DiffThreadCallback(int& state)
{
	if (state == CDiffThread::EVENT_COMPARE_COMPLETED)
	{
		m_elapsed = clock() - m_compareStart;
		// Merge20 (Phase 4 D0, design section 3.11): the compare
		// worker thread finished; record the scan-completed point.
		Merge20MeasureCompareCompleted();
	}
	PostMessage(m_pDirView->GetSafeHwnd(), MSG_UI_UPDATE, state, false);
}

void CDirDoc::InitDiffContext(CDiffContext *pCtxt)
{
	LoadLineFilterList(pCtxt);
	LoadSubstitutionFiltersList(pCtxt);

	auto* pOptions = GetOptionsMgr();
	DIFFOPTIONS options = {0};
	Options::DiffOptions::Load(pOptions, options);

	pCtxt->CreateCompareOptions(pOptions->GetInt(OPT_CMP_METHOD), options);

	pCtxt->m_iGuessEncodingType = pOptions->GetInt(OPT_CP_DETECT);
	if ((pCtxt->m_iGuessEncodingType >> 16) == 0)
		pCtxt->m_iGuessEncodingType |= 50001 << 16;
	pCtxt->m_bIgnoreSmallTimeDiff = pOptions->GetBool(OPT_IGNORE_SMALL_FILETIME);
	pCtxt->m_bStopAfterFirstDiff = pOptions->GetBool(OPT_CMP_STOP_AFTER_FIRST);
	pCtxt->m_nQuickCompareLimit = pOptions->GetInt(OPT_CMP_QUICK_LIMIT);
	pCtxt->m_nBinaryCompareLimit = pOptions->GetInt(OPT_CMP_BINARY_LIMIT);
	pCtxt->m_bPluginsEnabled = pOptions->GetBool(OPT_PLUGINS_ENABLED);
	pCtxt->m_bWalkUniques = pOptions->GetBool(OPT_CMP_WALK_UNIQUE_DIRS);
	pCtxt->m_bIgnoreReparsePoints = pOptions->GetBool(OPT_CMP_IGNORE_REPARSE_POINTS);
	pCtxt->m_bIgnoreCodepage = pOptions->GetBool(OPT_CMP_IGNORE_CODEPAGE);
	pCtxt->m_bEnableImageCompare = pOptions->GetBool(OPT_CMP_ENABLE_IMGCMP_IN_DIRCMP);
	pCtxt->m_dColorDistanceThreshold = pOptions->GetInt(OPT_CMP_IMG_THRESHOLD) / 1000.0;
	pCtxt->m_bPreferWICDecoder = pOptions->GetBool(OPT_CMP_IMG_PREFER_WIC_DECODER);

	m_imgfileFilter.SetMaskOrExpression(pOptions->GetString(OPT_CMP_IMG_FILEPATTERNS));
	pCtxt->m_pImgfileFilter = &m_imgfileFilter;

	pCtxt->m_pCompareStats = m_pCompareStats.get();

	// Make sure filters are up-to-date
	auto* pGlobalFileFilter = theApp.GetGlobalFileFilter();
	pGlobalFileFilter->ReloadUpdatedFilters();
	m_fileHelper.CloneFrom(pGlobalFileFilter);
	pCtxt->m_piFilterGlobal = &m_fileHelper;
	pCtxt->m_piFilterGlobal->SetDiffContext(pCtxt);
	
	pCtxt->m_pAdditionalCompareExpression.reset();
	const String additionalCompareCondition = pOptions->GetString(OPT_CMP_ADDITIONAL_CONDITION);
	if (!additionalCompareCondition.empty())
	{
		pCtxt->m_pAdditionalCompareExpression = std::make_unique<FilterExpression>(ucr::toUTF8(additionalCompareCondition));
		pCtxt->m_pAdditionalCompareExpression->SetDiffContext(pCtxt);
	}

	if (pOptions->GetInt(OPT_CMP_RENAME_MOVE_DETECTION) > 0)
	{
		const String renameMoveKeyExpression = pOptions->GetString(OPT_CMP_RENAME_MOVE_KEY);
		if (!renameMoveKeyExpression.empty())
		{
			pCtxt->m_pRenameMoveDetection = std::make_unique<RenameMoveDetection>();
			FilterExpression renameMoveKeyExpressionObj(ucr::toUTF8(renameMoveKeyExpression));
			renameMoveKeyExpressionObj.SetDiffContext(pCtxt);
			pCtxt->m_pRenameMoveDetection->SetRenameMoveKeyExpression(&renameMoveKeyExpressionObj);
		}
	}

	std::set<String> nameSet;
	if (m_pDirView)
		for (const auto& name : m_pDirView->GetDirViewColItems()->GetAdditionalPropertyNames())
			nameSet.insert(name);
	for (const auto& name : pGlobalFileFilter->GetPropertyNames())
		nameSet.insert(name);
	if (pCtxt->m_pAdditionalCompareExpression)
		for (const auto& name : pCtxt->m_pAdditionalCompareExpression->GetPropertyNames())
			nameSet.insert(ucr::toTString(name));
	if (pCtxt->m_pRenameMoveDetection)
		for (const auto& name : pCtxt->m_pRenameMoveDetection->GetRenameMoveKeyExpression()->GetPropertyNames())
			nameSet.insert(ucr::toTString(name));
	std::vector<String> names(nameSet.begin(), nameSet.end());
	pCtxt->m_pPropertySystem.reset(new PropertySystem(names));

	pCtxt->m_piPluginInfos = pOptions->GetBool(OPT_PLUGINS_ENABLED) ? &m_pluginman : nullptr;

	CheckFilter();
}

void CDirDoc::CheckFilter()
{
	if (!m_pCtxt || !m_pCtxt->m_piFilterGlobal)
		return;
	for (const auto* error: m_pCtxt->m_piFilterGlobal->GetErrorList())
	{
		const String msg = FormatFilterErrorSummary(*error);
		RootLogger::Error(msg);
	}
	if (m_pCtxt->m_pAdditionalCompareExpression && m_pCtxt->m_pAdditionalCompareExpression->errorCode != 0)
	{
		const String msg = FormatFilterErrorSummary(*m_pCtxt->m_pAdditionalCompareExpression);
		RootLogger::Error(msg);
		m_pCtxt->m_pAdditionalCompareExpression.reset();
	}
	if (m_pCtxt->m_pRenameMoveDetection)
	{
		auto* pRenameMoveKeyExpression = m_pCtxt->m_pRenameMoveDetection->GetRenameMoveKeyExpression();
		if (pRenameMoveKeyExpression && pRenameMoveKeyExpression->errorCode != 0)
		{
			const String msg = FormatFilterErrorSummary(*pRenameMoveKeyExpression);
			RootLogger::Error(msg);
			m_pCtxt->m_pRenameMoveDetection->SetRenameMoveKeyExpression(nullptr);
		}
	}
}

/**
 * @brief Perform directory comparison again from scratch
 */
void CDirDoc::Rescan()
{
	if (m_pCtxt == nullptr)
		return;

	CDirFrame *pf = m_pDirView->GetParentFrame();

	// If we're already doing a rescan, bail out
	UINT threadState = m_diffThread.GetThreadState();
	if (threadState == CDiffThread::THREAD_COMPARING)
		return;

	if (!m_bGeneratingReport)
		m_pCompareStats->Reset();

	m_compareStart = clock();

	// Merge20 (Phase 4 D0, design section 3.11): start the
	// folder-compare timing (compare start -> list finalized).
	Merge20MeasureBeginCompare();

	pf->ShowProgressBar();
	auto* pCmpProgressBar = GetCompProgressBar();
	if (pCmpProgressBar)
	{
		pCmpProgressBar->SetCompareStat(m_pCompareStats.get());
		pCmpProgressBar->StartUpdating();
	}

	if (!m_bGeneratingReport)
		m_pDirView->DeleteAllDisplayItems();
	// Don't clear if only scanning selected items
	if (!m_bMarkedRescan && !m_bGeneratingReport)
	{
		if (m_pCtxt->m_pRenameMoveDetection)
			m_pCtxt->m_pRenameMoveDetection->RemoveAllGroups();
		m_pCtxt->m_pRenameMoveDetection.reset();
		m_pCtxt->RemoveAll();
		m_pCtxt->InitDiffItemList();
		InitDiffContext(m_pCtxt.get());
	}
	else
	{
		auto pRenameMoveDetection = std::move(m_pCtxt->m_pRenameMoveDetection);
		InitDiffContext(m_pCtxt.get());
		m_pCtxt->m_pRenameMoveDetection = std::move(pRenameMoveDetection);
	}

	auto* pHeaderBar = pf->GetHeaderInterface();
	pHeaderBar->SetPaneCount(m_nDirs);
	pf->SetCompareDirs(m_nDirs);
	pHeaderBar->SetOnSetFocusCallback([this](int pane) {
		m_pDirView->SetActivePane(pane);
		GetOptionsMgr()->SaveOption(OPT_ACTIVE_PANE, pane);
	});
	pHeaderBar->SetOnCaptionChangedCallback([this](int pane, const String& sText) {
		m_strDesc[pane] = sText;
		UpdateHeaderPath(pane);
		m_pDirView->SetFocus();
	});
	pHeaderBar->SetOnFolderSelectedCallback([this](int pane, const String& sFolderpath) {
		if (m_diffThread.GetThreadState() == CDiffThread::THREAD_COMPARING)
			return;
		PathContext paths = m_pCtxt->GetNormalizedPaths();
		paths.SetPath(pane, sFolderpath);
		m_strDesc[pane].clear();
		if (m_pTempPathContext != nullptr)
		{
			m_pTempPathContext->m_strDisplayRoot[pane].clear();
			m_pTempPathContext->m_strRoot[pane].clear();
		}
		m_pDirView->SetFocus();
		InitCompare(paths, m_pCtxt->m_bRecursive, nullptr);
		Rescan();
	});
	pHeaderBar->SetDefaultHistoryCallbacks();
	for (int nIndex = 0; nIndex < m_nDirs; nIndex++)
	{
		UpdateHeaderPath(nIndex);
		// draw the headers as inactive ones
		pf->GetHeaderInterface()->SetActive(nIndex, false);
	}
	pf->GetHeaderInterface()->Resize();
	int nPane = GetOptionsMgr()->GetInt(OPT_ACTIVE_PANE);
	m_pDirView->SetActivePane((nPane >= 0 && nPane < m_nDirs) ? nPane : 0);
	m_pDirView->GetParentFrame()->SetStatus(_("Comparing items...").c_str());

	// Show current compare method name and active filter name in statusbar
	pf->SetFilterStatusDisplay(theApp.GetGlobalFileFilter()->GetMaskOrExpression().c_str());
	pf->SetCompareMethodStatusDisplay(m_pCtxt->GetCompareMethod());

	// Folder names to compare are in the compare context
	m_diffThread.SetContext(m_pCtxt.get());
	m_diffThread.SetThreadCount(GetOptionsMgr()->GetInt(OPT_CMP_COMPARE_THREADS));
	m_diffThread.RemoveListener(this, &CDirDoc::DiffThreadCallback);
	m_diffThread.AddListener(this, &CDirDoc::DiffThreadCallback);
	if (m_bGeneratingReport)
	{
		m_diffThread.SetCollectFunction([&](DiffFuncStruct* myStruct) {
			int m = 0;
			if (m_pReport->GetCopyToClipboard())
			{
				++m;
				if (m_pReport->GetReportType() == REPORT_TYPE_SIMPLEHTML)
					++m;
			}
			if (!m_pReport->GetReportFile().empty())
				++m;
			myStruct->context->m_pCompareStats->IncreaseTotalItems(
				(m_pDirView->GetListCtrl().GetItemCount() - (myStruct->context->m_bRecursive ? 0 : 1)) * m);
		});
		m_diffThread.SetCompareFunction([&](DiffFuncStruct* myStruct) {
			m_pReport->SetDiffFuncStruct(myStruct);
			myStruct->pSemaphore->wait();
			String errStr;
			if (m_pReport->GenerateReport(errStr))
			{
				if (errStr.empty())
				{
					if (GetReportFile().empty())
					{
						I18n::MessageBox(IDS_REPORT_SUCCESS, MB_OK | MB_ICONINFORMATION);
						if (GetOptionsMgr()->GetBool(OPT_REPORTFILES_OPENREPORTFILE))
						{
							if (m_pReport->GetReportType() == REPORT_TYPE_SIMPLEHTML)
								shell::Open(m_pReport->GetReportFile().c_str());
							else
								CMergeApp::OpenFileToExternalEditor(m_pReport->GetReportFile());
						}
					}
				}
				else
				{
					String msg = strutils::format_string1(
						_("Error creating the report:\n%1"),
						errStr);
					AfxMessageBox(msg.c_str(), MB_OK | MB_ICONSTOP);
				}
			}
			SetGeneratingReport(false);
			m_tempFile = m_pReport->GetTempFile();
			SetReport(nullptr);
		});
		m_diffThread.SetMarkedRescan(false);
	}
	else if (m_bMarkedRescan)
	{
		m_diffThread.SetCollectFunction([](DiffFuncStruct* myStruct) {
			int nItems = DirScan_UpdateMarkedItems(myStruct, nullptr);
			myStruct->context->m_pCompareStats->IncreaseTotalItems(nItems);
			auto* pRenameMoveDetection = myStruct->context->m_pRenameMoveDetection.get();
			if (pRenameMoveDetection)
			{
				bool doMoveDetection = GetOptionsMgr()->GetInt(OPT_CMP_RENAME_MOVE_DETECTION) > 1;
				pRenameMoveDetection->Detect(*myStruct->context, doMoveDetection);
				int nRenameMoveMergeMode = GetOptionsMgr()->GetInt(OPT_CMP_RENAME_MOVE_MERGE_MODE);
				if (nRenameMoveMergeMode > 0)
					pRenameMoveDetection->Merge(*myStruct->context, nRenameMoveMergeMode > 1);
			}
			});
		m_diffThread.SetCompareFunction([](DiffFuncStruct* myStruct) {
			if (myStruct->context->m_pRenameMoveDetection)
				myStruct->m_collectCompletedEvent.wait();
			DirScan_CompareRequestedItems(myStruct, nullptr);
			});
		m_diffThread.SetMarkedRescan(true);
	}
	else
	{
		m_diffThread.SetCollectFunction([](DiffFuncStruct* myStruct) {
			bool casesensitive = false;
			int depth = myStruct->context->m_bRecursive ? -1 : 0;
			PathContext paths = myStruct->context->GetNormalizedPaths();
			String subdir[3] = {_T(""), _T(""), _T("")}; // blank to start at roots specified in diff context
			// Build results list (except delaying file comparisons until below)
			DirScan_GetItems(paths, subdir, myStruct,
					casesensitive, depth, nullptr, myStruct->context->m_bWalkUniques);
			if (myStruct->context->m_pRenameMoveDetection)
			{
				bool doMoveDetection = GetOptionsMgr()->GetInt(OPT_CMP_RENAME_MOVE_DETECTION) > 1;
				myStruct->context->m_pRenameMoveDetection->Detect(*myStruct->context, doMoveDetection);
				int nRenameMoveMergeMode = GetOptionsMgr()->GetInt(OPT_CMP_RENAME_MOVE_MERGE_MODE);
				if (nRenameMoveMergeMode > 0)
					myStruct->context->m_pRenameMoveDetection->Merge(*myStruct->context, nRenameMoveMergeMode > 1);
			}
		});
		m_diffThread.SetCompareFunction([](DiffFuncStruct* myStruct) {
			if (myStruct->context->m_pRenameMoveDetection)
				myStruct->m_collectCompletedEvent.wait();
			DirScan_CompareItems(myStruct, nullptr);
		});
		m_diffThread.SetMarkedRescan(false);
	}
	m_diffThread.CompareDirectories();
	m_bMarkedRescan = false;
}

/**
 * @brief Empty & reload listview (of files & columns) with comparison results
 * @todo Better solution for special items ("..")?
 */
void CDirDoc::Redisplay()
{
	if (m_pDirView == nullptr)
		return;

	// Do not redisplay an empty CDirView
	// Not only does it not have results, but AddSpecialItems will crash
	// trying to dereference null context pointer to get to paths
	if (!HasDiffs())
		return;

	m_pDirView->Redisplay();
}

CDirView * CDirDoc::GetMainView() const
{
	CDirView *pView = nullptr;
	if (POSITION pos = GetFirstViewPosition())
	{
		pView = static_cast<CDirView*>(GetNextView(pos));
		ASSERT_KINDOF(CDirView, pView);
	}
	return pView;
}

/**
 * @brief Update in-memory diffitem status from disk and update view.
 * @param [in] diffPos POSITION of item in UI list.
 * @param [in] idx index to reload.
 * @note Do not call this function from DirView code! This function
 * calls slow DirView functions to get item position and to update GUI.
 * Use UpdateStatusFromDisk() function instead.
 */
void CDirDoc::ReloadItemStatus(DIFFITEM *diffPos, int idx)
{
	// in case just copied (into existence) or modified
	m_pCtxt->UpdateStatusFromDisk(diffPos, idx);

	int nIdx = m_pDirView->GetItemIndex(diffPos);
	if (nIdx != -1)
	{
		// Update view
		m_pDirView->UpdateDiffItemStatus(nIdx);
	}
}

/**
 * @brief Merge20 (S1): review state of one item, from the sidecar map.
 * @param [in] di Difference item.
 * @return Review state, TODO when the item has no entry yet (or when there
 * is no compare context yet).
 */
ReviewState CDirDoc::GetMerge20ReviewState(const DIFFITEM& di) const
{
	if (m_pCtxt == nullptr)
		return ReviewState::TODO;
	return m_pCtxt->GetMerge20ReviewState(di.getItemRelativePath());
}

/**
 * @brief Merge20 (S1): set the review state of one item in the sidecar map.
 * @param [in] di Difference item.
 * @param [in] state New review state.
 */
void CDirDoc::SetMerge20ReviewState(const DIFFITEM& di, ReviewState state)
{
	if (m_pCtxt == nullptr)
		return;
	m_pCtxt->SetMerge20ReviewState(di, state);
}

/**
 * @brief Merge20 (S2): output checkbox flag of one item.
 * @param [in] di Difference item.
 * @return Stored flag when the item has an entry, otherwise the initial
 * value from its existence category.
 */
bool CDirDoc::GetMerge20IncludeInOutput(const DIFFITEM& di) const
{
	if (m_pCtxt == nullptr)
		return true;
	return m_pCtxt->GetMerge20IncludeInOutput(di);
}

/**
 * @brief Merge20 (S2): set the output checkbox flag of one item.
 * @param [in] di Difference item.
 * @param [in] include New output flag.
 */
void CDirDoc::SetMerge20IncludeInOutput(const DIFFITEM& di, bool include, bool bRecordUndo /*= true*/)
{
	if (m_pCtxt == nullptr)
		return;
	m_pCtxt->SetMerge20IncludeInOutput(di, include, bRecordUndo);
}

/**
 * @brief Merge20 (S2): set the review state and apply the output linkage.
 * @param [in] di Difference item.
 * @param [in] state New review state.
 */
void CDirDoc::ApplyMerge20ReviewState(const DIFFITEM& di, ReviewState state, bool bRecordUndo /*= true*/)
{
	if (m_pCtxt == nullptr)
		return;
	m_pCtxt->ApplyMerge20ReviewState(di, state, bRecordUndo);
}

/**
 * @brief Merge20 (best-workflow B10): true when a judgment undo step is available.
 */
bool CDirDoc::Merge20CanUndo() const
{
	return m_pCtxt != nullptr && m_pCtxt->GetMerge20UndoStack().CanUndo();
}

/**
 * @brief Merge20 (best-workflow B10): true when a judgment redo step is available.
 */
bool CDirDoc::Merge20CanRedo() const
{
	return m_pCtxt != nullptr && m_pCtxt->GetMerge20UndoStack().CanRedo();
}

/**
 * @brief Merge20 (best-workflow B10): open merge doc showing one file of this compare.
 * @param [in] relPath Relative path (DIFFITEM::getItemRelativePath()).
 * @return The open CMergeDoc for the file, or nullptr when the file
 *         is not open (a conclusion entry then waits: TargetNotOpen
 *         leaves it on the stack so it can be retried after the
 *         file is opened).
 */
CMergeDoc* CDirDoc::Merge20FindOpenMergeDoc(const String& relPath)
{
	if (m_pCtxt == nullptr)
		return nullptr;
	for (IMergeDoc* pOpenDoc : m_MergeDocs)
	{
		CMergeDoc* pMergeDoc = dynamic_cast<CMergeDoc*>(pOpenDoc);
		if (pMergeDoc == nullptr)
			continue;
		DIFFITEM* pItem = Merge20FindItemForMergeDoc(*this, *pMergeDoc);
		if (pItem != nullptr && pItem->getItemRelativePath() == relPath)
			return pMergeDoc;
	}
	return nullptr;
}

/**
 * @brief Merge20 (best-workflow B10): apply one undo / redo entry.
 * @param [in] entry Entry to apply.
 * @param [in] bUndo True to rewind (old values), false to re-apply (new values).
 * @return Applied when the values were rewound / re-applied; Stale
 *         when the file vanished or its current values no longer
 *         match the entry (batch check, session restore, changed
 *         file reset, or another path changed it since); TargetNotOpen
 *         when a conclusion entry's file compare is not open.
 *         Review / output changes use the raw setters (no linkage,
 *         no recording); a conclusion goes through
 *         CMergeDoc::Merge20ApplyConclusionUndo, which rewrites the
 *         result segment without any learning record. A split /
 *         regroup goes through CMergeDoc::Merge20ApplySplitUndo
 *         (best-workflow B10 PR2), which calls the same split /
 *         regroup the menus call; a learning acceptance /
 *         dismissal goes through CMergeDoc::Merge20ApplyLearnUndo,
 *         which likewise records nothing in the learning data.
 */
Merge20UndoApplyResult CDirDoc::Merge20ApplyUndoEntry(const Merge20UndoEntry& entry, bool bUndo)
{
	if (m_pCtxt == nullptr)
		return Merge20UndoApplyResult::Stale;
	CDiffContext& ctxt = *m_pCtxt;
	const DIFFITEM* pItem = nullptr;
	for (DIFFITEM* pPos = ctxt.GetFirstDiffPosition(); pPos != nullptr;)
	{
		const DIFFITEM& di = ctxt.GetNextDiffPosition(pPos);
		if (di.diffcode.isDirectory())
			continue;
		if (di.getItemRelativePath() == entry.relPath)
		{
			pItem = &di;
			break;
		}
	}
	if (pItem == nullptr)
		return Merge20UndoApplyResult::Stale;
	const ReviewState expectedReview = bUndo ? entry.newReview : entry.oldReview;
	const bool bExpectedInclude = bUndo ? entry.newInclude : entry.oldInclude;
	switch (entry.kind)
	{
	case Merge20UndoKind::ReviewState:
		if (ctxt.GetMerge20ReviewState(entry.relPath) != expectedReview ||
			ctxt.GetMerge20IncludeInOutput(*pItem) != bExpectedInclude)
			return Merge20UndoApplyResult::Stale;
		ctxt.SetMerge20ReviewState(entry.relPath, bUndo ? entry.oldReview : entry.newReview);
		ctxt.SetMerge20IncludeInOutput(entry.relPath, bUndo ? entry.oldInclude : entry.newInclude);
		Merge20UpdateDirViewItem(*pItem);
		return Merge20UndoApplyResult::Applied;
	case Merge20UndoKind::IncludeInOutput:
		if (ctxt.GetMerge20IncludeInOutput(*pItem) != bExpectedInclude)
			return Merge20UndoApplyResult::Stale;
		ctxt.SetMerge20IncludeInOutput(entry.relPath, bUndo ? entry.oldInclude : entry.newInclude);
		Merge20UpdateDirViewItem(*pItem);
		return Merge20UndoApplyResult::Applied;
	case Merge20UndoKind::DiffConclusion:
	{
		CMergeDoc* pMergeDoc = Merge20FindOpenMergeDoc(entry.relPath);
		if (pMergeDoc == nullptr)
			return Merge20UndoApplyResult::TargetNotOpen;
		return pMergeDoc->Merge20ApplyConclusionUndo(entry, bUndo);
	}
	case Merge20UndoKind::SplitDiff:
	case Merge20UndoKind::RegroupDiff:
	{
		CMergeDoc* pMergeDoc = Merge20FindOpenMergeDoc(entry.relPath);
		if (pMergeDoc == nullptr)
			return Merge20UndoApplyResult::TargetNotOpen;
		return pMergeDoc->Merge20ApplySplitUndo(entry, bUndo);
	}
	case Merge20UndoKind::LearnAccept:
	case Merge20UndoKind::LearnDismiss:
	{
		CMergeDoc* pMergeDoc = Merge20FindOpenMergeDoc(entry.relPath);
		if (pMergeDoc == nullptr)
			return Merge20UndoApplyResult::TargetNotOpen;
		return pMergeDoc->Merge20ApplyLearnUndo(entry, bUndo);
	}
	default:
		return Merge20UndoApplyResult::Stale;
	}
}

/**
 * @brief Merge20 (best-workflow B10): undo the newest judgment operation.
 * @return Applied when rewound. A stale entry is dropped (never
 *         offered as redo); a not-open conclusion entry is left in
 *         place so it can be retried after the file is opened.
 */
Merge20UndoApplyResult CDirDoc::Merge20Undo()
{
	if (m_pCtxt == nullptr)
		return Merge20UndoApplyResult::NothingToUndo;
	Merge20UndoStack& stack = m_pCtxt->GetMerge20UndoStack();
	const Merge20UndoEntry* pTop = stack.PeekUndo();
	if (pTop == nullptr)
		return Merge20UndoApplyResult::NothingToUndo;
	const Merge20UndoEntry entry = *pTop;
	const Merge20UndoApplyResult result = Merge20ApplyUndoEntry(entry, true);
	if (result == Merge20UndoApplyResult::TargetNotOpen)
		return result;
	if (result == Merge20UndoApplyResult::Applied)
		stack.PopUndoToRedo();
	else
		stack.DiscardUndoTop();
	return result;
}

/**
 * @brief Merge20 (best-workflow B10): redo the newest undone judgment operation.
 * @return Applied when re-applied. A stale entry is dropped; a
 *         not-open conclusion entry is left in place.
 */
Merge20UndoApplyResult CDirDoc::Merge20Redo()
{
	if (m_pCtxt == nullptr)
		return Merge20UndoApplyResult::NothingToRedo;
	Merge20UndoStack& stack = m_pCtxt->GetMerge20UndoStack();
	const Merge20UndoEntry* pTop = stack.PeekRedo();
	if (pTop == nullptr)
		return Merge20UndoApplyResult::NothingToRedo;
	const Merge20UndoEntry entry = *pTop;
	const Merge20UndoApplyResult result = Merge20ApplyUndoEntry(entry, false);
	if (result == Merge20UndoApplyResult::TargetNotOpen)
		return result;
	if (result == Merge20UndoApplyResult::Applied)
		stack.PopRedoToUndo();
	else
		stack.DiscardRedoTop();
	return result;
}

namespace
{
/**
 * @brief Merge20 (Phase 4 D2): find the folder-compare item for a merge
 * doc's file paths, with the swapped-paths fallback (same lookup as
 * CDirDoc::UpdateChangedItem and SetMerge20Classification). Returns
 * nullptr when the paths belong to no item (e.g. a file compare opened
 * standalone, or a renamed item).
 */
DIFFITEM* FindMerge20ItemFromPaths(const CDiffContext& ctxt, const PathContext& paths)
{
	DIFFITEM* pos = FindItemFromPaths(ctxt, paths);
	if (pos == nullptr && paths.GetSize() >= 2)
	{
		PathContext pathsSwapped(paths);
		std::swap(pathsSwapped[0], pathsSwapped[pathsSwapped.GetSize() - 1]);
		pos = FindItemFromPaths(ctxt, pathsSwapped);
	}
	return pos;
}

/**
 * @brief Merge20 (Panel P3): true when an open merge doc shows the
 * folder item whose compare paths are given. One pane matches when
 * the paths are equal, or when the compare path is the NUL device
 * (a side missing from the folder compare: CDiffContext::
 * GetComparePaths uses it) and the doc stores an empty path there
 * (CDirView::Open leaves the FileLocation unset for a missing file).
 * A single pane path identifies the item inside one folder compare,
 * but every pane is checked so a doc opened on its own (or on
 * renamed files) never matches by accident.
 */
bool Merge20DocMatchesItem(IMergeDoc* pDoc, const PathContext& cmpPaths)
{
	if (pDoc == nullptr || pDoc->GetFileCount() != cmpPaths.GetSize())
		return false;
	for (int i = 0; i < cmpPaths.GetSize(); ++i)
	{
		const String docPath = pDoc->GetPath(i);
		const String cmpPath = cmpPaths[i];
		if (docPath == cmpPath)
			continue;
		if (cmpPath == paths::NATIVE_NULL_DEVICE_NAME && docPath.empty())
			continue;
		return false;
	}
	return true;
}
} // namespace

/**
 * @brief Merge20 (Phase 4 D2, IDirDoc): review state of the file a merge
 * doc shows, found by the merge doc's paths.
 * @param [in] paths Merge doc's file paths.
 * @return Review state, TODO when the item has no entry yet, when the
 * paths belong to no item, or when there is no compare context yet.
 */
ReviewState CDirDoc::GetMerge20ReviewState(const PathContext& paths) const
{
	if (m_pCtxt == nullptr)
		return ReviewState::TODO;
	DIFFITEM* pos = FindMerge20ItemFromPaths(*m_pCtxt, paths);
	if (pos == nullptr)
		return ReviewState::TODO;
	return m_pCtxt->GetMerge20ReviewState(pos->getItemRelativePath());
}

/**
 * @brief Merge20 (Phase 4 D2, IDirDoc): set the review state of the file
 * a merge doc shows, with the S2 output-flag linkage, and repaint the
 * folder-compare row (same repaint as SetMerge20Classification).
 * @param [in] paths Merge doc's file paths.
 * @param [in] state New review state.
 */
void CDirDoc::ApplyMerge20ReviewState(const PathContext& paths, ReviewState state)
{
	if (m_pCtxt == nullptr)
		return;
	DIFFITEM* pos = FindMerge20ItemFromPaths(*m_pCtxt, paths);
	if (pos == nullptr)
		return;
	m_pCtxt->ApplyMerge20ReviewState(*pos, state);
	if (m_pDirView != nullptr)
	{
		const int nIdx = m_pDirView->GetItemIndex(pos);
		if (nIdx != -1)
			m_pDirView->UpdateDiffItemStatus(nIdx);
	}
}

/**
 * @brief Merge20 (Phase 4 D3, IDirDoc): position of the file a merge
 * doc shows in the folder-compare list, for the dedicated bar's file
 * name + (n/m) display.
 * @param [in] paths Merge doc's file paths.
 * @param [out] nPos 1-based ordinal among the patrol set (0 when the
 *              file is not in it).
 * @param [out] nTotal Number of files in the patrol set.
 * @param [in] bTodoOnly False counts all displayed files, true counts
 *             only the unreviewed (TODO) files.
 * @return False when there is no compare context, no folder view, or
 *         the paths belong to no displayed file row.
 */
bool CDirDoc::GetMerge20FilePosition(const PathContext& paths, int& nPos, int& nTotal, bool bTodoOnly) const
{
	nPos = 0;
	nTotal = 0;
	if (m_pCtxt == nullptr || m_pDirView == nullptr)
		return false;
	DIFFITEM* pos = FindMerge20ItemFromPaths(*m_pCtxt, paths);
	if (pos == nullptr)
		return false;
	return m_pDirView->Merge20GetFilePosition(*pos, nPos, nTotal, bTodoOnly);
}

/**
 * @brief Merge20 (Panel P3, design section 12.7): open one folder item
 * in a file compare, from the file-list panel (P-R7).
 *
 * The panel's row click asks for one file: when it is already the
 * current merge doc's file, nothing happens (the panel already
 * selects it); when it is open in another merge doc of this folder
 * compare, that frame is activated instead of opening a duplicate;
 * otherwise the current merge doc is closed first (like
 * MoveToNextFile: the panel replaces the compare in place, the HTML
 * selFile behaviour) and the file is opened through the folder view,
 * so the folder selection and the open logic stay in one place.
 * A cancelled close (the user keeps unsaved changes) leaves the
 * current file open and opens nothing. The caller (the file compare
 * frame) must not touch the frame after this call when it closed
 * its own doc: CloseNow destroys the frame synchronously.
 * @param [in] pMergeDoc Current merge doc (nullptr when called
 *             without one; then only the open step runs).
 * @param [in] di Folder item to open.
 */
void CDirDoc::Merge20MoveToFile(IMergeDoc* pMergeDoc, const DIFFITEM& di)
{
	if (m_pCtxt == nullptr)
		return;
	PathContext cmpPaths;
	m_pCtxt->GetComparePaths(di, cmpPaths);
	// Already showing this file: nothing to do.
	if (Merge20DocMatchesItem(pMergeDoc, cmpPaths))
		return;
	// Already open in another frame of this folder compare:
	// activate that frame instead of opening a duplicate.
	for (auto pOpenDoc : m_MergeDocs)
	{
		if (pOpenDoc == nullptr || pOpenDoc == pMergeDoc)
			continue;
		if (!Merge20DocMatchesItem(pOpenDoc, cmpPaths))
			continue;
		if (CDocument* pCd = dynamic_cast<CDocument*>(pOpenDoc))
		{
			if (POSITION pos = pCd->GetFirstViewPosition())
			{
				if (CView* pView = pCd->GetNextView(pos))
				{
					if (CFrameWnd* pFrame = pView->GetParentFrame())
					{
						pFrame->ActivateFrame();
						return;
					}
				}
			}
		}
	}
	// Otherwise close the current file compare first (like
	// MoveToNextFile). A cancelled close opens nothing.
	if (pMergeDoc != nullptr && !pMergeDoc->CloseNow())
		return;
	if (m_pDirView != nullptr)
		m_pDirView->Merge20OpenFile(di);
}

/**
 * @brief Merge20 (Panel P3): relative path of the file a merge doc
 * shows, for the panel's selection follow (P-R7).
 *
 * The full-path lookup (FindMerge20ItemFromPaths, the D2 rule) fails
 * for one-sided files: the merge doc stores an empty path for the
 * missing side, and FindItemFromPaths needs every pane under a base
 * folder. The fallback matches on any single existing pane instead:
 * inside one folder compare a pane path belongs to exactly one item.
 * @param [in] paths Merge doc's file paths.
 * @param [out] relPath Relative path of the folder item, untouched
 *             when false is returned.
 * @return False when there is no compare context, or the paths
 *         belong to no item (a file compare opened on its own, or
 *         a file switched to a path outside the compare).
 */
bool CDirDoc::Merge20FindRelPath(const PathContext& paths, String& relPath) const
{
	if (m_pCtxt == nullptr)
		return false;
	if (DIFFITEM* pos = FindMerge20ItemFromPaths(*m_pCtxt, paths))
	{
		relPath = pos->getItemRelativePath();
		return true;
	}
	for (DIFFITEM* p = m_pCtxt->GetFirstDiffPosition(); p != nullptr;)
	{
		const DIFFITEM& di = m_pCtxt->GetNextDiffPosition(p);
		if (di.diffcode.isDirectory())
			continue;
		PathContext cmpPaths;
		m_pCtxt->GetComparePaths(di, cmpPaths);
		if (cmpPaths.GetSize() != paths.GetSize())
			continue;
		for (int i = 0; i < paths.GetSize(); ++i)
		{
			if (!paths[i].empty() && paths[i] == cmpPaths[i])
			{
				relPath = di.getItemRelativePath();
				return true;
			}
		}
	}
	return false;
}

/**
 * @brief Merge20 (Panel P3): folder item by relative path (the
 * panel's stable row key).
 * @param [in] relPath DIFFITEM::getItemRelativePath().
 * @return Folder item, or nullptr when there is no compare context
 *         or no item with that path (removed by a rescan).
 */
const DIFFITEM* CDirDoc::Merge20FindItemByRelPath(const String& relPath) const
{
	if (m_pCtxt == nullptr)
		return nullptr;
	for (DIFFITEM* p = m_pCtxt->GetFirstDiffPosition(); p != nullptr;)
	{
		const DIFFITEM& di = m_pCtxt->GetNextDiffPosition(p);
		if (di.getItemRelativePath() == relPath)
			return &di;
	}
	return nullptr;
}

/**
 * @brief Merge20 (Panel P3): repaint one folder-list row and update
 * the folder status-bar count after a panel-initiated change (P-R8).
 * The sidecar setters notify the panels, but the folder list itself
 * is only repainted here (its values are read on demand, so a
 * repaint is enough for the row) and the count only by
 * UpdateMerge20StatusCount (S3). GetItemIndex returns 0 on a miss
 * (upstream behaviour, not -1), so the row is verified through
 * GetItemKey before repainting: a hidden row is skipped instead of
 * repainting row 0.
 * @param [in] di Changed folder item.
 */
void CDirDoc::Merge20UpdateDirViewItem(const DIFFITEM& di)
{
	if (m_pDirView == nullptr)
		return;
	const int nIdx = m_pDirView->GetItemIndex(const_cast<DIFFITEM*>(&di));
	if (nIdx >= 0 && nIdx < m_pDirView->GetListCtrl().GetItemCount() &&
		m_pDirView->GetItemKey(nIdx) == &di)
	{
		m_pDirView->UpdateDiffItemStatus(static_cast<UINT>(nIdx));
	}
	m_pDirView->UpdateMerge20StatusCount();
}

/**
 * @brief Merge20 (Panel P3): refresh the folder list and its
 * status-bar count after a panel-initiated bulk change (P-R8:
 * batch check, directory check/uncheck). One invalidate plus one
 * count update for the whole batch (the S3 folder-side rule), not
 * one per file.
 */
void CDirDoc::Merge20UpdateDirViewStatus()
{
	if (m_pDirView == nullptr)
		return;
	m_pDirView->Invalidate();
	m_pDirView->UpdateMerge20StatusCount();
}

/**
 * @brief Merge20 (S8): audit memo of one item, from the sidecar map.
 * @param [in] di Difference item.
 * @return Memo text, empty when the item has no entry yet (or when
 * there is no compare context yet).
 */
String CDirDoc::GetMerge20Memo(const DIFFITEM& di) const
{
	if (m_pCtxt == nullptr)
		return String();
	return m_pCtxt->GetMerge20Memo(di.getItemRelativePath());
}

/**
 * @brief Merge20 (S8): set the audit memo of one item in the sidecar map.
 * @param [in] di Difference item.
 * @param [in] memo New memo text.
 */
void CDirDoc::SetMerge20Memo(const DIFFITEM& di, const String& memo)
{
	if (m_pCtxt == nullptr)
		return;
	m_pCtxt->SetMerge20Memo(di.getItemRelativePath(), memo);
}

/**
 * @brief Merge20 (S8): refresh the diff judgments of all still-open
 * merge docs into the sidecar map. Called before a session save, so a
 * save covers work in open file compares too.
 */
void CDirDoc::RefreshMerge20DiffJudgments()
{
	for (auto pMergeDoc : m_MergeDocs)
		Merge20SessionStashMergeDoc(*this, pMergeDoc);
}

/**
 * @brief Merge20 (Phase 4 D11): stash a session for apply after the
 * rescan that applied its saved compare options (see
 * Merge20SessionApplyToDoc). Overwrites any earlier stash: a newer
 * restore replaces the pending one.
 */
void CDirDoc::Merge20SetPendingSession(const Merge20Session& session, int skippedFiles, const std::vector<String>& optionsApplied)
{
	m_merge20PendingSession = session;
	m_merge20PendingSkippedFiles = skippedFiles;
	m_merge20PendingOptionsApplied = optionsApplied;
	m_bMerge20HasPendingSession = true;
}

/**
 * @brief Merge20 (Phase 4 D11): move the stashed session out and
 * clear the stash, so a later compare completion does not apply it
 * twice. Returns false when nothing was stashed.
 */
bool CDirDoc::Merge20TakePendingSession(Merge20Session& session, int& skippedFiles, std::vector<String>& optionsApplied)
{
	if (!m_bMerge20HasPendingSession)
		return false;
	session = std::move(m_merge20PendingSession);
	skippedFiles = m_merge20PendingSkippedFiles;
	optionsApplied = std::move(m_merge20PendingOptionsApplied);
	m_merge20PendingSession = Merge20Session();
	m_merge20PendingSkippedFiles = 0;
	m_merge20PendingOptionsApplied.clear();
	m_bMerge20HasPendingSession = false;
	return true;
}

/**
 * @brief Merge20 (S4, IDirDoc): store the classification computed by a
 * merge doc after a rescan.
 * @param [in] paths Merge doc's file paths, used to find the folder
 * compare item (same lookup as UpdateChangedItem, including the
 * swapped-paths fallback).
 * @param [in] state Classification result from Merge20ClassifyDoc.
 *
 * The existence category of the item refines the computed action:
 * an old-only file becomes COPY_OLD_FILE and a new-only file becomes
 * SKIP_NEW_FILE (the diff list alone cannot tell a whole old-only
 * file from a missing-in-new hunk), with the summary reworded the
 * way the HTML tool words it ("Old-only file (N lines)"). The risk
 * stays as computed. The row is repainted so the new columns show
 * up without a rescan; the review state and output flag are kept.
 */
void CDirDoc::SetMerge20Classification(const PathContext& paths, const Merge20FileState& state)
{
	if (m_pCtxt == nullptr)
		return;
	DIFFITEM* pos = FindItemFromPaths(*m_pCtxt, paths);
	if (pos == nullptr && paths.GetSize() >= 2)
	{
		PathContext pathsSwapped(paths);
		std::swap(pathsSwapped[0], pathsSwapped[pathsSwapped.GetSize() - 1]);
		pos = FindItemFromPaths(*m_pCtxt, pathsSwapped);
	}
	if (pos == nullptr)
		return;
	Merge20FileState st = state;
	if (m_pCtxt->GetCompareDirs() < 3)
	{
		if (pos->diffcode.isSideFirstOnly())
		{
			st.action = ActionCategory::COPY_OLD_FILE;
			st.summary = strutils::format_string1(_("Old-only file (%1 lines)"), locality::NumToLocaleStr(st.nMissing));
		}
		else if (pos->diffcode.isSideSecondOnly())
		{
			st.action = ActionCategory::SKIP_NEW_FILE;
			st.summary = strutils::format_string1(_("New-only file (%1 lines)"), locality::NumToLocaleStr(st.nAdded));
		}
	}
	m_pCtxt->SetMerge20Classification(pos->getItemRelativePath(), st);
	if (m_pDirView != nullptr)
	{
		const int nIdx = m_pDirView->GetItemIndex(pos);
		if (nIdx != -1)
			m_pDirView->UpdateDiffItemStatus(nIdx);
	}
}

void CDirDoc::InitStatusStrings()
{

}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CDirDoc::UpdateResources()
{
	if (m_pDirView != nullptr)
		m_pDirView->UpdateResources();

	SetTitle(nullptr);

	Redisplay();
}

/**
 * @brief Stash away our view pointer.
 */
void CDirDoc::SetDirView(CDirView * newView)
{
	m_pDirView = newView;
	// MFC has a view list for us, so lets check against it
	POSITION pos = GetFirstViewPosition();
	ASSERT(m_pDirView == static_cast<CDirView *>(GetNextView(pos))); // verify that our stashed pointer is the same as MFC's
}

/**
 * @brief A new MergeDoc has been opened.
 */
void CDirDoc::AddMergeDoc(IMergeDoc * pMergeDoc)
{
	ASSERT(pMergeDoc != nullptr);
	m_MergeDocs.AddTail(pMergeDoc);
}

/**
 * @brief MergeDoc informs us it is closing.
 */
void CDirDoc::MergeDocClosing(IMergeDoc * pMergeDoc)
{
	ASSERT(pMergeDoc != nullptr);
	// Merge20 (S8): stash the closing merge doc's diff judgments in
	// the sidecar map before the doc (and its result pane) is gone,
	// so a later session save still covers them.
	Merge20SessionStashMergeDoc(*this, pMergeDoc);
	if (POSITION pos = m_MergeDocs.CPtrList::Find(pMergeDoc))
		m_MergeDocs.RemoveAt(pos);
	else
		ASSERT(false);

	// If dir compare is empty (no compare results) and we are not closing
	// because of reuse close also dir compare
	if (m_pDirView != nullptr)
	{
		if (m_pCtxt == nullptr)
			m_pDirView->PostMessage(WM_COMMAND, ID_FILE_CLOSE);
	}
	else if (m_MergeDocs.GetCount() == 0)
	{
		delete this;
	}
}

/**
 * @brief Close MergeDocs opened from DirDoc.
 *
 * Asks confirmation for docs containing unsaved data and then
 * closes MergeDocs.
 * @return `true` if success, `false` if user canceled or closing failed
 */
bool CDirDoc::CloseMergeDocs()
{
	while (!m_MergeDocs.IsEmpty())
		if (!m_MergeDocs.GetTail()->CloseNow())
			return false;
	return true;
}

/**
 * @brief Update changed item's compare status
 * @param [in] paths Paths for files we update
 * @param [in] nDiffs Total amount of differences
 * @param [in] nTrivialDiffs Amount of ignored differences
 * @param [in] bIdentical `true` if files became identical, `false` otherwise.
 */
void CDirDoc::UpdateChangedItem(const PathContext &paths,
	UINT nDiffs, UINT nTrivialDiffs, bool bIdentical)
{
	DIFFITEM *pos = FindItemFromPaths(*m_pCtxt, paths);
	// If we failed files could have been swapped so lets try again
	if (!pos)
	{
		PathContext pathsSwapped(paths);
		std::swap(pathsSwapped[0], pathsSwapped[pathsSwapped.GetSize() - 1]);
		pos = FindItemFromPaths(*m_pCtxt, pathsSwapped);
		if (!pos && paths.GetSize() > 2)
		{
			pathsSwapped = paths;
			std::swap(pathsSwapped[0], pathsSwapped[1]);
			pos = FindItemFromPaths(*m_pCtxt, pathsSwapped);
			if (!pos && paths.GetSize() > 2)
			{
				pathsSwapped = paths;
				std::swap(pathsSwapped[1], pathsSwapped[2]);
				pos = FindItemFromPaths(*m_pCtxt, pathsSwapped);
			}
		}
	}
	
	// Update status if paths were found for items.
	// Fail means we had unique items compared as 'renamed' items
	// so there really is not status to update.
	if (pos > 0)
	{
		// Figure out new status code
		UINT diffcode = (bIdentical ? DIFFCODE::SAME : DIFFCODE::DIFF);

		// Update both views and diff context memory
		m_pCtxt->SetDiffStatusCode(pos, diffcode, DIFFCODE::COMPAREFLAGS);

		if (nDiffs != -1 && nTrivialDiffs != -1)
			m_pCtxt->SetDiffCounts(pos, nDiffs, nTrivialDiffs);
		for (int i = 0; i < m_pCtxt->GetCompareDirs(); ++i)
			ReloadItemStatus(pos, i);
	}
}

/**
 * @brief Cleans up after directory compare
 */
void CDirDoc::CompareReady()
{
	// Close and destroy the dialog after compare
	m_pDirView->GetParentFrame()->HideProgressBar();
	// Merge20 (Panel P3, design section 12.5): a rescan rebuilds the
	// DIFFITEMs, so the panels' cached rows go stale. Notify here
	// (compare completed, UI thread, before Redisplay): every open
	// panel rebuilds from the new list. P1 left this to the next
	// activation or state-change notification, so a rescanned list
	// stayed old until then. The notification is posted (async), so
	// the panels refresh after the folder view's Redisplay.
	if (m_pCtxt != nullptr)
		m_pCtxt->NotifyMerge20FileListChanged();
}

/**
 * @brief Refresh cached options.
 *
 * For compare speed, we have to cache some frequently needed options,
 * instead of getting option value every time from OptionsMgr. This
 * function must be called every time options are changed to OptionsMgr.
 */
void CDirDoc::RefreshOptions()
{
	if (m_pCtxt != nullptr)
		m_pCtxt->m_bRecursive = GetOptionsMgr()->GetBool(OPT_CMP_INCLUDE_SUBDIRS);
	if (m_pDirView != nullptr)
		m_pDirView->RefreshOptions();
}

/**
 * @brief Write path and filename to headerbar
 * @note SetText() does not repaint unchanged text
 */
void CDirDoc::UpdateHeaderPath(int nIndex)
{
	CDirFrame *pf = m_pDirView->GetParentFrame();
	ASSERT(pf != nullptr);
	String sText;

	if (!m_strDesc[nIndex].empty())
		sText = m_strDesc[nIndex];
	else
	{
		sText = m_pCtxt->GetPath(nIndex);
		ApplyDisplayRoot(nIndex, sText);
	}

	pf->GetHeaderInterface()->SetCaption(nIndex, sText);
	pf->GetHeaderInterface()->SetPath(nIndex, m_pCtxt->GetPath(nIndex));
}

/**
 * @brief Merge20 (best-workflow B4, gap analysis section 4.2 B4):
 * the B2 close-warning counts for this folder compare.
 *
 * The single gathering point for both uses: B2's close warning
 * (SaveModified below) and B4's panel remainder display
 * (CMerge20FileListBar::UpdateHighRiskRemain) call this, so the
 * two can never disagree about the definition or the value. The
 * walk is the one SaveModified had inline: every file (directory
 * rows excluded), with its review state, its S4 risk and its
 * recorded B1 diff conclusions (see Merge20CloseWarn.h for the
 * counting rules). Still-open merge docs are refreshed into the
 * sidecar first, exactly as SaveModified did, so a judgment made
 * in the open compare counts at once. The refresh writes only
 * the sidecar's judgment fields (SetMerge20DiffJudgments /
 * SetMerge20SplitGroups post no file-list notification), so
 * calling this from a panel refresh cannot re-trigger a refresh.
 * All-zero counts when there is no folder context.
 */
Merge20CloseWarnCounts CDirDoc::GetMerge20CloseWarnCounts()
{
	if (m_pCtxt == nullptr)
		return Merge20CloseWarnCounts();
	Merge20SessionRefreshDiffJudgments(*this);
	std::vector<Merge20CloseWarnFile> warnFiles;
	for (DIFFITEM* pPos = m_pCtxt->GetFirstDiffPosition(); pPos != nullptr;)
	{
		const DIFFITEM& di = m_pCtxt->GetNextDiffPosition(pPos);
		if (di.diffcode.isDirectory())
			continue;
		const Merge20FileState& state = m_pCtxt->GetMerge20FileStates().GetState(di.getItemRelativePath());
		Merge20CloseWarnFile file;
		file.review = state.review;
		file.risk = state.risk;
		for (const Merge20DiffJudgment& judgment : state.diffJudgments)
			file.diffConclusions.push_back(judgment.conclusion);
		warnFiles.push_back(std::move(file));
	}
	return Merge20CloseWarnComputeCounts(warnFiles);
}

/**
 * @brief virtual override called just before document is saved and closed
 */
BOOL CDirDoc::SaveModified() 
{
	// Do not allow closing if there is a thread running
	if (m_diffThread.GetThreadState() == CDiffThread::THREAD_COMPARING)
	{
		int ans = I18n::MessageBox(IDS_CONFIRM_CLOSE_WINDOW_COMPARING, MB_YESNO | MB_ICONWARNING);
		if (ans == IDNO)
			return FALSE;
		m_diffThread.Abort();
		while (m_diffThread.GetThreadState() == CDiffThread::THREAD_COMPARING)
			Sleep(50);
		CompareReady();
	}

	if (m_elapsed >= COMPARISON_TIME_THRESHOLD_SECONDS * 1000)
	{
		int ans = I18n::MessageBox(IDS_CONFIRM_CLOSE_WINDOW_LONG_COMPARISON, MB_YESNO | MB_ICONWARNING | MB_DONT_ASK_AGAIN);
		if (ans == IDNO)
			return FALSE;
	}

	// Merge20 (B2 stage 1, gap analysis section 4.2 B2): warn when
	// undecided items remain, before the session auto-save in
	// OnCloseDocument (which runs only after this returns TRUE).
	// The counts come from GetMerge20CloseWarnCounts above (the
	// same gathering B4's panel display uses, so the warning and
	// the remainder line can never disagree); still-open merge
	// docs are refreshed into the sidecar first, so their
	// judgments count too. [Show unreviewed only] enters the D3
	// TODO-only patrol and opens the next unreviewed file (the B9
	// pattern); [Close anyway] proceeds with the close; Cancel (or
	// closing the dialog) aborts the close and changes nothing. No
	// view means no patrol target, so the warning is skipped then.
	if (m_pCtxt != nullptr && m_pDirView != nullptr)
	{
		const Merge20CloseWarnCounts counts = GetMerge20CloseWarnCounts();
		if (Merge20CloseWarnHasUndecided(counts))
		{
			Merge20CloseWarnDlg dlg(m_pDirView, counts, _("Close anyway"));
			if (dlg.DoModal() != IDOK)
				return FALSE; // Cancel (or close): abort, change nothing.
			switch (dlg.GetResult())
			{
			case Merge20CloseWarnDlg::Result::TodoOnly:
				// Enter the D3 TODO-only patrol (the B9 pattern) and
				// open the next unreviewed file; the close is aborted.
				GetOptionsMgr()->Set(OPT_MERGE20_TODO_ONLY, true);
				m_pDirView->OpenNextFile();
				return FALSE;
			case Merge20CloseWarnDlg::Result::Proceed:
				break; // Close anyway: fall through to the base close.
			case Merge20CloseWarnDlg::Result::Cancel:
				return FALSE;
			}
		}
	}

	return CDocument::SaveModified();
}

/**
 * @brief Send signal to thread to stop current scan
 *
 * @sa CDirCompStateBar::OnStop()
 */
void CDirDoc::AbortCurrentScan()
{
	m_diffThread.Abort();
}

/**
 * @brief Send signal to thread to pause current scan
 */
void CDirDoc::PauseCurrentScan()
{
	m_diffThread.Pause();
}

/**
 * @brief Send signal to thread to continue current scan
 */
void CDirDoc::ContinueCurrentScan()
{
	m_diffThread.Continue();
}

/**
 * @brief Returns true if there is an active scan that hasn't been aborted.
 */
bool CDirDoc::IsCurrentScanAbortable() const
{
	return (m_diffThread.GetThreadState() == CDiffThread::THREAD_COMPARING 
		&& !m_diffThread.IsAborting());
}

/**
 * @brief Set directory description texts shown in headerbar
 */
void CDirDoc::SetDescriptions(const String strDesc[])
{
	if (strDesc != nullptr)
		std::copy_n(strDesc, m_nDirs, m_strDesc);
}

/**
 * @brief Replace internal root by display root
 * When we have a archive file open, this function converts physical folder
 * (that is in the temp folder where archive was extracted) to the virtual
 * path for showing. The virtual path is path to the archive file, archive
 * file name and folder inside the archive.
 * @param [in, out] sText Path to convert.
 */
void CDirDoc::ApplyDisplayRoot(int nIndex, String &sText)
{
	if (m_pTempPathContext != nullptr)
	{
		if (sText.find(m_pTempPathContext->m_strRoot[nIndex]) == String::npos)
		{
			for (int pane = 0; pane < m_nDirs; ++pane)
			{
				if (sText.find(m_pTempPathContext->m_strRoot[pane]) != String::npos)
				{
					nIndex = pane;
					break;
				}
			}
		}
		sText.erase(0, m_pTempPathContext->m_strRoot[nIndex].length());
		sText.insert(0, m_pTempPathContext->m_strDisplayRoot[nIndex]);
	}
}

/**
 * @brief Set document title to given string or items compared.
 * 
 * Formats and sets caption for directory compare window. Caption
 * has left- and right-side paths separated with '-'.
 *
 * @param [in] lpszTitle New title for window. If this parameter
 * is not `nullptr` we use this string, otherwise format caption from
 * actual paths.
 */
void CDirDoc::SetTitle(LPCTSTR lpszTitle)
{
	if (m_pDirView == nullptr)
		return;

	if (lpszTitle != nullptr)
		CDocument::SetTitle(lpszTitle);
	else if (m_pCtxt == nullptr || m_pCtxt->GetLeftPath().empty() ||
		m_pCtxt->GetRightPath().empty() || 
		(m_nDirs > 2 && m_pCtxt->GetMiddlePath().empty()))
	{
		String title = _("Folder Comparison Results");
		CDocument::SetTitle(title.c_str());
	}
	else
	{
		String sTitle;
		String sDirName[3];
		for (int index = 0; index < m_nDirs; index++)
		{
			String strPath = m_pCtxt->GetPath(index);
			ApplyDisplayRoot(index, strPath);
			const String& desc = m_strDesc[index];
			sDirName[index] = desc.empty() ? paths::FindFileName(strPath) : desc;
		}
		if (std::count(&sDirName[0], &sDirName[0] + m_nDirs, sDirName[0]) == m_nDirs)
			sTitle = sDirName[0] + strutils::format(_T(" x %d"), m_nDirs);
 		else
			sTitle = strutils::join(&sDirName[0], &sDirName[0] + m_nDirs, _T(" - "));
 		CDocument::SetTitle(sTitle.c_str());
	}	
}

/**
 * @brief A string to display as a tooltip for MDITabbar
 */
CString CDirDoc::GetTooltipString() const
{
	return MergeTextFormatter::GetTooltipString(m_pCtxt->GetNormalizedPaths(), m_strDesc, nullptr, nullptr).c_str();
}

/**
 * @brief Checks if current folders are opened from archive file.
 * @return true if we are inside archive, false otherwise.
 */
bool CDirDoc::IsArchiveFolders() const
{
	if (m_pTempPathContext != nullptr)
		return true;
	else
		return false;
}

void CDirDoc::Swap(int idx1, int idx2)
{
	if (m_diffThread.GetThreadState() != CDiffThread::THREAD_COMPLETED)
		return;
	std::swap(m_bRO[idx1], m_bRO[idx2]);
	std::swap(m_strDesc[idx1], m_strDesc[idx2]);
	if (m_pTempPathContext != nullptr)
		m_pTempPathContext->Swap(idx1, idx2);
	m_pCtxt->Swap(idx1, idx2);
	m_pCompareStats->Swap(idx1, idx2);
	for (int nIndex = 0; nIndex < m_nDirs; nIndex++)
		UpdateHeaderPath(nIndex);
	SetTitle(nullptr);
}

bool CDirDoc::MoveableToNextDiff()
{
	if (m_pDirView == nullptr)
		return false;
	CMessageBoxDialog dlg(nullptr, _("Move to next file?").c_str());
	const int nFormerResult = dlg.GetFormerResult();
	if (nFormerResult != -1 && nFormerResult == IDNO)
		return false;
	return m_pDirView->HasNextDiff();
}

bool CDirDoc::MoveableToPrevDiff()
{
	if (m_pDirView == nullptr)
		return false;
	CMessageBoxDialog dlg(nullptr, _("Move to previous file?").c_str());
	const int nFormerResult = dlg.GetFormerResult();
	if (nFormerResult != -1 && nFormerResult == IDNO)
		return false;
	return m_pDirView->HasPrevDiff();
}

void CDirDoc::MoveToNextDiff(IMergeDoc *pMergeDoc)
{
	if (m_pDirView == nullptr)
		return;
	if (AfxMessageBox(_("Move to next file?").c_str(), MB_YESNO | MB_DONT_ASK_AGAIN, IDS_MOVE_TO_NEXTFILE) == IDYES)
	{
		pMergeDoc->CloseNow();
		m_pDirView->OpenNextDiff();
		GetMainFrame()->OnUpdateFrameTitle(FALSE);
	}
}

void CDirDoc::MoveToPrevDiff(IMergeDoc *pMergeDoc)
{
	if (m_pDirView == nullptr)
		return;
	if (AfxMessageBox(_("Move to previous file?").c_str(), MB_YESNO | MB_DONT_ASK_AGAIN, IDS_MOVE_TO_PREVFILE) == IDYES)
	{
		pMergeDoc->CloseNow();
		m_pDirView->OpenPrevDiff();
		GetMainFrame()->OnUpdateFrameTitle(FALSE);
	}
}

void CDirDoc::MoveToFirstFile(IMergeDoc* pMergeDoc)
{
	if (m_pDirView == nullptr)
		return;
	if (AfxMessageBox(_("Move to first file?").c_str(), MB_YESNO | MB_DONT_ASK_AGAIN, IDS_MOVE_TO_FIRSTFILE) == IDYES)
	{
		pMergeDoc->CloseNow();
		m_pDirView->OpenFirstFile();
		GetMainFrame()->OnUpdateFrameTitle(FALSE);
	}
}

void CDirDoc::MoveToNextFile(IMergeDoc* pMergeDoc)
{
	if (m_pDirView == nullptr)
		return;
	pMergeDoc->CloseNow();
	m_pDirView->OpenNextFile();
	GetMainFrame()->OnUpdateFrameTitle(FALSE);
}

void CDirDoc::MoveToPrevFile(IMergeDoc* pMergeDoc)
{
	if (m_pDirView == nullptr)
		return;
	pMergeDoc->CloseNow();
	m_pDirView->OpenPrevFile();
	GetMainFrame()->OnUpdateFrameTitle(FALSE);
}

void CDirDoc::MoveToLastFile(IMergeDoc* pMergeDoc)
{
	if (m_pDirView == nullptr)
		return;
	if (AfxMessageBox(_("Move to last file?").c_str(), MB_YESNO | MB_DONT_ASK_AGAIN, IDS_MOVE_TO_LASTFILE) == IDYES)
	{
		pMergeDoc->CloseNow();
		m_pDirView->OpenLastFile();
		GetMainFrame()->OnUpdateFrameTitle(FALSE);
	}
}

bool CDirDoc::IsFirstFile()
{
	if (m_pDirView == nullptr)
		return true;
	return m_pDirView->IsFirstFile();
}

bool CDirDoc::IsLastFile()
{
	if (m_pDirView == nullptr)
		return true;
	return m_pDirView->IsLastFile();
}

bool CDirDoc::CompareFilesIfFilesAreLarge(int nFiles, const FileLocation ifileloc[])
{
	DIFFITEM di;
	bool bLargeFile = false;
	for (int i = 0; i < nFiles; ++i)
	{
		di.diffFileInfo[i].SetFile(paths::FindFileName(ifileloc[i].filepath));
		if (di.diffFileInfo[i].Update(ifileloc[i].filepath))
			di.diffcode.setSideFlag(i);
	}
	if (nFiles == 3)
		di.diffcode.diffcode |= DIFFCODE::THREEWAY;

	size_t fileSizeThreshold = GetOptionsMgr()->GetInt(OPT_FILE_SIZE_THRESHOLD);
	for (int i = 0; i < nFiles; ++i)
	{
		if (di.diffFileInfo[i].size != DirItem::FILE_SIZE_NONE && di.diffFileInfo[i].size > fileSizeThreshold)
			bLargeFile = true;
	}
	if (!bLargeFile)
		return false;

	PathContext paths;
	for (int i = 0; i < nFiles; ++i)
		paths.SetPath(i, ifileloc[i].filepath.empty() ? paths::NATIVE_NULL_DEVICE_NAME : paths::GetParentPath(ifileloc[i].filepath));
	CDiffContext ctxt(paths, CMP_QUICK_CONTENT);
	DirViewColItems ci(nFiles, std::vector<String>{});
	String msg = I18n::LoadString(IDS_COMPARE_LARGE_FILES) + _T("\n");
	if (nFiles < 3)
	{
		String sidestr[] = { _("Left:"), _("Right:") };
		for (int i = 0; i < nFiles; ++i)
		{
			if (ifileloc[i].filepath.empty())
			{
				msg += strutils::format(_T("%s %s\n\n"), sidestr[i], _("None"));
			}
			else
			{
				msg += strutils::format(_T("%s %s\n %s:  %s\n %s: %s (%s)\n\n"),
					sidestr[i].c_str(), ifileloc[i].filepath.c_str(),
					ci.GetColDisplayName(3 + i).c_str(), ci.ColGetTextToDisplay(&ctxt, 3 + i, di).c_str(),
					ci.GetColDisplayName(8 + i).c_str(), ci.ColGetTextToDisplay(&ctxt, 8 + i, di).c_str(), ci.ColGetTextToDisplay(&ctxt, 10 + i, di).c_str());
			}
		}
	}
	else
	{
		String sidestr[] = { _("Left:"), _("Middle:"), _("Right:") };
		for (int i = 0; i < nFiles; ++i)
		{
			if (ifileloc[i].filepath.empty())
			{
				msg += strutils::format(_T("%s %s\n\n"), sidestr[i], _("None"));
			}
			else
			{
				msg += strutils::format(_T("%s %s\n %s:  %s\n %s: %s (%s)\n\n"),
					sidestr[i].c_str(), ifileloc[i].filepath.c_str(),
					ci.GetColDisplayName(3 + i).c_str(), ci.ColGetTextToDisplay(&ctxt, 3 + i, di).c_str(),
					ci.GetColDisplayName(10 + i).c_str(), ci.ColGetTextToDisplay(&ctxt, 10 + i, di).c_str(), ci.ColGetTextToDisplay(&ctxt, 13 + i, di).c_str());
			}
		}
	}
	CMessageBoxDialog dlg(
		m_pDirView ? m_pDirView->GetParentFrame() : nullptr,
		msg.c_str(), _T(""),
		MB_YESNOCANCEL | MB_ICONQUESTION | MB_DONT_ASK_AGAIN, 0U, _T("CompareLargeFiles"));
	INT_PTR ans = dlg.GetFormerResult();
	if (ans != -1 || !theApp.GetNonInteractive())
	{
		ans = theApp.DoMessageBox(msg.c_str(),
			MB_YESNOCANCEL | MB_ICONQUESTION | MB_DONT_ASK_AGAIN, 0U, _T("CompareLargeFiles"));
		if (ans == IDCANCEL)
			return true;
		else if (ans == IDNO)
			return false;
	}

	int oldCompareMethod = GetOptionsMgr()->GetInt(OPT_CMP_METHOD);
	GetOptionsMgr()->SaveOption(OPT_CMP_METHOD, CMP_QUICK_CONTENT); // Use quick content compare for large files
	InitDiffContext(&ctxt);
	GetOptionsMgr()->SaveOption(OPT_CMP_METHOD, oldCompareMethod); // Restore previous compare method
	FolderCmp cmp(&ctxt);
	CWaitCursor waitstatus;
	cmp.prepAndCompareFiles(di);
	if (di.diffcode.isResultSame())
	{
		ctxt.GetComparePaths(di, paths);
		CMergeFrameCommon::ShowIdenticalMessage(paths, true);
	}
	else
	{
		AfxMessageBox(ci.ColGetTextToDisplay(&ctxt, 2, di).c_str(), MB_OK | MB_ICONINFORMATION);
	}
	theApp.SetLastCompareResult(di.diffcode.isResultDiff() ? 1 : 0);
	return true;
}

DirCompProgressBar* CDirDoc::GetCompProgressBar()
{
	CDirFrame *pf = m_pDirView->GetParentFrame();
	if (pf == nullptr)
		return nullptr;
	return pf->GetCompProgressBar();
}

void CDirDoc::OnBnClickedComparisonStop()
{
	auto* pCmpProgressBar = GetCompProgressBar();
	if (pCmpProgressBar != nullptr)
		pCmpProgressBar->EndUpdating();
	AbortCurrentScan();
}

void CDirDoc::OnBnClickedComparisonPause()
{
	auto* pCmpProgressBar = GetCompProgressBar();
	if (pCmpProgressBar != nullptr)
		pCmpProgressBar->SetPaused(true);
	PauseCurrentScan();
}

void CDirDoc::OnBnClickedComparisonContinue()
{
	auto* pCmpProgressBar = GetCompProgressBar();
	if (pCmpProgressBar != nullptr)
		pCmpProgressBar->SetPaused(false);
	ContinueCurrentScan();
}

void CDirDoc::OnCbnSelChangeCPUCores()
{
	auto* pCmpProgressBar = GetCompProgressBar();
	if (pCmpProgressBar == nullptr || m_pCtxt == nullptr)
		return;

	int requestedCores = pCmpProgressBar->GetNumberOfCPUCoresToUse();
	int totalThreads = m_pCtxt->m_pCompareStats->GetCompareThreadCount();

	if (totalThreads <= 0)
		return;

	// Clamp requested cores to valid range [1, totalThreads]
	requestedCores = std::clamp(requestedCores, 1, totalThreads);

	PauseCurrentScan();
	m_pCtxt->m_pCompareStats->SetIdleCompareThreadCount(totalThreads - requestedCores);
	ContinueCurrentScan();
}

