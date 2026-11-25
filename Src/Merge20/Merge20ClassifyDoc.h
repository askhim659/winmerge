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

class CMergeDoc;

/**
 * @brief Classify one open (2-way) merge doc into a per-file state.
 * @param [in] doc Merge doc whose diff list was just computed by Rescan.
 * @param [in] riskKeywords Risk keywords from the
 * OPT_MERGE20_RISK_KEYWORDS option (see Merge20ParseRiskKeywords).
 * @return Per-file state with classified=true, action, risk, counts,
 * summary and encodings filled in. The review state and output flag
 * are left default; the caller (CDirDoc) preserves the stored ones.
 */
Merge20FileState Merge20ClassifyFromMergeDoc(const CMergeDoc& doc, const std::vector<String>& riskKeywords);

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
