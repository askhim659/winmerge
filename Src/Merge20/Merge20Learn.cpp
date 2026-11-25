// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Learn.cpp
 *
 * @brief Implementation of the learning core (Phase 3 L0).
 *
 * Port of the HTML tool's learning functions (17_v17.15.html).
 * Differences from the HTML tool, all deliberate and covered by
 * Merge20Learn_test.cpp:
 * - The S4 classifier's rounding regex uses icase and its SQL regex
 *   includes `join`; the learning regexes below are the HTML tool's
 *   own patterns (round is case-sensitive, SQL has no `join`).
 * - A non-integer number anywhere in an input file makes the shared
 *   S8 parser reject the whole file, so a fractional count cannot be
 *   skipped per rule (design section 4.1 notes this deviation).
 * - Duplicate object keys: the S8 parser keeps them and Find()
 *   returns the first one (T-3-2 resolved by reading ParseObject);
 *   the counts parser below also takes the first occurrence.
 * - OP_TRIVIAL lines have no HTML equivalent; they count as "mod"
 *   for the feature-key type (a changed line pair).
 * - This module never applies a candidate (spec section 4(c)):
 *   Match() only computes the suggestion.
 *
 * This file stays ASCII: no Japanese literal is needed here (display
 * names are composed by the callers through `_()` in later slices,
 * except the two fixed HTML source names, written with \\x escapes).
 */
#include "pch.h"
#include <algorithm>
#include <climits>
#include <regex>
#include "Merge20/Merge20Learn.h"

const String kMerge20LearnApp = L"Merge20Learn";
const String kMerge20LearnHtmlApp = L"\x30de\x30fc\x30b8\x652f\x63f4" L"20" L"\x53f7";
const String kMerge20LearnSelfSourceId = L"self";
const String kMerge20LearnLegacySourceId = L"legacy";

namespace
{

	// Strategy names in canonical order (also the JSON `counts` key order
	// for a rule that was never reordered by imports).
	const wchar_t* kStrategyNames[] =
	{
		L"INSERT_OLD",
		L"KEEP_NEW",
		L"REPLACE_WITH_OLD",
		L"BOTH_OLD_NEW",
	};

	// JS whitespace (ECMAScript WhiteSpace + LineTerminator), used by
	// the exact-key normalizer so it matches the HTML tool's /\s+/g
	// without depending on the C locale.
	bool IsJsWhitespace(wchar_t ch)
	{
		if (ch >= 0x09 && ch <= 0x0D)
			return true;
		if (ch == 0x20 || ch == 0x00A0 || ch == 0x1680 || ch == 0x2028 ||
			ch == 0x2029 || ch == 0x202F || ch == 0x205F || ch == 0x3000 || ch == 0xFEFF)
			return true;
		if (ch >= 0x2000 && ch <= 0x200A)
			return true;
		return false;
	}

	// HTML hunkKeyExact norm(): collapse each run of whitespace to one
	// space and trim (JS String.replace(/\s+/g, ' ').trim()).
	String NormalizeLearnText(const String& text)
	{
		String out;
		bool pendingSpace = false;
		bool started = false;
		for (wchar_t ch : text)
		{
			if (IsJsWhitespace(ch))
			{
				if (started)
					pendingSpace = true;
				continue;
			}
			if (pendingSpace)
			{
				out += L' ';
				pendingSpace = false;
			}
			out += ch;
			started = true;
		}
		return out;
	}

	// HTML hunkFeatKey patterns (ECMAScript syntax, like Merge20Classify).
	const std::wregex& LearnRoundRegex()
	{
		// No icase flag in the HTML tool (unlike the S4 classifier).
		static const std::wregex re(LR"(Math\.(round|floor|ceil)|BigDecimal|RoundingMode)");
		return re;
	}

	const std::wregex& LearnIfRegex()
	{
		// No icase flag in the HTML tool (same as the S4 classifier).
		static const std::wregex re(LR"(\bif\s*\()");
		return re;
	}

	const std::wregex& LearnSqlRegex()
	{
		// No `join` in the HTML tool (unlike the S4 classifier).
		static const std::wregex re(LR"(\b(select|insert|update|delete|from|where)\b)", std::regex_constants::icase);
		return re;
	}

