// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20DedBar.cpp
 *
 * @brief Implementation file for CMerge20DedBar
 *
 * Merge20 (Phase 4 D1/D2): the dedicated compare mode bar, following
 * the CDiffViewBar precedent. D2 adds the buttons (Prev/Next file,
 * five review-state buttons, pin, unreviewed-only and sniper toggles,
 * the zoom display, exit) in a single row; D9 adds the diffs-only toggle button after the sniper toggle. D3 adds the file-info label at
 * the left end (current file name + (n/m) position in the patrol set).
 * The bar owns no state: clicks are forwarded as WM_COMMAND
 * (ID_MERGE20_DED_*) to the parent frame, which applies them and
 * refreshes the button states through the setters. Toggle buttons are
 * BS_PUSHLIKE checkboxes whose check is set by the frame (not
 * auto-toggled), so the shown state is always the frame's state.
 *
 * U5 (UI redesign, design sections 2.1 and 4/U5) owner-draws the
 * buttons (BS_OWNERDRAW + DrawItem) and the bar background
 * (Merge20Theme::DrawBrandBar: HTML .ded-bar gradient + 2px accent
 * underline). The five review buttons are coloured pills (HTML
 * .ded-rev colours: checked = filled + white text, unchecked =
 * outline + state-coloured text); the exit button is an accent
 * fill; checked toggles (pin / unreviewed-only / sniper /
 * diffs-only) are accent fills. Button order, fixed widths,
 * height 32px, command forwarding and toggle behaviour are
 * unchanged (D2).
 */

#include "StdAfx.h"
#include "Merge20/Merge20DedBar.h"
#include "Merge20/Merge20Theme.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20DedBar, CSizingControlBarCF);

namespace
{
	/** @brief Fixed bar height: one row of 24px buttons with 4px
	 * margins top and bottom. The height is enforced in LoadState
	 * and CalcFixedLayout (see section 24.6); never derive it from
	 * a saved size or a drag. */
	constexpr int Merge20DedBarHeight = 32;
	/** @brief Button height and vertical position in the bar. */
	constexpr int Merge20DedBarButtonHeight = 24;
	constexpr int Merge20DedBarButtonTop = 4;
	/** @brief Left margin and gap between buttons. */
	constexpr int Merge20DedBarMargin = 8;
	constexpr int Merge20DedBarGap = 4;

	/** @brief Fixed button widths, in bar order. Wide enough for the
	 * English and the Japanese labels (see Japanese.po). The first
	 * entry is the D3 file-info label (file name + (n/m)), not a
	 * button; long names are ellipsized (SS_ENDELLIPSIS). */
	constexpr int Merge20DedBarWidths[] =
	{
		300, // File info
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
		84, // Diffs only
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
		IDC_MERGE20_DED_DIFFS_ONLY,
		IDC_MERGE20_DED_ZOOM,
		IDC_MERGE20_DED_EXIT,
	};

	/** @brief Frame commands the buttons forward, in control-ID order
	 * (IDC_MERGE20_DED_EXIT..IDC_MERGE20_DED_ZOOM): OnButtonClicked
	 * indexes this table by (nID - IDC_MERGE20_DED_EXIT), so the order
	 * here must follow the IDC values, not the bar order. */
	constexpr UINT Merge20DedBarCommands[] =
	{
		ID_MERGE20_DED_EXIT,
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
	};

	/** @brief Text colour on the bar (U5, slate palette of design
	 * section 2.1, same as the brand bar): light slate-900, dark
	 * slate-100. */
	constexpr COLORREF Merge20DedBarTextLight = RGB(15, 23, 42);
	constexpr COLORREF Merge20DedBarTextDark = RGB(241, 245, 249);
	/** @brief Muted text / flat-button outline (U5): light
	 * slate-400, dark slate-500. */
	constexpr COLORREF Merge20DedBarMutedLight = RGB(148, 163, 184);
	constexpr COLORREF Merge20DedBarMutedDark = RGB(100, 116, 139);
	/** @brief Dark text on a light accent fill (U5): the exit
	 * button and checked toggles use the dark accent #38BDF8 in
	 * dark mode, on which white text would be unreadable. */
	constexpr COLORREF Merge20DedBarDarkOnAccent = RGB(9, 13, 22);

