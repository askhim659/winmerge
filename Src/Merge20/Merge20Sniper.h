// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Sniper.h
 *
 * @brief Auto sniper view arithmetic (Merge20 Phase 4 D6, design
 * sections 3.6 and 5.2/D6, spec sections 3 (4/4), 4(g) and 5.4).
 *
 * Pure module (no MFC, no `_()`): the index arithmetic, font-size
 * clamping, pixel-to-line scroll conversion and drag-direction
 * decision for the sniper overlay. The same code is compiled into
 * WinMergeU and into the GoogleTest runner.
 *
 * Port of the HTML tool's sniper behaviour (17_v17.15.html):
 * `showSniper` (line 1487) opens the overlay on the double-clicked
 * diff, `snSyncScroll` (line 1480) syncs the three panels,
 * `snZoom(d)` (line 1453) steps the font by 2 within 14-28px
 * (`Math.max(14, Math.min(28, snFont + d))`), the drag/keyboard
 * navigation moves to the previous/next diff wrapping around both
 * ends, and `snFont`/`snLayout` (line 845) are saved. The C++
 * spelling differs in two places, both deliberate: the wrap-around
 * takes the already-computed next/previous significant diff (from
 * DiffList) plus the first/last one, instead of re-scanning the
 * list, and the pixel scroll amounts of spec section 4(g)
 * (120px per line step, 600px per page step) are converted to whole
 * lines by dividing by the panel's line height.
 */
#pragma once

/**
 * @brief Sniper panel layout (OPT_MERGE20_SNIPER_LAYOUT, D0 key:
 * 0 = three columns, 1 = three rows; HTML `snLayout` 'cols'/'rows').
 */
enum class Merge20SniperLayout
{
	Columns = 0, /**< Three panels side by side (default) */
	Rows = 1, /**< Three panels stacked */
};

/**
 * @brief Result of a drag gesture on a sniper panel.
 */
enum class Merge20SniperDrag
{
	None, /**< Not a horizontal swipe: too short, or vertical */
	Next, /**< Swipe left: move to the next diff */
	Prev, /**< Swipe right: move to the previous diff */
};

/** @brief Smallest sniper font size in points (HTML snFont floor). */
constexpr int Merge20SniperFontMin = 14;
/** @brief Largest sniper font size in points (HTML snFont ceiling). */
constexpr int Merge20SniperFontMax = 28;
/** @brief Default sniper font size in points (D0 key default). */
constexpr int Merge20SniperFontDefault = 20;
/** @brief Font step of the A-/A+ buttons in points (HTML snZoom(d)). */
constexpr int Merge20SniperFontStep = 2;
/** @brief Horizontal drag distance that counts as a swipe, in pixels. */
constexpr int Merge20SniperSwipePixels = 50;
/** @brief Line scroll step (Up/Down keys), in pixels (spec 4(g)). */
constexpr int Merge20SniperScrollLinePixels = 120;
/** @brief Page scroll step (PgUp/PgDn/Space), in pixels (spec 4(g)). */
constexpr int Merge20SniperScrollPagePixels = 600;

/**
 * @brief Next diff with wrap-around (spec section 4(g): the move
 * wraps from the last diff back to the first).
 * @param [in] nNext Next significant diff from DiffList, or -1 when
 *             the current diff is the last one.
 * @param [in] nFirst First significant diff, or -1 when there is none.
 * @return nNext when it is a diff, nFirst when wrapping, -1 when
 *         there is no diff at all.
 */
int Merge20SniperWrapNext(int nNext, int nFirst);

/**
 * @brief Previous diff with wrap-around (spec section 4(g): the move
 * wraps from the first diff back to the last).
 * @param [in] nPrev Previous significant diff from DiffList, or -1
 *             when the current diff is the first one.
 * @param [in] nLast Last significant diff, or -1 when there is none.
 * @return nPrev when it is a diff, nLast when wrapping, -1 when
 *         there is no diff at all.
 */
int Merge20SniperWrapPrev(int nPrev, int nLast);

/**
 * @brief Clamp a sniper font size to the 14-28px range.
 * @param [in] nFont Requested size in points.
 * @return The size clamped to [Merge20SniperFontMin,
 *         Merge20SniperFontMax].
 */
int Merge20SniperClampFont(int nFont);

/**
 * @brief Step the sniper font size by one A-/A+ press.
 * @param [in] nFont Current size in points.
 * @param [in] nDelta +Merge20SniperFontStep for A+, -step for A-.
 * @return The stepped size, clamped to [14, 28].
 */
int Merge20SniperStepFont(int nFont, int nDelta);

/**
 * @brief Convert a pixel scroll amount to whole lines.
 * @param [in] nPixels Pixel amount (120 for a line step, 600 for a
 *             page step, spec section 4(g)).
 * @param [in] nLineHeight Panel line height in pixels.
 * @return Whole lines to scroll: at least 1 for a positive amount,
 *         0 when there is nothing to scroll (zero/negative pixels
 *         or no line height).
 */
int Merge20SniperScrollLines(int nPixels, int nLineHeight);

/**
 * @brief Decide what a drag gesture on a sniper panel means.
 * @param [in] nDx Horizontal drag distance in pixels (positive when
 *             the pointer moved right).
 * @param [in] nDy Vertical drag distance in pixels.
 * @return Next when the pointer swiped left by at least 50px, Prev
 *         when it swiped right by at least 50px, None otherwise
 *         (too short, or the vertical movement dominates, which is
 *         a scroll drag instead).
 */
Merge20SniperDrag Merge20SniperDragAction(int nDx, int nDy);

/**
 * @brief Toggle the sniper layout between three columns and three rows.
 * @param [in] layout Current layout.
 * @return The other layout.
 */
Merge20SniperLayout Merge20SniperToggleLayout(Merge20SniperLayout layout);
