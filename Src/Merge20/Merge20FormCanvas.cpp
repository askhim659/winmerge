// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCanvas.cpp
 *
 * @brief Implementation of the FORM compare visual-preview
 * canvas (Wave 5 F3).
 *
 * The drawing mirrors the HTML tool's `renderCanvasWithSpotlight`
 * (FROM_____1.html) without the spotlight part (F4): white
 * background, 100-unit grid (500-unit major lines), unchanged
 * shapes in grey, old shapes in red dashed, new shapes in green,
 * and the element text inside the shapes (design section 6 (6)).
 * Two deliberate differences from the HTML tool:
 * - Line widths and dash lengths are device pixels, not scaled
 *   with the zoom (GDI geometric pens); the HTML canvas scales
 *   them with the bitmap. The colours and the dash pattern
 *   ([5, 3]) are the same.
 * - The text is drawn with a real font (GDI `DrawText`) and cut
 *   with an ellipsis when it does not fit the rect (design
 *   section 6 (6)); the HTML tool draws no element text, only a
 *   "name (x, y)" label above changed shapes.
 * The drag scroll / click rule is the HTML one
 * (`enableDragToScrollAndClickShape`): a drag over 4 px scrolls,
 * a plain click selects a shape (changed shapes only, smallest
 * area wins, design section 3.4).
 * This file uses the default PCH (StdAfx.h), like the other
 * MFC modules; the pure geometry lives in Merge20FormCanvasGeom.
 */
#include "StdAfx.h"
#include "Merge20/Merge20FormCanvas.h"
#include <algorithm>
#include <cmath>
#include <wchar.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{

	// Colours (HTML `renderCanvasWithSpotlight`).
	const COLORREF kGridMajorColor = RGB(203, 213, 225); // #cbd5e1
	const COLORREF kGridMinorColor = RGB(241, 245, 249); // #f1f5f9
	const COLORREF kUnchangedColor = RGB(203, 213, 225); // #cbd5e1
	const COLORREF kOldBorderColor = RGB(225, 29, 72); // #e11d48
	const COLORREF kOldFillColor = RGB(254, 228, 232); // rgba(244, 63, 94, 0.16) over white
	const COLORREF kNewBorderColor = RGB(5, 150, 105); // #059669
	const COLORREF kNewFillColor = RGB(209, 250, 229); // rgba(16, 185, 129, 0.22) over white
	const COLORREF kUnchangedTextColor = RGB(100, 116, 139); // slate-500 grey
	const COLORREF kSelectedColor = RGB(79, 70, 229); // #4f46e5 (HTML arrow colour)
	const COLORREF kNoteColor = RGB(100, 116, 139); // slate-500 grey

	const int kScrollLinePx = 16; /**< Pixels per scroll line. */
	const int kScrollLinesPerWheelNotch = 3; /**< Wheel lines per notch. */

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

IMPLEMENT_DYNAMIC(CMerge20FormCanvas, CWnd)

BEGIN_MESSAGE_MAP(CMerge20FormCanvas, CWnd)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

CMerge20FormCanvas::CMerge20FormCanvas()
{
}

CMerge20FormCanvas::~CMerge20FormCanvas()
{
}

BOOL CMerge20FormCanvas::Create(CWnd* pParentWnd, UINT nID)
{
	const CString className = AfxRegisterWndClass(CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
		::LoadCursor(nullptr, IDC_ARROW), static_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH)));
	CRect rc(0, 0, 0, 0);
	return CWnd::Create(className, _T(""), WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL |
		WS_CLIPSIBLINGS | WS_BORDER, rc, pParentWnd, nID);
}

void CMerge20FormCanvas::SetResult(const Merge20FormFileResult* pResult)
{
	if (pResult == m_pResult)
		return; // Same file: keep the scroll offset and the selection.
	m_pResult = pResult;
	m_selectedShapeKey.clear();
	m_scrollX = 0;
	m_scrollY = 0;
	UpdateScale();
	UpdateScrollBars();
	Invalidate();
}

