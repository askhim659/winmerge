// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Master.cpp
 *
 * @brief Implementation of the master-data helpers (Merge20
 * Phase 4 D8, design sections 4.4 and 5.2/D8, spec sections
 * 3 (4/4) and 6).
 *
 * Pure module: no MFC, no `_()`, ASCII only (Japanese literals
 * are written as `\x` escapes, the S4 convention). The matching
 * helpers mirror the Phase 1 `Merge20Exclude.sct` prediffer
 * (`wildcardToRegExp` / `ruleMatchesLine`, JScript): a plain
 * pattern without `%` is a substring match, `%` stands for any
 * (possibly empty) string, a regular-expression pattern is a
 * regex search, and an empty pattern never matches. The JSON
 * conversion reuses the S8 lightweight JSON module
 * (`Merge20Session.h/.cpp`: DOM, parser, serializer, UTF-8
 * conversion), like the learning JSON v2 (L0).
 */
#include "pch.h"
#include <cwctype>
#include <regex>
#include "Merge20/Merge20Master.h"
#include "Merge20/Merge20Session.h"

namespace
{
/** @brief Escape one character for a literal regex fragment (the prediffer's `escapeRegExp`). */
bool MasterRegexSpecial(wchar_t ch)
{
	switch (ch)
	{
	case L'.': case L'*': case L'+': case L'?': case L'^': case L'$':
	case L'{': case L'}': case L'(': case L')': case L'|':
	case L'[': case L']': case L'\\':
		return true;
	default:
		return false;
	}
}

/** @brief Regex flags for a match (case-insensitive when asked). */
std::regex_constants::syntax_option_type MasterRegexFlags(bool ignoreCase)
{
	return ignoreCase
		? std::regex_constants::ECMAScript | std::regex_constants::icase
		: std::regex_constants::ECMAScript;
}

/** @brief Try a regex search; an invalid pattern never matches (the prediffer's try/catch). */
bool MasterRegexSearch(const String& line, const std::wregex& re)
{
	try
	{
		return std::regex_search(line, re);
	}
	catch (const std::regex_error&)
	{
		return false;
	}
}

/** @brief One exclude rule as a JSON object (fixed member order). */
Merge20JsonValue MasterExcludeRuleToJson(const Merge20ExcludeRule& rule)
{
	std::vector<std::pair<String, Merge20JsonValue>> members;
	members.emplace_back(L"pattern", Merge20JsonText(rule.pattern));
	members.emplace_back(L"useRegExp", Merge20JsonBool(rule.useRegExp));
	members.emplace_back(L"ignoreCase", Merge20JsonBool(rule.ignoreCase));
	members.emplace_back(L"enabled", Merge20JsonBool(rule.enabled));
	return Merge20JsonObject(std::move(members));
}

/** @brief One highlight rule as a JSON object (fixed member order). */
Merge20JsonValue MasterHighlightRuleToJson(const Merge20HighlightRule& rule)
{
	std::vector<std::pair<String, Merge20JsonValue>> members;
	members.emplace_back(L"pattern", Merge20JsonText(rule.pattern));
	members.emplace_back(L"name", Merge20JsonText(rule.name));
	members.emplace_back(L"useRegExp", Merge20JsonBool(rule.useRegExp));
	members.emplace_back(L"enabled", Merge20JsonBool(rule.enabled));
	return Merge20JsonObject(std::move(members));
}

/**
 * @brief Parse one exclude rule from a JSON object.
 * @return False when the entry is not an object or has no text
 * pattern (the caller counts it as skipped).
 */
bool MasterExcludeRuleFromJson(const Merge20JsonValue& value, Merge20ExcludeRule& out)
{
	if (value.type != Merge20JsonValue::Type::Object)
		return false;
	const Merge20JsonValue* pattern = value.Find(L"pattern");
	if (pattern == nullptr || pattern->type != Merge20JsonValue::Type::Text)
		return false;
	out.pattern = pattern->text;
	const Merge20JsonValue* useRegExp = value.Find(L"useRegExp");
	out.useRegExp = (useRegExp == nullptr) ? false : useRegExp->AsBool(false);
	const Merge20JsonValue* ignoreCase = value.Find(L"ignoreCase");
	out.ignoreCase = (ignoreCase == nullptr) ? false : ignoreCase->AsBool(false);
	const Merge20JsonValue* enabled = value.Find(L"enabled");
	out.enabled = (enabled == nullptr) ? true : enabled->AsBool(true);
	return true;
}

/**
 * @brief Parse one highlight rule from a JSON object.
 * @return False when the entry is not an object or has no text
 * pattern (the caller counts it as skipped).
 */
bool MasterHighlightRuleFromJson(const Merge20JsonValue& value, Merge20HighlightRule& out)
{
	if (value.type != Merge20JsonValue::Type::Object)
		return false;
	const Merge20JsonValue* pattern = value.Find(L"pattern");
	if (pattern == nullptr || pattern->type != Merge20JsonValue::Type::Text)
		return false;
	out.pattern = pattern->text;
	const Merge20JsonValue* name = value.Find(L"name");
	out.name = (name == nullptr) ? String() : name->AsText(String());
	const Merge20JsonValue* useRegExp = value.Find(L"useRegExp");
	out.useRegExp = (useRegExp == nullptr) ? false : useRegExp->AsBool(false);
	const Merge20JsonValue* enabled = value.Find(L"enabled");
	out.enabled = (enabled == nullptr) ? true : enabled->AsBool(true);
	return true;
}
}

