/**
 * @file Merge20Learn_test.cpp
 *
 * @brief Merge20 (Phase 3 L0) unit tests for the learning core.
 *
 * The expected values are the ground truth produced by running the
 * HTML tool's own learning functions (hunkKeyExact / hunkFeatKey /
 * matchLearn / learnDominant / learnAdd / removeLearnSource /
 * mergeLearnSources / importLearn / hashStr from 17_v17.15.html) in
 * Node. The pure module is MFC-free, so these tests run in the
 * UnitTests runner on CI.
 *
 * This file is ASCII; Japanese text is written with \\u escapes, like
 * Merge20Classify_test.cpp.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Learn.h"

namespace
{

	using Strat = Merge20LearnStrategy;

	Merge20DiffLine Line(OP_TYPE op, const wchar_t* oldText, const wchar_t* newText)
	{
		Merge20DiffLine line;
		line.op = op;
		line.oldText = oldText != nullptr ? oldText : L"";
		line.newText = newText != nullptr ? newText : L"";
		return line;
	}

	// The two mod rows used by the HTML ground truth (exact key,
	// correction and export fixtures).
	std::vector<Merge20DiffLine> ModRows()
	{
		return {
			Line(OP_DIFF, L"  int   x  =  1;  ", L"int x = 2;"),
			Line(OP_DIFF, L"\treturn x;", L"return  x ;"),
		};
	}

	String ModExactKey()
	{
		return L"EXACT|int x = 1;\x21c4" L"int x = 2;\nreturn x;\x21c4" L"return x ;";
	}

	String SqlFeatKey()
	{
		return L"FEAT|mod|sql";
	}

	std::vector<Merge20DiffLine> SqlRows()
	{
		return { Line(OP_DIFF, L"SELECT a FROM t", L"SELECT b FROM t") };
	}

	// Exact HTML export JSON (17_v17.15.html exportLearn shape), built
	// with the HTML tool's own functions in Node and pasted here.
	// app is the HTML tool's, sourceId is the exporter's owner id.
	std::string HtmlExportJsonUtf8()
	{
		String wide =
			L"{"
			L"\"version\":2,"
			L"\"sourceId\":\"u-11111111-2222-4333-8444-555555555555\","
			L"\"sourceName\":\"\x30de\x30fc\x30b8\x652f\x63f4" L"20" L"\x53f7_\x5b66\x7fd2_2026-11-25.json\","
			L"\"app\":\"\x30de\x30fc\x30b8\x652f\x63f4" L"20" L"\x53f7\","
			L"\"exportedAt\":\"2026-11-25T01:30:00Z\","
			L"\"rules\":[{"
			L"\"key\":\"EXACT|int x = 1;\x21c4" L"int x = 2;\\nreturn x;\x21c4" L"return x ;\","
			L"\"counts\":{\"REPLACE_WITH_OLD\":2,\"KEEP_NEW\":1},"
			L"\"n\":3,"
			L"\"ex\":\"src/Foo.java\","
			L"\"last\":\"2026-11-25\","
			L"\"sources\":{"
			L"\"u-11111111-2222-4333-8444-555555555555\":"
			L"{\"name\":\"owner.json\",\"counts\":{\"REPLACE_WITH_OLD\":2},\"n\":2},"
			L"\"u-99999999-8888-4777-8666-555555555555\":"
			L"{\"name\":\"other.json\",\"counts\":{\"KEEP_NEW\":1},\"n\":1}"
			L"},"
			L"\"lastStrat\":\"REPLACE_WITH_OLD\""
			L"}]}";
		return Merge20Utf8Encode(wide);
	}

	int CountOf(const Merge20LearnRule& rule, Strat strat)
	{
		auto it = rule.counts.find(strat);
		return it == rule.counts.end() ? 0 : it->second;
	}

	int SourceCountOf(const Merge20LearnRule& rule, const String& srcId, Strat strat)
	{
		auto sit = rule.sources.find(srcId);
		if (sit == rule.sources.end())
			return 0;
		auto it = sit->second.counts.find(strat);
		return it == sit->second.counts.end() ? 0 : it->second;
	}

} // namespace

TEST(Merge20Learn, StrategyNames)
{
	EXPECT_EQ(Merge20LearnStrategyName(Strat::InsertOld), String(L"INSERT_OLD"));
	EXPECT_EQ(Merge20LearnStrategyName(Strat::KeepNew), String(L"KEEP_NEW"));
	EXPECT_EQ(Merge20LearnStrategyName(Strat::ReplaceWithOld), String(L"REPLACE_WITH_OLD"));
	EXPECT_EQ(Merge20LearnStrategyName(Strat::BothOldNew), String(L"BOTH_OLD_NEW"));
	Strat strat;
	EXPECT_TRUE(Merge20LearnStrategyFromName(L"INSERT_OLD", strat));
	EXPECT_EQ(strat, Strat::InsertOld);
	EXPECT_TRUE(Merge20LearnStrategyFromName(L"BOTH_OLD_NEW", strat));
	EXPECT_EQ(strat, Strat::BothOldNew);
	EXPECT_FALSE(Merge20LearnStrategyFromName(L"UNKNOWN", strat));
	EXPECT_FALSE(Merge20LearnStrategyFromName(L"", strat));
}

TEST(Merge20Learn, HashStr)
{
	// Node ground truth (HTML hashStr).
	EXPECT_EQ(Merge20LearnHashStr(L""), String(L"488bdcb81aee8d83"));
	EXPECT_EQ(Merge20LearnHashStr(L"a.json"), String(L"10b90a9f37fc5091"));
	EXPECT_EQ(Merge20LearnHashStr(L"src/Foo.java"), String(L"467cbacb771d4d4e"));
	EXPECT_EQ(Merge20LearnHashStr(L"WinMerge-x64.zip"), String(L"c79ba81867cd6056"));
	EXPECT_EQ(Merge20LearnHashStr(
		L"\x30de\x30fc\x30b8\x652f\x63f4" L"20" L"\x53f7_\x5b66\x7fd2_2026-11-20.json"),
		String(L"d57fb621e1a8edb3"));
	EXPECT_EQ(Merge20LearnHashStr(L"old-learn.json"), String(L"a7884772efc7dec9"));
}

TEST(Merge20Learn, ExactKey)
{
	// Node ground truth (HTML hunkKeyExact).
	EXPECT_EQ(Merge20LearnExactKey(ModRows()), ModExactKey());
	EXPECT_EQ(Merge20LearnExactKey(
		{ Line(OP_1STONLY, L"  if (a) { b(); } ", nullptr) }),
		String(L"EXACT|if (a) { b(); }\x21c4"));
	EXPECT_EQ(Merge20LearnExactKey(
		{ Line(OP_2NDONLY, nullptr, L"  new line; ") }),
		String(L"EXACT|\x21c4" L"new line;"));
	EXPECT_EQ(Merge20LearnExactKey({ Line(OP_DIFF, L"", L"") }),
		String(L"EXACT|\x21c4"));
	// Tabs / newlines inside a line collapse to one space, like JS \s+.
	EXPECT_EQ(Merge20LearnExactKey(
		{ Line(OP_DIFF, L"a\t\tb\nc", L"d") }),
		String(L"EXACT|a b c\x21c4" L"d"));
	// OP_NONE (context) lines are not part of the diff.
	EXPECT_EQ(Merge20LearnExactKey({
			Line(OP_NONE, L"public void f() {", L"public void f() {"),
			Line(OP_DIFF, L"int a = 1;", L"int a = 2;"),
		}),
		String(L"EXACT|int a = 1;\x21c4" L"int a = 2;"));
}

TEST(Merge20Learn, FeatureKey)
{
	// Node ground truth (HTML hunkFeatKey). Each case is one feature.
	auto feat = [](std::vector<Merge20DiffLine> lines)
	{
		String key;
		if (!Merge20LearnFeatureKey(lines, key))
			return String();
		return key;
	};
	EXPECT_EQ(feat({ Line(OP_DIFF, L"x = Math.round(v);", L"x = Math.floor(v);") }),
		String(L"FEAT|mod|round"));
	EXPECT_EQ(feat({ Line(OP_DIFF, L"new BigDecimal(a)", L"new BigDecimal(b)") }),
		String(L"FEAT|mod|round"));
	// Round is case-sensitive in the HTML tool (no i flag).
	EXPECT_EQ(feat({ Line(OP_DIFF, L"x = math.round(v);", L"x = v;") }), String());
	// ifdel is tested on the old side only, case-sensitive.
	EXPECT_EQ(feat({ Line(OP_1STONLY, L"if (ratio < 0.20) { ratio = 0.20; }", nullptr) }),
		String(L"FEAT|del|ifdel"));
	EXPECT_EQ(feat({ Line(OP_2NDONLY, nullptr, L"if (x) { y(); }") }), String());
	EXPECT_EQ(feat({ Line(OP_1STONLY, L"IF (x) { y(); }", nullptr) }), String());
	// SQL is case-insensitive but has no `join` (unlike the S4 classifier).
	EXPECT_EQ(feat({ Line(OP_DIFF, L"SELECT a FROM t WHERE x=1", L"SELECT a FROM t WHERE x=2") }),
		String(L"FEAT|mod|sql"));
	EXPECT_EQ(feat({ Line(OP_DIFF, L"a JOIN b ON a.id=b.id", L"a JOIN b ON a.id=b.k") }), String());
	EXPECT_EQ(feat({ Line(OP_1STONLY, L"SELECT a FROM t", nullptr) }),
		String(L"FEAT|del|sql"));
	EXPECT_EQ(feat({ Line(OP_2NDONLY, nullptr, L"SELECT a FROM t") }),
		String(L"FEAT|add|sql"));
	// Priority: round > ifdel > sql > tax > comment.
	EXPECT_EQ(feat({ Line(OP_DIFF, L"SELECT Math.round(v) FROM t", L"SELECT Math.ceil(v) FROM t") }),
		String(L"FEAT|mod|round"));
	EXPECT_EQ(feat({ Line(OP_DIFF, L"if (x) { selectA(); }", L"select b from t") }),
		String(L"FEAT|mod|ifdel"));
	EXPECT_EQ(feat({ Line(OP_DIFF, L"SELECT \x7a0e\x7387 FROM t", L"SELECT \x7a0e\x7387 FROM t WHERE x=1") }),
		String(L"FEAT|mod|sql"));
	EXPECT_EQ(feat({ Line(OP_DIFF, L"\x7a0e\x7387 = 0.10;", L"\x7a0e\x7387 = 0.08;") }),
		String(L"FEAT|mod|tax"));
	EXPECT_EQ(feat({ Line(OP_DIFF, L"\x91d1\x984d = 100;", L"\x91d1\x984d = 200;") }),
		String(L"FEAT|mod|tax"));
	// Comment is tested on the old side only.
	EXPECT_EQ(feat({ Line(OP_1STONLY, L"  // \x65e7\x72ec\x81ea\x51e6\x7406", nullptr) }),
		String(L"FEAT|del|comment"));
	EXPECT_EQ(feat({ Line(OP_1STONLY, L"/* \x65e7\x30b3\x30e1\x30f3\x30c8 */", nullptr) }),
		String(L"FEAT|del|comment"));
	EXPECT_EQ(feat({ Line(OP_2NDONLY, nullptr, L"// \x65b0\x30b3\x30e1\x30f3\x30c8") }), String());
	EXPECT_EQ(feat({ Line(OP_DIFF, L"int a = 1;", L"int a = 2;") }), String());
}

