// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FileListBar.cpp
 *
 * @brief Implementation file for CMerge20FileListBar
 *
 * Merge20 (Panel P0, design section 12.4/12.7): the persistent
 * file-list panel. Follows the CDiffViewBar / Merge20DedBar
 * precedent (CSizingControlBarCF, created in
 * CMergeEditFrame::OnCreateClient, docked in OnCreate). Two
 * differences from the precedents, both deliberate:
 * - SCBS_SIZECHILD is NOT set: it sizes only the first child
 *   (sizecbar.cpp: "automatic child resizing - only one child is
 *   allowed"), and this bar has three children (count line, list,
 *   empty label). All children are laid out manually in
 *   LayoutChildren().
 * - The width is clamped (LoadState / CalcFixedLayout, D2 lesson),
 *   and the initial width is set explicitly (D1 lesson).
 *
 * P3 (design section 12.7) adds the linkage: a single click on a
 * row opens the file (or activates its frame when already open),
 * the output column toggles the flag, the review column cycles the
 * state, keys 1-5 / Space (via the frame's PreTranslateMessage)
 * and the file-row context menu set the review state, and Refresh()
 * re-selects the compare window's file, so the panel selection
 * follows file moves. Panel-initiated changes are also pushed to
 * the folder view (row repaint + status-bar count) through the
 * CDirDoc forwarders, closing the P2 known limitation.
 *
 * B4 (best-workflow, gap analysis sections 3.5 and 4.2 B4) adds
 * the undecided high-risk remainder line directly under the count
 * line: "Undecided high risk: N", from the same B2 counts
 * (CDirDoc::GetMerge20CloseWarnCounts). It shows only while N >
 * 0 and takes no vertical space at 0. The display lives here
 * alone (no status-bar duplicate). UpdateHighRiskRemain() runs
 * from Refresh(), so it rides the S3 status-update path: every
 * review-state notification refreshes the remainder too.
 *
 * U5 (UI redesign, design sections 2.1 and 4/U5) adds the WizMerge
 * accent: a 2px accent underline under the count, search and
 * filter-tab rows (OnPaint, rows recorded by LayoutChildren; each
 * child ends 2px above its row bottom so the line is never
 * overpainted), and owner-drawn filter tabs (TCS_OWNERDRAWFIXED +
 * DrawItem, HTML .pill.on: only the selected tab gets an accent
 * border + opaque tinted fill). Columns, rows, buttons and their
 * IDs are unchanged.
 */

#include "StdAfx.h"
#include "Merge20/Merge20FileListBar.h"
#include "Merge20/Merge20Theme.h"
#include "Merge20/Merge20BatchCheckDlg.h"
#include "Merge20/Merge20CloseWarn.h"
#include "Merge20/Merge20HighRiskRemain.h"
#include "Merge20/Merge20HighRiskConfirmDlg.h"
#include "DiffContext.h"
#include "DirDoc.h"
#include "Logger.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"
#include "Merge20/Merge20FileListFilter.h"

#include <map>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20FileListBar, CSizingControlBarCF);

namespace
{
	/** @brief Initial panel width: wide enough for the six P1
	 * columns (P0: 280px for the single file-name column). */
	constexpr LONG Merge20FileListDefaultWidth = 480;
	/** @brief Narrowest usable width (file names stay readable). */
	constexpr LONG Merge20FileListMinWidth = 160;
	/** @brief Widest allowed width (the file panes keep their area). */
	constexpr LONG Merge20FileListMaxWidth = 600;
	/** @brief Height of the count line at the top of the bar (P1). */
	constexpr LONG Merge20FileListCountHeight = 22;
	/** @brief Height of the high-risk remainder line (B4): same as
	 * the count line it sits under. */
	constexpr LONG Merge20FileListHighRiskHeight = 22;
	/** @brief Count-computation time (ms) above which the B4
	 * update writes one measure-log line (D10 pattern): a
	 * 100-1000-file compare that stays below this stays silent. */
	constexpr long long Merge20FileListHighRiskSlowMs = 50;
	/** @brief Height of the search row (P2): label + edit. */
	constexpr LONG Merge20FileListSearchHeight = 26;
	/** @brief Width of the search label (P2), so the edit fills the rest. */
	constexpr LONG Merge20FileListSearchLabelWidth = 160;
	/** @brief Height of the filter-tab row (P2). */
	constexpr LONG Merge20FileListTabsHeight = 26;
	/** @brief Height of the risk-tab row (B3): same as the filter tabs. */
	constexpr LONG Merge20FileListRiskTabsHeight = 26;
	/** @brief Risk-filter values of the four B3 risk tabs, in tab
	 * order: All risks (-1), High (2), Medium (1), None (0). The
	 * tab order is the patrol order (High first); the values are
	 * the static_cast<int>(RiskLevel) the filter module takes. */
	constexpr int Merge20FileListRiskTabValues[4] = { -1, 2, 1, 0 };

	/** @brief Tab index of one risk-filter value (-1 = all risks).
	 * An unknown value (stale or hand-edited option) falls back
	 * to the "All risks" tab. */
	int Merge20FileListRiskTabFromValue(int riskFilter)
	{
		for (int i = 0; i < 4; ++i)
		{
			if (Merge20FileListRiskTabValues[i] == riskFilter)
				return i;
		}
		return 0;
	}
	/** @brief Height of the button row (P2): batch check / expand all / collapse all. */
	constexpr LONG Merge20FileListButtonsHeight = 28;
	/** @brief Widths of the three buttons in the button row (P2). */
	constexpr LONG Merge20FileListBatchButtonWidth = 110;
	constexpr LONG Merge20FileListExpandButtonWidth = 90;
	constexpr LONG Merge20FileListCollapseButtonWidth = 110;
	/** @brief Width of the slim toggle button (P4). */
	constexpr LONG Merge20FileListSlimButtonWidth = 90;
	/** @brief Gap between the buttons in the button row (P2). */
	constexpr LONG Merge20FileListButtonGap = 4;

	/** @brief Build the relPath -> DIFFITEM map once, so the panel's
	 * value-only rows can be resolved to DIFFITEMs for applying
	 * output flags (P2, P-R3/P-R5). The walk itself cannot rescan,
	 * so the pointers stay valid while applying. */
	void Merge20FileListBuildRelPathMap(const CDiffContext& ctxt,
		std::map<String, const DIFFITEM*>& byRelPath)
	{
		for (DIFFITEM* p = ctxt.GetFirstDiffPosition(); p != nullptr;)
		{
			const DIFFITEM& di = ctxt.GetNextDiffPosition(p);
			byRelPath[di.getItemRelativePath()] = &di;
		}
	}

	/** @brief B4 remainder text for N undecided high-risk items.
	 * Literal _() call (see CMerge20FileListCtrl::SetupColumns):
	 * the .pot generator only scans literal arguments. */
	String Merge20FileListHighRiskText(int highRisk)
	{
		return strutils::format_string1(_("Undecided high risk: %1"), strutils::to_str(highRisk));
	}
} // namespace

BEGIN_MESSAGE_MAP(CMerge20FileListBar, CSizingControlBarCF)
	//{{AFX_MSG_MAP(CMerge20FileListBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_WINDOWPOSCHANGED()
	ON_WM_DRAWITEM()
	ON_WM_PAINT()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_SETTINGCHANGE()
	ON_EN_CHANGE(IDC_MERGE20_FILELIST_SEARCH, OnSearchChanged)
	ON_NOTIFY(TCN_SELCHANGE, IDC_MERGE20_FILELIST_FILTER_TABS, OnFilterTabChanged)
	ON_NOTIFY(TCN_SELCHANGE, IDC_MERGE20_FILELIST_RISK_TABS, OnRiskTabChanged)
	ON_BN_CLICKED(IDC_MERGE20_FILELIST_BATCH, OnMerge20BatchCheck)
	ON_BN_CLICKED(IDC_MERGE20_FILELIST_EXPAND_ALL, OnExpandAll)
	ON_BN_CLICKED(IDC_MERGE20_FILELIST_COLLAPSE_ALL, OnCollapseAll)
	ON_BN_CLICKED(IDC_MERGE20_FILELIST_SLIM, OnSlimToggle)
	ON_MESSAGE(WM_MERGE20_FILELIST_ROW_RCLICK, OnRowRClick)
	ON_MESSAGE(WM_MERGE20_FILELIST_ROW_CLICK, OnRowClick)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CMerge20FileListBar::CMerge20FileListBar()
: m_hwndFrame(nullptr)
, m_pDirDoc(nullptr)
{
}

CMerge20FileListBar::~CMerge20FileListBar()
{
}

BOOL CMerge20FileListBar::Create(
	CWnd* pParentWnd,
	LPCTSTR lpszWindowName /*= nullptr*/,
	DWORD dwStyle /*= WS_CHILD | WS_VISIBLE | CBRS_TOP*/,
	UINT nID /*= AFX_IDW_PANE_FIRST*/)
{
	return CSizingControlBarCF::Create(
		lpszWindowName,
		pParentWnd,
		nID,
		dwStyle);
}

int CMerge20FileListBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CSizingControlBarCF::OnCreate(lpCreateStruct) == -1)
		return -1;

	// Left-docked sizing edges (CLocationBar precedent). No
	// SCBS_SIZECHILD: see the file header comment.
	SetSCBStyle(SCBS_EDGELEFT | SCBS_EDGERIGHT);

	// Explicit initial size (D1 lesson): an empty sizing bar must
	// not take its size from layout defaults.
	m_szVert = CSize(Merge20FileListDefaultWidth, 200);
	m_szMinVert = CSize(Merge20FileListMinWidth, 100);
	m_szFloat = CSize(Merge20FileListDefaultWidth, 400);
	m_szMinFloat = CSize(Merge20FileListMinWidth, 100);

	CFont* pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	CRect rc(0, 0, 0, 0);
	// Search row (P2, P-R4): label + edit. The label takes the
	// default control ID (no ID): its text is re-applied in
	// UpdateResources() through the member variable.
	if (!m_stSearchLabel.Create(_("Search file name or folder:").c_str(),
		WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
		rc, this))
	{
		TRACE0("Failed to create Merge20FileListBar search label\n");
		return -1;
	}
	m_stSearchLabel.SetFont(pFont);
	if (!m_wndSearch.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
		rc, this, IDC_MERGE20_FILELIST_SEARCH))
	{
		TRACE0("Failed to create Merge20FileListBar search edit\n");
		return -1;
	}
	m_wndSearch.SetFont(pFont);
	// Filter tabs (P2, P-R10): "all" plus the seven action
	// categories. Single-line tabs show scroll arrows when they
	// do not fit, so all eight tabs stay reachable. U5: owner-drawn
	// (TCS_OWNERDRAWFIXED + DrawFilterTab, HTML .pill.on).
	if (!m_wndFilterTabs.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_OWNERDRAWFIXED,
		rc, this, IDC_MERGE20_FILELIST_FILTER_TABS))
	{
		TRACE0("Failed to create Merge20FileListBar filter tabs\n");
		return -1;
	}
	m_wndFilterTabs.SetFont(pFont);
	SetupFilterTabs();
	// Risk tabs (B3): "All risks" plus High / Medium / None. A
	// second tab row, owner-drawn like the action tabs (the P2
	// pattern above); a combo box cannot be laid out in this bar
	// (its window height carries the drop-down height).
	if (!m_wndRiskTabs.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_OWNERDRAWFIXED,
		rc, this, IDC_MERGE20_FILELIST_RISK_TABS))
	{
		TRACE0("Failed to create Merge20FileListBar risk tabs\n");
		return -1;
	}
	m_wndRiskTabs.SetFont(pFont);
	SetupRiskTabs();
	// Button row (P2): batch check (P-R3) and expand/collapse all
	// (P-R5). The texts are re-applied in UpdateResources().
	if (!m_btnBatch.Create(_("Batch check...").c_str(),
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FILELIST_BATCH))
	{
		TRACE0("Failed to create Merge20FileListBar batch button\n");
		return -1;
	}
	m_btnBatch.SetFont(pFont);
	if (!m_btnExpandAll.Create(_("Expand all").c_str(),
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FILELIST_EXPAND_ALL))
	{
		TRACE0("Failed to create Merge20FileListBar expand-all button\n");
		return -1;
	}
	m_btnExpandAll.SetFont(pFont);
	if (!m_btnCollapseAll.Create(_("Collapse all").c_str(),
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FILELIST_COLLAPSE_ALL))
	{
		TRACE0("Failed to create Merge20FileListBar collapse-all button\n");
		return -1;
	}
	m_btnCollapseAll.SetFont(pFont);
	// Slim toggle (P4, design section 12.4): a check button for
	// the "file name + review state only" view. The text is
	// re-applied in UpdateResources().
	if (!m_btnSlim.Create(_("Slim view").c_str(),
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
		rc, this, IDC_MERGE20_FILELIST_SLIM))
	{
		TRACE0("Failed to create Merge20FileListBar slim button\n");
		return -1;
	}
	m_btnSlim.SetFont(pFont);
	// Count line (P1, P-R6): "N displayed, M checked, K unreviewed"
	// (S3 definition). Shown with the list; hidden with it when
	// there is no folder document.
	if (!m_stCount.Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP,
		rc, this, IDC_MERGE20_FILELIST_COUNT))
	{
		TRACE0("Failed to create Merge20FileListBar count label\n");
		return -1;
	}
	m_stCount.SetFont(pFont);
	// High-risk remainder line (B4): "Undecided high risk: N".
	// Created hidden (no WS_VISIBLE): Refresh() shows it only
	// while N > 0, so at 0 it is not a warning display. The text
	// is set by UpdateHighRiskRemain(), not here.
	if (!m_stHighRisk.Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP,
		rc, this, IDC_MERGE20_FILELIST_HIGHRISK))
	{
		TRACE0("Failed to create Merge20FileListBar high-risk label\n");
		return -1;
	}
	m_stHighRisk.SetFont(pFont);
	if (!m_wndList.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
		rc, this, IDC_MERGE20_FILELIST_LIST))
	{
		TRACE0("Failed to create Merge20FileListCtrl\n");
		return -1;
	}
	m_wndList.SetFont(pFont);
	// P1 has six columns (selection, file name, action category,
	// risk, old/changed/new counts, review state); the control
	// owns their creation and header texts.
	m_wndList.SetupColumns();
	// The empty label starts hidden; Refresh() shows it when there
	// is no folder document (single-file compare, design section 12.5).
	if (!m_stEmpty.Create(_("No folder comparison").c_str(), WS_CHILD | SS_LEFT,
		rc, this, IDC_MERGE20_FILELIST_EMPTY))
	{
		TRACE0("Failed to create Merge20FileListBar empty label\n");
		return -1;
	}
	m_stEmpty.SetFont(pFont);
	UpdateResources();
	LayoutChildren();

	return 0;
}

