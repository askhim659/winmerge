// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCanvasGeom.cpp
 *
 * @brief Implementation of the FORM compare canvas geometry
 * (Wave 5 F3). Pure module: no MFC, no `_()`, ASCII only.
 */
#include "pch.h"
#include <algorithm>
#include <cmath>
#include "Merge20/Merge20FormCanvasGeom.h"

namespace
{

	// True when `key` is one of the changed shape keys.
	bool IsChangedShapeKey(const std::vector<std::wstring>& changedShapeKeys, const std::wstring& key)
	{
		return std::find(changedShapeKeys.begin(), changedShapeKeys.end(), key) != changedShapeKeys.end();
	}

	// Clickable area of a shape: the rect expanded by the hit
	// tolerance on every side (a line keeps its length, so a long
	// line is still a larger target than a small rect).
	double HitArea(const Merge20FormShape& shape)
	{
		const double w = std::abs(shape.x2 - shape.x) + 2.0 * kMerge20FormCanvasHitTolerance;
		const double h = std::abs(shape.y2 - shape.y) + 2.0 * kMerge20FormCanvasHitTolerance;
		return w * h;
	}

	// True when the click (XML coordinates) falls inside the
	// shape's rect expanded by the hit tolerance.
	bool HitContains(const Merge20FormShape& shape, double xmlX, double xmlY)
	{
		const double minX = (std::min)(shape.x, shape.x2) - kMerge20FormCanvasHitTolerance;
		const double maxX = (std::max)(shape.x, shape.x2) + kMerge20FormCanvasHitTolerance;
		const double minY = (std::min)(shape.y, shape.y2) - kMerge20FormCanvasHitTolerance;
		const double maxY = (std::max)(shape.y, shape.y2) + kMerge20FormCanvasHitTolerance;
		return xmlX >= minX && xmlX <= maxX && xmlY >= minY && xmlY <= maxY;
	}

} // namespace

double Merge20FormCanvasClampSliderPercent(double percent)
{
	if (std::isnan(percent))
		return 100.0;
	return (std::max)(kMerge20FormCanvasMinZoomPercent,
		(std::min)(kMerge20FormCanvasMaxZoomPercent, percent));
}

double Merge20FormCanvasScaleForMode(Merge20FormZoomMode mode, double sliderPercent,
	double nativeW, double viewportW)
{
	switch (mode)
	{
	case Merge20FormZoomMode::Percent100:
		return 1.0;
	case Merge20FormZoomMode::Percent25:
		return 0.25;
	case Merge20FormZoomMode::Fit:
	{
		if (nativeW <= 0.0)
			return 1.0;
		const double availW = (std::max)(300.0, viewportW - 40.0);
		return (std::min)(1.0, availW / nativeW);
	}
	case Merge20FormZoomMode::Slider:
	default:
		return Merge20FormCanvasClampSliderPercent(sliderPercent) / 100.0;
	}
}

double Merge20FormCanvasToScreen(double xmlCoord, double scale, double scroll)
{
	return (xmlCoord + kMerge20FormCanvasOriginOffset) * scale - scroll;
}

double Merge20FormCanvasToXml(double screenCoord, double scale, double scroll)
{
	return (screenCoord + scroll) / scale - kMerge20FormCanvasOriginOffset;
}

std::pair<double, double> Merge20FormCanvasContentSize(double nativeW, double nativeH, double scale)
{
	return std::make_pair(
		(nativeW + kMerge20FormCanvasOriginOffset) * scale,
		(nativeH + kMerge20FormCanvasOriginOffset) * scale);
}

bool Merge20FormCanvasGridLineIsMajor(double xmlCoord)
{
	const double offset = std::fmod(xmlCoord - kMerge20FormCanvasOriginOffset,
		kMerge20FormCanvasGridMajorStep);
	return offset == 0.0;
}

std::wstring Merge20FormCanvasHitTestKey(const std::vector<Merge20FormShape>& newShapes,
	const std::vector<Merge20FormShape>& oldShapes,
	const std::vector<std::wstring>& changedShapeKeys, double xmlX, double xmlY)
{
	std::wstring bestKey;
	double bestArea = 0.0;
	bool found = false;
	// New shapes first, then old ones (HTML click order); the
	// smallest clickable area wins, a tie keeps the first.
	const std::vector<Merge20FormShape>* sides[2] = { &newShapes, &oldShapes };
	for (const std::vector<Merge20FormShape>* pSide : sides)
	{
		for (const Merge20FormShape& shape : *pSide)
		{
			if (!IsChangedShapeKey(changedShapeKeys, shape.key))
				continue;
			if (!HitContains(shape, xmlX, xmlY))
				continue;
			const double area = HitArea(shape);
			if (!found || area < bestArea)
			{
				found = true;
				bestArea = area;
				bestKey = shape.key;
			}
		}
	}
	return found ? bestKey : std::wstring();
}

std::wstring Merge20FormCanvasTruncateText(const std::wstring& text, size_t maxChars)
{
	if (maxChars == 0)
		return std::wstring();
	if (text.size() <= maxChars)
		return text;
	if (maxChars == 1)
		return L"\u2026";
	return text.substr(0, maxChars - 1) + L"\u2026";
}

double Merge20FormCanvasPointToPixels(double point, int dpi)
{
	const double safePoint = (std::isfinite(point) && point > 0.0) ? point : 10.0;
	const double safeDpi = (dpi > 0) ? static_cast<double>(dpi) : 96.0;
	return safePoint * safeDpi / 72.0;
}
