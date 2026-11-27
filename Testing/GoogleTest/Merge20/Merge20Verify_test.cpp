/**
 * @file Merge20Verify_test.cpp
 *
 * @brief Merge20 (B7) unit tests for the output-time final
 * verification engine.
 *
 * Four layers, all pure: the re-diff (i) and its explained /
 * must-equal rule, the line-count / encoding check (ii), the
 * simple syntax check (iii) and its input-balance suppression,
 * and the conclusion-vs-output consistency check (iv). The
 * acceptance scenarios of the task are covered by
 * RediffDetectsDeletedLine (a hand-deleted line stops the
 * output), ConsistencyMismatch (a judged diff whose output does
 * not reflect the judgment stops the output), and
 * RediffCleanUseNew (a normal merge passes through), plus the
 * 100-file performance test, which prints its timing for the
 * release record.
 *
 * What is NOT tested here: the problem-list dialog (its layout,
 * the jump to a file/diff, the Japanese text), the gate wiring
 * into SaveMergeResult, and the adapter that builds the input
 * from a real merge doc. That stays real-machine unverified, as
 * for the other slices.
 *
 * This file stays ASCII: non-ASCII test text is written with
 * \x escapes (the dialog texts are composed by the callers
 * through `_()`).
 */
#include "pch.h"
#include <cstdio>
#include <gtest/gtest.h>
#include "Merge20/Merge20Verify.h"
#include "Merge20/Merge20Conclusion.h"

namespace
{

using Vec = std::vector<std::string>;

// One diff record. savedLines is the saved-form line count;
// savedBegin is assigned by Build() from the file layout.
Merge20VerifyDiffInput Diff(int index, int op, int oldBegin, int oldEnd,
	int newBegin, int newEnd, Merge20DiffConclusion conclusion, int segState,
	std::vector<int> srcPanes, int savedLines)
{
	Merge20VerifyDiffInput diff;
	diff.diffIndex = index;
	diff.op = op;
	diff.oldBegin = oldBegin;
	diff.oldEnd = oldEnd;
	diff.newBegin = newBegin;
	diff.newEnd = newEnd;
	diff.conclusion = static_cast<int>(conclusion);
	diff.segState = segState;
	diff.srcPanes = std::move(srcPanes);
	diff.savedLines = savedLines;
	return diff;
}

// Assign savedBegin to each diff from the file layout: common
// lines between diffs occupy the same count in the result as in
// OLD/NEW (equal regions pair up 1:1), so the cursor advances
// by the common count, then by the diff's saved lines.
Merge20VerifyFileInput Build(Vec oldLines, Vec newLines, Vec resultLines,
	std::vector<Merge20VerifyDiffInput> diffs, int newPane = 1)
{
	Merge20VerifyFileInput input;
	int resCursor = 0;
	int prevOldEnd = -1;
	for (auto& diff : diffs)
	{
		const int common = diff.oldBegin - prevOldEnd - 1;
		if (common > 0)
			resCursor += common;
		diff.savedBegin = resCursor;
		resCursor += diff.savedLines;
		prevOldEnd = diff.oldEnd;
	}
	input.panes.push_back(std::move(oldLines));
	if (newPane == 2)
		input.panes.push_back(input.panes[0]); // middle pane placeholder, replaced by caller
	input.panes.push_back(std::move(newLines));
	input.newPane = newPane;
	input.resultLines = std::move(resultLines);
	input.diffs = std::move(diffs);
	return input;
}

// 3-way input: panes are old / middle / new.
Merge20VerifyFileInput Build3Way(Vec oldLines, Vec middleLines, Vec newLines,
	Vec resultLines, std::vector<Merge20VerifyDiffInput> diffs)
{
	Merge20VerifyFileInput input = Build(std::move(oldLines), std::move(newLines),
		std::move(resultLines), std::move(diffs), 2);
	input.panes[1] = std::move(middleLines);
	return input;
}

Merge20VerifyOptions AllOn()
{
	return Merge20VerifyOptions{};
}

bool HasIssue(const Merge20VerifyResult& result, Merge20VerifyCheck check,
	Merge20VerifyIssueKind kind)
{
	for (const Merge20VerifyIssue& issue : result.issues)
	{
		if (issue.check == check && issue.kind == kind)
			return true;
	}
	return false;
}

bool HasCheckIssue(const Merge20VerifyResult& result, Merge20VerifyCheck check)
{
	for (const Merge20VerifyIssue& issue : result.issues)
	{
		if (issue.check == check)
			return true;
	}
	return false;
}

int CountCheckIssues(const Merge20VerifyResult& result, Merge20VerifyCheck check)
{
	int count = 0;
	for (const Merge20VerifyIssue& issue : result.issues)
	{
		if (issue.check == check)
			++count;
	}
	return count;
}

// The changed-line scenario used by most tests: OLD "b", NEW
// "B2" on line 2 of a 3-line file. The NEW line differs from
// the OLD line beyond case (a plain "B" would be equal to "b"
// under the default ignore-case compare, so a conclusion-vs-
// output mismatch built on it would not be a mismatch at all).
Merge20VerifyDiffInput ChangedLineDiff(Merge20DiffConclusion conclusion, int segState,
	std::vector<int> srcPanes, int savedLines)
{
	return Diff(0, Merge20ConclusionOpDiff, 1, 1, 1, 1, conclusion, segState,
		std::move(srcPanes), savedLines);
}

} // namespace