/**
 * @brief Load the saved bar size, then clamp the width.
 *
 * The base LoadState reads the persisted sizeVertCX. Without the
 * clamp, a saved abnormal width (or a dragged one) would come back
 * on the next frame and steal client area from the file panes
 * (D2 lesson, same shape as CMerge20DedBar::LoadState).
 */
void CMerge20FileListBar::LoadState(LPCTSTR lpszProfileName)
{
	CSizingControlBarCF::LoadState(lpszProfileName);
	m_szVert.cx = (std::max)(Merge20FileListMinWidth, (std::min)(m_szVert.cx, Merge20FileListMaxWidth));
	m_szFloat.cx = (std::max)(Merge20FileListMinWidth, (std::min)(m_szFloat.cx, Merge20FileListMaxWidth));
}

/**
 * @brief Clamp the width the layout gives the bar.
 *
 * The base implementation returns max(min, m_szVert.cx), so dragging
 * the bar's right edge would let the bar grow over the file panes.
 * Clamping the returned size keeps all the base row negotiation and
 * only caps the width; the bar is only ever docked left or right
 * (EnableDocking LEFT|RIGHT), so the horizontal branch is left to
 * the base.
 */
CSize CMerge20FileListBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
	CSize size = CSizingControlBarCF::CalcFixedLayout(bStretch, bHorz);
	if (!bHorz)
		size.cx = (std::max)(Merge20FileListMinWidth, (std::min)(size.cx, Merge20FileListMaxWidth));
	return size;
}

void CMerge20FileListBar::OnSize(UINT nType, int cx, int cy)
{
	CSizingControlBarCF::OnSize(nType, cx, cy);
	LayoutChildren();
}

/**
 * @brief Informs parent frame (CMergeEditFrame) when bar is closed.
 *
 * After bar is closed parent frame saves bar states.
 * (Same as CMerge20DedBar::OnWindowPosChanged.)
 */
void CMerge20FileListBar::OnWindowPosChanged(WINDOWPOS* lpwndpos)
{
	CSizingControlBarCF::OnWindowPosChanged(lpwndpos);

	if (m_hwndFrame != nullptr)
	{
		// If WINDOWPOS.flags has SWP_HIDEWINDOW flag set
		if ((lpwndpos->flags & SWP_HIDEWINDOW) != 0)
			::PostMessage(m_hwndFrame, MSG_STORE_PANESIZES, 0, 0);
	}
}

