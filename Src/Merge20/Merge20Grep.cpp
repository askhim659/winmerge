// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Grep.cpp
 *
 * @brief Implementation of the Grep helpers (Merge20 Phase 4 D7).
 * Pure module: no MFC, no `_()`, ASCII only.
 */
#include "pch.h"
#include <cwctype>
#include "Merge20/Merge20Grep.h"

namespace
{
/** @brief Trim leading/trailing spaces and tabs (no locale involved). */
String TrimGrepLine(const String& s)
{
	const size_t first = s.find_first_not_of(_T(" \t"));
	if (first == String::npos)
		return String();
	const size_t last = s.find_last_not_of(_T(" \t"));
	return s.substr(first, last - first + 1);
}
}

int Merge20GrepFindInLine(const String& line, const String& queryLower)
{
	if (queryLower.empty() || line.size() < queryLower.size())
		return -1;
	// No lowered copy of the line: compare character by character
	// with towlower (Merge20Classify.cpp precedent; the identity
	// for Japanese, so only ASCII case folds in practice).
	const size_t last = line.size() - queryLower.size();
	for (size_t i = 0; i <= last; ++i)
	{
		size_t j = 0;
		for (; j < queryLower.size(); ++j)
		{
			if (std::towlower(line[i + j]) != queryLower[j])
				break;
		}
		if (j == queryLower.size())
			return static_cast<int>(i);
	}
	return -1;
}

String Merge20GrepMakeExcerpt(const String& line, size_t hitPos, size_t queryLen)
{
	String trimmed = TrimGrepLine(line);
	if (trimmed.size() <= Merge20GrepExcerptMax)
		return trimmed;
	// Window the excerpt around the hit: keep about a third of the
	// window before the hit so the match stays readable in context.
	// hitPos is relative to the untrimmed line; clamp it into the
	// trimmed text (leading whitespace only shifts it left).
	const size_t lead = line.find_first_not_of(_T(" \t"));
	const size_t pos = (lead == String::npos) ? 0 : (hitPos > lead ? hitPos - lead : 0);
	const size_t keepBefore = Merge20GrepExcerptMax / 3;
	size_t begin = (pos > keepBefore) ? pos - keepBefore : 0;
	// Do not cut the match itself off at the right edge.
	if (begin + Merge20GrepExcerptMax < pos + queryLen)
		begin = pos + queryLen - Merge20GrepExcerptMax;
	if (begin + Merge20GrepExcerptMax > trimmed.size())
		begin = trimmed.size() - Merge20GrepExcerptMax;
	String excerpt = trimmed.substr(begin, Merge20GrepExcerptMax);
	if (begin > 0)
		excerpt = _T("...") + excerpt;
	if (begin + Merge20GrepExcerptMax < trimmed.size())
		excerpt += _T("...");
	return excerpt;
}

std::vector<String> Merge20GrepParseHistory(const String& stored)
{
	std::vector<String> history;
	size_t pos = 0;
	while (pos <= stored.size())
	{
		const size_t eol = stored.find(_T('\n'), pos);
		const String entry = TrimGrepLine(stored.substr(pos, eol == String::npos ? String::npos : eol - pos));
		if (!entry.empty())
			history.push_back(entry);
		if (eol == String::npos)
			break;
		pos = eol + 1;
	}
	return history;
}

String Merge20GrepFormatHistory(const std::vector<String>& history)
{
	String stored;
	for (size_t i = 0; i < history.size(); ++i)
	{
		if (i > 0)
			stored += _T('\n');
		stored += history[i];
	}
	return stored;
}

std::vector<String> Merge20GrepPushHistory(const std::vector<String>& history, const String& query)
{
	const String trimmed = TrimGrepLine(query);
	if (trimmed.empty())
		return history;
	std::vector<String> updated;
	updated.reserve(history.size() + 1);
	updated.push_back(trimmed);
	for (const String& entry : history)
	{
		if (entry != trimmed)
			updated.push_back(entry);
		if (updated.size() >= static_cast<size_t>(Merge20GrepHistoryMax))
			break;
	}
	return updated;
}

bool Merge20GrepReachedLimit(int hitCount)
{
	return hitCount >= Merge20GrepMaxHits;
}