	// Tax / amount / rate / rounding characters (HTML /[税率額端数]/).
	bool HasLearnTaxChar(const String& text)
	{
		for (wchar_t ch : text)
		{
			if (ch == 0x7A0E || ch == 0x7387 || ch == 0x984D || ch == 0x7AEF || ch == 0x6570)
				return true;
		}
		return false;
	}

	// HTML /^\\s*\\/\\//m and /^\\s*\\/\\*/m on the old side: any old
	// line whose first non-whitespace characters are "//" or "/*".
	bool HasLearnCommentLine(const String& oldText)
	{
		size_t pos = 0;
		for (;;)
		{
			size_t eol = oldText.find(L'\n', pos);
			String line = oldText.substr(pos, eol == String::npos ? String::npos : eol - pos);
			size_t i = 0;
			while (i < line.length() && IsJsWhitespace(line[i]))
				++i;
			if (line.compare(i, 2, L"//") == 0 || line.compare(i, 2, L"/*") == 0)
				return true;
			if (eol == String::npos)
				return false;
			pos = eol + 1;
		}
	}

	// Adjust one strategy count by delta, clamped at 0. A count that
	// reaches 0 is removed, together with its slot in the insertion-order
	// list (HTML `delete counts[strat]` removes the key, so a later
	// re-add appends it at the end again).
	void AdjustCount(std::map<Merge20LearnStrategy, int>& counts,
		std::vector<Merge20LearnStrategy>& order, Merge20LearnStrategy strat, int delta)
	{
		auto it = counts.find(strat);
		int value = (it == counts.end() ? 0 : it->second) + delta;
		if (value <= 0)
		{
			if (it != counts.end())
				counts.erase(it);
			order.erase(std::remove(order.begin(), order.end(), strat), order.end());
			return;
		}
		if (it == counts.end())
		{
			counts[strat] = value;
			order.push_back(strat);
		}
		else
		{
			it->second = value;
		}
	}

	int SumCounts(const std::map<Merge20LearnStrategy, int>& counts)
	{
		int total = 0;
		for (const auto& kv : counts)
			total += kv.second;
		return total;
	}

	// HTML learnDominant: the strategy with the most judgments; on a tie
	// the latest choice (`lastStrat`) wins when it holds the maximum.
	// First-max follows insertion order (HTML Object.entries order),
	// which the `order` list preserves.
	bool DominantRule(const Merge20LearnRule& rule,
		Merge20LearnStrategy& strat, int& best, int& total)
	{
		total = SumCounts(rule.counts);
		if (total <= 0 || rule.order.empty())
			return false;
		best = -1;
		strat = rule.order.front();
		for (Merge20LearnStrategy s : rule.order)
		{
			auto it = rule.counts.find(s);
			if (it != rule.counts.end() && it->second > best)
			{
				best = it->second;
				strat = s;
			}
		}
		if (rule.hasLastStrat)
		{
			auto it = rule.counts.find(rule.lastStrat);
			if (it != rule.counts.end() && it->second == best)
				strat = rule.lastStrat;
		}
		return best > 0;
	}

	// Diff lines of one diff, without OP_NONE (context) lines.
	std::vector<Merge20DiffLine> DiffOnlyLines(const std::vector<Merge20DiffLine>& lines)
	{
		std::vector<Merge20DiffLine> diffs;
		for (const auto& line : lines)
		{
			if (line.op != OP_NONE)
				diffs.push_back(line);
		}
		return diffs;
	}

	// JSON helpers (the DOM / parser / serializer are the S8 module's).
	String GetTextField(const Merge20JsonValue& obj, const String& field)
	{
		const Merge20JsonValue* value = obj.Find(field);
		return (value != nullptr && value->type == Merge20JsonValue::Type::Text)
			? value->text : String();
	}

	Merge20JsonValue LearnCountsJson(const std::map<Merge20LearnStrategy, int>& counts,
		const std::vector<Merge20LearnStrategy>& order)
	{
		std::vector<std::pair<String, Merge20JsonValue>> members;
		for (Merge20LearnStrategy strat : order)
		{
			auto it = counts.find(strat);
			if (it != counts.end())
				members.emplace_back(Merge20LearnStrategyName(strat), Merge20JsonInt(it->second));
		}
		return Merge20JsonObject(std::move(members));
	}

	Merge20JsonValue LearnSourceJson(const Merge20LearnSourceContribution& src)
	{
		std::vector<std::pair<String, Merge20JsonValue>> members;
		members.emplace_back(L"name", Merge20JsonText(src.name));
		members.emplace_back(L"counts", LearnCountsJson(src.counts, src.order));
		members.emplace_back(L"n", Merge20JsonInt(src.n));
		return Merge20JsonObject(std::move(members));
	}

