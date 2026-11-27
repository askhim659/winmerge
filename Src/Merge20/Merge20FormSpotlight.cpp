// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormSpotlight.cpp
 *
 * @brief Implementation of the FORM compare spotlight logic
 * (Wave 5 F4). Pure module: no MFC, no `_()`, ASCII only
 * (the bullet and arrow in the callout lines are written with
 * \u escapes, like Merge20FormDiff.cpp).
 */
#include "pch.h"
#include <algorithm>
#include <cmath>
#include "Merge20/Merge20FormCanvasGeom.h"
#include "Merge20/Merge20FormSpotlight.h"

namespace
{

	// Pi (MSVC has no M_PI without _USE_MATH_DEFINES).
	const double kPi = 3.14159265358979323846;

} // namespace

bool Merge20FormSpotlightIsActive(const Merge20FormSpotlightState& state)
{
	return state.recordIndex >= 0 && !state.shapeKey.empty();
}

bool Merge20FormSpotlightFocus(const std::vector<Merge20FormDiffRecord>& records,
	int recordIndex, Merge20FormSpotlightState& state)
{
	if (recordIndex < 0 || recordIndex >= static_cast<int>(records.size()))
		return false;
	const Merge20FormDiffRecord& record = records[static_cast<size_t>(recordIndex)];
	if (!record.hasVisual)
		return false;
	// Re-focusing the same record is a no-op for the state, but
	// still succeeds (the view scrolls to it again).
	state.shapeKey = record.element;
	state.recordIndex = recordIndex;
	return true;
}

int Merge20FormSpotlightStep(const std::vector<Merge20FormDiffRecord>& records,
	const Merge20FormSpotlightState& state, int dir)
{
	const int len = static_cast<int>(records.size());
	if (len == 0)
		return -1;
	if (dir != 1 && dir != -1)
		dir = 1;
	if (state.recordIndex < 0)
	{
		// Nothing focused yet: start at the first (next) or the
		// last (previous) visual record.
		if (dir > 0)
			return Merge20FormSpotlightFirstVisual(records);
		const std::vector<int> visual = Merge20FormSpotlightVisualIndices(records);
		return visual.empty() ? -1 : visual.back();
	}
	// HTML `stepSpotlight`: walk from the current record, wrap around.
	for (int step = 1; step <= len; ++step)
	{
		const int candidate = ((state.recordIndex + dir * step) % len + len) % len;
		if (records[static_cast<size_t>(candidate)].hasVisual)
			return candidate;
	}
	return -1;
}

void Merge20FormSpotlightClear(Merge20FormSpotlightState& state)
{
	state.shapeKey.clear();
	state.recordIndex = -1;
}

int Merge20FormSpotlightFirstVisual(const std::vector<Merge20FormDiffRecord>& records)
{
	for (size_t i = 0; i < records.size(); ++i)
	{
		if (records[i].hasVisual)
			return static_cast<int>(i);
	}
	return -1;
}

std::vector<int> Merge20FormSpotlightVisualIndices(const std::vector<Merge20FormDiffRecord>& records)
{
	std::vector<int> indices;
	for (size_t i = 0; i < records.size(); ++i)
	{
		if (records[i].hasVisual)
			indices.push_back(static_cast<int>(i));
	}
	return indices;
}

int Merge20FormSpotlightFindShapeIndex(const std::vector<Merge20FormShape>& shapes,
	const std::wstring& key)
{
	for (size_t i = 0; i < shapes.size(); ++i)
	{
		if (shapes[i].key == key)
			return static_cast<int>(i);
	}
	return -1;
}

Merge20FormSpotlightWindow Merge20FormSpotlightWindowRect(
	const Merge20FormShape* pOldShape, const Merge20FormShape* pNewShape)
{
	Merge20FormSpotlightWindow window;
	if (pOldShape == nullptr && pNewShape == nullptr)
		return window;
	double minX = 0.0;
	double minY = 0.0;
	double maxX = 0.0;
	double maxY = 0.0;
	bool first = true;
	const Merge20FormShape* shapes[2] = { pOldShape, pNewShape };
	for (const Merge20FormShape* pShape : shapes)
	{
		if (pShape == nullptr)
			continue;
		const double shapeMinX = (std::min)(pShape->x, pShape->x2);
		const double shapeMaxX = (std::max)(pShape->x, pShape->x2);
		const double shapeMinY = (std::min)(pShape->y, pShape->y2);
		const double shapeMaxY = (std::max)(pShape->y, pShape->y2);
		if (first)
		{
			minX = shapeMinX;
			minY = shapeMinY;
			maxX = shapeMaxX;
			maxY = shapeMaxY;
			first = false;
		}
		else
		{
			minX = (std::min)(minX, shapeMinX);
			minY = (std::min)(minY, shapeMinY);
			maxX = (std::max)(maxX, shapeMaxX);
			maxY = (std::max)(maxY, shapeMaxY);
		}
	}
	minX -= kMerge20FormSpotlightPadX;
	minY -= kMerge20FormSpotlightPadY;
	maxX += kMerge20FormSpotlightPadX;
	maxY += kMerge20FormSpotlightPadY;
	window.x = (std::max)(0.0, minX);
	window.y = (std::max)(0.0, minY);
	window.width = (std::max)(kMerge20FormSpotlightMinWidth, maxX - window.x);
	window.height = (std::max)(kMerge20FormSpotlightMinHeight, maxY - window.y);
	return window;
}

std::pair<int, int> Merge20FormSpotlightScrollToCenter(double centreXmlX, double centreXmlY,
	double scale, int viewportW, int viewportH)
{
	const double scrollX = (centreXmlX + kMerge20FormCanvasOriginOffset) * scale -
		static_cast<double>(viewportW) / 2.0;
	const double scrollY = (centreXmlY + kMerge20FormCanvasOriginOffset) * scale -
		static_cast<double>(viewportH) / 2.0;
	return std::make_pair(
		(std::max)(0, static_cast<int>(std::lround(scrollX))),
		(std::max)(0, static_cast<int>(std::lround(scrollY))));
}

Merge20FormSpotlightArrowHead Merge20FormSpotlightArrowHead(
	double x1, double y1, double x2, double y2)
{
	Merge20FormSpotlightArrowHead head;
	head.tipX = x2;
	head.tipY = y2;
	head.base1X = x2;
	head.base1Y = y2;
	head.base2X = x2;
	head.base2Y = y2;
	const double dx = x2 - x1;
	const double dy = y2 - y1;
	const double dist = std::hypot(dx, dy);
	if (dist < 2.0)
		return head;
	const double angle = std::atan2(dy, dx);
	head.base1X = x2 - kMerge20FormSpotlightArrowHeadLen * std::cos(angle - kPi / 6.0);
	head.base1Y = y2 - kMerge20FormSpotlightArrowHeadLen * std::sin(angle - kPi / 6.0);
	head.base2X = x2 - kMerge20FormSpotlightArrowHeadLen * std::cos(angle + kPi / 6.0);
	head.base2Y = y2 - kMerge20FormSpotlightArrowHeadLen * std::sin(angle + kPi / 6.0);
	return head;
}

std::vector<std::wstring> Merge20FormSpotlightCalloutLines(
	const std::vector<Merge20FormDiffRecord>& elemRecords)
{
	std::vector<std::wstring> lines;
	for (const Merge20FormDiffRecord& record : elemRecords)
	{
		lines.push_back(L"\u30fb" + record.attr + L": " + record.oldVal +
			L" \u2192 " + record.newVal + L" (" + record.desc + L")");
	}
	return lines;
}
