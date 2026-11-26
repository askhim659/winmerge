/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997-2000  Thingamahoochie Software
//    Author: Dean Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  MergeEditFrm.cpp
 *
 * @brief Implementation file for CMergeEditFrame
 *
 */

#include "stdafx.h"
#include "MergeEditFrm.h"
#include "Merge20/Merge20FocusDock.h"
#include "Merge20/Merge20FocusResultView.h"
#include "FrameWndHelper.h"
#include "MergeDoc.h"
#include "MergeEditView.h"
#include "MergeResultView.h"
#include "IDirDoc.h"
#include "Merge20/Merge20Types.h"
#include "MainFrm.h"
#include "LocationView.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "DarkModeLib.h"
#include "paths.h"
#include "SplitterPositions.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define SWAPPARAMS_IF(c, a, b) (c ? a : b), (c ? b : a)

/////////////////////////////////////////////////////////////////////////////
// CMergeEditFrame

IMPLEMENT_DYNCREATE(CMergeEditFrame, CMergeFrameCommon)

BEGIN_MESSAGE_MAP(CMergeEditFrame, CMergeFrameCommon)
	//{{AFX_MSG_MAP(CMergeEditFrame)
	ON_WM_CREATE()
	ON_WM_CLOSE()
	ON_WM_MDIACTIVATE()
	ON_WM_TIMER()
	ON_WM_SIZE()
	ON_MESSAGE_VOID(WM_IDLEUPDATECMDUI, OnIdleUpdateCmdUI)
	ON_MESSAGE(MSG_STORE_PANESIZES, OnStorePaneSizes)
	// [View] menu
	ON_UPDATE_COMMAND_UI(ID_VIEW_DETAIL_BAR, OnUpdateControlBarMenu)
	ON_COMMAND_EX(ID_VIEW_DETAIL_BAR, OnBarCheck)
	ON_UPDATE_COMMAND_UI(ID_VIEW_LOCATION_BAR, OnUpdateControlBarMenu)
	ON_COMMAND_EX(ID_VIEW_LOCATION_BAR, OnBarCheck)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MERGE_RESULT_BAR, OnUpdateViewMergeResultBar)
	ON_COMMAND_EX(ID_VIEW_MERGE_RESULT_BAR, OnBarCheck)
	ON_COMMAND(ID_VIEW_MERGE20_DEDICATED_MODE, OnViewMerge20DedicatedMode)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MERGE20_DEDICATED_MODE, OnUpdateViewMerge20DedicatedMode)
	// Merge20 (Phase 4 D2): dedicated-bar buttons forward here.
	ON_COMMAND(ID_MERGE20_DED_EXIT, OnMerge20DedExit)
	ON_COMMAND(ID_MERGE20_DED_PREV_FILE, OnMerge20DedPrevFile)
	ON_COMMAND(ID_MERGE20_DED_NEXT_FILE, OnMerge20DedNextFile)
	ON_COMMAND_RANGE(ID_MERGE20_DED_REVIEW_TODO, ID_MERGE20_DED_REVIEW_KEEP, OnMerge20DedReview)
	ON_COMMAND(ID_MERGE20_DED_PIN, OnMerge20DedPin)
	ON_COMMAND(ID_MERGE20_DED_TODO_ONLY, OnMerge20DedTodoOnly)
	ON_COMMAND(ID_MERGE20_DED_SNIPER, OnMerge20DedSniper)
	ON_COMMAND(ID_MERGE20_DED_ZOOM_RESET, OnMerge20DedZoomReset)
	ON_COMMAND(ID_VIEW_SPLITVERTICALLY, OnViewSplitVertically)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SPLITVERTICALLY, OnUpdateViewSplitVertically)
	// Display filter bar
	ON_COMMAND(ID_VIEW_DISPLAY_FILTER_BAR_MENU, OnViewDisplayFilterBar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_DISPLAY_FILTER_BAR_MENU, OnUpdateDisplayViewFilterBar)
	ON_COMMAND(IDCANCEL, OnDisplayFilterBarClose)
	ON_COMMAND(IDC_FILTERFILE_MASK_MENU, OnDisplayFilterBarMenu)
	// [Window] menu
	ON_COMMAND(ID_WINDOW_PRESERVE_SPLITTER_RATIOS, OnWindowPreserveSplitterPosition)
	ON_UPDATE_COMMAND_UI(ID_WINDOW_PRESERVE_SPLITTER_RATIOS, OnUpdateWindowPreserveSplitterPosition)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

constexpr UINT_PTR IDT_SAVEPOSITION = 2;
constexpr UINT_PTR IDT_PREVIEWMODE = 3;
// Merge20 (Phase 4 D2): dedicated-bar auto-hide poll (500ms).
constexpr UINT_PTR IDT_DEDHIDE = 4;

/////////////////////////////////////////////////////////////////////////////
// CMergeEditFrame construction/destruction

/**
 * @brief Constructor.
 */
CMergeEditFrame::CMergeEditFrame()
: CMergeFrameCommon(IDI_EQUALTEXTFILE, IDI_NOTEQUALTEXTFILE)
, m_pwndDetailMergeEditSplitterView(nullptr)
, m_pMergeResultView(nullptr)
, m_bResultBarVisible(false)
, m_bDedicatedMode(false)
, m_bDedWasMaximized(false)
, m_bDedFilePathBarVisible(false)
, m_bDedLocationBarVisible(false)
, m_bDedDetailBarVisible(false)
, m_bDedResultBarVisible(false)
, m_bDedStatusBarVisible(false)
, m_nDedPrevZoom(1000)
, m_ullDedBarFlashUntil(0)
, m_ullDedBarLastMouseOver(0)
{
	m_pMergeDoc = 0;
}

/**
 * Destructor.
 */
CMergeEditFrame::~CMergeEditFrame()
{
}