/**
 * @brief Paint the bar background and the U5 accent underlines.
 * The base OnPaint paints nothing (see sizecbar.cpp), and the
 * background comes from the class brush (COLOR_BTNFACE); this
 * repaints it explicitly so the underline strips stay clean,
 * then draws a 2px accent line at the bottom of the count,
 * search and filter-tab rows recorded by LayoutChildren. Each
 * child ends 2px above its row bottom, so the line is never
 * overpainted by a child.
 */
void CMerge20FileListBar::OnPaint()
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(&rect);
	if (rect.IsRectEmpty())
		return;
	dc.FillSolidRect(&rect, ::GetSysColor(COLOR_BTNFACE));
	const COLORREF crAccent = Merge20Theme::GetAccentColor(Merge20Theme::IsDarkMode());
	const CRect* pRows[] = { &m_rcSearchRow, &m_rcTabsRow, &m_rcCountRow };
	for (size_t i = 0; i < std::size(pRows); ++i)
	{
		if (pRows[i]->IsRectEmpty())
			continue;
		CRect rcLine = *pRows[i];
		rcLine.top = rcLine.bottom - 2;
		if (rcLine.top >= rect.top && rcLine.bottom <= rect.bottom)
			dc.FillSolidRect(&rcLine, crAccent);
	}
}

/**
 * @brief Route owner-draw requests to DrawFilterTab (U5).
 * Only the filter tabs and the B3 risk tabs are owner-drawn
 * (TCS_OWNERDRAWFIXED).
 */
void CMerge20FileListBar::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (lpDrawItemStruct != nullptr && lpDrawItemStruct->CtlType == ODT_TAB &&
		(nIDCtl == IDC_MERGE20_FILELIST_FILTER_TABS || nIDCtl == IDC_MERGE20_FILELIST_RISK_TABS))
		DrawFilterTab(nIDCtl == IDC_MERGE20_FILELIST_FILTER_TABS ? m_wndFilterTabs : m_wndRiskTabs,
			lpDrawItemStruct);
	else
		CSizingControlBarCF::OnDrawItem(nIDCtl, lpDrawItemStruct);
}

/**
 * @brief Owner-draw one filter tab (U5, HTML .pill.on).
 *
 * The selected tab gets an accent border + opaque tinted fill
 * (light #DBEAFE, dark #1E3A5F) with theme text; unselected tabs
 * get a muted outline and muted text on the bar background (the
 * "semi-transparent" look). The whole item rect is painted
 * first: the rounded pill leaves the corners unpainted, and a
 * tab repaints alone when selected, so the corners must carry
 * the bar background colour. The text comes from the tab item
 * itself (see SetupFilterTabs), so no new strings are needed.
 */