bool Merge20MasterIsValidRegex(const String& pattern)
{
	if (pattern.empty())
		return false;
	try
	{
		const std::wregex re(pattern);
		(void)re;
		return true;
	}
	catch (const std::regex_error&)
	{
		return false;
	}
}

bool Merge20MasterWildcardMatch(const String& pattern, const String& line, bool ignoreCase)
{
	if (pattern.empty())
		return false;
	// The prediffer's wildcardToRegExp: split on `%`, escape
	// each part, join with `.*`; the match is a substring
	// search, so no anchors are added.
	String regexText;
	size_t pos = 0;
	while (true)
	{
		const size_t pct = pattern.find(L'%', pos);
		const size_t end = (pct == String::npos) ? pattern.size() : pct;
		for (size_t i = pos; i < end; ++i)
		{
			if (MasterRegexSpecial(pattern[i]))
				regexText += L'\\';
			regexText += pattern[i];
		}
		if (pct == String::npos)
			break;
		regexText += L".*";
		pos = pct + 1;
	}
	try
	{
		const std::wregex re(regexText, MasterRegexFlags(ignoreCase));
		return MasterRegexSearch(line, re);
	}
	catch (const std::regex_error&)
	{
		return false;
	}
}

bool Merge20MasterRuleMatchesLine(const Merge20ExcludeRule& rule, const String& line)
{
	if (rule.pattern.empty())
		return false;
	if (rule.useRegExp)
	{
		try
		{
			const std::wregex re(rule.pattern, MasterRegexFlags(rule.ignoreCase));
			return MasterRegexSearch(line, re);
		}
		catch (const std::regex_error&)
		{
			return false;
		}
	}
	return Merge20MasterWildcardMatch(rule.pattern, line, rule.ignoreCase);
}

bool Merge20MasterExcludeLine(const std::vector<Merge20ExcludeRule>& rules, const String& line)
{
	for (const Merge20ExcludeRule& rule : rules)
	{
		if (rule.enabled && Merge20MasterRuleMatchesLine(rule, line))
			return true;
	}
	return false;
}

std::vector<Merge20HighlightRule> Merge20MasterDefaultHighlightRules()
{
	// The HTML tool's highlight presets (17_v17.15.html line
	// 576): "Math.floor/round", the plain word for "own code",
	// and "tax rate / correction rate". Written with `\x`
	// escapes so this file stays ASCII (the S4 convention):
	// \xff0f is the full-width solidus, \x72ec\x81ea is the
	// "own code" word, \x7a0e\x7387\x30fb\x88dc\x6b63\x7387 is
	// "tax rate / correction rate".
	Merge20HighlightRule rounding;
	rounding.pattern = L"Math\\.floor|Math\\.round";
	rounding.name = L"Math.floor\xff0fround";
	rounding.useRegExp = true;
	rounding.enabled = true;
	Merge20HighlightRule ownCode;
	ownCode.pattern = L"\x72ec\x81ea";
	ownCode.name = L"\x72ec\x81ea";
	ownCode.useRegExp = false;
	ownCode.enabled = true;
	Merge20HighlightRule taxRate;
	taxRate.pattern = L"0\\.92|0\\.014";
	taxRate.name = L"\x7a0e\x7387\x30fb\x88dc\x6b63\x7387";
	taxRate.useRegExp = true;
	taxRate.enabled = true;
	return { rounding, ownCode, taxRate };
}

