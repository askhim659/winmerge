/**
 * @file  Merge20Classify.h
 *
 * @brief Rule-based classification of one file: action category, risk
 * level and deterministic summary (Merge20 Phase 2 S4, design section
 * 3.1, spec sections 3 (2/4) and 4).
 *
 * This module is a port of the HTML tool's `runAnalysis` classification
 * and `analyzeInsights` (17_v17.15.html). It is deliberately free of
 * MFC and of the translation macro `_()`: the same code is compiled
 * into WinMergeU and into the GoogleTest runner, and the summary text
 * is composed from the structured result by the caller
 * (Merge20ClassifyDoc), where `_()` is available.
 *
 * Risk model (spec section 3 (2/4): one attention point means Medium,
 * several overlapping points mean High):
 * - one structural point when the file has missing-in-new lines, and
 *   one when it has modified lines. That alone reproduces the HTML
 *   risk table: missing-only -> Medium, modified-only -> Medium,
 *   missing+modified -> High, added-only / identical -> None.
 * - one more point when any diff line contains a risk keyword
 *   (tax / amount / rate / rounding / SQL words; the list is the
 *   OPT_MERGE20_RISK_KEYWORDS option, editable in the settings).
 * The rule-based insights (rounding change, SQL change, deleted if
 * guard, condition/boundary change, constant change) are reported in
 * the summary text; they do NOT add risk points, so that the demo
 * sample (fixed-asset-tax code) classifies exactly like the HTML tool
 * (a pure constant change in a comment line must stay Medium, not
 * become High).
 */
#pragma once

#include <vector>
#include "DiffList.h"
#include "Merge20Types.h"

/** @brief One diff line, in file order (HTML tool rows). */
struct Merge20DiffLine
{
	OP_TYPE op = OP_NONE; /**< OP_1STONLY: missing in new, OP_2NDONLY: added, OP_DIFF: changed, OP_TRIVIAL: ignored diff, OP_NONE: context line (see below) */
	String oldText;       /**< Old-side text, empty for added lines. */
	String newText;       /**< New-side text, empty for missing lines. */
};

/** @brief Kinds of deterministic insights, in detection order. */
enum class Merge20Insight
{
	ROUNDING,  /**< Rounding change. */
	SQL,       /**< SQL change. */
	IF_GUARD,  /**< Deleted if guard. */
	CONDITION, /**< Condition/boundary change. */
	CONSTANT   /**< Constant change. */
};

/**
 * @brief Structured result of Merge20ClassifyDiffLines().
 *
 * The summary text itself is composed by the caller from these fields
 * (see Merge20FormatSummary in Merge20ClassifyDoc), because the fixed
 * words must go through `_()` for translation.
 */
struct Merge20ClassifyResult
{
	int nMissing = 0;  /**< Missing-in-new (deleted) lines. */
	int nModified = 0; /**< Changed lines. */
	int nAdded = 0;    /**< Newly added lines. */
	int nExcluded = 0; /**< Lines in ignored (trivial) diffs. */
	ActionCategory action = ActionCategory::NO_ACTION_IDENTICAL;
	RiskLevel risk = RiskLevel::NONE;
	int attentionPoints = 0; /**< Attention points counted for the risk level (see the header comment). */
	bool hasRoundingChange = false;    /**< Math.round/floor/ceil, BigDecimal or RoundingMode changed. */
	bool hasSqlChange = false;         /**< SQL keyword changed. */
	bool hasIfGuardDeletion = false;   /**< An if guard was deleted (old-only logic lost). */
	bool hasConditionChange = false;   /**< The operator set of a line changed. */
	bool hasConstantChange = false;    /**< The number list of a line changed. */
	bool hasRiskKeyword = false;       /**< A risk keyword was found in a diff line. */
	std::vector<Merge20Insight> insights; /**< Detected insights, in detection order (at most 4, like the HTML tool). */
	String constantFrom; /**< First changed number, old side (valid when hasConstantChange). */
	String constantTo;   /**< First changed number, new side (valid when hasConstantChange). */
	std::vector<String> functions; /**< Functions affected by the diffs, up to 5, in first-seen order. */
};

/**
 * @brief Classify one file's diff lines.
 * @param [in] lines Diff lines in file order. OP_NONE lines are context
 * lines (the enclosing function definition above a hunk, supplied by the
 * caller so the affected function can be named even when the definition
 * itself did not change); they update the current function but are not
 * counted and produce no insights. OP_TRIVIAL lines are counted as
 * excluded lines only.
 * @param [in] riskKeywords Risk keywords (already split, see
 * Merge20ParseRiskKeywords); matched case-insensitively as substrings.
 * @return Structured classification result.
 */
Merge20ClassifyResult Merge20ClassifyDiffLines(const std::vector<Merge20DiffLine>& lines, const std::vector<String>& riskKeywords);

/**
 * @brief Split the OPT_MERGE20_RISK_KEYWORDS option value.
 * @param [in] text Semicolon-separated keyword list.
 * @return Keywords, trimmed, empty entries dropped, in order.
 */
std::vector<String> Merge20ParseRiskKeywords(const String& text);

/**
 * @brief Match one line against the function-definition pattern
 * (HTML analyzeInsights FN_RE).
 * @param [in] text Line text to test.
 * @param [out] functionName Function name when the line defines one.
 * @return True when the line defines a function.
 */
bool Merge20MatchFunctionName(const String& text, String& functionName);
