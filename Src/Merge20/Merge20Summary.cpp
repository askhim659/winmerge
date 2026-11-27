// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Summary.cpp
 *
 * @brief Implementation of the end-of-session summary counts (B9).
 *
 * The counting is a single pass with no special cases: the three
 * existence kinds (modified / old-only / new-only) are disjoint
 * because the caller derives them from one diffcode, and identical
 * is the complement of hasDiff, so modified + oldOnly + newOnly
 * + identical == total always holds.
 *
 * This file stays ASCII: no Japanese literal is needed here (the
 * count labels are composed by the dialog through `_()`).
 */
#include "pch.h"
#include "Merge20/Merge20Summary.h"

Merge20SummaryCounts Merge20SummaryComputeCounts(const std::vector<Merge20SummaryFile>& files)
{
	Merge20SummaryCounts counts;
	counts.total = static_cast<int>(files.size());
	for (const Merge20SummaryFile& file : files)
	{
		if (file.hasDiff)
			++counts.diffCount;
		else
			++counts.identical;
		if (file.isModified)
			++counts.modified;
		if (file.isOldOnly)
			++counts.oldOnly;
		if (file.isNewOnly)
			++counts.newOnly;
		if (file.review == ReviewState::SPECIAL)
			++counts.special;
		if (file.review == ReviewState::TODO)
			++counts.undecided;
		if (file.includeInOutput)
			++counts.outputTarget;
	}
	return counts;
}

bool Merge20SummaryHasUndecided(const Merge20SummaryCounts& counts)
{
	return counts.undecided > 0;
}
