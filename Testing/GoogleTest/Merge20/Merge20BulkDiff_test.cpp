/**
 * @file Merge20BulkDiff_test.cpp
 *
 * @brief Merge20 (D10) unit tests for the bulk line differ.
 *
 * The differ is a port of the HTML tool's `diffFast` / `lcs`
 * (17_v17.15.html, lines 1048-1072). The op stream is cross-checked
 * against the HTML functions themselves in Node (see the local
 * harness in the D10 notes); these tests pin the range grouping,
 * the trivial handling and the simple-compare fallback.
 *
 * This file stays ASCII.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20BulkDiff.h"

namespace
{

	std::vector<String> Lines(std::initializer_list<const wchar_t*> lines)
	{
		std::vector<String> out;
		for (const wchar_t* line : lines)
			out.emplace_back(line);
		return out;
	}

	Merge20BulkDiffOptions Options()
	{
		Merge20BulkDiffOptions options;
		options.ignoreWhitespace = true;
		options.ignoreCase = true;
		options.ignoreBlankLines = false;
		options.simpleThreshold = Merge20BulkSimpleThresholdDefault;
		return options;
	}

} // namespace

TEST(Merge20BulkDiff, IdenticalFilesHaveNoRanges)
{
	const Merge20BulkDiffResult r = Merge20BulkDiff(
		Lines({ L"a", L"b", L"c" }), Lines({ L"a", L"b", L"c" }), Options());
	EXPECT_TRUE(r.ranges.empty());
	EXPECT_FALSE(r.simpleUsed);
}

TEST(Merge20BulkDiff, OneLineChangedIsOneDiffRange)
{
	const Merge20BulkDiffResult r = Merge20BulkDiff(
		Lines({ L"a", L"b", L"c" }), Lines({ L"a", L"B2", L"c" }), Options());
	ASSERT_EQ(r.ranges.size(), 1u);
	EXPECT_EQ(r.ranges[0].op, OP_DIFF);
	EXPECT_EQ(r.ranges[0].beginOld, 1);
	EXPECT_EQ(r.ranges[0].endOld, 1);
	EXPECT_EQ(r.ranges[0].beginNew, 1);
	EXPECT_EQ(r.ranges[0].endNew, 1);
	EXPECT_FALSE(r.simpleUsed);
}

TEST(Merge20BulkDiff, AddedAndMissingLinesAreSeparateRanges)
{
	const Merge20BulkDiffResult r = Merge20BulkDiff(
		Lines({ L"a", L"gone", L"c", L"d" }), Lines({ L"a", L"c", L"d", L"new" }), Options());
	ASSERT_EQ(r.ranges.size(), 2u);
	EXPECT_EQ(r.ranges[0].op, OP_1STONLY);
	EXPECT_EQ(r.ranges[0].beginOld, 1);
	EXPECT_EQ(r.ranges[0].endOld, 1);
	EXPECT_EQ(r.ranges[0].beginNew, 1);
	EXPECT_EQ(r.ranges[0].endNew, 0);
	EXPECT_EQ(r.ranges[1].op, OP_2NDONLY);
	EXPECT_EQ(r.ranges[1].beginNew, 3);
	EXPECT_EQ(r.ranges[1].endNew, 3);
	EXPECT_EQ(r.ranges[1].beginOld, 4);
	EXPECT_EQ(r.ranges[1].endOld, 3);
}

TEST(Merge20BulkDiff, WhitespaceOnlyChangeIsTrivial)
{
	Merge20BulkDiffOptions options = Options();
	const Merge20BulkDiffResult r = Merge20BulkDiff(
		Lines({ L"a", L"  b  c", L"d" }), Lines({ L"a", L"b c", L"d" }), options);
	ASSERT_EQ(r.ranges.size(), 1u);
	EXPECT_EQ(r.ranges[0].op, OP_TRIVIAL);
	EXPECT_EQ(r.ranges[0].beginOld, 1);
	EXPECT_EQ(r.ranges[0].endOld, 1);

	options.ignoreWhitespace = false;
	const Merge20BulkDiffResult r2 = Merge20BulkDiff(
		Lines({ L"a", L"  b  c", L"d" }), Lines({ L"a", L"b c", L"d" }), options);
	ASSERT_EQ(r2.ranges.size(), 1u);
	EXPECT_EQ(r2.ranges[0].op, OP_DIFF);
}

TEST(Merge20BulkDiff, CaseOnlyChangeIsTrivial)
{
	Merge20BulkDiffOptions options = Options();
	const Merge20BulkDiffResult r = Merge20BulkDiff(
		Lines({ L"a", L"Hello", L"d" }), Lines({ L"a", L"HELLO", L"d" }), options);
	ASSERT_EQ(r.ranges.size(), 1u);
	EXPECT_EQ(r.ranges[0].op, OP_TRIVIAL);

	options.ignoreCase = false;
	const Merge20BulkDiffResult r2 = Merge20BulkDiff(
		Lines({ L"a", L"Hello", L"d" }), Lines({ L"a", L"HELLO", L"d" }), options);
	ASSERT_EQ(r2.ranges.size(), 1u);
	EXPECT_EQ(r2.ranges[0].op, OP_DIFF);
}

TEST(Merge20BulkDiff, BlankOnlyChangeIsTrivialWhenEnabled)
{
	Merge20BulkDiffOptions options = Options();
	options.ignoreBlankLines = true;
	const Merge20BulkDiffResult r = Merge20BulkDiff(
		Lines({ L"a", L"", L"c" }), Lines({ L"a", L"c" }), options);
	ASSERT_EQ(r.ranges.size(), 1u);
	EXPECT_EQ(r.ranges[0].op, OP_TRIVIAL);

	options.ignoreBlankLines = false;
	const Merge20BulkDiffResult r2 = Merge20BulkDiff(
		Lines({ L"a", L"", L"c" }), Lines({ L"a", L"c" }), options);
	ASSERT_EQ(r2.ranges.size(), 1u);
	EXPECT_EQ(r2.ranges[0].op, OP_1STONLY);
}

TEST(Merge20BulkDiff, HugeMiddleFallsBackToSimple)
{
	Merge20BulkDiffOptions options = Options();
	options.simpleThreshold = 10;
	std::vector<String> oldLines, newLines;
	for (int i = 0; i < 10; ++i)
	{
		oldLines.push_back(String(L"old") + std::to_wstring(i));
		newLines.push_back(String(L"new") + std::to_wstring(i));
	}
	const Merge20BulkDiffResult r = Merge20BulkDiff(oldLines, newLines, options);
	EXPECT_TRUE(r.simpleUsed);
	// Positional alignment: every position differs, so one range
	// covers all lines on both sides.
	ASSERT_EQ(r.ranges.size(), 1u);
	EXPECT_EQ(r.ranges[0].op, OP_DIFF);
	EXPECT_EQ(r.ranges[0].beginOld, 0);
	EXPECT_EQ(r.ranges[0].endOld, 9);
	EXPECT_EQ(r.ranges[0].beginNew, 0);
	EXPECT_EQ(r.ranges[0].endNew, 9);
}

TEST(Merge20BulkDiff, IdenticalHugeFilesDoNotUseSimple)
{
	// The threshold applies to the trimmed middle (HTML `lcs`):
	// identical files have an empty middle, so no fallback runs.
	Merge20BulkDiffOptions options = Options();
	options.simpleThreshold = 10;
	std::vector<String> oldLines, newLines;
	for (int i = 0; i < 10; ++i)
	{
		oldLines.push_back(String(L"line") + std::to_wstring(i));
		newLines.push_back(String(L"line") + std::to_wstring(i));
	}
	const Merge20BulkDiffResult r = Merge20BulkDiff(oldLines, newLines, options);
	EXPECT_FALSE(r.simpleUsed);
	EXPECT_TRUE(r.ranges.empty());
}

TEST(Merge20BulkDiff, ThresholdZeroDisablesFallback)
{
	Merge20BulkDiffOptions options = Options();
	options.simpleThreshold = 0;
	std::vector<String> oldLines, newLines;
	for (int i = 0; i < 10; ++i)
	{
		oldLines.push_back(String(L"old") + std::to_wstring(i));
		newLines.push_back(String(L"new") + std::to_wstring(i));
	}
	const Merge20BulkDiffResult r = Merge20BulkDiff(oldLines, newLines, options);
	EXPECT_FALSE(r.simpleUsed);
	ASSERT_EQ(r.ranges.size(), 1u);
	EXPECT_EQ(r.ranges[0].op, OP_DIFF);
}

TEST(Merge20BulkDiff, MovedBlockIsDelPlusAdd)
{
	const Merge20BulkDiffResult r = Merge20BulkDiff(
		Lines({ L"block1", L"block2", L"keep1", L"keep2" }),
		Lines({ L"keep1", L"keep2", L"block1", L"block2" }), Options());
	ASSERT_EQ(r.ranges.size(), 2u);
	EXPECT_EQ(r.ranges[0].op, OP_1STONLY);
	EXPECT_EQ(r.ranges[0].beginOld, 0);
	EXPECT_EQ(r.ranges[0].endOld, 1);
	EXPECT_EQ(r.ranges[1].op, OP_2NDONLY);
	EXPECT_EQ(r.ranges[1].beginNew, 2);
	EXPECT_EQ(r.ranges[1].endNew, 3);
}

TEST(Merge20BulkDiff, EmptyOldFileIsOneAddRange)
{
	const Merge20BulkDiffResult r = Merge20BulkDiff(
		Lines({}), Lines({ L"a", L"b", L"c" }), Options());
	ASSERT_EQ(r.ranges.size(), 1u);
	EXPECT_EQ(r.ranges[0].op, OP_2NDONLY);
	EXPECT_EQ(r.ranges[0].beginNew, 0);
	EXPECT_EQ(r.ranges[0].endNew, 2);
	EXPECT_EQ(r.ranges[0].beginOld, 0);
	EXPECT_EQ(r.ranges[0].endOld, -1);
}

TEST(Merge20BulkDiff, BothEmptyFilesHaveNoRanges)
{
	const Merge20BulkDiffResult r = Merge20BulkDiff(Lines({}), Lines({}), Options());
	EXPECT_TRUE(r.ranges.empty());
	EXPECT_FALSE(r.simpleUsed);
}

namespace
{

	std::vector<String> NumberedLines(const wchar_t* prefix, int count)
	{
		std::vector<String> out;
		for (int i = 0; i < count; ++i)
			out.push_back(String(prefix) + std::to_wstring(i));
		return out;
	}

	// Merge20BulkIsSimple over two vectors (the same reader shape the
	// opened-file path uses over its text buffers).
	bool IsSimpleForVectors(const std::vector<String>& oldLines, const std::vector<String>& newLines,
		const Merge20BulkDiffOptions& options)
	{
		const Merge20BulkLineReader getOldLine = [&](int line, String& text) -> bool {
			if (line < 0 || line >= static_cast<int>(oldLines.size()))
				return false;
			text = oldLines[static_cast<size_t>(line)];
			return true;
		};
		const Merge20BulkLineReader getNewLine = [&](int line, String& text) -> bool {
			if (line < 0 || line >= static_cast<int>(newLines.size()))
				return false;
			text = newLines[static_cast<size_t>(line)];
			return true;
		};
		return Merge20BulkIsSimple(getOldLine, static_cast<int>(oldLines.size()),
			getNewLine, static_cast<int>(newLines.size()), options);
	}

} // namespace

TEST(Merge20BulkIsSimple, HugeMiddleTriggers)
{
	Merge20BulkDiffOptions options = Options();
	options.simpleThreshold = 10;
	EXPECT_TRUE(IsSimpleForVectors(NumberedLines(L"old", 10), NumberedLines(L"new", 10), options));
}

TEST(Merge20BulkIsSimple, IdenticalFilesDoNotTrigger)
{
	// The threshold applies to the trimmed middle (HTML `lcs`):
	// identical files have an empty middle, so no fallback runs.
	Merge20BulkDiffOptions options = Options();
	options.simpleThreshold = 10;
	EXPECT_FALSE(IsSimpleForVectors(NumberedLines(L"line", 10), NumberedLines(L"line", 10), options));
}

TEST(Merge20BulkIsSimple, ThresholdZeroDisables)
{
	Merge20BulkDiffOptions options = Options();
	options.simpleThreshold = 0;
	EXPECT_FALSE(IsSimpleForVectors(NumberedLines(L"old", 10), NumberedLines(L"new", 10), options));
}

TEST(Merge20BulkIsSimple, HugeFilesWithTinyMiddleDoNotTrigger)
{
	// The key case for H-SIMPLE: 5000-line files that differ only in
	// 10 middle lines have a 10x10 middle, far below the threshold,
	// even though the untrimmed 5000x5000 product exceeds it. The
	// badge must NOT appear for these files.
	std::vector<String> oldLines = NumberedLines(L"line", 5000);
	std::vector<String> newLines = NumberedLines(L"line", 5000);
	for (int i = 0; i < 10; ++i)
		newLines[static_cast<size_t>(2490 + i)] = String(L"changed") + std::to_wstring(i);
	EXPECT_FALSE(IsSimpleForVectors(oldLines, newLines, Options()));
}

TEST(Merge20BulkIsSimple, DefaultThresholdBoundary)
{
	// HTML `lcs`: n*m > 2200000 (strict). 2000x2000 all different
	// is 4,000,000 (triggers); 1000x1000 is 1,000,000 (does not).
	EXPECT_TRUE(IsSimpleForVectors(NumberedLines(L"old", 2000), NumberedLines(L"new", 2000), Options()));
	EXPECT_FALSE(IsSimpleForVectors(NumberedLines(L"old", 1000), NumberedLines(L"new", 1000), Options()));
	// Exact boundary: 10x10 with threshold 100 does not trigger.
	Merge20BulkDiffOptions options = Options();
	options.simpleThreshold = 100;
	EXPECT_FALSE(IsSimpleForVectors(NumberedLines(L"old", 10), NumberedLines(L"new", 10), options));
	options.simpleThreshold = 99;
	EXPECT_TRUE(IsSimpleForVectors(NumberedLines(L"old", 10), NumberedLines(L"new", 10), options));
}

TEST(Merge20BulkIsSimple, WhitespaceOnlyDifferencesDoNotTrigger)
{
	// Normalized-equal lines trim away, so the middle is empty.
	Merge20BulkDiffOptions options = Options();
	options.simpleThreshold = 1;
	EXPECT_FALSE(IsSimpleForVectors(
		Lines({ L"a", L"  b  c ", L"d" }), Lines({ L"a", L"b c", L"d" }), options));
}

TEST(Merge20BulkIsSimple, CaseOnlyDifferencesFollowIgnoreCase)
{
	Merge20BulkDiffOptions options = Options();
	options.simpleThreshold = 1;
	EXPECT_FALSE(IsSimpleForVectors(Lines({ L"A", L"B" }), Lines({ L"a", L"b" }), options));
	options.ignoreCase = false;
	EXPECT_TRUE(IsSimpleForVectors(Lines({ L"A", L"B" }), Lines({ L"a", L"b" }), options));
}

TEST(Merge20BulkIsSimple, EmptySidesDoNotTrigger)
{
	EXPECT_FALSE(IsSimpleForVectors(Lines({}), Lines({ L"a", L"b" }), Options()));
	EXPECT_FALSE(IsSimpleForVectors(Lines({}), Lines({}), Options()));
}

TEST(Merge20BulkIsSimple, AgreesWithBulkDiffer)
{
	// The shared decision: for the same inputs the check and the
	// differ's simpleUsed must agree, or the badge would flicker
	// when a bulk-classified file is opened.
	const std::vector<std::pair<std::vector<String>, std::vector<String>>> cases =
	{
		{ NumberedLines(L"old", 10), NumberedLines(L"new", 10) },
		{ NumberedLines(L"line", 10), NumberedLines(L"line", 10) },
		{ Lines({ L"a", L"b", L"c" }), Lines({ L"a", L"B2", L"c" }) },
		{ Lines({ L"block1", L"block2", L"keep1", L"keep2" }), Lines({ L"keep1", L"keep2", L"block1", L"block2" }) },
		{ Lines({}), Lines({ L"a", L"b", L"c" }) },
	};
	Merge20BulkDiffOptions options = Options();
	options.simpleThreshold = 10;
	for (const auto& c : cases)
	{
		EXPECT_EQ(IsSimpleForVectors(c.first, c.second, options),
			Merge20BulkDiff(c.first, c.second, options).simpleUsed);
	}
}
