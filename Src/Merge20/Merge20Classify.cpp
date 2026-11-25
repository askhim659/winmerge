/**
 * @file  Merge20Classify.cpp
 *
 * @brief Implementation of the rule-based classification (S4).
 *
 * Port of the HTML tool's `runAnalysis` classification and
 * `analyzeInsights` (17_v17.15.html). The two passes mirror the HTML
 * structure: the first pass counts the line kinds, the second pass
 * scans the diff lines for the deterministic insights (rounding,
 * SQL, deleted if guard, condition/boundary change, constant change)
 * and the affected function names, stopping after 4 insights like
 * the HTML tool does.
 *
 * This file stays ASCII: the fixed insight words are not composed
 * here (the caller composes the summary text through `_()`), and the
 * risk keywords come from the OPT_MERGE20_RISK_KEYWORDS option, so
 * no Japanese literal is needed here.
 */
#include "pch.h"
#include <algorithm>
#include <cwctype>
#include <regex>
#include "Merge20/Merge20Classify.h"

namespace
{

	// HTML analyzeInsights patterns, rewritten for std::wregex
	// (ECMAScript syntax). Kept as function-local statics so they are
	// compiled once.
	const std::wregex& RoundingRegex()
	{
		static const std::wregex re(LR"(Math\.(round|floor|ceil)|BigDecimal|RoundingMode)", std::regex_constants::icase);
		return re;
	}

	const std::wregex& SqlRegex()
	{
		static const std::wregex re(LR"(\b(select|insert|update|delete|from|where|join)\b)", std::regex_constants::icase);
		return re;
	}

	const std::wregex& IfRegex()
	{
		// HTML uses /\bif\s*\(/ without the i flag (case-sensitive).
		static const std::wregex re(LR"(\bif\s*\()");
		return re;
	}

	const std::wregex& FunctionRegex()
	{
		static const std::wregex re(LR"((?:public|private|protected|static|final|synchronized|native|abstract)\s+(?:[\w<>\[\],\s]+?\s+)?(\w+)\s*\([^;]*\)|function\s+(\w+)\s*\(|def\s+(\w+)\s*\(|(?:Sub|Function)\s+(\w+)\s*\()", std::regex_constants::icase);
		return re;
	}

	const std::wregex& OperatorRegex()
	{
		static const std::wregex re(LR"(<=|>=|==|!=|&&|\|\||<|>)");
		return re;
	}

	const std::wregex& NumberRegex()
	{
		static const std::wregex re(LR"(\d+(?:\.\d+)?)");
		return re;
	}

	// HTML opsOf: the sorted, comma-joined operator list of a line.
	String OperatorsOf(const String& text)
	{
		std::vector<String> ops;
		for (std::wsregex_iterator it(text.begin(), text.end(), OperatorRegex()), end; it != end; ++it)
			ops.push_back(it->str());
		std::sort(ops.begin(), ops.end());
		return strutils::join(ops.begin(), ops.end(), String(_T(",")));
	}

	// HTML numsOf: the number list of a line, in order.
	std::vector<String> NumbersOf(const String& text)
	{
		std::vector<String> nums;
		for (std::wsregex_iterator it(text.begin(), text.end(), NumberRegex()), end; it != end; ++it)
			nums.push_back(it->str());
		return nums;
	}

	// Case-insensitive substring search (ASCII keywords such as "SQL"
	// must also match "sql"; towlower is the identity for Japanese
	// characters, so the same code handles both).
	bool ContainsKeyword(const String& text, const String& keyword)
	{
		if (keyword.empty() || text.length() < keyword.length())
			return false;
		for (size_t i = 0; i + keyword.length() <= text.length(); ++i)
		{
			bool match = true;
			for (size_t j = 0; j < keyword.length(); ++j)
			{
				if (std::towlower(text[i + j]) != std::towlower(keyword[j]))
				{
					match = false;
					break;
				}
			}
			if (match)
				return true;
		}
		return false;
	}

	String Trim(const String& text)
	{
		size_t begin = 0;
		while (begin < text.length() && std::iswspace(text[begin]))
			++begin;
		size_t end = text.length();
		while (end > begin && std::iswspace(text[end - 1]))
			--end;
		return text.substr(begin, end - begin);
	}

} // namespace

bool Merge20MatchFunctionName(const String& text, String& functionName)
{
	std::wsmatch match;
	if (!std::regex_search(text, match, FunctionRegex()))
		return false;
	for (size_t i = 1; i <= 4; ++i)
	{
		if (match[i].matched)
		{
			functionName = match[i].str();
			return true;
		}
	}
	return false;
}

std::vector<String> Merge20ParseRiskKeywords(const String& text)
{
	std::vector<String> keywords;
	for (const StringView& part : strutils::split(text, _T(';')))
	{
		String keyword = Trim(String(part));
		if (!keyword.empty())
			keywords.push_back(keyword);
	}
	return keywords;
}