void CMerge20FormCanvas::SetZoomMode(Merge20FormZoomMode mode)
{
	m_zoomMode = mode;
	UpdateScale();
	UpdateScrollBars();
	Invalidate();
}

void CMerge20FormCanvas::SetSliderPercent(double percent)
{
	m_sliderPercent = Merge20FormCanvasClampSliderPercent(percent);
	m_zoomMode = Merge20FormZoomMode::Slider;
	UpdateScale();
	UpdateScrollBars();
	Invalidate();
}

void CMerge20FormCanvas::SetSelectedShapeKey(const std::wstring& key)
{
	if (key == m_selectedShapeKey)
		return;
	m_selectedShapeKey = key;
	Invalidate();
}

void CMerge20FormCanvas::OnPaint()
{
	CPaintDC dc(this);
	CRect rcClient;
	GetClientRect(&rcClient);
	dc.FillSolidRect(&rcClient, RGB(255, 255, 255));
	if (m_pResult == nullptr)
		return;
	m_bFontApproximate = false;
	// Draw in content space: the viewport origin is the scroll
	// offset, so every coordinate below is a screen pixel at the
	// current zoom.
	dc.SetViewportOrg(-m_scrollX, -m_scrollY);
	DrawGrid(dc);
	DrawShapes(dc);
	dc.SetViewportOrg(0, 0);
	if (m_bFontApproximate)
	{
		// Design section 6 (6): the font is an approximation when
		// the XML names no font or the named font is not installed.
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(kNoteColor);
		CFont* pOldFont = dc.SelectObject(CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT))));
		const String note = _("Font is shown approximately.");
		CRect rcNote(6, rcClient.bottom - 20, rcClient.right - 6, rcClient.bottom - 4);
		dc.DrawText(note.c_str(), -1, &rcNote, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
		dc.SelectObject(pOldFont);
	}
}

void CMerge20FormCanvas::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	UpdateScale();
	UpdateScrollBars();
	Invalidate();
}

void CMerge20FormCanvas::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* /*pScrollBar*/)
{
	CRect rcClient;
	GetClientRect(&rcClient);
	int x = m_scrollX;
	switch (nSBCode)
	{
	case SB_LINELEFT:
		x -= kScrollLinePx;
		break;
	case SB_LINERIGHT:
		x += kScrollLinePx;
		break;
	case SB_PAGELEFT:
		x -= rcClient.Width();
		break;
	case SB_PAGERIGHT:
		x += rcClient.Width();
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		x = static_cast<int>(nPos);
		break;
	case SB_LEFT:
		x = 0;
		break;
	case SB_RIGHT:
		x = INT_MAX;
		break;
	default:
		return;
	}
	ScrollTo(x, m_scrollY);
}

void CMerge20FormCanvas::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* /*pScrollBar*/)
{
	CRect rcClient;
	GetClientRect(&rcClient);
	int y = m_scrollY;
	switch (nSBCode)
	{
	case SB_LINEUP:
		y -= kScrollLinePx;
		break;
	case SB_LINEDOWN:
		y += kScrollLinePx;
		break;
	case SB_PAGEUP:
		y -= rcClient.Height();
		break;
	case SB_PAGEDOWN:
		y += rcClient.Height();
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		y = static_cast<int>(nPos);
		break;
	case SB_TOP:
		y = 0;
		break;
	case SB_BOTTOM:
		y = INT_MAX;
		break;
	default:
		return;
	}
	ScrollTo(m_scrollX, y);
}

BOOL CMerge20FormCanvas::OnMouseWheel(UINT /*nFlags*/, short zDelta, CPoint /*pt*/)
{
	const int lines = zDelta / WHEEL_DELTA;
	ScrollBy(0, -lines * kScrollLinesPerWheelNotch * kScrollLinePx);
	return TRUE;
}

void CMerge20FormCanvas::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetCapture();
	SetFocus();
	m_bDragging = true;
	m_bMoved = false;
	m_dragStart = point;
	m_dragStartScroll = CPoint(m_scrollX, m_scrollY);
	CWnd::OnLButtonDown(nFlags, point);
}

