// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormImpact.cpp
 *
 * @brief Implementation of the FORM/XML impact analysis (Wave 5
 * F5, design section 6 (5)).
 *
 * This file stays ASCII: the Japanese warning literals are
 * written with \u escapes (see the note in Merge20FormDiff.h).
 * The conversion from the authored UTF-8 text is mechanical; the
 * escaped form is what is committed.
 */
#include "pch.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Merge20/Merge20FormImpact.h"

namespace
{

	// Format a number with one decimal place at most (`12.5`, `12`).
	std::wstring FormatNumber(double value)
	{
		const double rounded = std::round(value * 10.0) / 10.0;
		if (rounded == std::trunc(rounded))
			return std::to_wstring(static_cast<long long>(rounded));
		wchar_t buf[64];
		std::swprintf(buf, 64, L"%.1f", rounded);
		return std::wstring(buf);
	}

	// Display name of a shape: the `name` attribute, or the tag.
	std::wstring DisplayName(const Merge20FormShape& shape)
	{
		return shape.name.empty() ? shape.tag : shape.name;
	}

	// True when the shape can be judged at all: finite rectangle,
	// positive size, and real (not estimated) x2 / y2 (design
	// section 6 (5): estimated shapes are excluded).
	bool IsJudgable(const Merge20FormShape& shape)
	{
		if (shape.isEstimated)
			return false;
		if (!std::isfinite(shape.x) || !std::isfinite(shape.y) ||
			!std::isfinite(shape.x2) || !std::isfinite(shape.y2))
			return false;
		return shape.x2 > shape.x && shape.y2 > shape.y;
	}

	// Area of a shape rectangle (0 for a degenerate shape).
	double ShapeArea(const Merge20FormShape& shape)
	{
		if (!std::isfinite(shape.x) || !std::isfinite(shape.y) ||
			!std::isfinite(shape.x2) || !std::isfinite(shape.y2))
			return 0.0;
		const double w = shape.x2 - shape.x;
		const double h = shape.y2 - shape.y;
		if (w <= 0.0 || h <= 0.0)
			return 0.0;
		return w * h;
	}

	// Find a shape by key, or nullptr.
	const Merge20FormShape* FindShape(const std::vector<Merge20FormShape>& shapes,
		const std::wstring& key)
	{
		for (const Merge20FormShape& shape : shapes)
		{
			if (shape.key == key)
				return &shape;
		}
		return nullptr;
	}

	// Edge-to-edge distance of two rectangles (0 when they overlap
	// or touch). Used to find the nearest neighbour of a shape.
	double RectGap(const Merge20FormShape& a, const Merge20FormShape& b)
	{
		const double dx = (std::max)(0.0, (std::max)(b.x - a.x2, a.x - b.x2));
		const double dy = (std::max)(0.0, (std::max)(b.y - a.y2, a.y - b.y2));
		return std::sqrt(dx * dx + dy * dy);
	}

	// True when the shape's text already overflowed its rectangle
	// (the old-side test for the text-overflow check).
	bool TextOverflows(const Merge20FormShape& shape)
	{
		if (!IsJudgable(shape) || shape.text.empty())
			return false;
		const double width = shape.x2 - shape.x;
		return Merge20FormImpactEstimateTextWidth(shape.text, shape.point) > width;
	}

	// True when the shape already exceeds the limit (the old-side
	// test for the page-overflow check).
	bool ExceedsLimit(const Merge20FormShape& shape, double limitW, double limitH)
	{
		if (!IsJudgable(shape))
			return false;
		return shape.x < 0.0 || shape.y < 0.0 || shape.x2 > limitW || shape.y2 > limitH;
	}

} // namespace

bool Merge20FormImpactIsWideChar(wchar_t c)
{
	const unsigned int ch = static_cast<unsigned int>(c);
	// Full-width space, CJK radicals / ideographs / syllables,
	// full-width forms, Hiragana / Katakana / Hangul ranges.
	return ch == 0x3000 ||
		(ch >= 0x1100 && ch <= 0x115F) ||
		(ch >= 0x2E80 && ch <= 0x9FFF) ||
		(ch >= 0xAC00 && ch <= 0xD7AF) ||
		(ch >= 0xF900 && ch <= 0xFAFF) ||
		(ch >= 0xFE30 && ch <= 0xFE6F) ||
		(ch >= 0xFF00 && ch <= 0xFF60);
}

double Merge20FormImpactEstimateTextWidth(const std::wstring& text, double point)
{
	if (text.empty() || !(point > 0.0) || !std::isfinite(point))
		return 0.0;
	double width = 0.0;
	for (wchar_t c : text)
	{
		width += point * (Merge20FormImpactIsWideChar(c) ?
			kMerge20FormImpactWideCharFactor : kMerge20FormImpactNarrowCharFactor);
	}
	return width;
}

double Merge20FormImpactOverlapArea(const Merge20FormShape& a, const Merge20FormShape& b)
{
	const double left = (std::max)(a.x, b.x);
	const double top = (std::max)(a.y, b.y);
	const double right = (std::min)(a.x2, b.x2);
	const double bottom = (std::min)(a.y2, b.y2);
	if (right <= left || bottom <= top)
		return 0.0;
	return (right - left) * (bottom - top);
}

