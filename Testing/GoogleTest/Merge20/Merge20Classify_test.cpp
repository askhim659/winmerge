/**
 * @file Merge20Classify_test.cpp
 *
 * @brief Merge20 (S4) unit tests for the rule-based classifier.
 *
 * The demo cases (HouseValuationService / FixedAssetEngine /
 * NoticeFormMapper) use the HTML tool's loadDemo sample and the
 * ground truth produced by running the HTML tool's own diffFast +
 * analyzeInsights on it in Node (counts, action, risk, insights and
 * function names must match). The classifier is pure (no MFC, no
 * `_()`), so these tests run in the UnitTests runner on CI.
 *
 * This file stays ASCII: the Japanese test text is written with \u
 * escapes, like Merge20EncodingRoundtrip_test.cpp.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Classify.h"

namespace
{

	Merge20DiffLine Line(OP_TYPE op, const wchar_t* oldText, const wchar_t* newText)
	{
		Merge20DiffLine line;
		line.op = op;
		line.oldText = oldText != nullptr ? oldText : L"";
		line.newText = newText != nullptr ? newText : L"";
		return line;
	}

	Merge20DiffLine ContextLine(const wchar_t* text)
	{
		return Line(OP_NONE, text, text);
	}

	// Default risk keywords (must match the OPT_MERGE20_RISK_KEYWORDS
	// default in OptionsInit.cpp).
	std::vector<String> DefaultKeywords()
	{
		return Merge20ParseRiskKeywords(
			L"\x7a0e;\x984d;\x7387;\x7aef\x6570;\x91d1\x984d;\x7a0e\x984d;\x4e38\x3081;\x56db\x6368\x4e94\x5165;SQL");
	}

} // namespace

TEST(Merge20Classify, HouseValuationServiceDemo)
{
	// HTML ground truth: dc=2 mc=1, REVIEW_MIXED / HIGH, insights
	// [constant, if-guard, condition], functions [calc].
	const std::vector<Merge20DiffLine> lines = {
		Line(OP_DIFF, L"// 20250401001289", L"// 20261116009988"),
		ContextLine(L"    public long calc(long cost, double ratio) {"),
		Line(OP_1STONLY, L"        // \u3010\u65e7\u72ec\u81ea\u51e6\u7406\u3011\u4e0b\u96500.20\u4fdd\u8a3c", nullptr),
		Line(OP_1STONLY, L"        if (ratio < 0.20) { ratio = 0.20; }", nullptr),
	};
	const Merge20ClassifyResult r = Merge20ClassifyDiffLines(lines, DefaultKeywords());
	EXPECT_EQ(r.nMissing, 2);
	EXPECT_EQ(r.nModified, 1);
	EXPECT_EQ(r.nAdded, 0);
	EXPECT_EQ(r.action, ActionCategory::REVIEW_MIXED);
	EXPECT_EQ(r.risk, RiskLevel::HIGH);
	EXPECT_TRUE(r.hasConstantChange);
	EXPECT_EQ(r.constantFrom, String(L"20250401001289"));
	EXPECT_EQ(r.constantTo, String(L"20261116009988"));
	EXPECT_TRUE(r.hasIfGuardDeletion);
	EXPECT_TRUE(r.hasConditionChange);
	EXPECT_FALSE(r.hasRoundingChange);
	EXPECT_FALSE(r.hasSqlChange);
	EXPECT_FALSE(r.hasRiskKeyword);
	ASSERT_EQ(r.insights.size(), static_cast<size_t>(3));
	EXPECT_EQ(r.insights[0], Merge20Insight::CONSTANT);
	EXPECT_EQ(r.insights[1], Merge20Insight::IF_GUARD);
	EXPECT_EQ(r.insights[2], Merge20Insight::CONDITION);
	ASSERT_EQ(r.functions.size(), static_cast<size_t>(1));
	EXPECT_EQ(r.functions[0], String(L"calc"));
}

TEST(Merge20Classify, FixedAssetEngineDemo)
{
	// HTML ground truth: dc=4 mc=2, REVIEW_MIXED / HIGH, insights
	// [constant, rounding, if-guard, condition], functions [computeTax].
	const std::vector<Merge20DiffLine> lines = {
		Line(OP_DIFF, L"/* 00010402998 */", L"/* 00020805112 */"),
		ContextLine(L"    public long computeTax(long val, boolean res) {"),
		Line(OP_DIFF,
			L"        // \u3010\u81ea\u6cbb\u4f53\u72ec\u81ea\u3011\u6fc0\u5909\u7de9\u548c\u63aa\u7f6e",
			L"        long raw = (long)Math.floor(base * RATE);"),
		Line(OP_1STONLY, L"        if (base > 50000000L) {", nullptr),
		Line(OP_1STONLY, L"            base = Math.round(base * 0.92);", nullptr),
		Line(OP_1STONLY, L"        }", nullptr),
		Line(OP_1STONLY, L"        long raw = Math.round(base * RATE);", nullptr),
	};
	const Merge20ClassifyResult r = Merge20ClassifyDiffLines(lines, DefaultKeywords());
	EXPECT_EQ(r.nMissing, 4);
	EXPECT_EQ(r.nModified, 2);
	EXPECT_EQ(r.action, ActionCategory::REVIEW_MIXED);
	EXPECT_EQ(r.risk, RiskLevel::HIGH);
	EXPECT_TRUE(r.hasConstantChange);
	EXPECT_EQ(r.constantFrom, String(L"00010402998"));
	EXPECT_EQ(r.constantTo, String(L"00020805112"));
	EXPECT_TRUE(r.hasRoundingChange);
	EXPECT_TRUE(r.hasIfGuardDeletion);
	EXPECT_TRUE(r.hasConditionChange);
	EXPECT_FALSE(r.hasSqlChange);
	ASSERT_EQ(r.insights.size(), static_cast<size_t>(4));
	EXPECT_EQ(r.insights[0], Merge20Insight::CONSTANT);
	EXPECT_EQ(r.insights[1], Merge20Insight::ROUNDING);
	EXPECT_EQ(r.insights[2], Merge20Insight::IF_GUARD);
	EXPECT_EQ(r.insights[3], Merge20Insight::CONDITION);
	ASSERT_EQ(r.functions.size(), static_cast<size_t>(1));
	EXPECT_EQ(r.functions[0], String(L"computeTax"));
}

