// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20FilterDeck_test.cpp
 *
 * @brief Merge20 (H-FILTER) unit tests for the filter deck
 * (Merge20FilterDeckIsFileVisible, the action preset, the HTML
 * tgSt/tgAc coupling, and the active check). The HTML ground
 * truth is 17_v17.15.html: filtFiles (selSt && selAc), acPreset
 * (existence {MODIFIED, OLD_ONLY}, action {MERGE_CLEAN_OLD,
 * REVIEW_MIXED, REVIEW_MOD_ONLY, COPY_OLD_FILE}), tgSt/tgAc
 * (add-only coupling).
 *
 * This file is ASCII; the module is MFC-free, so these tests run
 * in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20FilterDeck.h"
#include "Merge20/Merge20Types.h"

namespace
{
	bool Visible(
		const Merge20FilterDeckState& state,
		Merge20FileStatus status = Merge20FileStatus::Modified,
		bool classified = true,
		ActionCategory action = ActionCategory::MERGE_CLEAN_OLD,
		ReviewState review = ReviewState::TODO)
	{
		return Merge20FilterDeckIsFileVisible(state, status, classified, action, review);
	}
}

TEST(Merge20FilterDeck, DefaultShowsEveryFile)
{
	// All three groups fully on: every combination passes.
	const Merge20FilterDeckState state = Merge20FilterDeckDefaultState();
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::REVIEW_MIXED, ReviewState::DONE));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::OldOnly, true, ActionCategory::COPY_OLD_FILE, ReviewState::TODO));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::NewOnly, true, ActionCategory::SKIP_NEW_FILE, ReviewState::HOLD));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Identical, true, ActionCategory::NO_ACTION_IDENTICAL, ReviewState::KEEP));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, false, ActionCategory::NO_ACTION_IDENTICAL, ReviewState::SPECIAL));
	EXPECT_FALSE(Merge20FilterDeckIsActive(state));
}

TEST(Merge20FilterDeck, StatusOffHidesThatStatusOnly)
{
	// HTML tgSt('IDENTICAL') off: identical files go, the rest stay.
	Merge20FilterDeckState state = Merge20FilterDeckDefaultState();
	state.showStatus[static_cast<int>(Merge20FileStatus::Identical)] = false;
	EXPECT_FALSE(Visible(state, Merge20FileStatus::Identical, true, ActionCategory::NO_ACTION_IDENTICAL));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::MERGE_CLEAN_OLD));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::OldOnly, true, ActionCategory::COPY_OLD_FILE));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::NewOnly, true, ActionCategory::SKIP_NEW_FILE));
	EXPECT_TRUE(Merge20FilterDeckIsActive(state));
}

TEST(Merge20FilterDeck, ActionOffHidesClassifiedOnly)
{
	// Turning one action category off hides classified files of
	// that category, but unclassified files still pass (they have
	// no classification yet; hiding them would collapse the list
	// to opened files only).
	Merge20FilterDeckState state = Merge20FilterDeckDefaultState();
	state.showAction[static_cast<int>(ActionCategory::COPY_OLD_FILE)] = false;
	EXPECT_FALSE(Visible(state, Merge20FileStatus::OldOnly, true, ActionCategory::COPY_OLD_FILE));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::OldOnly, false, ActionCategory::COPY_OLD_FILE));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::MERGE_CLEAN_OLD));
	EXPECT_TRUE(Merge20FilterDeckIsActive(state));
}

TEST(Merge20FilterDeck, ReviewOffHidesThatReviewStateOnly)
{
	// The D11 review dimension: DONE off hides DONE files only.
	Merge20FilterDeckState state = Merge20FilterDeckDefaultState();
	state.showReview[static_cast<int>(ReviewState::DONE)] = false;
	EXPECT_FALSE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::MERGE_CLEAN_OLD, ReviewState::DONE));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::MERGE_CLEAN_OLD, ReviewState::TODO));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::MERGE_CLEAN_OLD, ReviewState::KEEP));
	EXPECT_TRUE(Merge20FilterDeckIsActive(state));
}