void CMerge20FileListBar::DrawFilterTab(CTabCtrl& wndTabs, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (lpDrawItemStruct == nullptr)
		return;
	CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
	if (pDC == nullptr)
		return;
	const CRect rcItem(lpDrawItemStruct->rcItem);
	if (rcItem.IsRectEmpty() || lpDrawItemStruct->itemID < 0)
		return;
	const bool bDark = Merge20Theme::IsDarkMode();
	const bool bSelected = (lpDrawItemStruct->itemState & ODS_SELECTED) != 0;
	const bool bFocused = (lpDrawItemStruct->itemState & ODS_FOCUS) != 0;

	TCHAR szText[256] = { 0 };
	TCITEM item = { 0 };
	item.mask = TCIF_TEXT;
	item.pszText = szText;
	item.cchTextMax = 256;
	wndTabs.GetItem(static_cast<int>(lpDrawItemStruct->itemID), &item);

	const COLORREF crBarBg = ::GetSysColor(COLOR_BTNFACE);
	const COLORREF crAccent = Merge20Theme::GetAccentColor(bDark);
	const COLORREF crText = bDark ? RGB(241, 245, 249) : RGB(15, 23, 42);
	const COLORREF crMuted = bDark ? RGB(148, 163, 184) : RGB(100, 116, 139);
	COLORREF crFill = crBarBg;
	COLORREF crLine = crMuted;
	COLORREF crTabText = crMuted;
	if (bSelected)
	{
		// Opaque tinted fill (light #DBEAFE, dark #1E3A5F) +
		// accent border + theme text.
		crFill = bDark ? RGB(30, 58, 95) : RGB(219, 234, 254);
		crLine = crAccent;
		crTabText = crText;
	}
	pDC->FillSolidRect(&rcItem, crBarBg);
	CRect rcDraw = rcItem;
	rcDraw.DeflateRect(1, 1);
	Merge20Theme::DrawRoundedRect(pDC, rcDraw, 5, crFill, crLine);

	CFont* pFont = wndTabs.GetFont();
	if (pFont == nullptr)
		pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	CFont* pOldFont = pDC->SelectObject(pFont);
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(crTabText);
	CRect rcText = rcDraw;
	pDC->DrawText(CString(szText), &rcText,
		DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
	if (bFocused)
	{
		CRect rcFocus = rcDraw;
		rcFocus.DeflateRect(2, 2);
		pDC->DrawFocusRect(&rcFocus);
	}
	pDC->SelectObject(pOldFont);
}

/**
 * @brief Repaint the bar, the underlines and the tabs after a
 * system colour change (U5). The base handler updates the
 * class background brush.
 */
void CMerge20FileListBar::OnSysColorChange()
{
	CSizingControlBarCF::OnSysColorChange();
	RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

/**
 * @brief Same repaint as OnSysColorChange, for theme changes
 * announced through WM_SETTINGCHANGE (U5).
 */
void CMerge20FileListBar::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	CSizingControlBarCF::OnSettingChange(uFlags, lpszSection);
	RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

/**
 * @brief Rebuild the list from the folder document.
 *
 * With a folder document the count line and the list are shown
 * and the empty label is hidden; without one (single-file
 * compare, design section 12.5) the count line and the list are
 * hidden and disabled and the empty label shows the one line "No
 * folder comparison". No-op while the bar itself is hidden: the
 * frame calls Refresh() again when the bar is shown (OnBarCheck)
 * or activated (OnMDIActivate).
 */
void CMerge20FileListBar::Refresh()
{
	if (m_wndList.GetSafeHwnd() == nullptr || !IsWindowVisible())
		return;
	const bool bHasDirDoc = (m_pDirDoc != nullptr);
	// P2: the search row, the filter tabs and the buttons belong to
	// the folder compare too: without a folder document they are
	// hidden and disabled, like the count line and the list.
	m_stSearchLabel.ShowWindow(bHasDirDoc ? SW_SHOW : SW_HIDE);
	m_wndSearch.ShowWindow(bHasDirDoc ? SW_SHOW : SW_HIDE);
	m_wndFilterTabs.ShowWindow(bHasDirDoc ? SW_SHOW : SW_HIDE);
	m_wndRiskTabs.ShowWindow(bHasDirDoc ? SW_SHOW : SW_HIDE);
	m_btnBatch.ShowWindow(bHasDirDoc ? SW_SHOW : SW_HIDE);
	m_btnExpandAll.ShowWindow(bHasDirDoc ? SW_SHOW : SW_HIDE);
	m_btnCollapseAll.ShowWindow(bHasDirDoc ? SW_SHOW : SW_HIDE);
	m_stCount.ShowWindow(bHasDirDoc ? SW_SHOW : SW_HIDE);
	m_wndList.ShowWindow(bHasDirDoc ? SW_SHOW : SW_HIDE);
	m_stEmpty.ShowWindow(bHasDirDoc ? SW_HIDE : SW_SHOW);	m_stSearchLabel.EnableWindow(bHasDirDoc);
	m_wndSearch.EnableWindow(bHasDirDoc);
	m_wndFilterTabs.EnableWindow(bHasDirDoc);
	m_wndRiskTabs.EnableWindow(bHasDirDoc);
	m_btnBatch.EnableWindow(bHasDirDoc);
	m_btnExpandAll.EnableWindow(bHasDirDoc);
	m_btnCollapseAll.EnableWindow(bHasDirDoc);
	m_wndList.EnableWindow(bHasDirDoc);
	if (bHasDirDoc)
		m_wndList.SetDiffContext(&m_pDirDoc->GetDiffContext());
	else
	{
		m_wndList.SetDiffContext(nullptr);
		// No folder document: the remainder line goes away with
		// the count line. Its state resets, so a later folder
		// compare starts hidden until the first count arrives.
		if (m_bHighRiskVisible)
		{
			m_bHighRiskVisible = false;
			m_nHighRiskShown = 0;
		}
		m_stHighRisk.ShowWindow(SW_HIDE);
	}
	m_wndList.Refresh();
	if (bHasDirDoc)
	{
		m_stCount.SetWindowText(m_wndList.GetCountText().c_str());
		UpdateHighRiskRemain();
	}
	// P3 (P-R7): re-select the file the compare window shows. The
	// rows were just rebuilt (a rescan may have moved them), so the
	// selection is re-applied by relative path, not kept by index.
	// A filtered-out or removed file clears the selection instead
	// of leaving a stale row selected (see SelectRowByRelPath).
	if (!m_sCurrentRelPath.empty())
		m_wndList.SelectRowByRelPath(m_sCurrentRelPath);
	LayoutChildren();
}

/**
 * @brief Update the B4 high-risk remainder line (gap analysis
 * section 4.2 B4).
 *
 * The count is B2's highRisk (CDirDoc::GetMerge20CloseWarnCounts),
 * so this line and the close warning can never disagree: a TODO
 * file with risk HIGH counts 1, and so does each undecided diff
 * in a HIGH-risk file (see Merge20CloseWarn.h). The pure trigger
 * module (Merge20HighRiskRemainDecideUpdate) decides whether the
 * control needs touching at all: a Refresh with an unchanged
 * count leaves it alone, so search keystrokes and unrelated
 * notifications do not churn the control. At 0 the line hides;
 * LayoutChildren (called by Refresh() after this) reclaims its
 * row, so it takes no vertical space and is never a warning
 * display. No-op without a folder document or before OnCreate.
 *
 * The count is timed (D10 measure-log pattern): a slow update on
 * a 100-1000-file compare writes one log line, so the
 * real-machine check can see it; fast updates stay silent.
 */
void CMerge20FileListBar::UpdateHighRiskRemain()
{
	if (m_pDirDoc == nullptr || m_stHighRisk.GetSafeHwnd() == nullptr)
		return;
	const auto t0 = std::chrono::steady_clock::now();
	const Merge20CloseWarnCounts counts = m_pDirDoc->GetMerge20CloseWarnCounts();
	const long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();
	if (elapsedMs >= Merge20FileListHighRiskSlowMs)
	{
		const std::string line = "Merge20 high-risk remain: " + std::to_string(elapsedMs) +
			" ms for " + std::to_string(counts.total) + " files";
		RootLogger::Info(String(line.begin(), line.end()));
	}
	const Merge20HighRiskRemainUpdate update = Merge20HighRiskRemainDecideUpdate(
		m_bHighRiskVisible, m_nHighRiskShown, counts.highRisk);
	if (!update.changed)
		return;
	m_bHighRiskVisible = update.visible;
	m_nHighRiskShown = update.highRisk;
	if (update.visible)
		m_stHighRisk.SetWindowText(Merge20FileListHighRiskText(update.highRisk).c_str());
	m_stHighRisk.ShowWindow(update.visible ? SW_SHOW : SW_HIDE);
}

/**
 * @brief Save the panel state (Panel P4).
 *
 * The slim flag, the selected filter tab, the search string and
 * the folded directories go to the OPT_MERGE20_FILELIST_* keys
 * (OptionsMgr persists them; the frame calls this from
 * SavePosition, so the values are written when the frame closes
 * and on the delayed pane-size save after hiding a bar). The
 * display toggle and the width are NOT saved here: P0 already
 * persists them (OnBarCheck and the bar's SaveState). The search
 * string is saved here, not on every keystroke (OnSearchChanged
 * would write the registry on each typed character); the tab,
 * the folded set and the slim flag are saved at once when they
 * change, and again here (the D11 dual-save precedent). Several
 * frames share these keys: the last frame to close wins, the
 * same rule as the bar width.
 */
void CMerge20FileListBar::SavePanelState()
{
	COptionsMgr* pOptions = GetOptionsMgr();
	if (pOptions == nullptr)
		return;
	pOptions->SaveOption(OPT_MERGE20_FILELIST_SLIM, m_bSlim);
	pOptions->SaveOption(OPT_MERGE20_FILELIST_TAB, m_nSelectedTab);
	pOptions->SaveOption(OPT_MERGE20_FILELIST_RISK_FILTER,
		Merge20FileListRiskTabValues[static_cast<size_t>(m_nSelectedRiskTab)]);
	if (m_wndSearch.GetSafeHwnd() != nullptr)
	{
		CString text;
		m_wndSearch.GetWindowText(text);
		pOptions->SaveOption(OPT_MERGE20_FILELIST_SEARCH,
			String(static_cast<LPCTSTR>(text)));
	}
	if (m_wndList.GetSafeHwnd() != nullptr)
	{
		pOptions->SaveOption(OPT_MERGE20_FILELIST_COLLAPSED,
			Merge20JoinCollapsedPaths(m_wndList.GetCollapsedPaths()));
	}
}

/**
 * @brief Restore the panel state (Panel P4).
 *
 * Reads the OPT_MERGE20_FILELIST_* keys and applies them without
 * saving. Slim first: it decides which rows LayoutChildren
 * shows. The tab value is clamped (a stale or hand-edited value
 * must not select a tab that does not exist). The folded set may
 * hold stale paths (folders that no longer exist): they only
 * match rows that exist. Setting the search edit fires EN_CHANGE,
 * which filters the list; an empty saved string is a no-op.
 * Called by the frame from OnCreate, before the first
 * RefreshFileListPanel(), so the first list already shows the
 * restored state. Every access is guarded: the frame calls this
 * after the bar is created, but a missing child must not crash.
 */
void CMerge20FileListBar::RestorePanelState()
{
	COptionsMgr* pOptions = GetOptionsMgr();
	if (pOptions == nullptr)
		return;
	ApplySlimState(pOptions->GetBool(OPT_MERGE20_FILELIST_SLIM));
	int tab = pOptions->GetInt(OPT_MERGE20_FILELIST_TAB);
	if (tab < 0)
		tab = 0;
	if (tab > 7)
		tab = 7;
	m_nSelectedTab = tab;
	if (m_wndFilterTabs.GetSafeHwnd() != nullptr)
		m_wndFilterTabs.SetCurSel(tab);
	// B3: the risk tab is restored from the filter value (an
	// unknown value falls back to "All risks"). The list gets
	// the value, not the tab index (see SetRiskFilter).
	m_nSelectedRiskTab = Merge20FileListRiskTabFromValue(
		pOptions->GetInt(OPT_MERGE20_FILELIST_RISK_FILTER));
	if (m_wndRiskTabs.GetSafeHwnd() != nullptr)
		m_wndRiskTabs.SetCurSel(m_nSelectedRiskTab);
	if (m_wndList.GetSafeHwnd() != nullptr)
	{
		m_wndList.SetActionFilter(tab == 0 ? -1 : tab - 1);
		m_wndList.SetRiskFilter(
			Merge20FileListRiskTabValues[static_cast<size_t>(m_nSelectedRiskTab)]);
		m_wndList.SetCollapsedPaths(Merge20SplitCollapsedPaths(
			pOptions->GetString(OPT_MERGE20_FILELIST_COLLAPSED)));
	}
	if (m_wndSearch.GetSafeHwnd() != nullptr)
	{
		m_wndSearch.SetWindowText(
			pOptions->GetString(OPT_MERGE20_FILELIST_SEARCH).c_str());
	}
	LayoutChildren();
}

/**
 * @brief Lay the search row, the filter tabs, the buttons, the
 * count line, the list and the empty label out.
 *
 * Top to bottom: search row, filter tabs, buttons, count line,
 * then the list fills the rest; only one of the list and the
 * empty label is visible at a time (see Refresh()), and the empty
 * label fills the whole bar when it shows.
 *
 * P4 (slim): in slim mode only the file-name and review columns
 * show (CMerge20FileListCtrl::ApplySlim), and the search /
 * filter / batch rows fold away, leaving the slim toggle, the
 * count line and the list. The slim button stays visible in slim
 * mode: it is the way back out. Hidden rows take no vertical
 * space; without a folder document every row is hidden.
 */
void CMerge20FileListBar::LayoutChildren()
{
	CRect rcClient;
	GetClientRect(&rcClient);
	const bool bHasDoc = m_pDirDoc != nullptr;
	const bool bShowTopRows = bHasDoc && !m_bSlim;
	int y = rcClient.top;
	// U5: the accent underlines are drawn by OnPaint at the bottom
	// 2px of these rows; cleared here and set where each row is
	// laid out below.
	m_rcSearchRow.SetRectEmpty();
	m_rcTabsRow.SetRectEmpty();
	m_rcCountRow.SetRectEmpty();
	if (m_stSearchLabel.GetSafeHwnd() != nullptr)
		m_stSearchLabel.ShowWindow(bShowTopRows ? SW_SHOW : SW_HIDE);
	if (m_wndSearch.GetSafeHwnd() != nullptr)
		m_wndSearch.ShowWindow(bShowTopRows ? SW_SHOW : SW_HIDE);
	if (bShowTopRows)
	{
		CRect rcLabel = rcClient;
		rcLabel.top = y;
		// U5: the label ends 2px above the row bottom, leaving
		// the accent underline strip free (same for the tabs and
		// count rows below).
		rcLabel.bottom = y + Merge20FileListSearchHeight - 2;
		rcLabel.right = rcLabel.left + Merge20FileListSearchLabelWidth;
		m_stSearchLabel.MoveWindow(&rcLabel);
		CRect rcSearch = rcClient;
		rcSearch.top = y + 3;
		rcSearch.bottom = y + Merge20FileListSearchHeight - 3;
		rcSearch.left += Merge20FileListSearchLabelWidth + Merge20FileListButtonGap;
		m_wndSearch.MoveWindow(&rcSearch);
		m_rcSearchRow.SetRect(rcClient.left, y, rcClient.right, y + Merge20FileListSearchHeight);
		y += Merge20FileListSearchHeight;
	}
	if (m_wndFilterTabs.GetSafeHwnd() != nullptr)
		m_wndFilterTabs.ShowWindow(bShowTopRows ? SW_SHOW : SW_HIDE);
	if (bShowTopRows)
	{
		CRect rcTabs = rcClient;
		rcTabs.top = y;
		rcTabs.bottom = y + Merge20FileListTabsHeight - 2; // U5: underline strip
		m_wndFilterTabs.MoveWindow(&rcTabs);
		m_rcTabsRow.SetRect(rcClient.left, y, rcClient.right, y + Merge20FileListTabsHeight);
		y += Merge20FileListTabsHeight;
	}
	// B3: the risk-tab row sits directly under the action tabs
	// (filters together, B4 remainder below the count line). No
	// U5 underline strip of its own: the row is a second tab row,
	// and only the search / tabs / count rows carry one.
	if (m_wndRiskTabs.GetSafeHwnd() != nullptr)
		m_wndRiskTabs.ShowWindow(bShowTopRows ? SW_SHOW : SW_HIDE);
	if (bShowTopRows)
	{
		CRect rcRiskTabs = rcClient;
		rcRiskTabs.top = y;
		rcRiskTabs.bottom = y + Merge20FileListRiskTabsHeight;
		m_wndRiskTabs.MoveWindow(&rcRiskTabs);
		y += Merge20FileListRiskTabsHeight;
	}
	if (m_btnBatch.GetSafeHwnd() != nullptr)
		m_btnBatch.ShowWindow(bShowTopRows ? SW_SHOW : SW_HIDE);
	if (m_btnExpandAll.GetSafeHwnd() != nullptr)
		m_btnExpandAll.ShowWindow(bShowTopRows ? SW_SHOW : SW_HIDE);
	if (m_btnCollapseAll.GetSafeHwnd() != nullptr)
		m_btnCollapseAll.ShowWindow(bShowTopRows ? SW_SHOW : SW_HIDE);
	if (m_btnSlim.GetSafeHwnd() != nullptr)
		m_btnSlim.ShowWindow(bHasDoc ? SW_SHOW : SW_HIDE);
	if (bHasDoc)
	{
		if (m_bSlim)
		{
			// Slim mode: only the toggle in the button row, at the left.
			if (m_btnSlim.GetSafeHwnd() != nullptr)
			{
				CRect rcSlim = rcClient;
				rcSlim.top = y + 3;
				rcSlim.bottom = y + Merge20FileListButtonsHeight - 3;
				rcSlim.right = rcSlim.left + Merge20FileListSlimButtonWidth;
				m_btnSlim.MoveWindow(&rcSlim);
			}
		}
		else
		{
			if (m_btnBatch.GetSafeHwnd() != nullptr)
			{
				CRect rcBatch = rcClient;
				rcBatch.top = y + 3;
				rcBatch.bottom = y + Merge20FileListButtonsHeight - 3;
				rcBatch.right = rcBatch.left + Merge20FileListBatchButtonWidth;
				m_btnBatch.MoveWindow(&rcBatch);
			}
			if (m_btnExpandAll.GetSafeHwnd() != nullptr)
			{
				CRect rcExpand = rcClient;
				rcExpand.top = y + 3;
				rcExpand.bottom = y + Merge20FileListButtonsHeight - 3;
				rcExpand.left += Merge20FileListBatchButtonWidth + Merge20FileListButtonGap;
				rcExpand.right = rcExpand.left + Merge20FileListExpandButtonWidth;
				m_btnExpandAll.MoveWindow(&rcExpand);
			}
			if (m_btnCollapseAll.GetSafeHwnd() != nullptr)
			{
				CRect rcCollapse = rcClient;
				rcCollapse.top = y + 3;
				rcCollapse.bottom = y + Merge20FileListButtonsHeight - 3;
				rcCollapse.left += Merge20FileListBatchButtonWidth + Merge20FileListButtonGap +
					Merge20FileListExpandButtonWidth + Merge20FileListButtonGap;
				rcCollapse.right = rcCollapse.left + Merge20FileListCollapseButtonWidth;
				m_btnCollapseAll.MoveWindow(&rcCollapse);
			}
			if (m_btnSlim.GetSafeHwnd() != nullptr)
			{
				CRect rcSlim = rcClient;
				rcSlim.top = y + 3;
				rcSlim.bottom = y + Merge20FileListButtonsHeight - 3;
				rcSlim.left += Merge20FileListBatchButtonWidth + Merge20FileListButtonGap +
					Merge20FileListExpandButtonWidth + Merge20FileListButtonGap +
					Merge20FileListCollapseButtonWidth + Merge20FileListButtonGap;
				rcSlim.right = rcSlim.left + Merge20FileListSlimButtonWidth;
				m_btnSlim.MoveWindow(&rcSlim);
			}
		}
		y += Merge20FileListButtonsHeight;
	}
	if (m_stCount.GetSafeHwnd() != nullptr)
	{
		CRect rcCount = rcClient;
		rcCount.top = y;
		rcCount.bottom = y + Merge20FileListCountHeight - 2; // U5: underline strip
		m_stCount.MoveWindow(&rcCount);
	}
	if (bHasDoc)
		m_rcCountRow.SetRect(rcClient.left, y, rcClient.right, y + Merge20FileListCountHeight);
	y += Merge20FileListCountHeight;
	// B4: the high-risk remainder line sits directly under the
	// count line and only while N > 0; at 0 it takes no space, so
	// the list starts right under the count line as before.
	if (bHasDoc && m_bHighRiskVisible && m_stHighRisk.GetSafeHwnd() != nullptr)
	{
		CRect rcHighRisk = rcClient;
		rcHighRisk.top = y;
		rcHighRisk.bottom = y + Merge20FileListHighRiskHeight;
		m_stHighRisk.MoveWindow(&rcHighRisk);
		y += Merge20FileListHighRiskHeight;
	}
	if (m_wndList.GetSafeHwnd() != nullptr)
	{
		CRect rcList = rcClient;
		rcList.top = y;
		m_wndList.MoveWindow(&rcList);
	}
	if (m_stEmpty.GetSafeHwnd() != nullptr)
	{
		CRect rcText = rcClient;
		rcText.DeflateRect(8, 8, 8, 8);
		m_stEmpty.MoveWindow(&rcText);
	}
}

/**
 * @brief Stores HWND of frame window (CMergeEditFrame).
 */
void CMerge20FileListBar::SetFrameHwnd(HWND hwndFrame)
{
	m_hwndFrame = hwndFrame;
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CMerge20FileListBar::UpdateResources()
{
	String sCaption = _("File List Panel");
	SetWindowText(sCaption.c_str());
	if (m_stSearchLabel.GetSafeHwnd() != nullptr)
	{
		String sText = _("Search file name or folder:");
		m_stSearchLabel.SetWindowText(sText.c_str());
	}
	if (m_btnBatch.GetSafeHwnd() != nullptr)
	{
		String sText = _("Batch check...");
		m_btnBatch.SetWindowText(sText.c_str());
	}
	if (m_btnExpandAll.GetSafeHwnd() != nullptr)
	{
		String sText = _("Expand all");
		m_btnExpandAll.SetWindowText(sText.c_str());
	}
	if (m_btnCollapseAll.GetSafeHwnd() != nullptr)
	{
		String sText = _("Collapse all");
		m_btnCollapseAll.SetWindowText(sText.c_str());
	}
	if (m_btnSlim.GetSafeHwnd() != nullptr)
	{
		String sText = _("Slim view");
		m_btnSlim.SetWindowText(sText.c_str());
		m_btnSlim.SetCheck(m_bSlim ? BST_CHECKED : BST_UNCHECKED);
	}
	if (m_wndFilterTabs.GetSafeHwnd() != nullptr)
		SetupFilterTabs();
	if (m_wndRiskTabs.GetSafeHwnd() != nullptr)
		SetupRiskTabs();
	if (m_stEmpty.GetSafeHwnd() != nullptr)
	{
		String sText = _("No folder comparison");
		m_stEmpty.SetWindowText(sText.c_str());
	}
	if (m_wndList.GetSafeHwnd() != nullptr)
	{
		// Re-apply the column header texts (same msgids as the
		// folder compare list, so no new string is needed except
		// the provisional old/changed/new column).
		m_wndList.UpdateColumnHeaders();
		// The count line text comes from the S3 msgid through
		// GetCountText(); re-apply it so a language change shows
		// at once instead of waiting for the next Refresh().
		if (m_pDirDoc != nullptr && m_stCount.GetSafeHwnd() != nullptr)
			m_stCount.SetWindowText(m_wndList.GetCountText().c_str());
		// B4: re-apply the remainder text the same way, so a
		// language change shows at once. The line stays hidden
		// at 0; only the shown count is re-applied.
		if (m_bHighRiskVisible && m_stHighRisk.GetSafeHwnd() != nullptr)
			m_stHighRisk.SetWindowText(Merge20FileListHighRiskText(m_nHighRiskShown).c_str());
	}
	// U5: repaint the accent underlines and the owner-drawn tabs.
	Invalidate(FALSE);
	if (m_wndFilterTabs.GetSafeHwnd() != nullptr)
		m_wndFilterTabs.Invalidate(FALSE);
	if (m_wndRiskTabs.GetSafeHwnd() != nullptr)
		m_wndRiskTabs.Invalidate(FALSE);
}

/**
 * @brief (Re-)insert the eight filter-tab labels (P2, P-R10).
 *
 * Tab 0 is "all"; tabs 1-7 are the action categories in
 * ActionCategory enum order, so the tab index maps to the filter
 * value by (index - 1). The seven category labels reuse the S4
 * column value msgids (ColActionCategoryGet), so the tab shows
 * exactly what the folder compare list shows for that category.
 * Only the "all" tab is a new msgid.
 */
void CMerge20FileListBar::SetupFilterTabs()
{
	// Literal _() calls again (see Merge20FileListCtrl::
	// SetupColumns): the .pot generator only scans literal
	// arguments.
	const String labels[8] =
	{
		_("All action categories"),
		_("Merge (missing in new)"),
		_("Review (missing + changed)"),
		_("Review (modified)"),
		_("New additions only"),
		_("Old-only file"),
		_("New-only file"),
		_("Identical"),
	};
	m_wndFilterTabs.DeleteAllItems();
	for (int i = 0; i < 8; ++i)
		m_wndFilterTabs.InsertItem(i, labels[static_cast<size_t>(i)].c_str());
	m_wndFilterTabs.SetCurSel(m_nSelectedTab);
}

/**
 * @brief (Re-)insert the four risk-tab labels (B3).
 *
 * Tab 0 is "All risks"; tabs 1-3 are High / Medium / None in
 * patrol order (High first). The filter value rides in the item
 * data (TCIF_PARAM), so the tab order is free. "High" /
 * "Medium" / "None" reuse the S4 risk value msgids (ColRiskGet),
 * so the tab shows exactly what the Risk column shows for that
 * level. Only "All risks" is a new msgid.
 */
void CMerge20FileListBar::SetupRiskTabs()
{
	// Literal _() calls again (see Merge20FileListCtrl::
	// SetupColumns): the .pot generator only scans literal
	// arguments.
	const String labels[4] =
	{
		_("All risks"),
		_("High"),
		_("Medium"),
		_("None"),
	};
	m_wndRiskTabs.DeleteAllItems();
	for (int i = 0; i < 4; ++i)
	{
		TCITEM item = {};
		item.mask = TCIF_TEXT | TCIF_PARAM;
		item.pszText = const_cast<LPTSTR>(labels[static_cast<size_t>(i)].c_str());
		item.lParam = Merge20FileListRiskTabValues[i];
		m_wndRiskTabs.InsertItem(i, &item);
	}
	m_wndRiskTabs.SetCurSel(m_nSelectedRiskTab);
}

/**
 * @brief Risk tab changed (B3): filter the panel by the selected
 * risk level (tab 0 = all risks).
 */
void CMerge20FileListBar::OnRiskTabChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	*pResult = 0;
	const int sel = m_wndRiskTabs.GetCurSel();
	if (sel < 0 || sel >= 4)
		return;
	m_nSelectedRiskTab = sel;
	const int riskFilter = Merge20FileListRiskTabValues[static_cast<size_t>(sel)];
	m_wndList.SetRiskFilter(riskFilter);
	// P4: the selected tab is saved at once (a discrete user
	// action; the D11 dual-save precedent). SavePanelState saves
	// it again at SavePosition.
	COptionsMgr* pOptions = GetOptionsMgr();
	if (pOptions != nullptr)
		pOptions->SaveOption(OPT_MERGE20_FILELIST_RISK_FILTER, riskFilter);
}

/**
 * @brief Search box changed (P2, P-R4): filter the panel on every
 * keystroke (a 1,000-row rebuild is cheap, so no debounce).
 *
 * P4: the string itself is saved by SavePanelState (frame
 * SavePosition), not here: saving on every keystroke would write
 * the registry on each typed character.
 */
void CMerge20FileListBar::OnSearchChanged()
{
	if (m_wndSearch.GetSafeHwnd() == nullptr || m_wndList.GetSafeHwnd() == nullptr)
		return;
	CString text;
	m_wndSearch.GetWindowText(text);
	m_wndList.SetSearchText(static_cast<LPCTSTR>(text));
}

/**
 * @brief Filter tab changed (P2, P-R10): filter the panel by the
 * selected action category (tab 0 = all categories).
 */
void CMerge20FileListBar::OnFilterTabChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	*pResult = 0;
	const int sel = m_wndFilterTabs.GetCurSel();
	if (sel < 0)
		return;
	m_nSelectedTab = sel;
	m_wndList.SetActionFilter(sel == 0 ? -1 : sel - 1);
	// P4: the selected tab is saved at once (a discrete user
	// action; the D11 dual-save precedent). SavePanelState saves
	// it again at SavePosition.
	COptionsMgr* pOptions = GetOptionsMgr();
	if (pOptions != nullptr)
		pOptions->SaveOption(OPT_MERGE20_FILELIST_TAB, sel);
}

/**
 * @brief Open the S3 batch-check dialog for the panel's displayed
 * rows (P2, P-R3).
 *
 * The dialog itself is reused unchanged (Merge20BatchCheckDlg):
 * its counts come from the panel's displayed rows
 * (CountByReviewState), so the counts match what the panel shows.
 * Applying resolves the displayed rows to DIFFITEMs through a
 * relPath map built once, and is wrapped in the P1 notification
 * suppression, so one batch check posts one notification instead
 * of one per file. "Clear all" covers the whole comparison, as in
 * S3 (the HTML popup's batchChk(false) iterates all files).
 * The panel refreshes itself directly afterwards (it does not
 * wait for the notification).
 */
void CMerge20FileListBar::OnMerge20BatchCheck()
{
	if (m_pDirDoc == nullptr)
		return;
	int counts[5] = {};
	m_wndList.CountByReviewState(counts);
	Merge20BatchCheckDlg dlg(this, counts);
	if (dlg.DoModal() != IDOK)
		return;
	CDiffContext& ctxt = m_pDirDoc->GetDiffContext();
	std::map<String, const DIFFITEM*> byRelPath;
	Merge20FileListBuildRelPathMap(ctxt, byRelPath);
	ctxt.SuspendMerge20FileListNotify();
	switch (dlg.GetAction())
	{
	case Merge20BatchCheckDlg::Action::Selected:
	{
		for (int i = 0; i < m_wndList.GetDisplayedRowCount(); ++i)
		{
			const CMerge20FileListCtrl::Row* pRow = m_wndList.GetDisplayedRow(i);
			if (pRow == nullptr || pRow->isDirectory)
				continue;
			if (!dlg.IsStateSelected(pRow->reviewState))
				continue;
			const std::map<String, const DIFFITEM*>::const_iterator it = byRelPath.find(pRow->relPath);
			if (it != byRelPath.end())
				m_pDirDoc->SetMerge20IncludeInOutput(*it->second, true);
		}
		break;
	}
	case Merge20BatchCheckDlg::Action::CheckAllDisplayed:
	{
		for (int i = 0; i < m_wndList.GetDisplayedRowCount(); ++i)
		{
			const CMerge20FileListCtrl::Row* pRow = m_wndList.GetDisplayedRow(i);
			if (pRow == nullptr || pRow->isDirectory)
				continue;
			const std::map<String, const DIFFITEM*>::const_iterator it = byRelPath.find(pRow->relPath);
			if (it != byRelPath.end())
				m_pDirDoc->SetMerge20IncludeInOutput(*it->second, true);
		}
		break;
	}
	case Merge20BatchCheckDlg::Action::ClearAll:
		for (DIFFITEM* p = ctxt.GetFirstDiffPosition(); p != nullptr;)
		{
			const DIFFITEM& di = ctxt.GetNextDiffPosition(p);
			if (!di.diffcode.isDirectory())
				m_pDirDoc->SetMerge20IncludeInOutput(di, false);
		}
		break;
	case Merge20BatchCheckDlg::Action::None:
		break;
	}
	ctxt.ResumeMerge20FileListNotify();
	Refresh();
	// P3 (P-R8): the folder list and its status-bar count follow the
	// batch change at once (one invalidate + one count update for
	// the whole batch, the S3 folder-side rule). This closes the P2
	// known limitation for the batch path.
	m_pDirDoc->Merge20UpdateDirViewStatus();
}

/**
 * @brief Unfold every directory row (P2, P-R5).
 */
void CMerge20FileListBar::OnExpandAll()
{
	m_wndList.ExpandAll();
}

/**
 * @brief Fold every directory row that has children (P2, P-R5).
 */
void CMerge20FileListBar::OnCollapseAll()
{
	m_wndList.CollapseAll();
}

/**
 * @brief Slim toggle button (Panel P4, design section 12.4).
 *
 * BS_AUTOCHECKBOX flips its own check before BN_CLICKED, so the
 * handler reads the check and applies it.
 */
void CMerge20FileListBar::OnSlimToggle()
{
	if (m_btnSlim.GetSafeHwnd() == nullptr)
		return;
	SetSlim(m_btnSlim.GetCheck() != 0);
}

/**
 * @brief Set the slim display and save the flag (Panel P4).
 */
void CMerge20FileListBar::SetSlim(bool slim)
{
	ApplySlimState(slim);
	COptionsMgr* pOptions = GetOptionsMgr();
	if (pOptions != nullptr)
		pOptions->SaveOption(OPT_MERGE20_FILELIST_SLIM, m_bSlim);
}

/**
 * @brief Apply the slim display without saving (Panel P4).
 *
 * The button check, the list columns (only file name and review
 * state stay) and the folded-away top rows. In slim mode the
 * output column is hidden, so the output flag can only be
 * toggled through the review state (DONE turns it back on, the
 * S2 linkage) or by leaving slim mode.
 */
void CMerge20FileListBar::ApplySlimState(bool slim)
{
	m_bSlim = slim;
	if (m_btnSlim.GetSafeHwnd() != nullptr)
		m_btnSlim.SetCheck(slim ? BST_CHECKED : BST_UNCHECKED);
	if (m_wndList.GetSafeHwnd() != nullptr)
		m_wndList.ApplySlim(slim);
	LayoutChildren();
}

/**
 * @brief Right-click on a panel row, reported by the list control
 * (P2): show the row context menu.
 */
LRESULT CMerge20FileListBar::OnRowRClick(WPARAM wParam, LPARAM /*lParam*/)
{
	ShowRowContextMenu(static_cast<int>(wParam));
	return 0;
}

/**
 * @brief Show the context menu for a displayed directory row
 * (P2, P-R5).
 *
 * Fold/unfold for that folder and "check/uncheck everything
 * below the folder" (the S3 directory operation, same msgids and
 * command IDs as the folder compare list: the bar handles them
 * here because the menu belongs to this window). File rows get
 * no menu. The menu is run with TPM_RETURNCMD and the choice is
 * handled here, so no new command IDs are needed: the
 * fold/unfold items reuse the expand-all / collapse-all control
 * IDs (1815/1816) as menu IDs, and the directory operations reuse
 * the existing command IDs (38932/38933).
 */
void CMerge20FileListBar::ShowRowContextMenu(int row)
{
	const CMerge20FileListCtrl::Row* pRow = m_wndList.GetDisplayedRow(row);
	if (pRow == nullptr || m_pDirDoc == nullptr)
		return;
	// P3 (P-R8): file rows get the review-state menu; directory rows
	// keep the P2 menu below.
	if (!pRow->isDirectory)
	{
		ShowFileRowContextMenu(row);
		return;
	}
	const String relPath = pRow->relPath;
	const bool collapsed = m_wndList.IsRowCollapsed(row);
	const bool hasChildren = pRow->hasChildren;
	CMenu menu;
	menu.CreatePopupMenu();
	if (hasChildren)
	{
		String sExpand = _("Expand folder");
		menu.AppendMenu(MF_STRING | (collapsed ? MF_ENABLED : MF_GRAYED),
			IDC_MERGE20_FILELIST_EXPAND_ALL, sExpand.c_str());
		String sCollapse = _("Collapse folder");
		menu.AppendMenu(MF_STRING | (collapsed ? MF_GRAYED : MF_ENABLED),
			IDC_MERGE20_FILELIST_COLLAPSE_ALL, sCollapse.c_str());
	}
	String sCheckAll = _("Check all files under folder");
	menu.AppendMenu(MF_STRING, ID_MERGE20_DIR_CHECK_ALL, sCheckAll.c_str());
	String sUncheckAll = _("Uncheck all files under folder");
	menu.AppendMenu(MF_STRING, ID_MERGE20_DIR_UNCHECK_ALL, sUncheckAll.c_str());
	CPoint pt;
	::GetCursorPos(&pt);
	const UINT cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
		pt.x, pt.y, this);
	if (cmd == 0)
		return;
	if (cmd == IDC_MERGE20_FILELIST_EXPAND_ALL)
	{
		m_wndList.SetCollapsed(relPath, false);
		return;
	}
	if (cmd == IDC_MERGE20_FILELIST_COLLAPSE_ALL)
	{
		m_wndList.SetCollapsed(relPath, true);
		return;
	}
	CDiffContext& ctxt = m_pDirDoc->GetDiffContext();
	std::map<String, const DIFFITEM*> byRelPath;
	Merge20FileListBuildRelPathMap(ctxt, byRelPath);
	const std::map<String, const DIFFITEM*>::const_iterator it = byRelPath.find(relPath);
	if (it == byRelPath.end())
		return;
	ctxt.SuspendMerge20FileListNotify();
	if (cmd == ID_MERGE20_DIR_CHECK_ALL)
		SetOutputForDirectory(*it->second, true);
	else if (cmd == ID_MERGE20_DIR_UNCHECK_ALL)
		SetOutputForDirectory(*it->second, false);
	ctxt.ResumeMerge20FileListNotify();
	Refresh();
	// P3 (P-R8): the folder list and its status-bar count follow the
	// directory change at once (one invalidate + one count update
	// for the whole subtree, the S3 folder-side rule). This closes
	// the P2 known limitation for the directory path.
	m_pDirDoc->Merge20UpdateDirViewStatus();
}

