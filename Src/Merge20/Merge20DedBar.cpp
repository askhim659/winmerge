// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20DedBar.cpp
 *
 * @brief Implementation file for CMerge20DedBar
 *
 * Merge20 (Phase 4 D1/D2): the dedicated compare mode bar, following
 * the CDiffViewBar precedent. D2 adds the buttons (Prev/Next file,
 * five review-state buttons, pin, unreviewed-only and sniper toggles,
 * zoom display, exit) in a single row. The bar owns no state: clicks
 * are forwarded as WM_COMMAND (ID_MERGE20_DED_*) to the parent frame,
 * which applies them and refreshes the button states through the
 * setters. Toggle buttons are BS_PUSHLIKE checkboxes whose check is
 * set by the frame (not auto-toggled), so the shown state is always
 * the frame's state.
 */

#include "StdAfx.h"
#include "Merge20/Merge20DedBar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20DedBar, CSizingControlBarCF);

namespace
{
	/** @brief Button height and vertical position in the bar. */
	constexpr int Merge20DedBarButtonHeight = 24;
	constexpr int Merge20DedBarButtonTop = 4;
	/** @brief Left margin and gap between buttons. */
	constexpr int Merge20DedBarMargin = 8;
	constexpr int Merge20DedBarGap = 4;

	/** @brief Fixed button widths, in bar order. Wide enough for the
	 * English and the Japanese labels (see Japanese.po). */
	constexpr int Merge20DedBarWidths[] =
	{
		60, // Prev
		60, // Next
		72, // TODO
		72, // DONE
		72, // HOLD
		72, // SPECIAL
		72, // KEEP
		60, // Pin
		110, // Unreviewed only
		84, // Sniper
		76, // Zoom
		150, // Exit
	};

	/** @brief Control IDs of the buttons, in bar order. */
	constexpr UINT Merge20DedBarControlIDs[] =
	{
		IDC_MERGE20_DED_PREV_FILE,
		IDC_MERGE20_DED_NEXT_FILE,
		IDC_MERGE20_DED_REVIEW_TODO,
		IDC_MERGE20_DED_REVIEW_DONE,
		IDC_MERGE20_DED_REVIEW_HOLD,
		IDC_MERGE20_DED_REVIEW_SPECIAL,
		IDC_MERGE20_DED_REVIEW_KEEP,
		IDC_MERGE20_DED_PIN,
		IDC_MERGE20_DED_TODO_ONLY,
		IDC_MERGE20_DED_SNIPER,
		IDC_MERGE20_DED_ZOOM,
		IDC_MERGE20_DED_EXIT,
	};

	/** @brief Frame commands the buttons forward, in bar order. */
	constexpr UINT Merge20DedBarCommands[] =
	{
		ID_MERGE20_DED_PREV_FILE,
		ID_MERGE20_DED_NEXT_FILE,
		ID_MERGE20_DED_REVIEW_TODO,
		ID_MERGE20_DED_REVIEW_DONE,
		ID_MERGE20_DED_REVIEW_HOLD,
		ID_MERGE20_DED_REVIEW_SPECIAL,
		ID_MERGE20_DED_REVIEW_KEEP,
		ID_MERGE20_DED_PIN,
		ID_MERGE20_DED_TODO_ONLY,
		ID_MERGE20_DED_SNIPER,
		ID_MERGE20_DED_ZOOM_RESET,
		ID_MERGE20_DED_EXIT,
	};
} // namespace

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMerge20DedBar::CMerge20DedBar()
: m_hwndFrame(nullptr)
, m_reviewState(ReviewState::TODO)
, m_nZoom(0) // 0 is never a real zoom; the first SetZoomText sets the text
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
	ON_WM_SIZE()
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MERGE20_DED_EXIT, IDC_MERGE20_DED_ZOOM, OnButtonClicked)
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

	// One row of 24px buttons: fix the bar height to 32px. (A saved
	// height from LoadState still wins afterwards; the user can drag
	// the bar taller, LayoutButtons keeps the row at the top.)
	m_szHorz = CSize(120, 32);
	m_szMinHorz = CSize(33, 32);

	CFont* pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	const DWORD dwPushStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
	const DWORD dwToggleStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHLIKE | BS_CHECKBOX;
	CRect rc(0, 0, 0, 0);
	CButton* pButtons[] =
	{
		&m_btnPrev, &m_btnNext,
		&m_btnReview[0], &m_btnReview[1], &m_btnReview[2], &m_btnReview[3], &m_btnReview[4],
		&m_btnPin, &m_btnTodoOnly, &m_btnSniper, &m_btnZoom, &m_btnExit,
	};
	for (size_t i = 0; i < std::size(pButtons); ++i)
	{
		// Toggle buttons are the five review states, pin, unreviewed-only
		// and sniper (indexes 2-9); Prev/Next, the zoom display and exit
		// (indexes 0, 1, 10, 11) are plain push buttons.
		const DWORD dwStyle = (i >= 2 && i <= 9) ? dwToggleStyle : dwPushStyle;
		if (!pButtons[i]->Create(_T(""), dwStyle, rc, this, Merge20DedBarControlIDs[i]))
		{
			TRACE0("Failed to create Merge20DedBar button\n");
			return -1;
		}
		pButtons[i]->SetFont(pFont);
	}
	UpdateResources();
	// Initial button state: TODO is checked (the first SetReviewState
	// with TODO would be a no-op, as m_reviewState starts as TODO) and
	// the zoom shows 75% until the frame's idle update refreshes it.
	m_btnReview[0].SetCheck(BST_CHECKED);
	SetZoomText(750);
	LayoutButtons();

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

