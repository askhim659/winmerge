// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Theme.h
 *
 * @brief WizMerge visual foundation: shared theme constants and
 * drawing helpers (UI redesign U0, design sections 2.1 and 4/U0).
 *
 * All RGB values are taken from the accent table in design section
 * 2.1 (ported from the HTML tool "Merge20" v17.15). Every colour
 * has a light and a dark variant; callers pass the result of
 * Merge20Theme::IsDarkMode() (which wraps DarkMode::isEnabled()).
 *
 * U0 draws nothing by itself: no existing code calls these helpers
 * yet, so adding this module changes no on-screen output. U1 (brand
 * bar) and U5 (bar accent unification) are the first callers.
 *
 * This header assumes StdAfx.h is included first (same as
 * Merge20DedBar.h): CDC/CFont/CRect come from the MFC headers there.
 * The file is ASCII-only, like the other Merge20 sources.
 */
#pragma once

#include "Merge20/Merge20Types.h"

namespace Merge20Theme
{
	/** @brief True when the app currently runs in dark mode. */
	bool IsDarkMode();

	/**
	 * @brief Primary WizMerge accent colour.
	 * @param [in] bDark True for the dark theme.
	 * @return Light: #2563EB, dark: #38BDF8 (design section 2.1).
	 */
	COLORREF GetAccentColor(bool bDark);

	/**
	 * @brief Bright cyan glow colour, used next to the accent in
	 * dark mode (the glowing dot of the HTML top bar).
	 * @return #00F2FE in both themes.
	 */
	COLORREF GetAccentGlowColor();

	/**
	 * @brief Brand bar gradient start colour.
	 * @param [in] bDark True for the dark theme.
	 * @return Light: white, dark: #090D16 (design section 2.1).
	 */
	COLORREF GetBrandBarStartColor(bool bDark);

	/**
	 * @brief Brand bar gradient end colour.
	 * @param [in] bDark True for the dark theme.
	 * @return Light: #E0E7FF, dark: #1E1B4B (design section 2.1).
	 */
	COLORREF GetBrandBarEndColor(bool bDark);

	/**
	 * @brief Brand bar 2px underline colour.
	 * @param [in] bDark True for the dark theme.
	 * @return Light: the primary accent, dark: cyan
	 * (design section 2.1).
	 */
	COLORREF GetBrandBarUnderlineColor(bool bDark);

	/**
	 * @brief Pill colour of one review state (HTML .ded-rev colours).
	 * @param [in] state Review state (TODO/DONE/HOLD/SPECIAL/KEEP).
	 * @param [in] bDark True for the dark theme.
	 * @return TODO light #64748B / dark #475569, DONE #059669,
	 * HOLD light #D97706 / dark #CA8A04, SPECIAL #E11D48,
	 * KEEP light #0D9488 / dark #0E7490 (design section 2.1).
	 */
	COLORREF GetReviewStateColor(ReviewState state, bool bDark);

	/**
	 * @brief Text colour for "high" risk (kept from P1).
	 * @param [in] bDark True for the dark theme.
	 * @return Light: RGB(192,0,0) (existing P1 value, unchanged).
	 * Dark: #FB7185 (candidate; U5 confirms it on a real machine).
	 */
	COLORREF GetRiskHighColor(bool bDark);

	/**
	 * @brief Fill a rectangle with a linear two-colour gradient.
	 * @param [in] pDC Device context to draw into.
	 * @param [in] rect Rectangle to fill.
	 * @param [in] crStart Colour at the left (horizontal) or top
	 * (vertical) edge.
	 * @param [in] crEnd Colour at the opposite edge.
	 * @param [in] bHorizontal True for a left-to-right gradient,
	 * false for top-to-bottom.
	 */
	void FillGradient(CDC* pDC, const CRect& rect,
		COLORREF crStart, COLORREF crEnd, bool bHorizontal = true);

	/**
	 * @brief Paint a brand bar background: horizontal gradient plus
	 * a 2px accent underline at the bottom edge.
	 * @param [in] pDC Device context to draw into.
	 * @param [in] rect Bar rectangle (the underline occupies its
	 * bottom 2 pixels).
	 * @param [in] bDark True for the dark theme.
	 */
	void DrawBrandBar(CDC* pDC, const CRect& rect, bool bDark);

	/**
	 * @brief Draw a filled rounded rectangle (colour pill shape).
	 * @param [in] pDC Device context to draw into.
	 * @param [in] rect Rectangle to draw.
	 * @param [in] nRadius Corner radius in pixels.
	 * @param [in] crFill Fill colour.
	 * @param [in] crOutline Outline colour, or CLR_NONE for no
	 * outline.
	 */
	void DrawRoundedRect(CDC* pDC, const CRect& rect, int nRadius,
		COLORREF crFill, COLORREF crOutline);

	/**
	 * @brief Create a bold version of the default GUI font
	 * (NONCLIENTMETRICS message font, FW_BOLD) for wordmarks.
	 * @param [out] font Receives the created font. The caller owns
	 * it and must destroy it (CFont destructor does that).
	 * @return True when the font was created.
	 */
	bool CreateBoldFont(CFont& font);
} // namespace Merge20Theme