	// One rule as a JSON object. `includeSources` is false for merged
	// (unified) output; `alwaysLastStrat` is true there because the HTML
	// tool's mergeLearnSources always writes lastStrat ("" when unset).
	Merge20JsonValue LearnRuleJson(const Merge20LearnRule& rule,
		bool includeSources, bool alwaysLastStrat)
	{
		std::vector<std::pair<String, Merge20JsonValue>> members;
		members.emplace_back(L"key", Merge20JsonText(rule.key));
		members.emplace_back(L"counts", LearnCountsJson(rule.counts, rule.order));
		members.emplace_back(L"n", Merge20JsonInt(rule.n));
		members.emplace_back(L"ex", Merge20JsonText(rule.ex));
		members.emplace_back(L"last", Merge20JsonText(rule.last));
		if (rule.hasLastStrat || alwaysLastStrat)
		{
			members.emplace_back(L"lastStrat", Merge20JsonText(
				rule.hasLastStrat ? Merge20LearnStrategyName(rule.lastStrat) : String()));
		}
		if (includeSources)
		{
			std::vector<std::pair<String, Merge20JsonValue>> sources;
			for (const auto& kv : rule.sources)
				sources.emplace_back(kv.first, LearnSourceJson(kv.second));
			members.emplace_back(L"sources", Merge20JsonObject(std::move(sources)));
		}
		return Merge20JsonObject(std::move(members));
	}

	bool IsValidLearnKey(const String& key)
	{
		if (key.rfind(L"EXACT|", 0) == 0)
			return true;
		if (key.rfind(L"FEAT|", 0) == 0)
		{
			String rest = key.substr(5);
			size_t bar = rest.find(L'|');
			if (bar == String::npos)
				return false;
			String type = rest.substr(0, bar);
			String feat = rest.substr(bar + 1);
			if (type != L"del" && type != L"mod" && type != L"add")
				return false;
			if (feat != L"round" && feat != L"ifdel" && feat != L"sql" &&
				feat != L"tax" && feat != L"comment")
				return false;
			return true;
		}
		return false;
	}

	// Parse one counts object. Unknown strategy names are ignored per
	// key; a non-integer or negative value fails the whole rule.
	// Duplicate keys keep the first occurrence (like Find()).
	bool ParseLearnCounts(const Merge20JsonValue* value,
		std::map<Merge20LearnStrategy, int>& countsOut,
		std::vector<Merge20LearnStrategy>& orderOut)
	{
		if (value == nullptr || value->type != Merge20JsonValue::Type::Object)
			return false;
		for (const auto& member : value->object)
		{
			Merge20LearnStrategy strat;
			if (!Merge20LearnStrategyFromName(member.first, strat))
				continue;
			if (member.second.type != Merge20JsonValue::Type::Int ||
				member.second.integer < 0 ||
				member.second.integer > INT_MAX)
				return false;
			if (countsOut.find(strat) != countsOut.end())
				continue;
			countsOut[strat] = static_cast<int>(member.second.integer);
			orderOut.push_back(strat);
		}
		return true;
	}

	// One parsed rule (shared by FromJson and ImportJson).
	struct ParsedLearnRule
	{
		String key;
		std::map<Merge20LearnStrategy, int> counts;
		std::vector<Merge20LearnStrategy> order;
		String ex;
		String last;
		bool hasLastStrat = false;
		Merge20LearnStrategy lastStrat = Merge20LearnStrategy::KeepNew;
		bool hasSources = false;
		std::map<String, Merge20LearnSourceContribution> sources;
	};

	struct ParsedLearnFile
	{
		Merge20LearnFileHeader header;
		std::vector<ParsedLearnRule> rules;
	};