void CMerge20DedBar::OnSize(UINT nType, int cx, int cy)
{
	CSizingControlBarCF::OnSize(nType, cx, cy);
	LayoutButtons();
}

/**
 * @brief Lay the buttons out in one row, left aligned, in spec order.
 * Buttons past the right edge are clipped (the bar is full-width when
 * docked at the top; the exit button is last per spec section 5.2,
 * and the View menu still exits dedicated mode).
 */
void CMerge20DedBar::LayoutButtons()
{
	if (m_btnPrev.GetSafeHwnd() == nullptr)
		return;
	CButton* pButtons[] =
	{
		&m_btnPrev, &m_btnNext,
		&m_btnReview[0], &m_btnReview[1], &m_btnReview[2], &m_btnReview[3], &m_btnReview[4],
		&m_btnPin, &m_btnTodoOnly, &m_btnSniper, &m_btnZoom, &m_btnExit,
	};
	int x = Merge20DedBarMargin;
	for (size_t i = 0; i < std::size(pButtons); ++i)
	{
		pButtons[i]->SetWindowPos(nullptr, x, Merge20DedBarButtonTop,
			Merge20DedBarWidths[i], Merge20DedBarButtonHeight,
			SWP_NOZORDER | SWP_NOACTIVATE);
		x += Merge20DedBarWidths[i] + Merge20DedBarGap;
	}
}

/**
 * @brief Forward a button click to the parent frame as its command.
 * The frame applies the action (and, for Prev/Next, may close this
 * frame, so nothing is touched after the PostMessage).
 */
void CMerge20DedBar::OnButtonClicked(UINT nID)
{
	const size_t idx = static_cast<size_t>(nID) - IDC_MERGE20_DED_EXIT;
	if (idx >= std::size(Merge20DedBarCommands))
		return;
	CWnd* pParent = GetParent();
	if (pParent != nullptr)
		pParent->PostMessage(WM_COMMAND, Merge20DedBarCommands[idx]);
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
	if (m_btnPrev.GetSafeHwnd() == nullptr)
		return;
	String sText = _("Prev");
	m_btnPrev.SetWindowText(sText.c_str());
	sText = _("Next");
	m_btnNext.SetWindowText(sText.c_str());
	// The msgids are the plain state names (no "1:" prefix: the S2
	// number keys are text input in the file editor, so the buttons
	// must not imply they work here). Literals, so the .po extraction
	// finds them; do not build them from a table.
	sText = _("TODO");
	m_btnReview[0].SetWindowText(sText.c_str());
	sText = _("DONE");
	m_btnReview[1].SetWindowText(sText.c_str());
	sText = _("HOLD");
	m_btnReview[2].SetWindowText(sText.c_str());
	sText = _("SPECIAL");
	m_btnReview[3].SetWindowText(sText.c_str());
	sText = _("KEEP");
	m_btnReview[4].SetWindowText(sText.c_str());
	sText = _("Pin");
	m_btnPin.SetWindowText(sText.c_str());
	sText = _("Unreviewed only");
	m_btnTodoOnly.SetWindowText(sText.c_str());
	sText = _("Sniper");
	m_btnSniper.SetWindowText(sText.c_str());
	sText = _("Back to Normal Mode");
	m_btnExit.SetWindowText(sText.c_str());
	SetZoomText(m_nZoom);
}

void CMerge20DedBar::SetReviewState(ReviewState state)
{
	if (m_btnReview[0].GetSafeHwnd() == nullptr || state == m_reviewState)
		return;
	m_reviewState = state;
	for (int i = 0; i < 5; ++i)
		m_btnReview[i].SetCheck(i == static_cast<int>(state) ? BST_CHECKED : BST_UNCHECKED);
}

void CMerge20DedBar::SetReviewEnabled(bool bEnable)
{
	if (m_btnReview[0].GetSafeHwnd() == nullptr)
		return;
	for (int i = 0; i < 5; ++i)
		m_btnReview[i].EnableWindow(bEnable);
}

void CMerge20DedBar::SetPrevNextEnabled(bool bPrev, bool bNext)
{
	if (m_btnPrev.GetSafeHwnd() == nullptr)
		return;
	m_btnPrev.EnableWindow(bPrev);
	m_btnNext.EnableWindow(bNext);
}

void CMerge20DedBar::SetPinned(bool bPinned)
{
	if (m_btnPin.GetSafeHwnd() == nullptr)
		return;
	m_btnPin.SetCheck(bPinned ? BST_CHECKED : BST_UNCHECKED);
}

void CMerge20DedBar::SetTodoOnly(bool bTodoOnly)
{
	if (m_btnTodoOnly.GetSafeHwnd() == nullptr)
		return;
	m_btnTodoOnly.SetCheck(bTodoOnly ? BST_CHECKED : BST_UNCHECKED);
}

void CMerge20DedBar::SetSniperOn(bool bSniperOn)
{
	if (m_btnSniper.GetSafeHwnd() == nullptr)
		return;
	m_btnSniper.SetCheck(bSniperOn ? BST_CHECKED : BST_UNCHECKED);
}

void CMerge20DedBar::SetZoomText(int nZoom)
{
	if (m_btnZoom.GetSafeHwnd() == nullptr || nZoom == m_nZoom)
		return;
	m_nZoom = nZoom;
	String sText = strutils::format(_T("%d%%"), nZoom / 10);
	m_btnZoom.SetWindowText(sText.c_str());
}
