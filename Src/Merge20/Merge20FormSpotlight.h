// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormSpotlight.h
 *
 * @brief FORM/XML comparison (Wave 5 F4): the spotlight
 * selection and stepping logic (design
 * merge20-form-compare-design.md sections 2.7, 6 (4) and
 * 10 F4).
 *
 * This module is a port of the HTML tool's spotlight functions
 * (`focusDiffRecord` / `stepSpotlight` / `clearSpotlight` /
 * `scrollToShapeKey` / `scrollToChangedArea` /
 * `updateSpotlightBanner`, FROM_____1.html) for the parts that
 * need no window: which record is focused (per card), stepping
 * to the previous / next record with a visual (wrapping around),
 * the spotlight window around the old/new shapes, the scroll
 * offsets that centre a shape, the old -> new move arrow head
 * and the callout lines shown on the canvas.
 *
 * Pure module (no MFC, no `_()`): the same code is compiled
 * into WinMergeU and into the GoogleTest runner. The canvas
 * (Merge20FormCanvas) and the view (CFormCompareView) own the
 * drawing and the banner; they call these functions and keep
 * one Merge20FormSpotlightState per file card.
 */
#pragma once

#include <string>
#include <utility>
#include <vector>
#include "Merge20/Merge20FormXml.h"

/** @brief Spotlight selection of one FORM file card (design section 2.7). */
struct Merge20FormSpotlightState
{
	std::wstring shapeKey; /**< Key of the focused shape (the record's element). */
	int recordIndex = -1; /**< Index into Merge20FormFileResult::diffRecords, -1 when none. */
};

/** @brief Spotlight window padding in XML units (HTML: -36 / +36 horizontally). */
constexpr double kMerge20FormSpotlightPadX = 36.0;
/** @brief Spotlight window padding in XML units (HTML: -48 / +48 vertically). */
constexpr double kMerge20FormSpotlightPadY = 48.0;
/** @brief Spotlight window minimum width in XML units (HTML: 80). */
constexpr double kMerge20FormSpotlightMinWidth = 80.0;
/** @brief Spotlight window minimum height in XML units (HTML: 60). */
constexpr double kMerge20FormSpotlightMinHeight = 60.0;
/** @brief Move-arrow head length in XML units (HTML `drawArrow`: 12). */
constexpr double kMerge20FormSpotlightArrowHeadLen = 12.0;

/** @brief True when the state selects a record. */
bool Merge20FormSpotlightIsActive(const Merge20FormSpotlightState& state);

/**
 * @brief Focus a diff record (HTML `focusDiffRecord`): only a
 * record with `hasVisual` can be focused. Focusing the record
 * that is already focused re-focuses it (it is not cleared).
 * @param [in] records Diff records of the file.
 * @param [in] recordIndex Index of the record to focus.
 * @param [out] state Updated to the focused record.
 * @return False when the index is out of range or the record
 * has no visual (the state is left unchanged).
 */
bool Merge20FormSpotlightFocus(const std::vector<Merge20FormDiffRecord>& records,
	int recordIndex, Merge20FormSpotlightState& state);

/**
 * @brief Step to the previous / next spotlight target (HTML
 * `stepSpotlight`): only records with `hasVisual` are
 * candidates, and the walk wraps around the end of the list.
 * When nothing is focused yet, +1 starts at the first visual
 * record and -1 at the last one.
 * @param [in] records Diff records of the file.
 * @param [in] state Current spotlight state.
 * @param [in] dir +1 for the next change, -1 for the previous one.
 * @return The record index to focus, or -1 when no record has a visual.
 */
int Merge20FormSpotlightStep(const std::vector<Merge20FormDiffRecord>& records,
	const Merge20FormSpotlightState& state, int dir);

/** @brief Clear the spotlight (HTML `clearSpotlight`): back to the whole view. */
void Merge20FormSpotlightClear(Merge20FormSpotlightState& state);

/** @brief Index of the first record with a visual, or -1 when there is none. */
int Merge20FormSpotlightFirstVisual(const std::vector<Merge20FormDiffRecord>& records);

/** @brief Indices of the records with a visual, in record order. */
std::vector<int> Merge20FormSpotlightVisualIndices(const std::vector<Merge20FormDiffRecord>& records);

/** @brief Index of the shape with this key, or -1 when absent. */
int Merge20FormSpotlightFindShapeIndex(const std::vector<Merge20FormShape>& shapes,
	const std::wstring& key);

/** @brief Spotlight window in XML coordinates (HTML `renderCanvasWithSpotlight` section 4). */
struct Merge20FormSpotlightWindow
{
	double x = 0.0; /**< Left (XML units, clamped to 0). */
	double y = 0.0; /**< Top (XML units, clamped to 0). */
	double width = 0.0; /**< Width (XML units, at least 80 when there is a shape). */
	double height = 0.0; /**< Height (XML units, at least 60 when there is a shape). */
};

/**
 * @brief Window around the focused old/new shapes (HTML section
 * 4): the union of the two rects padded by 36 units
 * horizontally and 48 vertically, at least 80x60, clamped to
 * the origin. Both shapes may be null (an added or deleted
 * element has only one side); when both are null the window is
 * empty (width and height 0).
 */
Merge20FormSpotlightWindow Merge20FormSpotlightWindowRect(
	const Merge20FormShape* pOldShape, const Merge20FormShape* pNewShape);

/**
 * @brief Scroll offsets that centre an XML point in the
 * viewport (HTML `scrollToShapeKey`): `(centre + 32) * scale -
 * viewport / 2`, never below zero. The caller clamps the
 * result to the content size.
 * @param [in] centreXmlX Centre X in XML coordinates.
 * @param [in] centreXmlY Centre Y in XML coordinates.
 * @param [in] scale Effective canvas scale.
 * @param [in] viewportW Viewport width in screen pixels.
 * @param [in] viewportH Viewport height in screen pixels.
 * @return (scrollX, scrollY) in screen pixels.
 */
std::pair<int, int> Merge20FormSpotlightScrollToCenter(double centreXmlX, double centreXmlY,
	double scale, int viewportW, int viewportH);

/** @brief Arrow head of the old -> new move arrow (HTML `drawArrow`). */
struct Merge20FormSpotlightArrowHead
{
	double tipX = 0.0; /**< Arrow tip X (the new centre). */
	double tipY = 0.0; /**< Arrow tip Y (the new centre). */
	double base1X = 0.0; /**< First base corner of the head. */
	double base1Y = 0.0; /**< First base corner of the head. */
	double base2X = 0.0; /**< Second base corner of the head. */
	double base2Y = 0.0; /**< Second base corner of the head. */
};

/**
 * @brief Arrow head points for the move arrow from (x1, y1) to
 * (x2, y2) (HTML `drawArrow`: a 12-unit head, the two base
 * corners at +/-30 degrees from the shaft). When the two
 * points are closer than 2 units (the HTML rule for skipping
 * the arrow) the base corners equal the tip.
 */
Merge20FormSpotlightArrowHead Merge20FormSpotlightMakeArrowHead(
	double x1, double y1, double x2, double y2);

/**
 * @brief Callout lines for the focused element (HTML section
 * D): one line per record of the element, of the form
 * `・attr: old → new (desc)`. The header line (`Change
 * details: <key>`) is drawn by the canvas, not built here,
 * because it goes through `_()`.
 */
std::vector<std::wstring> Merge20FormSpotlightCalloutLines(
	const std::vector<Merge20FormDiffRecord>& elemRecords);
