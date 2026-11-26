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
 */

#include "StdAfx.h"
#include "Merge20/Merge20FileListBar.h"
#include "DirDoc.h"

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
} // namespace

BEGIN_MESSAGE_MAP(CMerge20FileListBar, CSizingControlBarCF)
	//{{AFX_MSG_MAP(CMerge20FileListBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_WINDOWPOSCHANGED()
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
	m_stCount.ShowWindow(bHasDirDoc ? SW_SHOW : SW_HIDE);
	m_wndList.ShowWindow(bHasDirDoc ? SW_SHOW : SW_HIDE);
	m_stEmpty.ShowWindow(bHasDirDoc ? SW_HIDE : SW_SHOW);
	m_wndList.EnableWindow(bHasDirDoc);
	if (bHasDirDoc)
		m_wndList.SetDiffContext(&m_pDirDoc->GetDiffContext());
	else
		m_wndList.SetDiffContext(nullptr);
	m_wndList.Refresh();
	if (bHasDirDoc)
		m_stCount.SetWindowText(m_wndList.GetCountText().c_str());
	LayoutChildren();
}

/**
 * @brief Lay the count line, the list and the empty label out.
 *
 * The count line takes a strip at the top and the list fills the
 * rest; only one of the list and the empty label is visible at a
 * time (see Refresh()), and the empty label fills the whole bar
 * when it shows.
 */
void CMerge20FileListBar::LayoutChildren()
{
	CRect rcClient;
	GetClientRect(&rcClient);
	if (m_stCount.GetSafeHwnd() != nullptr)
	{
		CRect rcCount = rcClient;
		rcCount.bottom = rcCount.top + Merge20FileListCountHeight;
		m_stCount.MoveWindow(&rcCount);
	}
	if (m_wndList.GetSafeHwnd() != nullptr)
	{
		CRect rcList = rcClient;
		rcList.top += Merge20FileListCountHeight;
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
	}
}