TEST(Merge20Verify, RediffCleanUseNew)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "B2", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::UseNew, Merge20VerifySegChosen, { 1 }, 1) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
	EXPECT_TRUE(result.issues.empty());
	EXPECT_EQ(result.expectedLineCount, 3);
	EXPECT_EQ(result.checks.size(), 4u);
}

TEST(Merge20Verify, RediffCleanUseOld)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "b", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::UseOld, Merge20VerifySegChosen, { 0 }, 1) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
	EXPECT_TRUE(result.issues.empty());
}

TEST(Merge20Verify, RediffCleanUseBoth)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "b", "B2", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::UseBoth, Merge20VerifySegChosen, { 0, 1 }, 2) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
	EXPECT_TRUE(result.issues.empty());
}

// Acceptance scenario (2): one result line hand-deleted.
TEST(Merge20Verify, RediffDetectsDeletedLine)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::UseNew, Merge20VerifySegChosen, { 1 }, 0) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	ASSERT_EQ(CountCheckIssues(result, Merge20VerifyCheck::Rediff), 1);
	EXPECT_EQ(result.issues[0].diffIndex, 0);
	EXPECT_EQ(result.issues[0].newLine, 2);
}

// A hand-deleted common line (no diff involved) is also caught.
TEST(Merge20Verify, RediffDetectsDeletedCommonLine)
{
	auto input = Build({ "a", "b", "c", "d" }, { "a", "b", "c", "d" }, { "a", "b", "d" }, {});
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	ASSERT_EQ(CountCheckIssues(result, Merge20VerifyCheck::Rediff), 1);
	EXPECT_EQ(result.issues[0].diffIndex, -1);
	EXPECT_EQ(result.issues[0].newLine, 3);
}

// Two deleted common lines give two rows, so the dialog can
// jump to each of them.
TEST(Merge20Verify, RediffTwoDeletedLines)
{
	Vec lines;
	for (int i = 0; i < 100; ++i)
		lines.push_back("line " + std::to_string(i));
	Vec result = lines;
	result.erase(result.begin() + 50);
	result.erase(result.begin() + 10);
	auto input = Build(lines, lines, result, {});
	const Merge20VerifyResult r = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(r.passed);
	EXPECT_EQ(CountCheckIssues(r, Merge20VerifyCheck::Rediff), 2);
}