void CMerge20FormCanvas::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (m_bDragging)
	{
		m_bDragging = false;
		::ReleaseCapture();
		if (!m_bMoved && m_pResult != nullptr)
		{
			// A plain click selects a shape (HTML click rule).
			const double xmlX = Merge20FormCanvasToXml(static_cast<double>(point.x),
				m_scale, static_cast<double>(m_scrollX));
			const double xmlY = Merge20FormCanvasToXml(static_cast<double>(point.y),
				m_scale, static_cast<double>(m_scrollY));
			const std::wstring hit = Merge20FormCanvasHitTestKey(m_pResult->newShapes,
				m_pResult->oldShapes, m_pResult->changedShapeKeys, xmlX, xmlY);
			if (hit != m_selectedShapeKey)
			{
				m_selectedShapeKey = hit;
				Invalidate();
			}
			if (!hit.empty() && GetParent() != nullptr)
			{
				GetParent()->SendMessage(WM_COMMAND,
					MAKEWPARAM(GetDlgCtrlID(), MERGE20_FORM_CANVAS_SELCHANGE),
					reinterpret_cast<LPARAM>(GetSafeHwnd()));
			}
		}
	}
	CWnd::OnLButtonUp(nFlags, point);
}

void CMerge20FormCanvas::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bDragging)
	{
		const int dx = point.x - m_dragStart.x;
		const int dy = point.y - m_dragStart.y;
		if (std::abs(dx) > kMerge20FormCanvasDragThresholdPx ||
			std::abs(dy) > kMerge20FormCanvasDragThresholdPx)
		{
			m_bMoved = true;
		}
		if (m_bMoved)
			ScrollTo(m_dragStartScroll.x - dx, m_dragStartScroll.y - dy);
	}
	CWnd::OnMouseMove(nFlags, point);
}

void CMerge20FormCanvas::UpdateScale()
{
	CRect rcClient;
	GetClientRect(&rcClient);
	const double nativeW = (m_pResult != nullptr) ? m_pResult->nativeW : 1000.0;
	m_scale = Merge20FormCanvasScaleForMode(m_zoomMode, m_sliderPercent,
		nativeW, static_cast<double>(rcClient.Width()));
}

void CMerge20FormCanvas::UpdateScrollBars()
{
	CRect rcClient;
	GetClientRect(&rcClient);
	const std::pair<double, double> content =
		Merge20FormCanvasContentSize((m_pResult != nullptr) ? m_pResult->nativeW : 1000.0,
			(m_pResult != nullptr) ? m_pResult->nativeH : 800.0, m_scale);
	SCROLLINFO si = {};
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin = 0;
	si.nMax = (std::max)(0, static_cast<int>(std::lround(content.first)) - 1);
	si.nPage = (std::max)(1, rcClient.Width());
	si.nPos = m_scrollX;
	SetScrollInfo(SB_HORZ, &si, TRUE);
	si.nMax = (std::max)(0, static_cast<int>(std::lround(content.second)) - 1);
	si.nPage = (std::max)(1, rcClient.Height());
	si.nPos = m_scrollY;
	SetScrollInfo(SB_VERT, &si, TRUE);
	ClampScroll();
}

void CMerge20FormCanvas::ClampScroll()
{
	CRect rcClient;
	GetClientRect(&rcClient);
	const std::pair<double, double> content =
		Merge20FormCanvasContentSize((m_pResult != nullptr) ? m_pResult->nativeW : 1000.0,
			(m_pResult != nullptr) ? m_pResult->nativeH : 800.0, m_scale);
	const int maxX = (std::max)(0, static_cast<int>(std::lround(content.first)) - rcClient.Width());
	const int maxY = (std::max)(0, static_cast<int>(std::lround(content.second)) - rcClient.Height());
	m_scrollX = (std::max)(0, (std::min)(m_scrollX, maxX));
	m_scrollY = (std::max)(0, (std::min)(m_scrollY, maxY));
}