TEST(Merge20Classify, NoticeFormMapperDemo)
{
	// HTML ground truth: dc=0 mc=2, REVIEW_MOD_ONLY / MEDIUM (the
	// constant change alone must NOT raise the risk to High),
	// insights [constant], functions [getForm].
	const std::vector<Merge20DiffLine> lines = {
		Line(OP_DIFF, L"// 10002345", L"// 20009876"),
		Line(OP_DIFF,
			L"    public String getForm() { return \"F_CUSTOM_2025.xml\"; }",
			L"    public String getForm() { return \"F_STD_2026.xml\"; }"),
	};
	const Merge20ClassifyResult r = Merge20ClassifyDiffLines(lines, DefaultKeywords());
	EXPECT_EQ(r.nMissing, 0);
	EXPECT_EQ(r.nModified, 2);
	EXPECT_EQ(r.action, ActionCategory::REVIEW_MOD_ONLY);
	EXPECT_EQ(r.risk, RiskLevel::MEDIUM);
	EXPECT_TRUE(r.hasConstantChange);
	EXPECT_EQ(r.constantFrom, String(L"10002345"));
	EXPECT_EQ(r.constantTo, String(L"20009876"));
	EXPECT_FALSE(r.hasRoundingChange);
	EXPECT_FALSE(r.hasIfGuardDeletion);
	EXPECT_FALSE(r.hasConditionChange);
	EXPECT_FALSE(r.hasSqlChange);
	ASSERT_EQ(r.functions.size(), static_cast<size_t>(1));
	EXPECT_EQ(r.functions[0], String(L"getForm"));
}

TEST(Merge20Classify, AddedOnlyAndIdentical)
{
	const Merge20ClassifyResult added = Merge20ClassifyDiffLines({
		Line(OP_2NDONLY, nullptr, L"    // new line"),
		Line(OP_2NDONLY, nullptr, L"    int x = 1;"),
	}, DefaultKeywords());
	EXPECT_EQ(added.nAdded, 2);
	EXPECT_EQ(added.action, ActionCategory::NO_MERGE_NEW_ADD);
	EXPECT_EQ(added.risk, RiskLevel::NONE);

	const Merge20ClassifyResult identical = Merge20ClassifyDiffLines({}, DefaultKeywords());
	EXPECT_EQ(identical.action, ActionCategory::NO_ACTION_IDENTICAL);
	EXPECT_EQ(identical.risk, RiskLevel::NONE);
}

