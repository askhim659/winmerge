// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormMaster.h
 *
 * @brief FORM/XML comparison (Wave 5 F6): the name master
 * (design merge20-form-compare-design.md sections 2.10, 8 and
 * 10 F6).
 *
 * Pure module (no MFC, no `_()`): the same code is compiled into
 * WinMergeU and into the GoogleTest runner. It is a port of the
 * HTML tool's `parseMaster` / `getFormName` / `defaultMasterData`
 * (FROM_____1.html), with the deliberate differences of design
 * section 8.2:
 * - A line without a tab is a parse error (the HTML tool falls
 *   back to whitespace splitting); the caller warns and aborts
 *   the save.
 * - A duplicated (id, name) row is removed on parse (the HTML
 *   tool keeps duplicates); a different name for the same id is
 *   not a duplicate.
 * - The default data is design appendix A (171 rows, 142 unique
 *   ids), verified against the HTML `defaultMasterData` (same
 *   rows, same order).
 *
 * Storage (design section 8.2) is the tab-separated text file
 * `Merge20FormMaster.txt`, one `ID<TAB>name` row per line. The
 * file itself is read and written by the dialog module
 * (Merge20FormMasterDlg, MFC side: the INI/AppData directory
 * decision lives there, like Merge20LearnGetDirectory); this
 * module only parses and serializes the text.
 */
#pragma once

#include <string>
#include <vector>

/** @brief One name-master row: one id, one name. */
struct Merge20FormMasterEntry
{
	std::wstring id; /**< Form id (e.g. `ZEB001R007`). */
	std::wstring name; /**< Form name (e.g. the asset certificate name). */
};

/** @brief The name master: ordered rows; one id may have several names. */
struct Merge20FormMaster
{
	std::vector<Merge20FormMasterEntry> entries; /**< Rows in file order. */
};

/**
 * @brief Parse name-master text (`ID<TAB>name` per line).
 * @param [in] text Text to parse (CRLF or LF line endings).
 * @param [out] out Parsed master (empty when the parse fails).
 * @param [out] badLine 1-based number of the offending line on
 * failure (0 when the parse succeeds).
 * @return False when a non-empty line has no tab, or an empty id
 * or name. Blank lines are ignored. A duplicated (id, name) row
 * is removed (first occurrence kept); a different name for the
 * same id is kept.
 */
bool Merge20FormMasterParseText(const std::wstring& text,
	Merge20FormMaster& out, int& badLine);

/**
 * @brief Serialize the master as `ID<TAB>name` lines (CRLF).
 * @param [in] master Master to serialize.
 * @return The text, one line per entry (trailing CRLF included).
 */
std::wstring Merge20FormMasterToText(const Merge20FormMaster& master);

/**
 * @brief The default master (design appendix A): 171 rows,
 * 142 unique ids.
 * @return A fresh copy of the default master.
 */
Merge20FormMaster Merge20FormMasterGetDefault();

/**
 * @brief Resolve a form name for a file (HTML `getFormName`).
 * @param [in] master Master to look in.
 * @param [in] fileName File name (a relative path is fine; only
 * the file part is used).
 * @return The names joined with ` / ` (up to 3 names), or
 * `first name + " + "N more kinds"` when there are 4 or more,
 * or `(unregistered)` when the id is not in the master.
 * The base name is the file name without its directory and
 * without its extension; an exact match wins, otherwise the
 * first id contained in the base name wins (insertion order).
 */
std::wstring Merge20FormMasterResolve(const Merge20FormMaster& master,
	const std::wstring& fileName);

/**
 * @brief Number of unique ids in the master (the count badge:
 * the HTML `masterCountBadge` shows this number of kinds).
 * @param [in] master Master to count.
 * @return Unique id count.
 */
int Merge20FormMasterCountUniqueIds(const Merge20FormMaster& master);

/**
 * @brief The display text for an unregistered form
 * (`(unregistered)`, HTML `getFormName`).
 * @return The unregistered text.
 */
std::wstring Merge20FormMasterUnregistered();
