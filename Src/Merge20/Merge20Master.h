// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Master.h
 *
 * @brief Master data for the Merge20 folder compare workflow:
 * exclude-word rules, highlight rules and risk keywords
 * (Merge20 Phase 4 D8, design sections 4.4 and 5.2/D8, spec
 * sections 3 (4/4) and 6).
 *
 * Pure module (no MFC, no `_()`): the same code is compiled into
 * WinMergeU and into the GoogleTest runner.
 *
 * Storage (design section 4.4):
 * - The exclude-word rules are the live input of the Phase 1
 *   `Merge20Exclude.sct` prediffer, which reads them from the
 *   option keys `Plugins\Merge20Exclude.sct/` (Count,
 *   Enabled<i>, IgnoreCase<i>, UseRegExp<i>, Pattern<i>,
 *   IgnoreL2). The MFC adapter (Merge20MasterDoc) syncs this
 *   struct to and from those keys, so the dialog edits exactly
 *   what the prediffer applies. The JSON file is the shareable
 *   snapshot of the same data, not a second live store.
 * - The highlight rules and the risk keywords have no other
 *   reader yet (highlight display in the compare views is
 *   follow-up work; the risk keywords are read from
 *   OPT_MERGE20_RISK_KEYWORDS by the S4 classifier). Their
 *   live store is this JSON file plus the option key.
 *
 * JSON format (one file, `Merge20Masters.json`, UTF-8, no BOM,
 * 2-space indent; the S8 lightweight JSON module
 * `Merge20Session.h/.cpp` supplies the DOM, parser and
 * serializer, as for the learning JSON v2):
 * @code
 * {
 *   "app": "Merge20Masters",
 *   "version": 1,
 *   "excludeIgnoreL2": true,
 *   "excludeRules": [
 *     {"pattern": "...", "useRegExp": false,
 *      "ignoreCase": false, "enabled": true}
 *   ],
 *   "highlightRules": [
 *     {"pattern": "...", "name": "...",
 *      "useRegExp": true, "enabled": true}
 *   ],
 *   "riskKeywords": "tax;amount;..."
 * }
 * @endcode
 * One rule is a partial match with a `%` wildcard or a regular
 * expression (the HTML tool's `exRules` / `hlRules`, 17_v17.15.html
 * lines 1008-1029): a plain pattern without `%` is a substring
 * match, a plain pattern with `%` matches any text at each `%`
 * (e.g. `--1.0.8%` matches `--1.0.813.000`), a regular-expression
 * pattern is a `std::wregex` search. The matching helpers below
 * mirror the prediffer's JScript (`wildcardToRegExp` /
 * `ruleMatchesLine`) so the dialog, the tests and the prediffer
 * agree on what a rule excludes.
 */
#pragma once

#include <string>
#include <vector>
#include "UnicodeString.h"

/** @brief Format version of the Merge20 masters file. */
constexpr int kMerge20MasterFormatVersion = 1;
/** @brief `app` marker of the Merge20 masters file. */
constexpr wchar_t kMerge20MasterApp[] = L"Merge20Masters";

/**
 * @brief One exclude-word rule (HTML `exRules` entry plus the
 * prediffer's ignore-case flag).
 */
struct Merge20ExcludeRule
{
	String pattern;     /**< `%` wildcard pattern, or a regular expression. */
	bool useRegExp = false; /**< True: pattern is a regular expression. */
	bool ignoreCase = false; /**< True: match without case distinction. */
	bool enabled = true;   /**< False: rule is kept but not applied. */
};

/**
 * @brief One highlight rule (HTML `hlRules` entry plus the
 * display name from design section 4.4; the HTML tool has no
 * name field, the presets are named here).
 */
struct Merge20HighlightRule
{
	String pattern;     /**< Text or regular expression to highlight. */
	String name;        /**< Display name (preset name, e.g. "Math.floor/round"). */
	bool useRegExp = false; /**< True: pattern is a regular expression. */
	bool enabled = true;   /**< False: rule is kept but not applied. */
};

/** @brief All three masters as edited by the master dialog. */
struct Merge20Masters
{
	/** @brief Ignore a 2nd-line version number (the prediffer's IgnoreL2, default ON). */
	bool excludeIgnoreL2 = true;
	std::vector<Merge20ExcludeRule> excludeRules;
	std::vector<Merge20HighlightRule> highlightRules;
	/** @brief Risk keywords, semicolon-separated (OPT_MERGE20_RISK_KEYWORDS form). */
	String riskKeywords;
};

/**
 * @brief True when the pattern compiles as a `std::wregex`
 * (ECMAScript syntax). An empty pattern is not a valid
 * regular expression.
 */
bool Merge20MasterIsValidRegex(const String& pattern);

/**
 * @brief Plain-pattern match with the `%` wildcard (the
 * prediffer's `wildcardToRegExp`): `%` stands for any
 * (possibly empty) string, the rest is literal, and the match
 * is a substring search (not anchored). A pattern without `%`
 * is a plain substring search.
 * @param [in] pattern Plain pattern, possibly with `%`.
 * @param [in] line Line text to test.
 * @param [in] ignoreCase True: match without case distinction.
 * @return True when the pattern matches the line.
 */
bool Merge20MasterWildcardMatch(const String& pattern, const String& line, bool ignoreCase);

/**
 * @brief One exclude rule against one line (the prediffer's
 * `ruleMatchesLine`): an empty pattern never matches; a
 * regular-expression rule is a `std::wregex` search, a plain
 * rule is Merge20MasterWildcardMatch. The rule's `enabled`
 * flag is NOT checked here (the prediffer checks it in the
 * loop); callers that apply rules must skip disabled ones.
 */
bool Merge20MasterRuleMatchesLine(const Merge20ExcludeRule& rule, const String& line);

/**
 * @brief True when any enabled exclude rule matches the line
 * (the prediffer's per-line loop).
 */
bool Merge20MasterExcludeLine(const std::vector<Merge20ExcludeRule>& rules, const String& line);

/**
 * @brief The HTML tool's highlight presets
 * (17_v17.15.html line 576), as the default highlight rules:
 * "Math.floor/round" (`Math\.floor|Math\.round`, regex),
 * the plain word for "own code", and "tax rate / correction
 * rate" (`0\.92|0\.014`, regex).
 */
std::vector<Merge20HighlightRule> Merge20MasterDefaultHighlightRules();

/**
 * @brief Serialize the masters to JSON text (UTF-8, no BOM,
 * 2-space indent, the S8 serializer's shape).
 * @param [in] masters Masters to write.
 * @param [out] outUtf8 JSON text.
 * @return True on success (always true for well-formed masters).
 */
bool Merge20MastersToJson(const Merge20Masters& masters, std::string& outUtf8);

/**
 * @brief Parse JSON text into the masters.
 * @param [in] utf8 UTF-8 JSON text (a leading UTF-8 BOM is skipped).
 * @param [out] out Parsed masters.
 * @param [out] errDetail Empty on success, otherwise the reason.
 * @param [out] skippedRules Rule entries skipped for a bad format
 * (a wrong-typed entry or a non-object entry); the rest of the
 * file still loads.
 * @return True on success. A wrong `app` or `version` fails the
 * load; a single bad rule entry is skipped instead (and counted).
 */
bool Merge20MastersFromJson(const std::string& utf8, Merge20Masters& out, std::string& errDetail, int& skippedRules);