	bool ParseLearnRule(const Merge20JsonValue& value, const String& fallbackKey,
		ParsedLearnRule& out)
	{
		if (value.type != Merge20JsonValue::Type::Object)
			return false;
		String key = GetTextField(value, L"key");
		if (key.empty())
			key = fallbackKey;
		if (key.empty() || !IsValidLearnKey(key))
			return false;
		if (!ParseLearnCounts(value.Find(L"counts"), out.counts, out.order))
			return false;
		if (out.counts.empty())
			return false;
		out.key = key;
		out.ex = GetTextField(value, L"ex");
		out.last = GetTextField(value, L"last");
		const Merge20JsonValue* lastStrat = value.Find(L"lastStrat");
		if (lastStrat != nullptr && lastStrat->type == Merge20JsonValue::Type::Text)
		{
			Merge20LearnStrategy strat;
			if (Merge20LearnStrategyFromName(lastStrat->text, strat) &&
				out.counts.find(strat) != out.counts.end())
			{
				out.hasLastStrat = true;
				out.lastStrat = strat;
			}
		}
		const Merge20JsonValue* sources = value.Find(L"sources");
		if (sources != nullptr && sources->type == Merge20JsonValue::Type::Object)
		{
			out.hasSources = true;
			for (const auto& member : sources->object)
			{
				if (member.second.type != Merge20JsonValue::Type::Object)
					continue;
				Merge20LearnSourceContribution contrib;
				contrib.name = GetTextField(member.second, L"name");
				if (!ParseLearnCounts(member.second.Find(L"counts"),
					contrib.counts, contrib.order))
					continue;
				contrib.n = SumCounts(contrib.counts);
				if (contrib.n == 0)
					continue;
				if (out.sources.find(member.first) == out.sources.end())
					out.sources[member.first] = std::move(contrib);
			}
			if (out.sources.empty())
				out.hasSources = false;
		}
		return true;
	}

	bool ParseLearnFile(const std::string& utf8, const String& fileName,
		ParsedLearnFile& out, std::string& errDetail, int& skippedRules)
	{
		skippedRules = 0;
		errDetail.clear();
		Merge20JsonValue root;
		if (!Merge20JsonParse(utf8, root, errDetail))
			return false;
		if (root.type != Merge20JsonValue::Type::Object)
		{
			errDetail = "top level is not an object";
			return false;
		}
		Merge20LearnFileHeader header;
		const Merge20JsonValue* app = root.Find(L"app");
		if (app != nullptr)
		{
			if (app->type != Merge20JsonValue::Type::Text ||
				(app->text != kMerge20LearnApp && app->text != kMerge20LearnHtmlApp))
			{
				errDetail = "unsupported app";
				return false;
			}
			header.app = app->text;
			header.hasApp = true;
		}
		const Merge20JsonValue* version = root.Find(L"version");
		if (version != nullptr)
		{
			if (version->type != Merge20JsonValue::Type::Int ||
				(version->integer != 1 && version->integer != kMerge20LearnFormatVersion))
			{
				errDetail = "unsupported version";
				return false;
			}
			header.version = static_cast<int>(version->integer);
			header.hasVersion = true;
		}
		header.sourceId = GetTextField(root, L"sourceId");
		header.sourceName = GetTextField(root, L"sourceName");
		header.exportedAt = GetTextField(root, L"exportedAt");
		const Merge20JsonValue* sources = root.Find(L"sources");
		if (sources != nullptr && sources->type == Merge20JsonValue::Type::Array)
		{
			for (const auto& item : sources->array)
			{
				if (item.type == Merge20JsonValue::Type::Text)
					header.sourceNames.push_back(item.text);
			}
		}
		if (header.sourceId.empty())
			header.sourceId = L"file:" + Merge20LearnHashStr(fileName);
		if (header.sourceName.empty())
			header.sourceName = fileName;
		out.header = header;
		const Merge20JsonValue* rules = root.Find(L"rules");
		if (rules != nullptr)
		{
			if (rules->type == Merge20JsonValue::Type::Array)
			{
				for (const auto& item : rules->array)
				{
					ParsedLearnRule parsed;
					if (!ParseLearnRule(item, String(), parsed))
					{
						++skippedRules;
						continue;
					}
					out.rules.push_back(std::move(parsed));
				}
			}
			else if (rules->type == Merge20JsonValue::Type::Object)
			{
				for (const auto& member : rules->object)
				{
					ParsedLearnRule parsed;
					if (!ParseLearnRule(member.second, member.first, parsed))
					{
						++skippedRules;
						continue;
					}
					out.rules.push_back(std::move(parsed));
				}
			}
			else
			{
				errDetail = "rules is not an array or object";
				return false;
			}
		}
		else
		{
			// HTML importLearn fallback: the document itself is a rules map.
			static const wchar_t* kHeaderKeys[] =
			{
				L"app", L"version", L"sourceId", L"sourceName", L"exportedAt", L"sources", L"rules",
			};
			for (const auto& member : root.object)
			{
				bool isHeader = false;
				for (const wchar_t* headerKey : kHeaderKeys)
				{
					if (member.first == headerKey)
					{
						isHeader = true;
						break;
					}
				}
				if (isHeader)
					continue;
				ParsedLearnRule parsed;
				if (!ParseLearnRule(member.second, member.first, parsed))
				{
					++skippedRules;
					continue;
				}
				out.rules.push_back(std::move(parsed));
			}
		}
		return true;
	}

