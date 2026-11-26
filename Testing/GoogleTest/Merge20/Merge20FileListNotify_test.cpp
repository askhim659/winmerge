// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20FileListNotify_test.cpp
 *
 * @brief Merge20 (Panel P0) unit tests for the folder-compare
 * state-change notification (CDiffContext::SetMerge20FileList-
 * NotifyCallback / NotifyMerge20FileListChanged, design section
 * 12.5). The callback is what lets the file-list panel refresh
 * without polling: the frame registers a callback that posts
 * MSG_MERGE20_FILELIST_REFRESH to itself.
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