	/** @brief Blend crFrom towards crTo by nPercentTo (0-100). */
	COLORREF Merge20DedBarBlend(COLORREF crFrom, COLORREF crTo, int nPercentTo)
	{
		return RGB(
			GetRValue(crFrom) + (GetRValue(crTo) - GetRValue(crFrom)) * nPercentTo / 100,
			GetGValue(crFrom) + (GetGValue(crTo) - GetGValue(crFrom)) * nPercentTo / 100,
			GetBValue(crFrom) + (GetBValue(crTo) - GetBValue(crFrom)) * nPercentTo / 100);
	}

	/** @brief Lighten a colour towards white (U5): unselected
	 * review pills in dark mode use the lightened state colour
	 * for outline and text, because the raw dark TODO grey
	 * (#475569) is unreadable on the dark gradient. */
	COLORREF Merge20DedBarLighten(COLORREF cr, int nPercent)
	{
		return Merge20DedBarBlend(cr, RGB(255, 255, 255), nPercent);
	}

	/**
	 * @brief Bar background colour at one x position (U5).
	 * The bar background is a horizontal gradient
	 * (Merge20Theme::DrawBrandBar), so an unselected flat button
	 * fills with the gradient colour at its own centre and blends
	 * into the bar instead of showing a solid patch.
	 */
	COLORREF Merge20DedBarBackgroundAt(int x, int nWidth, bool bDark)
	{
		if (nWidth <= 1)
			return Merge20Theme::GetBrandBarStartColor(bDark);
		if (x < 0)
			x = 0;
		if (x > nWidth)
			x = nWidth;
		return Merge20DedBarBlend(
			Merge20Theme::GetBrandBarStartColor(bDark),
			Merge20Theme::GetBrandBarEndColor(bDark), x * 100 / nWidth);
	}
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
	ON_WM_DRAWITEM()
	ON_WM_PAINT()
	ON_WM_CTLCOLOR()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_SETTINGCHANGE()
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MERGE20_DED_EXIT, IDC_MERGE20_DED_ZOOM, OnButtonClicked)
	// Merge20 (Phase 4 D9): the diffs-only button (IDC 1805) is outside
	// the contiguous 1745-1756 range above, so it gets its own range.
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MERGE20_DED_DIFFS_ONLY, IDC_MERGE20_DED_DIFFS_ONLY, OnButtonClicked)
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

	// One row of 24px buttons: fix the bar height to 32px. The
	// LoadState override below re-asserts this after any persisted
	// size is read, and CalcFixedLayout clamps the laid-out height,
	// so a saved or dragged taller size can never come back.
	m_szHorz = CSize(120, Merge20DedBarHeight);
	m_szMinHorz = CSize(33, Merge20DedBarHeight);

	CFont* pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	// U5: all buttons are owner-drawn (BS_OWNERDRAW + DrawItem).
	// The check state is still held by the button itself
	// (SetCheck/GetCheck, ODS_CHECKED in DrawItem), so the frame
	// setters work unchanged; only the painting is custom. Push
	// vs toggle is a drawing decision now (indexes 2-10 are the
	// toggles: five review states, pin, unreviewed-only, sniper,
	// diffs-only; Prev/Next, the zoom display and exit are plain
	// push buttons).
	const DWORD dwButtonStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW;
	CRect rc(0, 0, 0, 0);
	// Merge20 (Phase 4 D3): the file-info label (current file name +
	// (n/m) position), first in the bar. A label, not a button: it
	// never sends BN_CLICKED, so it is not part of the button arrays
	// below. Long names are ellipsized, and the text is vertically
	// centered (SS_CENTERIMAGE) to line up with the 24px buttons.
	if (!m_stFileInfo.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_ENDELLIPSIS | SS_CENTERIMAGE,
		rc, this, IDC_MERGE20_DED_FILE_INFO))
	{
		TRACE0("Failed to create Merge20DedBar file info\n");
		return -1;
	}
	m_stFileInfo.SetFont(pFont);
	CButton* pButtons[] =
	{
		&m_btnPrev, &m_btnNext,
		&m_btnReview[0], &m_btnReview[1], &m_btnReview[2], &m_btnReview[3], &m_btnReview[4],
		&m_btnPin, &m_btnTodoOnly, &m_btnSniper, &m_btnDiffsOnly, &m_btnZoom, &m_btnExit,
	};
	for (size_t i = 0; i < std::size(pButtons); ++i)
	{
		if (!pButtons[i]->Create(_T(""), dwButtonStyle, rc, this, Merge20DedBarControlIDs[i]))
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
 * @brief Load the saved bar size, then force the fixed thin height.
 *
 * The base LoadState reads the persisted sizeHorzCY (and the
 * vertical/floating sizes). A D1 run saved the 200px class default
 * there, and a dragged height persisted too, so without this the
 * D2 bar would come back 200px tall (or taller) on the next frame
 * even though OnCreate sets 32px: the frame calls LoadState after
 * OnCreate. The width/length components are kept as saved.
 */
void CMerge20DedBar::LoadState(LPCTSTR lpszProfileName)
{
	CSizingControlBarCF::LoadState(lpszProfileName);
	m_szHorz.cy = Merge20DedBarHeight;
	m_szVert.cx = Merge20DedBarHeight;
	m_szFloat.cy = Merge20DedBarHeight;
}

/**
 * @brief Clamp the height the layout gives the bar to one button row.
 *
 * The base implementation returns max(min, m_szHorz.cy), so dragging
 * the bar's bottom edge (OnTrackUpdateSize writes m_szHorz.cy for
 * every sizing bar on the row, up to the remaining client height)
 * would let the bar grow over the file panes. Clamping the returned
 * size keeps all the base row negotiation and only caps the height;
 * the dragged m_szHorz.cy is never used for layout, and LoadState
 * resets it on the next frame. The bar is only ever docked top or
 * bottom (EnableDocking TOP|BOTTOM), so the vertical branch is
 * unreachable; it is left to the base.
 */
CSize CMerge20DedBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
	CSize size = CSizingControlBarCF::CalcFixedLayout(bStretch, bHorz);
	if (bHorz)
		size.cy = Merge20DedBarHeight;
	return size;
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
 * @brief Paint the bar background (U5): the HTML .ded-bar
 * gradient with a 2px accent underline (Merge20Theme::
 * DrawBrandBar). The base OnPaint paints nothing (see
 * sizecbar.cpp: CSizingControlBar::OnPaint is an empty CPaintDC),
 * so this replaces nothing; the buttons paint themselves through
 * DrawItem.
 */
void CMerge20DedBar::OnPaint()
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(&rect);
	if (rect.IsRectEmpty())
		return;
	Merge20Theme::DrawBrandBar(&dc, rect, Merge20Theme::IsDarkMode());
}