	// Lowercase hex without padding (JS Number.toString(16)).
	String ToHexUnpadded(uint32_t value)
	{
		const wchar_t* digits = L"0123456789abcdef";
		if (value == 0)
			return L"0";
		String out;
		while (value != 0)
		{
			out = String(1, digits[value & 0xF]) + out;
			value >>= 4;
		}
		return out;
	}

} // namespace

bool Merge20LearnDominant(const Merge20LearnRule& rule,
	Merge20LearnStrategy& strat, int& best, int& total)
{
	// The matcher (Merge20LearnStore::Match) and the learning dialog
	// (L4) share this one implementation (HTML `learnDominant`).
	return DominantRule(rule, strat, best, total);
}

String Merge20LearnStrategyName(Merge20LearnStrategy strategy)
{
	return kStrategyNames[static_cast<int>(strategy)];
}

bool Merge20LearnStrategyFromName(const String& name, Merge20LearnStrategy& out)
{
	for (int i = 0; i < 4; ++i)
	{
		if (name == kStrategyNames[i])
		{
			out = static_cast<Merge20LearnStrategy>(i);
			return true;
		}
	}
	return false;
}

String Merge20LearnSourceDisplayName(const String& srcId, const String& storedName)
{
	if (!storedName.empty())
		return storedName;
	if (srcId == kMerge20LearnSelfSourceId)
		return L"\x3053\x306e\x30d6\x30e9\x30a6\x30b6\x3067\x5b66\x7fd2\xff08\x81ea\x5206\xff09";
	if (srcId == kMerge20LearnLegacySourceId)
		return L"\x4ee5\x524d\x306e\x30c7\x30fc\x30bf\xff08\x51fa\x5178\x4e0d\x660e\xff09";
	return srcId;
}

String Merge20LearnHashStr(const String& text)
{
	// HTML hashStr (cyrb53-style): h1 = 0xdeadbeef, h2 = 0x41c6ce57,
	// per UTF-16 code unit, then a final mix. uint32_t arithmetic wraps
	// exactly like JS Math.imul (32-bit signed multiply).
	auto imul = [](uint32_t a, uint32_t b) { return a * b; };
	uint32_t h1 = 0xdeadbeefu;
	uint32_t h2 = 0x41c6ce57u;
	for (wchar_t ch : text)
	{
		uint32_t c = static_cast<uint32_t>(ch);
		h1 = imul(h1 ^ c, 2654435761u);
		h2 = imul(h2 ^ c, 1597334677u);
	}
	h1 = imul(h1 ^ (h1 >> 16), 2246822507u) ^ imul(h2 ^ (h2 >> 13), 3266489909u);
	h2 = imul(h2 ^ (h2 >> 16), 2246822507u) ^ imul(h1 ^ (h1 >> 13), 3266489909u);
	return ToHexUnpadded(h2) + ToHexUnpadded(h1);
}

String Merge20LearnExactKey(const std::vector<Merge20DiffLine>& lines)
{
	String key = L"EXACT|";
	bool first = true;
	for (const auto& line : lines)
	{
		if (line.op == OP_NONE)
			continue;
		if (!first)
			key += L"\n";
		first = false;
		key += NormalizeLearnText(line.oldText);
		key += L"\x21c4";
		key += NormalizeLearnText(line.newText);
	}
	return key;
}

