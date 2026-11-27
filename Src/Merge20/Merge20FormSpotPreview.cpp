// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormSpotPreview.cpp
 *
 * @brief Implementation of the spotlight banner's old/new
 * preview (Wave 5 F4).
 *
 * The shape is fitted to the client area (keeping its aspect
 * ratio, with a 6 px margin), so the before / after pair reads
 * at a glance whatever the shape's real size is. The colours
 * follow the canvas (design section 2.6): old = red dashed,
 * new = green solid. The element text is drawn inside the
 * shape with an ellipsis when it does not fit (the same rule
 * as the canvas, design section 6 (6)).
 * This file uses the default PCH (StdAfx.h), like the other
 * MFC modules.
 */
#include "StdAfx.h"
#include "Merge20/Merge20FormSpotPreview.h"
#include "Merge20/Merge20FormCanvasGeom.h"
#include <algorithm>
#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{

	// Colours (the canvas colours, Merge20FormCanvas.cpp).
	const COLORREF kOldBorderColor = RGB(225, 29, 72); // #e11d48
	const COLORREF kOldFillColor = RGB(252, 207, 215); // rgba(244, 63, 94, 0.25) over white
	const COLORREF kNewBorderColor = RGB(5, 150, 105); // #059669
	const COLORREF kNewFillColor = RGB(183, 234, 217); // rgba(16, 185, 129, 0.30) over white
	const COLORREF kEmptyColor = RGB(100, 116, 139); // slate-500 grey

	const int kPreviewMarginPx = 6; /**< Margin around the fitted shape. */

	// Create a dashed pen (HTML dash pattern [5, 3]).
	HPEN CreateDashedPen(int width, COLORREF color)
	{
		LOGBRUSH brush = {};
		brush.lbStyle = BS_SOLID;
		brush.lbColor = color;
		const DWORD styles[2] = { 5, 3 };
		return ::ExtCreatePen(PS_GEOMETRIC | PS_USERSTYLE, width, &brush, 2, styles);
	}

} // namespace

IMPLEMENT_DYNAMIC(CMerge20FormSpotPreview, CWnd)

BEGIN_MESSAGE_MAP(CMerge20FormSpotPreview, CWnd)
	ON_WM_PAINT()
END_MESSAGE_MAP()

CMerge20FormSpotPreview::CMerge20FormSpotPreview()
{
}

CMerge20FormSpotPreview::~CMerge20FormSpotPreview()
{
}

BOOL CMerge20FormSpotPreview::Create(CWnd* pParentWnd, UINT nID)
{
	const CString className = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW,
		::LoadCursor(nullptr, IDC_ARROW), static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)));
	CRect rc(0, 0, 0, 0);
	return CWnd::Create(className, _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_BORDER,
		rc, pParentWnd, nID);
}

void CMerge20FormSpotPreview::SetShape(const Merge20FormShape* pShape, bool bOld)
{
	if (pShape == m_pShape && bOld == m_bOld)
		return;
	m_pShape = pShape;
	m_bOld = bOld;
	Invalidate();
}

void CMerge20FormSpotPreview::OnPaint()
{
	CPaintDC dc(this);
	CRect rcClient;
	GetClientRect(&rcClient);
	dc.FillSolidRect(&rcClient, RGB(255, 255, 255));
	CFont* pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	CFont* pOldFont = dc.SelectObject(pFont);
	dc.SetBkMode(TRANSPARENT);
	if (m_pShape == nullptr)
	{
		dc.SetTextColor(kEmptyColor);
		const String empty = _("No shape");
		dc.DrawText(empty.c_str(), -1, &rcClient, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		dc.SelectObject(pOldFont);
		return;
	}
	const double shapeW = std::abs(m_pShape->x2 - m_pShape->x);
	const double shapeH = std::abs(m_pShape->y2 - m_pShape->y);
	const double availW = static_cast<double>((std::max)(1, rcClient.Width() - 2 * kPreviewMarginPx));
	const double availH = static_cast<double>((std::max)(1, rcClient.Height() - 2 * kPreviewMarginPx));
	// Fit the shape into the client area, keeping its aspect
	// ratio; a degenerate (zero-size) shape gets a minimum box.
	double scale = 1.0;
	if (shapeW > 0.0 && shapeH > 0.0)
		scale = (std::min)(availW / shapeW, availH / shapeH);
	const int drawW = (std::max)(8, static_cast<int>(std::lround(shapeW * scale)));
	const int drawH = (std::max)(8, static_cast<int>(std::lround(shapeH * scale)));
	CRect rcShape(
		rcClient.left + (rcClient.Width() - drawW) / 2,
		rcClient.top + (rcClient.Height() - drawH) / 2,
		rcClient.left + (rcClient.Width() - drawW) / 2 + drawW,
		rcClient.top + (rcClient.Height() - drawH) / 2 + drawH);
	const COLORREF borderColor = m_bOld ? kOldBorderColor : kNewBorderColor;
	const COLORREF fillColor = m_bOld ? kOldFillColor : kNewFillColor;
	const int borderWidth = m_bOld ? 2 : 3;
	dc.FillSolidRect(&rcShape, fillColor);
	CBrush* pOldBrush = dc.SelectObject(CBrush::FromHandle(static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH))));
	if (m_bOld)
	{
		HPEN hPen = CreateDashedPen(borderWidth, borderColor);
		HPEN hOldPen = static_cast<HPEN>(dc.SelectObject(hPen));
		dc.Rectangle(&rcShape);
		dc.SelectObject(hOldPen);
		::DeleteObject(hPen);
	}
	else
	{
		CPen pen(PS_SOLID, borderWidth, borderColor);
		CPen* pOldPen = dc.SelectObject(&pen);
		dc.Rectangle(&rcShape);
		dc.SelectObject(pOldPen);
	}
	dc.SelectObject(pOldBrush);
	if (!m_pShape->isLine && !m_pShape->text.empty())
	{
		CRect rcText(rcShape);
		rcText.DeflateRect(2, 1, 2, 1);
		if (rcText.Width() > 0 && rcText.Height() > 0)
		{
			dc.SetTextColor(borderColor);
			dc.DrawText(m_pShape->text.c_str(), -1, &rcText,
				DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
		}
	}
	dc.SelectObject(pOldFont);
}
