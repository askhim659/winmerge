// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20Impact_test.cpp
 *
 * @brief Merge20 (best-workflow B5, PR1) unit tests for the
 * cross-file impact search engine.
 *
 * What is fixed here: the symbol list is exactly what S4 extracts
 * (Merge20ClassifyResult::functions, unchanged); the match is
 * D7's case-insensitive substring search with the D7 excerpt and
 * the shared 80-hit cutoff; the judged file itself is never
 * searched; an empty result means "no use in other files"; every
 * result carries the text-search-estimate flag and the fixed
 * estimate-note msgid. The background worker
 * (Merge20ImpactSearch) adds no matching logic of its own: it
 * feeds file lines to the same collector tested here, so these
 * tests also fix the worker's matching behaviour.
 *
 * This file is ASCII; the pure module is MFC-free, so these tests
 * run in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Classify.h"
#include "Merge20/Merge20Impact.h"

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

	// One diff that changes calcTax, with the enclosing function
	// definition supplied as a context line (S4 convention).
	Merge20ClassifyResult ClassifyCalcTaxDiff()
	{
		const std::vector<Merge20DiffLine> lines = {
			ContextLine(L"    public long calcTax(long base) {"),
			Line(OP_DIFF, L"        long v = calcTax(base);", L"        long v = calcTax(base) * 2;"),
		};
		return Merge20ClassifyDiffLines(lines, {});
	}

} // namespace

TEST(Merge20Impact, SymbolsFromClassifyReturnsS4Functions)
{
	const std::vector<String> symbols = Merge20ImpactSymbolsFromClassify(ClassifyCalcTaxDiff());
	ASSERT_EQ(symbols.size(), static_cast<size_t>(1));
	EXPECT_EQ(symbols[0], String(L"calcTax"));
}

TEST(Merge20Impact, SymbolsFromClassifyEmptyWhenNoFunction)
{
	const std::vector<Merge20DiffLine> lines = {
		Line(OP_DIFF, L"        x = 1;", L"        x = 2;"),
	};
	EXPECT_TRUE(Merge20ImpactSymbolsFromClassify(Merge20ClassifyDiffLines(lines, {})).empty());
}

TEST(Merge20Impact, SymbolsFromClassifyDedupes)
{
	Merge20ClassifyResult result;
	result.functions = { L"calc", L"calc", L"", L"tax" };
	const std::vector<String> symbols = Merge20ImpactSymbolsFromClassify(result);
	ASSERT_EQ(symbols.size(), static_cast<size_t>(2));
	EXPECT_EQ(symbols[0], String(L"calc"));
	EXPECT_EQ(symbols[1], String(L"tax"));
}

TEST(Merge20Impact, IsSelfFileSamePath)
{
	EXPECT_TRUE(Merge20ImpactIsSelfFile(L"src/a.cpp", L"src/a.cpp"));
}

TEST(Merge20Impact, IsSelfFileDifferentPath)
{
	EXPECT_FALSE(Merge20ImpactIsSelfFile(L"src/b.cpp", L"src/a.cpp"));
}

TEST(Merge20Impact, IsSelfFileCaseInsensitive)
{
	EXPECT_TRUE(Merge20ImpactIsSelfFile(L"SRC/A.cpp", L"src/a.cpp"));
}

TEST(Merge20Impact, IsSelfFileEmptyCurrentNeverMatches)
{
	EXPECT_FALSE(Merge20ImpactIsSelfFile(L"src/a.cpp", L""));
}

TEST(Merge20Impact, CollectorFindsUseInOtherFile)
{
	Merge20ImpactCollector collector({ L"calcTax" }, L"src/a.cpp");
	const std::vector<Merge20ImpactHit> added =
		collector.AddLine(L"src/b.cpp", 1, 41, L"    long v = calcTax(base);");
	ASSERT_EQ(added.size(), static_cast<size_t>(1));
	EXPECT_EQ(added[0].symbol, String(L"calcTax"));
	EXPECT_EQ(added[0].relPath, String(L"src/b.cpp"));
	EXPECT_EQ(added[0].pane, 1);
	EXPECT_EQ(added[0].line, 41);
	EXPECT_EQ(added[0].excerpt, String(L"long v = calcTax(base);"));
	ASSERT_EQ(collector.GetHitCount(), static_cast<size_t>(1));
}

TEST(Merge20Impact, CollectorSkipsSelfFile)
{
	Merge20ImpactCollector collector({ L"calcTax" }, L"src/a.cpp");
	EXPECT_TRUE(collector.AddLine(L"src/a.cpp", 0, 10, L"    long v = calcTax(base);").empty());
	EXPECT_TRUE(collector.AddLine(L"src/a.cpp", 1, 10, L"    long v = calcTax(base);").empty());
	EXPECT_TRUE(collector.IsEmpty());
}

TEST(Merge20Impact, CollectorMatchIsCaseInsensitive)
{
	Merge20ImpactCollector collector({ L"calcTax" }, L"src/a.cpp");
	const std::vector<Merge20ImpactHit> added =
		collector.AddLine(L"src/b.cpp", 0, 0, L"    long v = CALCTAX(base);");
	ASSERT_EQ(added.size(), static_cast<size_t>(1));
	// The hit keeps the S4 spelling of the symbol, not the line's.
	EXPECT_EQ(added[0].symbol, String(L"calcTax"));
}

