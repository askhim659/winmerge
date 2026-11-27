// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20CloseWarn.cpp
 *
 * @brief Implementation of the undecided-items warning counts (B2
 * stage 1).
 *
 * The counting is a single pass with no special cases: a file
 * contributes at most 1 to undecidedFiles, each recorded diff
 * contributes at most 1 to undecidedDiffs, and highRisk counts
 * only undecided items in HIGH-risk files, so highRisk is always
 * <= undecidedFiles + undecidedDiffs.
 *
 * This file stays ASCII: no Japanese literal is needed here (the
 * count labels are composed by the dialog through `_()`).
 */
#include "pch.h"
#include "Merge20/Merge20CloseWarn.h"

Merge20CloseWarnCounts Merge20CloseWarnComputeCounts(const std::vector<Merge20CloseWarnFile>& files)
{
	Merge20CloseWarnCounts counts;
	counts.total = static_cast<int>(files.size());
	const int undecidedOrdinal = static_cast<int>(Merge20DiffConclusion::Undecided);
	for (const Merge20CloseWarnFile& file : files)
	{
		const bool isHighRisk = file.risk == RiskLevel::HIGH;
		if (file.review == ReviewState::TODO)
		{
			++counts.undecidedFiles;
			if (isHighRisk)
				++counts.highRisk;
		}
		for (const int conclusion : file.diffConclusions)
		{
			if (conclusion == undecidedOrdinal)
			{
				++counts.undecidedDiffs;
				if (isHighRisk)
					++counts.highRisk;
			}
		}
	}
	return counts;
}

bool Merge20CloseWarnHasUndecided(const Merge20CloseWarnCounts& counts)
{
	return counts.undecidedFiles > 0 || counts.undecidedDiffs > 0;
}