TEST(Merge20FilterDeck, GroupsCombineWithAnd)
{
	// All three groups must pass: a file failing any one is hidden.
	Merge20FilterDeckState state = Merge20FilterDeckDefaultState();
	state.showStatus[static_cast<int>(Merge20FileStatus::OldOnly)] = false;
	state.showAction[static_cast<int>(ActionCategory::REVIEW_MIXED)] = false;
	state.showReview[static_cast<int>(ReviewState::HOLD)] = false;
	// Passes all three.
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::MERGE_CLEAN_OLD, ReviewState::TODO));
	// Fails the status group.
	EXPECT_FALSE(Visible(state, Merge20FileStatus::OldOnly, true, ActionCategory::COPY_OLD_FILE, ReviewState::TODO));
	// Fails the action group.
	EXPECT_FALSE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::REVIEW_MIXED, ReviewState::TODO));
	// Fails the review group.
	EXPECT_FALSE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::MERGE_CLEAN_OLD, ReviewState::HOLD));
}

TEST(Merge20FilterDeck, ActionPresetMatchesHtmlAcPreset)
{
	// HTML acPreset: selSt = {MODIFIED, OLD_ONLY}, selAc =
	// {MERGE_CLEAN_OLD, REVIEW_MIXED, REVIEW_MOD_ONLY,
	// COPY_OLD_FILE}. The review group is untouched.
	Merge20FilterDeckState state = Merge20FilterDeckDefaultState();
	state.showReview[static_cast<int>(ReviewState::DONE)] = false;
	Merge20FilterDeckApplyActionPreset(state);
	// The four action categories stay.
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::MERGE_CLEAN_OLD, ReviewState::TODO));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::REVIEW_MIXED, ReviewState::TODO));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::REVIEW_MOD_ONLY, ReviewState::TODO));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::OldOnly, true, ActionCategory::COPY_OLD_FILE, ReviewState::TODO));
	// The other three action categories go.
	EXPECT_FALSE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::NO_MERGE_NEW_ADD, ReviewState::TODO));
	EXPECT_FALSE(Visible(state, Merge20FileStatus::NewOnly, true, ActionCategory::SKIP_NEW_FILE, ReviewState::TODO));
	EXPECT_FALSE(Visible(state, Merge20FileStatus::Identical, true, ActionCategory::NO_ACTION_IDENTICAL, ReviewState::TODO));
	// New-only files go with the status group too.
	EXPECT_FALSE(Visible(state, Merge20FileStatus::NewOnly, false, ActionCategory::SKIP_NEW_FILE, ReviewState::TODO));
	// Unclassified files of a kept status still pass.
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, false, ActionCategory::NO_MERGE_NEW_ADD, ReviewState::TODO));
	// The review group is untouched by the preset.
	EXPECT_FALSE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::MERGE_CLEAN_OLD, ReviewState::DONE));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Modified, true, ActionCategory::MERGE_CLEAN_OLD, ReviewState::TODO));
	EXPECT_TRUE(Merge20FilterDeckIsActive(state));
}

TEST(Merge20FilterDeck, ShowAllRestoresEverything)
{
	Merge20FilterDeckState state = Merge20FilterDeckDefaultState();
	Merge20FilterDeckApplyActionPreset(state);
	state.showReview[static_cast<int>(ReviewState::TODO)] = false;
	Merge20FilterDeckShowAll(state);
	EXPECT_TRUE(Visible(state, Merge20FileStatus::Identical, true, ActionCategory::NO_ACTION_IDENTICAL, ReviewState::TODO));
	EXPECT_TRUE(Visible(state, Merge20FileStatus::NewOnly, true, ActionCategory::SKIP_NEW_FILE, ReviewState::KEEP));
	EXPECT_FALSE(Merge20FilterDeckIsActive(state));
}

