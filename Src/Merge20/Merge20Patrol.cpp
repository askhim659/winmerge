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
#include <algorithm>
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

Merge20RiskRank Merge20RiskRankOfRisk(RiskLevel risk)
{
	switch (risk)
	{
	case RiskLevel::HIGH:
		return Merge20RiskRank::HIGH;
	case RiskLevel::MEDIUM:
		return Merge20RiskRank::MEDIUM;
	case RiskLevel::NONE:
	default:
		return Merge20RiskRank::NONE;
	}
}

Merge20RiskRank Merge20RiskRankOfFile(bool classified, RiskLevel risk)
{
	if (!classified)
		return Merge20RiskRank::UNCLASSIFIED;
	return Merge20RiskRankOfRisk(risk);
}

std::vector<int> Merge20BuildRiskOrder(const std::vector<Merge20RiskRank>& ranks, const std::vector<bool>& isTarget)
{
	std::vector<int> order;
	for (size_t i = 0; i < isTarget.size(); ++i)
	{
		if (isTarget[i])
			order.push_back(static_cast<int>(i));
	}
	// Stable sort by rank: High, Medium, None, unclassified, with
	// list/diff order kept within one rank. A rank past the ranks
	// vector counts as unclassified (see the header comment).
	std::stable_sort(order.begin(), order.end(),
		[&ranks](int a, int b)
		{
			const Merge20RiskRank ra = a < static_cast<int>(ranks.size()) ?
				ranks[static_cast<size_t>(a)] : Merge20RiskRank::UNCLASSIFIED;
			const Merge20RiskRank rb = b < static_cast<int>(ranks.size()) ?
				ranks[static_cast<size_t>(b)] : Merge20RiskRank::UNCLASSIFIED;
			return ra < rb;
		});
	return order;
}

int Merge20FindNextInOrder(const std::vector<int>& order, int current, int direction, bool wrap)
{
	if (direction != 1 && direction != -1)
		return -1;
	if (order.empty())
		return -1;
	// Position of the current row/diff in the order (-1 when it is
	// not a patrol target: it counts as no current row, like the
	// out-of-range current in Merge20FindNextTarget).
	int pos = -1;
	for (size_t i = 0; i < order.size(); ++i)
	{
		if (order[i] == current)
		{
			pos = static_cast<int>(i);
			break;
		}
	}
	if (pos < 0)
		return direction > 0 ? order.front() : order.back();
	const int nextPos = pos + direction;
	if (nextPos >= 0 && nextPos < static_cast<int>(order.size()))
		return order[static_cast<size_t>(nextPos)];
	if (!wrap)
		return -1;
	// Wrap around: the first entry forward, the last one backward.
	// With a single target that is the current row/diff itself,
	// exactly like Merge20FindNextTarget.
	return direction > 0 ? order.front() : order.back();
}

int Merge20OrdinalInOrder(const std::vector<int>& order, int current)
{
	for (size_t i = 0; i < order.size(); ++i)
	{
		if (order[i] == current)
			return static_cast<int>(i) + 1;
	}
	return 0;
}
