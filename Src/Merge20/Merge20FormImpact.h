// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormImpact.h
 *
 * @brief FORM/XML comparison (Wave 5 F5): impact analysis of
 * layout changes (design merge20-form-compare-design.md
 * section 6 (5) and section 10 F5).
 *
 * Pure module (no MFC, no `_()`): the same code is compiled into
 * WinMergeU and into the GoogleTest runner. All geometry is
 * judged on the NEW-side shape set, against the OLD-side shape
 * set for the "newly appeared" test. Warning texts are Japanese
 * (written with \u escapes in the .cpp, like Merge20FormDiff);
 * the two estimated checks (text overflow, font ripple) say so
 * in the text ("possibility", "approximate").
 *
 * Checks (design section 6 (5)):
 * - Overlap: two shapes that did not overlap on the old side
 *   overlap on the new side. The overlap area is compared with
 *   the SMALLER shape's area: 10% or more is a warning, below
 *   10% is a "minor overlap" note. Two `isLine` shapes (rules)
 *   never warn; a rule overlapping an item does warn.
 * - Text overflow: a shape's text does not fit its rectangle.
 *   The width is estimated from the font size (`point`) and the
 *   character count (half-width `point * 0.6`, full-width
 *   `point * 1.0`), so the warning says "possibility" and that
 *   the judgement is approximate. A shape that already
 *   overflowed on the old side does not warn again.
 * - Font ripple: an element whose font size (`point`) grew gets
 *   its text-width growth estimated; when the nearest
 *   neighbouring shape is closer than that growth, warn that
 *   the change may spill over to the neighbour ("possibility",
 *   approximate). Neighbours that already overlap are left to
 *   the overlap check.
 * - Page overflow: a shape exceeds the paper size when known,
 *   otherwise the canvas native size (`nativeW` / `nativeH`).
 *   A shape that already exceeded the limit on the old side
 *   does not warn again. NOTE: F1 computes the native size as
 *   the largest shape coordinate + 160, so no shape can exceed
 *   it; with an unknown paper size the canvas check can only
 *   fire when the caller passes a smaller limit (the unit tests
 *   do that). The live path is the paper size, which is unknown
 *   until a form declares it (design T-F-4).
 *
 * Shapes with estimated `x2` / `y2` (`isEstimated`, design
 * section 4.1) are excluded from every check: an estimated
 * rectangle must not produce a false warning. Degenerate
 * shapes (non-finite coordinates, zero or negative size) are
 * skipped the same way.
 */
#pragma once

#include <string>
#include <vector>
#include "Merge20/Merge20FormXml.h"

/** @brief Overlap warning threshold (design section 6 (5)): 10% of the smaller shape's area. */
constexpr double kMerge20FormImpactOverlapThreshold = 0.10;
/** @brief Estimated width of one half-width character (design section 6 (5)): `point * 0.6`. */
constexpr double kMerge20FormImpactNarrowCharFactor = 0.6;
/** @brief Estimated width of one full-width character (design section 6 (5)): `point * 1.0`. */
constexpr double kMerge20FormImpactWideCharFactor = 1.0;

/** @brief Kinds of impact warnings (design section 6 (5)). */
enum class Merge20FormImpactKind
{
	Overlap, /**< New overlap of 10% or more (layout-breaking). */
	MinorOverlap, /**< New overlap below 10% (note, not a warning). */
	TextOverflow, /**< Text may overflow its rectangle (approximate, layout-breaking). */
	FontRipple, /**< A font-size growth may spill over to a neighbour (approximate). */
	PageOverflow, /**< Shape exceeds the paper / canvas size (layout-breaking). */
};

/** @brief One impact warning. */
struct Merge20FormImpactWarning
{
	Merge20FormImpactKind kind = Merge20FormImpactKind::Overlap;
	std::wstring shapeKey; /**< Key of the shape the warning is about (stable key, design section 4.1). */
	std::wstring otherKey; /**< Key of the other shape (overlap / font ripple), empty otherwise. */
	double overlapRatio = 0.0; /**< Overlap area / smaller shape area (overlap kinds only). */
	double amount = 0.0; /**< Estimated width change / overflow amount in XML units. */
	std::wstring text; /**< Japanese warning sentence. */
};

/** @brief Outcome of the impact analysis of one file. */
struct Merge20FormImpactOutcome
{
	std::vector<Merge20FormImpactWarning> warnings; /**< All warnings, in detection order. */
	bool hasLayoutBreaking = false; /**< True when an Overlap / TextOverflow / PageOverflow warning exists. */
};

/**
 * @brief True when the character is full-width (CJK / full-width
 * forms). Everything else counts as half-width for the estimate.
 * @param [in] c Character to test.
 */
bool Merge20FormImpactIsWideChar(wchar_t c);

/**
 * @brief Estimate the width of a text at a font size (design
 * section 6 (5)): full-width characters count `point * 1.0`,
 * the rest `point * 0.6`. This is an estimate, never a font
 * metric (design section 6 (5), "approximate").
 * @param [in] text Text to measure (empty text is 0).
 * @param [in] point Font size in points.
 * @return Estimated width in XML units.
 */
double Merge20FormImpactEstimateTextWidth(const std::wstring& text, double point);

/**
 * @brief Overlap area of two shape rectangles (0 when they do
 * not overlap; touching edges is 0, not an overlap).
 * @param [in] a First shape.
 * @param [in] b Second shape.
 * @return Overlap area in square XML units.
 */
double Merge20FormImpactOverlapArea(const Merge20FormShape& a, const Merge20FormShape& b);

/**
 * @brief Run the impact analysis (design section 6 (5)) over one
 * file's shapes. The old side is needed only for the "newly
 * appeared" test; when it is empty (a file added, or a parse
 * failure) no warning is produced, because there is nothing to
 * compare against.
 * @param [in] oldShapes Old-side shapes.
 * @param [in] newShapes New-side shapes.
 * @param [in] nativeW Canvas native width (F1: largest coordinate + 160).
 * @param [in] nativeH Canvas native height.
 * @param [in] paperW Paper width when known, 0 when unknown.
 * @param [in] paperH Paper height when known, 0 when unknown.
 * @return The warnings and whether any of them is layout-breaking.
 */
Merge20FormImpactOutcome Merge20AnalyzeFormImpact(
	const std::vector<Merge20FormShape>& oldShapes,
	const std::vector<Merge20FormShape>& newShapes,
	double nativeW, double nativeH, double paperW, double paperH);

/**
 * @brief Run the impact analysis on a finished file result and
 * attach the warnings to it: each warning is appended to the
 * `impact` text of every diff record of the involved shapes
 * (the record's `element` is the shape's stable key), and the
 * file severity is raised to `LayoutBreaking` when a
 * layout-breaking warning exists. Call this after
 * `Merge20ApplyLargeChangeFlag` (F1), so the upgrade is not
 * overwritten by the large-change severity recompute.
 * @param [in,out] result Per-file result (F1 output).
 * @param [in] paperW Paper width when known, 0 when unknown.
 * @param [in] paperH Paper height when known, 0 when unknown.
 * @return The analysis outcome (the attached warnings).
 */
Merge20FormImpactOutcome Merge20ApplyFormImpact(
	Merge20FormFileResult& result, double paperW, double paperH);
