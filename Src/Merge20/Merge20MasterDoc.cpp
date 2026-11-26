// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20MasterDoc.cpp
 *
 * @brief Implementation of the master-data adapter (Merge20
 * Phase 4 D8, design sections 4.4 and 5.2/D8, spec sections
 * 3 (4/4) and 6).
 *
 * The prediffer contract: the Phase 1 `Merge20Exclude.sct`
 * scriptlet reads its rules through `mergeApp.GetOption`
 * (`Plugins\Merge20Exclude.sct/` + Count / Enabled<i> /
 * IgnoreCase<i> / UseRegExp<i> / Pattern<i> / IgnoreL2), and
 * `MergeAppCOMClass::GetOption` forwards the name verbatim to
 * `GetOptionsMgr()->Get(name)`. Reading and writing the same
 * key strings from C++ therefore touches the exact values the
 * prediffer applies — no second live store, no drift. A key
 * that was never written reads as VT_NULL; the adapter applies
 * the scriptlet's defaults then (the `GetOption(key, default)`
 * calls in the scriptlet), detected the same way
 * `MergeAppCOMClass` does (`GetType() == varprop::VT_NULL`).
 *
 * File I/O mirrors the learning adapter (L1/L5):
 * `std::ifstream` / `std::ofstream` with a temporary file plus
 * rename for writes (the S8 session-save way).
 */
#include "StdAfx.h"
#include "Merge20/Merge20MasterDoc.h"
#include "Merge20/Merge20Session.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "IniOptionsMgr.h"
#include "Environment.h"
#include "paths.h"
#include "UnicodeString.h"
#include "I18n.h"
#include <filesystem>
#include <fstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
/** @brief File name of the masters file. */
constexpr wchar_t kMasterFileName[] = L"Merge20Masters.json";
/** @brief Option-key prefix of the prediffer's rules (the scriptlet's REGKEY_PATH, verbatim). */
constexpr wchar_t kExcludeOptionPrefix[] = L"Plugins\\Merge20Exclude.sct/";

/** @brief Decode UTF-8 bytes into a wide string (the L1 helper idiom). */
String Utf8ToString(const std::string& utf8)
{
	String out;
	Merge20Utf8Decode(utf8, out);
	return out;
}

/** @brief Read an int option, or the default when the key was never written. */
int MasterGetOptionInt(const String& name, int defaultValue)
{
	const varprop::VariantValue& value = GetOptionsMgr()->Get(name);
	if (value.GetType() == varprop::VT_NULL)
		return defaultValue;
	return value.GetInt();
}

/** @brief Read a bool option, or the default when the key was never written. */
bool MasterGetOptionBool(const String& name, bool defaultValue)
{
	const varprop::VariantValue& value = GetOptionsMgr()->Get(name);
	if (value.GetType() == varprop::VT_NULL)
		return defaultValue;
	return value.GetBool();
}

/** @brief Read a string option, or the default when the key was never written. */
String MasterGetOptionString(const String& name, const String& defaultValue)
{
	const varprop::VariantValue& value = GetOptionsMgr()->Get(name);
	if (value.GetType() == varprop::VT_NULL)
		return defaultValue;
	return value.GetString();
}

/** @brief Option key of one per-rule field (the scriptlet's `"Plugins\\Merge20Exclude.sct/" + field + i`). */
String MasterExcludeRuleKey(const String& field, int index)
{
	return String(kExcludeOptionPrefix) + field + strutils::to_str(index);
}

// Read a whole file as UTF-8 text (the L5 ReadFileToUtf8 idiom).
bool MasterReadFileToUtf8(const String& filePath, std::string& utf8, std::string& errDetail)
{
	errDetail.clear();
	std::ifstream in(std::filesystem::path(filePath), std::ios::binary);
	if (!in)
	{
		errDetail = "cannot open file";
		return false;
	}
	utf8.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	if (in.bad())
	{
		errDetail = "failed to read file";
		return false;
	}
	return true;
}

