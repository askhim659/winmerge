// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20FileListNotify_test.cpp
 *
 * @brief Merge20 (Panel P0/P1/P3) unit tests for the folder-compare
 * state-change notification (CDiffContext::SetMerge20FileList-
 * NotifyCallback / AddMerge20FileListNotifyCallback /
 * RemoveMerge20FileListNotifyCallback /
 * SuspendMerge20FileListNotify / ResumeMerge20FileListNotify /
 * NotifyMerge20FileListChanged, design section 12.5). The callback
 * is what lets the file-list panel refresh without polling: the
 * frame registers a callback that posts MSG_MERGE20_FILELIST_REFRESH
 * to itself.
 *
 * Only the path-based setters are covered here: the DIFFITEM
 * overloads need a built DIFFITEM, and ApplyMerge20ReviewState
 * needs one too. The callback itself is setter-independent (one
 * shared NotifyMerge20FileListChanged), so the path-based
 * setters prove the mechanism.
 *
 * This file is ASCII; the module is MFC-free, so these tests run in
 * the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "DiffContext.h"
#include "DiffWrapper.h"
#include "Merge20/Merge20Types.h"

TEST(Merge20FileListNotify, NoCallbackNoCrash)
{
	// No callback registered: the setters must stay silent no-ops
	// for notification (no crash, no throw).
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	ctxt.SetMerge20ReviewState(_T("a.txt"), ReviewState::DONE);
	EXPECT_EQ(ctxt.GetMerge20ReviewState(_T("a.txt")), ReviewState::DONE);
}

TEST(Merge20FileListNotify, ReviewStateNotifies)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nCalls = 0;
	ctxt.SetMerge20FileListNotifyCallback([&nCalls]() { ++nCalls; });
	ctxt.SetMerge20ReviewState(_T("a.txt"), ReviewState::DONE);
	EXPECT_EQ(nCalls, 1);
	ctxt.SetMerge20ReviewState(_T("a.txt"), ReviewState::HOLD);
	EXPECT_EQ(nCalls, 2);
}

TEST(Merge20FileListNotify, ClassificationNotifies)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nCalls = 0;
	ctxt.SetMerge20FileListNotifyCallback([&nCalls]() { ++nCalls; });
	Merge20FileState state;
	state.classified = true;
	ctxt.SetMerge20Classification(_T("b.txt"), state);
	EXPECT_EQ(nCalls, 1);
}

TEST(Merge20FileListNotify, MemoNotifies)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nCalls = 0;
	ctxt.SetMerge20FileListNotifyCallback([&nCalls]() { ++nCalls; });
	ctxt.SetMerge20Memo(_T("c.txt"), _T("note"));
	EXPECT_EQ(nCalls, 1);
}

TEST(Merge20FileListNotify, CallbackReplace)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nFirst = 0;
	int nSecond = 0;
	ctxt.SetMerge20FileListNotifyCallback([&nFirst]() { ++nFirst; });
	// The last registration wins (one shared context, one frame).
	ctxt.SetMerge20FileListNotifyCallback([&nSecond]() { ++nSecond; });
	ctxt.SetMerge20ReviewState(_T("d.txt"), ReviewState::DONE);
	EXPECT_EQ(nFirst, 0);
	EXPECT_EQ(nSecond, 1);
	// Clearing the callback silences notification again.
	ctxt.SetMerge20FileListNotifyCallback(nullptr);
	ctxt.SetMerge20ReviewState(_T("d.txt"), ReviewState::TODO);
	EXPECT_EQ(nSecond, 1);
}

// Merge20 (Panel P1): every frame registered with Add is notified,
// so two open file compares from the same folder compare both
// refresh (P0 notified only the last registered frame).
TEST(Merge20FileListNotify, AddMulticastBothFire)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nFirst = 0;
	int nSecond = 0;
	ctxt.AddMerge20FileListNotifyCallback(1, [&nFirst]() { ++nFirst; });
	ctxt.AddMerge20FileListNotifyCallback(2, [&nSecond]() { ++nSecond; });
	ctxt.SetMerge20ReviewState(_T("e.txt"), ReviewState::DONE);
	EXPECT_EQ(nFirst, 1);
	EXPECT_EQ(nSecond, 1);
}

// Merge20 (Panel P1): registering twice with the same key replaces
// that frame's callback instead of notifying it twice.
TEST(Merge20FileListNotify, AddSameKeyReplaces)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nFirst = 0;
	int nSecond = 0;
	ctxt.AddMerge20FileListNotifyCallback(1, [&nFirst]() { ++nFirst; });
	ctxt.AddMerge20FileListNotifyCallback(1, [&nSecond]() { ++nSecond; });
	ctxt.SetMerge20ReviewState(_T("f.txt"), ReviewState::DONE);
	EXPECT_EQ(nFirst, 0);
	EXPECT_EQ(nSecond, 1);
}