/**
 * @brief Show the context menu for a displayed file row (P3, P-R8).
 *
 * The five review states (checked = current state) and the
 * output-flag toggle (checked = included in output). The labels
 * reuse the S1 review-state msgids, so the menu shows exactly what
 * the folder compare list shows for each state; only the toggle is
 * a new msgid. The menu is run with TPM_RETURNCMD and the choice
 * is handled here, so no ON_COMMAND entries are needed: the review
 * items reuse the dedicated-bar review command IDs (38945-38949,
 * state = ID - ID_MERGE20_DED_REVIEW_TODO), and the toggle uses
 * ID_MERGE20_FILELIST_TOGGLE_OUTPUT (38967).
 */
void CMerge20FileListBar::ShowFileRowContextMenu(int row)
{
	const CMerge20FileListCtrl::Row* pRow = m_wndList.GetDisplayedRow(row);
	if (pRow == nullptr || pRow->isDirectory || m_pDirDoc == nullptr)
		return;
	// Literal _() calls (see SetupFilterTabs): the .pot generator
	// only scans literal arguments.
	const String stateLabels[5] =
	{
		_("Not reviewed"),
		_("Done"),
		_("On hold"),
		_("Needs manual work"),
		_("Keep as is"),
	};
	CMenu menu;
	menu.CreatePopupMenu();
	const int curState = static_cast<int>(pRow->reviewState);
	for (int i = 0; i < 5; ++i)
	{
		menu.AppendMenu(MF_STRING | (i == curState ? MF_CHECKED : 0),
			ID_MERGE20_DED_REVIEW_TODO + i, stateLabels[i].c_str());
	}
	menu.AppendMenu(MF_SEPARATOR);
	String sToggle = _("Toggle output");
	menu.AppendMenu(MF_STRING | (pRow->includeInOutput ? MF_CHECKED : 0),
		ID_MERGE20_FILELIST_TOGGLE_OUTPUT, sToggle.c_str());
	CPoint pt;
	::GetCursorPos(&pt);
	const UINT cmd = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
		pt.x, pt.y, this);
	if (cmd == 0)
		return;
	if (cmd == ID_MERGE20_FILELIST_TOGGLE_OUTPUT)
	{
		ToggleRowOutput(row);
		return;
	}
	if (cmd >= ID_MERGE20_DED_REVIEW_TODO && cmd <= ID_MERGE20_DED_REVIEW_KEEP)
		ApplyRowReview(row, static_cast<ReviewState>(cmd - ID_MERGE20_DED_REVIEW_TODO));
}

