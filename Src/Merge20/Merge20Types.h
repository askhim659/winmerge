/**
 * @file  Merge20Types.h
 *
 * @brief Shared types for the Merge20 fork (folder compare workflow state).
 *
 * Merge20 (S1, design sections 2.2 and 2.4): review state, action category,
 * risk level and the per-file work state. This header is UI-independent so
 * that later slices (classification, batch approve, CSV, session save) can
 * reuse it without pulling in MFC headers. Display strings for these values
 * live at the call sites via _() so that this file stays ASCII-only.
 */
#pragma once

#include <vector>
#include "UnicodeString.h"

/** @brief Review state of one file (HTML tool REV, spec section 4(b)). */
enum class ReviewState
{
	TODO = 0, /**< Not reviewed yet (initial state of every file) */
	DONE,     /**< Done: safe-merge target */
	HOLD,     /**< On hold */
	SPECIAL,  /**< Needs manual work */
	KEEP      /**< Keep as is */
};

/** @brief Action category (HTML tool ALL_AC, design section 3.1). */
enum class ActionCategory
{
	MERGE_CLEAN_OLD,    /**< Merge needed (missing in new) */
	REVIEW_MIXED,       /**< Review (missing + changed) */
	REVIEW_MOD_ONLY,    /**< Review (modified only) */
	NO_MERGE_NEW_ADD,   /**< New additions only */
	COPY_OLD_FILE,      /**< Old-only file */
	SKIP_NEW_FILE,      /**< New-only file */
	NO_ACTION_IDENTICAL /**< Identical */
};

/** @brief Risk level (design section 3.1). */
enum class RiskLevel
{
	NONE = 0,
	MEDIUM,
	HIGH
};

/**
 * @brief One per-diff judgment in a merge result pane (S8).
 *
 * A snapshot of one MergeResultSegment (see MergeResultPane.h) plus the
 * diff-range identity it belongs to. `state` is the ResultSegmentState
 * ordinal (Common=0, Auto=1, Chosen=2, Unresolved=3, Conflict=4,
 * Edited=5); `op` is the OP_TYPE of the diff range (see DiffList.h).
 * The begin/end line numbers are the real (non-ghost) line numbers of
 * the diff range, used on restore to tell a changed diff from the one
 * that was judged (spec section 6.1). Stored as plain ints so this
 * header stays free of MergeDoc headers.
 */
struct Merge20DiffJudgment
{
	int diffIdx = -1; /**< Index in the diff list */
	int op = 0;       /**< OP_TYPE of the diff range */
	int begin[3] = { -1, -1, -1 }; /**< First real line of the range, per pane */
	int end[3] = { -1, -1, -1 };   /**< Last real line of the range, per pane */
	int state = 0;    /**< ResultSegmentState ordinal */
	std::vector<int> srcPanes; /**< Chosen source panes (0/1/2), in order; empty when unresolved */
};

/**
 * @brief Per-file work state kept outside DIFFITEM (design section 2.4).
 *
 * DIFFITEM must not hold GUI work state (see the comment in DiffItem.h),
 * so this struct lives in a sidecar map keyed by relative path
 * (DIFFITEM::getItemRelativePath()). Only `review` is used by S1; the
 * other fields are filled in by later slices (S2: includeInOutput,
 * S4: classified/action/risk/counts/summary, S6/S8: memo, S8:
 * diffJudgments).
 */
struct Merge20FileState
{
	ReviewState review = ReviewState::TODO;
	bool includeInOutput = true; /**< Output checkbox. Initial value depends on the existence category (design section 3.2); S2 sets it. */
	bool classified = false; /**< True once S4 classification ran for this file (file opened in a merge doc). Unopened files show "Not calculated" in the list. */
	ActionCategory action = ActionCategory::NO_ACTION_IDENTICAL;
	RiskLevel risk = RiskLevel::NONE;
	int nMissing = 0;  /**< Missing-in-new (deleted) lines */
	int nModified = 0; /**< Changed lines */
	int nAdded = 0;    /**< Newly added lines */
	int nExcluded = 0; /**< Excluded lines */
	bool simpleCompare = false; /**< True when a simplified fallback compare was used */
	String summary;    /**< Rule-based summary text */
	String memo;       /**< Audit memo */
	std::vector<Merge20DiffJudgment> diffJudgments; /**< Per-diff judgments of the merge result pane (S8). Empty when the file was never judged in a merge session, or when its merge doc was never opened. */
	String leftEncoding;  /**< Left-side encoding, for display */
	String rightEncoding; /**< Right-side encoding, for display */
};
