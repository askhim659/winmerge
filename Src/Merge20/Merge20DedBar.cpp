// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20DedBar.cpp
 *
 * @brief Implementation file for CMerge20DedBar
 *
 * Merge20 (Phase 4 D1): the dedicated compare mode bar, following the
 * CDiffViewBar precedent. D1 only creates the bar and wires its
 * show/hide to CMergeEditFrame::SetDedicatedMode; the bar has no child
 * controls yet (D2 adds the buttons).
 */

#include "StdAfx.h"
#include "Merge20/Merge20DedBar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20DedBar, CSizingControlBarCF);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMerge20DedBar::CMerge20DedBar()
: m_hwndFrame(nullptr)
{
}


CMerge20DedBar::~CMerge20DedBar()
{
}


BEGIN_MESSAGE_MAP(CMerge20DedBar, CSizingControlBarCF)
	//{{AFX_MSG_MAP(CMerge20DedBar)
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_WINDOWPOSCHANGED()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CMerge20DedBar message handlers

/**
* @brief Just create ourself
*
* @note The controls are created in the parent frame CMergeEditFrame
* (D2 adds them; D1 has none yet).
*
*/
BOOL CMerge20DedBar::Create(
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


///	Create the frame window associated with the view bar. */
int CMerge20DedBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CSizingControlBarCF::OnCreate(lpCreateStruct) == -1)
		return -1;

	SetSCBStyle(SCBS_EDGETOP | SCBS_EDGEBOTTOM | SCBS_SIZECHILD);

	return 0;
}

/**
* @note The window must always be docked after movement
* there are too much troubles if we get reparented to some minidockbar
*
*/
void CMerge20DedBar::OnLButtonDown(UINT nFlags, CPoint point)
{
	CSizingControlBarCF::OnLButtonDown(nFlags, point);
	if (m_pDockBar != nullptr)
	{
		if (!IsHorzDocked())
			m_pDockContext->ToggleDocking();
	}
}

/**
 * @brief Informs parent frame (CMergeEditFrame) when bar is closed.
 *
 * After bar is closed parent frame saves bar states.
 */
void CMerge20DedBar::OnWindowPosChanged(WINDOWPOS* lpwndpos)
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
 * @brief Stores HWND of frame window (CMergeEditFrame).
 */
void CMerge20DedBar::SetFrameHwnd(HWND hwndFrame)
{
	m_hwndFrame = hwndFrame;
}
/**
 * @brief Update any resources necessary after a GUI language change
 */
void CMerge20DedBar::UpdateResources()
{
	String sCaption = _("Dedicated Compare Mode");
	SetWindowText(sCaption.c_str());
}
