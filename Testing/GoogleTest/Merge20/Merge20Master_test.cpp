// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Master_test.cpp
 *
 * @brief Merge20 (Phase 4 D8) unit tests for the master-data helpers.
 *
 * The expected values are the ground truth from the HTML tool's
 * `exRules` / `hlRules` (17_v17.15.html, lines 576 and 1008-1029)
 * and from the Phase 1 `Merge20Exclude.sct` prediffer
 * (`wildcardToRegExp` / `ruleMatchesLine`): a plain pattern
 * without `%` is a substring match, `%` stands for any text
 * (e.g. `--1.0.8%` matches `--1.0.813.000`), a regular-expression
 * pattern is a regex search, and an empty pattern never matches.
 * The JSON round trip checks the D8 format (design section 4.4):
 * UTF-8, no BOM, 2-space indent, `app` "Merge20Masters",
 * version 1.
 *
 * This file is ASCII; the pure module is MFC-free, so these tests
 * run in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Master.h"

namespace
{
Merge20ExcludeRule PlainRule(const wchar_t* pattern, bool enabled = true)
{
	Merge20ExcludeRule rule;
	rule.pattern = pattern;
	rule.useRegExp = false;
	rule.ignoreCase = false;
	rule.enabled = enabled;
	return rule;
}
}

TEST(Merge20Master, WildcardPercentMatchesAnyText)
{
	// HTML example: --1.0.8% matches --1.0.813.000 and --1.0.800.
	EXPECT_TRUE(Merge20MasterWildcardMatch(L"--1.0.8%", L"--1.0.813.000", false));
	EXPECT_TRUE(Merge20MasterWildcardMatch(L"--1.0.8%", L"--1.0.800", false));
	EXPECT_FALSE(Merge20MasterWildcardMatch(L"--1.0.8%", L"--1.0.700", false));
	EXPECT_TRUE(Merge20MasterWildcardMatch(L"a%b%c", L"aXXbYYc", false));
	EXPECT_FALSE(Merge20MasterWildcardMatch(L"a%b%c", L"aXXc", false));
}

TEST(Merge20Master, WildcardPlainIsSubstring)
{
	EXPECT_TRUE(Merge20MasterWildcardMatch(L"Last Modified:", L"// Last Modified: 2026", false));
	EXPECT_FALSE(Merge20MasterWildcardMatch(L"Last Modified:", L"unrelated", false));
	// Regex specials in a plain pattern are literal.
	EXPECT_TRUE(Merge20MasterWildcardMatch(L"a.b", L"xa.b", false));
	EXPECT_FALSE(Merge20MasterWildcardMatch(L"a.b", L"xayb", false));
}

TEST(Merge20Master, WildcardEmptyNeverMatches)
{
	EXPECT_FALSE(Merge20MasterWildcardMatch(L"", L"anything", false));
}

TEST(Merge20Master, WildcardIgnoreCase)
{
	EXPECT_TRUE(Merge20MasterWildcardMatch(L"tax", L"TAXAmount", true));
	EXPECT_FALSE(Merge20MasterWildcardMatch(L"tax", L"TAXAmount", false));
	EXPECT_TRUE(Merge20MasterWildcardMatch(L"ver%ion", L"VERSION 1.0", true));
}

TEST(Merge20Master, RuleMatchesRegex)
{
	// HTML default exclude rule: @version\s+[0-9.]+.
	Merge20ExcludeRule rule;
	rule.pattern = L"@version\\s+[0-9.]+";
	rule.useRegExp = true;
	EXPECT_TRUE(Merge20MasterRuleMatchesLine(rule, L"// @version 1.2.3"));
	EXPECT_FALSE(Merge20MasterRuleMatchesLine(rule, L"// @version x"));
	// An invalid regular expression never matches (prediffer try/catch).
	rule.pattern = L"[unclosed";
	EXPECT_FALSE(Merge20MasterRuleMatchesLine(rule, L"[unclosed"));
	rule.pattern = L"";
	EXPECT_FALSE(Merge20MasterRuleMatchesLine(rule, L"anything"));
}

TEST(Merge20Master, IsValidRegex)
{
	EXPECT_TRUE(Merge20MasterIsValidRegex(L"@version\\s+[0-9.]+"));
	EXPECT_FALSE(Merge20MasterIsValidRegex(L"[unclosed"));
	EXPECT_FALSE(Merge20MasterIsValidRegex(L""));
}

TEST(Merge20Master, ExcludeLineSkipsDisabled)
{
	EXPECT_TRUE(Merge20MasterExcludeLine({ PlainRule(L"tax") }, L"taxAmount"));
	EXPECT_FALSE(Merge20MasterExcludeLine({ PlainRule(L"tax", false) }, L"taxAmount"));
	EXPECT_FALSE(Merge20MasterExcludeLine({ PlainRule(L"tax") }, L"nothing here"));
}

