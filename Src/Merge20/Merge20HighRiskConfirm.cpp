// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20HighRiskConfirm.cpp
 *
 * @brief Implementation of the high-risk confirmation decision
 * (Merge20 best-workflow B14).
 *
 * This file stays ASCII: it computes decisions only and never
 * formats user-visible text.
 */
#include "pch.h"
#include "Merge20/Merge20HighRiskConfirm.h"

bool Merge20HighRiskConfirmIsDestructive(Merge20DiffConclusion conclusion)
{
	switch (conclusion)
	{
	case Merge20DiffConclusion::UseOld:
	case Merge20DiffConclusion::UseNew:
	case Merge20DiffConclusion::Reject:
		return true;
	case Merge20DiffConclusion::UseBoth:
	case Merge20DiffConclusion::Keep:
	case Merge20DiffConclusion::Undecided:
	default:
		return false;
	}
}

bool Merge20HighRiskConfirmNeedsConfirm(RiskLevel risk, Merge20DiffConclusion conclusion, bool alreadyDecided)
{
	if (alreadyDecided)
		return false;
	return risk == RiskLevel::HIGH && Merge20HighRiskConfirmIsDestructive(conclusion);
}

std::vector<int> Merge20HighRiskConfirmCollect(const std::vector<Merge20HighRiskConfirmTarget>& targets)
{
	std::vector<int> needConfirm;
	for (const Merge20HighRiskConfirmTarget& target : targets)
	{
		if (target.diffIndex < 0)
			continue;
		if (!Merge20HighRiskConfirmNeedsConfirm(target.risk, target.conclusion, target.alreadyDecided))
			continue;
		needConfirm.push_back(target.diffIndex);
	}
	return needConfirm;
}

bool Merge20HighRiskConfirmFileCycleNeedsConfirm(RiskLevel risk, ReviewState fromState, ReviewState toState)
{
	// Only the step that declares a high-risk file done is
	// guarded: it turns the output checkbox ON (S2 linkage) and
	// is the one Space step a wrong key can commit by mistake.
	// The other steps of the cycle only defer or exclude the file.
	return risk == RiskLevel::HIGH &&
		fromState == ReviewState::TODO && toState == ReviewState::DONE;
}