// Acceptance scenario (3), rediff half: the result holds NEW
// content for a UseOld-concluded diff.
TEST(Merge20Verify, RediffUnexplainedInUseNewDiff)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "X", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::UseNew, Merge20VerifySegChosen, { 1 }, 1) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	EXPECT_TRUE(HasIssue(result, Merge20VerifyCheck::Rediff,
		Merge20VerifyIssueKind::RediffUnexplained));
	EXPECT_EQ(result.issues[0].diffIndex, 0);
	EXPECT_EQ(result.issues[0].resultLine, 2);
}

// An auto-merged one-sided diff that keeps the OLD lines is a
// system decision, not an unintended difference.
TEST(Merge20Verify, RediffAutoOldIsExplained)
{
	auto input = Build({ "a", "b", "c" }, { "a", "c" }, { "a", "b", "c" },
		{ Diff(0, Merge20ConclusionOpOldOnly, 1, 1, 1, 0,
			Merge20DiffConclusion::Undecided, Merge20VerifySegAuto, { 0 }, 1) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
	EXPECT_TRUE(result.issues.empty());
}

// Whitespace-only differences do not count when the compare
// ignores whitespace (the default); they do when it does not.
TEST(Merge20Verify, RediffIgnoresWhitespaceWhenOptionOn)
{
	auto input = Build({ "a", "b ", "c" }, { "a", "b", "c" }, { "a", "b ", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::UseNew, Merge20VerifySegChosen, { 1 }, 1) });
	EXPECT_TRUE(Merge20VerifyRunFile(input, AllOn()).passed);
	input.ignoreWhitespace = false;
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	EXPECT_TRUE(HasIssue(result, Merge20VerifyCheck::Rediff,
		Merge20VerifyIssueKind::RediffUnexplained));
}

TEST(Merge20Verify, LineCountMismatchOnInsertedLine)
{
	auto input = Build({ "a", "b", "c" }, { "a", "b", "c" }, { "a", "b", "EXTRA", "c" }, {});
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	EXPECT_TRUE(HasIssue(result, Merge20VerifyCheck::LineCountEncoding,
		Merge20VerifyIssueKind::LineCountMismatch));
	for (const Merge20VerifyIssue& issue : result.issues)
	{
		if (issue.kind == Merge20VerifyIssueKind::LineCountMismatch)
		{
			EXPECT_EQ(issue.expectedCount, 3);
			EXPECT_EQ(issue.actualCount, 4);
		}
	}
}

TEST(Merge20Verify, EncodingUnrepresentableCharacter)
{
	auto input = Build({ "a", "b", "c" }, { "a", "b", "c" },
		{ "a", "b", "c" }, {});
	// U+00E9 (e-acute) as UTF-8, with an ASCII-only target
	// encoding (a stand-in for a legacy codepage).
	input.resultLines[1] = "\xC3\xA9";
	input.isRepresentable = [](char32_t cp) { return cp < 0x80; };
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	ASSERT_TRUE(HasIssue(result, Merge20VerifyCheck::LineCountEncoding,
		Merge20VerifyIssueKind::EncodingUnrepresentable));
	for (const Merge20VerifyIssue& issue : result.issues)
	{
		if (issue.kind == Merge20VerifyIssueKind::EncodingUnrepresentable)
		{
			EXPECT_EQ(issue.resultLine, 2);
			EXPECT_EQ(issue.codePoint, 0xE9);
		}
	}
}

TEST(Merge20Verify, EncodingRepresentableMultibytePasses)
{
	// U+65E5 (CJK) as UTF-8 on every side, representable in the
	// target: the re-diff stays silent, so only (ii) is exercised.
	auto input = Build({ "a", "\xE6\x97\xA5", "c" }, { "a", "\xE6\x97\xA5", "c" },
		{ "a", "\xE6\x97\xA5", "c" }, {});
	input.isRepresentable = [](char32_t cp) { return cp < 0x80 || cp == 0x65E5; };
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
}

TEST(Merge20Verify, EncodingInvalidUtf8Surrogate)
{
	auto input = Build({ "a", "b", "c" }, { "a", "b", "c" }, { "a", "b", "c" }, {});
	// CESU-8 lone surrogate: never valid UTF-8.
	input.resultLines[1] = "\xED\xA0\x80";
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	EXPECT_TRUE(HasIssue(result, Merge20VerifyCheck::LineCountEncoding,
		Merge20VerifyIssueKind::EncodingInvalidUtf8));
}

TEST(Merge20Verify, EncodingInvalidUtf8Truncated)
{
	auto input = Build({ "a", "b", "c" }, { "a", "b", "c" }, { "a", "b", "c" }, {});
	input.resultLines[1] = "\xE6\x97"; // truncated 3-byte sequence
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	EXPECT_TRUE(HasIssue(result, Merge20VerifyCheck::LineCountEncoding,
		Merge20VerifyIssueKind::EncodingInvalidUtf8));
}

TEST(Merge20Verify, SyntaxBalancedPasses)
{
	auto input = Build({ "if (a) { f(x); }" }, { "if (a) { f(x); }" },
		{ "if (a) { f(x); }" }, {});
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
}

TEST(Merge20Verify, SyntaxUnmatchedCloser)
{
	Merge20VerifyOptions options = AllOn();
	options.rediff = false;
	options.lineCountEncoding = false;
	options.consistency = false;
	auto input = Build({ "a = (b);" }, { "a = (b);" }, { "a = (b));" }, {});
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, options);
	EXPECT_FALSE(result.passed);
	ASSERT_EQ(CountCheckIssues(result, Merge20VerifyCheck::Syntax), 1);
	EXPECT_EQ(result.issues[0].kind, Merge20VerifyIssueKind::SyntaxUnmatchedCloser);
	EXPECT_EQ(result.issues[0].resultLine, 1);
}

