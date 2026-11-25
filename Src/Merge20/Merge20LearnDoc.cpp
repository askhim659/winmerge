// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20LearnDoc.cpp
 *
 * @brief Adapter between the learning core (Merge20Learn, L0) and the
 * application. See Merge20LearnDoc.h.
 */
#include "StdAfx.h"
#include "Merge20/Merge20LearnDoc.h"
#include "Merge20/Merge20Learn.h"
#include "Merge20/Merge20Session.h"
#include "OptionsDef.h"
#include "OptionsMgr.h"
#include "IniOptionsMgr.h"
#include "Environment.h"
#include "paths.h"
#include "UnicodeString.h"
#include "MergeApp.h"
#include "I18n.h"
#include <combaseapi.h>
#include <filesystem>
#include <fstream>

namespace
{

	// Lazily loaded learning store (see Merge20LearnGetStore): the
	// single in-memory copy, whether it has been loaded yet, and the
	// failure of the load (if any) for the learning dialog (L4).
	Merge20LearnStore g_learnStore;
	bool g_learnStoreLoaded = false;
	String g_learnStoreLoadError;

	// File name of the learning data (fixed: the data is not bound
	// to a folder pair, so no hash is needed, unlike the session file).
	const String kLearnFileName = _T("Merge20Learn.json");

	String Utf8ToString(const std::string& utf8)
	{
		String out;
		Merge20Utf8Decode(utf8, out);
		return out;
	}

	// This PC's source id ("u-<GUID>") from a GUID. StringFromGUID2
	// returns "{XXXXXXXX-...}" in upper case; the braces are dropped
	// and the hex is lowercased so the id looks like the HTML tool's
	// `u-<UUID>` (lower-case hex, no braces).
	String OwnerIdFromGuid(const GUID& guid)
	{
		wchar_t buf[39] = {};
		if (::StringFromGUID2(guid, buf, 39) == 0)
			return String();
		String text(buf);
		if (text.size() >= 2 && text.front() == _T('{') && text.back() == _T('}'))
			text = text.substr(1, text.size() - 2);
		return _T("u-") + strutils::makelower(text);
	}

	// Read the learning file into a fresh store. A missing file is
	// not an error (first run): the caller treats it as an empty
	// store. Returns false with errDetail set when the file exists
	// but cannot be read or parsed.
	bool LoadStoreFromFile(const String& filePath, Merge20LearnStore& out, std::string& errDetail, int& skippedRules)
	{
		skippedRules = 0;
		errDetail.clear();
		std::ifstream in(std::filesystem::path(filePath), std::ios::binary);
		if (!in)
		{
			errDetail = "cannot open learning file";
			return false;
		}
		std::string utf8((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (in.bad())
		{
			errDetail = "failed to read learning file";
			return false;
		}
		Merge20LearnFileHeader header;
		return Merge20LearnFromJson(utf8, kLearnFileName, out, header, errDetail, skippedRules);
	}

	// Write the store as learning JSON v2 (temporary file plus
	// rename, the same way as Merge20SessionSaveToFile in S8).
	bool SaveStoreToFile(const Merge20LearnStore& store, const String& filePath, std::string& errDetail)
	{
		errDetail.clear();
		const String exportedAt = String(CTime::GetCurrentTime().Format(_T("%Y-%m-%dT%H:%M:%S")).GetString());
		std::string utf8;
		if (!Merge20LearnToJson(store, store.GetOwnerId(), kLearnFileName, exportedAt, utf8))
		{
			errDetail = "failed to serialize learning data";
			return false;
		}
		std::filesystem::path target(filePath);
		// The ".tmp" suffix is ASCII, so building the temp path from
		// the String name is safe on both Windows (native wide path)
		// and POSIX (codecvt conversion of an ASCII name).
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
				errDetail = "failed to write learning file";
				std::error_code ec;
				std::filesystem::remove(tmp, ec);
				return false;
			}
		}
		std::error_code ec;
		std::filesystem::rename(tmp, target, ec);
		if (ec)
		{
			// Rename failed (e.g. a locked target): fall back to a
			// direct write so the save still has a chance to succeed.
			std::ofstream out(target, std::ios::binary | std::ios::trunc);
			if (!out)
			{
				errDetail = "cannot create learning file";
				std::filesystem::remove(tmp, ec);
				return false;
			}
			out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
			out.flush();
			if (!out)
			{
				errDetail = "failed to write learning file";
				std::filesystem::remove(tmp, ec);
				return false;
			}
			std::filesystem::remove(tmp, ec);
		}
		return true;
	}

} // namespace