/**
 * @brief Folder item for a displayed row (P3), or nullptr for a
 * directory row or a row whose item is gone (rescan). The pointer
 * is into the folder context's item list and is valid only for the
 * synchronous call that uses it: a rescan rebuilds the items, and
 * no pointer is kept past the call.
 */
const DIFFITEM* CMerge20FileListBar::FindRowItem(int row) const
{
	const CMerge20FileListCtrl::Row* pRow = m_wndList.GetDisplayedRow(row);
	if (pRow == nullptr || pRow->isDirectory || m_pDirDoc == nullptr)
		return nullptr;
	CDiffContext& ctxt = m_pDirDoc->GetDiffContext();
	std::map<String, const DIFFITEM*> byRelPath;
	Merge20FileListBuildRelPathMap(ctxt, byRelPath);
	const std::map<String, const DIFFITEM*>::const_iterator it = byRelPath.find(pRow->relPath);
	if (it == byRelPath.end())
		return nullptr;
	return it->second;
}

/**
 * @brief Apply a review state to one displayed file row (P3, P-R8).
 *
 * The S2 output-flag linkage (DONE turns the flag back ON, HOLD /
 * KEEP turn it OFF) is applied by CDirDoc::ApplyMerge20ReviewState,
 * the same rule the folder list's 1-5 keys use. The folder row is
 * repainted and its status-bar count updated at once (P3), and the
 * panel refreshes itself directly afterwards (it does not wait for
 * the notification).
 */
