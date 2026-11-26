// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Grep_test.cpp
 *
 * @brief Merge20 (Phase 4 D7) unit tests for the Grep helpers.
 *
 * The expected values are the ground truth from the HTML tool's
 * `runGrep` (17_v17.15.html, line 1810): the query is trimmed and
 * lower-cased, every OLD/NEW line is scanned for a substring match,
 * and hits stop at 80. The excerpt is a trimmed window of at most
 * 120 characters around the hit (a C++-side addition: the HTML tool
 * shows the whole line). The history keeps the latest 10 queries,
 * most recent first (design section 4.1).
 *
 * This file is ASCII; the pure module is MFC-free, so these tests
 * run in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Grep.h"

TEST(Merge20Grep, FindInLineMatchesSubstring)
{
	EXPECT_EQ(Merge20GrepFindInLine(L"long taxAmount = 0;", L"tax"), 5);
}

TEST(Merge20Grep, FindInLineIsCaseInsensitive)
{
	// Query is already lower-cased by the caller; the line is not.
	EXPECT_EQ(Merge20GrepFindInLine(L"long TAXAmount = 0;", L"tax"), 5);
	EXPECT_EQ(Merge20GrepFindInLine(L"SELECT * FROM T;", L"select"), 0);
}

TEST(Merge20Grep, FindInLineNoMatch)
{
	EXPECT_EQ(Merge20GrepFindInLine(L"long taxAmount = 0;", L"sql"), -1);
	EXPECT_EQ(Merge20GrepFindInLine(L"short", L"longerquery"), -1);
}

TEST(Merge20Grep, FindInLineEmptyQueryNeverMatches)
{
	EXPECT_EQ(Merge20GrepFindInLine(L"long taxAmount = 0;", L""), -1);
	EXPECT_EQ(Merge20GrepFindInLine(L"", L"tax"), -1);
}

TEST(Merge20Grep, FindInLineMatchesJapanese)
{
	// \x7a0e = tax, \x984d = amount: towlower is the identity here.
	EXPECT_EQ(Merge20GrepFindInLine(L"\x7a0e\x984d\x306f\x7a0e\x7387\x3067\x3059", L"\x7a0e\x984d"), 0);
	EXPECT_EQ(Merge20GrepFindInLine(L"\x7a0e\x984d\x306f\x7a0e\x7387\x3067\x3059", L"\x7387"), 4);
}

TEST(Merge20Grep, MakeExcerptKeepsShortLine)
{
	EXPECT_EQ(Merge20GrepMakeExcerpt(L"    long taxAmount = 0;   ", 9, 3), L"long taxAmount = 0;");
}

TEST(Merge20Grep, MakeExcerptWindowsLongLine)
{
	const String line(300, L'x');
	const String withHit = line.substr(0, 150) + L"tax" + line.substr(153);
	const String excerpt = Merge20GrepMakeExcerpt(withHit, 150, 3);
	EXPECT_LE(excerpt.size(), Merge20GrepExcerptMax + 6);
	EXPECT_NE(excerpt.find(L"tax"), String::npos);
	EXPECT_EQ(excerpt.substr(0, 3), L"...");
	EXPECT_EQ(excerpt.substr(excerpt.size() - 3), L"...");
}

TEST(Merge20Grep, MakeExcerptEmptyLine)
{
	EXPECT_EQ(Merge20GrepMakeExcerpt(L"   ", 0, 0), L"");
}

TEST(Merge20Grep, ParseHistoryEmpty)
{
	EXPECT_TRUE(Merge20GrepParseHistory(L"").empty());
}

TEST(Merge20Grep, ParseHistoryMultiple)
{
	const std::vector<String> history = Merge20GrepParseHistory(L"sql\ntax\nround");
	ASSERT_EQ(history.size(), 3u);
	EXPECT_EQ(history[0], L"sql");
	EXPECT_EQ(history[1], L"tax");
	EXPECT_EQ(history[2], L"round");
}

TEST(Merge20Grep, ParseHistoryDropsEmptyEntries)
{
	const std::vector<String> history = Merge20GrepParseHistory(L"sql\n\ntax\n");
	ASSERT_EQ(history.size(), 2u);
	EXPECT_EQ(history[0], L"sql");
	EXPECT_EQ(history[1], L"tax");
}

TEST(Merge20Grep, FormatHistoryJoinsWithNewline)
{
	EXPECT_EQ(Merge20GrepFormatHistory({ L"sql", L"tax" }), L"sql\ntax");
	EXPECT_EQ(Merge20GrepFormatHistory({}), L"");
}

TEST(Merge20Grep, PushHistoryAddsToFront)
{
	const std::vector<String> history = Merge20GrepPushHistory({ L"tax" }, L"sql");
	ASSERT_EQ(history.size(), 2u);
	EXPECT_EQ(history[0], L"sql");
	EXPECT_EQ(history[1], L"tax");
}

TEST(Merge20Grep, PushHistoryMovesExistingToFront)
{
	const std::vector<String> history = Merge20GrepPushHistory({ L"sql", L"tax" }, L"tax");
	ASSERT_EQ(history.size(), 2u);
	EXPECT_EQ(history[0], L"tax");
	EXPECT_EQ(history[1], L"sql");
}

TEST(Merge20Grep, PushHistoryTrimsAndIgnoresEmpty)
{
	EXPECT_EQ(Merge20GrepPushHistory({ L"tax" }, L"  sql  ")[0], L"sql");
	EXPECT_EQ(Merge20GrepPushHistory({ L"tax" }, L"   ").size(), 1u);
}

TEST(Merge20Grep, PushHistoryCapsAtTen)
{
	std::vector<String> history;
	for (int i = 0; i < 10; ++i)
		history.push_back(L"q" + std::to_wstring(i));
	const std::vector<String> updated = Merge20GrepPushHistory(history, L"new");
	ASSERT_EQ(updated.size(), static_cast<size_t>(Merge20GrepHistoryMax));
	EXPECT_EQ(updated[0], L"new");
	EXPECT_EQ(updated[9], L"q8");
}

TEST(Merge20Grep, ReachedLimitAtEighty)
{
	EXPECT_FALSE(Merge20GrepReachedLimit(0));
	EXPECT_FALSE(Merge20GrepReachedLimit(79));
	EXPECT_TRUE(Merge20GrepReachedLimit(80));
	EXPECT_TRUE(Merge20GrepReachedLimit(81));
}
