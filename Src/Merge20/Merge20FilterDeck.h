// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FilterDeck.h
 *
 * @brief Filter-deck state for the folder compare list (H-FILTER,
 * coverage matrix H-11/H-13/H-39).
 *
 * Merge20 (H-FILTER): the HTML tool's filter deck (17_v17.15.html)
 * is three checkbox groups over one file list: the existence
 * category (selSt: MODIFIED / OLD_ONLY / NEW_ONLY / IDENTICAL,
 * several may be on), the action category (selAc / tgAc over the
 * seven ALL_AC values, several may be on) plus the "extract only
 * files needing action" preset (acPreset), and the review state
 * (selRev / revFilterPop, several may be on). A file is shown only
 * when it passes all three groups (HTML filtFiles: selSt.has(f.st)
 * && selAc.has(f.ac); selRev is the batch-check target there and
 * the list filter here, see below).
 *
 * This pure module owns the deck state and the per-file visibility
 * decision so it can be unit-tested without MFC. The folder view
 * (IsShowable, DirActions.cpp) applies it; the deck bar
 * (CMerge20FilterDeckBar) only edits the state through the option
 * keys and asks the view to rebuild. The file-list panel does NOT
 * use this module: its action-category filter tabs stay panel-local
 * (single-select, design section 12.3 P-R10; see MERGE20_NOTES.md
 * section 43 for why the deck and the tabs are not merged).
 *
 * Rules carried over from the HTML tool, and where the fork
 * deliberately differs (see MERGE20_NOTES.md section 43):
 * - Defaults are all ON (an exclusion filter: uncheck to hide).
 *   The HTML defaults hide IDENTICAL (selSt without IDENTICAL,
 *   selAc without NO_ACTION_IDENTICAL); the fork keeps identical
 *   files visible by default, like every WinMerge show_* filter
 *   and the D11 review filter. Turning IDENTICAL off is one click.
 * - The "only files needing action" preset is exactly HTML
 *   acPreset: existence {MODIFIED, OLD_ONLY}, action
 *   {MERGE_CLEAN_OLD, REVIEW_MIXED, REVIEW_MOD_ONLY,
 *   COPY_OLD_FILE}. The review group is left untouched (HTML
 *   acPreset does not touch selRev either).
 * - The HTML coupling between the two groups (tgSt / tgAc) is
 *   kept, add-only: turning an existence category ON also turns
 *   on its action category (IDENTICAL -> NO_ACTION_IDENTICAL,
 *   OLD_ONLY -> COPY_OLD_FILE, NEW_ONLY -> SKIP_NEW_FILE), and
 *   turning an action category ON also turns on its existence
 *   category (the three one-sided ones, anything else ->
 *   MODIFIED). Turning a box OFF never clears its counterpart
 *   (HTML does not do that either).
 * - Unclassified files (S4 classification has not run yet: the
 *   file was never opened) pass the action dimension. The panel
 *   tabs do the opposite (an unclassified file matches no tab,
 *   only "all") because a tab is an inclusion ("show only X");
 *   the deck is an exclusion ("hide X"), and hiding every
 *   not-yet-opened file whenever one action box is off would
 *   collapse the list to opened files only. The preset keeps
 *   unclassified files visible for the same reason.
 * - The review group is the D11 filter (OPT_MERGE20_SHOW_REVIEW_*,
 *   show_review[5]): the deck checkboxes and the folder-list
 *   context-menu submenu ("Show review states") edit the same
 *   five keys, so there is one stored state with two controls.
 *   The HTML spOnly pill ("SPECIAL only") has no separate deck
 *   control: unchecking the other four review states shows
 *   SPECIAL files only, which is the same result.
 *
 * This file stays ASCII and free of MFC and of the translation
 * macro _(): the same code is compiled into WinMergeU and into
 * the GoogleTest runner (Merge20FileListFilter.h precedent).
 */
#pragma once

#include "Merge20Types.h"

/** @brief Existence category of one file (HTML selSt). */
enum class Merge20FileStatus
{
	Modified = 0, /**< Both sides exist, contents differ */
	OldOnly = 1,  /**< Old (left) side only */
	NewOnly = 2,  /**< New (right) side only */
	Identical = 3 /**< Both sides exist, no difference */
};

/**
 * @brief One filter deck: which existence categories, action
 * categories and review states are shown. Indexing: showStatus by
 * static_cast<int>(Merge20FileStatus), showAction by
 * static_cast<int>(ActionCategory), showReview by
 * static_cast<int>(ReviewState).
 */
struct Merge20FilterDeckState
{
	bool showStatus[4] = { true, true, true, true };
	bool showAction[7] = { true, true, true, true, true, true, true };
	bool showReview[5] = { true, true, true, true, true };
};

/** @brief The default deck: everything shown. */
inline Merge20FilterDeckState Merge20FilterDeckDefaultState()
{
	return Merge20FilterDeckState();
}

/**
 * @brief Decide whether one file row passes the deck.
 * @param [in] state The deck state.
 * @param [in] status Existence category of the file.
 * @param [in] classified True once S4 classification ran for this
 *             file (Merge20FileState::classified).
 * @param [in] action Action category (meaningful only when
 *             classified; unclassified files pass this dimension).
 * @param [in] review Review state of the file.
 * @return True when the file is shown (all three groups pass).
 */
bool Merge20FilterDeckIsFileVisible(
	const Merge20FilterDeckState& state,
	Merge20FileStatus status,
	bool classified,
	ActionCategory action,
	ReviewState review);

/**
 * @brief Apply the "only files needing action" preset (HTML
 * acPreset): existence {MODIFIED, OLD_ONLY}, action
 * {MERGE_CLEAN_OLD, REVIEW_MIXED, REVIEW_MOD_ONLY, COPY_OLD_FILE}.
 * The review group is left untouched.
 */
void Merge20FilterDeckApplyActionPreset(Merge20FilterDeckState& state);

/** @brief Show everything again (all three groups fully on). */
void Merge20FilterDeckShowAll(Merge20FilterDeckState& state);

/**
 * @brief HTML tgSt coupling, add-only: when an existence category
 * is turned ON, also turn on its action category (IDENTICAL ->
 * NO_ACTION_IDENTICAL, OLD_ONLY -> COPY_OLD_FILE, NEW_ONLY ->
 * SKIP_NEW_FILE; MODIFIED has no single counterpart, so nothing
 * is added for it). Turning a category OFF calls for no sync.
 */
void Merge20FilterDeckSyncStatusOn(
	Merge20FilterDeckState& state, Merge20FileStatus status);

/**
 * @brief HTML tgAc coupling, add-only: when an action category is
 * turned ON, also turn on its existence category
 * (NO_ACTION_IDENTICAL -> IDENTICAL, COPY_OLD_FILE -> OLD_ONLY,
 * SKIP_NEW_FILE -> NEW_ONLY, anything else -> MODIFIED).
 * Turning a category OFF calls for no sync.
 */
void Merge20FilterDeckSyncActionOn(
	Merge20FilterDeckState& state, ActionCategory action);

/**
 * @brief True when the deck hides anything (any box off). The
 * folder view uses this to decide whether a folder needs the
 * "no visible child" scan (D11 precedent: IsReviewFilterActive).
 */
bool Merge20FilterDeckIsActive(const Merge20FilterDeckState& state);
