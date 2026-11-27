// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Undo.h
 *
 * @brief Undo / redo of judgment operations (Merge20 best-workflow
 * B10, pillar 6: "no judgment is lost by a slip of the hand").
 *
 * Pure module (no MFC, no `_()`): one undo entry per judgment
 * operation, kept in a stack owned by the folder compare
 * (CDiffContext, one stack per folder compare). The three kinds
 * covered by B10 PR1 are a review-state change (S2 keys 1-5, Space,
 * panel cycling), an output-checkbox toggle, and a per-diff
 * conclusion (B1). Split / regroup (D5) and learning-candidate
 * accept / dismiss are PR2 (see the PR body).
 *
 * The stack stores values only, never pointers: an entry holds the
 * file (relative path), the old and new values, and -- for a diff
 * conclusion -- the diff's identity (index, op, first/last real
 * lines), so a stale entry can be detected when the diff list has
 * moved on (rescan, split / regroup). Applying an entry is the
 * adapter's job (CDirDoc / CMergeDoc); this module only owns the
 * stack discipline:
 * - depth is capped at Merge20UndoMaxDepth (100): older entries
 *   drop off the bottom, never a crash;
 * - a new operation clears the redo stack;
 * - an entry that changes nothing is never pushed.
 *
 * Learning invariance (gap analysis section 3.1): the stack never
 * touches learning. Undo applies a conclusion through
 * CMergeDoc::Merge20SetDiffConclusionRestored and raw state
 * setters, none of which calls Merge20LearnRecordDiffJudgment;
 * the only learning record point stays CMergeDoc::
 * Merge20SetDiffConclusion. A unit test pins the stack side of
 * that contract (UndoDoesNotRecordLearning).
 *
 * The same code is compiled into WinMergeU and into the GoogleTest
 * runner.
 */
#pragma once

#include <deque>
#include "Merge20/Merge20Conclusion.h"
#include "Merge20Types.h"

/** @brief Maximum undo depth (gap analysis B10: "e.g. 100"). */
constexpr size_t Merge20UndoMaxDepth = 100;

/** @brief Kind of judgment operation one undo entry rewinds. */
enum class Merge20UndoKind
{
	ReviewState,    /**< Review-state change (S2, with the S2 output-flag linkage) */
	IncludeInOutput, /**< Output-checkbox toggle */
	DiffConclusion, /**< Per-diff conclusion (B1, with the B1 file linkage) */
};

/**
 * @brief One undoable judgment operation.
 *
 * Values are stored as ordinals / plain values so the entry stays
 * free of document pointers. Fields that do not apply to the kind
 * are ignored: a review-state entry carries old/new review and
 * old/new output flag (the S2 linkage changes both at once); an
 * output entry carries only the flag; a conclusion entry carries
 * the old/new conclusion plus the review / output flag before and
 * after the B1 linkage, plus the diff identity used to detect a
 * stale entry.
 */
struct Merge20UndoEntry
{
	Merge20UndoKind kind = Merge20UndoKind::ReviewState;
	String relPath; /**< File, as DIFFITEM::getItemRelativePath() */
	int diffIndex = -1; /**< Diff list index (DiffConclusion only) */
	int diffOp = 0; /**< DIFFRANGE op (DiffConclusion only) */
	int diffBegin[3] = { -1, -1, -1 }; /**< First real line per pane (DiffConclusion only) */
	int diffEnd[3] = { -1, -1, -1 }; /**< Last real line per pane (DiffConclusion only) */
	ReviewState oldReview = ReviewState::TODO;
	ReviewState newReview = ReviewState::TODO;
	bool oldInclude = true;
	bool newInclude = true;
	Merge20DiffConclusion oldConclusion = Merge20DiffConclusion::Undecided;
	Merge20DiffConclusion newConclusion = Merge20DiffConclusion::Undecided;
};

/**
 * @brief Whether the entry changes anything at all.
 * @return False when every value the kind carries is unchanged
 *         (such entries are never pushed: pressing '2' on a file
 *         that is already DONE is not an undo step).
 */
bool Merge20UndoEntryHasChange(const Merge20UndoEntry& entry);

/**
 * @brief Whether a conclusion entry still names the same diff.
 * @param [in] entry Conclusion entry to check.
 * @param [in] op Current DIFFRANGE op at the entry's index.
 * @param [in] begin Current first real lines per pane.
 * @param [in] end Current last real lines per pane.
 * @return False when the op or any begin/end line differs: the
 *         diff list moved on (rescan cleared the conclusions, or
 *         a split / regroup shifted the indices), so the entry is
 *         stale and must be dropped, not applied. Only meaningful
 *         for DiffConclusion entries; other kinds return true.
 */
bool Merge20UndoEntryMatchesDiff(const Merge20UndoEntry& entry, int op, const int begin[3], const int end[3]);

/**
 * @brief Outcome of one undo / redo step, for the status message.
 *
 * Applied: the entry was rewound (undo) or re-applied (redo).
 * NothingToUndo / NothingToRedo: the stack was empty.
 * Stale: the top entry no longer matches the current state (its
 * new values differ, or its diff moved): the entry is dropped and
 * nothing changes.
 * TargetNotOpen: a conclusion entry's file compare is not open:
 * the entry is left on the stack so it can be retried after the
 * file is opened.
 */
enum class Merge20UndoApplyResult
{
	Applied,
	NothingToUndo,
	NothingToRedo,
	Stale,
	TargetNotOpen,
};

/**
 * @brief Undo / redo stack for one folder compare.
 *
 * Undo pops the newest entry to the redo stack; redo pops it back.
 * A push clears the redo stack. Depth is capped: pushing past
 * Merge20UndoMaxDepth drops the oldest entry. Peek* / Discard*
 * let the adapter validate an entry before committing to it: a
 * stale entry is discarded (never offered as redo), a not-open
 * target leaves the entry in place.
 */
class Merge20UndoStack
{
public:
	/** @brief Push one entry; no-op entries are ignored and a push clears the redo stack. */
	void Push(const Merge20UndoEntry& entry);
	/** @brief Top undo entry, or nullptr when empty. */
	const Merge20UndoEntry* PeekUndo() const;
	/** @brief Top redo entry, or nullptr when empty. */
	const Merge20UndoEntry* PeekRedo() const;
	/** @brief Move the top undo entry to the redo stack. False when empty. */
	bool PopUndoToRedo();
	/** @brief Move the top redo entry to the undo stack. False when empty. */
	bool PopRedoToUndo();
	/** @brief Drop the top undo entry without offering it as redo. False when empty. */
	bool DiscardUndoTop();
	/** @brief Drop the top redo entry. False when empty. */
	bool DiscardRedoTop();
	/** @brief True when an undo step is available. */
	bool CanUndo() const { return !m_undo.empty(); }
	/** @brief True when a redo step is available. */
	bool CanRedo() const { return !m_redo.empty(); }
	/** @brief Number of undo steps available. */
	size_t UndoSize() const { return m_undo.size(); }
	/** @brief Number of redo steps available. */
	size_t RedoSize() const { return m_redo.size(); }
	/** @brief Drop both stacks (session restore: the restored states make every entry stale). */
	void Clear();

private:
	std::deque<Merge20UndoEntry> m_undo;
	std::deque<Merge20UndoEntry> m_redo;
};