TEST(Merge20Verify, SyntaxUnclosedOpener)
{
	Merge20VerifyOptions options = AllOn();
	options.rediff = false;
	options.lineCountEncoding = false;
	options.consistency = false;
	auto input = Build({ "a = (b);" }, { "a = (b);" }, { "a = (b;" }, {});
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, options);
	EXPECT_FALSE(result.passed);
	ASSERT_EQ(CountCheckIssues(result, Merge20VerifyCheck::Syntax), 1);
	EXPECT_EQ(result.issues[0].kind, Merge20VerifyIssueKind::SyntaxUnclosedOpener);
}

TEST(Merge20Verify, SyntaxMismatchedPair)
{
	Merge20VerifyOptions options = AllOn();
	options.rediff = false;
	options.lineCountEncoding = false;
	options.consistency = false;
	auto input = Build({ "a = (b);" }, { "a = (b);" }, { "a = (b];" }, {});
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, options);
	EXPECT_FALSE(result.passed);
	ASSERT_EQ(CountCheckIssues(result, Merge20VerifyCheck::Syntax), 1);
	EXPECT_EQ(result.issues[0].kind, Merge20VerifyIssueKind::SyntaxMismatchedPair);
}

// Brackets inside strings and comments never count.
TEST(Merge20Verify, SyntaxIgnoresStringsAndComments)
{
	auto input = Build({ "s = \"(\"; // )", "t = '(';" }, { "s = \"(\"; // )", "t = '(';" },
		{ "s = \"(\"; // )", "t = '(';" }, {});
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
}

// An imbalance the merge did not introduce is not blamed on
// the merge: both inputs are unbalanced, so the result's
// brackets are not checked at all.
TEST(Merge20Verify, SyntaxSuppressedWhenInputUnbalanced)
{
	Merge20VerifyOptions options = AllOn();
	options.rediff = false;
	options.lineCountEncoding = false;
	options.consistency = false;
	auto input = Build({ "a = (b;" }, { "a = (b;" }, { "a = (b));" }, {});
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, options);
	EXPECT_TRUE(result.passed);
	EXPECT_TRUE(result.issues.empty());
}

