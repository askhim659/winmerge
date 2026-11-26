/** 
 * @file  DiffViewBar.cpp
 *
 * @brief Implementation file for CDiffViewBar
 *
 */

#include "stdafx.h"
#include "DiffViewBar.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CDiffViewBar, TViewBarBase);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CDiffViewBar::CDiffViewBar()
: m_hwndFrame(nullptr)
{
}


CDiffViewBar::~CDiffViewBar()
{
}

/**
 * @brief Merge20 (Phase 4 D11, design section 4.1(a)): restore the
 * saved focus-dock height after the base LoadState reads the
 * persisted size. The base LoadState reads the persisted sizeHorzCY
 * (and the frame calls LoadState after OnCreate, so a constructor
 * value would be overwritten); the saved height wins over it when
 * present, clamped to 60-2000px. A value of 0 (never saved, or an
 * old profile) leaves the persisted size alone.
 */
void CDiffViewBar::LoadState(LPCTSTR lpszProfileName)
{
	TViewBarBase::LoadState(lpszProfileName);
	const int nHeight = GetOptionsMgr()->GetInt(OPT_MERGE20_FOCUS_DOCK_HEIGHT);
	if (nHeight > 0)
	{
		const int nClamped = (std::max)(60, (std::min)(nHeight, 2000));
		m_szHorz.cy = nClamped;
		m_szFloat.cy = nClamped;
	}
}


BEGIN_MESSAGE_MAP(CDiffViewBar, TViewBarBase)
	//{{AFX_MSG_MAP(CRegBar)
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_WINDOWPOSCHANGED()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDiffViewBar message handlers

/**
* @brief Just create ourself
*
* @note The control are created in the parent frame CMergeEditFrame
*
*/
BOOL CDiffViewBar::Create(
	CWnd* pParentWnd,
	LPCTSTR lpszWindowName /*= nullptr*/,
	DWORD dwStyle /*= WS_CHILD | WS_VISIBLE | CBRS_TOP*/,
	UINT nID /*= AFX_IDW_PANE_FIRST*/)
{
	return TViewBarBase::Create(
		lpszWindowName,
		pParentWnd,
		nID,
		dwStyle);
}


///	Create the frame window associated with the view bar. */
int CDiffViewBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (TViewBarBase::OnCreate(lpCreateStruct) == -1)
		return -1;

	SetSCBStyle(SCBS_EDGETOP | SCBS_EDGEBOTTOM | SCBS_SIZECHILD);
	
	return 0;
}

/**
* @note The window must always be docked after movement
* there are too much troubles if we get reparented to some minidockbar 
*
*/
void CDiffViewBar::OnLButtonDown(UINT nFlags, CPoint point)
{
	TViewBarBase::OnLButtonDown(nFlags, point);
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
void CDiffViewBar::OnWindowPosChanged(WINDOWPOS* lpwndpos)
{
	TViewBarBase::OnWindowPosChanged(lpwndpos);

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
void CDiffViewBar::SetFrameHwnd(HWND hwndFrame)
{
	m_hwndFrame = hwndFrame;
}
/**
 * @brief Update any resources necessary after a GUI language change
 */
void CDiffViewBar::UpdateResources()
{
	String sCaption = _("Diff Pane");
	SetWindowText(sCaption.c_str());
}
