// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FilterDeck.cpp
 *
 * @brief Implementation of the folder-compare filter deck
 * (H-FILTER, coverage matrix H-11/H-13/H-39). See
 * Merge20FilterDeck.h for the rules.
 *
 * This file stays ASCII: no Japanese literal is needed here (all
 * user-visible strings are composed by the deck bar through _()).
 */
#include "pch.h"
#include "Merge20/Merge20FilterDeck.h"

bool Merge20FilterDeckIsFileVisible(
	const Merge20FilterDeckState& state,
	Merge20FileStatus status,
	bool classified,
	ActionCategory action,
	ReviewState review)
{
	const int statusIndex = static_cast<int>(status);
	if (statusIndex < 0 || statusIndex >= 4 || !state.showStatus[statusIndex])
		return false;
	const int reviewIndex = static_cast<int>(review);
	if (reviewIndex < 0 || reviewIndex >= 5 || !state.showReview[reviewIndex])
		return false;
	// Unclassified files pass the action dimension (see the header):
	// the S4 classification only exists once the file was opened,
	// so filtering unclassified files out would hide every
	// not-yet-opened file as soon as one action box is off.
	if (classified)
	{
		const int actionIndex = static_cast<int>(action);
		if (actionIndex < 0 || actionIndex >= 7 || !state.showAction[actionIndex])
			return false;
	}
	return true;
}

void Merge20FilterDeckApplyActionPreset(Merge20FilterDeckState& state)
{
	// HTML acPreset (17_v17.15.html): selSt = {MODIFIED, OLD_ONLY},
	// selAc = {MERGE_CLEAN_OLD, REVIEW_MIXED, REVIEW_MOD_ONLY,
	// COPY_OLD_FILE}. The review group is untouched.
	const bool status[4] = { true, true, false, false };
	const bool action[7] = { true, true, true, false, true, false, false };
	for (int i = 0; i < 4; ++i)
		state.showStatus[i] = status[i];
	for (int i = 0; i < 7; ++i)
		state.showAction[i] = action[i];
}

void Merge20FilterDeckShowAll(Merge20FilterDeckState& state)
{
	for (int i = 0; i < 4; ++i)
		state.showStatus[i] = true;
	for (int i = 0; i < 7; ++i)
		state.showAction[i] = true;
	for (int i = 0; i < 5; ++i)
		state.showReview[i] = true;
}

void Merge20FilterDeckSyncStatusOn(
	Merge20FilterDeckState& state, Merge20FileStatus status)
{
	// HTML tgSt (17_v17.15.html): turning a category on adds its
	// counterpart; MODIFIED maps to four action categories, so no
	// single counterpart is added for it.
	switch (status)
	{
	case Merge20FileStatus::Identical:
		state.showAction[static_cast<int>(ActionCategory::NO_ACTION_IDENTICAL)] = true;
		break;
	case Merge20FileStatus::OldOnly:
		state.showAction[static_cast<int>(ActionCategory::COPY_OLD_FILE)] = true;
		break;
	case Merge20FileStatus::NewOnly:
		state.showAction[static_cast<int>(ActionCategory::SKIP_NEW_FILE)] = true;
		break;
	case Merge20FileStatus::Modified:
	default:
		break;
	}
}

void Merge20FilterDeckSyncActionOn(
	Merge20FilterDeckState& state, ActionCategory action)
{
	// HTML tgAc (17_v17.15.html): turning a category on adds its
	// counterpart; the four MODIFIED-side categories all map to
	// MODIFIED.
	switch (action)
	{
	case ActionCategory::NO_ACTION_IDENTICAL:
		state.showStatus[static_cast<int>(Merge20FileStatus::Identical)] = true;
		break;
	case ActionCategory::COPY_OLD_FILE:
		state.showStatus[static_cast<int>(Merge20FileStatus::OldOnly)] = true;
		break;
	case ActionCategory::SKIP_NEW_FILE:
		state.showStatus[static_cast<int>(Merge20FileStatus::NewOnly)] = true;
		break;
	default:
		state.showStatus[static_cast<int>(Merge20FileStatus::Modified)] = true;
		break;
	}
}

bool Merge20FilterDeckIsActive(const Merge20FilterDeckState& state)
{
	for (int i = 0; i < 4; ++i)
	{
		if (!state.showStatus[i])
			return true;
	}
	for (int i = 0; i < 7; ++i)
	{
		if (!state.showAction[i])
			return true;
	}
	for (int i = 0; i < 5; ++i)
	{
		if (!state.showReview[i])
			return true;
	}
	return false;
}