TEST(Merge20Learn, MatchThresholds)
{
	Merge20LearnStore store;
	Merge20LearnCandidate out;
	// Exact match: 1 record is enough.
	store.Add(ModExactKey(), Strat::ReplaceWithOld, L"self", L"x", 1);
	ASSERT_TRUE(store.Match(ModRows(), out));
	EXPECT_EQ(out.strategy, Strat::ReplaceWithOld);
	EXPECT_EQ(out.via, String(L"exact"));
	EXPECT_EQ(out.n, 1);
	// Feature match: 2 records are not enough.
	store.Clear();
	store.Add(SqlFeatKey(), Strat::KeepNew, L"self", L"x", 2);
	EXPECT_FALSE(store.Match(SqlRows(), out));
	// 2 of 3 (66%) is below 80%.
	store.Add(SqlFeatKey(), Strat::ReplaceWithOld, L"self", L"x", 1);
	EXPECT_FALSE(store.Match(SqlRows(), out));
	// 3 of 4 (75%) is below 80%.
	store.Add(SqlFeatKey(), Strat::KeepNew, L"self", L"x", 1);
	EXPECT_FALSE(store.Match(SqlRows(), out));
	// 4 of 5 (80%) is enough; n is the total record count.
	store.Add(SqlFeatKey(), Strat::KeepNew, L"self", L"x", 1);
	ASSERT_TRUE(store.Match(SqlRows(), out));
	EXPECT_EQ(out.strategy, Strat::KeepNew);
	EXPECT_EQ(out.via, String(L"feat"));
	EXPECT_EQ(out.n, 5);
	// 8 of 10 (80%) is enough; 7 of 9 (77%) is not.
	store.Clear();
	store.Add(SqlFeatKey(), Strat::KeepNew, L"self", L"x", 8);
	store.Add(SqlFeatKey(), Strat::ReplaceWithOld, L"self", L"x", 2);
	ASSERT_TRUE(store.Match(SqlRows(), out));
	EXPECT_EQ(out.n, 10);
	store.Clear();
	store.Add(SqlFeatKey(), Strat::KeepNew, L"self", L"x", 7);
	store.Add(SqlFeatKey(), Strat::ReplaceWithOld, L"self", L"x", 2);
	EXPECT_FALSE(store.Match(SqlRows(), out));
	// 3 of 3 (100%) is enough.
	store.Clear();
	store.Add(SqlFeatKey(), Strat::KeepNew, L"self", L"x", 3);
	ASSERT_TRUE(store.Match(SqlRows(), out));
	EXPECT_EQ(out.n, 3);
	// Exact match beats feature match (HTML matchLearn checks exact first).
	store.Add(ModExactKey(), Strat::BothOldNew, L"self", L"x", 1);
	store.Add(SqlFeatKey(), Strat::KeepNew, L"self", L"x", 5);
	ASSERT_TRUE(store.Match(SqlRows(), out));
	// SqlRows has no exact rule, so the feature candidate is returned here;
	// the exact rule wins for the mod rows that carry both keys.
	EXPECT_EQ(out.strategy, Strat::KeepNew);
	EXPECT_EQ(out.via, String(L"feat"));
	std::vector<Merge20DiffLine> bothRows = {
		Line(OP_DIFF, L"SELECT a FROM t", L"SELECT b FROM t"),
	};
	store.Add(Merge20LearnExactKey(bothRows), Strat::BothOldNew, L"self", L"x", 1);
	ASSERT_TRUE(store.Match(bothRows, out));
	EXPECT_EQ(out.strategy, Strat::BothOldNew);
	EXPECT_EQ(out.via, String(L"exact"));
	EXPECT_EQ(out.n, 1);
	// No rows / context lines only: no candidate.
	EXPECT_FALSE(store.Match({}, out));
	EXPECT_FALSE(store.Match({ Line(OP_NONE, L"int a;", L"int a;") }, out));
}