// Write UTF-8 text to a file with a temporary file plus rename
// (the S8 session-save way); falls back to a direct write when
// the rename fails (e.g. a locked target).
bool MasterWriteTextFileAtomic(const String& filePath, const std::string& utf8, std::string& errDetail)
{
	errDetail.clear();
	std::filesystem::path target(filePath);
	String tmpName = filePath;
	tmpName += L".tmp";
	std::filesystem::path tmp(tmpName);
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		if (!out)
		{
			errDetail = "cannot create temporary file";
			return false;
		}
		out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
		out.flush();
		if (!out)
		{
			errDetail = "failed to write file";
			std::error_code ec;
			std::filesystem::remove(tmp, ec);
			return false;
		}
	}
	std::error_code ec;
	std::filesystem::rename(tmp, target, ec);
	if (ec)
	{
		std::ofstream out(target, std::ios::binary | std::ios::trunc);
		if (!out)
		{
			errDetail = "cannot create file";
			std::filesystem::remove(tmp, ec);
			return false;
		}
		out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
		out.flush();
		if (!out)
		{
			errDetail = "failed to write file";
			std::filesystem::remove(tmp, ec);
			return false;
		}
		std::filesystem::remove(tmp, ec);
	}
	return true;
}
}

String Merge20MasterGetDirectory()
{
	if (dynamic_cast<CIniOptionsMgr*>(GetOptionsMgr()) != nullptr)
		return paths::ConcatPath(env::GetProgPath(), _T("Merge20Masters"));
	return paths::ConcatPath(env::GetAppDataPath(), _T("WinMerge\\Merge20Masters"));
}

String Merge20MasterGetFilePath()
{
	return paths::ConcatPath(Merge20MasterGetDirectory(), kMasterFileName);
}

std::vector<Merge20ExcludeRule> Merge20MasterLoadExcludeFromOptions()
{
	std::vector<Merge20ExcludeRule> rules;
	const int count = MasterGetOptionInt(String(kExcludeOptionPrefix) + _T("Count"), 0);
	for (int i = 1; i <= count; ++i)
	{
		Merge20ExcludeRule rule;
		rule.enabled = MasterGetOptionBool(MasterExcludeRuleKey(_T("Enabled"), i), true);
		rule.ignoreCase = MasterGetOptionBool(MasterExcludeRuleKey(_T("IgnoreCase"), i), false);
		rule.useRegExp = MasterGetOptionBool(MasterExcludeRuleKey(_T("UseRegExp"), i), false);
		rule.pattern = MasterGetOptionString(MasterExcludeRuleKey(_T("Pattern"), i), String());
		rules.push_back(rule);
	}
	return rules;
}

bool Merge20MasterLoadExcludeIgnoreL2FromOptions()
{
	return MasterGetOptionBool(String(kExcludeOptionPrefix) + _T("IgnoreL2"), true);
}

void Merge20MasterSaveExcludeToOptions(const std::vector<Merge20ExcludeRule>& rules, bool ignoreL2)
{
	// The scriptlet reads only up to Count, so keys beyond the
	// new Count (left over from a longer rule list) are ignored;
	// there is no option-delete API to remove them.
	GetOptionsMgr()->SaveOption(String(kExcludeOptionPrefix) + _T("Count"), static_cast<int>(rules.size()));
	for (size_t i = 0; i < rules.size(); ++i)
	{
		const int index = static_cast<int>(i) + 1;
		const Merge20ExcludeRule& rule = rules[i];
		GetOptionsMgr()->SaveOption(MasterExcludeRuleKey(_T("Enabled"), index), rule.enabled);
		GetOptionsMgr()->SaveOption(MasterExcludeRuleKey(_T("IgnoreCase"), index), rule.ignoreCase);
		GetOptionsMgr()->SaveOption(MasterExcludeRuleKey(_T("UseRegExp"), index), rule.useRegExp);
		GetOptionsMgr()->SaveOption(MasterExcludeRuleKey(_T("Pattern"), index), rule.pattern);
	}
	GetOptionsMgr()->SaveOption(String(kExcludeOptionPrefix) + _T("IgnoreL2"), ignoreL2);
}