// Acceptance scenario (3), consistency half: a UseOld
// conclusion whose output holds the NEW lines.
TEST(Merge20Verify, ConsistencyMismatch)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "B2", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::UseOld, Merge20VerifySegChosen, { 1 }, 1) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	ASSERT_TRUE(HasIssue(result, Merge20VerifyCheck::Consistency,
		Merge20VerifyIssueKind::ConsistencyMismatch));
	for (const Merge20VerifyIssue& issue : result.issues)
	{
		if (issue.check == Merge20VerifyCheck::Consistency)
		{
			EXPECT_EQ(issue.diffIndex, 0);
			EXPECT_EQ(issue.expectedCount, 1);
			EXPECT_EQ(issue.actualCount, 1);
		}
	}
}

// A judged diff left Unresolved has no chosen content.
TEST(Merge20Verify, ConsistencyUnresolved)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" },
		{ "a", "<<<<<<< new", "B2", "=======", "b", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::UseNew, Merge20VerifySegConflict, {}, 4) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	EXPECT_TRUE(HasIssue(result, Merge20VerifyCheck::Consistency,
		Merge20VerifyIssueKind::ConsistencyUnresolved));
	EXPECT_FALSE(HasCheckIssue(result, Merge20VerifyCheck::Rediff));
}

// Reject on a changed block: the conflict section IS the
// expected saved form.
TEST(Merge20Verify, ConsistencyRejectChangedConflictPasses)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" },
		{ "a", "<<<<<<< new", "B2", "=======", "b", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::Reject, Merge20VerifySegConflict, {}, 4) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
}

// ...but Reject with chosen content means the Reject did not stick.
TEST(Merge20Verify, ConsistencyRejectChangedChosenFails)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "B2", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::Reject, Merge20VerifySegChosen, { 1 }, 1) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	EXPECT_TRUE(HasIssue(result, Merge20VerifyCheck::Consistency,
		Merge20VerifyIssueKind::ConsistencyMismatch));
}

// Reject on a one-sided diff: the empty side contributes
// nothing, so the diff saves zero lines.
TEST(Merge20Verify, ConsistencyRejectOneSidedPasses)
{
	auto input = Build({ "a", "b", "c" }, { "a", "c" }, { "a", "c" },
		{ Diff(0, Merge20ConclusionOpOldOnly, 1, 1, 1, 0,
			Merge20DiffConclusion::Reject, Merge20VerifySegChosen, { 1 }, 0) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
}

// Keep means "the current result stands": no expected content.
TEST(Merge20Verify, ConsistencyKeepSkipped)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "X", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::Keep, Merge20VerifySegChosen, { 0 }, 1) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
}

// Undecided diffs have no expected content either.
TEST(Merge20Verify, ConsistencyUndecidedSkipped)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "b", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::Undecided, Merge20VerifySegAuto, { 0 }, 1) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
}

// A hand edit after a conclusion no longer reflects it.
TEST(Merge20Verify, ConsistencyEditedAfterConclusionFails)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "X", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::UseOld, Merge20VerifySegEdited, { 0 }, 1) });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_FALSE(result.passed);
	EXPECT_TRUE(HasIssue(result, Merge20VerifyCheck::Consistency,
		Merge20VerifyIssueKind::ConsistencyMismatch));
}

// 3-way UseBoth: old lines then new lines.
TEST(Merge20Verify, ConsistencyThreeWayUseBoth)
{
	auto diff = ChangedLineDiff(Merge20DiffConclusion::UseBoth, Merge20VerifySegChosen, { 0, 2 }, 2);
	diff.thirdBegin = 1;
	diff.thirdEnd = 1;
	auto input = Build3Way({ "a", "b", "c" }, { "a", "b", "c" }, { "a", "B2", "c" },
		{ "a", "b", "B2", "c" }, { diff });
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
}

