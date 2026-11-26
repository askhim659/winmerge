// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20MasterDoc.h
 *
 * @brief Adapter between the master-data core (Merge20Master, D8)
 * and the application: where the masters file lives, the live
 * stores, and the JSON import/export.
 *
 * Merge20 (Phase 4 D8, design sections 4.4 and 5.2/D8, spec
 * sections 3 (4/4) and 6): the three masters have different
 * live stores, and the dialog edits all three through this
 * adapter:
 * - Exclude-word rules live in the Phase 1 prediffer's option
 *   keys (`Plugins\Merge20Exclude.sct/`: Count, Enabled<i>,
 *   IgnoreCase<i>, UseRegExp<i>, Pattern<i>, IgnoreL2) — the
 *   exact key strings the `Merge20Exclude.sct` scriptlet reads
 *   through `mergeApp.GetOption`, so the dialog edits what the
 *   prediffer applies. A missing key reads as the scriptlet's
 *   default (enabled, not ignore-case, not regex, empty pattern;
 *   Count 0; IgnoreL2 on).
 * - Highlight rules live in the masters JSON file (no other
 *   reader exists yet; showing them in the compare views is
 *   follow-up work). A missing or broken file falls back to
 *   the HTML presets.
 * - Risk keywords live in `OPT_MERGE20_RISK_KEYWORDS` (the S4
 *   classifier reads that key on every classification).
 *
 * The masters JSON file (`Merge20Masters.json`, in its own
 * directory next to the learning directory) mirrors the live
 * state on every save, and is the shareable form (export /
 * import, L5 precedent: a save dialog plus a sharing-notice
 * confirmation). The file is written with a temporary file
 * plus rename, the same way as the session save (S8).
 */
#pragma once

#include <string>
#include "UnicodeString.h"
#include "Merge20/Merge20Master.h"

/** @brief Directory holding the masters file. Created on demand by the save path; empty when it cannot be determined. */
String Merge20MasterGetDirectory();
/** @brief Full path of the masters file (`Merge20Masters.json`). */
String Merge20MasterGetFilePath();

/**
 * @brief Exclude rules as the prediffer sees them: read from
 * the `Plugins\Merge20Exclude.sct/` option keys (missing keys
 * read as the scriptlet's defaults).
 */
std::vector<Merge20ExcludeRule> Merge20MasterLoadExcludeFromOptions();
/** @brief Write exclude rules (and the IgnoreL2 flag) to the prediffer's option keys. */
void Merge20MasterSaveExcludeToOptions(const std::vector<Merge20ExcludeRule>& rules, bool ignoreL2);
/** @brief IgnoreL2 flag as the prediffer sees it (default ON). */
bool Merge20MasterLoadExcludeIgnoreL2FromOptions();

/**
 * @brief Load the live state for the dialog: exclude rules
 * from the prediffer's option keys, highlight rules from the
 * masters file (or the HTML presets when the file is missing),
 * risk keywords from OPT_MERGE20_RISK_KEYWORDS.
 * @param [out] out Loaded masters.
 * @param [out] errStr Human-readable reason when the masters
 * file existed but could not be read (the highlight rules
 * then fall back to the presets); empty otherwise.
 * @return False only when the masters file existed but was
 * broken (the caller reports errStr and still opens the
 * dialog with the fallback rules).
 */
bool Merge20MasterLoadStore(Merge20Masters& out, String& errStr);

/**
 * @brief Save the live state: exclude rules to the prediffer's
 * option keys, risk keywords to OPT_MERGE20_RISK_KEYWORDS,
 * and the whole masters to the masters file (mirror).
 * @param [out] errStr Human-readable reason on failure (spec
 * section 7: a failed save is reported, never lost silently).
 * @return False when the file write failed (the option keys
 * are already written in that case).
 */
bool Merge20MasterSaveStore(const Merge20Masters& masters, String& errStr);

/**
 * @brief Merge20 (Phase 4 D8): default file name for a masters
 * JSON export (the HTML tool's export file-name shape, L5
 * precedent). The save dialog decides the folder; only the
 * file name is fixed.
 */
String Merge20MasterDefaultExportFileName();

/**
 * @brief Merge20 (Phase 4 D8): write the given masters as
 * masters JSON to an explicit file (the export shape). The
 * live stores are not changed.
 * @param [out] errDetail ASCII reason on failure.
 */
bool Merge20MasterExportToFile(const Merge20Masters& masters, const String& filePath, std::string& errDetail);

/**
 * @brief Merge20 (Phase 4 D8): read a masters JSON file into
 * a masters struct (the import shape). The live stores are
 * not changed; the caller applies the result (by saving it).
 * @param [out] errDetail ASCII reason on failure (a wrong
 * `app` / `version` rejects the file).
 * @param [out] skippedRules Rule entries skipped for a bad format.
 */
bool Merge20MasterImportFromFile(const String& filePath, Merge20Masters& out, std::string& errDetail, int& skippedRules);
