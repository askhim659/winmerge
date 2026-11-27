// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCanvasGeom.h
 *
 * @brief FORM/XML comparison (Wave 5 F3): pure geometry of the
 * visual-preview canvas (design merge20-form-compare-design.md
 * sections 3.4, 6 (6) and 10 F3).
 *
 * The MFC canvas control (Merge20FormCanvas) draws with GDI; this
 * module holds the parts that need no window so GoogleTest can pin
 * them: the four zoom modes and their scale, the XML coordinate
 * <-> screen pixel transform (the HTML OFFSET of 32 units,
 * FROM_____1.html `renderCanvasWithSpotlight`), the shape hit
 * test (changed shapes only, smallest area wins, design
 * section 3.4), the text truncation with an ellipsis (design
 * section 6 (6)) and the point-to-pixel font conversion.
 *
 * Pure module (no MFC, no `_()`): the same code is compiled into
 * WinMergeU and into the GoogleTest runner. It works on the F0
 * data model (Merge20FormShape).
 */
#pragma once

#include <string>
#include <utility>
#include <vector>
#include "Merge20/Merge20FormXml.h"

/** @brief Origin margin of the canvas (HTML OFFSET = 32 XML units). */
constexpr double kMerge20FormCanvasOriginOffset = 32.0;
/** @brief Grid step in XML units (HTML: 100 units). */
constexpr double kMerge20FormCanvasGridStep = 100.0;
/** @brief Major grid step in XML units (HTML: 500 units, darker line). */
constexpr double kMerge20FormCanvasGridMajorStep = 500.0;
/** @brief Zoom slider range in percent (HTML zoom-slider 10-200). */
constexpr double kMerge20FormCanvasMinZoomPercent = 10.0;
/** @brief Zoom slider range in percent (HTML zoom-slider 10-200). */
constexpr double kMerge20FormCanvasMaxZoomPercent = 200.0;
/** @brief Click tolerance around a shape, in XML units (HTML: rect expanded by 8). */
constexpr double kMerge20FormCanvasHitTolerance = 8.0;
/** @brief Drag threshold in screen pixels (HTML: moved when over 4 px). */
constexpr int kMerge20FormCanvasDragThresholdPx = 4;

/** @brief Zoom modes (design section 2.8: 100% / 25% / fit / slider). */
enum class Merge20FormZoomMode
{
	Percent100, /**< 100% (native 1:1). */
	Percent25, /**< 25% (paper-size view). */
	Fit, /**< Fit the viewport width, at most 100% (HTML `setCanvasScale` 'fit'). */
	Slider, /**< Slider percent (10-200%). */
};

/**
 * @brief Clamp a slider percent to the 10-200% range.
 * @param [in] percent Requested percent.
 * @return The percent clamped to [10, 200].
 */
double Merge20FormCanvasClampSliderPercent(double percent);

/**
 * @brief Effective scale of a zoom mode (design section 2.8).
 * Fit mirrors the HTML rule (`setCanvasScale` 'fit'):
 * `min(1, max(300, viewportW - 40) / nativeW)` (width only).
 * @param [in] mode Zoom mode.
 * @param [in] sliderPercent Slider percent (used for Slider only).
 * @param [in] nativeW Canvas native width in XML units (0 returns 1.0).
 * @param [in] viewportW Viewport width in screen pixels.
 * @return The scale (1.0 = native 1:1, one XML unit is one pixel).
 */
double Merge20FormCanvasScaleForMode(Merge20FormZoomMode mode, double sliderPercent,
	double nativeW, double viewportW);

/**
 * @brief XML coordinate to screen pixel (HTML: `(xml + 32) * scale`).
 * @param [in] xmlCoord XML coordinate.
 * @param [in] scale Effective scale.
 * @param [in] scroll Scroll offset in screen pixels.
 * @return Screen pixel.
 */
double Merge20FormCanvasToScreen(double xmlCoord, double scale, double scroll);

/**
 * @brief Screen pixel to XML coordinate (HTML click rule:
 * `(screen / scale) - 32`).
 * @param [in] screenCoord Screen pixel.
 * @param [in] scale Effective scale.
 * @param [in] scroll Scroll offset in screen pixels.
 * @return XML coordinate.
 */
double Merge20FormCanvasToXml(double screenCoord, double scale, double scroll);

/**
 * @brief Scaled content size in screen pixels: the native size
 * plus the 32-unit origin margin, times the scale.
 * @param [in] nativeW Canvas native width in XML units.
 * @param [in] nativeH Canvas native height in XML units.
 * @param [in] scale Effective scale.
 * @return (width, height) in screen pixels.
 */
std::pair<double, double> Merge20FormCanvasContentSize(double nativeW, double nativeH, double scale);

/**
 * @brief True when the grid line at this XML coordinate is a major
 * (500-unit) line: the line sits at `32 + k * 100` and
 * `(coord - 32) % 500 == 0` (HTML grid rule).
 * @param [in] xmlCoord XML coordinate of the grid line.
 */
bool Merge20FormCanvasGridLineIsMajor(double xmlCoord);

/**
 * @brief Hit test for a shape click (HTML `click` handler, design
 * section 3.4): only changed shapes are candidates, each rect is
 * expanded by the 8-unit tolerance, and when several shapes
 * overlap the one with the smallest clickable (expanded) area
 * wins; a tie keeps the first in order (new shapes before old
 * ones, document order inside each side).
 * @param [in] newShapes New-side shapes.
 * @param [in] oldShapes Old-side shapes.
 * @param [in] changedShapeKeys Keys of the changed shapes.
 * @param [in] xmlX Click X in XML coordinates.
 * @param [in] xmlY Click Y in XML coordinates.
 * @return The key of the hit shape, or an empty string when no
 * changed shape contains the click.
 */
std::wstring Merge20FormCanvasHitTestKey(const std::vector<Merge20FormShape>& newShapes,
	const std::vector<Merge20FormShape>& oldShapes,
	const std::vector<std::wstring>& changedShapeKeys, double xmlX, double xmlY);

/**
 * @brief Truncate text to at most `maxChars` characters, adding an
 * ellipsis when text is cut (design section 6 (6): cut at the
 * rect width and append "..."). The ellipsis itself counts as one
 * of the `maxChars` characters, so the result never exceeds
 * `maxChars` characters.
 * @param [in] text Text to truncate.
 * @param [in] maxChars Maximum characters of the result (0 returns empty).
 * @return The text unchanged when it fits, otherwise the first
 * `maxChars - 1` characters plus U+2026 (a single U+2026 when
 * `maxChars` is 1).
 */
std::wstring Merge20FormCanvasTruncateText(const std::wstring& text, size_t maxChars);

/**
 * @brief Font point size to pixels: `point * dpi / 72` (design
 * section 6 (6)). A missing, non-finite or non-positive point
 * size falls back to 10pt; a missing or non-positive DPI falls
 * back to 96.
 * @param [in] point Font size in points.
 * @param [in] dpi Screen DPI (LOGPIXELSY).
 * @return Font height in pixels.
 */
double Merge20FormCanvasPointToPixels(double point, int dpi);