bool Merge20LearnFeatureKey(const std::vector<Merge20DiffLine>& lines, String& keyOut)
{
	bool allDel = true;
	bool anyMod = false;
	bool anyRow = false;
	String oldText;
	String newText;
	bool first = true;
	for (const auto& line : lines)
	{
		if (line.op == OP_NONE)
			continue;
		anyRow = true;
		if (line.op != OP_1STONLY)
			allDel = false;
		if (line.op == OP_DIFF || line.op == OP_TRIVIAL)
			anyMod = true;
		if (!first)
		{
			oldText += L"\n";
			newText += L"\n";
		}
		first = false;
		oldText += line.oldText;
		newText += line.newText;
	}
	if (!anyRow)
		return false;
	String type = allDel ? L"del" : (anyMod ? L"mod" : L"add");
	String both = oldText + L"\n" + newText;
	String feat;
	if (std::regex_search(both, LearnRoundRegex()))
		feat = L"round";
	else if (std::regex_search(oldText, LearnIfRegex()))
		feat = L"ifdel";
	else if (std::regex_search(both, LearnSqlRegex()))
		feat = L"sql";
	else if (HasLearnTaxChar(both))
		feat = L"tax";
	else if (HasLearnCommentLine(oldText))
		feat = L"comment";
	else
		return false;
	keyOut = L"FEAT|" + type + L"|" + feat;
	return true;
}

int Merge20LearnStore::TotalJudgments() const
{
	int total = 0;
	for (const auto& kv : m_rules)
		total += kv.second.n;
	return total;
}

const Merge20LearnRule* Merge20LearnStore::FindRule(const String& key) const
{
	auto it = m_rules.find(key);
	return it == m_rules.end() ? nullptr : &it->second;
}

void Merge20LearnStore::Add(const String& key, Merge20LearnStrategy strat,
	const String& srcId, const String& srcName, int delta)
{
	auto it = m_rules.find(key);
	if (it == m_rules.end())
	{
		if (delta <= 0)
			return;
		Merge20LearnRule rule;
		rule.key = key;
		it = m_rules.emplace(key, std::move(rule)).first;
	}
	Merge20LearnRule& rec = it->second;
	auto sit = rec.sources.find(srcId);
	if (sit == rec.sources.end())
	{
		Merge20LearnSourceContribution src;
		src.name = srcName;
		sit = rec.sources.emplace(srcId, std::move(src)).first;
	}
	Merge20LearnSourceContribution& src = sit->second;
	AdjustCount(src.counts, src.order, strat, delta);
	AdjustCount(rec.counts, rec.order, strat, delta);
	src.n = SumCounts(src.counts);
	rec.n = SumCounts(rec.counts);
	if (src.n == 0)
		rec.sources.erase(sit);
	if (rec.n == 0)
		m_rules.erase(it);
}

void Merge20LearnStore::Record(const String& key, Merge20LearnStrategy strat,
	const Merge20LearnStrategy* prevStrat,
	const String& srcId, const String& srcName,
	const String& ex, const String& last)
{
	if (prevStrat != nullptr && *prevStrat != strat)
		Add(key, *prevStrat, srcId, srcName, -1);
	Add(key, strat, srcId, srcName, 1);
	Merge20LearnRuleMeta meta;
	meta.setEx = true;
	meta.ex = ex;
	meta.setLast = true;
	meta.last = last;
	meta.setLastStrat = true;
	meta.lastStrat = strat;
	SetRuleMeta(key, meta);
}

void Merge20LearnStore::SetRuleMeta(const String& key, const Merge20LearnRuleMeta& meta)
{
	auto it = m_rules.find(key);
	if (it == m_rules.end())
		return;
	if (meta.setEx)
		it->second.ex = meta.ex;
	if (meta.setLast)
		it->second.last = meta.last;
	if (meta.setLastStrat)
	{
		it->second.lastStrat = meta.lastStrat;
		it->second.hasLastStrat = true;
	}
}

void Merge20LearnStore::RemoveSource(const String& srcId)
{
	for (auto it = m_rules.begin(); it != m_rules.end();)
	{
		Merge20LearnRule& rec = it->second;
		auto sit = rec.sources.find(srcId);
		if (sit == rec.sources.end())
		{
			++it;
			continue;
		}
		for (Merge20LearnStrategy strat : sit->second.order)
			AdjustCount(rec.counts, rec.order, strat, -sit->second.counts.at(strat));
		rec.sources.erase(sit);
		rec.n = SumCounts(rec.counts);
		if (rec.n == 0)
			it = m_rules.erase(it);
		else
			++it;
	}
}

