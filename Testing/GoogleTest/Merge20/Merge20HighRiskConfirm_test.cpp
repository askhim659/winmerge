/**
 * @file Merge20HighRiskConfirm_test.cpp
 *
 * @brief Merge20 (B14) unit tests for the high-risk confirmation
 * decision.
 *
 * Three layers, all pure: which conclusions are destructive, when
 * one diff judgment needs the dialog, and when one Space-cycle
 * step on a file needs it. The acceptance scenarios of the task
 * are covered by AcceptanceScenario (a high-risk diff judged old
 * needs confirmation, a risk-free diff does not) and by
 * LearningCandidateDoesNotSuppressConfirm (a candidate on a
 * high-risk diff never suppresses the dialog: the candidate is
 * not an input to the decision, gap analysis section 3.1).
 * CollectMixedRange covers the once-per-judgment rule: a range
 * operation shows one dialog for every high-risk diff in it,
 * never one dialog per diff.
 *
 * What is NOT tested here: the dialog display itself (its
 * layout, the risk reason and candidate text, the Japanese
 * translation) and the wiring of the three call sites
 * (context menu, focus dock, learning acceptance, Space). That
 * stays real-machine unverified, as for the other slices.
 *
 * This file stays ASCII: no Japanese text is needed here (the
 * dialog texts are composed by the callers through `_()`).
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20HighRiskConfirm.h"

namespace
{

	// One target for the once-per-judgment collection.
	Merge20HighRiskConfirmTarget Target(int diffIndex, RiskLevel risk,
		Merge20DiffConclusion conclusion, bool alreadyDecided = false)
	{
		Merge20HighRiskConfirmTarget target;
		target.diffIndex = diffIndex;
		target.risk = risk;
		target.conclusion = conclusion;
		target.alreadyDecided = alreadyDecided;
		return target;
	}

} // namespace

TEST(Merge20HighRiskConfirm, DestructiveUseOld)
{
	EXPECT_TRUE(Merge20HighRiskConfirmIsDestructive(Merge20DiffConclusion::UseOld));
}

TEST(Merge20HighRiskConfirm, DestructiveUseNew)
{
	EXPECT_TRUE(Merge20HighRiskConfirmIsDestructive(Merge20DiffConclusion::UseNew));
}

TEST(Merge20HighRiskConfirm, DestructiveReject)
{
	EXPECT_TRUE(Merge20HighRiskConfirmIsDestructive(Merge20DiffConclusion::Reject));
}

TEST(Merge20HighRiskConfirm, NotDestructiveUseBoth)
{
	EXPECT_FALSE(Merge20HighRiskConfirmIsDestructive(Merge20DiffConclusion::UseBoth));
}

TEST(Merge20HighRiskConfirm, NotDestructiveKeep)
{
	EXPECT_FALSE(Merge20HighRiskConfirmIsDestructive(Merge20DiffConclusion::Keep));
}

TEST(Merge20HighRiskConfirm, NotDestructiveUndecided)
{
	EXPECT_FALSE(Merge20HighRiskConfirmIsDestructive(Merge20DiffConclusion::Undecided));
}

TEST(Merge20HighRiskConfirm, HighRiskDestructiveNeedsConfirm)
{
	EXPECT_TRUE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::HIGH,
		Merge20DiffConclusion::UseOld, false));
	EXPECT_TRUE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::HIGH,
		Merge20DiffConclusion::UseNew, false));
	EXPECT_TRUE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::HIGH,
		Merge20DiffConclusion::Reject, false));
}

TEST(Merge20HighRiskConfirm, NonHighRiskNeverNeedsConfirm)
{
	EXPECT_FALSE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::MEDIUM,
		Merge20DiffConclusion::UseOld, false));
	EXPECT_FALSE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::NONE,
		Merge20DiffConclusion::UseOld, false));
	EXPECT_FALSE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::NONE,
		Merge20DiffConclusion::Reject, false));
}

TEST(Merge20HighRiskConfirm, HighRiskNonDestructiveNeedsNoConfirm)
{
	EXPECT_FALSE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::HIGH,
		Merge20DiffConclusion::UseBoth, false));
	EXPECT_FALSE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::HIGH,
		Merge20DiffConclusion::Keep, false));
	EXPECT_FALSE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::HIGH,
		Merge20DiffConclusion::Undecided, false));
}

TEST(Merge20HighRiskConfirm, AlreadyDecidedNeedsNoConfirm)
{
	// Re-applying the same conclusion to a diff that already
	// holds it is not a new judgment: the consecutive operation
	// on the same diff is not prompted again.
	EXPECT_FALSE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::HIGH,
		Merge20DiffConclusion::UseOld, true));
}

TEST(Merge20HighRiskConfirm, AcceptanceScenario)
{
	// Task verification (1) and (3): a high-risk diff judged old
	// needs the dialog; a risk-free diff judged the same way does
	// not, so it keeps the old one-key speed.
	EXPECT_TRUE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::HIGH,
		Merge20DiffConclusion::UseOld, false));
	EXPECT_FALSE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::NONE,
		Merge20DiffConclusion::UseOld, false));
}

TEST(Merge20HighRiskConfirm, LearningCandidateDoesNotSuppressConfirm)
{
	// Task verification (4) and gap analysis section 3.1: the
	// risk axis and the learning axis are independent. The
	// decision takes no candidate input, so a candidate on a
	// high-risk diff can never suppress the dialog; the candidate
	// text is only shown inside it.
	EXPECT_TRUE(Merge20HighRiskConfirmNeedsConfirm(RiskLevel::HIGH,
		Merge20DiffConclusion::UseOld, false));
}

TEST(Merge20HighRiskConfirm, CollectEmpty)
{
	EXPECT_TRUE(Merge20HighRiskConfirmCollect({}).empty());
}

TEST(Merge20HighRiskConfirm, CollectMixedRange)
{
	// One dialog per judgment, not one per diff: a range of four
	// diffs yields exactly the two high-risk destructive ones, in
	// order, each once.
	const std::vector<Merge20HighRiskConfirmTarget> targets =
	{
		Target(0, RiskLevel::HIGH, Merge20DiffConclusion::UseOld),
		Target(1, RiskLevel::NONE, Merge20DiffConclusion::UseOld),
		Target(2, RiskLevel::HIGH, Merge20DiffConclusion::UseBoth),
		Target(3, RiskLevel::HIGH, Merge20DiffConclusion::Reject),
	};
	const std::vector<int> needConfirm = Merge20HighRiskConfirmCollect(targets);
	ASSERT_EQ(needConfirm.size(), 2u);
	EXPECT_EQ(needConfirm[0], 0);
	EXPECT_EQ(needConfirm[1], 3);
}

TEST(Merge20HighRiskConfirm, CollectSkipsAlreadyDecided)
{
	const std::vector<Merge20HighRiskConfirmTarget> targets =
	{
		Target(0, RiskLevel::HIGH, Merge20DiffConclusion::UseOld, true),
		Target(1, RiskLevel::HIGH, Merge20DiffConclusion::UseNew),
	};
	const std::vector<int> needConfirm = Merge20HighRiskConfirmCollect(targets);
	ASSERT_EQ(needConfirm.size(), 1u);
	EXPECT_EQ(needConfirm[0], 1);
}

TEST(Merge20HighRiskConfirm, CollectIgnoresNegativeIndex)
{
	const std::vector<Merge20HighRiskConfirmTarget> targets =
	{
		Target(-1, RiskLevel::HIGH, Merge20DiffConclusion::UseOld),
	};
	EXPECT_TRUE(Merge20HighRiskConfirmCollect(targets).empty());
}

TEST(Merge20HighRiskConfirm, FileCycleHighTodoToDoneNeedsConfirm)
{
	// Task verification (5): the Space step that declares a
	// high-risk file done needs the dialog, so a wrong key cannot
	// commit it.
	EXPECT_TRUE(Merge20HighRiskConfirmFileCycleNeedsConfirm(
		RiskLevel::HIGH, ReviewState::TODO, ReviewState::DONE));
}

TEST(Merge20HighRiskConfirm, FileCycleHighOtherStepsNeedNoConfirm)
{
	// The other steps of the cycle never declare a file done, so
	// they keep the old one-key behaviour.
	EXPECT_FALSE(Merge20HighRiskConfirmFileCycleNeedsConfirm(
		RiskLevel::HIGH, ReviewState::DONE, ReviewState::HOLD));
	EXPECT_FALSE(Merge20HighRiskConfirmFileCycleNeedsConfirm(
		RiskLevel::HIGH, ReviewState::HOLD, ReviewState::SPECIAL));
	EXPECT_FALSE(Merge20HighRiskConfirmFileCycleNeedsConfirm(
		RiskLevel::HIGH, ReviewState::SPECIAL, ReviewState::KEEP));
	EXPECT_FALSE(Merge20HighRiskConfirmFileCycleNeedsConfirm(
		RiskLevel::HIGH, ReviewState::KEEP, ReviewState::TODO));
}

TEST(Merge20HighRiskConfirm, FileCycleNonHighTodoToDoneNeedsNoConfirm)
{
	EXPECT_FALSE(Merge20HighRiskConfirmFileCycleNeedsConfirm(
		RiskLevel::MEDIUM, ReviewState::TODO, ReviewState::DONE));
	EXPECT_FALSE(Merge20HighRiskConfirmFileCycleNeedsConfirm(
		RiskLevel::NONE, ReviewState::TODO, ReviewState::DONE));
}