TEST(Merge20Impact, CollectorOneHitPerSymbolPerLine)
{
	Merge20ImpactCollector collector({ L"calcTax", L"computeTax" }, L"src/a.cpp");
	const std::vector<Merge20ImpactHit> added =
		collector.AddLine(L"src/b.cpp", 0, 7, L"    total = calcTax(x) + computeTax(y);");
	ASSERT_EQ(added.size(), static_cast<size_t>(2));
	EXPECT_EQ(added[0].symbol, String(L"calcTax"));
	EXPECT_EQ(added[1].symbol, String(L"computeTax"));
	EXPECT_EQ(added[0].line, added[1].line);
}

TEST(Merge20Impact, CollectorNoHitWhenSymbolAbsent)
{
	Merge20ImpactCollector collector({ L"calcTax" }, L"src/a.cpp");
	EXPECT_TRUE(collector.AddLine(L"src/b.cpp", 0, 0, L"    long v = other(base);").empty());
	EXPECT_TRUE(collector.IsEmpty());
	const Merge20ImpactResult result = collector.TakeResult(5, 9);
	EXPECT_TRUE(result.hits.empty());
	EXPECT_FALSE(result.limitReached);
	// Empty hits with symbols present is the "no use in other
	// files" result (PR2 shows it explicitly).
	ASSERT_EQ(result.symbols.size(), static_cast<size_t>(1));
	EXPECT_EQ(result.symbols[0], String(L"calcTax"));
}

TEST(Merge20Impact, CollectorDedupesSymbols)
{
	Merge20ImpactCollector collector({ L"calc", L"calc", L"" }, L"");
	ASSERT_EQ(collector.GetSymbols().size(), static_cast<size_t>(1));
	EXPECT_EQ(collector.GetSymbols()[0], String(L"calc"));
}

TEST(Merge20Impact, CollectorEmptySymbolsAddsNothing)
{
	Merge20ImpactCollector collector({}, L"");
	EXPECT_TRUE(collector.AddLine(L"src/b.cpp", 0, 0, L"    long v = calcTax(base);").empty());
	const Merge20ImpactResult result = collector.TakeResult(3, 3);
	EXPECT_TRUE(result.symbols.empty());
	EXPECT_TRUE(result.hits.empty());
}

TEST(Merge20Impact, CollectorLimitAtEighty)
{
	Merge20ImpactCollector collector({ L"calcTax" }, L"src/a.cpp");
	for (int i = 0; i < 100; ++i)
		collector.AddLine(L"src/b.cpp", 0, i, L"    calcTax(x);");
	ASSERT_EQ(collector.GetHitCount(), Merge20ImpactMaxHits);
	EXPECT_TRUE(collector.IsFull());
	EXPECT_TRUE(Merge20ImpactReachedLimit(collector.GetHitCount()));
	const Merge20ImpactResult result = collector.TakeResult(2, 2);
	EXPECT_TRUE(result.limitReached);
	EXPECT_EQ(result.hits.size(), Merge20ImpactMaxHits);
	// The cutoff keeps the earliest hits (folder-list, then line order).
	EXPECT_EQ(result.hits.front().line, 0);
	EXPECT_EQ(result.hits.back().line, 79);
}

TEST(Merge20Impact, CollectorBelowLimitNotReached)
{
	Merge20ImpactCollector collector({ L"calcTax" }, L"src/a.cpp");
	collector.AddLine(L"src/b.cpp", 0, 0, L"    calcTax(x);");
	EXPECT_FALSE(collector.IsFull());
	EXPECT_FALSE(Merge20ImpactReachedLimit(collector.GetHitCount()));
	EXPECT_FALSE(collector.TakeResult(1, 1).limitReached);
}

TEST(Merge20Impact, ReachedLimitSharesD7Cutoff)
{
	EXPECT_FALSE(Merge20ImpactReachedLimit(0));
	EXPECT_FALSE(Merge20ImpactReachedLimit(79));
	EXPECT_TRUE(Merge20ImpactReachedLimit(80));
	EXPECT_TRUE(Merge20ImpactReachedLimit(81));
	EXPECT_EQ(Merge20ImpactMaxHits, static_cast<size_t>(Merge20GrepMaxHits));
}

TEST(Merge20Impact, TakeResultCarriesEstimateFlagAndCounts)
{
	Merge20ImpactCollector collector({ L"calcTax" }, L"src/a.cpp");
	collector.AddLine(L"src/b.cpp", 1, 3, L"    calcTax(x);");
	const Merge20ImpactResult result = collector.TakeResult(4, 7);
	EXPECT_TRUE(result.isTextSearchEstimate);
	EXPECT_EQ(result.filesScanned, 4);
	EXPECT_EQ(result.filesTotal, 7);
	ASSERT_EQ(result.hits.size(), static_cast<size_t>(1));
	EXPECT_EQ(result.hits[0].relPath, String(L"src/b.cpp"));
}

TEST(Merge20Impact, EstimateNoteMsgIdIsFixed)
{
	// The note wording is decided in PR1: the display renders this
	// msgid through _(), and the .po msgstr is the fixed Japanese
	// text in Merge20Impact.h. The constant must stay non-empty so
	// PR2 cannot silently drop the note.
	const std::wstring note(Merge20ImpactEstimateNoteMsgId);
	EXPECT_FALSE(note.empty());
	EXPECT_NE(note.find(L"text search"), std::wstring::npos);
	EXPECT_NE(note.find(L"reference analysis"), std::wstring::npos);
}
