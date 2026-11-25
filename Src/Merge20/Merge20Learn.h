// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Learn.h
 *
 * @brief Learning core (Merge20 Phase 3 L0, design sections 3.2, 4.2-4.4,
 * spec section 4(d)).
 *
 * Pure module (no MFC, no `_()`): exact-key / feature-key generation,
 * threshold matching, correction handling, per-source accounting and
 * JSON v2 conversion for the learning data. The same code is compiled
 * into WinMergeU and into the GoogleTest runner.
 *
 * Port of the HTML tool's learning functions (17_v17.15.html):
 * `hunkKeyExact`, `hunkFeatKey`, `matchLearn`, `learnDominant`,
 * `learnAdd`, `removeLearnSource`, `mergeLearnSources`, `learnSources`,
 * `learnFromHunk`, `exportLearn` / `importLearn` and `hashStr`.
 * Behaviour was checked against the HTML functions run in Node
 * (see Merge20Learn_test.cpp).
 *
 * This module only *computes* candidates (spec section 4(c) forbids
 * automatic application): Match() returns the suggested strategy for
 * one diff, it never touches a merge result. Recording is done by the
 * caller through Merge20LearnRecordJudgment() (L2 calls it from the
 * explicit-judgment handler only). Persistence and ON/OFF are L1.
 *
 * This file stays ASCII: the fixed words (strategy names, JSON field
 * names) are ASCII, and Japanese text is written with \\x escapes.
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "Merge20/Merge20Classify.h"
#include "Merge20/Merge20Session.h"

/** @brief Format version of the Merge20 learning file (JSON v2). */
constexpr int kMerge20LearnFormatVersion = 2;

/** @brief `app` value written by the fork. */
extern const String kMerge20LearnApp; // "Merge20Learn"
/** @brief `app` value written by the HTML tool, also accepted on import. */
extern const String kMerge20LearnHtmlApp; // Japanese "Merge20Learn" (see .cpp)
/** @brief Source id used by the HTML tool for its own learning. Never created by the fork. */
extern const String kMerge20LearnSelfSourceId; // "self"
/** @brief Source id for pre-v2 data without a source (HTML initLearn migration). */
extern const String kMerge20LearnLegacySourceId; // "legacy"

/**
 * @brief Judgment strategy (HTML tool `str[h]`, also the JSON `counts` key).
 *
 * The four strings are identical to the HTML tool's so that learning
 * JSON files stay interchangeable between the fork and the HTML tool.
 */
enum class Merge20LearnStrategy
{
	InsertOld,      /**< INSERT_OLD: insert the old side ("old insertion"). */
	KeepNew,        /**< KEEP_NEW: keep the new side ("new kept" / rejected). */
	ReplaceWithOld, /**< REPLACE_WITH_OLD: replace with the old side. */
	BothOldNew     /**< BOTH_OLD_NEW: keep both sides. */
};

/** @brief Strategy name used in JSON `counts` (e.g. "INSERT_OLD"). */
String Merge20LearnStrategyName(Merge20LearnStrategy strategy);
/** @brief Parse a strategy name; false when the name is not one of the four. */
bool Merge20LearnStrategyFromName(const String& name, Merge20LearnStrategy& out);

/**
 * @brief One source's contribution to one rule (HTML `rec.sources[srcId]`).
 *
 * `counts` keeps insertion order (see `order`): the HTML tool's
 * `learnDominant` resolves a tie by insertion order, and the JSON
 * writer keeps the same order, so the fork preserves it too.
 */
struct Merge20LearnSourceContribution
{
	String name; /**< Display name of the source (file name or "this PC"). */
	std::map<Merge20LearnStrategy, int> counts; /**< Judgments of this source only. */
	std::vector<Merge20LearnStrategy> order; /**< First-seen order of `counts` keys. */
	int n = 0; /**< Sum of `counts` (kept in sync by Add()). */
};

/** @brief One learned rule (HTML `learnRules[key]`). */
struct Merge20LearnRule
{
	String key; /**< "EXACT|..." or "FEAT|<type>|<feat>". */
	std::map<Merge20LearnStrategy, int> counts; /**< All sources summed. */
	std::vector<Merge20LearnStrategy> order; /**< First-seen order of `counts` keys. */
	int n = 0; /**< Sum of `counts` (kept in sync by Add()). */
	String ex; /**< Example file (relative path of the last recorded file). */
	String last; /**< Last recorded date ("YYYY-MM-DD"). */
	Merge20LearnStrategy lastStrat = Merge20LearnStrategy::KeepNew; /**< Latest choice (tie-breaker). */
	bool hasLastStrat = false; /**< False when never recorded / not in the file. */
	std::map<String /*source id*/, Merge20LearnSourceContribution> sources; /**< Per-source counts. */
};

