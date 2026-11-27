// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Theme.cpp
 *
 * @brief Implementation of the WizMerge theme helpers (UI redesign
 * U0, design sections 2.1 and 4/U0). All RGB values come from the
 * accent table in design section 2.1; see Merge20Theme.h.
 *
 * MFC-dependent, so this file is registered only in Merge.vcxproj
 * (like Merge20DedBar.cpp) and not in UnitTests.vcxproj. U0 adds no
 * caller: nothing on screen changes until U1/U5 use these helpers.
 */
#include "StdAfx.h"
#include "Merge20/Merge20Theme.h"
#include "DarkModeLib.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace Merge20Theme
{
namespace
{
	/** @brief Blend one colour channel: start + (end - start) * t. */
	int BlendChannel(int nStart, int nEnd, int nPos, int nTotal)
	{
		if (nTotal <= 1)
			return nEnd;
		return nStart + (nEnd - nStart) * nPos / nTotal;
	}
} // namespace

bool IsDarkMode()
{
	return DarkMode::isEnabled();
}

COLORREF GetAccentColor(bool bDark)
{
	// Design section 2.1: light #2563EB, dark #38BDF8.
	return bDark ? RGB(56, 189, 248) : RGB(37, 99, 235);
}

COLORREF GetAccentGlowColor()
{
	// Design section 2.1: #00F2FE in both themes.
	return RGB(0, 242, 254);
}

COLORREF GetBrandBarStartColor(bool bDark)
{
	// Design section 2.1: light white, dark #090D16.
	return bDark ? RGB(9, 13, 22) : RGB(255, 255, 255);
}

COLORREF GetBrandBarEndColor(bool bDark)
{
	// Design section 2.1: light #E0E7FF, dark #1E1B4B.
	return bDark ? RGB(30, 27, 75) : RGB(224, 231, 255);
}

COLORREF GetBrandBarUnderlineColor(bool bDark)
{
	// Design section 2.1: the 2px underline is the primary accent
	// in light mode and cyan in dark mode.
	return bDark ? RGB(56, 189, 248) : RGB(37, 99, 235);
}

COLORREF GetReviewStateColor(ReviewState state, bool bDark)
{
	// Design section 2.1 (HTML .ded-rev colours).
	switch (state)
	{
	case ReviewState::DONE:
		return RGB(5, 150, 105); // #059669, both themes
	case ReviewState::HOLD:
		// Light #D97706, dark #CA8A04.
		return bDark ? RGB(202, 138, 4) : RGB(217, 119, 6);
	case ReviewState::SPECIAL:
		return RGB(225, 29, 72); // #E11D48, both themes
	case ReviewState::KEEP:
		// Light #0D9488, dark #0E7490.
		return bDark ? RGB(14, 116, 144) : RGB(13, 148, 136);
	case ReviewState::TODO:
	default:
		// Light #64748B, dark #475569.
		return bDark ? RGB(71, 85, 105) : RGB(100, 116, 139);
	}
}

COLORREF GetRiskHighColor(bool bDark)
{
	// Design section 2.1: light keeps the existing P1 value
	// RGB(192,0,0); dark #FB7185 is a candidate for U5 to confirm
	// on a real machine.
	return bDark ? RGB(251, 113, 133) : RGB(192, 0, 0);
}

void FillGradient(CDC* pDC, const CRect& rect,
	COLORREF crStart, COLORREF crEnd, bool bHorizontal)
{
	if (pDC == nullptr || rect.IsRectEmpty())
		return;
	const int nSteps = bHorizontal ? rect.Width() : rect.Height();
	if (nSteps <= 0)
		return;
	for (int i = 0; i < nSteps; ++i)
	{
		const COLORREF cr = RGB(
			BlendChannel(GetRValue(crStart), GetRValue(crEnd), i, nSteps),
			BlendChannel(GetGValue(crStart), GetGValue(crEnd), i, nSteps),
			BlendChannel(GetBValue(crStart), GetBValue(crEnd), i, nSteps));
		CRect strip = rect;
		if (bHorizontal)
		{
			strip.left = rect.left + i;
			strip.right = strip.left + 1;
		}
		else
		{
			strip.top = rect.top + i;
			strip.bottom = strip.top + 1;
		}
		pDC->FillSolidRect(strip, cr);
	}
}

void DrawBrandBar(CDC* pDC, const CRect& rect, bool bDark)
{
	if (pDC == nullptr || rect.IsRectEmpty())
		return;
	// Gradient background over the whole bar, then a 2px accent
	// underline along the bottom edge (design section 2.1).
	FillGradient(pDC, rect,
		GetBrandBarStartColor(bDark), GetBrandBarEndColor(bDark), true);
	CRect underline = rect;
	underline.top = underline.bottom - 2;
	if (underline.top < rect.top)
		underline.top = rect.top;
	pDC->FillSolidRect(underline, GetBrandBarUnderlineColor(bDark));
}

void DrawRoundedRect(CDC* pDC, const CRect& rect, int nRadius,
	COLORREF crFill, COLORREF crOutline)
{
	if (pDC == nullptr || rect.IsRectEmpty())
		return;
	CBrush brushFill(crFill);
	CPen penOutline;
	if (crOutline == CLR_NONE)
		penOutline.CreateStockObject(NULL_PEN);
	else
		penOutline.CreatePen(PS_SOLID, 1, crOutline);
	CBrush* pOldBrush = pDC->SelectObject(&brushFill);
	CPen* pOldPen = pDC->SelectObject(&penOutline);
	pDC->RoundRect(rect, CPoint(nRadius, nRadius));
	pDC->SelectObject(pOldPen);
	pDC->SelectObject(pOldBrush);
}

bool CreateBoldFont(CFont& font)
{
	NONCLIENTMETRICS ncm = { 0 };
	ncm.cbSize = sizeof(ncm);
	if (::SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
		sizeof(ncm), &ncm, 0) == 0)
		return false;
	ncm.lfMessageFont.lfWeight = FW_BOLD;
	return font.CreateFontIndirect(&ncm.lfMessageFont) != FALSE;
}
} // namespace Merge20Theme