bool Merge20MastersToJson(const Merge20Masters& masters, std::string& outUtf8)
{
	std::vector<Merge20JsonValue> excludeRules;
	for (const Merge20ExcludeRule& rule : masters.excludeRules)
		excludeRules.push_back(MasterExcludeRuleToJson(rule));
	std::vector<Merge20JsonValue> highlightRules;
	for (const Merge20HighlightRule& rule : masters.highlightRules)
		highlightRules.push_back(MasterHighlightRuleToJson(rule));
	std::vector<std::pair<String, Merge20JsonValue>> rootMembers;
	rootMembers.emplace_back(L"app", Merge20JsonText(kMerge20MasterApp));
	rootMembers.emplace_back(L"version", Merge20JsonInt(kMerge20MasterFormatVersion));
	rootMembers.emplace_back(L"excludeIgnoreL2", Merge20JsonBool(masters.excludeIgnoreL2));
	rootMembers.emplace_back(L"excludeRules", Merge20JsonArray(std::move(excludeRules)));
	rootMembers.emplace_back(L"highlightRules", Merge20JsonArray(std::move(highlightRules)));
	rootMembers.emplace_back(L"riskKeywords", Merge20JsonText(masters.riskKeywords));
	outUtf8 = Merge20JsonStringify(Merge20JsonObject(std::move(rootMembers)));
	return true;
}

bool Merge20MastersFromJson(const std::string& utf8, Merge20Masters& out, std::string& errDetail, int& skippedRules)
{
	skippedRules = 0;
	errDetail.clear();
	out = Merge20Masters();
	Merge20JsonValue root;
	if (!Merge20JsonParse(utf8, root, errDetail))
		return false;
	if (root.type != Merge20JsonValue::Type::Object)
	{
		errDetail = "root is not an object";
		return false;
	}
	const Merge20JsonValue* app = root.Find(L"app");
	if (app == nullptr || app->AsText(String()) != kMerge20MasterApp)
	{
		errDetail = "wrong app";
		return false;
	}
	const Merge20JsonValue* version = root.Find(L"version");
	if (version == nullptr || version->AsInt(-1) != kMerge20MasterFormatVersion)
	{
		errDetail = "unsupported version";
		return false;
	}
	const Merge20JsonValue* ignoreL2 = root.Find(L"excludeIgnoreL2");
	out.excludeIgnoreL2 = (ignoreL2 == nullptr) ? true : ignoreL2->AsBool(true);
	const Merge20JsonValue* excludeRules = root.Find(L"excludeRules");
	if (excludeRules != nullptr && excludeRules->type == Merge20JsonValue::Type::Array)
	{
		for (const Merge20JsonValue& entry : excludeRules->array)
		{
			Merge20ExcludeRule rule;
			if (MasterExcludeRuleFromJson(entry, rule))
				out.excludeRules.push_back(rule);
			else
				++skippedRules;
		}
	}
	const Merge20JsonValue* highlightRules = root.Find(L"highlightRules");
	if (highlightRules != nullptr && highlightRules->type == Merge20JsonValue::Type::Array)
	{
		for (const Merge20JsonValue& entry : highlightRules->array)
		{
			Merge20HighlightRule rule;
			if (MasterHighlightRuleFromJson(entry, rule))
				out.highlightRules.push_back(rule);
			else
				++skippedRules;
		}
	}
	const Merge20JsonValue* riskKeywords = root.Find(L"riskKeywords");
	out.riskKeywords = (riskKeywords == nullptr) ? String() : riskKeywords->AsText(String());
	return true;
}