std::vector<Merge20LearnSourceSummary> Merge20LearnStore::GetSources() const
{
	std::map<String, Merge20LearnSourceSummary> byId;
	for (const auto& kv : m_rules)
	{
		for (const auto& src : kv.second.sources)
		{
			Merge20LearnSourceSummary& entry = byId[src.first];
			if (entry.id.empty())
			{
				entry.id = src.first;
				entry.name = Merge20LearnSourceDisplayName(src.first, src.second.name);
			}
			entry.rules += 1;
			entry.n += src.second.n;
		}
	}
	std::vector<Merge20LearnSourceSummary> out;
	for (const auto& kv : byId)
		out.push_back(kv.second);
	std::sort(out.begin(), out.end(),
		[](const Merge20LearnSourceSummary& a, const Merge20LearnSourceSummary& b)
		{
			if (a.n != b.n)
				return a.n > b.n;
			return a.id < b.id;
		});
	return out;
}

std::vector<Merge20LearnRule> Merge20LearnStore::MergeSources(const std::vector<String>& srcIds) const
{
	std::vector<Merge20LearnRule> merged;
	for (const auto& kv : m_rules)
	{
		Merge20LearnRule out;
		out.key = kv.first;
		out.ex = kv.second.ex;
		out.last = kv.second.last;
		out.lastStrat = kv.second.lastStrat;
		out.hasLastStrat = kv.second.hasLastStrat;
		for (const String& srcId : srcIds)
		{
			auto sit = kv.second.sources.find(srcId);
			if (sit == kv.second.sources.end())
				continue;
			for (Merge20LearnStrategy strat : sit->second.order)
				AdjustCount(out.counts, out.order, strat, sit->second.counts.at(strat));
		}
		out.n = SumCounts(out.counts);
		if (out.n > 0)
			merged.push_back(std::move(out));
	}
	return merged;
}

bool Merge20LearnStore::Match(const std::vector<Merge20DiffLine>& lines,
	Merge20LearnCandidate& out) const
{
	std::vector<Merge20DiffLine> diffs = DiffOnlyLines(lines);
	if (diffs.empty())
		return false;
	const Merge20LearnRule* exact = FindRule(Merge20LearnExactKey(diffs));
	if (exact != nullptr)
	{
		Merge20LearnStrategy strat;
		int best = 0;
		int total = 0;
		if (Merge20LearnDominant(*exact, strat, best, total) && best >= 1)
		{
			out.strategy = strat;
			out.via = L"exact";
			out.n = best;
			out.diffIdx = -1;
			out.srcPanes.clear();
			return true;
		}
	}
	String featKey;
	if (Merge20LearnFeatureKey(diffs, featKey))
	{
		const Merge20LearnRule* feat = FindRule(featKey);
		if (feat != nullptr)
		{
			Merge20LearnStrategy strat;
			int best = 0;
			int total = 0;
			// 80% with integers: best*5 >= total*4 (no floating point,
			// matching the S8 JSON module, which stores integers only).
			if (Merge20LearnDominant(*feat, strat, best, total) && total >= 3 && best * 5 >= total * 4)
			{
				out.strategy = strat;
				out.via = L"feat";
				out.n = total;
				out.diffIdx = -1;
				out.srcPanes.clear();
				return true;
			}
		}
	}
	return false;
}

void Merge20LearnRecordJudgment(Merge20LearnStore& store,
	const std::vector<Merge20DiffLine>& lines,
	Merge20LearnStrategy strategy, const Merge20LearnStrategy* prevLearned,
	const String& srcId, const String& srcName,
	const String& ex, const String& last)
{
	std::vector<Merge20DiffLine> diffs = DiffOnlyLines(lines);
	if (diffs.empty())
		return;
	store.Record(Merge20LearnExactKey(diffs), strategy, prevLearned, srcId, srcName, ex, last);
	String featKey;
	if (Merge20LearnFeatureKey(diffs, featKey))
		store.Record(featKey, strategy, prevLearned, srcId, srcName, ex, last);
}

bool Merge20LearnToJson(const Merge20LearnStore& store,
	const String& sourceId, const String& sourceName, const String& exportedAt,
	std::string& outUtf8)
{
	std::vector<std::pair<String, Merge20JsonValue>> root;
	root.emplace_back(L"app", Merge20JsonText(kMerge20LearnApp));
	root.emplace_back(L"version", Merge20JsonInt(kMerge20LearnFormatVersion));
	root.emplace_back(L"sourceId", Merge20JsonText(sourceId));
	root.emplace_back(L"sourceName", Merge20JsonText(sourceName));
	if (!exportedAt.empty())
		root.emplace_back(L"exportedAt", Merge20JsonText(exportedAt));
	std::vector<Merge20JsonValue> rules;
	for (const auto& kv : store.Rules())
		rules.push_back(LearnRuleJson(kv.second, true, false));
	root.emplace_back(L"rules", Merge20JsonArray(std::move(rules)));
	outUtf8 = Merge20JsonStringify(Merge20JsonObject(std::move(root)));
	return true;
}

