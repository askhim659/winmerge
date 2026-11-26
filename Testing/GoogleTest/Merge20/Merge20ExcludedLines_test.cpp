/**
 * @file Merge20ExcludedLines_test.cpp
 *
 * @brief Merge20 (S0-F1) unit tests for the prediffer-blanked
 * line count.
 *
 * The count is the number of line positions where the text
 * before the prediffer has a non-blank line and the text after
 * it has a blank (empty or whitespace-only) line, or no line
 * at all. Lines already blank before the prediffer, lines the
 * prediffer changed without blanking, and lines the prediffer
 * added are never counted.
 *
 * This file stays ASCII: all test texts are ASCII.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20ExcludedLines.h"

TEST(Merge20ExcludedLines, UnchangedTextCountsZero)
{
	EXPECT_EQ(Merge20CountExcludedLines(L"a\nKEEP\nb\n", L"a\nKEEP\nb\n"), 0);
	EXPECT_EQ(Merge20CountExcludedLines(L"", L""), 0);
}

TEST(Merge20ExcludedLines, OneBlankedLine)
{
	EXPECT_EQ(Merge20CountExcludedLines(L"a\nKEEP\nb\n", L"a\n\nb\n"), 1);
}

TEST(Merge20ExcludedLines, VersionLineTwoBlanked)
{
	EXPECT_EQ(Merge20CountExcludedLines(
		L"header\n--1.0.8--2026-01-01\nbody\n",
		L"header\n\nbody\n"), 1);
}

TEST(Merge20ExcludedLines, AlreadyBlankLinesAreNotCounted)
{
	EXPECT_EQ(Merge20CountExcludedLines(L"a\n\nb\n", L"a\n\nb\n"), 0);
	// A whitespace-only line blanked to empty was already blank.
	EXPECT_EQ(Merge20CountExcludedLines(L"a\n   \nb\n", L"a\n\nb\n"), 0);
}

TEST(Merge20ExcludedLines, WhitespaceOnlyAfterCountsAsBlanked)
{
	EXPECT_EQ(Merge20CountExcludedLines(L"a\nKEEP\nb\n", L"a\n   \nb\n"), 1);
}

TEST(Merge20ExcludedLines, DeletedLineCountsAsExcluded)
{
	EXPECT_EQ(Merge20CountExcludedLines(L"a\nb\n", L"a\n"), 1);
	EXPECT_EQ(Merge20CountExcludedLines(L"a", L""), 1);
}

TEST(Merge20ExcludedLines, AddedLinesAreNotCounted)
{
	EXPECT_EQ(Merge20CountExcludedLines(L"a\n", L"a\nb\n"), 0);
}

TEST(Merge20ExcludedLines, ChangedButNotBlankedIsNotCounted)
{
	EXPECT_EQ(Merge20CountExcludedLines(L"a\nold\n", L"a\nnew\n"), 0);
}

TEST(Merge20ExcludedLines, CrLfLineBreaks)
{
	EXPECT_EQ(Merge20CountExcludedLines(L"a\r\nKEEP\r\nb\r\n", L"a\r\n\r\nb\r\n"), 1);
	EXPECT_EQ(Merge20CountExcludedLines(L"a\r\nKEEP\r\n", L"a\r\nKEEP\r\n"), 0);
}

TEST(Merge20ExcludedLines, CrOnlyLineBreaks)
{
	EXPECT_EQ(Merge20CountExcludedLines(L"a\rKEEP\r", L"a\r\r"), 1);
}

TEST(Merge20ExcludedLines, MultipleBlankedLines)
{
	EXPECT_EQ(Merge20CountExcludedLines(
		L"one\ntwo\nthree\nfour\n",
		L"\n\nthree\n\n"), 3);
}

TEST(Merge20ExcludedLines, NoTrailingEolAndTrailingBreak)
{
	// No trailing EOL: the last line still counts when blanked.
	EXPECT_EQ(Merge20CountExcludedLines(L"a\nb", L"a\n"), 1);
	// A trailing line break makes no phantom extra line.
	EXPECT_EQ(Merge20CountExcludedLines(L"a\n", L"a\n"), 0);
	EXPECT_EQ(Merge20CountExcludedLines(L"a\n", L"\n"), 1);
}
