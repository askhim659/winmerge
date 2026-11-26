// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Slim.cpp
 *
 * @brief Implementation of the slim-list helpers (Merge20 Phase 4
 * D9, design sections 3.10 and 5.2/D9, spec sections 3 (4/4)
 * and 5.1).
 *
 * Pure module: no MFC, no `_()`, ASCII only.
 */
#include "pch.h"
#include "Merge20/Merge20Slim.h"

namespace Merge20Slim
{
std::vector<std::string> VisibleColumns()
{
	return { "Name", "Soutput", "Sreview" };
}

bool IsVisibleColumn(const std::string& regName)
{
	for (const std::string& name : VisibleColumns())
	{
		if (name == regName)
			return true;
	}
	return false;
}

std::string ColumnOrders()
{
	std::string orders = "0 |";
	for (const std::string& name : VisibleColumns())
	{
		orders += " ";
		orders += name;
	}
	return orders;
}

bool LabelNeedsWrap(int labelWidthPx)
{
	return labelWidthPx > kReviewColumnWidth;
}
} // namespace Merge20Slim
