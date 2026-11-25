// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20LearnDoc.h
 *
 * @brief Adapter between the learning core (Merge20Learn, L0) and the
 * application: where the learning file lives, lazy loading, saving,
 * the ON/OFF switch and this PC's source id.
 *
 * Merge20 (Phase 3 L1, design sections 3.4, 3.5 and 5): the learning
 * data is one JSON v2 file, `Merge20Learn.json`, in its own directory
 * next to the session directory (`Merge20Learn`, not `Merge20Sessions`):
 * the learning data is shared across folder pairs, while a session is
 * bound to one folder pair, so the two must not be mixed. The file is
 * loaded lazily on first access (a missing or broken file never blocks
 * startup; the failure is remembered and reported when the learning
 * dialog is opened, L4), and saved immediately whenever the store
 * changes (L2 records, L5 import/delete/clear). Saving uses a
 * temporary file plus rename, the same way as the session save (S8).
 *
 * The ON/OFF key `OPT_MERGE20_LEARNING_ENABLED` was added in S8 but
 * had no reader until now; `Merge20LearnIsEnabled()` is its first
 * reader (L2 checks it at the start of the record and candidate
 * functions, and the compare options page binds it to a checkbox).
 * This PC's source id is kept in the new key
 * `OPT_MERGE20_LEARNING_OWNER` ("u-<GUID>", never "self": the HTML
 * tool treats "self" specially, design section 4.3).
 *
 * This header is MFC-free apart from the store type, so the pure
 * module stays testable; all options-manager and file access lives
 * in the .cpp. No automatic application is done here (spec section
 * 4(c)): loading and saving the store never touches a merge result.
 */
#pragma once

#include "UnicodeString.h"
#include "Merge20/Merge20Learn.h"

/** @brief Directory holding the learning file. Created on demand by the save path; empty when it cannot be determined. */
String Merge20LearnGetDirectory();
/** @brief Full path of the learning file (`Merge20Learn.json`). */
String Merge20LearnGetFilePath();

/**
 * @brief Learning ON/OFF (first reader of `OPT_MERGE20_LEARNING_ENABLED`).
 * @return False while learning is off: L2 then records nothing and
 * computes no candidates (the HTML tool's OFF behaviour).
 */
bool Merge20LearnIsEnabled();

/**
 * @brief This PC's source id ("u-<GUID>").
 *
 * Read from `OPT_MERGE20_LEARNING_OWNER`; when the key is still empty
 * (first use), a GUID is generated with `CoCreateGuid`, stored in the
 * key and returned. The id is stable per PC afterwards, so judgments
 * recorded here keep the same source across restarts.
 */
String Merge20LearnGetOwnerId();

/** @brief Display name of this PC's source ("Learning on this PC (self)"). */
String Merge20LearnGetOwnerDisplayName();

/**
 * @brief The lazily loaded learning store.
 *
 * The file is read on the first call only; a missing file means an
 * empty store, and a broken file also means an empty store with the
 * failure remembered (see Merge20LearnGetLoadError). The returned
 * store is the single in-memory copy; callers that change it must
 * call Merge20LearnSaveStore afterwards.
 */
Merge20LearnStore& Merge20LearnGetStore();

/**
 * @brief True when the lazy load failed; errStr holds the reason.
 * @return False when the file loaded (or did not exist yet).
 */
bool Merge20LearnGetLoadError(String& errStr);

/**
 * @brief Save the store to `Merge20Learn.json` (temporary file plus rename).
 * @param [out] errStr Human-readable reason on failure (spec section 7:
 *   a failed save is reported, never lost silently).
 * @return False when the write failed.
 */
bool Merge20LearnSaveStore(String& errStr);
