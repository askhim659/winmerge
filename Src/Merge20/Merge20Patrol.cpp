// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Patrol.cpp
 *
 * @brief Implementation of the unreviewed-only patrol index arithmetic
 * (Merge20 Phase 4 D3).
 *
 * This file stays ASCII: it computes indices only and never formats
 * user-visible text.
 */
#include "pch.h"
#include "Merge20/Merge20Patrol.h"

namespace
{

/** @brief First target index, or -1 when there is none. */
int FindFirstTarget(const std::vector<bool>& isTarget)
{
	for (size_t i = 0; i < isTarget.size(); ++i)
	{
		if (isTarget[i])
			return static_cast<int>(i);
	}
	return -1;
}

/** @brief Last target index, or -1 when there is none. */
int FindLastTarget(const std::vector<bool>& isTarget)
{
	for (size_t i = isTarget.size(); i-- > 0;)
	{
		if (isTarget[i])
			return static_cast<int>(i);
	}
	return -1;
}

} // namespace

int Merge20FindNextTarget(const std::vector<bool>& isTarget, int current, int direction)
{
	if (direction != 1 && direction != -1)
		return -1;
	if (isTarget.empty())
		return -1;
	// No current row (or an out-of-range one): the next target is the
	// first one, the previous target is the last one (HTML navFile
	// with i == -1 behaves the same way).
	if (current < 0 || current >= static_cast<int>(isTarget.size()))
		return direction > 0 ? FindFirstTarget(isTarget) : FindLastTarget(isTarget);
	if (direction > 0)
	{
		for (int i = current + 1; i < static_cast<int>(isTarget.size()); ++i)
		{
			if (isTarget[static_cast<size_t>(i)])
				return i;
		}
		// Wrap around to the first target (HTML navFile: cand[0]).
		return FindFirstTarget(isTarget);
	}
	for (int i = current - 1; i >= 0; --i)
	{
		if (isTarget[static_cast<size_t>(i)])
			return i;
	}
	// Wrap around to the last target (HTML navFile: cand[cand.length - 1]).
	return FindLastTarget(isTarget);
}

bool Merge20HasOtherTarget(const std::vector<bool>& isTarget, int current)
{
	const int next = Merge20FindNextTarget(isTarget, current, 1);
	return next != -1 && next != current;
}

int Merge20CountTargets(const std::vector<bool>& isTarget)
{
	int count = 0;
	for (size_t i = 0; i < isTarget.size(); ++i)
	{
		if (isTarget[i])
			++count;
	}
	return count;
}

int Merge20OrdinalOfTarget(const std::vector<bool>& isTarget, int current)
{
	if (current < 0 || current >= static_cast<int>(isTarget.size()) ||
		!isTarget[static_cast<size_t>(current)])
		return 0;
	int ordinal = 0;
	for (int i = 0; i <= current; ++i)
	{
		if (isTarget[static_cast<size_t>(i)])
			++ordinal;
	}
	return ordinal;
}