bool Merge20LearnMergedToJson(const std::vector<Merge20LearnRule>& rules,
	const String& sourceId, const String& sourceName, const String& exportedAt,
	const std::vector<String>& sourceNames, std::string& outUtf8)
{
	std::vector<std::pair<String, Merge20JsonValue>> root;
	root.emplace_back(L"app", Merge20JsonText(kMerge20LearnApp));
	root.emplace_back(L"version", Merge20JsonInt(kMerge20LearnFormatVersion));
	root.emplace_back(L"sourceId", Merge20JsonText(sourceId));
	root.emplace_back(L"sourceName", Merge20JsonText(sourceName));
	if (!exportedAt.empty())
		root.emplace_back(L"exportedAt", Merge20JsonText(exportedAt));
	std::vector<Merge20JsonValue> names;
	for (const String& name : sourceNames)
		names.push_back(Merge20JsonText(name));
	root.emplace_back(L"sources", Merge20JsonArray(std::move(names)));
	std::vector<Merge20JsonValue> ruleValues;
	for (const auto& rule : rules)
		ruleValues.push_back(LearnRuleJson(rule, false, true));
	root.emplace_back(L"rules", Merge20JsonArray(std::move(ruleValues)));
	outUtf8 = Merge20JsonStringify(Merge20JsonObject(std::move(root)));
	return true;
}

bool Merge20LearnFromJson(const std::string& utf8, const String& fileName,
	Merge20LearnStore& out, Merge20LearnFileHeader& header,
	std::string& errDetail, int& skippedRules)
{
	ParsedLearnFile parsed;
	if (!ParseLearnFile(utf8, fileName, parsed, errDetail, skippedRules))
		return false;
	out.Clear();
	header = parsed.header;
	for (const auto& rule : parsed.rules)
	{
		if (rule.hasSources)
		{
			for (const auto& src : rule.sources)
			{
				for (Merge20LearnStrategy strat : src.second.order)
					out.Add(rule.key, strat, src.first, src.second.name,
						src.second.counts.at(strat));
			}
		}
		else
		{
			for (Merge20LearnStrategy strat : rule.order)
				out.Add(rule.key, strat, parsed.header.sourceId, parsed.header.sourceName,
					rule.counts.at(strat));
		}
		Merge20LearnRuleMeta meta;
		meta.setEx = true;
		meta.ex = rule.ex;
		meta.setLast = true;
		meta.last = rule.last;
		meta.setLastStrat = rule.hasLastStrat;
		meta.lastStrat = rule.lastStrat;
		out.SetRuleMeta(rule.key, meta);
	}
	return true;
}

bool Merge20LearnImportJson(const std::string& utf8, const String& fileName,
	Merge20LearnStore& store, Merge20LearnFileHeader& header,
	std::string& errDetail, int& skippedRules, int& addedRules, int& mergedRules)
{
	ParsedLearnFile parsed;
	if (!ParseLearnFile(utf8, fileName, parsed, errDetail, skippedRules))
		return false;
	header = parsed.header;
	addedRules = 0;
	mergedRules = 0;
	// Same file re-imported replaces its contribution (no double count).
	store.RemoveSource(header.sourceId);
	for (const auto& rule : parsed.rules)
	{
		const bool existed = store.FindRule(rule.key) != nullptr;
		for (Merge20LearnStrategy strat : rule.order)
			store.Add(rule.key, strat, header.sourceId, header.sourceName,
				rule.counts.at(strat));
		const Merge20LearnRule* rec = store.FindRule(rule.key);
		if (rec != nullptr)
		{
			Merge20LearnRuleMeta meta;
			if (!rule.ex.empty())
			{
				meta.setEx = true;
				meta.ex = rule.ex;
			}
			if (!rule.last.empty())
			{
				meta.setLast = true;
				meta.last = rule.last;
			}
			if (rule.hasLastStrat)
			{
				meta.setLastStrat = true;
				meta.lastStrat = rule.lastStrat;
			}
			store.SetRuleMeta(rule.key, meta);
		}
		if (existed)
			++mergedRules;
		else
			++addedRules;
	}
	return true;
}
