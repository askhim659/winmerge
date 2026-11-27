// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20ResumeSummary_test.cpp
 *
 * @brief Merge20 (B13) unit tests for the resume summary shown in
 * the session-restore offer.
 *
 * The expected values follow the counting rules in
 * Merge20ResumeSummary.h: undecidedFiles / undecidedDiffs /
 * highRisk are the B2 counts (Merge20CloseWarnComputeCounts) over
 * the session files, outputFiles counts includeInOutput, and total
 * is the number of recorded files. The acceptance scenario of the
 * task (2 unreviewed files left, offer shows "unreviewed 2" and
 * the save time) is covered by AcceptanceScenario and
 * FormatLinesFull. The wording lines are built from templates the
 * caller supplies, so the tests pass English templates and assert
 * the exact lines (order, numbers, 1-based diff/line numbers).
 *
 * This file stays ASCII: no Japanese text is needed here (the
 * templates are the caller's, through `_()`).
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20ResumeSummary.h"

namespace
{

	// Conclusion ordinals in B1 vocabulary, so the tests read as
	// "undecided / decided" rather than bare ints.
	int Undecided() { return static_cast<int>(Merge20DiffConclusion::Undecided); }
	int UseOld() { return static_cast<int>(Merge20DiffConclusion::UseOld); }
	int UseNew() { return static_cast<int>(Merge20DiffConclusion::UseNew); }
	int Keep() { return static_cast<int>(Merge20DiffConclusion::Keep); }

	// One session file: the given review state and risk, one
	// recorded diff judgment per conclusion ordinal, and the
	// output flag as saved.
	Merge20SessionFile SessionFile(const String& relPath, ReviewState review, RiskLevel risk,
		bool includeInOutput, std::vector<int> conclusions = {})
	{
		Merge20SessionFile file;
		file.relPath = relPath;
		file.review = review;
		file.risk = risk;
		file.includeInOutput = includeInOutput;
		for (const int conclusion : conclusions)
		{
			Merge20DiffJudgment diff;
			diff.conclusion = conclusion;
			file.diffs.push_back(diff);
		}
		return file;
	}

	// The English templates the tests use (the real caller passes
	// the same shapes through `_()`).
	Merge20ResumeSummaryTemplates TestTemplates()
	{
		Merge20ResumeSummaryTemplates templates;
		templates.savedAt = L"Saved at: %1";
		templates.counts = L"Unreviewed: %1 files, undecided diffs: %2, output targets: %3 (of %4 files)";
		templates.resumeWithPosition = L"The file compare \"%1\" that was open last time will also be opened (diff %2, line %3).";
		templates.resumeNoPosition = L"The file compare \"%1\" that was open last time will also be opened.";
		templates.noResume = L"No file compare was open last time.";
		return templates;
	}

	// The task's acceptance scenario as a session: 2 unreviewed
	// files and 3 undecided diffs (the B2 acceptance scenario),
	// with output flags as saved. File A is TODO and HIGH risk
	// with 2 undecided diffs and 1 decided one; file B is TODO
	// with 1 undecided diff; file C is DONE and HIGH risk with
	// decided diffs only; file D is DONE with no recorded diffs.
	Merge20Session AcceptanceSession()
	{
		Merge20Session session;
		session.savedAt = L"2026-11-27 10:00:00";
		session.files =
		{
			SessionFile(L"src\\a.cpp", ReviewState::TODO, RiskLevel::HIGH, true, { Undecided(), Undecided(), UseOld() }),
			SessionFile(L"src\\b.cpp", ReviewState::TODO, RiskLevel::NONE, true, { Undecided() }),
			SessionFile(L"src\\c.cpp", ReviewState::DONE, RiskLevel::HIGH, true, { UseNew(), Keep() }),
			SessionFile(L"src\\d.cpp", ReviewState::DONE, RiskLevel::NONE, false, {}),
		};
		session.display.resumeRelPath = L"src\\a.cpp";
		session.display.resumeDiff = 4;
		session.display.resumeTopLine = 120;
		return session;
	}

} // namespace

TEST(Merge20ResumeSummary, EmptySession)
{
	Merge20Session session;
	const Merge20ResumeSummary summary = Merge20ResumeSummaryFromSession(session);
	EXPECT_EQ(summary.warn.total, 0);
	EXPECT_EQ(summary.warn.undecidedFiles, 0);
	EXPECT_EQ(summary.warn.undecidedDiffs, 0);
	EXPECT_EQ(summary.warn.highRisk, 0);
	EXPECT_EQ(summary.outputFiles, 0);
	// An empty save time has no line; the counts line and the
	// no-resume line are still shown.
	const std::vector<String> lines = Merge20ResumeSummaryFormatLines(
		summary, session.savedAt, session.display.resumeRelPath,
		session.display.resumeDiff, session.display.resumeTopLine, TestTemplates());
	ASSERT_EQ(lines.size(), 2U);
	EXPECT_EQ(lines[0], L"Unreviewed: 0 files, undecided diffs: 0, output targets: 0 (of 0 files)");
	EXPECT_EQ(lines[1], L"No file compare was open last time.");
}

TEST(Merge20ResumeSummary, AcceptanceScenario)
{
	const Merge20Session session = AcceptanceSession();
	const Merge20ResumeSummary summary = Merge20ResumeSummaryFromSession(session);
	EXPECT_EQ(summary.warn.total, 4);
	EXPECT_EQ(summary.warn.undecidedFiles, 2);
	EXPECT_EQ(summary.warn.undecidedDiffs, 3);
	// High risk: file A itself (TODO + HIGH) plus its 2 undecided
	// diffs. File C is HIGH but decided, so it adds nothing.
	EXPECT_EQ(summary.warn.highRisk, 3);
	EXPECT_EQ(summary.outputFiles, 3);
}

TEST(Merge20ResumeSummary, CountsAgreeWithCloseWarn)
{
	// The summary must agree with the B2 close warning on the
	// same files: build the B2 input by hand from the same session
	// and require identical counts (gap analysis B13 (b)).
	const Merge20Session session = AcceptanceSession();
	const Merge20ResumeSummary summary = Merge20ResumeSummaryFromSession(session);
	std::vector<Merge20CloseWarnFile> warnFiles;
	for (const Merge20SessionFile& file : session.files)
	{
		Merge20CloseWarnFile record;
		record.review = file.review;
		record.risk = file.risk;
		for (const Merge20DiffJudgment& diff : file.diffs)
			record.diffConclusions.push_back(diff.conclusion);
		warnFiles.push_back(std::move(record));
	}
	const Merge20CloseWarnCounts expected = Merge20CloseWarnComputeCounts(warnFiles);
	EXPECT_EQ(summary.warn.total, expected.total);
	EXPECT_EQ(summary.warn.undecidedFiles, expected.undecidedFiles);
	EXPECT_EQ(summary.warn.undecidedDiffs, expected.undecidedDiffs);
	EXPECT_EQ(summary.warn.highRisk, expected.highRisk);
}

TEST(Merge20ResumeSummary, OutputFilesCountOnlyIncluded)
{
	Merge20Session session;
	session.files =
	{
		SessionFile(L"src\\a.cpp", ReviewState::DONE, RiskLevel::NONE, true, {}),
		SessionFile(L"src\\b.cpp", ReviewState::DONE, RiskLevel::NONE, false, {}),
		SessionFile(L"src\\c.cpp", ReviewState::TODO, RiskLevel::NONE, false, {}),
	};
	const Merge20ResumeSummary summary = Merge20ResumeSummaryFromSession(session);
	EXPECT_EQ(summary.warn.total, 3);
	EXPECT_EQ(summary.outputFiles, 1);
}

TEST(Merge20ResumeSummary, FormatLinesFull)
{
	// The task's acceptance scenario as wording: save time, the
	// counts, and the resume position (diff 4 -> "diff 5", top
	// line 120 -> "line 121": both shown 1-based).
	const Merge20Session session = AcceptanceSession();
	const Merge20ResumeSummary summary = Merge20ResumeSummaryFromSession(session);
	const std::vector<String> lines = Merge20ResumeSummaryFormatLines(
		summary, session.savedAt, session.display.resumeRelPath,
		session.display.resumeDiff, session.display.resumeTopLine, TestTemplates());
	ASSERT_EQ(lines.size(), 3U);
	EXPECT_EQ(lines[0], L"Saved at: 2026-11-27 10:00:00");
	EXPECT_EQ(lines[1], L"Unreviewed: 2 files, undecided diffs: 3, output targets: 3 (of 4 files)");
	EXPECT_EQ(lines[2], L"The file compare \"src\\a.cpp\" that was open last time will also be opened (diff 5, line 121).");
}

TEST(Merge20ResumeSummary, FormatLinesResumeWithoutDiff)
{
	// A file compare was open but had no current diff (resumeDiff
	// -1): the position-free resume line is used instead.
	const Merge20ResumeSummary summary = Merge20ResumeSummaryFromSession(AcceptanceSession());
	const std::vector<String> lines = Merge20ResumeSummaryFormatLines(
		summary, L"2026-11-27 10:00:00", L"src\\b.cpp", -1, 0, TestTemplates());
	ASSERT_EQ(lines.size(), 3U);
	EXPECT_EQ(lines[0], L"Saved at: 2026-11-27 10:00:00");
	EXPECT_EQ(lines[1], L"Unreviewed: 2 files, undecided diffs: 3, output targets: 3 (of 4 files)");
	EXPECT_EQ(lines[2], L"The file compare \"src\\b.cpp\" that was open last time will also be opened.");
}

TEST(Merge20ResumeSummary, FormatLinesAllReviewed)
{
	// Everything reviewed and decided: the counts line says so
	// plainly (0 unreviewed, 0 undecided diffs) instead of hiding.
	Merge20Session session;
	session.savedAt = L"2026-11-27 10:00:00";
	session.files =
	{
		SessionFile(L"src\\a.cpp", ReviewState::DONE, RiskLevel::HIGH, true, { UseOld(), UseNew() }),
		SessionFile(L"src\\b.cpp", ReviewState::DONE, RiskLevel::NONE, true, { Keep() }),
	};
	const Merge20ResumeSummary summary = Merge20ResumeSummaryFromSession(session);
	EXPECT_EQ(summary.warn.undecidedFiles, 0);
	EXPECT_EQ(summary.warn.undecidedDiffs, 0);
	const std::vector<String> lines = Merge20ResumeSummaryFormatLines(
		summary, session.savedAt, String(), -1, 0, TestTemplates());
	ASSERT_EQ(lines.size(), 3U);
	EXPECT_EQ(lines[1], L"Unreviewed: 0 files, undecided diffs: 0, output targets: 2 (of 2 files)");
	EXPECT_EQ(lines[2], L"No file compare was open last time.");
}