BOOL CMergeEditFrame::OnCreateClient( LPCREATESTRUCT /*lpcs*/,
	CCreateContext* pContext)
{
	m_wndSplitter.HideBorders(true);
	m_wndSplitter.Create(this, 2, 1, CSize(1, 1), pContext, WS_CHILD | WS_VISIBLE | 1/*SPLS_DYNAMIC_SPLIT*/);

	// Merge frame has also a dockable bar at the very left
	// This is not the client area, but we create it now because we want
	// to use the CCreateContext
	String sCaption = _("Location Pane");
	if (!m_wndLocationBar.Create(this, sCaption.c_str(), WS_CHILD | WS_VISIBLE, ID_VIEW_LOCATION_BAR))
	{
		TRACE0("Failed to create LocationBar\n");
		return FALSE;
	}

	CLocationView *pLocationView = new CLocationView;
	DWORD dwStyle = AFX_WS_DEFAULT_VIEW & ~WS_BORDER;
	pLocationView->Create(nullptr, nullptr, dwStyle, CRect(0,0,40,100), &m_wndLocationBar, 152, pContext);

	// Merge frame has also a dockable bar at the very bottom
	// This is not the client area, but we create it now because we want
	// to use the CCreateContext
	sCaption = _("Diff Pane");
	if (!m_wndDetailBar.Create(this, sCaption.c_str(), WS_CHILD | WS_VISIBLE, ID_VIEW_DETAIL_BAR))
	{
		TRACE0("Failed to create DiffViewBar\n");
		return FALSE;
	}

	// Merge20 (Phase 4 D4): the Diff Pane is now a focus dock: the
	// bar's single child (SCBS_SIZECHILD) is the dock container,
	// which holds the header strip, the detail splitter view and
	// the result-segment view (design section 3.5). The splitter
	// view is created with the dock as its parent instead of the
	// bar, so the bar keeps sizing exactly one child.
	if (!m_wndFocusDock.Create(&m_wndDetailBar))
	{
		TRACE0("Failed to create Merge20FocusDock\n");
		return FALSE;
	}

	m_pwndDetailMergeEditSplitterView = new CMergeEditSplitterView();
	m_pwndDetailMergeEditSplitterView->m_bDetailView = true;
	if (!m_pwndDetailMergeEditSplitterView->Create(nullptr, nullptr, dwStyle, CRect(0,0,1,1), &m_wndFocusDock, ID_VIEW_DETAIL_BAR+1, pContext))
	{
		TRACE0("Failed to create CMergeEditSplitterView\n");
		return FALSE;
	}

	// Merge20 (Phase 4 D4): the third segment: the result pane's
	// lines for the current diff, in a read-only view (design
	// section 3.5, option (b): the lines are extracted from the
	// result buffer, so the dock never fights the document's
	// single result view).
	CMerge20FocusResultView* pFocusResultView = new CMerge20FocusResultView();
	if (!pFocusResultView->Create(nullptr, nullptr, dwStyle, CRect(0,0,1,1), &m_wndFocusDock, IDC_MERGE20_FOCUS_RESULT, pContext))
	{
		TRACE0("Failed to create CMerge20FocusResultView\n");
		delete pFocusResultView;
		return FALSE;
	}
	pFocusResultView->SendMessage(WM_INITIALUPDATE);

	m_wndFocusDock.SetViews(m_pwndDetailMergeEditSplitterView, pFocusResultView);
	m_wndFocusDock.SetMergeDoc(dynamic_cast<CMergeDoc*>(pContext->m_pCurrentDoc));
	m_wndFocusDock.ShowDiff(-1); // No diff selected yet: placeholder + disabled buttons.

	// Merge20 (Phase 4 D1/D2): dedicated-mode bar. D2 adds the
	// Prev/Next, review-state, pin/toggle, zoom and exit buttons,
	// and fixes the bar height to one button row (see
	// CMerge20DedBar::LoadState / CalcFixedLayout).
	sCaption = _("Dedicated Compare Mode");
	if (!m_wndDedBar.Create(this, sCaption.c_str(), WS_CHILD | WS_VISIBLE, ID_VIEW_MERGE20_DEDICATED_MODE))
	{
		TRACE0("Failed to create Merge20DedBar\n");
		return FALSE;
	}

	// tell merge doc about these views
	m_pMergeDoc = dynamic_cast<CMergeDoc *>(pContext->m_pCurrentDoc);
	m_pMergeDoc->ForEachView([&](auto& pView) {
		pView->SetStatusInterface(m_wndStatusBar.GetIMergeEditStatus(pView->m_nThisPane));
	});
	m_pMergeDoc->SetLocationView(pLocationView);

	// kdiff3-style merge result pane: a dockable
	// bar at the bottom with a full-width editable result view
	if (m_pMergeDoc->m_nBuffers >= 2)
	{
		sCaption = _("Merge Result Pane");
		if (!m_wndResultBar.Create(this, sCaption.c_str(), WS_CHILD | WS_VISIBLE, ID_VIEW_MERGE_RESULT_BAR, pContext, m_pMergeDoc))
		{
			TRACE0("Failed to create MergeResultBar\n");
			return FALSE;
		}
		// Store frame window handle for notification
		m_wndResultBar.SetFrameHwnd(GetSafeHwnd());
	}

	m_wndFilePathBar.SetPaneCount(m_pMergeDoc->m_nBuffers);
	m_wndFilePathBar.SetOnSetFocusCallback([&](int pane) {
		const int nGroup = m_pMergeDoc->GetActiveMergeView()->m_nThisGroup;
		m_pMergeDoc->GetView(nGroup, pane)->SetActivePane();
	});
	m_wndFilePathBar.SetOnCaptionChangedCallback([this](int pane, const String& sText) {
		const int nGroup = m_pMergeDoc->GetActiveMergeView()->m_nThisGroup;
		m_pMergeDoc->SetDescription(pane, sText);
		m_pMergeDoc->UpdateHeaderPath(pane);
		m_pMergeDoc->GetView(nGroup, pane)->SetFocus();
	});
	m_wndFilePathBar.SetOnFileSelectedCallback([this](int pane, const String& sFilepath, const String& sDescription) {
		const int nGroup = m_pMergeDoc->GetActiveMergeView()->m_nThisGroup;
		if (m_pMergeDoc->ChangeFile(pane, sFilepath, sDescription))
		{
			m_pMergeDoc->GetView(nGroup, pane)->SetFocus();
			// Only add to MRU if description is empty (i.e., not from clipboard history)
			if (sDescription.empty())
				MruHelper::addToMru(pane, sFilepath);
		}
	});
	m_wndFilePathBar.SetOnFileSaveCallback([&](int pane) {
		const int id = (m_pMergeDoc->m_nBuffers < 3 && pane == 1) ? ID_FILE_SAVE_RIGHT : ID_FILE_SAVE_LEFT + pane;
		PostMessage(WM_COMMAND, id);
	});
	m_wndFilePathBar.SetDefaultHistoryCallbacks();
	m_wndStatusBar.SetPaneCount(m_pMergeDoc->m_nBuffers);

	// Set frame window handles so we can post stage changes back
	pLocationView->SetFrameHwnd(GetSafeHwnd());
	m_wndLocationBar.SetFrameHwnd(GetSafeHwnd());
	m_wndDetailBar.SetFrameHwnd(GetSafeHwnd());
	m_wndDedBar.SetFrameHwnd(GetSafeHwnd());

	// Load splitter positions
	LoadOptions();

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMergeEditFrame message handlers

/**
* @brief Create the child frame, the splitter, the filename bar, the status bar,
* the diff dockable bar, and the four views
*
* @note  the panels layout is 
* <ul>
*  <li>   -----------
*  <li>		!  header !
*  <li>		!.........!
*  <li>		!.   .   .!
*  <li>		!. 1 . 2 .!
*  <li>		!.   .   .!
*  <li>		!.........!
*  <li>		!.........!
*  <li>		!  status !
*  <li>		-----------
*  <li>		!.........!
*  <li>		!.   3   .!
*  <li>		!.dockable!
*  <li>		! splitbar!
*  <li>		!.   4   .!
*  <li>		!.........!
*  <li>		-----------
* </ul>
*/
int CMergeEditFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (__super::OnCreate(lpCreateStruct) == -1)
		return -1;

	EnableDocking(CBRS_ALIGN_TOP|CBRS_ALIGN_BOTTOM|CBRS_ALIGN_LEFT|CBRS_ALIGN_RIGHT);

	FrameWndHelper::RemoveBarBorder(this);

	// Merge frame has a header bar at top
	if (!m_wndFilePathBar.Create(this))
	{
		TRACE0("Failed to create dialog bar\n");
		return -1;      // fail to create
	}

	// Set filename bars inactive so colors get initialized
	m_wndFilePathBar.SetActive(0, false);
	m_wndFilePathBar.SetActive(1, false);
	m_wndFilePathBar.SetActive(2, false);

	// Merge frame also has a dockable bar at the very left
	// created in OnCreateClient 
	m_wndLocationBar.SetBarStyle(m_wndLocationBar.GetBarStyle() |
		CBRS_SIZE_DYNAMIC | CBRS_ALIGN_LEFT);
	m_wndLocationBar.EnableDocking(CBRS_ALIGN_LEFT | CBRS_ALIGN_RIGHT);
	CRect rc{ 0, 0, 0, 0 };
	DockControlBar(&m_wndLocationBar, AFX_IDW_DOCKBAR_LEFT, &rc);

	// Merge frame also has a dockable bar at the very bottom
	// created in OnCreateClient
	m_wndDetailBar.SetBarStyle(m_wndDetailBar.GetBarStyle() |
		CBRS_SIZE_DYNAMIC | CBRS_ALIGN_TOP);
	m_wndDetailBar.EnableDocking(CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM);
	DockControlBar(&m_wndDetailBar, AFX_IDW_DOCKBAR_BOTTOM, &rc);

	// Merge20 (Phase 4 D1): dedicated-mode bar, docked at the top.
	// It starts hidden; entering dedicated mode shows it.
	m_wndDedBar.SetBarStyle(m_wndDedBar.GetBarStyle() |
		CBRS_SIZE_DYNAMIC | CBRS_ALIGN_TOP);
	m_wndDedBar.EnableDocking(CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM);
	DockControlBar(&m_wndDedBar, AFX_IDW_DOCKBAR_TOP, &rc);

	// Merge result pane bar, docked at the bottom
	if (m_wndResultBar.m_hWnd != nullptr)
	{
		m_wndResultBar.SetBarStyle(m_wndResultBar.GetBarStyle() |
			CBRS_SIZE_DYNAMIC | CBRS_ALIGN_TOP);
		m_wndResultBar.EnableDocking(CBRS_ALIGN_TOP | CBRS_ALIGN_BOTTOM);
		DockControlBar(&m_wndResultBar, AFX_IDW_DOCKBAR_BOTTOM, &rc);
	}

	// Merge frame also has a status bar at bottom, 
	// m_wndDetailBar is below, so we create this bar after m_wndDetailBar
	if (!m_wndStatusBar.Create(this))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}	

	// load docking positions and sizes
	CDockState pDockState;
	pDockState.LoadState(_T("Settings"));
	if (FrameWndHelper::EnsureValidDockState(this, pDockState)) // checks for valid so won't ASSERT
		SetDockState(pDockState);
	// for the dimensions of the diff and location pane, use the CSizingControlBar loader
	m_wndLocationBar.LoadState(_T("Settings"));
	m_wndDetailBar.LoadState(_T("Settings"));
	m_wndDedBar.LoadState(_T("Settings"));
	if (m_wndResultBar.m_hWnd != nullptr)
	{
		m_wndResultBar.LoadState(_T("Settings"));
		// The merge result pane is opt-in per document: it is shown only
		// when the compare was opened with a merge output path or conflict
		// file, or when the user turns it on in the View menu. Compare
		// sessions otherwise start without it, regardless of saved state.
		ShowControlBar(&m_wndResultBar, FALSE, FALSE);
	}

	// Merge20 (Phase 4 D1): the dedicated-mode bar is shown only while
	// dedicated mode is active.
	ShowControlBar(&m_wndDedBar, FALSE, FALSE);

	// Merge20 (Phase 4 D1): if the previous session left dedicated mode
	// on (OPT_MERGE20_DED_MODE), enter it again for this frame. Posted
	// (not called directly) so the frame finishes creation first.
	if (GetOptionsMgr()->GetBool(OPT_MERGE20_DED_MODE))
		PostMessage(WM_COMMAND, ID_VIEW_MERGE20_DEDICATED_MODE);

	return 0;
}