// Merge20 (Panel P1): the single Set callback and the Add callbacks
// are independent channels; both fire.
TEST(Merge20FileListNotify, AddAndSetBothFire)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nSingle = 0;
	int nExtra = 0;
	ctxt.SetMerge20FileListNotifyCallback([&nSingle]() { ++nSingle; });
	ctxt.AddMerge20FileListNotifyCallback(7, [&nExtra]() { ++nExtra; });
	ctxt.SetMerge20ReviewState(_T("g.txt"), ReviewState::DONE);
	EXPECT_EQ(nSingle, 1);
	EXPECT_EQ(nExtra, 1);
}

// Merge20 (Panel P1): while suspended, state changes are silent;
// Resume notifies exactly once with the final state, and later
// changes notify normally again.
TEST(Merge20FileListNotify, SuspendSuppressesAndResumeNotifiesOnce)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nCalls = 0;
	ctxt.SetMerge20FileListNotifyCallback([&nCalls]() { ++nCalls; });
	ctxt.SuspendMerge20FileListNotify();
	ctxt.SetMerge20ReviewState(_T("h.txt"), ReviewState::DONE);
	ctxt.SetMerge20ReviewState(_T("h.txt"), ReviewState::HOLD);
	EXPECT_EQ(nCalls, 0);
	ctxt.ResumeMerge20FileListNotify();
	EXPECT_EQ(nCalls, 1);
	ctxt.SetMerge20ReviewState(_T("h.txt"), ReviewState::TODO);
	EXPECT_EQ(nCalls, 2);
}

// Merge20 (Panel P1): nested suspends nest; only the outermost
// Resume notifies.
TEST(Merge20FileListNotify, SuspendNested)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nCalls = 0;
	ctxt.SetMerge20FileListNotifyCallback([&nCalls]() { ++nCalls; });
	ctxt.SuspendMerge20FileListNotify();
	ctxt.SuspendMerge20FileListNotify();
	ctxt.SetMerge20ReviewState(_T("i.txt"), ReviewState::DONE);
	ctxt.ResumeMerge20FileListNotify();
	EXPECT_EQ(nCalls, 0);
	ctxt.ResumeMerge20FileListNotify();
	EXPECT_EQ(nCalls, 1);
}

// Merge20 (Panel P1): Resume with no suspend and no pending change
// is a no-op (no spurious notification), and a change with no
// suspend still notifies.
TEST(Merge20FileListNotify, ResumeWithoutSuspendIsNoop)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nCalls = 0;
	ctxt.SetMerge20FileListNotifyCallback([&nCalls]() { ++nCalls; });
	ctxt.ResumeMerge20FileListNotify();
	EXPECT_EQ(nCalls, 0);
	ctxt.SetMerge20ReviewState(_T("j.txt"), ReviewState::DONE);
	EXPECT_EQ(nCalls, 1);
}

// Merge20 (Panel P3): removing one frame's registration stops its
// callback, while the other registered frames keep firing.
TEST(Merge20FileListNotify, RemoveUnregistersOne)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nFirst = 0;
	int nSecond = 0;
	ctxt.AddMerge20FileListNotifyCallback(1, [&nFirst]() { ++nFirst; });
	ctxt.AddMerge20FileListNotifyCallback(2, [&nSecond]() { ++nSecond; });
	ctxt.RemoveMerge20FileListNotifyCallback(1);
	ctxt.SetMerge20ReviewState(_T("k.txt"), ReviewState::DONE);
	EXPECT_EQ(nFirst, 0);
	EXPECT_EQ(nSecond, 1);
}

// Merge20 (Panel P3): removing an unknown key is a no-op (no crash,
// registered callbacks unaffected), so a frame destroyed without
// registering, or removed twice, is safe.
TEST(Merge20FileListNotify, RemoveUnknownKeyIsNoop)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nCalls = 0;
	ctxt.AddMerge20FileListNotifyCallback(1, [&nCalls]() { ++nCalls; });
	ctxt.RemoveMerge20FileListNotifyCallback(99);
	ctxt.RemoveMerge20FileListNotifyCallback(1);
	ctxt.RemoveMerge20FileListNotifyCallback(1);
	ctxt.SetMerge20ReviewState(_T("l.txt"), ReviewState::DONE);
	EXPECT_EQ(nCalls, 0);
}

// Merge20 (Panel P3): a removed key can be registered again (a frame
// recreated with the same window handle is notified again).
TEST(Merge20FileListNotify, RemoveThenReaddFires)
{
	CDiffContext ctxt(PathContext(), CMP_CONTENT);
	int nCalls = 0;
	ctxt.AddMerge20FileListNotifyCallback(1, [&nCalls]() { ++nCalls; });
	ctxt.RemoveMerge20FileListNotifyCallback(1);
	ctxt.SetMerge20ReviewState(_T("m.txt"), ReviewState::DONE);
	EXPECT_EQ(nCalls, 0);
	ctxt.AddMerge20FileListNotifyCallback(1, [&nCalls]() { ++nCalls; });
	ctxt.SetMerge20ReviewState(_T("m.txt"), ReviewState::HOLD);
	EXPECT_EQ(nCalls, 1);
}