bool Merge20MasterLoadStore(Merge20Masters& out, String& errStr)
{
	errStr.clear();
	out = Merge20Masters();
	out.excludeIgnoreL2 = Merge20MasterLoadExcludeIgnoreL2FromOptions();
	out.excludeRules = Merge20MasterLoadExcludeFromOptions();
	out.riskKeywords = GetOptionsMgr()->GetString(OPT_MERGE20_RISK_KEYWORDS);
	out.highlightRules = Merge20MasterDefaultHighlightRules();
	const String filePath = Merge20MasterGetFilePath();
	if (paths::DoesPathExist(filePath) != paths::IS_EXISTING_FILE)
		return true;
	std::string utf8;
	std::string errDetail;
	int skippedRules = 0;
	if (!MasterReadFileToUtf8(filePath, utf8, errDetail))
	{
		errStr = strutils::format_string1(_("Could not load the master data: %1"), Utf8ToString(errDetail));
		return false;
	}
	Merge20Masters fromFile;
	if (!Merge20MastersFromJson(utf8, fromFile, errDetail, skippedRules))
	{
		errStr = strutils::format_string1(_("Could not load the master data: %1"), Utf8ToString(errDetail));
		return false;
	}
	// The file mirrors the live state, but the option keys are
	// authoritative for the exclude rules and the risk keywords
	// (a file edited by hand must not silently override them);
	// only the highlight rules come from the file.
	out.highlightRules = fromFile.highlightRules;
	return true;
}

bool Merge20MasterSaveStore(const Merge20Masters& masters, String& errStr)
{
	errStr.clear();
	Merge20MasterSaveExcludeToOptions(masters.excludeRules, masters.excludeIgnoreL2);
	GetOptionsMgr()->SaveOption(OPT_MERGE20_RISK_KEYWORDS, masters.riskKeywords);
	const String dir = Merge20MasterGetDirectory();
	if (paths::EnsurePathExist(dir).empty())
	{
		errStr = strutils::format_string1(_("Cannot create the master data folder:\n%1"), dir);
		return false;
	}
	std::string utf8;
	if (!Merge20MastersToJson(masters, utf8))
	{
		errStr = _("Could not save the master data: failed to serialize master data");
		return false;
	}
	std::string errDetail;
	if (!MasterWriteTextFileAtomic(Merge20MasterGetFilePath(), utf8, errDetail))
	{
		errStr = strutils::format_string1(_("Could not save the master data: %1"), Utf8ToString(errDetail));
		return false;
	}
	return true;
}

String Merge20MasterDefaultExportFileName()
{
	const String today = String(CTime::GetCurrentTime().Format(_T("%Y-%m-%d")).GetString());
	// The HTML export file-name shape (L5 precedent), written
	// with \u escapes so the source stays ASCII.
	return String(L"\u30DE\u30FC\u30B8\u652F\u63F420\u53F7_\u30DE\u30B9\u30BF_") + today + _T(".json");
}

bool Merge20MasterExportToFile(const Merge20Masters& masters, const String& filePath, std::string& errDetail)
{
	errDetail.clear();
	std::string utf8;
	if (!Merge20MastersToJson(masters, utf8))
	{
		errDetail = "failed to serialize master data";
		return false;
	}
	return MasterWriteTextFileAtomic(filePath, utf8, errDetail);
}

bool Merge20MasterImportFromFile(const String& filePath, Merge20Masters& out, std::string& errDetail, int& skippedRules)
{
	errDetail.clear();
	skippedRules = 0;
	std::string utf8;
	if (!MasterReadFileToUtf8(filePath, utf8, errDetail))
		return false;
	return Merge20MastersFromJson(utf8, out, errDetail, skippedRules);
}