BOOL CMergeEditFrame::OnBarCheck(UINT nID)
{
	BOOL result = __super::OnBarCheck(nID);
	// Fix for osdn.net #42862
	if (nID == ID_VIEW_DETAIL_BAR && m_wndDetailBar.IsWindowVisible())
	{
		int nDiff = m_pMergeDoc->GetCurrentDiff();
		m_pMergeDoc->ForEachView ([nDiff](auto& pView) { if (pView->m_bDetailView) pView->OnDisplayDiff(nDiff); });
		// Merge20 (Phase 4 D4): reopening the bar also refreshes the
		// focus dock (its result segment may have changed while hidden).
		m_wndFocusDock.ShowDiff(nDiff);
	}
	// The merge result pane's own state follows the bar's visibility, which
	// is synchronized in OnIdleUpdateCmdUI (the bar can also be closed with
	// its close button, which does not come through here)
	if (nID == ID_VIEW_MERGE_RESULT_BAR)
		GetMainFrame()->UpdateToolbar();
	return result;
}

/**
 * @brief Toggle dedicated compare mode (Merge20 Phase 4 D1).
 *
 * The mode is kept in OPT_MERGE20_DED_MODE (D0 key) so the next file
 * compare opens in dedicated mode again (design section 4.1(a)).
 * The flag is set before entering/exiting so the pane-size save
 * triggered by hiding the bars (MSG_STORE_PANESIZES) is suppressed
 * while the mode is active (see OnStorePaneSizes).
 */
void CMergeEditFrame::SetDedicatedMode(bool bEnable)
{
	if (bEnable == m_bDedicatedMode)
		return;
	m_bDedicatedMode = bEnable;
	if (bEnable)
		EnterDedicatedMode();
	else
		ExitDedicatedMode();
	GetOptionsMgr()->SaveOption(OPT_MERGE20_DED_MODE, bEnable);
}

/**
 * @brief Enter dedicated compare mode: maximize the MDI child window,
 * hide this frame's bars, and show the dedicated-mode bar.
 *
 * WinMerge has no fullscreen mode, so the mode is the maximized child
 * window plus hidden bars (design section 2.2(2)). Only this frame's
 * own bars are hidden (T-4-1 fallback): the main frame's menu bar /
 * toolbar / tab bar / status bar have no public hide API callable from
 * the child, and hiding the menu would remove the View-menu exit path
 * while the dedicated bar is still a skeleton without its own exit
 * button (D2 adds it).
 */