TEST(Merge20Learn, DominantTie)
{
	// Node ground truth (HTML learnDominant): insertion order decides
	// the tie when no lastStrat is set; lastStrat wins when it holds
	// the maximum, even when it was not inserted first.
	Merge20LearnStore store;
	Merge20LearnCandidate out;
	store.Add(ModExactKey(), Strat::ReplaceWithOld, L"self", L"x", 2);
	store.Add(ModExactKey(), Strat::KeepNew, L"self", L"x", 2);
	ASSERT_TRUE(store.Match(ModRows(), out));
	EXPECT_EQ(out.strategy, Strat::ReplaceWithOld);
	Merge20LearnRuleMeta meta;
	meta.setLastStrat = true;
	meta.lastStrat = Strat::KeepNew;
	store.SetRuleMeta(ModExactKey(), meta);
	ASSERT_TRUE(store.Match(ModRows(), out));
	EXPECT_EQ(out.strategy, Strat::KeepNew);
	meta.lastStrat = Strat::ReplaceWithOld;
	store.SetRuleMeta(ModExactKey(), meta);
	ASSERT_TRUE(store.Match(ModRows(), out));
	EXPECT_EQ(out.strategy, Strat::ReplaceWithOld);
	meta.lastStrat = Strat::BothOldNew; // not among the counts
	store.SetRuleMeta(ModExactKey(), meta);
	ASSERT_TRUE(store.Match(ModRows(), out));
	EXPECT_EQ(out.strategy, Strat::ReplaceWithOld);
}

