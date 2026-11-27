// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Impact.cpp
 *
 * @brief Implementation of the cross-file impact search engine
 * (Merge20 best-workflow B5, PR1).
 *
 * The matching itself is D7's: Merge20GrepFindInLine (case-
 * insensitive substring) and Merge20GrepMakeExcerpt (trimmed
 * 120-character window). The only B5 logic on top is the symbol
 * list (S4's functions, unchanged), the self-file exclusion (the
 * judged file is not another file) and the per-(line, symbol)
 * hit fan-out with the shared 80-hit cutoff.
 *
 * This file stays ASCII: no Japanese literal is needed here (the
 * estimate-note wording is a msgid constant in the header; the
 * Japanese rendering lives in the .po file).
 */
#include "pch.h"
#include <algorithm>
#include <cwctype>
#include "Merge20/Merge20Impact.h"

namespace
{

/** @brief Lower-case one symbol for the D7 matcher (towlower is the identity for Japanese, as in Merge20Grep). */
String LowerSymbol(const String& symbol)
{
	String lowered = symbol;
	for (size_t i = 0; i < lowered.size(); ++i)
		lowered[i] = std::towlower(lowered[i]);
	return lowered;
}

/** @brief Case-insensitive path compare (Windows paths). */
bool PathsEqualIgnoreCase(const String& a, const String& b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (std::towlower(a[i]) != std::towlower(b[i]))
			return false;
	}
	return true;
}

/** @brief Drop empty entries and exact duplicates, keeping first-seen order (S4 order). */
std::vector<String> DedupeSymbols(const std::vector<String>& symbols)
{
	std::vector<String> deduped;
	for (const String& symbol : symbols)
	{
		if (symbol.empty())
			continue;
		if (std::find(deduped.begin(), deduped.end(), symbol) == deduped.end())
			deduped.push_back(symbol);
	}
	return deduped;
}

} // namespace

std::vector<String> Merge20ImpactSymbolsFromClassify(const Merge20ClassifyResult& result)
{
	return DedupeSymbols(result.functions);
}

bool Merge20ImpactIsSelfFile(const String& relPath, const String& currentRelPath)
{
	if (currentRelPath.empty())
		return false;
	return PathsEqualIgnoreCase(relPath, currentRelPath);
}

bool Merge20ImpactReachedLimit(size_t hitCount)
{
	return Merge20GrepReachedLimit(static_cast<int>(hitCount));
}

Merge20ImpactCollector::Merge20ImpactCollector(std::vector<String> symbols, String currentRelPath)
	: m_symbols(DedupeSymbols(symbols))
	, m_currentRelPath(std::move(currentRelPath))
{
	m_symbolsLower.reserve(m_symbols.size());
	for (const String& symbol : m_symbols)
		m_symbolsLower.push_back(LowerSymbol(symbol));
}

std::vector<Merge20ImpactHit> Merge20ImpactCollector::AddLine(const String& relPath, int pane, int line, const String& text)
{
	std::vector<Merge20ImpactHit> added;
	if (m_symbols.empty() || IsFull())
		return added;
	// The judged file itself is not another file: its lines are
	// the diff, not an impact on it.
	if (Merge20ImpactIsSelfFile(relPath, m_currentRelPath))
		return added;
	for (size_t i = 0; i < m_symbols.size(); ++i)
	{
		if (IsFull())
			break;
		const int pos = Merge20GrepFindInLine(text, m_symbolsLower[i]);
		if (pos < 0)
			continue;
		Merge20ImpactHit hit;
		hit.symbol = m_symbols[i];
		hit.relPath = relPath;
		hit.pane = pane;
		hit.line = line;
		hit.excerpt = Merge20GrepMakeExcerpt(text, static_cast<size_t>(pos), m_symbols[i].size());
		m_hits.push_back(hit);
		added.push_back(hit);
	}
	return added;
}

Merge20ImpactResult Merge20ImpactCollector::TakeResult(int filesScanned, int filesTotal) const
{
	Merge20ImpactResult result;
	result.symbols = m_symbols;
	result.hits = m_hits;
	result.limitReached = IsFull();
	result.isTextSearchEstimate = true;
	result.filesScanned = filesScanned;
	result.filesTotal = filesTotal;
	return result;
}