void CMerge20FormCanvas::ScrollBy(int dx, int dy)
{
	ScrollTo(m_scrollX + dx, m_scrollY + dy);
}

void CMerge20FormCanvas::ScrollTo(int x, int y)
{
	m_scrollX = x;
	m_scrollY = y;
	ClampScroll();
	SetScrollPos(SB_HORZ, m_scrollX, TRUE);
	SetScrollPos(SB_VERT, m_scrollY, TRUE);
	Invalidate();
}

CPoint CMerge20FormCanvas::XmlToScreen(double xmlX, double xmlY) const
{
	return CPoint(
		static_cast<int>(std::lround(Merge20FormCanvasToScreen(xmlX, m_scale, static_cast<double>(m_scrollX)))),
		static_cast<int>(std::lround(Merge20FormCanvasToScreen(xmlY, m_scale, static_cast<double>(m_scrollY)))));
}

void CMerge20FormCanvas::DrawGrid(CDC& dc) const
{
	const Merge20FormFileResult& result = *m_pResult;
	CPen penMajor(PS_SOLID, 1, kGridMajorColor);
	CPen penMinor(PS_SOLID, 1, kGridMinorColor);
	const int yTop = static_cast<int>(std::lround(
		Merge20FormCanvasToScreen(0.0, m_scale, static_cast<double>(m_scrollY))));
	const int yBottom = static_cast<int>(std::lround(
		Merge20FormCanvasToScreen(result.nativeH, m_scale, static_cast<double>(m_scrollY))));
	const int xLeft = static_cast<int>(std::lround(
		Merge20FormCanvasToScreen(0.0, m_scale, static_cast<double>(m_scrollX))));
	const int xRight = static_cast<int>(std::lround(
		Merge20FormCanvasToScreen(result.nativeW, m_scale, static_cast<double>(m_scrollX))));
	for (double xml = 0.0; xml < result.nativeW; xml += kMerge20FormCanvasGridStep)
	{
		const int sx = static_cast<int>(std::lround(
			Merge20FormCanvasToScreen(xml, m_scale, static_cast<double>(m_scrollX))));
		CPen* pOldPen = dc.SelectObject(Merge20FormCanvasGridLineIsMajor(xml) ? &penMajor : &penMinor);
		dc.MoveTo(sx, yTop);
		dc.LineTo(sx, yBottom);
		dc.SelectObject(pOldPen);
	}
	for (double xml = 0.0; xml < result.nativeH; xml += kMerge20FormCanvasGridStep)
	{
		const int sy = static_cast<int>(std::lround(
			Merge20FormCanvasToScreen(xml, m_scale, static_cast<double>(m_scrollY))));
		CPen* pOldPen = dc.SelectObject(Merge20FormCanvasGridLineIsMajor(xml) ? &penMajor : &penMinor);
		dc.MoveTo(xLeft, sy);
		dc.LineTo(xRight, sy);
		dc.SelectObject(pOldPen);
	}
}

void CMerge20FormCanvas::DrawShapes(CDC& dc)
{
	const Merge20FormFileResult& result = *m_pResult;
	// 1. Unchanged shapes (grey, from the new side).
	for (const Merge20FormShape& shape : result.newShapes)
	{
		if (!IsChangedKey(shape.key))
			DrawOneShape(dc, shape, true, false);
	}
	// 2. Changed shapes, old side first (red dashed), then new
	// side (green) so the new shape stays on top.
	for (const Merge20FormShape& shape : result.oldShapes)
	{
		if (IsChangedKey(shape.key))
			DrawOneShape(dc, shape, false, true);
	}
	for (const Merge20FormShape& shape : result.newShapes)
	{
		if (IsChangedKey(shape.key))
			DrawOneShape(dc, shape, true, true);
	}
	DrawSelectedOutline(dc);
}