TEST(Merge20Classify, MissingOnlyIsMedium)
{
	const Merge20ClassifyResult r = Merge20ClassifyDiffLines({
		Line(OP_1STONLY, L"    // old only code", nullptr),
	}, DefaultKeywords());
	EXPECT_EQ(r.action, ActionCategory::MERGE_CLEAN_OLD);
	EXPECT_EQ(r.risk, RiskLevel::MEDIUM);
	EXPECT_EQ(r.attentionPoints, 1);
}

TEST(Merge20Classify, RiskKeywordUpgradesToHigh)
{
	// Deleted line containing a risk keyword: 1 structural point +
	// 1 keyword point = High.
	const Merge20ClassifyResult r = Merge20ClassifyDiffLines({
		Line(OP_1STONLY, L"    // \u7a0e\u984d\u306e\u8a08\u7b97", nullptr),
	}, DefaultKeywords());
	EXPECT_TRUE(r.hasRiskKeyword);
	EXPECT_EQ(r.attentionPoints, 2);
	EXPECT_EQ(r.risk, RiskLevel::HIGH);
}

TEST(Merge20Classify, SqlInsightDoesNotAddRiskPoint)
{
	// The SQL insight is reported, but the literal keyword "SQL" does
	// not appear in these lines, so the risk stays Medium.
	const Merge20ClassifyResult r = Merge20ClassifyDiffLines({
		Line(OP_DIFF, L"    select * from t;", L"    select id from t where x = 1;"),
	}, DefaultKeywords());
	EXPECT_TRUE(r.hasSqlChange);
	EXPECT_EQ(r.action, ActionCategory::REVIEW_MOD_ONLY);
	EXPECT_FALSE(r.hasRiskKeyword);
	EXPECT_EQ(r.risk, RiskLevel::MEDIUM);

	// The literal word "SQL" is a keyword hit and raises the risk.
	const Merge20ClassifyResult r2 = Merge20ClassifyDiffLines({
		Line(OP_DIFF, L"    // SQL text", L"    // SQL text2"),
	}, DefaultKeywords());
	EXPECT_TRUE(r2.hasRiskKeyword);
	EXPECT_EQ(r2.risk, RiskLevel::HIGH);
}

TEST(Merge20Classify, TrivialLinesAreExcludedOnly)
{
	const Merge20ClassifyResult r = Merge20ClassifyDiffLines({
		Line(OP_TRIVIAL, L"   ", L" "),
		Line(OP_TRIVIAL, L"   ", L" "),
		Line(OP_DIFF, L"a", L"b"),
	}, DefaultKeywords());
	EXPECT_EQ(r.nExcluded, 2);
	EXPECT_EQ(r.nModified, 1);
	EXPECT_FALSE(r.hasRoundingChange);
	EXPECT_FALSE(r.hasSqlChange);
}

TEST(Merge20Classify, FunctionNameMatching)
{
	String fn;
	EXPECT_TRUE(Merge20MatchFunctionName(L"    public long calc(long cost, double ratio) {", fn));
	EXPECT_EQ(fn, String(L"calc"));
	EXPECT_TRUE(Merge20MatchFunctionName(L"function foo(x) {", fn));
	EXPECT_EQ(fn, String(L"foo"));
	EXPECT_TRUE(Merge20MatchFunctionName(L"def bar(x):", fn));
	EXPECT_EQ(fn, String(L"bar"));
	EXPECT_FALSE(Merge20MatchFunctionName(L"public class NoticeFormMapper {", fn));
	EXPECT_FALSE(Merge20MatchFunctionName(L"    // plain comment", fn));
}

TEST(Merge20Classify, ParseRiskKeywords)
{
	const std::vector<String> kws = DefaultKeywords();
	EXPECT_EQ(kws.size(), static_cast<size_t>(9));
	EXPECT_TRUE(Merge20ParseRiskKeywords(L"").empty());
	const std::vector<String> trimmed = Merge20ParseRiskKeywords(L" a ; ; b; ");
	ASSERT_EQ(trimmed.size(), static_cast<size_t>(2));
	EXPECT_EQ(trimmed[0], String(L"a"));
	EXPECT_EQ(trimmed[1], String(L"b"));
}