/**
 * @brief Route owner-draw requests to DrawDedButton (U5).
 * Only the buttons are owner-drawn; the file-info label is a
 * static control and never sends WM_DRAWITEM.
 */
void CMerge20DedBar::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (lpDrawItemStruct != nullptr && lpDrawItemStruct->CtlType == ODT_BUTTON)
		DrawDedButton(lpDrawItemStruct);
	else
		CSizingControlBarCF::OnDrawItem(nIDCtl, lpDrawItemStruct);
}

/**
 * @brief Owner-draw one bar button (U5).
 *
 * Roles (by control ID): the five review buttons are coloured
 * pills in the HTML .ded-rev colours (checked = filled with the
 * state colour + white text, unchecked = state-coloured outline
 * and text on the bar gradient); the exit button is an accent
 * fill; the four toggles (pin / unreviewed-only / sniper /
 * diffs-only) are an accent fill while checked and flat while
 * unchecked; Prev/Next and the zoom display are flat. A disabled
 * button is muted grey. The pressed look is a slightly darkened
 * fill, the focused look a focus rect. Text comes from the
 * button's own window text (set by UpdateResources), so no new
 * strings are needed.
 */
void CMerge20DedBar::DrawDedButton(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (lpDrawItemStruct == nullptr)
		return;
	CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
	if (pDC == nullptr)
		return;
	const CRect rcItem(lpDrawItemStruct->rcItem);
	if (rcItem.IsRectEmpty())
		return;
	const UINT nID = lpDrawItemStruct->CtlID;
	const bool bDark = Merge20Theme::IsDarkMode();
	const bool bChecked = (lpDrawItemStruct->itemState & ODS_CHECKED) != 0;
	const bool bPressed = (lpDrawItemStruct->itemState & ODS_SELECTED) != 0;
	const bool bDisabled = (lpDrawItemStruct->itemState & ODS_DISABLED) != 0;
	const bool bFocused = (lpDrawItemStruct->itemState & ODS_FOCUS) != 0;

	// Role by control ID (resource.h: review 1748-1752 in
	// ReviewState order, exit 1745, toggles 1753-1755 + 1805).
	int nReview = -1;
	if (nID >= IDC_MERGE20_DED_REVIEW_TODO && nID <= IDC_MERGE20_DED_REVIEW_KEEP)
		nReview = static_cast<int>(nID) - IDC_MERGE20_DED_REVIEW_TODO;
	const bool bExit = (nID == IDC_MERGE20_DED_EXIT);
	const bool bToggle = (nID == IDC_MERGE20_DED_PIN ||
		nID == IDC_MERGE20_DED_TODO_ONLY ||
		nID == IDC_MERGE20_DED_SNIPER ||
		nID == IDC_MERGE20_DED_DIFFS_ONLY);

	CRect rcBar;
	GetClientRect(&rcBar);
	const COLORREF crBg = Merge20DedBarBackgroundAt(
		rcItem.CenterPoint().x, rcBar.Width(), bDark);
	const COLORREF crText = bDark ? Merge20DedBarTextDark : Merge20DedBarTextLight;
	const COLORREF crMuted = bDark ? Merge20DedBarMutedDark : Merge20DedBarMutedLight;
	COLORREF crFill = crBg;
	COLORREF crLine = crMuted;
	COLORREF crBtnText = crText;
	if (bDisabled)
	{
		crBtnText = crMuted;
		crLine = crMuted;
	}
	else if (nReview >= 0)
	{
		const COLORREF crState = Merge20Theme::GetReviewStateColor(
			static_cast<ReviewState>(nReview), bDark);
		if (bChecked)
		{
			// Selected pill: filled with the state colour, white
			// text (white on every state colour meets the 3:1
			// large-text contrast; see design section 2.1).
			crFill = crState;
			crLine = crState;
			crBtnText = RGB(255, 255, 255);
		}
		else
		{
			// Unselected pill: outline + state-coloured text.
			// In dark mode the state colour is lightened so the
			// dark TODO grey stays readable on the gradient.
			crLine = bDark ? Merge20DedBarLighten(crState, 45) : crState;
			crBtnText = crLine;
		}
	}
	else if (bExit || (bToggle && bChecked))
	{
		// Accent fill: the exit button, and a toggle while on.
		// Dark mode uses the light cyan accent, so the text is
		// dark there; light mode uses white text on the blue.
		crFill = Merge20Theme::GetAccentColor(bDark);
		crLine = crFill;
		crBtnText = bDark ? Merge20DedBarDarkOnAccent : RGB(255, 255, 255);
	}
	if (bPressed && !bDisabled)
		crFill = Merge20DedBarBlend(crFill, RGB(0, 0, 0), 12);

	// Paint the whole item rect first (U5): the rounded pill
	// leaves the corners unpainted, and the button repaints alone
	// when its check changes, so the corners must carry the bar
	// gradient colour or the previous button would show through.
	pDC->FillSolidRect(&rcItem, crBg);
	CRect rcDraw = rcItem;
	rcDraw.DeflateRect(1, 1);
	Merge20Theme::DrawRoundedRect(pDC, rcDraw, 6, crFill, crLine);

	CString sText;
	if (CWnd* pButton = CWnd::FromHandle(lpDrawItemStruct->hwndItem))
		pButton->GetWindowText(sText);
	CFont* pFont = nullptr;
	if (CWnd* pButton = CWnd::FromHandle(lpDrawItemStruct->hwndItem))
		pFont = pButton->GetFont();
	if (pFont == nullptr)
		pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	CFont* pOldFont = pDC->SelectObject(pFont);
	pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(crBtnText);
	CRect rcText = rcDraw;
	if (bPressed)
		rcText.OffsetRect(1, 1);
	pDC->DrawText(sText, &rcText,
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
 * @brief Let the file-info label show the bar gradient (U5):
 * transparent background + theme text colour. Other controls
 * keep the default handling.
 */
HBRUSH CMerge20DedBar::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	if (nCtlColor == CTLCOLOR_STATIC && pWnd == &m_stFileInfo)
	{
		pDC->SetBkMode(TRANSPARENT);
		pDC->SetTextColor(Merge20Theme::IsDarkMode() ?
			Merge20DedBarTextDark : Merge20DedBarTextLight);
		return static_cast<HBRUSH>(::GetStockObject(HOLLOW_BRUSH));
	}
	return CSizingControlBarCF::OnCtlColor(pDC, pWnd, nCtlColor);
}

/**
 * @brief Repaint the bar and its buttons after a system colour
 * change (U5): the gradient, the pill colours and the text
 * colours all depend on the theme. The base handler updates
 * the class background brush.
 */
void CMerge20DedBar::OnSysColorChange()
{
	CSizingControlBarCF::OnSysColorChange();
	RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

/**
 * @brief Same repaint as OnSysColorChange, for theme changes
 * announced through WM_SETTINGCHANGE (U5).
 */
void CMerge20DedBar::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	CSizingControlBarCF::OnSettingChange(uFlags, lpszSection);
	RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

/**
 * @brief Lay the file-info label and the buttons out in one row, left
 * aligned, in spec order. Buttons past the right edge are clipped
 * (the bar is full-width when docked at the top; the exit button is
 * last per spec section 5.2, and the View menu still exits dedicated
 * mode).
 */
void CMerge20DedBar::LayoutButtons()
{
	if (m_btnPrev.GetSafeHwnd() == nullptr)
		return;
	int x = Merge20DedBarMargin;
	// Merge20 (Phase 4 D3): the file-info label first.
	if (m_stFileInfo.GetSafeHwnd() != nullptr)
	{
		m_stFileInfo.SetWindowPos(nullptr, x, Merge20DedBarButtonTop,
			Merge20DedBarWidths[0], Merge20DedBarButtonHeight,
			SWP_NOZORDER | SWP_NOACTIVATE);
		x += Merge20DedBarWidths[0] + Merge20DedBarGap;
	}
	CButton* pButtons[] =
	{
		&m_btnPrev, &m_btnNext,
		&m_btnReview[0], &m_btnReview[1], &m_btnReview[2], &m_btnReview[3], &m_btnReview[4],
		&m_btnPin, &m_btnTodoOnly, &m_btnSniper, &m_btnDiffsOnly, &m_btnZoom, &m_btnExit,
	};
	for (size_t i = 0; i < std::size(pButtons); ++i)
	{
		pButtons[i]->SetWindowPos(nullptr, x, Merge20DedBarButtonTop,
			Merge20DedBarWidths[i + 1], Merge20DedBarButtonHeight,
			SWP_NOZORDER | SWP_NOACTIVATE);
		x += Merge20DedBarWidths[i + 1] + Merge20DedBarGap;
	}
}

/**
 * @brief Forward a button click to the parent frame as its command.
 * The frame applies the action (and, for Prev/Next, may close this
 * frame, so nothing is touched after the PostMessage).
 */
void CMerge20DedBar::OnButtonClicked(UINT nID)
{
	// Merge20 (Phase 4 D9): the diffs-only button (IDC 1805) is outside
	// the contiguous 1745-1756 range the table below indexes, so it is
	// forwarded explicitly instead of through the table.
	if (nID == IDC_MERGE20_DED_DIFFS_ONLY)
	{
		if (CWnd* pParent = GetParent())
			pParent->PostMessage(WM_COMMAND, ID_MERGE20_DED_DIFFS_ONLY);
		return;
	}
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
	sText = _("Diffs only");
	m_btnDiffsOnly.SetWindowText(sText.c_str());
	sText = _("Back to Normal Mode");
	m_btnExit.SetWindowText(sText.c_str());
	SetZoomText(m_nZoom);
	// U5: the new texts need owner-drawn repaints, and the bar
	// background follows the theme.
	Invalidate(FALSE);
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

/**
 * @brief Show the diffs-only state on the toggle button (Merge20
 * Phase 4 D9). As with SetPinned, the check is set by the frame;
 * the button never toggles itself.
 */
void CMerge20DedBar::SetDiffsOnly(bool bDiffsOnly)
{
	if (m_btnDiffsOnly.GetSafeHwnd() == nullptr)
		return;
	m_btnDiffsOnly.SetCheck(bDiffsOnly ? BST_CHECKED : BST_UNCHECKED);
}

void CMerge20DedBar::SetZoomText(int nZoom)
{
	if (m_btnZoom.GetSafeHwnd() == nullptr || nZoom == m_nZoom)
		return;
	m_nZoom = nZoom;
	String sText = strutils::format(_T("%d%%"), nZoom / 10);
	m_btnZoom.SetWindowText(sText.c_str());
}

/**
 * @brief Show the current file name + (n/m) position (Merge20 Phase 4
 * D3). A no-op when the text did not change, so the idle-time refresh
 * does not flicker the label. The text itself is built by the frame
 * (UpdateDedBar); the bar only displays it.
 */
void CMerge20DedBar::SetFileInfo(const String& sText)
{
	if (m_stFileInfo.GetSafeHwnd() == nullptr || sText == m_sFileInfo)
		return;
	m_sFileInfo = sText;
	m_stFileInfo.SetWindowText(sText.c_str());
	// U5: the label is transparent over the bar gradient (see
	// OnCtlColor), so the bar itself must repaint behind it or
	// the previous text would show through.
	Invalidate(FALSE);
}
