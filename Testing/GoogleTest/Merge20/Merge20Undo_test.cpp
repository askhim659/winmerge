// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Undo_test.cpp
 *
 * @brief Merge20 (best-workflow B10 PR1, extended by PR2) unit tests for the judgment
 * undo / redo stack.
 *
 * The stack is a pure module, so these tests run in the UnitTests
 * runner on CI. What is pinned here:
 * - push / undo / redo order across the three PR1 kinds (review
 *   state, output flag, diff conclusion);
 * - the 100-entry cap: older entries drop off, never a crash;
 * - a new operation clears the redo stack;
 * - no-op entries are never pushed;
 * - a stale entry (diff identity moved on) is detected by
 *   Merge20UndoEntryMatchesDiff, so the adapter can drop it
 *   instead of applying a conclusion to the wrong diff;
 * - learning invariance (gap analysis section 3.1): the stack
 *   carries no learning payload and its push / undo / redo cycle
 *   never records anything -- the recording point stays
 *   CMergeDoc::Merge20SetDiffConclusion, and the undo path
 *   (Merge20SetDiffConclusionRestored plus raw state setters)
 *   contains no learning call. The test drives the cycle with a
 *   stand-in learning counter that only the record path touches,
 *   mirroring that split.
 * - (B10 PR2) the split / regroup kinds are changes only for a
 *   group of at least two rows, and the carried judgment (parent
 *   conclusion, segment state, source panes) survives the stack
 *   round-trip;
 * - (B10 PR2) a learning acceptance / dismissal is a change when
 *   a candidate was consumed (even when the conclusion did not
 *   move), the accepted source panes survive the round-trip so a
 *   redo needs no candidate lookup, the diff identity matcher
 *   covers the learning kinds, and the learn undo / redo cycle
 *   never records anything either.
 *
 * This file is ASCII; the pure module is MFC-free.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include <tchar.h>
#include "Merge20/Merge20Undo.h"