void CMerge20FileListBar::ApplyRowReview(int row, ReviewState state)
{
	const DIFFITEM* pItem = FindRowItem(row);
	if (pItem == nullptr)
		return;
	m_pDirDoc->ApplyMerge20ReviewState(*pItem, state);
	m_pDirDoc->Merge20UpdateDirViewItem(*pItem);
	Refresh();
}

/**
 * @brief Cycle the review state of one displayed file row (P3, P-R8):
 * TODO -> DONE -> HOLD -> SPECIAL -> KEEP -> TODO, the same order
 * as Space in the folder compare list (S2).
 */
void CMerge20FileListBar::CycleRowReview(int row)
{
	const CMerge20FileListCtrl::Row* pRow = m_wndList.GetDisplayedRow(row);
	if (pRow == nullptr || pRow->isDirectory)
		return;
	ApplyRowReview(row, static_cast<ReviewState>((static_cast<int>(pRow->reviewState) + 1) % 5));
}

/**
 * @brief Toggle the output flag of one displayed file row (P3, P-R8),
 * with the folder-view sync and a panel refresh. The flag is read
 * from the cached row (the same value the panel shows).
 */
void CMerge20FileListBar::ToggleRowOutput(int row)
{
	const DIFFITEM* pItem = FindRowItem(row);
	if (pItem == nullptr)
		return;
	const CMerge20FileListCtrl::Row* pRow = m_wndList.GetDisplayedRow(row);
	if (pRow == nullptr)
		return;
	m_pDirDoc->SetMerge20IncludeInOutput(*pItem, !pRow->includeInOutput);
	m_pDirDoc->Merge20UpdateDirViewItem(*pItem);
	Refresh();
}

