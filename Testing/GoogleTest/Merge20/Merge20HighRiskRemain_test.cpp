/**
 * @file Merge20HighRiskRemain_test.cpp
 *
 * @brief Merge20 (B4) unit tests for the undecided high-risk
 * remainder display.
 *
 * Two layers, both pure: the display-update decision
 * (Merge20HighRiskRemainDecideUpdate) and the count it displays,
 * which is B2's highRisk (Merge20CloseWarnComputeCounts), so the
 * tests also prove B4's N and B2's Z are the same value. The
 * acceptance scenario of the task (3 high-risk unreviewed files,
 * then one DONE, then all DONE) is covered by
 * AcceptanceScenario below; the diff-level case (a DONE file
 * that still holds undecided diffs) by DiffsKeepRemainder, and
 * the 100-1000-file scale by ScaleThousandFiles.
 *
 * What is NOT tested here: the on-screen rendering itself (the
 * panel bar, its static control and the Japanese translation).
 * That stays real-machine unverified, as for the other slices.
 *
 * This file stays ASCII: no Japanese text is needed here (the
 * remainder text is composed by the panel bar through `_()`).
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20HighRiskRemain.h"
#include "Merge20/Merge20CloseWarn.h"

namespace
{

	// One close-warning record: the given review state and risk,
	// with one conclusion ordinal per recorded diff judgment.
	Merge20CloseWarnFile RemainFile(ReviewState review, RiskLevel risk,
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

	// The B4 remainder decision for a fresh B2 count, from a
	// hidden line showing nothing (the panel's initial state).
	Merge20HighRiskRemainUpdate DecideFromHidden(int highRisk)
	{
		return Merge20HighRiskRemainDecideUpdate(false, 0, highRisk);
	}

} // namespace

TEST(Merge20HighRiskRemain, InitialShow)
{
	// First Refresh with a remainder: the line appears with N.
	const Merge20HighRiskRemainUpdate update = DecideFromHidden(3);
	EXPECT_TRUE(update.changed);
	EXPECT_TRUE(update.visible);
	EXPECT_EQ(update.highRisk, 3);
}

TEST(Merge20HighRiskRemain, UnchangedCountIsNoop)
{
	// A Refresh with the same count touches nothing: no text
	// set, no show/hide churn on a 100-1000-file compare, where
	// Refresh() runs on every review-state notification.
	const Merge20HighRiskRemainUpdate update =
		Merge20HighRiskRemainDecideUpdate(true, 3, 3);
	EXPECT_FALSE(update.changed);
	EXPECT_TRUE(update.visible);
	EXPECT_EQ(update.highRisk, 3);
}

TEST(Merge20HighRiskRemain, CountDecreaseUpdates)
{
	// Judging one file drops the remainder: the line updates.
	const Merge20HighRiskRemainUpdate update =
		Merge20HighRiskRemainDecideUpdate(true, 3, 2);
	EXPECT_TRUE(update.changed);
	EXPECT_TRUE(update.visible);
	EXPECT_EQ(update.highRisk, 2);
}

TEST(Merge20HighRiskRemain, ZeroHides)
{
	// The last remainder judged: the line hides (it is never a
	// warning display at 0).
	const Merge20HighRiskRemainUpdate update =
		Merge20HighRiskRemainDecideUpdate(true, 2, 0);
	EXPECT_TRUE(update.changed);
	EXPECT_FALSE(update.visible);
	EXPECT_EQ(update.highRisk, 0);
}

TEST(Merge20HighRiskRemain, ZeroStaysHidden)
{
	// No remainder, none before: the first Refresh touches
	// nothing, and a later Refresh with 0 still touches nothing.
	EXPECT_FALSE(DecideFromHidden(0).changed);
	EXPECT_FALSE(DecideFromHidden(0).visible);
	const Merge20HighRiskRemainUpdate update =
		Merge20HighRiskRemainDecideUpdate(false, 0, 0);
	EXPECT_FALSE(update.changed);
	EXPECT_FALSE(update.visible);
}

TEST(Merge20HighRiskRemain, CountIncreaseUpdates)
{
	// A rescan (or a reopened file) can grow the remainder again:
	// the line updates while staying visible.
	const Merge20HighRiskRemainUpdate update =
		Merge20HighRiskRemainDecideUpdate(true, 2, 5);
	EXPECT_TRUE(update.changed);
	EXPECT_TRUE(update.visible);
	EXPECT_EQ(update.highRisk, 5);
}

TEST(Merge20HighRiskRemain, AcceptanceScenario)
{
	// The task's verification scenario, through the B2 counts B4
	// displays: 3 high-risk unreviewed files, then one DONE, then
	// all DONE. Each step feeds the fresh highRisk into the
	// display decision, so the test covers the count and the
	// trigger together.
	const std::vector<Merge20CloseWarnFile> three =
	{
		RemainFile(ReviewState::TODO, RiskLevel::HIGH),
		RemainFile(ReviewState::TODO, RiskLevel::HIGH),
		RemainFile(ReviewState::TODO, RiskLevel::HIGH),
	};
	Merge20HighRiskRemainUpdate update =
		DecideFromHidden(Merge20CloseWarnComputeCounts(three).highRisk);
	EXPECT_TRUE(update.changed);
	EXPECT_TRUE(update.visible);
	EXPECT_EQ(update.highRisk, 3);

	const std::vector<Merge20CloseWarnFile> two =
	{
		RemainFile(ReviewState::TODO, RiskLevel::HIGH),
		RemainFile(ReviewState::TODO, RiskLevel::HIGH),
		RemainFile(ReviewState::DONE, RiskLevel::HIGH),
	};
	const Merge20CloseWarnCounts countsTwo = Merge20CloseWarnComputeCounts(two);
	EXPECT_EQ(countsTwo.highRisk, 2);
	update = Merge20HighRiskRemainDecideUpdate(true, 3, countsTwo.highRisk);
	EXPECT_TRUE(update.changed);
	EXPECT_TRUE(update.visible);
	EXPECT_EQ(update.highRisk, 2);

	const std::vector<Merge20CloseWarnFile> none =
	{
		RemainFile(ReviewState::DONE, RiskLevel::HIGH),
		RemainFile(ReviewState::DONE, RiskLevel::HIGH),
		RemainFile(ReviewState::DONE, RiskLevel::HIGH),
	};
	const Merge20CloseWarnCounts countsNone = Merge20CloseWarnComputeCounts(none);
	EXPECT_EQ(countsNone.highRisk, 0);
	update = Merge20HighRiskRemainDecideUpdate(true, 2, countsNone.highRisk);
	EXPECT_TRUE(update.changed);
	EXPECT_FALSE(update.visible);
	EXPECT_EQ(update.highRisk, 0);
}

TEST(Merge20HighRiskRemain, DiffsKeepRemainder)
{
	// A DONE file can still hold undecided diffs (its review was
	// set by key without judging the diffs): B2 counts those
	// diffs in highRisk, and B4 shows them too, so the two can
	// never disagree. A non-high file's undecided diffs do not
	// count.
	const std::vector<Merge20CloseWarnFile> files =
	{
		RemainFile(ReviewState::DONE, RiskLevel::HIGH, { Undecided(), Undecided() }),
		RemainFile(ReviewState::DONE, RiskLevel::NONE, { Undecided() }),
	};
	const Merge20CloseWarnCounts counts = Merge20CloseWarnComputeCounts(files);
	EXPECT_EQ(counts.undecidedFiles, 0);
	EXPECT_EQ(counts.undecidedDiffs, 3);
	EXPECT_EQ(counts.highRisk, 2);
	const Merge20HighRiskRemainUpdate update = DecideFromHidden(counts.highRisk);
	EXPECT_TRUE(update.changed);
	EXPECT_TRUE(update.visible);
	EXPECT_EQ(update.highRisk, 2);
}

TEST(Merge20HighRiskRemain, ScaleThousandFiles)
{
	// 1000 files: the pure count stays exact at the top of the
	// required scale, so the panel's update has a correct count
	// to display. 300 HIGH TODO files with one undecided diff
	// each, 200 MEDIUM TODO files, and 500 DONE files of which
	// 100 HIGH ones still hold two undecided diffs each.
	std::vector<Merge20CloseWarnFile> files;
	files.reserve(1000);
	for (int i = 0; i < 300; ++i)
		files.push_back(RemainFile(ReviewState::TODO, RiskLevel::HIGH, { Undecided(), UseOld() }));
	for (int i = 0; i < 200; ++i)
		files.push_back(RemainFile(ReviewState::TODO, RiskLevel::MEDIUM));
	for (int i = 0; i < 100; ++i)
		files.push_back(RemainFile(ReviewState::DONE, RiskLevel::HIGH, { Undecided(), Undecided() }));
	for (int i = 0; i < 400; ++i)
		files.push_back(RemainFile(ReviewState::DONE, RiskLevel::NONE));
	const Merge20CloseWarnCounts counts = Merge20CloseWarnComputeCounts(files);
	EXPECT_EQ(counts.total, 1000);
	EXPECT_EQ(counts.undecidedFiles, 500);
	EXPECT_EQ(counts.undecidedDiffs, 500);
	// 300 TODO HIGH files + their 300 undecided diffs + 200
	// undecided diffs in DONE HIGH files.
	EXPECT_EQ(counts.highRisk, 800);
	EXPECT_LE(counts.highRisk, counts.undecidedFiles + counts.undecidedDiffs);
	const Merge20HighRiskRemainUpdate update = DecideFromHidden(counts.highRisk);
	EXPECT_TRUE(update.changed);
	EXPECT_TRUE(update.visible);
	EXPECT_EQ(update.highRisk, 800);
}