void CMergeEditFrame::EnterDedicatedMode()
{
	// Remember the current layout so exiting restores it exactly.
	m_bDedFilePathBarVisible = !!m_wndFilePathBar.IsWindowVisible();
	m_bDedLocationBarVisible = !!m_wndLocationBar.IsWindowVisible();
	m_bDedDetailBarVisible = !!m_wndDetailBar.IsWindowVisible();
	m_bDedResultBarVisible = m_wndResultBar.m_hWnd != nullptr && !!m_wndResultBar.IsWindowVisible();
	m_bDedStatusBarVisible = !!m_wndStatusBar.IsWindowVisible();
	m_bDedWasMaximized = !!IsZoomed();

	// Merge20 (Phase 4 D2, design section 3.2): keep the pre-entry zoom
	// to restore on exit, and apply the stored dedicated zoom (default
	// 750 = 75%, HTML DEF_SCALE). The zoom is a global option, so
	// applying it here is also what makes a file move keep it: the new
	// frame reads OPT_VIEW_ZOOM on creation, and UpdateDedBar mirrors
	// every zoom change back into OPT_MERGE20_DED_ZOOM. The position
	// returns to the top (GotoLine), the zoom is kept.
	m_nDedPrevZoom = GetOptionsMgr()->GetInt(OPT_VIEW_ZOOM);
	if (m_pMergeDoc != nullptr)
	{
		if (CMergeEditView* pView = m_pMergeDoc->GetActiveMergeView())
		{
			pView->ZoomTextTo(GetOptionsMgr()->GetInt(OPT_MERGE20_DED_ZOOM));
			pView->GotoLine(0, true, pView->m_nThisPane);
		}
	}

	ShowControlBar(&m_wndFilePathBar, FALSE, FALSE);
	ShowControlBar(&m_wndLocationBar, FALSE, FALSE);
	ShowControlBar(&m_wndDetailBar, FALSE, FALSE);
	if (m_wndResultBar.m_hWnd != nullptr)
		ShowControlBar(&m_wndResultBar, FALSE, FALSE);
	ShowControlBar(&m_wndStatusBar, FALSE, FALSE);
	ShowControlBar(&m_wndDedBar, TRUE, FALSE);

	// Merge20 (Phase 4 D2): auto-hide. The bar starts shown (flash
	// grace 1.6s, then it hides once the cursor has been away for 3s,
	// unless pinned). The timer polls the cursor position.
	const ULONGLONG now = ::GetTickCount64();
	m_ullDedBarFlashUntil = now + 1600;
	m_ullDedBarLastMouseOver = now;
	SetTimer(IDT_DEDHIDE, 500, nullptr);

	if (GetMDIFrame() != nullptr)
		GetMDIFrame()->MDIMaximize(this);
	GetMainFrame()->UpdateToolbar();
}

/**
 * @brief Exit dedicated compare mode: hide the dedicated-mode bar and
 * restore the bars and the maximized state saved on entry.
 */
void CMergeEditFrame::ExitDedicatedMode()
{
	KillTimer(IDT_DEDHIDE);
	// Merge20 (Phase 4 D2): restore the pre-entry zoom, and turn the
	// sniper off (design section 3.1: leaving dedicated mode turns the
	// sniper off; its toggle is refreshed on the next entry).
	if (m_pMergeDoc != nullptr)
	{
		if (CMergeEditView* pView = m_pMergeDoc->GetActiveMergeView())
			pView->ZoomTextTo(m_nDedPrevZoom);
	}
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SNIPER_ON, false);
	// Merge20 (Phase 4 D6): leaving dedicated mode closes the
	// sniper overlay as well (the option above only flips the
	// toggle; the dialog itself must be hidden here).
	m_wndSniper.CloseSniper();

	ShowControlBar(&m_wndDedBar, FALSE, FALSE);
	ShowControlBar(&m_wndFilePathBar, m_bDedFilePathBarVisible, FALSE);
	ShowControlBar(&m_wndLocationBar, m_bDedLocationBarVisible, FALSE);
	ShowControlBar(&m_wndDetailBar, m_bDedDetailBarVisible, FALSE);
	if (m_wndResultBar.m_hWnd != nullptr)
		ShowControlBar(&m_wndResultBar, m_bDedResultBarVisible, FALSE);
	ShowControlBar(&m_wndStatusBar, m_bDedStatusBarVisible, FALSE);

	if (!m_bDedWasMaximized && GetMDIFrame() != nullptr)
		GetMDIFrame()->MDIRestore(this);
	GetMainFrame()->UpdateToolbar();
}

/**
 * @brief View-menu handler: toggle dedicated compare mode.
 */
void CMergeEditFrame::OnViewMerge20DedicatedMode()
{
	SetDedicatedMode(!m_bDedicatedMode);
}

/**
 * @brief View-menu update handler: check the item while in dedicated mode.
 */
void CMergeEditFrame::OnUpdateViewMerge20DedicatedMode(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck(m_bDedicatedMode);
}

/**
 * @brief Show the dedicated bar briefly (Merge20 Phase 4 D2, HTML
 * flashDedBar(), 1.6s). Called on file moves (via the new frame's
 * entry) and on review/zoom operations while the bar is hidden. A
 * no-op when pinned (the bar never hides then) or not dedicated.
 */
void CMergeEditFrame::FlashDedBar()
{
	if (!m_bDedicatedMode || m_wndDedBar.m_hWnd == nullptr)
		return;
	if (!m_wndDedBar.IsWindowVisible())
		ShowControlBar(&m_wndDedBar, TRUE, FALSE);
	m_ullDedBarFlashUntil = ::GetTickCount64() + 1600;
}

/**
 * @brief Refresh the dedicated bar from the current state. Called from
 * OnIdleUpdateCmdUI while dedicated: the zoom display, the pin /
 * unreviewed-only / sniper toggles, the Prev/Next enablement and the
 * review-state check. Also mirrors the current zoom into
 * OPT_MERGE20_DED_ZOOM, so the next file (a new frame entering
 * dedicated mode) and the next session keep the dedicated zoom.
 */