TEST(Merge20Master, DefaultHighlightPresets)
{
	// HTML presets (17_v17.15.html line 576).
	const std::vector<Merge20HighlightRule> presets = Merge20MasterDefaultHighlightRules();
	ASSERT_EQ(presets.size(), 3u);
	EXPECT_EQ(presets[0].pattern, L"Math\\.floor|Math\\.round");
	EXPECT_TRUE(presets[0].useRegExp);
	EXPECT_EQ(presets[0].name, L"Math.floor\xff0fround");
	EXPECT_EQ(presets[1].pattern, L"\x72ec\x81ea");
	EXPECT_FALSE(presets[1].useRegExp);
	EXPECT_EQ(presets[1].name, L"\x72ec\x81ea");
	EXPECT_EQ(presets[2].pattern, L"0\\.92|0\\.014");
	EXPECT_TRUE(presets[2].useRegExp);
	EXPECT_EQ(presets[2].name, L"\x7a0e\x7387\x30fb\x88dc\x6b63\x7387");
	for (const Merge20HighlightRule& preset : presets)
		EXPECT_TRUE(preset.enabled);
}

TEST(Merge20Master, JsonRoundTrip)
{
	Merge20Masters masters;
	masters.excludeIgnoreL2 = false;
	masters.excludeRules = { PlainRule(L"Last Modified:") };
	Merge20ExcludeRule versionRule;
	versionRule.pattern = L"@version\\s+[0-9.]+";
	versionRule.useRegExp = true;
	masters.excludeRules.push_back(versionRule);
	masters.highlightRules = Merge20MasterDefaultHighlightRules();
	// \x7a0e = tax, \x984d = amount (the S4 default keywords).
	masters.riskKeywords = L"\x7a0e;\x984d;SQL";

	std::string json;
	ASSERT_TRUE(Merge20MastersToJson(masters, json));
	// No BOM, 2-space indent.
	EXPECT_NE(json.rfind("\xef\xbb\xbf", 0), 0u);
	EXPECT_NE(json.find("\n  \"app\""), std::string::npos);

	Merge20Masters back;
	std::string errDetail;
	int skippedRules = -1;
	ASSERT_TRUE(Merge20MastersFromJson(json, back, errDetail, skippedRules));
	EXPECT_EQ(skippedRules, 0);
	EXPECT_FALSE(back.excludeIgnoreL2);
	ASSERT_EQ(back.excludeRules.size(), 2u);
	EXPECT_EQ(back.excludeRules[0].pattern, L"Last Modified:");
	EXPECT_FALSE(back.excludeRules[0].useRegExp);
	EXPECT_TRUE(back.excludeRules[0].enabled);
	EXPECT_TRUE(back.excludeRules[1].useRegExp);
	ASSERT_EQ(back.highlightRules.size(), 3u);
	EXPECT_EQ(back.highlightRules[2].name, L"\x7a0e\x7387\x30fb\x88dc\x6b63\x7387");
	EXPECT_EQ(back.riskKeywords, L"\x7a0e;\x984d;SQL");
}

TEST(Merge20Master, JsonErrors)
{
	Merge20Masters out;
	std::string errDetail;
	int skippedRules = -1;
	EXPECT_FALSE(Merge20MastersFromJson("{\"app\":\"Other\",\"version\":1}", out, errDetail, skippedRules));
	EXPECT_FALSE(Merge20MastersFromJson("{\"app\":\"Merge20Masters\",\"version\":2}", out, errDetail, skippedRules));
	EXPECT_FALSE(Merge20MastersFromJson("{not json", out, errDetail, skippedRules));
}

TEST(Merge20Master, JsonSkipsBadRules)
{
	Merge20Masters out;
	std::string errDetail;
	int skippedRules = -1;
	ASSERT_TRUE(Merge20MastersFromJson(
		"{\"app\":\"Merge20Masters\",\"version\":1,"
		"\"excludeRules\":[{\"pattern\":\"a\"},{\"pattern\":5},\"nope\",{\"pattern\":\"b\",\"enabled\":false}],"
		"\"riskKeywords\":\"a;b\"}",
		out, errDetail, skippedRules));
	EXPECT_EQ(skippedRules, 2);
	ASSERT_EQ(out.excludeRules.size(), 2u);
	EXPECT_EQ(out.excludeRules[1].pattern, L"b");
	EXPECT_FALSE(out.excludeRules[1].enabled);
	EXPECT_EQ(out.riskKeywords, L"a;b");
	// A missing excludeIgnoreL2 defaults to true (the prediffer default).
	EXPECT_TRUE(out.excludeIgnoreL2);
	EXPECT_TRUE(out.highlightRules.empty());
}

TEST(Merge20Master, HtmlExcludeDefaults)
{
	Merge20Masters out;
	std::string errDetail;
	int skippedRules = -1;
	ASSERT_TRUE(Merge20MastersFromJson(
		"{\"app\":\"Merge20Masters\",\"version\":1,"
		"\"excludeRules\":[{\"pattern\":\"Last Modified:\",\"useRegExp\":false,\"ignoreCase\":false,\"enabled\":true},"
		"{\"pattern\":\"@version\\\\s+[0-9.]+\",\"useRegExp\":true,\"ignoreCase\":false,\"enabled\":true}]}",
		out, errDetail, skippedRules));
	EXPECT_EQ(skippedRules, 0);
	ASSERT_EQ(out.excludeRules.size(), 2u);
	EXPECT_TRUE(Merge20MasterExcludeLine(out.excludeRules, L"// Last Modified: today"));
	EXPECT_TRUE(Merge20MasterExcludeLine(out.excludeRules, L"@version 2.0"));
	EXPECT_FALSE(Merge20MasterExcludeLine(out.excludeRules, L"ordinary line"));
}
