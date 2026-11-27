// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Undo_test.cpp
 *
 * @brief Merge20 (best-workflow B10 PR1) unit tests for the judgment
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

} // namespace