void CMergeEditFrame::UpdateDedBar()
{
	if (!m_bDedicatedMode || m_wndDedBar.m_hWnd == nullptr || m_pMergeDoc == nullptr)
		return;
	const int nZoom = GetOptionsMgr()->GetInt(OPT_VIEW_ZOOM);
	if (nZoom != GetOptionsMgr()->GetInt(OPT_MERGE20_DED_ZOOM))
		GetOptionsMgr()->SaveOption(OPT_MERGE20_DED_ZOOM, nZoom);
	m_wndDedBar.SetZoomText(nZoom);
	m_wndDedBar.SetPinned(GetOptionsMgr()->GetBool(OPT_MERGE20_DED_PINNED));
	const bool bTodoOnly = GetOptionsMgr()->GetBool(OPT_MERGE20_TODO_ONLY);
	m_wndDedBar.SetTodoOnly(bTodoOnly);
	m_wndDedBar.SetSniperOn(GetOptionsMgr()->GetBool(OPT_MERGE20_SNIPER_ON));
	IDirDoc* pDirDoc = m_pMergeDoc->GetDirDoc();
	const bool bHasDirDoc = (pDirDoc != nullptr);
	m_wndDedBar.SetPrevNextEnabled(bHasDirDoc && !pDirDoc->IsFirstFile(),
		bHasDirDoc && !pDirDoc->IsLastFile());
	m_wndDedBar.SetReviewEnabled(bHasDirDoc);
	m_wndDedBar.SetReviewState(bHasDirDoc ?
		pDirDoc->GetMerge20ReviewState(m_pMergeDoc->m_filePaths) : ReviewState::TODO);
	// Merge20 (Phase 4 D3): file name + (n/m) position in the patrol
	// set (spec section 5.2; D2 deferred this, see MERGE20_NOTES.md
	// section 24.2 item 6). The denominator is the patrol set: all
	// displayed files, or only the TODO files while unreviewed-only
	// is on; the numerator is the 1-based ordinal among that set, 0
	// when this file is not in it (e.g. TODO-only is on but the file
	// is already reviewed). With no TODO file at all the label says
	// so instead of "(0/0)".
	int nPos = 0, nTotal = 0;
	if (bHasDirDoc && pDirDoc->GetMerge20FilePosition(m_pMergeDoc->m_filePaths, nPos, nTotal, bTodoOnly))
	{
		if (bTodoOnly && nTotal == 0)
		{
			m_wndDedBar.SetFileInfo(_("No unreviewed files"));
		}
		else
		{
			const String sName = paths::FindFileName(m_pMergeDoc->m_filePaths.GetLeft());
			m_wndDedBar.SetFileInfo(strutils::format(_T("%s (%d/%d)"), sName.c_str(), nPos, nTotal));
		}
	}
	else
	{
		// No folder compare: show the file name without a position.
		const String sName = paths::FindFileName(m_pMergeDoc->m_filePaths.GetLeft());
		m_wndDedBar.SetFileInfo(sName);
	}
}

/**
 * @brief Dedicated-bar exit button: back to normal mode.
 */
void CMergeEditFrame::OnMerge20DedExit()
{
	SetDedicatedMode(false);
}

/**
 * @brief Dedicated-bar Prev button: previous file. Posted to the main
 * frame (whose OnPrevFile does the move) instead of calling the move
 * directly: the move closes this frame, so nothing of this frame may
 * be touched after it.
 */
void CMergeEditFrame::OnMerge20DedPrevFile()
{
	if (CWnd* pMainWnd = AfxGetMainWnd())
		pMainWnd->PostMessage(WM_COMMAND, ID_PREVFILE);
}

/**
 * @brief Dedicated-bar Next button: next file (see OnMerge20DedPrevFile).
 */
void CMergeEditFrame::OnMerge20DedNextFile()
{
	if (CWnd* pMainWnd = AfxGetMainWnd())
		pMainWnd->PostMessage(WM_COMMAND, ID_NEXTFILE);
}

/**
 * @brief Dedicated-bar review-state button: apply the state to this
 * file. Exactly the S2 key behaviour (CDirDoc::ApplyMerge20ReviewState
 * with the output-flag linkage); the bar button only replaces the
 * 1-5 keys, which are text input in the file editor.
 */
void CMergeEditFrame::OnMerge20DedReview(UINT nID)
{
	if (m_pMergeDoc == nullptr)
		return;
	IDirDoc* pDirDoc = m_pMergeDoc->GetDirDoc();
	if (pDirDoc == nullptr)
		return;
	const ReviewState state = static_cast<ReviewState>(nID - ID_MERGE20_DED_REVIEW_TODO);
	pDirDoc->ApplyMerge20ReviewState(m_pMergeDoc->m_filePaths, state);
	FlashDedBar();
}

/**
 * @brief Dedicated-bar pin button: toggle auto-hide (OPT_MERGE20_DED_PINNED).
 * Pinning on shows the bar (FlashDedBar); pinning off lets the timer
 * hide it again once the cursor leaves.
 */
void CMergeEditFrame::OnMerge20DedPin()
{
	const bool bPinned = !GetOptionsMgr()->GetBool(OPT_MERGE20_DED_PINNED);
	GetOptionsMgr()->SaveOption(OPT_MERGE20_DED_PINNED, bPinned);
	if (bPinned)
		FlashDedBar();
}

/**
 * @brief Dedicated-bar unreviewed-only toggle (display + state only;
 * the patrol behaviour is D3). The state is kept in OPT_MERGE20_TODO_ONLY.
 */
void CMergeEditFrame::OnMerge20DedTodoOnly()
{
	const bool bTodoOnly = !GetOptionsMgr()->GetBool(OPT_MERGE20_TODO_ONLY);
	GetOptionsMgr()->SaveOption(OPT_MERGE20_TODO_ONLY, bTodoOnly);
	FlashDedBar();
}

/**
 * @brief Dedicated-bar sniper toggle (display + state only; the sniper
 * itself is D6). The state is kept in OPT_MERGE20_SNIPER_ON.
 */
void CMergeEditFrame::OnMerge20DedSniper()
{
	const bool bSniperOn = !GetOptionsMgr()->GetBool(OPT_MERGE20_SNIPER_ON);
	GetOptionsMgr()->SaveOption(OPT_MERGE20_SNIPER_ON, bSniperOn);
	// Merge20 (Phase 4 D6): turning the sniper off closes an open
	// overlay (turning it on only arms the double-click).
	if (!bSniperOn)
		m_wndSniper.CloseSniper();
	FlashDedBar();
}

/**
 * @brief Open the auto sniper overlay for the given diff
 * (Merge20 Phase 4 D6). Called from the result view's
 * double-click while sniper mode is ON in dedicated mode.
 */
void CMergeEditFrame::ShowMerge20Sniper(int nDiff)
{
	if (m_pMergeDoc == nullptr)
		return;
	if (m_wndSniper.GetSafeHwnd() == nullptr && !m_wndSniper.Create(this))
	{
		TRACE0("Failed to create CMerge20SniperDlg\n");
		return;
	}
	m_wndSniper.OpenSniper(m_pMergeDoc, nDiff);
}

/**
 * @brief Dedicated-bar zoom display button, and the R key: back to 75%
 * (the D0 default of OPT_MERGE20_DED_ZOOM, HTML DEF_SCALE). Not
 * ZoomText(0): that resets to the original font size (100%).
 */
void CMergeEditFrame::OnMerge20DedZoomReset()
{
	if (!m_bDedicatedMode)
		return;
	if (m_pMergeDoc != nullptr)
	{
		if (CMergeEditView* pView = m_pMergeDoc->GetActiveMergeView())
			pView->ZoomTextTo(750);
	}
	GetOptionsMgr()->SaveOption(OPT_MERGE20_DED_ZOOM, 750);
	FlashDedBar();
}

/**
 * @brief Bare R key in dedicated mode: reset the zoom to 75%.
 *
 * Merge20 (Phase 4 D2, spec section 4(g)): R is a dedicated-mode-only
 * key, handled here (T-4-6). Only when the focus is on the frame or
 * the dedicated bar: in an editable compare view (or the file-path
 * bar) 'r' is text input, so the key is left alone there. This is the
 * same rule as the S2 keys 1-5, which only work in the folder list;
 * the zoom button on the bar is the mouse path to the same reset.
 */
