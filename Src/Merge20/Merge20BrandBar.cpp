// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20BrandBar.cpp
 *
 * @brief Implementation file for CMerge20BrandBar
 *
 * Merge20 (UI U1, design sections 2.1 and 4/U1): the WizMerge brand
 * header. Owner-drawn (Merge20Theme::DrawBrandBar for the gradient +
 * 2px accent underline): glowing dot + bold "WizMerge" + version on
 * the left, progress summary on the right. See Merge20BrandBar.h.
 */

#include "StdAfx.h"
#include "Merge20/Merge20BrandBar.h"
#include "Merge20/Merge20Theme.h"
#include "VersionInfo.h"
#include "DirDoc.h"
#include "MergeDoc.h"
#include "DirView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20BrandBar, CSizingControlBarCF);

namespace
{
	/** @brief Fixed bar height: one 30px row (design section 4/U1).
	 * The height is enforced in LoadState and CalcFixedLayout (see
	 * section 24.6); never derive it from a saved size or a drag. */
	constexpr int Merge20BrandBarHeight = 30;
	/** @brief Left margin: dot centre x, wordmark x. */
	constexpr int Merge20BrandBarDotCenterX = 13;
	constexpr int Merge20BrandBarWordmarkX = 26;
	/** @brief Right margin of the summary text. */
	constexpr int Merge20BrandBarRightMargin = 12;
	/** @brief Summary refresh timer (bar-local id) and interval. */
	constexpr UINT_PTR Merge20BrandBarRefreshTimer = 1;
	constexpr UINT Merge20BrandBarRefreshIntervalMs = 500;
	/** @brief Wordmark/summary text colours (slate palette of design
	 * section 2.1: light slate-900, dark slate-100). */
	constexpr COLORREF Merge20BrandBarTextLight = RGB(15, 23, 42);
	constexpr COLORREF Merge20BrandBarTextDark = RGB(241, 245, 249);
} // namespace

BEGIN_MESSAGE_MAP(CMerge20BrandBar, CSizingControlBarCF)
	//{{AFX_MSG_MAP(CMerge20BrandBar)
	ON_WM_CREATE()
	ON_WM_LBUTTONDOWN()
	ON_WM_PAINT()
	ON_WM_TIMER()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CMerge20BrandBar::CMerge20BrandBar()
	: m_bDark(false)
{
}

CMerge20BrandBar::~CMerge20BrandBar()
{
}

BOOL CMerge20BrandBar::Create(
	CWnd* pParentWnd,
	LPCTSTR lpszWindowName,
	DWORD dwStyle,
	UINT nID)
{
	return CSizingControlBarCF::Create(
		lpszWindowName,
		pParentWnd,
		nID,
		dwStyle);
}

/**
 * @brief Update any resources necessary after a GUI language change
 */
void CMerge20BrandBar::UpdateResources()
{
	String sCaption = _("Brand Header");
	SetWindowText(sCaption.c_str());
	Invalidate(FALSE);
}

int CMerge20BrandBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CSizingControlBarCF::OnCreate(lpCreateStruct) == -1)
		return -1;

	SetSCBStyle(SCBS_EDGETOP | SCBS_EDGEBOTTOM | SCBS_SIZECHILD);

	// One fixed 30px row. The LoadState override below re-asserts
	// this after any persisted size is read, and CalcFixedLayout
	// clamps the laid-out height, so a saved or dragged taller size
	// can never come back (D1/D2 lesson, section 24.6).
	m_szHorz = CSize(120, Merge20BrandBarHeight);
	m_szMinHorz = CSize(33, Merge20BrandBarHeight);

	// Version from the same source as the About dialog
	// (CVersionInfo, the product version string).
	CVersionInfo verinfo;
	m_sVersion = verinfo.GetProductVersion();

	// Bold wordmark font (Merge20Theme); the rest uses the default
	// GUI font, like the dedicated-mode bar's buttons.
	if (!Merge20Theme::CreateBoldFont(m_fontBold))
		TRACE0("Failed to create Merge20BrandBar bold font\n");

	SetTimer(Merge20BrandBarRefreshTimer, Merge20BrandBarRefreshIntervalMs, nullptr);
	RefreshSummary();

	return 0;
}

/**
 * @brief Load the saved bar size, then force the fixed thin height.
 *
 * Same defence as CMerge20DedBar::LoadState (section 24.6): the base
 * reads a persisted sizeHorzCY (the D1 run saved the 200px class
 * default there, and a dragged height persisted too), so without
 * this the bar would come back 200px tall (or taller) on the next
 * frame even though OnCreate sets 30px: the frame calls LoadState
 * after OnCreate. The width/length components are kept as saved.
 */
void CMerge20BrandBar::LoadState(LPCTSTR lpszProfileName)
{
	CSizingControlBarCF::LoadState(lpszProfileName);
	m_szHorz.cy = Merge20BrandBarHeight;
	m_szVert.cx = Merge20BrandBarHeight;
	m_szFloat.cy = Merge20BrandBarHeight;
}

/**
 * @brief Clamp the height the layout gives the bar to one 30px row.
 *
 * Same defence as CMerge20DedBar::CalcFixedLayout (section 24.6):
 * the base implementation returns max(min, m_szHorz.cy), so dragging
 * the bar's bottom edge would let the bar grow over the views.
 * Clamping the returned size keeps all the base row negotiation and
 * only caps the height; the dragged m_szHorz.cy is never used for
 * layout, and LoadState resets it on the next frame. The bar is only
 * ever docked top or bottom (EnableDocking TOP|BOTTOM), so the
 * vertical branch is unreachable; it is left to the base.
 */
