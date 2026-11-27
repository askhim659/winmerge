// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20HighRiskRemain.cpp
 *
 * @brief Implementation of the B4 remainder-display decision.
 *
 * One comparison, no special cases: the line is visible exactly
 * while the fresh count is above zero, and the control is
 * touched only when the visibility flips or, while visible, the
 * count changed. A hidden line with a fresh zero count is not a
 * change (the initial state is hidden), so the first Refresh of
 * a compare with no high-risk remainder does nothing.
 *
 * This file stays ASCII: no Japanese literal is needed here (the
 * remainder text is composed by the panel bar through `_()`).
 */
#include "pch.h"
#include "Merge20/Merge20HighRiskRemain.h"

Merge20HighRiskRemainUpdate Merge20HighRiskRemainDecideUpdate(bool lastVisible, int lastHighRisk, int highRisk)
{
	Merge20HighRiskRemainUpdate update;
	update.highRisk = highRisk;
	update.visible = highRisk > 0;
	update.changed = (update.visible != lastVisible) ||
		(update.visible && highRisk != lastHighRisk);
	return update;
}