BOOL CMergeEditFrame::PreTranslateMessage(MSG* pMsg)
{
	// Merge20 (Phase 4 D6): while the sniper overlay is open, its
	// keys (Esc/J/K/arrows) work even when the focus is on a header
	// button. The panels handle their own keys in
	// CMerge20SniperView::OnKeyDown; the dialog's own
	// PreTranslateMessage is not reached by the pump for a modeless
	// dialog, so the frame forwards here, through the dialog's
	// public HandleSniperKey (CWnd::PreTranslateMessage is
	// protected and cannot be called from the frame).
	if (m_wndSniper.IsOpen() && pMsg->message == WM_KEYDOWN &&
		m_wndSniper.HandleSniperKey(static_cast<UINT>(pMsg->wParam)))
		return TRUE;
	if (m_bDedicatedMode && pMsg->message == WM_KEYDOWN &&
		pMsg->wParam == 'R' &&
		(GetKeyState(VK_CONTROL) & 0x8000) == 0 &&
		(GetKeyState(VK_MENU) & 0x8000) == 0 &&
		(GetKeyState(VK_SHIFT) & 0x8000) == 0)
	{
		HWND hFocus = ::GetFocus();
		if (hFocus == GetSafeHwnd() || hFocus == m_wndDedBar.GetSafeHwnd() ||
			(m_wndDedBar.GetSafeHwnd() != nullptr && ::IsChild(m_wndDedBar.GetSafeHwnd(), hFocus)))
		{
			PostMessage(WM_COMMAND, ID_MERGE20_DED_ZOOM_RESET);
			return TRUE;
		}
	}
	return __super::PreTranslateMessage(pMsg);
}

/**
 * @brief Make the merge result pane visible (used when the document was
 * opened with an output path, e.g. as a merge tool or for conflict files).
 */
void CMergeEditFrame::ShowMergeResultPane(bool bVisible)
{
	if (m_wndResultBar.m_hWnd != nullptr && bVisible != !!m_wndResultBar.IsWindowVisible())
	{
		ShowControlBar(&m_wndResultBar, bVisible , FALSE);
		GetMainFrame()->UpdateToolbar();
	}
}

/**
 * @brief Enable the Merge Result Pane menu item when a merge session is active.
 */
void CMergeEditFrame::OnUpdateViewMergeResultBar(CCmdUI* pCmdUI)
{
	if (m_wndResultBar.m_hWnd == nullptr)
	{
		pCmdUI->Enable(FALSE);
		pCmdUI->SetCheck(FALSE);
		return;
	}
	CMergeDoc* pMergeDoc = GetMergeDoc();
	pCmdUI->Enable(pMergeDoc->GetMergeResultBuildState());
	OnUpdateControlBarMenu(pCmdUI);
}

void CMergeEditFrame::ActivateFrame(int nCmdShow) 
{
	CMergeFrameCommon::ActivateFrame(nCmdShow);
}

BOOL CMergeEditFrame::DestroyWindow() 
{
	// Merge20 (Phase 4 D1): restore the normal-mode layout first, so the
	// dock state saved below is the normal one. ExitDedicatedMode (not
	// SetDedicatedMode) is used so OPT_MERGE20_DED_MODE keeps the
	// dedicated-mode preference for the next frame.
	if (m_bDedicatedMode)
		ExitDedicatedMode();
	SavePosition();
	SaveActivePane();
	SaveWindowState();
	CFrameWnd* pParentFrame = GetParentFrame();
	BOOL result = CMergeFrameCommon::DestroyWindow();
	if (pParentFrame)
		pParentFrame->OnUpdateFrameTitle(FALSE);
	return result;
}

/**
 * @brief Save coordinates of the frame, splitters, and bars
 *
 * @note Do not save the maximized/restored state here. We are interested
 * in the state of the active frame, and maybe this frame is not active
 */
void CMergeEditFrame::SavePosition()
{
	// save the bars layout
	// save docking positions and sizes
	CDockState m_pDockState;
	GetDockState(m_pDockState);
	m_pDockState.SaveState(_T("Settings"));
	// for the dimensions of the diff pane, use the CSizingControlBar save
	m_wndLocationBar.SaveState(_T("Settings"));
	m_wndDetailBar.SaveState(_T("Settings"));
	m_wndDedBar.SaveState(_T("Settings"));
	if (m_wndResultBar.m_hWnd != nullptr)
		m_wndResultBar.SaveState(_T("Settings"));

	// Save splitter positions
	SaveOptions();
}

void CMergeEditFrame::SaveActivePane()
{
	for (int iRowParent = 0; iRowParent < m_wndSplitter.GetRowCount(); ++iRowParent)
	{
		int iRow, iCol;
		auto& splitterWnd = static_cast<CMergeEditSplitterView*>(m_wndSplitter.GetPane(iRowParent, 0))->m_wndSplitter;
		splitterWnd.GetActivePane(&iRow, &iCol);
		if (iRow >= 0 || iCol >= 0)
			GetOptionsMgr()->SaveOption(OPT_ACTIVE_PANE, (std::max)(iRow, iCol));
	}
}

void CMergeEditFrame::OnClose() 
{
	// Merge20 (Phase 4 D6): hide the sniper overlay before the
	// frame (and its document) goes away.
	m_wndSniper.CloseSniper();
	// clean up pointers.
	CMergeFrameCommon::OnClose();
}

void CMergeEditFrame::OnSetPreviewMode(BOOL bPreview, CPrintPreviewState* pState)
{
	KillTimer(IDT_PREVIEWMODE);
	SetTimer(IDT_PREVIEWMODE, 100, nullptr);
	__super::OnSetPreviewMode(bPreview, pState);
}

/// update splitting position for panels 1/2 and headerbar and statusbar 
void CMergeEditFrame::UpdateHeaderSizes()
{
	if(!::IsWindow(m_wndFilePathBar.m_hWnd) || !::IsWindow(m_wndSplitter.m_hWnd))
		return;
	
	int w[3];
	int wStatus[3]{};
	int pane;
	CMergeDoc * pDoc = GetMergeDoc();
	auto& wndSplitter = GetMergeEditSplitterWnd(0);
	if (wndSplitter.GetColumnCount() > 1)
	{
		for (pane = 0; pane < wndSplitter.GetColumnCount(); pane++)
		{
			int wmin;
			wndSplitter.GetColumnInfo(pane, w[pane], wmin);
			if (w[pane]<1) w[pane]=1; // Perry 2003-01-22 (I don't know why this happens)
			wStatus[pane] = w[pane] + (pane == 0 ? 5 : 7);
		}
	}
	else
	{
		int w2, wmin;
		wndSplitter.GetColumnInfo(0, w2, wmin);
		for (pane = 0; pane < pDoc->m_nBuffers; pane++)
		{
			w[pane] = (w2 - 4 * pDoc->m_nBuffers) / pDoc->m_nBuffers;
			wStatus[pane] = w[pane];
		}
	}

	if (!std::equal(m_nLastSplitPos, m_nLastSplitPos + pDoc->m_nBuffers, w))
	{
		std::copy_n(w, pDoc->m_nBuffers, m_nLastSplitPos);

		// resize controls in header dialog bar
		m_wndFilePathBar.Resize(w);

		m_wndStatusBar.Resize(wStatus);
	}
}