TEST(Merge20Learn, RecordCorrection)
{
	// Node ground truth (HTML learnFromHunk): correcting a judgment
	// subtracts 1 from the old strategy and adds 1 to the new one;
	// repeating the same judgment only adds 1.
	Merge20LearnStore store;
	const String srcId = L"u-11111111-2222-4333-8444-555555555555";
	// Another source's judgments must survive the correction.
	store.Add(ModExactKey(), Strat::KeepNew, L"u-99999999-8888-4777-8666-555555555555",
		L"other.json", 2);
	Merge20LearnRecordJudgment(store, ModRows(), Strat::ReplaceWithOld, nullptr,
		srcId, L"owner.json", L"src/Foo.java", L"2026-11-25");
	const Merge20LearnRule* rule = store.FindRule(ModExactKey());
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(CountOf(*rule, Strat::ReplaceWithOld), 1);
	EXPECT_EQ(CountOf(*rule, Strat::KeepNew), 2);
	EXPECT_EQ(rule->n, 3);
	EXPECT_TRUE(rule->hasLastStrat);
	EXPECT_EQ(rule->lastStrat, Strat::ReplaceWithOld);
	EXPECT_EQ(rule->ex, String(L"src/Foo.java"));
	EXPECT_EQ(rule->last, String(L"2026-11-25"));
	EXPECT_EQ(SourceCountOf(*rule, L"u-99999999-8888-4777-8666-555555555555", Strat::KeepNew), 2);
	// The feature key (FEAT|mod|sql is not present here: the mod rows
	// have no feature) is not recorded.
	EXPECT_EQ(store.FindRule(SqlFeatKey()), nullptr);
	Strat prev = Strat::ReplaceWithOld;
	Merge20LearnRecordJudgment(store, ModRows(), Strat::KeepNew, &prev,
		srcId, L"owner.json", L"src/Foo.java", L"2026-11-25");
	rule = store.FindRule(ModExactKey());
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(CountOf(*rule, Strat::ReplaceWithOld), 0);
	EXPECT_EQ(CountOf(*rule, Strat::KeepNew), 3); // 2 other + 1 own
	EXPECT_EQ(SourceCountOf(*rule, srcId, Strat::KeepNew), 1);
	EXPECT_EQ(rule->lastStrat, Strat::KeepNew);
	prev = Strat::KeepNew;
	Merge20LearnRecordJudgment(store, ModRows(), Strat::KeepNew, &prev,
		srcId, L"owner.json", L"src/Foo.java", L"2026-11-25");
	rule = store.FindRule(ModExactKey());
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(CountOf(*rule, Strat::KeepNew), 4);
	EXPECT_EQ(SourceCountOf(*rule, srcId, Strat::KeepNew), 2);
	// Recording a diff with only context lines records nothing.
	Merge20LearnRecordJudgment(store,
		{ Line(OP_NONE, L"int a;", L"int a;") },
		Strat::KeepNew, nullptr, srcId, L"owner.json", L"src/Foo.java", L"2026-11-25");
	EXPECT_EQ(store.RuleCount(), static_cast<size_t>(1));
	// A diff with a feature records both keys (HTML learnFromHunk).
	Merge20LearnRecordJudgment(store, SqlRows(), Strat::KeepNew, nullptr,
		srcId, L"owner.json", L"src/Q.java", L"2026-11-25");
	const Merge20LearnRule* feat = store.FindRule(SqlFeatKey());
	ASSERT_NE(feat, nullptr);
	EXPECT_EQ(CountOf(*feat, Strat::KeepNew), 1);
	EXPECT_EQ(feat->ex, String(L"src/Q.java"));
}

