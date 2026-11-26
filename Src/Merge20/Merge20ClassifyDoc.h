/**
 * @file  Merge20ClassifyDoc.h
 *
 * @brief Adapter between CMergeDoc and the pure classifier (S4).
 *
 * Merge20ClassifyFromMergeDoc walks the merge doc's diff list, reads
 * the diff line texts from the two text buffers and runs
 * Merge20ClassifyDiffLines on them. Only 2-way docs are classified;
 * the caller must check m_nBuffers == 2, because 3-way diff ranges
 * do not map onto OLD/NEW line counts, so 3-way files stay
 * "not calculated" for now.
 *
 * The summary text is composed here (through `_()`), because the pure
 * classifier must stay free of the translation macro so it can be
 * unit-tested.
 */
#pragma once

#include <vector>
#include "UnicodeString.h"
#include "Merge20Types.h"
#include "Merge20/Merge20Classify.h"
#include "Merge20/Merge20BulkDiff.h"

class CMergeDoc;

/**
 * @brief True when the opened 2-way doc exceeds the simple-compare threshold (H-SIMPLE).
 * @param [in] doc Opened merge doc (2-way; other buffer counts return false).
 * @param [in] options Compare options (whitespace / case / threshold), the same ones the bulk path uses.
 * @return True when the trimmed-middle line counts of the two text
 * buffers satisfy n*m > threshold (HTML `lcs`, via
 * Merge20BulkIsSimple), i.e. the bulk classifier would have used
 * the positional fallback for this file.
 *
 * The lines are read from the text buffers (ghost lines skipped
 * when counting), so the check works both before the diff passes
 * (Rescan computes it early to skip the heavy post-passes) and at
 * the classification hook afterwards. The caller passes the
 * result into Merge20ClassifyFromMergeDoc, so the bulk path and
 * the opened-file path agree and the [Simple] badge survives
 * opening. The opened diff itself stays the full diffutils diff;
 * the engine is not replaced (see MERGE20_NOTES.md §45).
 */
bool Merge20ClassifyIsSimpleCompare(const CMergeDoc& doc, const Merge20BulkDiffOptions& options);

/**
 * @brief Classify one open (2-way) merge doc into a per-file state.
 * @param [in] doc Merge doc whose diff list was just computed by Rescan.
 * @param [in] riskKeywords Risk keywords from the
 * OPT_MERGE20_RISK_KEYWORDS option (see Merge20ParseRiskKeywords).
 * @param [in] simpleCompare Whether the file exceeds the
 * simple-compare threshold (Merge20ClassifyIsSimpleCompare,
 * computed by the caller). Stored as-is in the returned state, so
 * the folder list keeps the [Simple] badge after the file is
 * opened; the classification itself still comes from the full
 * diffutils diff list.
 * @return Per-file state with classified=true, action, risk, counts,
 * summary and encodings filled in. The review state and output flag
 * are left default; the caller (CDirDoc) preserves the stored ones.
 */
Merge20FileState Merge20ClassifyFromMergeDoc(const CMergeDoc& doc, const std::vector<String>& riskKeywords,
	bool simpleCompare);

/**
 * @brief Classify one file from bulk-diff ranges and in-memory lines (D10).
 * @param [in] ranges Diff ranges from Merge20BulkDiff (DIFFRANGE convention:
 * empty sides use end = begin - 1).
 * @param [in] oldLines Old-side lines (0-based, in file order).
 * @param [in] newLines New-side lines (0-based, in file order).
 * @param [in] riskKeywords Risk keywords from the
 * OPT_MERGE20_RISK_KEYWORDS option (see Merge20ParseRiskKeywords).
 * @return Per-file state with classified=true, action, risk, counts and
 * summary filled in. The review state and output flag are left default;
 * the caller (CDirDoc) preserves the stored ones. Encodings stay empty
 * (the bulk path has no buffer encoding objects) and simpleCompare
 * stays false (the caller sets it from the differ result).
 */
Merge20FileState Merge20ClassifyFromRanges(const std::vector<Merge20BulkRange>& ranges,
	const std::vector<String>& oldLines, const std::vector<String>& newLines,
	const std::vector<String>& riskKeywords);

/**
 * @brief Compose the summary column text from a classification result.
 * @param [in] result Structured classification result.
 * @return Single-line summary: counts first, then the detected
 * insights (in detection order, like the HTML tool), then the
 * affected functions. This is a rule-based summary, not generated
 * AI: the same input always returns the same text (spec section
 * 3 (2/4)). The fixed words go through `_()` for translation.
 */
String Merge20FormatSummary(const Merge20ClassifyResult& result);
