/**
 * @file Merge20Summary_test.cpp
 *
 * @brief Merge20 (B9) unit tests for the end-of-session summary counts.
 *
 * The expected values follow the gap analysis section 4.2 B9 and the
 * H-DASH seven kinds (coverage matrix H-36): total, with differences,
 * changed, old-only, new-only, identical, needs manual work, plus
 * unreviewed (TODO) and output targets. The three existence kinds
 * are disjoint and together with identical sum to the total.
 *
 * This file stays ASCII: no Japanese text is needed here (the count
 * labels are composed by the dialog through `_()`).
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Summary.h"

namespace
{

	// One summary record with the given flags (review TODO and output
	// off unless stated), so each test reads as a small file list.
	Merge20SummaryFile SummaryFile(bool hasDiff, bool isModified, bool isOldOnly, bool isNewOnly,
		ReviewState review = ReviewState::TODO, bool includeInOutput = false)
	{
		Merge20SummaryFile file;
		file.hasDiff = hasDiff;
		file.isModified = isModified;
		file.isOldOnly = isOldOnly;
		file.isNewOnly = isNewOnly;
		file.review = review;
		file.includeInOutput = includeInOutput;
		return file;
	}

} // namespace

TEST(Merge20Summary, EmptyInput)
{
	const Merge20SummaryCounts counts = Merge20SummaryComputeCounts({});
	EXPECT_EQ(counts.total, 0);
	EXPECT_EQ(counts.diffCount, 0);
	EXPECT_EQ(counts.modified, 0);
	EXPECT_EQ(counts.oldOnly, 0);
	EXPECT_EQ(counts.newOnly, 0);
	EXPECT_EQ(counts.identical, 0);
	EXPECT_EQ(counts.special, 0);
	EXPECT_EQ(counts.undecided, 0);
	EXPECT_EQ(counts.outputTarget, 0);
	EXPECT_FALSE(Merge20SummaryHasUndecided(counts));
}

TEST(Merge20Summary, AllIdenticalReviewed)
{
	// Two identical files, both reviewed, both output targets.
	const std::vector<Merge20SummaryFile> files =
	{
		SummaryFile(false, false, false, false, ReviewState::DONE, true),
		SummaryFile(false, false, false, false, ReviewState::DONE, true),
	};
	const Merge20SummaryCounts counts = Merge20SummaryComputeCounts(files);
	EXPECT_EQ(counts.total, 2);
	EXPECT_EQ(counts.diffCount, 0);
	EXPECT_EQ(counts.identical, 2);
	EXPECT_EQ(counts.undecided, 0);
	EXPECT_EQ(counts.outputTarget, 2);
	EXPECT_FALSE(Merge20SummaryHasUndecided(counts));
}

TEST(Merge20Summary, MixedFiles)
{
	// changed DONE/on, old-only TODO/off, new-only SPECIAL/on,
	// identical TODO/on, changed HOLD/off, new-only KEEP/on.
	const std::vector<Merge20SummaryFile> files =
	{
		SummaryFile(true, true, false, false, ReviewState::DONE, true),
		SummaryFile(true, false, true, false, ReviewState::TODO, false),
		SummaryFile(true, false, false, true, ReviewState::SPECIAL, true),
		SummaryFile(false, false, false, false, ReviewState::TODO, true),
		SummaryFile(true, true, false, false, ReviewState::HOLD, false),
		SummaryFile(true, false, false, true, ReviewState::KEEP, true),
	};
	const Merge20SummaryCounts counts = Merge20SummaryComputeCounts(files);
	EXPECT_EQ(counts.total, 6);
	EXPECT_EQ(counts.diffCount, 5);
	EXPECT_EQ(counts.modified, 2);
	EXPECT_EQ(counts.oldOnly, 1);
	EXPECT_EQ(counts.newOnly, 2);
	EXPECT_EQ(counts.identical, 1);
	EXPECT_EQ(counts.special, 1);
	EXPECT_EQ(counts.undecided, 2);
	EXPECT_EQ(counts.outputTarget, 4);
	// The three existence kinds plus identical sum to the total.
	EXPECT_EQ(counts.modified + counts.oldOnly + counts.newOnly + counts.identical, counts.total);
	EXPECT_TRUE(Merge20SummaryHasUndecided(counts));
}

TEST(Merge20Summary, UndecidedDrivesWarning)
{
	// One unreviewed file is enough for the warning; reviewing it
	// clears the warning without changing the other counts.
	const std::vector<Merge20SummaryFile> before =
	{
		SummaryFile(true, true, false, false, ReviewState::TODO, true),
	};
	EXPECT_TRUE(Merge20SummaryHasUndecided(Merge20SummaryComputeCounts(before)));
	const std::vector<Merge20SummaryFile> after =
	{
		SummaryFile(true, true, false, false, ReviewState::DONE, true),
	};
	const Merge20SummaryCounts counts = Merge20SummaryComputeCounts(after);
	EXPECT_EQ(counts.undecided, 0);
	EXPECT_EQ(counts.total, 1);
	EXPECT_EQ(counts.diffCount, 1);
	EXPECT_EQ(counts.outputTarget, 1);
	EXPECT_FALSE(Merge20SummaryHasUndecided(counts));
}