TEST(Merge20Learn, RemoveSource)
{
	// Node ground truth (HTML removeLearnSource).
	Merge20LearnStore store;
	store.Add(L"EXACT|k1", Strat::ReplaceWithOld, L"u-aaa", L"A.json", 3);
	store.Add(L"EXACT|k1", Strat::KeepNew, L"u-bbb", L"B.json", 1);
	store.Add(L"EXACT|k2", Strat::KeepNew, L"u-bbb", L"B.json", 2);
	store.RemoveSource(L"u-aaa");
	const Merge20LearnRule* k1 = store.FindRule(L"EXACT|k1");
	ASSERT_NE(k1, nullptr);
	EXPECT_EQ(CountOf(*k1, Strat::ReplaceWithOld), 0);
	EXPECT_EQ(CountOf(*k1, Strat::KeepNew), 1);
	EXPECT_EQ(k1->n, 1);
	EXPECT_EQ(k1->sources.count(L"u-aaa"), static_cast<size_t>(0));
	ASSERT_NE(store.FindRule(L"EXACT|k2"), nullptr);
	std::vector<Merge20LearnSourceSummary> sources = store.GetSources();
	ASSERT_EQ(sources.size(), static_cast<size_t>(1));
	EXPECT_EQ(sources[0].id, String(L"u-bbb"));
	EXPECT_EQ(sources[0].rules, 2);
	EXPECT_EQ(sources[0].n, 3);
	store.RemoveSource(L"u-bbb");
	EXPECT_EQ(store.RuleCount(), static_cast<size_t>(0));
	EXPECT_TRUE(store.GetSources().empty());
	// Removing an unknown source is a no-op.
	store.Add(L"EXACT|k1", Strat::KeepNew, L"u-bbb", L"B.json", 1);
	store.RemoveSource(L"u-zzz");
	EXPECT_EQ(store.RuleCount(), static_cast<size_t>(1));
}

TEST(Merge20Learn, GetSources)
{
	Merge20LearnStore store;
	store.Add(L"EXACT|k1", Strat::ReplaceWithOld, L"u-aaa", L"A.json", 3);
	store.Add(L"EXACT|k1", Strat::KeepNew, L"u-bbb", L"B.json", 1);
	store.Add(L"EXACT|k2", Strat::KeepNew, L"u-bbb", L"B.json", 2);
	store.Add(L"EXACT|k3", Strat::BothOldNew, L"u-ccc", L"C.json", 5);
	std::vector<Merge20LearnSourceSummary> sources = store.GetSources();
	ASSERT_EQ(sources.size(), static_cast<size_t>(3));
	EXPECT_EQ(sources[0].id, String(L"u-ccc")); // most judgments first
	EXPECT_EQ(sources[0].n, 5);
	EXPECT_EQ(sources[1].id, String(L"u-aaa"));
	EXPECT_EQ(sources[1].n, 3);
	EXPECT_EQ(sources[2].id, String(L"u-bbb"));
	EXPECT_EQ(sources[2].n, 3);
	EXPECT_EQ(sources[2].rules, 2);
	EXPECT_EQ(store.TotalJudgments(), 11);
}

TEST(Merge20Learn, MergeSources)
{
	// Node ground truth (HTML mergeLearnSources).
	Merge20LearnStore store;
	store.Add(L"EXACT|k1", Strat::ReplaceWithOld, L"u-aaa", L"A.json", 3);
	store.Add(L"EXACT|k1", Strat::KeepNew, L"u-bbb", L"B.json", 1);
	store.Add(L"EXACT|k2", Strat::KeepNew, L"u-bbb", L"B.json", 2);
	store.Add(L"EXACT|k3", Strat::BothOldNew, L"u-ccc", L"C.json", 5);
	std::vector<Merge20LearnRule> aOnly = store.MergeSources({ L"u-aaa" });
	ASSERT_EQ(aOnly.size(), static_cast<size_t>(1));
	EXPECT_EQ(aOnly[0].key, String(L"EXACT|k1"));
	EXPECT_EQ(CountOf(aOnly[0], Strat::ReplaceWithOld), 3);
	EXPECT_EQ(aOnly[0].n, 3);
	EXPECT_TRUE(aOnly[0].sources.empty());
	std::vector<Merge20LearnRule> ab = store.MergeSources({ L"u-aaa", L"u-bbb" });
	ASSERT_EQ(ab.size(), static_cast<size_t>(2));
	EXPECT_EQ(CountOf(ab[0], Strat::ReplaceWithOld), 3);
	EXPECT_EQ(CountOf(ab[0], Strat::KeepNew), 1);
	EXPECT_EQ(ab[0].n, 4);
	EXPECT_EQ(CountOf(ab[1], Strat::KeepNew), 2);
	EXPECT_TRUE(store.MergeSources({}).empty());
	EXPECT_TRUE(store.MergeSources({ L"u-zzz" }).empty());
}