// OP_NONE entries are not differences: skipped everywhere.
TEST(Merge20Verify, OpNoneSkipped)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "B2", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::UseOld, Merge20VerifySegChosen, { 1 }, 1) });
	input.diffs[0].op = Merge20ConclusionOpNone;
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
}

// No diffs at all, identical files: nothing to check, passes.
TEST(Merge20Verify, EmptyNoDiffsPasses)
{
	auto input = Build({ "a", "b" }, { "a", "b" }, { "a", "b" }, {});
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, AllOn());
	EXPECT_TRUE(result.passed);
	EXPECT_EQ(result.expectedLineCount, 2);
}

// Each check switches off on its own; the others still fire.
TEST(Merge20Verify, OptionsSwitchOffPerCheck)
{
	auto input = Build({ "a", "b", "c" }, { "a", "B2", "c" }, { "a", "X", "c" },
		{ ChangedLineDiff(Merge20DiffConclusion::UseNew, Merge20VerifySegChosen, { 1 }, 1) });
	Merge20VerifyOptions options = AllOn();
	options.consistency = false;
	const Merge20VerifyResult result = Merge20VerifyRunFile(input, options);
	EXPECT_FALSE(result.passed);
	EXPECT_FALSE(HasCheckIssue(result, Merge20VerifyCheck::Consistency));
	EXPECT_TRUE(HasCheckIssue(result, Merge20VerifyCheck::Rediff));
	EXPECT_EQ(result.checks[3].enabled, false);
	EXPECT_EQ(result.checks[0].enabled, true);

	Merge20VerifyOptions allOff;
	allOff.rediff = false;
	allOff.lineCountEncoding = false;
	allOff.syntax = false;
	allOff.consistency = false;
	const Merge20VerifyResult silent = Merge20VerifyRunFile(input, allOff);
	EXPECT_TRUE(silent.passed);
	EXPECT_TRUE(silent.issues.empty());
	for (const Merge20VerifyCheckResult& cr : silent.checks)
		EXPECT_FALSE(cr.enabled);
}

// Acceptance (d): 100 files must verify fast enough to be
// unnoticeable at output time. The timing is printed for the
// release record; the bound is loose on purpose (CI machines
// vary), the printed number is the real measurement.
TEST(Merge20Verify, Performance100Files)
{
	const auto started = std::chrono::steady_clock::now();
	int totalIssues = 0;
	for (int f = 0; f < 100; ++f)
	{
		Vec oldLines, newLines, resultLines;
		for (int i = 0; i < 500; ++i)
		{
			oldLines.push_back("common line " + std::to_string(i) + " file " + std::to_string(f));
			newLines.push_back(oldLines.back());
			resultLines.push_back(oldLines.back());
		}
		std::vector<Merge20VerifyDiffInput> diffs;
		for (int d = 0; d < 5; ++d)
		{
			const int line = 50 + d * 80;
			newLines[static_cast<size_t>(line)] = "changed line " + std::to_string(line);
			resultLines[static_cast<size_t>(line)] = newLines[static_cast<size_t>(line)];
			diffs.push_back(ChangedLineDiff(Merge20DiffConclusion::UseNew,
				Merge20VerifySegChosen, { 1 }, 1));
			diffs.back().diffIndex = d;
			diffs.back().oldBegin = line;
			diffs.back().oldEnd = line;
			diffs.back().newBegin = line;
			diffs.back().newEnd = line;
		}
		auto input = Build(std::move(oldLines), std::move(newLines),
			std::move(resultLines), std::move(diffs));
		const Merge20VerifyResult r = Merge20VerifyRunFile(input, AllOn());
		EXPECT_TRUE(r.passed);
		totalIssues += static_cast<int>(r.issues.size());
	}
	const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - started).count();
	std::printf("Merge20 verify: 100 files x 500 lines, %lld ms total, %d issues\n",
		ms, totalIssues);
	EXPECT_EQ(totalIssues, 0);
	EXPECT_LT(ms, 10000);
}