TEST(Merge20FilterDeck, StatusOnSyncsActionOn)
{
	// HTML tgSt, add-only: turning IDENTICAL / OLD_ONLY /
	// NEW_ONLY on also turns on its action category. MODIFIED
	// has no single counterpart, so nothing is added for it.
	Merge20FilterDeckState state = Merge20FilterDeckDefaultState();
	state.showAction[static_cast<int>(ActionCategory::NO_ACTION_IDENTICAL)] = false;
	state.showAction[static_cast<int>(ActionCategory::COPY_OLD_FILE)] = false;
	state.showAction[static_cast<int>(ActionCategory::SKIP_NEW_FILE)] = false;
	Merge20FilterDeckSyncStatusOn(state, Merge20FileStatus::Identical);
	EXPECT_TRUE(state.showAction[static_cast<int>(ActionCategory::NO_ACTION_IDENTICAL)]);
	EXPECT_FALSE(state.showAction[static_cast<int>(ActionCategory::COPY_OLD_FILE)]);
	Merge20FilterDeckSyncStatusOn(state, Merge20FileStatus::OldOnly);
	EXPECT_TRUE(state.showAction[static_cast<int>(ActionCategory::COPY_OLD_FILE)]);
	Merge20FilterDeckSyncStatusOn(state, Merge20FileStatus::NewOnly);
	EXPECT_TRUE(state.showAction[static_cast<int>(ActionCategory::SKIP_NEW_FILE)]);
	// MODIFIED adds nothing.
	state.showAction[static_cast<int>(ActionCategory::MERGE_CLEAN_OLD)] = false;
	Merge20FilterDeckSyncStatusOn(state, Merge20FileStatus::Modified);
	EXPECT_FALSE(state.showAction[static_cast<int>(ActionCategory::MERGE_CLEAN_OLD)]);
}

TEST(Merge20FilterDeck, ActionOnSyncsStatusOn)
{
	// HTML tgAc, add-only: turning an action category on also
	// turns on its existence category; the four MODIFIED-side
	// categories all map to MODIFIED.
	Merge20FilterDeckState state = Merge20FilterDeckDefaultState();
	state.showStatus[static_cast<int>(Merge20FileStatus::Identical)] = false;
	state.showStatus[static_cast<int>(Merge20FileStatus::OldOnly)] = false;
	state.showStatus[static_cast<int>(Merge20FileStatus::NewOnly)] = false;
	state.showStatus[static_cast<int>(Merge20FileStatus::Modified)] = false;
	Merge20FilterDeckSyncActionOn(state, ActionCategory::NO_ACTION_IDENTICAL);
	EXPECT_TRUE(state.showStatus[static_cast<int>(Merge20FileStatus::Identical)]);
	EXPECT_FALSE(state.showStatus[static_cast<int>(Merge20FileStatus::OldOnly)]);
	Merge20FilterDeckSyncActionOn(state, ActionCategory::COPY_OLD_FILE);
	EXPECT_TRUE(state.showStatus[static_cast<int>(Merge20FileStatus::OldOnly)]);
	Merge20FilterDeckSyncActionOn(state, ActionCategory::SKIP_NEW_FILE);
	EXPECT_TRUE(state.showStatus[static_cast<int>(Merge20FileStatus::NewOnly)]);
	Merge20FilterDeckSyncActionOn(state, ActionCategory::REVIEW_MIXED);
	EXPECT_TRUE(state.showStatus[static_cast<int>(Merge20FileStatus::Modified)]);
}

TEST(Merge20FilterDeck, OutOfRangeIndexesHide)
{
	// A bogus enum value never shows (defensive: the casts in the
	// view pass real enum values, but the predicate must not read
	// out of bounds for one).
	Merge20FilterDeckState state = Merge20FilterDeckDefaultState();
	EXPECT_FALSE(Merge20FilterDeckIsFileVisible(state,
		static_cast<Merge20FileStatus>(9), true,
		ActionCategory::MERGE_CLEAN_OLD, ReviewState::TODO));
	EXPECT_FALSE(Merge20FilterDeckIsFileVisible(state,
		Merge20FileStatus::Modified, true,
		static_cast<ActionCategory>(9), ReviewState::TODO));
	EXPECT_FALSE(Merge20FilterDeckIsFileVisible(state,
		Merge20FileStatus::Modified, true,
		ActionCategory::MERGE_CLEAN_OLD, static_cast<ReviewState>(9)));
}
