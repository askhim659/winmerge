// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Sniper.cpp
 *
 * @brief Implementation of the auto sniper view arithmetic
 * (Merge20 Phase 4 D6).
 *
 * This file stays ASCII: it computes indices, sizes and directions
 * only and never formats user-visible text.
 */
#include "pch.h"
#include "Merge20/Merge20Sniper.h"

int Merge20SniperWrapNext(int nNext, int nFirst)
{
	if (nNext >= 0)
		return nNext;
	return nFirst;
}

int Merge20SniperWrapPrev(int nPrev, int nLast)
{
	if (nPrev >= 0)
		return nPrev;
	return nLast;
}

int Merge20SniperClampFont(int nFont)
{
	if (nFont < Merge20SniperFontMin)
		return Merge20SniperFontMin;
	if (nFont > Merge20SniperFontMax)
		return Merge20SniperFontMax;
	return nFont;
}

int Merge20SniperStepFont(int nFont, int nDelta)
{
	return Merge20SniperClampFont(nFont + nDelta);
}

int Merge20SniperScrollLines(int nPixels, int nLineHeight)
{
	if (nPixels <= 0 || nLineHeight <= 0)
		return 0;
	const int nLines = nPixels / nLineHeight;
	return nLines > 0 ? nLines : 1;
}

Merge20SniperDrag Merge20SniperDragAction(int nDx, int nDy)
{
	// A swipe is a mostly-horizontal drag of at least 50px (HTML
	// `initSniper`: `Math.abs(dx) >= 50 && Math.abs(dx) >
	// Math.abs(dy)`). When the vertical movement matches or beats
	// the horizontal one, the gesture is a scroll drag, not a move
	// to another diff.
	const int nAbsDx = nDx < 0 ? -nDx : nDx;
	const int nAbsDy = nDy < 0 ? -nDy : nDy;
	if (nAbsDx < Merge20SniperSwipePixels || nAbsDy >= nAbsDx)
		return Merge20SniperDrag::None;
	// Swipe left (the page-turn direction) goes to the next diff,
	// swipe right goes back to the previous one.
	return nDx < 0 ? Merge20SniperDrag::Next : Merge20SniperDrag::Prev;
}

Merge20SniperLayout Merge20SniperToggleLayout(Merge20SniperLayout layout)
{
	return layout == Merge20SniperLayout::Columns ?
		Merge20SniperLayout::Rows : Merge20SniperLayout::Columns;
}