/**
 * @brief Open the file of one displayed file row (P3, P-R7).
 *
 * The relative path is stashed and MSG_MERGE20_FILELIST_OPEN_FILE
 * is posted to the frame: the frame resolves the item and moves to
 * it (closing this frame, or activating the frame that already
 * shows the file). Posting, not calling, keeps this bar alive:
 * opening the file closes this frame, so nothing of this bar may
 * be touched after the move. The file already shown is a no-op
 * (the panel already selects it). Two rapid clicks stash the
 * second path before the first message is pumped, so the last
 * clicked file wins.
 */
void CMerge20FileListBar::OpenRowFile(int row)
{
	const CMerge20FileListCtrl::Row* pRow = m_wndList.GetDisplayedRow(row);
	if (pRow == nullptr || pRow->isDirectory || m_hwndFrame == nullptr)
		return;
	if (pRow->relPath == m_sCurrentRelPath)
		return;
	m_sPendingOpenRelPath = pRow->relPath;
	::PostMessage(m_hwndFrame, MSG_MERGE20_FILELIST_OPEN_FILE, 0, 0);
}

/**
 * @brief Single click on a panel row, reported by the list control
 * (P3, P-R7/P-R8): dispatch by column. The output column toggles
 * the flag (the S2 folder-list behaviour), the review column cycles
 * the state (the Space behaviour), any other column opens the file.
 * Directory rows do nothing on a single click (folding stays on
 * double-click, P2).
 */
LRESULT CMerge20FileListBar::OnRowClick(WPARAM wParam, LPARAM lParam)
{
	const int row = static_cast<int>(wParam);
	const int subItem = static_cast<int>(lParam);
	const CMerge20FileListCtrl::Row* pRow = m_wndList.GetDisplayedRow(row);
	if (pRow == nullptr || pRow->isDirectory || m_pDirDoc == nullptr)
		return 0;
	if (subItem == CMerge20FileListCtrl::COL_OUTPUT)
	{
		ToggleRowOutput(row);
		return 0;
	}
	if (subItem == CMerge20FileListCtrl::COL_REVIEW)
	{
		CycleRowReview(row);
		return 0;
	}
	OpenRowFile(row);
	return 0;
}

/**
 * @brief Handle a bare 1-5 / Space key while the panel list has the
 * focus (P3, P-R8). Called from the frame's PreTranslateMessage
 * (the list itself cannot swallow keys). Keys 1-5 set the review
 * state of the selected row (the S2 folder-list behaviour, with
 * the output-flag linkage); Space cycles the state. Bare keys
 * only, so accelerators with Ctrl/Alt/Shift keep working, and
 * only when the focus is exactly the list (typing in the search
 * box must keep working). Returns true when the key was handled.
 */
bool CMerge20FileListBar::HandleListKey(UINT nVKey)
{
	if (m_wndList.GetSafeHwnd() == nullptr || ::GetFocus() != m_wndList.GetSafeHwnd())
		return false;
	if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 ||
		(GetKeyState(VK_MENU) & 0x8000) != 0 ||
		(GetKeyState(VK_SHIFT) & 0x8000) != 0)
	{
		return false;
	}
	const int row = m_wndList.GetSelectedRow();
	if (row < 0)
		return false;
	if (nVKey >= '1' && nVKey <= '5')
	{
		ApplyRowReview(row, static_cast<ReviewState>(nVKey - '1'));
		return true;
	}
	if (nVKey == VK_SPACE)
	{
		// Merge20 (best-workflow B14): Space cycles blindly, so
		// the step that declares a high-risk file done (TODO ->
		// DONE) needs one confirmation; a cancel changes nothing.
		// Directory rows hold no file judgment, so they are not
		// guarded. Keys 1-5 stay unguarded (see DirView.cpp).
		const CMerge20FileListCtrl::Row* pRow = m_wndList.GetDisplayedRow(row);
		if (pRow != nullptr && !pRow->isDirectory && m_pDirDoc != nullptr)
		{
			const ReviewState nextState = static_cast<ReviewState>((static_cast<int>(pRow->reviewState) + 1) % 5);
			if (!Merge20ConfirmHighRiskFileReview(this, pRow->relPath,
				m_pDirDoc->GetDiffContext().GetMerge20FileStates().GetState(pRow->relPath),
				pRow->reviewState, nextState))
			{
				return true;
			}
		}
		CycleRowReview(row);
		return true;
	}
	return false;
}

/**
 * @brief Set the output flag of every file below one directory
 * row, recursively (P2, P-R5).
 *
 * The S3 DIFFITEM tree walk (CDirView::Merge20SetOutputForDirectory):
 * the tree is walked, not the visible list, so files in collapsed
 * subfolders are included too. Directory rows themselves have no
 * flag.
 */
void CMerge20FileListBar::SetOutputForDirectory(const DIFFITEM& dir, bool include)
{
	for (DIFFITEM* pChild = dir.GetFirstChild(); pChild != nullptr; pChild = pChild->GetFwdSiblingLink())
	{
		if (pChild->diffcode.isDirectory())
			SetOutputForDirectory(*pChild, include);
		else
			m_pDirDoc->SetMerge20IncludeInOutput(*pChild, include);
	}
}