String Merge20LearnGetDirectory()
{
	if (dynamic_cast<CIniOptionsMgr*>(GetOptionsMgr()) != nullptr)
		return paths::ConcatPath(env::GetProgPath(), _T("Merge20Learn"));
	return paths::ConcatPath(env::GetAppDataPath(), _T("WinMerge\\Merge20Learn"));
}

String Merge20LearnGetFilePath()
{
	return paths::ConcatPath(Merge20LearnGetDirectory(), kLearnFileName);
}

bool Merge20LearnIsEnabled()
{
	return GetOptionsMgr()->GetBool(OPT_MERGE20_LEARNING_ENABLED);
}

String Merge20LearnGetOwnerId()
{
	String ownerId = GetOptionsMgr()->GetString(OPT_MERGE20_LEARNING_OWNER);
	if (!ownerId.empty())
		return ownerId;
	GUID guid{};
	if (::CoCreateGuid(&guid) == S_OK)
		ownerId = OwnerIdFromGuid(guid);
	if (ownerId.empty())
	{
		// CoCreateGuid essentially never fails; fall back to a
		// time-based id so learning still has a stable source id
		// for this run (it is saved like the GUID one, so a later
		// GUID is only generated when the key is still empty).
		ownerId = _T("u-local-") + String(CTime::GetCurrentTime().Format(_T("%Y%m%d%H%M%S")).GetString());
	}
	GetOptionsMgr()->SaveOption(OPT_MERGE20_LEARNING_OWNER, ownerId);
	return ownerId;
}

String Merge20LearnGetOwnerDisplayName()
{
	return _("Learning on this PC (self)");
}

Merge20LearnStore& Merge20LearnGetStore()
{
	if (!g_learnStoreLoaded)
	{
		g_learnStoreLoaded = true;
		g_learnStore.SetOwnerId(Merge20LearnGetOwnerId());
		const String filePath = Merge20LearnGetFilePath();
		if (paths::DoesPathExist(filePath) == paths::IS_EXISTING_FILE)
		{
			std::string errDetail;
			int skippedRules = 0;
			Merge20LearnStore fresh;
			fresh.SetOwnerId(g_learnStore.GetOwnerId());
			if (!LoadStoreFromFile(filePath, fresh, errDetail, skippedRules))
			{
				// A broken file never blocks startup: keep the empty
				// store and remember the failure for the learning
				// dialog (L4), which reports it when opened.
				g_learnStoreLoadError = Utf8ToString(errDetail);
			}
			else
			{
				g_learnStore = std::move(fresh);
			}
		}
	}
	return g_learnStore;
}

bool Merge20LearnGetLoadError(String& errStr)
{
	// Touch the store so the lazy load has run before reporting.
	(void)Merge20LearnGetStore();
	errStr = g_learnStoreLoadError;
	return !errStr.empty();
}

bool Merge20LearnSaveStore(String& errStr)
{
	errStr.clear();
	const String dir = Merge20LearnGetDirectory();
	if (paths::EnsurePathExist(dir).empty())
	{
		errStr = strutils::format_string1(_("Cannot create the learning folder:\n%1"), dir);
		return false;
	}
	Merge20LearnStore& store = Merge20LearnGetStore();
	const String filePath = Merge20LearnGetFilePath();
	std::string errDetail;
	if (!SaveStoreToFile(store, filePath, errDetail))
	{
		errStr = strutils::format_string1(_("Cannot save the learning data:\n%1"), Utf8ToString(errDetail));
		return false;
	}
	return true;
}