/** @brief One entry of the source summary (HTML `learnSources()`). */
struct Merge20LearnSourceSummary
{
	String id; /**< Source id. */
	String name; /**< Display name (see Merge20LearnSourceDisplayName). */
	int rules = 0; /**< Rules this source contributes to. */
	int n = 0; /**< Judgments of this source (all rules summed). */
};

/**
 * @brief One learning candidate for a diff (HTML `matchLearn` result).
 *
 * L0 fills `strategy`, `via` and `n` only. `diffIdx` and `srcPanes`
 * are filled by the L2 adapter (design section 2.2 maps a strategy to
 * `srcPanes`); they stay empty here.
 */
struct Merge20LearnCandidate
{
	Merge20LearnStrategy strategy = Merge20LearnStrategy::KeepNew;
	String via; /**< "exact" (exact match) or "feat" (feature match). */
	int n = 0; /**< Exact: judgments for the dominant strategy. Feature: total records. */
	int diffIdx = -1; /**< Diff index (L2 adapter fills this). */
	std::vector<int> srcPanes; /**< Source panes to apply (L2 adapter fills this). */
};

/** @brief Header of a learning JSON file. */
struct Merge20LearnFileHeader
{
	String app; /**< `app` as read (empty when absent, i.e. v1). */
	bool hasApp = false;
	int version = kMerge20LearnFormatVersion; /**< `version` as read (2 when absent). */
	bool hasVersion = false;
	String sourceId; /**< File's source id (resolved: header value or "file:<hash>"). */
	String sourceName; /**< File's source name (resolved: header value or file name). */
	String exportedAt; /**< `exportedAt` as read (may be empty). */
	std::vector<String> sourceNames; /**< `sources` (merged output only). */
};

/**
 * @brief Display name of a source (HTML `learnSrcName`).
 * @param [in] srcId Source id.
 * @param [in] storedName Name stored in the file (may be empty).
 * @return Stored name when non-empty; the HTML tool's fixed names for
 * "self" / "legacy"; the id itself otherwise.
 */
String Merge20LearnSourceDisplayName(const String& srcId, const String& storedName);

/**
 * @brief File-name hash (HTML `hashStr`, ported exactly).
 *
 * 32-bit cyrb53-style hash over the UTF-16 code units of the text,
 * returned as unpadded lowercase hex: hex(h2) + hex(h1). Used as the
 * source id ("file:<hash>") of a v1 file without `sourceId`, so that
 * the fork and the HTML tool agree on the id of the same file.
 */
String Merge20LearnHashStr(const String& text);

/**
 * @brief Exact-match key of one diff (HTML `hunkKeyExact`).
 *
 * Each diff line becomes norm(old) + U+21C4 + norm(new), lines joined
 * with '\\n', prefixed with "EXACT|". norm() collapses every run of
 * JS whitespace to one space and trims (see Merge20LearnNormalize).
 * OP_NONE (context) lines are skipped: they are not part of the diff.
 */
String Merge20LearnExactKey(const std::vector<Merge20DiffLine>& lines);

/**
 * @brief Feature key of one diff (HTML `hunkFeatKey`); false when no feature.
 *
 * Type is "del" when every diff line is OP_1STONLY, "mod" when any line
 * is OP_DIFF/OP_TRIVIAL, otherwise "add". The feature is the first
 * match in the order round -> ifdel -> sql -> tax -> comment:
 * round tests old+new for Math.(round|floor|ceil)|BigDecimal|RoundingMode
 * (case-sensitive, like the HTML tool; NOT the S4 classifier, which uses
 * icase); ifdel tests the old side only for `\\bif\\s*\\(` (case-sensitive);
 * sql tests old+new for `\\b(select|insert|update|delete|from|where)\\b`
 * (case-insensitive, WITHOUT `join`, like the HTML tool; the S4
 * classifier includes `join`); tax tests old+new for a tax/amount/rate/
 * rounding character; comment tests the old side only for a line starting
 * with double-slash or slash-star. Key form is "FEAT|<type>|<feat>".
 */
bool Merge20LearnFeatureKey(const std::vector<Merge20DiffLine>& lines, String& keyOut);

/** @brief Metadata update for one rule (see Merge20LearnStore::SetRuleMeta). */
struct Merge20LearnRuleMeta
{
	bool setEx = false;
	String ex;
	bool setLast = false;
	String last;
	bool setLastStrat = false;
	Merge20LearnStrategy lastStrat = Merge20LearnStrategy::KeepNew;
};