TEST(Merge20Learn, SourceDisplayName)
{
	EXPECT_EQ(Merge20LearnSourceDisplayName(L"self", String()),
		String(L"\x3053\x306e\x30d6\x30e9\x30a6\x30b6\x3067\x5b66\x7fd2\xff08\x81ea\x5206\xff09"));
	EXPECT_EQ(Merge20LearnSourceDisplayName(L"legacy", String()),
		String(L"\x4ee5\x524d\x306e\x30c7\x30fc\x30bf\xff08\x51fa\x5178\x4e0d\x660e\xff09"));
	EXPECT_EQ(Merge20LearnSourceDisplayName(L"u-aaa", L"A.json"), String(L"A.json"));
	EXPECT_EQ(Merge20LearnSourceDisplayName(L"u-aaa", String()), String(L"u-aaa"));
}

TEST(Merge20Learn, JsonRoundTrip)
{
	Merge20LearnStore store(L"u-11111111-2222-4333-8444-555555555555");
	store.Add(ModExactKey(), Strat::ReplaceWithOld,
		L"u-11111111-2222-4333-8444-555555555555", L"owner.json", 2);
	store.Add(ModExactKey(), Strat::KeepNew,
		L"u-99999999-8888-4777-8666-555555555555", L"other.json", 1);
	Merge20LearnRuleMeta meta;
	meta.setEx = true;
	meta.ex = L"src/Foo.java";
	meta.setLast = true;
	meta.last = L"2026-11-25";
	meta.setLastStrat = true;
	meta.lastStrat = Strat::ReplaceWithOld;
	store.SetRuleMeta(ModExactKey(), meta);
	std::string json;
	ASSERT_TRUE(Merge20LearnToJson(store,
		L"u-11111111-2222-4333-8444-555555555555",
		L"\x30de\x30fc\x30b8\x652f\x63f4" L"20" L"\x53f7_\x5b66\x7fd2_2026-11-25.json",
		L"2026-11-25T01:30:00Z", json));
	EXPECT_NE(json.find("\"app\": \"Merge20Learn\""), std::string::npos);
	EXPECT_NE(json.find("\"version\": 2"), std::string::npos);
	Merge20LearnStore loaded;
	Merge20LearnFileHeader header;
	std::string err;
	int skipped = -1;
	ASSERT_TRUE(Merge20LearnFromJson(json,
		L"\x30de\x30fc\x30b8\x652f\x63f4" L"20" L"\x53f7_\x5b66\x7fd2_2026-11-25.json",
		loaded, header, err, skipped));
	EXPECT_TRUE(err.empty());
	EXPECT_EQ(skipped, 0);
	EXPECT_EQ(header.sourceId, String(L"u-11111111-2222-4333-8444-555555555555"));
	EXPECT_TRUE(header.hasApp);
	EXPECT_TRUE(header.hasVersion);
	ASSERT_EQ(loaded.RuleCount(), static_cast<size_t>(1));
	const Merge20LearnRule* rule = loaded.FindRule(ModExactKey());
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(CountOf(*rule, Strat::ReplaceWithOld), 2);
	EXPECT_EQ(CountOf(*rule, Strat::KeepNew), 1);
	EXPECT_EQ(rule->n, 3);
	EXPECT_EQ(rule->ex, String(L"src/Foo.java"));
	EXPECT_EQ(rule->last, String(L"2026-11-25"));
	EXPECT_TRUE(rule->hasLastStrat);
	EXPECT_EQ(rule->lastStrat, Strat::ReplaceWithOld);
	EXPECT_EQ(SourceCountOf(*rule,
		L"u-11111111-2222-4333-8444-555555555555", Strat::ReplaceWithOld), 2);
	EXPECT_EQ(SourceCountOf(*rule,
		L"u-99999999-8888-4777-8666-555555555555", Strat::KeepNew), 1);
	// The candidate still matches after the round trip.
	Merge20LearnCandidate out;
	ASSERT_TRUE(loaded.Match(ModRows(), out));
	EXPECT_EQ(out.strategy, Strat::ReplaceWithOld);
	EXPECT_EQ(out.via, String(L"exact"));
}