void CMerge20FormCanvas::DrawOneShape(CDC& dc, const Merge20FormShape& shape, bool isNew, bool changed)
{
	CPoint pt1 = XmlToScreen(shape.x, shape.y);
	CPoint pt2 = XmlToScreen(shape.x2, shape.y2);
	CRect rc(pt1, pt2);
	rc.NormalizeRect();
	// HTML minimum drawn size (2 px unchanged, 6 px changed, in
	// canvas units); at least 1 screen pixel.
	const int minSize = changed ? 6 : 2;
	const int minPx = (std::max)(1, static_cast<int>(std::lround(minSize * m_scale)));
	if (rc.Width() < minPx)
		rc.right = rc.left + minPx;
	if (rc.Height() < minPx)
		rc.bottom = rc.top + minPx;

	COLORREF borderColor = kUnchangedColor;
	COLORREF fillColor = kUnchangedColor;
	int borderWidth = 1;
	bool dashed = false;
	if (changed)
	{
		if (isNew)
		{
			borderColor = kNewBorderColor;
			fillColor = kNewFillColor;
			borderWidth = 3;
		}
		else
		{
			borderColor = kOldBorderColor;
			fillColor = kOldFillColor;
			borderWidth = 2;
			dashed = true;
		}
	}

	if (shape.isLine)
	{
		// A line is a segment, not a rect (HTML rule).
		if (dashed)
		{
			HPEN hPen = CreateDashedPen(borderWidth, borderColor);
			HPEN hOldPen = static_cast<HPEN>(dc.SelectObject(hPen));
			dc.MoveTo(pt1);
			dc.LineTo(pt2);
			dc.SelectObject(hOldPen);
			::DeleteObject(hPen);
		}
		else
		{
			CPen pen(PS_SOLID, borderWidth, borderColor);
			CPen* pOldPen = dc.SelectObject(&pen);
			dc.MoveTo(pt1);
			dc.LineTo(pt2);
			dc.SelectObject(pOldPen);
		}
		return;
	}

	if (changed)
		dc.FillSolidRect(&rc, fillColor);
	CBrush* pOldBrush = dc.SelectObject(CBrush::FromHandle(static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH))));
	if (dashed)
	{
		HPEN hPen = CreateDashedPen(borderWidth, borderColor);
		HPEN hOldPen = static_cast<HPEN>(dc.SelectObject(hPen));
		dc.Rectangle(&rc);
		dc.SelectObject(hOldPen);
		::DeleteObject(hPen);
	}
	else
	{
		CPen pen(PS_SOLID, borderWidth, borderColor);
		CPen* pOldPen = dc.SelectObject(&pen);
		dc.Rectangle(&rc);
		dc.SelectObject(pOldPen);
	}
	dc.SelectObject(pOldBrush);

	if (!shape.text.empty())
	{
		const COLORREF textColor = !changed ? kUnchangedTextColor :
			(isNew ? kNewBorderColor : kOldBorderColor);
		DrawShapeText(dc, shape, textColor);
	}
}