/**
 * @brief Learned-rule store (HTML `learnRules` + owner id).
 *
 * The store keeps the invariant `rule.counts == sum of rule.sources`
 * and `rule.n == sum of rule.counts` (HTML `learnAdd` keeps the same
 * invariant). A rule whose total reaches 0 is removed, like the HTML
 * tool does.
 */
class Merge20LearnStore
{
public:
	Merge20LearnStore() = default;
	explicit Merge20LearnStore(const String& ownerId) : m_ownerId(ownerId) {}

	/** @brief This PC's source id ("u-<GUID>", set by L1; empty in L0 tests). */
	const String& GetOwnerId() const { return m_ownerId; }
	void SetOwnerId(const String& ownerId) { m_ownerId = ownerId; }

	/** @brief Number of rules. */
	size_t RuleCount() const { return m_rules.size(); }
	/** @brief Total judgments (sum of all rules' `n`). */
	int TotalJudgments() const;
	/** @brief All rules (key order). */
	const std::map<String, Merge20LearnRule>& Rules() const { return m_rules; }
	/** @brief Find one rule by key (nullptr when absent). */
	const Merge20LearnRule* FindRule(const String& key) const;
	/** @brief Remove all rules. */
	void Clear() { m_rules.clear(); }

	/**
	 * @brief Add (or subtract) judgments (HTML `learnAdd`).
	 * @param [in] key Exact or feature key.
	 * @param [in] strat Strategy to adjust.
	 * @param [in] srcId Source id the judgments belong to.
	 * @param [in] srcName Display name of the source (used only when the source is new).
	 * @param [in] delta +1 / -1 for one judgment, or a larger count when importing.
	 * A positive delta creates the rule when missing; a non-positive
	 * delta on a missing rule is a no-op. Counts are clamped at 0.
	 */
	void Add(const String& key, Merge20LearnStrategy strat,
		const String& srcId, const String& srcName, int delta);

	/**
	 * @brief Record one explicit judgment for one key (HTML `learnFromHunk` per key).
	 * When `prevStrat` is set and differs from `strat`, the old strategy
	 * loses one judgment (correction) before the new one gains one.
	 * `lastStrat` becomes `strat`, `ex`/`last` are updated.
	 */
	void Record(const String& key, Merge20LearnStrategy strat,
		const Merge20LearnStrategy* prevStrat,
		const String& srcId, const String& srcName,
		const String& ex, const String& last);

	/** @brief Update a rule's metadata (no-op when the rule does not exist). */
	void SetRuleMeta(const String& key, const Merge20LearnRuleMeta& meta);

	/**
	 * @brief Remove one source's contribution (HTML `removeLearnSource`).
	 * Only that source's counts are subtracted; other sources stay.
	 * Rules that reach 0 judgments are removed.
	 */
	void RemoveSource(const String& srcId);

	/** @brief Source summary (HTML `learnSources()`), most judgments first. */
	std::vector<Merge20LearnSourceSummary> GetSources() const;

	/**
	 * @brief Merge selected sources (HTML `mergeLearnSources`).
	 * Counts of the selected sources are summed per rule; `sources` is
	 * dropped (the merged output is one snapshot). Rules with 0
	 * judgments are left out. `lastStrat` is kept when the rule has one.
	 */
	std::vector<Merge20LearnRule> MergeSources(const std::vector<String>& srcIds) const;

	/**
	 * @brief Find the candidate for one diff (HTML `matchLearn`).
	 * Exact match first (1 or more records); otherwise feature match
	 * (3 or more records and the dominant strategy holds at least 80%,
	 * judged with integers: best*5 >= total*4). Ties are resolved by
	 * `lastStrat` (latest choice), like HTML `learnDominant`.
	 * @param [in] lines Diff lines of one diff (OP_NONE lines are skipped).
	 * @param [out] out Candidate (strategy / via / n).
	 * @return True when a candidate exists.
	 */
	bool Match(const std::vector<Merge20DiffLine>& lines, Merge20LearnCandidate& out) const;

private:
	std::map<String /*key*/, Merge20LearnRule> m_rules;
	String m_ownerId;
};

/**
 * @brief Record one explicit judgment for a diff (HTML `learnFromHunk`).
 * Builds the exact key and, when present, the feature key, and records
 * the judgment for both (with the correction rule of Merge20LearnStore::Record).
 * `prevLearned` is the candidate strategy that was shown for this diff,
 * or nullptr when no candidate was shown. `last` is the current date
 * ("YYYY-MM-DD", supplied by the caller so this module stays time-free).
 * Does nothing when the diff has no diff lines.
 */