TEST(Merge20Learn, HtmlJsonImport)
{
	// The HTML tool's real export (see HtmlExportJsonUtf8) imports
	// flattened to one source (HTML importLearn does the same).
	Merge20LearnStore store;
	Merge20LearnFileHeader header;
	std::string err;
	int skipped = -1;
	int added = -1;
	int merged = -1;
	ASSERT_TRUE(Merge20LearnImportJson(HtmlExportJsonUtf8(),
		L"\x30de\x30fc\x30b8\x652f\x63f4" L"20" L"\x53f7_\x5b66\x7fd2_2026-11-25.json",
		store, header, err, skipped, added, merged));
	EXPECT_TRUE(err.empty());
	EXPECT_EQ(skipped, 0);
	EXPECT_EQ(added, 1);
	EXPECT_EQ(merged, 0);
	EXPECT_EQ(header.sourceId, String(L"u-11111111-2222-4333-8444-555555555555"));
	const Merge20LearnRule* rule = store.FindRule(ModExactKey());
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(CountOf(*rule, Strat::ReplaceWithOld), 2);
	EXPECT_EQ(CountOf(*rule, Strat::KeepNew), 1);
	EXPECT_EQ(rule->n, 3);
	EXPECT_EQ(rule->ex, String(L"src/Foo.java"));
	EXPECT_EQ(rule->last, String(L"2026-11-25"));
	EXPECT_TRUE(rule->hasLastStrat);
	EXPECT_EQ(rule->lastStrat, Strat::ReplaceWithOld);
	ASSERT_EQ(rule->sources.size(), static_cast<size_t>(1));
	EXPECT_EQ(rule->sources.count(L"u-11111111-2222-4333-8444-555555555555"),
		static_cast<size_t>(1));
	// Re-importing the same file replaces it: no double counting.
	// (The HTML tool counts it as "new" again, because the rule was
	// removed just before the re-import.)
	ASSERT_TRUE(Merge20LearnImportJson(HtmlExportJsonUtf8(),
		L"\x30de\x30fc\x30b8\x652f\x63f4" L"20" L"\x53f7_\x5b66\x7fd2_2026-11-25.json",
		store, header, err, skipped, added, merged));
	EXPECT_EQ(added, 1);
	EXPECT_EQ(merged, 0);
	rule = store.FindRule(ModExactKey());
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(rule->n, 3);
	// FromJson (not import) preserves the per-rule sources.
	Merge20LearnStore loaded;
	ASSERT_TRUE(Merge20LearnFromJson(HtmlExportJsonUtf8(),
		L"\x30de\x30fc\x30b8\x652f\x63f4" L"20" L"\x53f7_\x5b66\x7fd2_2026-11-25.json",
		loaded, header, err, skipped));
	ASSERT_EQ(loaded.RuleCount(), static_cast<size_t>(1));
	rule = loaded.FindRule(ModExactKey());
	ASSERT_NE(rule, nullptr);
	ASSERT_EQ(rule->sources.size(), static_cast<size_t>(2));
	EXPECT_EQ(SourceCountOf(*rule,
		L"u-11111111-2222-4333-8444-555555555555", Strat::ReplaceWithOld), 2);
	EXPECT_EQ(SourceCountOf(*rule,
		L"u-99999999-8888-4777-8666-555555555555", Strat::KeepNew), 1);
	std::vector<Merge20LearnSourceSummary> sources = loaded.GetSources();
	ASSERT_EQ(sources.size(), static_cast<size_t>(2));
	EXPECT_EQ(sources[0].name, String(L"owner.json"));
	EXPECT_EQ(sources[1].name, String(L"other.json"));
}

TEST(Merge20Learn, V1Import)
{
	// v1: no app / version / sourceId, rules without sources.
	std::string json = Merge20Utf8Encode(
		L"{\"rules\":[{\"key\":\"EXACT|if (a) { b(); }\x21c4\","
		L"\"counts\":{\"INSERT_OLD\":2},\"n\":2,"
		L"\"ex\":\"src/Bar.java\",\"last\":\"2026-11-20\"}]}");
	Merge20LearnStore store;
	Merge20LearnFileHeader header;
	std::string err;
	int skipped = -1;
	int added = -1;
	int merged = -1;
	ASSERT_TRUE(Merge20LearnImportJson(json, L"old-learn.json",
		store, header, err, skipped, added, merged));
	EXPECT_TRUE(err.empty());
	EXPECT_EQ(skipped, 0);
	EXPECT_EQ(added, 1);
	EXPECT_EQ(merged, 0);
	// The v1 source id is the file-name hash (HTML importLearn).
	EXPECT_EQ(header.sourceId, String(L"file:a7884772efc7dec9"));
	EXPECT_EQ(header.sourceName, String(L"old-learn.json"));
	EXPECT_FALSE(header.hasApp);
	EXPECT_FALSE(header.hasVersion);
	const Merge20LearnRule* rule = store.FindRule(L"EXACT|if (a) { b(); }\x21c4");
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(CountOf(*rule, Strat::InsertOld), 2);
	EXPECT_EQ(rule->ex, String(L"src/Bar.java"));
	EXPECT_EQ(rule->last, String(L"2026-11-20"));
	ASSERT_EQ(rule->sources.size(), static_cast<size_t>(1));
	EXPECT_EQ(rule->sources.count(L"file:a7884772efc7dec9"), static_cast<size_t>(1));
}