void CMerge20FormCanvas::DrawShapeText(CDC& dc, const Merge20FormShape& shape, COLORREF color)
{
	if (shape.isLine || shape.text.empty())
		return;
	CPoint pt1 = XmlToScreen(shape.x, shape.y);
	CPoint pt2 = XmlToScreen(shape.x2, shape.y2);
	CRect rc(pt1, pt2);
	rc.NormalizeRect();
	rc.DeflateRect(2, 1, 2, 1);
	if (rc.Width() <= 0 || rc.Height() <= 0)
		return;
	const int dpi = dc.GetDeviceCaps(LOGPIXELSY);
	const int pixelHeight = (std::max)(1, static_cast<int>(std::lround(
		Merge20FormCanvasPointToPixels(shape.point, dpi) * m_scale)));
	CFont font;
	if (!CreateShapeFont(dc, shape, font, pixelHeight))
		return;
	CFont* pOldFont = dc.SelectObject(&font);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(color);
	// Largest N (characters including the ellipsis) whose
	// truncated text fits the rect width (binary search; the
	// width grows with N because TruncateText only extends).
	const int availW = rc.Width();
	auto TextWidth = [&](int chars) {
		const std::wstring shown = Merge20FormCanvasTruncateText(shape.text, static_cast<size_t>(chars));
		CSize size(0, 0);
		dc.GetTextExtentPoint32(shown.c_str(), static_cast<int>(shown.size()), &size);
		return size.cx;
	};
	int lo = 0;
	int hi = static_cast<int>(shape.text.size());
	while (lo < hi)
	{
		const int mid = (lo + hi + 1) / 2;
		if (TextWidth(mid) <= availW)
			lo = mid;
		else
			hi = mid - 1;
	}
	if (lo > 0)
	{
		const std::wstring shown = Merge20FormCanvasTruncateText(shape.text, static_cast<size_t>(lo));
		dc.DrawText(shown.c_str(), -1, &rc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
	}
	dc.SelectObject(pOldFont);
}

bool CMerge20FormCanvas::CreateShapeFont(CDC& dc, const Merge20FormShape& shape, CFont& font, int pixelHeight)
{
	// Default face from the stock GUI font.
	LOGFONT lf = {};
	::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
	lf.lfHeight = -pixelHeight;
	lf.lfWidth = 0;
	if (!shape.fontName.empty())
	{
		// Try the font named in the XML (design section 6 (6)):
		// when GDI substitutes another face, fall back.
		::wcsncpy_s(lf.lfFaceName, LF_FACESIZE, shape.fontName.c_str(), _TRUNCATE);
		if (font.CreateFontIndirect(&lf))
		{
			CFont* pOldFont = dc.SelectObject(&font);
			TCHAR actualFace[LF_FACESIZE] = {};
			dc.GetTextFace(LF_FACESIZE, actualFace);
			dc.SelectObject(pOldFont);
			if (actualFace[0] != 0 && ::_wcsicmp(actualFace, shape.fontName.c_str()) == 0)
				return true;
			font.DeleteObject();
		}
		m_bFontApproximate = true;
	}
	else
	{
		m_bFontApproximate = true;
	}
	// Fallback: the default face at the shape's point size.
	::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
	lf.lfHeight = -pixelHeight;
	lf.lfWidth = 0;
	return font.CreateFontIndirect(&lf) != FALSE;
}

void CMerge20FormCanvas::DrawSelectedOutline(CDC& dc)
{
	if (m_selectedShapeKey.empty() || m_pResult == nullptr)
		return;
	const Merge20FormShape* pShape = nullptr;
	for (const Merge20FormShape& shape : m_pResult->newShapes)
	{
		if (shape.key == m_selectedShapeKey)
		{
			pShape = &shape;
			break;
		}
	}
	if (pShape == nullptr)
	{
		for (const Merge20FormShape& shape : m_pResult->oldShapes)
		{
			if (shape.key == m_selectedShapeKey)
			{
				pShape = &shape;
				break;
			}
		}
	}
	if (pShape == nullptr)
		return;
	CPoint pt1 = XmlToScreen(pShape->x, pShape->y);
	CPoint pt2 = XmlToScreen(pShape->x2, pShape->y2);
	CRect rc(pt1, pt2);
	rc.NormalizeRect();
	rc.InflateRect(3, 3);
	CPen pen(PS_SOLID, 2, kSelectedColor);
	CBrush* pOldBrush = dc.SelectObject(CBrush::FromHandle(static_cast<HBRUSH>(::GetStockObject(NULL_BRUSH))));
	CPen* pOldPen = dc.SelectObject(&pen);
	dc.Rectangle(&rc);
	dc.SelectObject(pOldPen);
	dc.SelectObject(pOldBrush);
}

bool CMerge20FormCanvas::IsChangedKey(const std::wstring& key) const
{
	if (m_pResult == nullptr)
		return false;
	return std::find(m_pResult->changedShapeKeys.begin(),
		m_pResult->changedShapeKeys.end(), key) != m_pResult->changedShapeKeys.end();
}
