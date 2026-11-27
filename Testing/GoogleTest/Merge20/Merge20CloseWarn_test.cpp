/**
 * @file Merge20CloseWarn_test.cpp
 *
 * @brief Merge20 (B2 stage 1) unit tests for the undecided-items
 * warning counts.
 *
 * The expected values follow the counting rules in
 * Merge20CloseWarn.h: undecidedFiles counts TODO files,
 * undecidedDiffs counts recorded diff judgments whose B1
 * conclusion is Undecided (files never opened in a merge session
 * have no recorded diffs and contribute 0), and highRisk counts
 * the undecided items in HIGH-risk files, so it is always <=
 * undecidedFiles + undecidedDiffs. The acceptance scenario of
 * the task (2 unreviewed files, 3 undecided diffs) is covered by
 * AcceptanceScenario below.
 *
 * This file stays ASCII: no Japanese text is needed here (the count
 * labels are composed by the dialog through `_()`).
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20CloseWarn.h"

namespace
{

	// One close-warning record: the given review state and risk,
	// with one conclusion ordinal per recorded diff judgment.
	Merge20CloseWarnFile CloseWarnFile(ReviewState review, RiskLevel risk,
		std::vector<int> conclusions = {})
	{
		Merge20CloseWarnFile file;
		file.review = review;
		file.risk = risk;
		file.diffConclusions = std::move(conclusions);
		return file;
	}

	// Conclusion ordinals in B1 vocabulary, so the tests read as
	// "undecided / decided" rather than bare ints.
	int Undecided() { return static_cast<int>(Merge20DiffConclusion::Undecided); }
	int UseOld() { return static_cast<int>(Merge20DiffConclusion::UseOld); }
	int UseNew() { return static_cast<int>(Merge20DiffConclusion::UseNew); }
	int Keep() { return static_cast<int>(Merge20DiffConclusion::Keep); }

} // namespace

TEST(Merge20CloseWarn, EmptyInput)
{
	const Merge20CloseWarnCounts counts = Merge20CloseWarnComputeCounts({});
	EXPECT_EQ(counts.total, 0);
	EXPECT_EQ(counts.undecidedFiles, 0);
	EXPECT_EQ(counts.undecidedDiffs, 0);
	EXPECT_EQ(counts.highRisk, 0);
	EXPECT_FALSE(Merge20CloseWarnHasUndecided(counts));
}

TEST(Merge20CloseWarn, AllDecided)
{
	// Two reviewed files, every recorded diff decided: no warning.
	const std::vector<Merge20CloseWarnFile> files =
	{
		CloseWarnFile(ReviewState::DONE, RiskLevel::HIGH, { UseOld(), UseNew() }),
		CloseWarnFile(ReviewState::DONE, RiskLevel::NONE, { Keep() }),
	};
	const Merge20CloseWarnCounts counts = Merge20CloseWarnComputeCounts(files);
	EXPECT_EQ(counts.total, 2);
	EXPECT_EQ(counts.undecidedFiles, 0);
	EXPECT_EQ(counts.undecidedDiffs, 0);
	EXPECT_EQ(counts.highRisk, 0);
	EXPECT_FALSE(Merge20CloseWarnHasUndecided(counts));
}

TEST(Merge20CloseWarn, AcceptanceScenario)
{
	// The task's acceptance scenario: 2 unreviewed files and 3
	// undecided diffs. File A is TODO and HIGH risk with 2
	// undecided diffs and 1 decided one; file B is TODO with 1
	// undecided diff; file C is DONE and HIGH risk with decided
	// diffs only; file D is DONE with no recorded diffs.
	const std::vector<Merge20CloseWarnFile> files =
	{
		CloseWarnFile(ReviewState::TODO, RiskLevel::HIGH, { Undecided(), Undecided(), UseOld() }),
		CloseWarnFile(ReviewState::TODO, RiskLevel::NONE, { Undecided() }),
		CloseWarnFile(ReviewState::DONE, RiskLevel::HIGH, { UseNew(), Keep() }),
		CloseWarnFile(ReviewState::DONE, RiskLevel::NONE, {}),
	};
	const Merge20CloseWarnCounts counts = Merge20CloseWarnComputeCounts(files);
	EXPECT_EQ(counts.total, 4);
	EXPECT_EQ(counts.undecidedFiles, 2);
	EXPECT_EQ(counts.undecidedDiffs, 3);
	// High risk: file A itself (TODO + HIGH) plus its 2 undecided
	// diffs. File C is HIGH but decided, so it adds nothing.
	EXPECT_EQ(counts.highRisk, 3);
	EXPECT_TRUE(Merge20CloseWarnHasUndecided(counts));
}

TEST(Merge20CloseWarn, FilesOnlyUndecided)
{
	// TODO files that were never opened have no recorded diffs:
	// they count as files only, and their diffs are not invented.
	const std::vector<Merge20CloseWarnFile> files =
	{
		CloseWarnFile(ReviewState::TODO, RiskLevel::HIGH, {}),
		CloseWarnFile(ReviewState::TODO, RiskLevel::MEDIUM, {}),
		CloseWarnFile(ReviewState::DONE, RiskLevel::NONE, {}),
	};
	const Merge20CloseWarnCounts counts = Merge20CloseWarnComputeCounts(files);
	EXPECT_EQ(counts.total, 3);
	EXPECT_EQ(counts.undecidedFiles, 2);
	EXPECT_EQ(counts.undecidedDiffs, 0);
	EXPECT_EQ(counts.highRisk, 1);
	EXPECT_TRUE(Merge20CloseWarnHasUndecided(counts));
}

TEST(Merge20CloseWarn, DiffsOnlyUndecided)
{
	// A DONE file can still hold undecided diffs (its review was
	// set by key without judging the diffs): the warning still
	// fires, on the diffs alone.
	const std::vector<Merge20CloseWarnFile> files =
	{
		CloseWarnFile(ReviewState::DONE, RiskLevel::HIGH, { Undecided(), Undecided() }),
		CloseWarnFile(ReviewState::DONE, RiskLevel::NONE, { Undecided() }),
	};
	const Merge20CloseWarnCounts counts = Merge20CloseWarnComputeCounts(files);
	EXPECT_EQ(counts.total, 2);
	EXPECT_EQ(counts.undecidedFiles, 0);
	EXPECT_EQ(counts.undecidedDiffs, 3);
	EXPECT_EQ(counts.highRisk, 2);
	EXPECT_TRUE(Merge20CloseWarnHasUndecided(counts));
}

TEST(Merge20CloseWarn, DecidingClearsWarning)
{
	// Deciding the last undecided diff and reviewing the last TODO
	// file clears the warning without changing the total.
	const std::vector<Merge20CloseWarnFile> before =
	{
		CloseWarnFile(ReviewState::TODO, RiskLevel::HIGH, { Undecided() }),
	};
	EXPECT_TRUE(Merge20CloseWarnHasUndecided(Merge20CloseWarnComputeCounts(before)));
	const std::vector<Merge20CloseWarnFile> after =
	{
		CloseWarnFile(ReviewState::DONE, RiskLevel::HIGH, { UseOld() }),
	};
	const Merge20CloseWarnCounts counts = Merge20CloseWarnComputeCounts(after);
	EXPECT_EQ(counts.total, 1);
	EXPECT_EQ(counts.undecidedFiles, 0);
	EXPECT_EQ(counts.undecidedDiffs, 0);
	EXPECT_EQ(counts.highRisk, 0);
	EXPECT_FALSE(Merge20CloseWarnHasUndecided(counts));
}

TEST(Merge20CloseWarn, HighRiskNeverExceedsUndecided)
{
	// High risk is a subset of the undecided items: files + diffs.
	const std::vector<Merge20CloseWarnFile> files =
	{
		CloseWarnFile(ReviewState::TODO, RiskLevel::HIGH, { Undecided(), Undecided() }),
		CloseWarnFile(ReviewState::SPECIAL, RiskLevel::HIGH, { Undecided() }),
		CloseWarnFile(ReviewState::HOLD, RiskLevel::HIGH, {}),
		CloseWarnFile(ReviewState::KEEP, RiskLevel::NONE, { Undecided() }),
	};
	const Merge20CloseWarnCounts counts = Merge20CloseWarnComputeCounts(files);
	EXPECT_EQ(counts.undecidedFiles, 1);
	EXPECT_EQ(counts.undecidedDiffs, 4);
	// File A (TODO + HIGH) + its 2 undecided diffs + the SPECIAL
	// file's 1 undecided diff in a HIGH-risk file. HOLD and KEEP
	// files are not TODO, so they add no file count.
	EXPECT_EQ(counts.highRisk, 4);
	EXPECT_LE(counts.highRisk, counts.undecidedFiles + counts.undecidedDiffs);
}