namespace
{

Merge20UndoEntry ReviewEntry(const String& relPath, ReviewState oldState, ReviewState newState,
	bool oldInclude = true, bool newInclude = true)
{
	Merge20UndoEntry entry;
	entry.kind = Merge20UndoKind::ReviewState;
	entry.relPath = relPath;
	entry.oldReview = oldState;
	entry.newReview = newState;
	entry.oldInclude = oldInclude;
	entry.newInclude = newInclude;
	return entry;
}

Merge20UndoEntry IncludeEntry(const String& relPath, bool oldInclude, bool newInclude)
{
	Merge20UndoEntry entry;
	entry.kind = Merge20UndoKind::IncludeInOutput;
	entry.relPath = relPath;
	entry.oldInclude = oldInclude;
	entry.newInclude = newInclude;
	return entry;
}

Merge20UndoEntry ConclusionEntry(const String& relPath, int diffIndex,
	Merge20DiffConclusion oldConclusion, Merge20DiffConclusion newConclusion)
{
	Merge20UndoEntry entry;
	entry.kind = Merge20UndoKind::DiffConclusion;
	entry.relPath = relPath;
	entry.diffIndex = diffIndex;
	entry.diffOp = Merge20ConclusionOpDiff;
	entry.diffBegin[0] = 10;
	entry.diffBegin[1] = 12;
	entry.diffEnd[0] = 14;
	entry.diffEnd[1] = 16;
	entry.oldConclusion = oldConclusion;
	entry.newConclusion = newConclusion;
	return entry;
}

Merge20SplitGroup SplitGroupForTest()
{
	Merge20SplitGroup group;
	group.op = Merge20ConclusionOpDiff;
	group.begin[0] = 10;
	group.end[0] = 14;
	group.begin[1] = 12;
	group.end[1] = 16;
	group.begin[2] = -1;
	group.end[2] = -1;
	group.dbegin = 30;
	group.dend = 34;
	group.rows = 5;
	return group;
}

Merge20UndoEntry SplitEntry(const String& relPath, Merge20UndoKind kind)
{
	Merge20UndoEntry entry;
	entry.kind = kind;
	entry.relPath = relPath;
	entry.diffIndex = 3;
	entry.splitGroup = SplitGroupForTest();
	entry.oldConclusion = Merge20DiffConclusion::UseOld;
	entry.newConclusion = Merge20DiffConclusion::UseOld;
	entry.splitSegmentState = 2; // ResultSegmentState::Chosen
	entry.splitSrcPanes = { 0 };
	return entry;
}

Merge20UndoEntry LearnEntry(const String& relPath, Merge20UndoKind kind,
	Merge20DiffConclusion oldConclusion, Merge20DiffConclusion newConclusion)
{
	Merge20UndoEntry entry;
	entry.kind = kind;
	entry.relPath = relPath;
	entry.diffIndex = 2;
	entry.diffOp = Merge20ConclusionOpDiff;
	entry.diffBegin[0] = 10;
	entry.diffBegin[1] = 12;
	entry.diffEnd[0] = 14;
	entry.diffEnd[1] = 16;
	entry.oldConclusion = oldConclusion;
	entry.newConclusion = newConclusion;
	entry.oldReview = ReviewState::TODO;
	entry.newReview = ReviewState::DONE;
	entry.hadCandidate = true;
	if (kind == Merge20UndoKind::LearnAccept)
		entry.learnSrcPanes = { 0 };
	return entry;
}

TEST(Merge20Undo, PushUndoRedoOrderAcrossKinds)
{
	Merge20UndoStack stack;
	EXPECT_FALSE(stack.CanUndo());
	EXPECT_FALSE(stack.CanRedo());

	stack.Push(ReviewEntry(_T("a.cpp"), ReviewState::TODO, ReviewState::DONE));
	stack.Push(IncludeEntry(_T("b.cpp"), true, false));
	stack.Push(ConclusionEntry(_T("c.cpp"), 3,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::UseOld));
	EXPECT_EQ(stack.UndoSize(), 3U);
	EXPECT_TRUE(stack.CanUndo());
	EXPECT_FALSE(stack.CanRedo());

	// Newest first.
	ASSERT_NE(stack.PeekUndo(), nullptr);
	EXPECT_EQ(stack.PeekUndo()->kind, Merge20UndoKind::DiffConclusion);
	EXPECT_EQ(stack.PeekUndo()->relPath, String(_T("c.cpp")));
	EXPECT_TRUE(stack.PopUndoToRedo());
	ASSERT_NE(stack.PeekUndo(), nullptr);
	EXPECT_EQ(stack.PeekUndo()->kind, Merge20UndoKind::IncludeInOutput);
	EXPECT_TRUE(stack.PopUndoToRedo());
	ASSERT_NE(stack.PeekUndo(), nullptr);
	EXPECT_EQ(stack.PeekUndo()->kind, Merge20UndoKind::ReviewState);
	EXPECT_TRUE(stack.PopUndoToRedo());
	EXPECT_FALSE(stack.CanUndo());
	EXPECT_EQ(stack.RedoSize(), 3U);

	// Redo walks back in the same order.
	ASSERT_NE(stack.PeekRedo(), nullptr);
	EXPECT_EQ(stack.PeekRedo()->kind, Merge20UndoKind::ReviewState);
	EXPECT_TRUE(stack.PopRedoToUndo());
	ASSERT_NE(stack.PeekRedo(), nullptr);
	EXPECT_EQ(stack.PeekRedo()->kind, Merge20UndoKind::IncludeInOutput);
	EXPECT_TRUE(stack.PopRedoToUndo());
	ASSERT_NE(stack.PeekRedo(), nullptr);
	EXPECT_EQ(stack.PeekRedo()->kind, Merge20UndoKind::DiffConclusion);
	EXPECT_TRUE(stack.PopRedoToUndo());
	EXPECT_FALSE(stack.CanRedo());
	EXPECT_EQ(stack.UndoSize(), 3U);
}

TEST(Merge20Undo, NewPushClearsRedo)
{
	Merge20UndoStack stack;
	stack.Push(ReviewEntry(_T("a.cpp"), ReviewState::TODO, ReviewState::DONE));
	stack.Push(ReviewEntry(_T("a.cpp"), ReviewState::DONE, ReviewState::HOLD));
	EXPECT_TRUE(stack.PopUndoToRedo());
	EXPECT_TRUE(stack.CanRedo());
	stack.Push(ReviewEntry(_T("a.cpp"), ReviewState::DONE, ReviewState::SPECIAL));
	EXPECT_FALSE(stack.CanRedo());
	EXPECT_EQ(stack.RedoSize(), 0U);
	EXPECT_EQ(stack.UndoSize(), 2U);
}

TEST(Merge20Undo, DepthCapDropsOldestEntries)
{
	Merge20UndoStack stack;
	for (int i = 0; i < 101; ++i)
	{
		Merge20UndoEntry entry = ReviewEntry(_T("a.cpp"), ReviewState::TODO, ReviewState::DONE);
		entry.newReview = static_cast<ReviewState>(i % 5);
		// Keep every entry a real change: old TODO, new cycles
		// TODO(0) only when i % 5 == 0 -- those are no-ops and
		// are skipped, so push a distinct include flip instead.
		if (i % 5 == 0)
			entry.newInclude = false;
		stack.Push(entry);
	}
	EXPECT_EQ(stack.UndoSize(), Merge20UndoMaxDepth);
	// Undoing all 100 must not crash and must end empty.
	int nUndone = 0;
	while (stack.CanUndo())
	{
		EXPECT_TRUE(stack.PopUndoToRedo());
		++nUndone;
	}
	EXPECT_EQ(nUndone, 100);
	EXPECT_FALSE(stack.CanUndo());
}

TEST(Merge20Undo, CapKeepsNewestEntries)
{
	Merge20UndoStack stack;
	for (int i = 0; i < 105; ++i)
	{
		Merge20UndoEntry entry = IncludeEntry(_T("f.cpp"), true, false);
		entry.diffIndex = i; // tag the entry with its push order
		stack.Push(entry);
	}
	EXPECT_EQ(stack.UndoSize(), Merge20UndoMaxDepth);
	// The newest entry (i = 104) is on top; the oldest kept is
	// i = 5 (entries 0-4 dropped).
	ASSERT_NE(stack.PeekUndo(), nullptr);
	EXPECT_EQ(stack.PeekUndo()->diffIndex, 104);
	int nUndone = 0;
	int oldestSeen = 104;
	while (stack.CanUndo())
	{
		ASSERT_NE(stack.PeekUndo(), nullptr);
		oldestSeen = stack.PeekUndo()->diffIndex;
		EXPECT_TRUE(stack.PopUndoToRedo());
		++nUndone;
	}
	EXPECT_EQ(nUndone, 100);
	EXPECT_EQ(oldestSeen, 5);
}

TEST(Merge20Undo, NoopEntriesAreNeverPushed)
{
	Merge20UndoStack stack;
	// Same review state, same flag: not a step.
	stack.Push(ReviewEntry(_T("a.cpp"), ReviewState::DONE, ReviewState::DONE));
	// Same flag: not a step.
	stack.Push(IncludeEntry(_T("b.cpp"), true, true));
	// Same conclusion: not a step.
	stack.Push(ConclusionEntry(_T("c.cpp"), 1,
		Merge20DiffConclusion::UseOld, Merge20DiffConclusion::UseOld));
	EXPECT_EQ(stack.UndoSize(), 0U);
	EXPECT_FALSE(stack.CanUndo());
	// A review change that only flips the flag (S2 linkage) is a step.
	stack.Push(ReviewEntry(_T("a.cpp"), ReviewState::DONE, ReviewState::DONE, true, false));
	EXPECT_EQ(stack.UndoSize(), 1U);
}

TEST(Merge20Undo, DiscardTopDropsStaleEntry)
{
	Merge20UndoStack stack;
	stack.Push(ReviewEntry(_T("a.cpp"), ReviewState::TODO, ReviewState::DONE));
	stack.Push(ReviewEntry(_T("b.cpp"), ReviewState::TODO, ReviewState::HOLD));
	EXPECT_TRUE(stack.DiscardUndoTop());
	EXPECT_EQ(stack.UndoSize(), 1U);
	EXPECT_FALSE(stack.CanRedo());
	ASSERT_NE(stack.PeekUndo(), nullptr);
	EXPECT_EQ(stack.PeekUndo()->relPath, String(_T("a.cpp")));
	EXPECT_TRUE(stack.DiscardUndoTop());
	EXPECT_FALSE(stack.CanUndo());
	EXPECT_FALSE(stack.DiscardUndoTop());
	EXPECT_FALSE(stack.PopUndoToRedo());
	EXPECT_FALSE(stack.PopRedoToUndo());
}

TEST(Merge20Undo, ClearDropsBothStacks)
{
	Merge20UndoStack stack;
	stack.Push(ReviewEntry(_T("a.cpp"), ReviewState::TODO, ReviewState::DONE));
	stack.Push(IncludeEntry(_T("b.cpp"), true, false));
	EXPECT_TRUE(stack.PopUndoToRedo());
	stack.Clear();
	EXPECT_EQ(stack.UndoSize(), 0U);
	EXPECT_EQ(stack.RedoSize(), 0U);
	EXPECT_FALSE(stack.CanUndo());
	EXPECT_FALSE(stack.CanRedo());
}

TEST(Merge20Undo, ConclusionIdentityMatchesSameDiff)
{
	const Merge20UndoEntry entry = ConclusionEntry(_T("c.cpp"), 3,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::UseOld);
	const int begin[3] = { 10, 12, -1 };
	const int end[3] = { 14, 16, -1 };
	EXPECT_TRUE(Merge20UndoEntryMatchesDiff(entry, Merge20ConclusionOpDiff, begin, end));
}

TEST(Merge20Undo, ConclusionIdentityRejectsMovedDiff)
{
	const Merge20UndoEntry entry = ConclusionEntry(_T("c.cpp"), 3,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::UseOld);
	const int begin[3] = { 10, 12, -1 };
	const int end[3] = { 14, 16, -1 };
	// Different op (rescan changed the diff kind).
	EXPECT_FALSE(Merge20UndoEntryMatchesDiff(entry, Merge20ConclusionOpOldOnly, begin, end));
	// Same op, shifted lines (split / regroup moved the range).
	const int shiftedBegin[3] = { 11, 12, -1 };
	EXPECT_FALSE(Merge20UndoEntryMatchesDiff(entry, Merge20ConclusionOpDiff, shiftedBegin, end));
	const int shiftedEnd[3] = { 14, 17, -1 };
	EXPECT_FALSE(Merge20UndoEntryMatchesDiff(entry, Merge20ConclusionOpDiff, begin, shiftedEnd));
	// Non-conclusion entries have no diff identity to check.
	const Merge20UndoEntry review = ReviewEntry(_T("a.cpp"), ReviewState::TODO, ReviewState::DONE);
	EXPECT_TRUE(Merge20UndoEntryMatchesDiff(review, Merge20ConclusionOpOldOnly, begin, end));
}

TEST(Merge20Undo, UndoDoesNotRecordLearning)
{
	// Stand-in for the learning store's record count: only the
	// record path (what Merge20SetDiffConclusion does) touches
	// it. The undo / redo path only moves values, like
	// Merge20SetDiffConclusionRestored plus the raw state
	// setters, which contain no learning call.
	int learnRecordCount = 0;
	Merge20UndoStack stack;
	// Record: a user sets a conclusion (learning +1).
	Merge20UndoEntry entry = ConclusionEntry(_T("c.cpp"), 2,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::UseNew);
	entry.newReview = ReviewState::DONE; // B1 linkage: TODO -> DONE
	++learnRecordCount;
	stack.Push(entry);
	EXPECT_EQ(learnRecordCount, 1);

	// Undo, then redo, then undo again: the stack cycle alone.
	EXPECT_TRUE(stack.PopUndoToRedo());
	EXPECT_EQ(learnRecordCount, 1);
	EXPECT_TRUE(stack.PopRedoToUndo());
	EXPECT_EQ(learnRecordCount, 1);
	EXPECT_TRUE(stack.PopUndoToRedo());
	EXPECT_EQ(learnRecordCount, 1);

	// The entry itself carries no learning payload: re-pushing
	// the same values is a no-op for learning as well.
	EXPECT_EQ(stack.RedoSize(), 1U);
	EXPECT_EQ(stack.UndoSize(), 0U);
}

TEST(Merge20Undo, SplitRegroupEntriesAreChanges)
{
	Merge20UndoStack stack;
	// A real split (5 rows) is a step; a regroup back is a step too.
	stack.Push(SplitEntry(_T("a.cpp"), Merge20UndoKind::SplitDiff));
	stack.Push(SplitEntry(_T("a.cpp"), Merge20UndoKind::RegroupDiff));
	EXPECT_EQ(stack.UndoSize(), 2U);
	// A group with fewer than two rows is not a split at all.
	Merge20UndoEntry oneRow = SplitEntry(_T("b.cpp"), Merge20UndoKind::SplitDiff);
	oneRow.splitGroup.rows = 1;
	stack.Push(oneRow);
	EXPECT_EQ(stack.UndoSize(), 2U);
	Merge20UndoEntry noRows = SplitEntry(_T("b.cpp"), Merge20UndoKind::SplitDiff);
	noRows.splitGroup.rows = 0;
	stack.Push(noRows);
	EXPECT_EQ(stack.UndoSize(), 2U);
	// The carried judgment survives the stack round-trip.
	ASSERT_NE(stack.PeekUndo(), nullptr);
	EXPECT_EQ(stack.PeekUndo()->kind, Merge20UndoKind::RegroupDiff);
	EXPECT_EQ(stack.PeekUndo()->splitGroup.rows, 5);
	EXPECT_EQ(stack.PeekUndo()->oldConclusion, Merge20DiffConclusion::UseOld);
	EXPECT_EQ(stack.PeekUndo()->splitSegmentState, 2);
	ASSERT_EQ(stack.PeekUndo()->splitSrcPanes.size(), 1U);
	EXPECT_EQ(stack.PeekUndo()->splitSrcPanes[0], 0);
}

TEST(Merge20Undo, LearnEntriesAreChangesWhenCandidateConsumed)
{
	Merge20UndoStack stack;
	// Consuming the candidate is the change, even when the
	// conclusion did not move (accepting the already-set
	// strategy still hides the candidate).
	stack.Push(LearnEntry(_T("a.cpp"), Merge20UndoKind::LearnAccept,
		Merge20DiffConclusion::UseOld, Merge20DiffConclusion::UseOld));
	stack.Push(LearnEntry(_T("a.cpp"), Merge20UndoKind::LearnDismiss,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::Undecided));
	EXPECT_EQ(stack.UndoSize(), 2U);
	// No candidate consumed: not a step.
	Merge20UndoEntry noCandidate = LearnEntry(_T("b.cpp"), Merge20UndoKind::LearnDismiss,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::Undecided);
	noCandidate.hadCandidate = false;
	stack.Push(noCandidate);
	EXPECT_EQ(stack.UndoSize(), 2U);
	// An acceptance that moved the conclusion is a step even
	// without the candidate flag (defensive: the UI always sets
	// it, but the entry must not be lost if it did not).
	Merge20UndoEntry moved = LearnEntry(_T("b.cpp"), Merge20UndoKind::LearnAccept,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::UseNew);
	moved.hadCandidate = false;
	stack.Push(moved);
	EXPECT_EQ(stack.UndoSize(), 3U);
	// The accepted source panes survive the round-trip, so a redo
	// can re-apply them without the candidate map.
	ASSERT_NE(stack.PeekUndo(), nullptr);
	ASSERT_EQ(stack.PeekUndo()->learnSrcPanes.size(), 1U);
	EXPECT_EQ(stack.PeekUndo()->learnSrcPanes[0], 0);
}

TEST(Merge20Undo, PushUndoRedoOrderAcrossAllKinds)
{
	Merge20UndoStack stack;
	stack.Push(ReviewEntry(_T("a.cpp"), ReviewState::TODO, ReviewState::DONE));
	stack.Push(SplitEntry(_T("b.cpp"), Merge20UndoKind::SplitDiff));
	stack.Push(LearnEntry(_T("c.cpp"), Merge20UndoKind::LearnAccept,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::UseOld));
	stack.Push(LearnEntry(_T("d.cpp"), Merge20UndoKind::LearnDismiss,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::Undecided));
	stack.Push(SplitEntry(_T("e.cpp"), Merge20UndoKind::RegroupDiff));
	EXPECT_EQ(stack.UndoSize(), 5U);

	// Newest first: regroup, dismiss, accept, split, review.
	const Merge20UndoKind expectedUndo[] = {
		Merge20UndoKind::RegroupDiff, Merge20UndoKind::LearnDismiss,
		Merge20UndoKind::LearnAccept, Merge20UndoKind::SplitDiff,
		Merge20UndoKind::ReviewState,
	};
	for (Merge20UndoKind kind : expectedUndo)
	{
		ASSERT_NE(stack.PeekUndo(), nullptr);
		EXPECT_EQ(stack.PeekUndo()->kind, kind);
		EXPECT_TRUE(stack.PopUndoToRedo());
	}
	EXPECT_FALSE(stack.CanUndo());
	EXPECT_EQ(stack.RedoSize(), 5U);

	// Redo walks back in the same order.
	for (int i = 4; i >= 0; --i)
	{
		ASSERT_NE(stack.PeekRedo(), nullptr);
		EXPECT_EQ(stack.PeekRedo()->kind, expectedUndo[i]);
		EXPECT_TRUE(stack.PopRedoToUndo());
	}
	EXPECT_FALSE(stack.CanRedo());
	EXPECT_EQ(stack.UndoSize(), 5U);
}

TEST(Merge20Undo, LearnIdentityMatchesSameDiff)
{
	const Merge20UndoEntry accept = LearnEntry(_T("c.cpp"), Merge20UndoKind::LearnAccept,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::UseOld);
	const Merge20UndoEntry dismiss = LearnEntry(_T("c.cpp"), Merge20UndoKind::LearnDismiss,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::Undecided);
	const int begin[3] = { 10, 12, -1 };
	const int end[3] = { 14, 16, -1 };
	EXPECT_TRUE(Merge20UndoEntryMatchesDiff(accept, Merge20ConclusionOpDiff, begin, end));
	EXPECT_TRUE(Merge20UndoEntryMatchesDiff(dismiss, Merge20ConclusionOpDiff, begin, end));
	// A split / regroup shifted the diff: both learn entries go stale.
	const int shiftedBegin[3] = { 11, 12, -1 };
	EXPECT_FALSE(Merge20UndoEntryMatchesDiff(accept, Merge20ConclusionOpDiff, shiftedBegin, end));
	EXPECT_FALSE(Merge20UndoEntryMatchesDiff(dismiss, Merge20ConclusionOpDiff, shiftedBegin, end));
	// A different op (rescan changed the diff kind) is stale too.
	EXPECT_FALSE(Merge20UndoEntryMatchesDiff(accept, Merge20ConclusionOpOldOnly, begin, end));
	// Split / regroup entries carry no diff identity: the matcher
	// never rejects them (the adapter matches by group identity).
	const Merge20UndoEntry split = SplitEntry(_T("c.cpp"), Merge20UndoKind::SplitDiff);
	EXPECT_TRUE(Merge20UndoEntryMatchesDiff(split, Merge20ConclusionOpOldOnly, shiftedBegin, end));
}

TEST(Merge20Undo, LearnUndoRedoCycleDoesNotRecordLearning)
{
	// Same stand-in as UndoDoesNotRecordLearning, for the PR2
	// kinds: the stack cycle alone never touches the record
	// count. The apply path (CMergeDoc::Merge20ApplyLearnUndo)
	// likewise contains no learning call -- an acceptance's +1
	// stays in the store as a past judgment, and a dismissal
	// never recorded one -- so undo / redo of either kind leaves
	// the learning data exactly as the original operation left it.
	int learnRecordCount = 0;
	Merge20UndoStack stack;
	// Accept: the user accepts a candidate (learning +1).
	Merge20UndoEntry accept = LearnEntry(_T("c.cpp"), Merge20UndoKind::LearnAccept,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::UseOld);
	++learnRecordCount;
	stack.Push(accept);
	// Dismiss: no record at all.
	Merge20UndoEntry dismiss = LearnEntry(_T("c.cpp"), Merge20UndoKind::LearnDismiss,
		Merge20DiffConclusion::Undecided, Merge20DiffConclusion::Undecided);
	stack.Push(dismiss);
	EXPECT_EQ(learnRecordCount, 1);

	// Undo the dismiss, redo it, undo the accept, redo it: the
	// stack cycle alone.
	EXPECT_TRUE(stack.PopUndoToRedo());
	EXPECT_EQ(learnRecordCount, 1);
	EXPECT_TRUE(stack.PopRedoToUndo());
	EXPECT_EQ(learnRecordCount, 1);
	EXPECT_TRUE(stack.PopUndoToRedo());
	EXPECT_TRUE(stack.PopUndoToRedo());
	EXPECT_EQ(learnRecordCount, 1);
	EXPECT_TRUE(stack.PopRedoToUndo());
	EXPECT_TRUE(stack.PopRedoToUndo());
	EXPECT_EQ(learnRecordCount, 1);
	EXPECT_EQ(stack.UndoSize(), 2U);
	EXPECT_EQ(stack.RedoSize(), 0U);
}

} // namespace