Merge20FormImpactOutcome Merge20AnalyzeFormImpact(
	const std::vector<Merge20FormShape>& oldShapes,
	const std::vector<Merge20FormShape>& newShapes,
	double nativeW, double nativeH, double paperW, double paperH)
{
	Merge20FormImpactOutcome outcome;
	// Nothing to compare against (a file added, or a parse
	// failure): the checks below are all "newly appeared"
	// tests, so they stay silent.
	if (oldShapes.empty())
		return outcome;
	const bool paperKnown = paperW > 0.0 && paperH > 0.0 && std::isfinite(paperW) && std::isfinite(paperH);
	const double limitW = paperKnown ? paperW : nativeW;
	const double limitH = paperKnown ? paperH : nativeH;

	// (1) Overlap: pairs that did not overlap on the old side
	// but overlap on the new side. Two rules (isLine) never warn;
	// a rule overlapping an item does.
	for (size_t i = 0; i < newShapes.size(); ++i)
	{
		const Merge20FormShape& a = newShapes[i];
		if (!IsJudgable(a))
			continue;
		for (size_t j = i + 1; j < newShapes.size(); ++j)
		{
			const Merge20FormShape& b = newShapes[j];
			if (!IsJudgable(b))
				continue;
			if (a.isLine && b.isLine)
				continue;
			const double area = Merge20FormImpactOverlapArea(a, b);
			if (area <= 0.0)
				continue;
			// Already overlapping on the old side: not new.
			const Merge20FormShape* oldA = FindShape(oldShapes, a.key);
			const Merge20FormShape* oldB = FindShape(oldShapes, b.key);
			if (oldA != nullptr && oldB != nullptr &&
				Merge20FormImpactOverlapArea(*oldA, *oldB) > 0.0)
				continue;
			const double smaller = (std::min)(ShapeArea(a), ShapeArea(b));
			if (smaller <= 0.0)
				continue;
			const double ratio = area / smaller;
			Merge20FormImpactWarning warning;
			warning.shapeKey = a.key;
			warning.otherKey = b.key;
			warning.overlapRatio = ratio;
			if (ratio >= kMerge20FormImpactOverlapThreshold)
			{
				warning.kind = Merge20FormImpactKind::Overlap;
				warning.text = L"\u300c" + DisplayName(a) + L"\u300d\u3068\u300c" + DisplayName(b) +
					L"\u300d\u304c\u91cd\u306a\u3063\u3066\u3044\u307e\u3059\uff08\u91cd\u306a\u308a\u306f\u5c0f\u3055\u3044\u65b9\u306e\u56f3\u5f62\u306e\u7d04" +
					FormatNumber(ratio * 100.0) + L"%\uff09";
				outcome.hasLayoutBreaking = true;
			}
			else
			{
				warning.kind = Merge20FormImpactKind::MinorOverlap;
				warning.text = L"\u300c" + DisplayName(a) + L"\u300d\u3068\u300c" + DisplayName(b) +
					L"\u300d\u304c\u308f\u305a\u304b\u306b\u91cd\u306a\u3063\u3066\u3044\u307e\u3059\uff08\u8efd\u5fae\u306a\u91cd\u306a\u308a\uff1a\u7d04" +
					FormatNumber(ratio * 100.0) + L"%\uff09";
			}
			outcome.warnings.push_back(warning);
		}
	}

	// (2) Text overflow: estimated text width against the
	// rectangle width. Only when the old side did not overflow.
	// (3) Font ripple: a grown font size, checked in the same
	// pass over the new shapes.
	for (const Merge20FormShape& shape : newShapes)
	{
		if (!IsJudgable(shape) || shape.text.empty())
			continue;
		const double width = shape.x2 - shape.x;
		const double estWidth = Merge20FormImpactEstimateTextWidth(shape.text, shape.point);
		const Merge20FormShape* oldShape = FindShape(oldShapes, shape.key);
		if (estWidth > width && (oldShape == nullptr || !TextOverflows(*oldShape)))
		{
			Merge20FormImpactWarning warning;
			warning.kind = Merge20FormImpactKind::TextOverflow;
			warning.shapeKey = shape.key;
			warning.amount = estWidth - width;
			warning.text = L"\u300c" + DisplayName(shape) +
				L"\u300d\u306e\u30c6\u30ad\u30b9\u30c8\u304c\u67a0\u306b\u53ce\u307e\u3089\u306a\u3044\u53ef\u80fd\u6027\u3042\u308a\uff08\u63a8\u5b9a\u30c6\u30ad\u30b9\u30c8\u5e45\u7d04" +
				FormatNumber(estWidth) + L"\u5358\u4f4d\u306b\u5bfe\u3057\u67a0\u5e45" + FormatNumber(width) +
				L"\u5358\u4f4d\u3002\u5e45\u306f\u30d5\u30a9\u30f3\u30c8\u30b5\u30a4\u30ba\u304b\u3089\u306e\u63a8\u5b9a\u306e\u305f\u3081\u8fd1\u4f3c\u3067\u3059\uff09";
			outcome.warnings.push_back(warning);
			outcome.hasLayoutBreaking = true;
		}
		if (oldShape != nullptr && shape.point > oldShape->point)
		{
			const double growth = Merge20FormImpactEstimateTextWidth(shape.text, shape.point) -
				Merge20FormImpactEstimateTextWidth(shape.text, oldShape->point);
			if (growth > 0.0)
			{
				// Nearest neighbour (edge-to-edge gap); a
				// neighbour that already overlaps is left to
				// the overlap check.
				const Merge20FormShape* nearest = nullptr;
				double nearestGap = 0.0;
				for (const Merge20FormShape& other : newShapes)
				{
					if (&other == &shape || !IsJudgable(other))
						continue;
					const double gap = RectGap(shape, other);
					if (gap <= 0.0)
						continue;
					if (nearest == nullptr || gap < nearestGap)
					{
						nearest = &other;
						nearestGap = gap;
					}
				}
				if (nearest != nullptr && nearestGap < growth)
				{
					Merge20FormImpactWarning warning;
					warning.kind = Merge20FormImpactKind::FontRipple;
					warning.shapeKey = shape.key;
					warning.otherKey = nearest->key;
					warning.amount = growth;
					warning.text = L"\u300c" + DisplayName(shape) +
						L"\u300d\u306e\u30d5\u30a9\u30f3\u30c8\u30b5\u30a4\u30ba\u5909\u66f4\u306b\u3088\u308a\u30c6\u30ad\u30b9\u30c8\u5e45\u304c\u7d04" +
						FormatNumber(growth) + L"\u5358\u4f4d\u5e83\u304c\u308a\u3001\u96a3\u63a5\u3059\u308b\u300c" +
						DisplayName(*nearest) +
						L"\u300d\u306b\u6ce2\u53ca\u3059\u308b\u53ef\u80fd\u6027\u3042\u308a\uff08\u63a8\u5b9a\u306b\u3088\u308b\u8fd1\u4f3c\u5224\u5b9a\u3067\u3059\uff09";
					outcome.warnings.push_back(warning);
				}
			}
		}
	}

	// (4) Page overflow: the shape exceeds the paper size when
	// known, otherwise the canvas native size. Only when the
	// old side did not exceed the same limit.
	for (const Merge20FormShape& shape : newShapes)
	{
		if (!IsJudgable(shape))
			continue;
		if (!ExceedsLimit(shape, limitW, limitH))
			continue;
		const Merge20FormShape* oldShape = FindShape(oldShapes, shape.key);
		if (oldShape != nullptr && ExceedsLimit(*oldShape, limitW, limitH))
			continue;
		std::wstring detail;
		if (shape.x2 > limitW)
			detail = L"\u53f3\u7aef" + FormatNumber(shape.x2) + L"\u5358\u4f4d";
		else if (shape.y2 > limitH)
			detail = L"\u4e0b\u7aef" + FormatNumber(shape.y2) + L"\u5358\u4f4d";
		else if (shape.x < 0.0)
			detail = L"\u5de6\u7aef" + FormatNumber(shape.x) + L"\u5358\u4f4d";
		else
			detail = L"\u4e0a\u7aef" + FormatNumber(shape.y) + L"\u5358\u4f4d";
		Merge20FormImpactWarning warning;
		warning.kind = Merge20FormImpactKind::PageOverflow;
		warning.shapeKey = shape.key;
		warning.amount = shape.x2 > limitW ? shape.x2 - limitW :
			(shape.y2 > limitH ? shape.y2 - limitH :
			(shape.x < 0.0 ? -shape.x : -shape.y));
		warning.text = paperKnown ?
			(L"\u300c" + DisplayName(shape) + L"\u300d\u304c\u7528\u7d19\u306e\u7bc4\u56f2\u3092\u8d85\u3048\u3066\u3044\u307e\u3059\uff08" + detail + L"\uff09") :
			(L"\u300c" + DisplayName(shape) + L"\u300d\u304c\u30ad\u30e3\u30f3\u30d0\u30b9\u306e\u7bc4\u56f2\u3092\u8d85\u3048\u3066\u3044\u307e\u3059\uff08" + detail + L"\uff09");
		outcome.warnings.push_back(warning);
		outcome.hasLayoutBreaking = true;
	}

	return outcome;
}

Merge20FormImpactOutcome Merge20ApplyFormImpact(
	Merge20FormFileResult& result, double paperW, double paperH)
{
	Merge20FormImpactOutcome outcome = Merge20AnalyzeFormImpact(
		result.oldShapes, result.newShapes,
		result.nativeW, result.nativeH, paperW, paperH);
	for (const Merge20FormImpactWarning& warning : outcome.warnings)
	{
		for (Merge20FormDiffRecord& record : result.diffRecords)
		{
			if (record.element != warning.shapeKey && record.element != warning.otherKey)
				continue;
			if (record.impact.empty())
				record.impact = warning.text;
			else
				record.impact += L" " + warning.text;
		}
	}
	if (outcome.hasLayoutBreaking)
		result.severity = Merge20FormSeverity::LayoutBreaking;
	return outcome;
}