CSize CMerge20BrandBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
	CSize size = CSizingControlBarCF::CalcFixedLayout(bStretch, bHorz);
	if (bHorz)
		size.cy = Merge20BrandBarHeight;
	return size;
}

/**
* @note The window must always be docked after movement
* there are too much troubles if we get reparented to some minidockbar
*
*/
void CMerge20BrandBar::OnLButtonDown(UINT nFlags, CPoint point)
{
	CSizingControlBarCF::OnLButtonDown(nFlags, point);
	if (m_pDockBar != nullptr)
	{
		if (!IsHorzDocked())
			m_pDockContext->ToggleDocking();
	}
}

void CMerge20BrandBar::OnPaint()
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(&rect);
	if (rect.IsRectEmpty())
		return;
	const bool bDark = Merge20Theme::IsDarkMode();
	Merge20Theme::DrawBrandBar(&dc, rect, bDark);

	dc.SetBkMode(TRANSPARENT);
	const COLORREF crText = bDark ? Merge20BrandBarTextDark : Merge20BrandBarTextLight;
	const int yCenter = rect.Height() / 2;

	// Glowing dot (HTML top bar): accent ring + bright cyan core.
	CPen penNull;
	penNull.CreateStockObject(NULL_PEN);
	CBrush brushAccent(Merge20Theme::GetAccentColor(bDark));
	CBrush brushGlow(Merge20Theme::GetAccentGlowColor());
	CPen* pOldPen = dc.SelectObject(&penNull);
	CBrush* pOldBrush = dc.SelectObject(&brushAccent);
	dc.Ellipse(Merge20BrandBarDotCenterX - 7, yCenter - 7,
		Merge20BrandBarDotCenterX + 7, yCenter + 7);
	dc.SelectObject(&brushGlow);
	dc.Ellipse(Merge20BrandBarDotCenterX - 4, yCenter - 4,
		Merge20BrandBarDotCenterX + 4, yCenter + 4);
	dc.SelectObject(pOldBrush);
	dc.SelectObject(pOldPen);

	// Bold "WizMerge" wordmark, then the version in accent colour.
	CFont* pFontNormal = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	CFont* pOldFont = dc.SelectObject(m_fontBold.GetSafeHandle() != nullptr ? &m_fontBold : pFontNormal);
	dc.SetTextColor(crText);
	CRect rcWord(rect);
	rcWord.left = Merge20BrandBarWordmarkX;
	dc.DrawText(CString(_T("WizMerge")), &rcWord, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	const CSize sizeWord = dc.GetTextExtent(_T("WizMerge"));
	dc.SelectObject(pFontNormal);
	dc.SetTextColor(Merge20Theme::GetAccentColor(bDark));
	CRect rcVersion(rect);
	rcVersion.left = Merge20BrandBarWordmarkX + sizeWord.cx + 8;
	dc.DrawText(CString(m_sVersion.c_str()), &rcVersion, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	const CSize sizeVersion = dc.GetTextExtent(m_sVersion.c_str());

	// Progress summary on the right end, ellipsized if the window
	// is narrow (never overlapping the version).
	dc.SetTextColor(crText);
	CRect rcSummary(rect);
	rcSummary.left = rcVersion.left + sizeVersion.cx + 16;
	if (rcSummary.left < rect.Width() / 2)
		rcSummary.left = rect.Width() / 2;
	rcSummary.right = rect.right - Merge20BrandBarRightMargin;
	dc.DrawText(CString(m_sSummaryText.c_str()), &rcSummary,
		DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
	dc.SelectObject(pOldFont);
}

void CMerge20BrandBar::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == Merge20BrandBarRefreshTimer)
		RefreshSummary();
	else
		__super::OnTimer(nIDEvent);
}

void CMerge20BrandBar::RefreshSummary()
{
	const String sNew = GetSummaryText();
	const bool bDark = Merge20Theme::IsDarkMode();
	if (sNew != m_sSummaryText || bDark != m_bDark)
	{
		m_sSummaryText = sNew;
		m_bDark = bDark;
		Invalidate(FALSE);
	}
}

/**
 * @brief Current right-end text for the active document.
 * @return The active folder compare's S3 count text ("N displayed, M
 * checked, K unreviewed", same definition as
 * CDirView::GetMerge20StatusCountText); "File Compare" while a file
 * compare is active; empty otherwise (start page, no document).
 */
String CMerge20BrandBar::GetSummaryText()
{
	CMDIFrameWnd* pMainFrame = dynamic_cast<CMDIFrameWnd*>(AfxGetMainWnd());
	if (pMainFrame == nullptr)
		return String();
	CMDIChildWnd* pChild = pMainFrame->MDIGetActive();
	if (pChild == nullptr)
		return String();
	CDocument* pDoc = pChild->GetActiveDocument();
	if (pDoc == nullptr)
		return String();
	// Folder compare: the S3 count text, same definition
	// ("N displayed, M checked, K unreviewed").
	if (CDirDoc* pDirDoc = dynamic_cast<CDirDoc*>(pDoc))
	{
		CDirView* pView = pDirDoc->GetMainView();
		if (pView == nullptr)
			return String();
		return pView->GetMerge20StatusCountText();
	}
	// File compare: state wording only (no counts here; the counts
	// belong to the folder compare). Anything else (start page,
	// image/web compares): no summary.
	if (dynamic_cast<CMergeDoc*>(pDoc) != nullptr)
		return _("File Compare");
	return String();
}