TEST(Merge20Learn, JsonErrors)
{
	Merge20LearnStore store;
	Merge20LearnFileHeader header;
	std::string err;
	int skipped = -1;
	auto load = [&](const std::string& json)
	{
		skipped = -1;
		err.clear();
		return Merge20LearnFromJson(json, L"f.json", store, header, err, skipped);
	};
	// Wrong app is rejected.
	EXPECT_FALSE(load("{\"app\":\"Other\",\"version\":2,\"rules\":[]}"));
	EXPECT_FALSE(err.empty());
	// Version 3 is rejected; a string version is rejected.
	EXPECT_FALSE(load("{\"app\":\"Merge20Learn\",\"version\":3,\"rules\":[]}"));
	EXPECT_FALSE(load("{\"app\":\"Merge20Learn\",\"version\":\"2\",\"rules\":[]}"));
	// Version 1 is accepted.
	EXPECT_TRUE(load("{\"app\":\"Merge20Learn\",\"version\":1,\"rules\":[]}"));
	// A non-integer number rejects the whole file: the shared S8 parser
	// only accepts integers, so per-rule skipping is not possible here
	// (design section 4.1 deviation, documented in Merge20Learn.cpp).
	EXPECT_FALSE(load(
		"{\"app\":\"Merge20Learn\",\"version\":2,\"rules\":["
		"{\"key\":\"EXACT|a\",\"counts\":{\"KEEP_NEW\":1.5},\"n\":1}]}"));
	// Not an object / bad JSON is rejected.
	EXPECT_FALSE(load("[1,2]"));
	EXPECT_FALSE(load("{bad"));
	// Bad rules are skipped, good rules are kept.
	EXPECT_TRUE(load(
		"{\"app\":\"Merge20Learn\",\"version\":2,\"rules\":["
		"{\"key\":\"BAD|a\",\"counts\":{\"KEEP_NEW\":1}},"
		"{\"key\":\"EXACT|ok\",\"counts\":{\"KEEP_NEW\":2}},"
		"{\"key\":\"EXACT|nocounts\"},"
		"{\"key\":\"EXACT|neg\",\"counts\":{\"KEEP_NEW\":-1}},"
		"{\"key\":\"EXACT|empty\",\"counts\":{}},"
		"{\"key\":\"FEAT|mod|bogus\",\"counts\":{\"KEEP_NEW\":1}},"
		"{\"key\":\"EXACT|str\",\"counts\":{\"KEEP_NEW\":\"1\"}}"
		"]}"));
	EXPECT_EQ(skipped, 6);
	ASSERT_EQ(store.RuleCount(), static_cast<size_t>(1));
	EXPECT_NE(store.FindRule(L"EXACT|ok"), nullptr);
	// Unknown strategy names inside counts are ignored per key.
	EXPECT_TRUE(load(
		"{\"app\":\"Merge20Learn\",\"version\":2,\"rules\":["
		"{\"key\":\"EXACT|mix\",\"counts\":{\"KEEP_NEW\":2,\"UNKNOWN\":9}}]}"));
	EXPECT_EQ(skipped, 0);
	const Merge20LearnRule* rule = store.FindRule(L"EXACT|mix");
	ASSERT_NE(rule, nullptr);
	EXPECT_EQ(CountOf(*rule, Strat::KeepNew), 2);
	EXPECT_EQ(rule->n, 2);
	// Rules as an object map (HTML importLearn accepts this shape).
	EXPECT_TRUE(load(
		"{\"app\":\"Merge20Learn\",\"version\":2,\"rules\":{"
		"\"EXACT|m1\":{\"counts\":{\"INSERT_OLD\":1}},"
		"\"EXACT|m2\":{\"counts\":{\"BOTH_OLD_NEW\":3}}}}"));
	EXPECT_EQ(store.RuleCount(), static_cast<size_t>(2));
	// A lastStrat that is not among the counts is ignored.
	EXPECT_TRUE(load(
		"{\"app\":\"Merge20Learn\",\"version\":2,\"rules\":["
		"{\"key\":\"EXACT|ls\",\"counts\":{\"KEEP_NEW\":1},\"lastStrat\":\"BOTH_OLD_NEW\"}]}"));
	rule = store.FindRule(L"EXACT|ls");
	ASSERT_NE(rule, nullptr);
	EXPECT_FALSE(rule->hasLastStrat);
}

TEST(Merge20Learn, MergedJson)
{
	Merge20LearnStore store;
	store.Add(L"EXACT|k1", Strat::ReplaceWithOld, L"u-aaa", L"A.json", 3);
	store.Add(L"EXACT|k1", Strat::KeepNew, L"u-bbb", L"B.json", 1);
	store.Add(L"EXACT|k2", Strat::KeepNew, L"u-bbb", L"B.json", 2);
	std::vector<Merge20LearnRule> merged = store.MergeSources({ L"u-aaa", L"u-bbb" });
	std::vector<String> names = { L"A.json", L"B.json" };
	std::string json;
	ASSERT_TRUE(Merge20LearnMergedToJson(merged,
		L"u-11111111-2222-4333-8444-555555555555",
		L"\x30de\x30fc\x30b8\x652f\x63f4" L"20" L"\x53f7_\x5b66\x7fd2\x7d71\x5408_2026-11-26.json",
		L"2026-11-26T01:30:00Z", names, json));
	EXPECT_NE(json.find("\"sources\": ["), std::string::npos);
	EXPECT_NE(json.find("A.json"), std::string::npos);
	// The merged file loads back with the same rules (as one source on import).
	Merge20LearnStore loaded;
	Merge20LearnFileHeader header;
	std::string err;
	int skipped = -1;
	ASSERT_TRUE(Merge20LearnFromJson(json,
		L"\x30de\x30fc\x30b8\x652f\x63f4" L"20" L"\x53f7_\x5b66\x7fd2\x7d71\x5408_2026-11-26.json",
		loaded, header, err, skipped));
	EXPECT_EQ(skipped, 0);
	ASSERT_EQ(loaded.RuleCount(), static_cast<size_t>(2));
	const Merge20LearnRule* k1 = loaded.FindRule(L"EXACT|k1");
	ASSERT_NE(k1, nullptr);
	EXPECT_EQ(CountOf(*k1, Strat::ReplaceWithOld), 3);
	EXPECT_EQ(CountOf(*k1, Strat::KeepNew), 1);
	EXPECT_EQ(header.sourceNames.size(), static_cast<size_t>(2));
}