IHeaderBar * CMergeEditFrame::GetHeaderInterface()
{
	return &m_wndFilePathBar;
}

void CMergeEditFrame::UpdateAutoPaneResize()
{
	auto& wndSplitter = GetMergeEditSplitterWnd(0);
	wndSplitter.AutoResizePanes(GetOptionsMgr()->GetBool(OPT_RESIZE_PANES));
}

void CMergeEditFrame::UpdateSplitter()
{
	for (int iRow = 0; iRow < m_wndSplitter.GetRowCount(); ++iRow)
		GetMergeEditSplitterWnd(iRow).RecalcLayout();
	m_wndSplitter.RecalcLayout();
	m_pwndDetailMergeEditSplitterView->m_wndSplitter.RecalcLayout();
}

void CMergeEditFrame::LoadOptions()
{
	const String& optname = m_pMergeDoc->GetDocumentType() == IMergeDoc::DocumentType::Table ? OPT_CMP_TBL_SPLITTER_RATIOS : OPT_CMP_TEXT_SPLITTER_RATIOS;
	auto& splitterWnd = GetMergeEditSplitterWnd(0);
	// Text compare uses horizontal (columns) splitting
	const bool horizontal = splitterWnd.GetColumnCount() != 1;
	SplitterPositions::LoadPaneRatio(optname, 0, m_pMergeDoc->m_nBuffers,
		[this, horizontal](const double* positions, int count) {
			GetMergeEditSplitterWnd(0).SetSplitterRatios(positions, count, horizontal);
		});
	int rowCount = SplitterPositions::GetSplitterRowCount(optname);
	if (rowCount > 1)
		PostMessage(WM_COMMAND, ID_WINDOW_SPLIT);
}

void CMergeEditFrame::SaveOptions()
{
	const String& optname = m_pMergeDoc->GetDocumentType() == IMergeDoc::DocumentType::Table ? OPT_CMP_TBL_SPLITTER_RATIOS : OPT_CMP_TEXT_SPLITTER_RATIOS;
	const auto& splitterWnd = GetMergeEditSplitterWnd(0);
	// Text compare uses horizontal (columns) splitting
	const bool horizontal = splitterWnd.GetColumnCount() != 1;
	const int nRows = m_wndSplitter.GetRowCount();
	if (!GetOptionsMgr()->GetString(optname).empty())
	{
		for (int r = 0; r < nRows; ++r)
		{
			SplitterPositions::SavePaneRatios(optname, r, m_pMergeDoc->m_nBuffers,
				[this, r, horizontal](int i) {
					return GetMergeEditSplitterWnd(r).GetSplitterRatio(i, horizontal);
				});
		}
		double ratio = m_wndSplitter.GetSplitterRatio(0, false);
		SplitterPositions::SaveRowRatio(optname, nRows > 1 ? &ratio : nullptr);
	}
}

/**
 * @brief Synchronize control with splitter position,
 */
void CMergeEditFrame::OnIdleUpdateCmdUI()
{
	UpdateHeaderSizes();
	// Merge20 (Phase 4 D2): refresh the dedicated bar (zoom display,
	// toggles, Prev/Next enablement, review-state check).
	if (m_bDedicatedMode)
		UpdateDedBar();
	// Follow the merge result bar however it was shown or hidden (View menu,
	// its close button, docking state), so that closing it always gives the
	// compared files back their normal editable state
	if (m_wndResultBar.m_hWnd != nullptr && m_pMergeDoc != nullptr)
	{
		const bool bVisible = !!m_wndResultBar.IsVisible();
		// Update merge result conflict statistics in status bar
		if (bVisible)
		{
			int nUnresolved = 0, nConflicts = 0, nWhiteSpaceOnly = 0;
			m_pMergeDoc->GetResultUnresolvedCounts(nUnresolved, nConflicts, nWhiteSpaceOnly);
			m_wndResultBar.UpdateConflictInfo(m_pMergeDoc->GetMergeResultSavePath(), m_pMergeDoc->IsMergeResultModified(),
				nConflicts, nUnresolved, nWhiteSpaceOnly);
		}
	}
	CMergeFrameCommon::OnIdleUpdateCmdUI();
}

LRESULT CMergeEditFrame::CPreviewNumPageButton::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_SETTEXT:
	{
		// Translate the One Page Button and Two Page Button on the Print Preivew toolbar.
		String text = reinterpret_cast<TCHAR*>(lParam);
		String translated = I18n::tr(text);
		if (translated != text)
		{
			SetWindowText(translated.c_str());
			return TRUE;
		}
		break;
	}
	case WM_DESTROY:
		UnsubclassWindow();
		break;
	}
	return __super::WindowProc(message, wParam, lParam);
}

void CMergeEditFrame::OnTimer(UINT_PTR nIDEvent) 
{
	if (nIDEvent == IDT_SAVEPOSITION)
	{
		KillTimer(IDT_SAVEPOSITION);
		// Merge20 (Phase 4 D1): while dedicated mode hides the bars, do
		// not let the delayed pane-size save persist the hidden layout.
		// The normal layout is saved again when the frame is destroyed.
		if (!m_bDedicatedMode)
			SavePosition();
	}
	else if (nIDEvent == IDT_PREVIEWMODE)
	{
		KillTimer(IDT_PREVIEWMODE);
		CWnd* pPreviewBar = GetDlgItem(AFX_IDW_PREVIEW_BAR);
		if (pPreviewBar)
		{
			pPreviewBar->Invalidate();
			I18n::TranslateDialog(pPreviewBar->GetSafeHwnd());
			m_wndPreviewNumPage.SubclassWindow(pPreviewBar->GetDlgItem(AFX_ID_PREVIEW_NUMPAGE)->GetSafeHwnd());
			HWND hPreviewBar = pPreviewBar->GetSafeHwnd();
			if (hPreviewBar != nullptr)
				DarkMode::setChildCtrlsSubclassAndThemeEx(hPreviewBar, true, true);
		}
	}
	else if (nIDEvent == IDT_DEDHIDE)
	{
		// Merge20 (Phase 4 D2): auto-hide the dedicated bar (HTML
		// dedPinned / #dedHover). Polls the cursor every 500ms: while
		// the bar is shown and not pinned, it hides once the flash
		// grace is over and the cursor has been away for 3s; while
		// hidden, moving the cursor to the top 8px strip (the hover
		// strip) shows it again for 1.6s.
		if (!m_bDedicatedMode || m_wndDedBar.m_hWnd == nullptr)
		{
			KillTimer(IDT_DEDHIDE);
		}
		else
		{
			const ULONGLONG now = ::GetTickCount64();
			CPoint ptCursor;
			::GetCursorPos(&ptCursor);
			ScreenToClient(&ptCursor);
			CRect rcBar;
			m_wndDedBar.GetWindowRect(&rcBar);
			ScreenToClient(&rcBar);
			if (m_wndDedBar.IsWindowVisible())
			{
				CRect rcHover = rcBar;
				rcHover.InflateRect(0, 4);
				if (rcHover.PtInRect(ptCursor))
				{
					m_ullDedBarLastMouseOver = now;
				}
				else if (!GetOptionsMgr()->GetBool(OPT_MERGE20_DED_PINNED) &&
					now > m_ullDedBarFlashUntil &&
					now > m_ullDedBarLastMouseOver + 3000)
				{
					ShowControlBar(&m_wndDedBar, FALSE, FALSE);
				}
			}
			else
			{
				CRect rcClient;
				GetClientRect(&rcClient);
				if (ptCursor.y >= rcClient.top && ptCursor.y <= rcClient.top + 8 &&
					ptCursor.x >= rcClient.left && ptCursor.x <= rcClient.right)
				{
					ShowControlBar(&m_wndDedBar, TRUE, FALSE);
					m_ullDedBarFlashUntil = now + 1600;
					m_ullDedBarLastMouseOver = now;
				}
			}
		}
	}
	else
	{
		UpdateHeaderSizes();
	}
	CMergeFrameCommon::OnTimer(nIDEvent);
}