void Merge20LearnRecordJudgment(Merge20LearnStore& store,
	const std::vector<Merge20DiffLine>& lines,
	Merge20LearnStrategy strategy, const Merge20LearnStrategy* prevLearned,
	const String& srcId, const String& srcName,
	const String& ex, const String& last);

/**
 * @brief Write the store as learning JSON v2 (HTML `exportLearn` shape).
 * @param [in] store Store to write.
 * @param [in] sourceId This file's source id (the store owner's id).
 * @param [in] sourceName This file's source name (output file name).
 * @param [in] exportedAt Output time (ISO 8601, may be empty to omit it).
 * @param [out] outUtf8 JSON text (UTF-8, 2-space indent, no BOM).
 * @return Always true for a well-formed store.
 */
bool Merge20LearnToJson(const Merge20LearnStore& store,
	const String& sourceId, const String& sourceName, const String& exportedAt,
	std::string& outUtf8);

/**
 * @brief Write merged rules as one learning JSON v2 (HTML `exportUnifiedLearn` shape).
 * @param [in] rules Rules from Merge20LearnStore::MergeSources (sources dropped).
 * @param [in] sourceId / sourceName / exportedAt As in Merge20LearnToJson.
 * @param [in] sourceNames Display names of the merged sources (root `sources`).
 * @param [out] outUtf8 JSON text (UTF-8, 2-space indent, no BOM).
 */
bool Merge20LearnMergedToJson(const std::vector<Merge20LearnRule>& rules,
	const String& sourceId, const String& sourceName, const String& exportedAt,
	const std::vector<String>& sourceNames, std::string& outUtf8);

/**
 * @brief Parse a learning JSON file into a store (replaces the store).
 *
 * Accepts the fork's `app` ("Merge20Learn") and the HTML tool's `app`
 * (Japanese "Merge20Learn"); a missing `app` is read as v1. `version`
 * 1, 2 or missing is accepted; other values (or a wrong `app`) reject
 * the whole file. A rule keeps its per-source counts when `sources`
 * is present; otherwise its counts are attributed to the file's source
 * (header `sourceId`, or "file:<hashStr(file name)>" for v1). A rule's
 * aggregate `counts` is recomputed from its sources when sources are
 * present (the store invariant); its `n` is always recomputed.
 * Bad rules are skipped and counted in `skippedRules` (missing/invalid
 * key, missing/non-object `counts`, a non-integer or negative count,
 * or no judgments left after filtering). Unknown strategy names inside
 * `counts` are ignored per key. A non-integer number anywhere makes
 * the S8 parser reject the whole file (design section 4.1 notes this
 * as a deviation: per-rule skipping of fractional counts is not
 * possible with the shared integer-only parser).
 * @param [in] utf8 JSON text (UTF-8).
 * @param [in] fileName File name (v1 source-id fallback and `sourceName` fallback).
 * @param [out] out Store (cleared first).
 * @param [out] header Parsed header (source id/name resolved).
 * @param [out] errDetail Empty on success, otherwise the reason.
 * @param [out] skippedRules Rules skipped for bad field types.
 * @return True on success.
 */
bool Merge20LearnFromJson(const std::string& utf8, const String& fileName,
	Merge20LearnStore& out, Merge20LearnFileHeader& header,
	std::string& errDetail, int& skippedRules);

/**
 * @brief Import a learning JSON file, accumulating (HTML `importLearn`).
 *
 * The file's source id is the header `sourceId` (or "file:<hash>" for
 * v1) and its name the header `sourceName` (or the file name). The
 * source's previous contribution is removed first, so re-importing
 * the same file replaces it instead of double-counting. Each rule's
 * aggregate `counts` is added under that one source (per-rule
 * `sources` in the file are NOT preserved: the HTML tool flattens
 * them the same way). `ex`/`last` take the imported value when
 * non-empty, otherwise keep the stored value; `lastStrat` is taken
 * from the file when present. `addedRules` counts rules that did not
 * exist after the removal, `mergedRules` counts rules that did
 * (same counting as the HTML tool).
 * @return False only when the whole file is rejected (see Merge20LearnFromJson).
 */
bool Merge20LearnImportJson(const std::string& utf8, const String& fileName,
	Merge20LearnStore& store, Merge20LearnFileHeader& header,
	std::string& errDetail, int& skippedRules, int& addedRules, int& mergedRules);