Merge20ClassifyResult Merge20ClassifyDiffLines(const std::vector<Merge20DiffLine>& lines, const std::vector<String>& riskKeywords)
{
	Merge20ClassifyResult result;

	// Pass 1: count the line kinds and look for risk keywords. The
	// keyword scan runs over every diff line (it must not stop at the
	// insight cap of pass 2).
	for (const Merge20DiffLine& line : lines)
	{
		switch (line.op)
		{
		case OP_1STONLY:
			++result.nMissing;
			break;
		case OP_2NDONLY:
			++result.nAdded;
			break;
		case OP_DIFF:
			++result.nModified;
			break;
		case OP_TRIVIAL:
			++result.nExcluded;
			break;
		case OP_NONE:
		case OP_3RDONLY:
		default:
			break;
		}
		if (line.op == OP_1STONLY || line.op == OP_2NDONLY || line.op == OP_DIFF)
		{
			if (!result.hasRiskKeyword)
			{
				for (const String& keyword : riskKeywords)
				{
					if (ContainsKeyword(line.oldText, keyword) || ContainsKeyword(line.newText, keyword))
					{
						result.hasRiskKeyword = true;
						break;
					}
				}
			}
		}
	}

	// Action category (HTML runAnalysis).
	const int dc = result.nMissing;
	const int mc = result.nModified;
	const int acnt = result.nAdded;
	if (dc == 0 && mc == 0 && acnt == 0)
		result.action = ActionCategory::NO_ACTION_IDENTICAL;
	else if (dc > 0 && mc == 0)
		result.action = ActionCategory::MERGE_CLEAN_OLD;
	else if (dc > 0 && mc > 0)
		result.action = ActionCategory::REVIEW_MIXED;
	else if (dc == 0 && mc > 0)
		result.action = ActionCategory::REVIEW_MOD_ONLY;
	else
		result.action = ActionCategory::NO_MERGE_NEW_ADD;

	// Risk level (spec section 3 (2/4)): one attention point means
	// Medium, several overlapping points mean High. The structural
	// points alone reproduce the HTML risk table.
	result.attentionPoints = (dc > 0 ? 1 : 0) + (mc > 0 ? 1 : 0) + (result.hasRiskKeyword ? 1 : 0);
	result.risk = result.attentionPoints == 0 ? RiskLevel::NONE :
		result.attentionPoints == 1 ? RiskLevel::MEDIUM : RiskLevel::HIGH;

	// Pass 2: deterministic insights (HTML analyzeInsights), in file
	// order, at most 4 insights. The current function is updated from
	// every line (including context lines), exactly like the HTML tool,
	// but only non-trivial diff lines add it to the affected list.
	String curFn = _T("-");
	bool seenRounding = false, seenSql = false, seenIfDel = false, seenOp = false, seenNum = false;
	for (const Merge20DiffLine& line : lines)
	{
		if (result.insights.size() >= 4)
			break;
		String fn;
		if (Merge20MatchFunctionName(line.oldText, fn) || Merge20MatchFunctionName(line.newText, fn))
			curFn = fn;
		if (line.op == OP_NONE || line.op == OP_TRIVIAL)
			continue;
		if (curFn != _T("-") && result.functions.size() < 5 &&
			std::find(result.functions.begin(), result.functions.end(), curFn) == result.functions.end())
		{
			result.functions.push_back(curFn);
		}
		if (!seenRounding && (std::regex_search(line.oldText, RoundingRegex()) || std::regex_search(line.newText, RoundingRegex())))
		{
			seenRounding = true;
			result.hasRoundingChange = true;
			result.insights.push_back(Merge20Insight::ROUNDING);
		}
		if (!seenSql && (std::regex_search(line.oldText, SqlRegex()) || std::regex_search(line.newText, SqlRegex())))
		{
			seenSql = true;
			result.hasSqlChange = true;
			result.insights.push_back(Merge20Insight::SQL);
		}
		if (!seenIfDel && line.op == OP_1STONLY && std::regex_search(line.oldText, IfRegex()))
		{
			seenIfDel = true;
			result.hasIfGuardDeletion = true;
			result.insights.push_back(Merge20Insight::IF_GUARD);
		}
		if (!seenOp && OperatorsOf(line.oldText) != OperatorsOf(line.newText))
		{
			seenOp = true;
			result.hasConditionChange = true;
			result.insights.push_back(Merge20Insight::CONDITION);
		}
		if (!seenNum)
		{
			const std::vector<String> oldNums = NumbersOf(line.oldText);
			const std::vector<String> newNums = NumbersOf(line.newText);
			if (strutils::join(oldNums.begin(), oldNums.end(), String(_T(","))) !=
				strutils::join(newNums.begin(), newNums.end(), String(_T(","))))
			{
				seenNum = true;
				result.hasConstantChange = true;
				result.insights.push_back(Merge20Insight::CONSTANT);
				const auto a = std::find_if(oldNums.begin(), oldNums.end(),
					[&newNums](const String& n) { return std::find(newNums.begin(), newNums.end(), n) == newNums.end(); });
				const auto b = std::find_if(newNums.begin(), newNums.end(),
					[&oldNums](const String& n) { return std::find(oldNums.begin(), oldNums.end(), n) == oldNums.end(); });
				if (a != oldNums.end() && b != newNums.end())
				{
					result.constantFrom = *a;
					result.constantTo = *b;
				}
			}
		}
	}
	return result;
}