void CMergeEditFrame::OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd)
{
	CMergeFrameCommon::OnMDIActivate(bActivate, pActivateWnd, pDeactivateWnd);

	CMergeDoc *pDoc = GetMergeDoc();
	if (bActivate && pDoc != nullptr)
		this->GetParentFrame()->PostMessage(WM_USER+1);
	return;
}

/**
 * @brief Split panes vertically
 */
void CMergeEditFrame::OnViewSplitVertically() 
{
	auto& wndSplitter = GetMergeEditSplitterWnd(0);
	bool bSplitVertically = (wndSplitter.GetColumnCount() != 1);
	bSplitVertically = !bSplitVertically; // toggle
	GetOptionsMgr()->SaveOption(OPT_SPLIT_HORIZONTALLY, !bSplitVertically);
	for (int iRow = 0; iRow < m_wndSplitter.GetRowCount(); ++iRow)
		GetMergeEditSplitterWnd(iRow).FlipSplit();
	m_pwndDetailMergeEditSplitterView->m_wndSplitter.FlipSplit();
}

/**
 * @brief Update "Split Vertically" UI items
 */
void CMergeEditFrame::OnUpdateViewSplitVertically(CCmdUI* pCmdUI) 
{
	auto& wndSplitter = GetMergeEditSplitterWnd(0);
	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck((wndSplitter.GetColumnCount() != 1));
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CMergeEditFrame::UpdateResources()
{
	m_wndStatusBar.UpdateResources();
	m_wndLocationBar.UpdateResources();
	m_wndDetailBar.UpdateResources();
	m_wndFocusDock.UpdateResources();
	m_wndDedBar.UpdateResources();
	if (m_wndResultBar.m_hWnd != nullptr)
		m_wndResultBar.UpdateResources();
}

/**
 * @brief Save pane sizes and positions when one of panes requests it.
 */
LRESULT CMergeEditFrame::OnStorePaneSizes(WPARAM wParam, LPARAM lParam)
{
	// Merge20 (Phase 4 D1): while dedicated mode hides the bars, ignore
	// the pane-size save request (hiding a bar posts this message). The
	// saved dock state must stay the normal-mode layout, so a frame
	// opened later does not start with its bars hidden.
	if (m_bDedicatedMode)
		return 0;
	KillTimer(IDT_SAVEPOSITION);
	SetTimer(IDT_SAVEPOSITION, 300, nullptr);
	return 0;
}

void CMergeEditFrame::OnSize(UINT nType, int cx, int cy) 
{
	__super::OnSize(nType, cx, cy);
	
	UpdateHeaderSizes();
}

void CMergeEditFrame::OnViewDisplayFilterBar()
{
	if (!m_pFilterBar)
		ShowFilterBar();
	else
		HideFilterBar();
}

void CMergeEditFrame::OnUpdateDisplayViewFilterBar(CCmdUI* pCmdUI)
{
	pCmdUI->Enable(TRUE);
	pCmdUI->SetCheck(m_pFilterBar != nullptr);
}

void CMergeEditFrame::OnDisplayFilterBarClose()
{
	HideFilterBar();
	GetActiveView()->SetFocus();
}

void CMergeEditFrame::OnDisplayFilterBarMenu()
{
	if (m_pFilterBar)
		m_pFilterBar->ShowFilterMenu();
}

void CMergeEditFrame::ShowFilterBar()
{
	if (!m_pFilterBar)
		m_pFilterBar.reset(new CLineFilterBar());
	if (!::IsWindow(m_pFilterBar->GetSafeHwnd()) && !m_pFilterBar->Create(this))
	{
		TRACE0("Failed to create filter bar\n");
		m_pFilterBar.reset();
		return;
	}
	ShowControlBar(m_pFilterBar.get(), TRUE, FALSE);
}

void CMergeEditFrame::HideFilterBar()
{
	if (m_pFilterBar != nullptr && ::IsWindow(m_pFilterBar->GetSafeHwnd()))
	{
		ShowControlBar(m_pFilterBar.get(), FALSE, FALSE);
		m_pFilterBar->DestroyWindow();
	}
	m_pFilterBar.reset();
}

/**
 * @brief Remember/restore splitter position
 */
void CMergeEditFrame::OnWindowPreserveSplitterPosition()
{
	const String& optname = m_pMergeDoc->GetDocumentType() == IMergeDoc::DocumentType::Table ? OPT_CMP_TBL_SPLITTER_RATIOS : OPT_CMP_TEXT_SPLITTER_RATIOS;
	auto& splitterWnd = GetMergeEditSplitterWnd(0);
	if (!GetOptionsMgr()->GetString(optname).empty())
	{
		GetOptionsMgr()->SaveOption(optname, _T(""));
		const bool horizontal = splitterWnd.GetColumnCount() != 1;
		for (int r = 0; r < m_wndSplitter.GetRowCount(); ++r)
			GetMergeEditSplitterWnd(r).ResetSplitterRatios(horizontal);
		m_wndSplitter.ResetSplitterRatios(false);
		return;
	}
	const bool horizontal = splitterWnd.GetColumnCount() != 1;
	const int nRows = m_wndSplitter.GetRowCount();
	for (int r = 0; r < nRows; ++r)
	{
		SplitterPositions::SavePaneRatios(optname, r, m_pMergeDoc->m_nBuffers,
			[this, r, horizontal](int i) { return GetMergeEditSplitterWnd(r).GetSplitterRatio(i, horizontal); });
	}
}

void CMergeEditFrame::OnUpdateWindowPreserveSplitterPosition(CCmdUI* pCmdUI)
{
	const String& optname = m_pMergeDoc->GetDocumentType() == IMergeDoc::DocumentType::Table ? OPT_CMP_TBL_SPLITTER_RATIOS : OPT_CMP_TEXT_SPLITTER_RATIOS;
	pCmdUI->SetCheck(!GetOptionsMgr()->GetString(optname).empty());
}

