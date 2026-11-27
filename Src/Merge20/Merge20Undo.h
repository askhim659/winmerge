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
 * conclusion (B1). B10 PR2 adds the remaining kinds: a split /
 * regroup (D5) and a learning-candidate accept / dismiss (L3).
 * The same code is compiled into WinMergeU and into the GoogleTest
 * runner.
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
 * that contract (UndoDoesNotRecordLearning). B10 PR2 extends the
 * same guarantee to the new kinds: undoing / redoing a learning
 * acceptance never records a judgment and never rewrites the
 * learning JSON -- the acceptance's +1 record stays in the store
 * as a past judgment, and the undo only rewinds the display (the
 * diff goes back to Unresolved and its candidate shows again).
 * Undoing a dismissal only drops the diff identity from the
 * dismissed set, which is doc-local state, not learning data.
 *
 * Split / regroup entries (B10 PR2) carry the split group (the
 * original range plus the row count) and the judgment the
 * operation carried (the parent conclusion and segment state for
 * a split, the first row's for a regroup). The adapter applies
 * them by calling the same Merge20SplitDiff / Merge20RegroupDiff
 * the menus call, with recording suppressed, so the result pane
 * is rebuilt exactly as the manual operation rebuilds it and the
 * next session save stashes the restored groups (the save reads
 * them from the open doc). An entry is applied only when every
 * row still carries the stored judgment: a row re-judged after
 * the split makes the entry stale (the adapter drops it) instead
 * of silently discarding that row's judgment, which a regroup
 * would otherwise do (it keeps only the first row's judgment).
 *
 * The same code is compiled into WinMergeU and into the GoogleTest
 * runner.
 */
#pragma once

#include <deque>
#include <vector>
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
	SplitDiff, /**< Per-row split of a diff group (D5, B10 PR2) */
	RegroupDiff, /**< Regroup of a split group back to one diff (D5, B10 PR2) */
	LearnAccept, /**< Learning-candidate acceptance (L3, B10 PR2) */
	LearnDismiss, /**< Learning-candidate dismissal (L3, B10 PR2) */
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
 * stale entry. A split / regroup entry carries the split group
 * (the original range plus the row count) and the judgment the
 * operation carried: oldConclusion is the carried conclusion (the
 * parent's for a split, the first row's for a regroup) and
 * splitSegmentState / splitSrcPanes are the carried segment state
 * and source panes, used to detect a row re-judged since the
 * operation. A learning entry carries the diff identity like a
 * conclusion entry, the old/new conclusion and review / output
 * flag before and after the acceptance linkage (dismissal changes
 * none of them), the source panes the acceptance chose
 * (LearnAccept only, so a redo can re-apply them without the
 * candidate map), and hadCandidate, which is what makes an
 * acceptance / dismissal a change at all (the candidate
 * disappearing is the change, even when the conclusion did not
 * move).
 */
struct Merge20UndoEntry
{
	Merge20UndoKind kind = Merge20UndoKind::ReviewState;
	String relPath; /**< File, as DIFFITEM::getItemRelativePath() */
	int diffIndex = -1; /**< Diff list index (DiffConclusion / Learn* only; a hint for Split / Regroup, whose lookup is by group identity) */
	int diffOp = 0; /**< DIFFRANGE op (DiffConclusion / Learn* only) */
	int diffBegin[3] = { -1, -1, -1 }; /**< First real line per pane (DiffConclusion / Learn* only) */
	int diffEnd[3] = { -1, -1, -1 }; /**< Last real line per pane (DiffConclusion / Learn* only) */
	ReviewState oldReview = ReviewState::TODO;
	ReviewState newReview = ReviewState::TODO;
	bool oldInclude = true;
	bool newInclude = true;
	Merge20DiffConclusion oldConclusion = Merge20DiffConclusion::Undecided;
	Merge20DiffConclusion newConclusion = Merge20DiffConclusion::Undecided;
	Merge20SplitGroup splitGroup; /**< Split / regroup only: the original range plus the row count */
	int splitSegmentState = -1; /**< Split / regroup only: ResultSegmentState ordinal carried by the operation (-1 when no segment) */
	std::vector<int> splitSrcPanes; /**< Split / regroup only: source panes carried by the operation */
	std::vector<int> learnSrcPanes; /**< LearnAccept only: source panes the acceptance chose */
	bool hadCandidate = false; /**< LearnAccept / LearnDismiss only: a candidate was consumed by the operation */
};

/**
 * @brief Whether the entry changes anything at all.
 * @return False when every value the kind carries is unchanged
 *         (such entries are never pushed: pressing '2' on a file
 *         that is already DONE is not an undo step). A split /
 *         regroup entry is a change when its group has at least
 *         two rows; a learning entry is a change when a candidate
 *         was consumed (or, for an acceptance, when the conclusion
 *         or the file state moved).
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
 *         for DiffConclusion and Learn* entries; other kinds
 *         return true (split / regroup entries are matched by
 *         group identity instead, see Merge20Split.h).
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
