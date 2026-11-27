// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormMaster.cpp
 *
 * @brief Implementation of the FORM name master (Wave 5 F6,
 * design sections 2.10, 8 and 10 F6).
 *
 * This file stays ASCII: the Japanese literals are written with
 * \u escapes (see the note in Merge20FormMaster.h). The
 * conversion from the authored UTF-8 text is mechanical; the
 * escaped form is what is committed.
 */
#include "pch.h"
#include <algorithm>
#include <set>
#include <utility>
#include "Merge20/Merge20FormMaster.h"

namespace
{

	// Trim spaces, tabs and carriage returns off both ends.
	std::wstring MasterTrim(const std::wstring& text)
	{
		const std::wstring blanks = L" \t\r\n";
		const size_t first = text.find_first_not_of(blanks);
		if (first == std::wstring::npos)
			return std::wstring();
		const size_t last = text.find_last_not_of(blanks);
		return text.substr(first, last - first + 1);
	}

	// Split the master into (id -> names) groups in first-seen
	// id order, keeping each id's names in file order.
	// Returns the groups: (id, names).
	std::vector<std::pair<std::wstring, std::vector<std::wstring>>> MasterGroupById(
		const Merge20FormMaster& master)
	{
		std::vector<std::pair<std::wstring, std::vector<std::wstring>>> groups;
		for (const Merge20FormMasterEntry& entry : master.entries)
		{
			auto it = std::find_if(groups.begin(), groups.end(),
				[&entry](const std::pair<std::wstring, std::vector<std::wstring>>& group)
				{ return group.first == entry.id; });
			if (it == groups.end())
				groups.emplace_back(entry.id, std::vector<std::wstring>{ entry.name });
			else
				it->second.push_back(entry.name);
		}
		return groups;
	}

	// Format one id's names the HTML way (design section 2.10):
	// up to 3 names joined with " / ", 4 or more as
	// "first name + N more kinds".
	std::wstring MasterFormatNames(const std::vector<std::wstring>& names)
	{
		if (names.empty())
			return Merge20FormMasterUnregistered();
		if (names.size() > 3)
		{
			return names[0] + L" \u307b\u304b\u5168" + std::to_wstring(names.size()) + L"\u7a2e";
		}
		std::wstring out;
		for (size_t i = 0; i < names.size(); ++i)
		{
			if (i > 0)
				out += L" \uff0f ";
			out += names[i];
		}
		return out;
	}

	// The default master (design appendix A, 171 rows, 142 unique
	// ids), verified against the HTML `defaultMasterData` (same
	// rows, same order).
	const wchar_t* const kDefaultMaster[][2] =
	{
		{ L"ZEB001R007", L"\u8cc7\u7523\u8a3c\u660e\u66f8" },
		{ L"ZEB001R008", L"\u516c\u8ab2\u8a3c\u660e\u66f8\uff08\u511f\u5374\u8cc7\u7523\uff09" },
		{ L"ZEB001R010", L"\u7121\u8cc7\u7523\u8a3c\u660e\u66f8" },
		{ L"ZEB001R001", L"\u8a55\u4fa1\u8a3c\u660e\u66f8" },
		{ L"ZEB001R002", L"\u516c\u8ab2\u8a3c\u660e\u66f8\uff08\u8ab2\u7a0e\u8a3c\u660e\u66f8\uff09" },
		{ L"ZEB001R004", L"\u6240\u6709\u8a3c\u660e\u66f8" },
		{ L"ZEB001R005", L"\u5bb6\u5c4b\u6ec5\u5931\u8a3c\u660e\u66f8" },
		{ L"ZEB001R006", L"\u540d\u5bc4\u5e33" },
		{ L"ZEB001R006", L"\u540d\u5bc4\u5e33\uff08\u7269\u4ef6\u9078\u629e\uff09" },
		{ L"ZEB001R011", L"\u511f\u5374\u8cc7\u7523\u8a3c\u660e\u66f8" },
		{ L"ZEB001R012", L"\u8a55\u4fa1\u8a3c\u660e\u66f8\uff08\u511f\u5374\u8cc7\u7523\uff09" },
		{ L"ZEB001R013", L"\u571f\u5730\uff08\u88dc\u5145\uff09\u8ab2\u7a0e\u53f0\u5e33\uff08\u95b2\u89a7\u7528\uff09" },
		{ L"ZEB001R013", L"\u571f\u5730\uff08\u88dc\u5145\uff09\u8ab2\u7a0e\u53f0\u5e33\uff08\u5185\u90e8\u7528\uff09" },
		{ L"ZEB001R015", L"\u5bb6\u5c4b\uff08\u88dc\u5145\uff09\u8ab2\u7a0e\u53f0\u5e33\uff08\u95b2\u89a7\u7528\uff09" },
		{ L"ZEB001R015", L"\u5bb6\u5c4b\uff08\u88dc\u5145\uff09\u8ab2\u7a0e\u53f0\u5e33\uff08\u5185\u90e8\u7528\uff09" },
		{ L"ZEB001R006", L"\u540d\u5bc4\u5e33\uff08\u5c65\u6b74\uff09" },
		{ L"ZEB002R005", L"\u7d0d\u7a0e\u901a\u77e5\u66f8\u5b9b\u540d\u90e8\u5206" },
		{ L"ZEB002R006", L"\u7d0d\u7a0e\u901a\u77e5\u66f8\u8ab2\u7a0e\u6839\u62e0\u90e8\u5206" },
		{ L"ZEB002R010", L"\u7d0d\u4ed8\u66f8" },
		{ L"ZEB002R011", L"\u90f5\u4fbf\u632f\u8fbc\u7968" },
		{ L"ZEB002R013", L"\u8ab2\u7a0e\u660e\u7d30\u66f8" },
		{ L"ZEB002R014", L"\u66f4\u6b63\u6c7a\u5b9a\u901a\u77e5\u66f8" },
		{ L"ZEB433R001", L"\u4fa1\u683c\u6c7a\u5b9a\u901a\u77e5\u66f8" },
		{ L"ZEB433R002", L"\u4fa1\u683c\u6c7a\u5b9a\u6c7a\u8b70\u66f8" },
		{ L"ZEB433R001", L"\u4fa1\u683c\u6c7a\u5b9a\u901a\u77e5\u66f8_\u5909\u66f4\u767a\u884c" },
		{ L"ZEB433R002", L"\u4fa1\u683c\u6c7a\u5b9a\u6c7a\u8b70\u66f8_\u5909\u66f4\u767a\u884c" },
		{ L"ZEB002R017", L"\u7d0d\u7a0e\u901a\u77e5\u66f8_A4" },
		{ L"ZEB002R018", L"\u8ab2\u7a0e\u660e\u7d30\u66f8_A4" },
		{ L"ZEB002R005", L"\u7d0d\u7a0e\u901a\u77e5\u66f8\u5b9b\u540d\u90e8\u5206\uff08\u4e00\u62ec\u767a\u884c\uff09" },
		{ L"ZEB002R006", L"\u7d0d\u7a0e\u901a\u77e5\u66f8\u8ab2\u7a0e\u6839\u62e0\u90e8\u5206\uff08\u4e00\u62ec\u767a\u884c\uff09" },
		{ L"ZEB002R010", L"\u7d0d\u4ed8\u66f8\uff08\u4e00\u62ec\u767a\u884c\uff09" },
		{ L"ZEB002R013", L"\u8ab2\u7a0e\u660e\u7d30\u66f8\uff08\u4e00\u62ec\u767a\u884c\uff09" },
		{ L"ZEB003R001", L"\u30a2\u30c3\u30d7\u30ed\u30fc\u30c9\u30a8\u30e9\u30fc\u30ea\u30b9\u30c8" },
		{ L"ZEB003R002", L"GIS\u9023\u643a\u30a8\u30e9\u30fc\u30ea\u30b9\u30c8" },
		{ L"ZEB003R003", L"\u5bb6\u5c4b\u53f0\u5e33" },
		{ L"ZEB003R004", L"\u571f\u5730\u8ab2\u7a0e\u6a19\u6e96\u984d\u8a08\u7b97\u904e\u7a0b" },
		{ L"ZEB107R001", L"\u5b9a\u671f\u8ab2\u7a0e\u8abf\u5b9a\u8868" },
		{ L"ZEB108R001", L"\u571f\u5730\u7e26\u89a7\u5e33\u7c3f" },
		{ L"ZEB108R002", L"\u571f\u5730\u7e26\u89a7\u5e33\u7c3f" },
		{ L"ZEB109R001", L"\u5bb6\u5c4b\u7e26\u89a7\u5e33\u7c3f" },
		{ L"ZEB109R002", L"\u5bb6\u5c4b\u7e26\u89a7\u5e33\u7c3f" },
		{ L"ZEB203R001", L"\u90f5\u4fbf\u632f\u8fbc\u7968" },
		{ L"ZEB203R002", L"\u7d0d\u4ed8\u66f8\u767a\u9001\u7c3f" },
		{ L"ZEB204R003", L"\u56fa\u5b9a\u8cc7\u7523\u7a0e\uff3f\u30d6\u30c3\u30ad\u30f3\u30b0\u7d0d\u4ed8\u66f8\uff08\u53e3\u5ea7\u5916\uff09" },
		{ L"ZEB204R003", L"\u56fa\u5b9a\u8cc7\u7523\u7a0e\uff3f\u30d6\u30c3\u30ad\u30f3\u30b0\u7d0d\u4ed8\u66f8\uff08\u53e3\u5ea7\u5916\u4f59\u308a\uff09" },
		{ L"ZEB204R003", L"\u56fa\u5b9a\u8cc7\u7523\u7a0e\uff3f\u30d6\u30c3\u30ad\u30f3\u30b0\u7d0d\u4ed8\u66f8\uff08\u53e3\u5ea7\uff09" },
		{ L"ZEB204R003", L"\u56fa\u5b9a\u8cc7\u7523\u7a0e\uff3f\u30d6\u30c3\u30ad\u30f3\u30b0\u7d0d\u4ed8\u66f8\uff08\u53e3\u5ea7\u4f59\u308a\uff09" },
		{ L"ZEB204R007", L"\u7d0d\u4ed8\u66f8\uff08\u4e00\u822c\uff09" },
		{ L"ZEB204R007", L"\u7d0d\u4ed8\u66f8\uff08\u4e00\u822c\u4f59\u308a\uff09" },
		{ L"ZEB204R009", L"\u7d0d\u7a0e\u901a\u77e5\u66f8\uff08\u4e00\u822c\uff09" },
		{ L"ZEB204R009", L"\u7d0d\u7a0e\u901a\u77e5\u66f8\uff08\u4e00\u822c\u4f59\u308a\uff09" },
		{ L"ZEB204R009", L"\u7d0d\u7a0e\u901a\u77e5\u66f8\uff08\u53e3\u5ea7\uff09" },
		{ L"ZEB204R009", L"\u7d0d\u7a0e\u901a\u77e5\u66f8\uff08\u53e3\u5ea7\u4f59\u308a\uff09" },
		{ L"ZEB204R028", L"\u5171\u6709\u7d0d\u7a0e\u901a\u77e5\u66f8" },
		{ L"ZEB204R028", L"\u5171\u6709\u7d0d\u7a0e\u901a\u77e5\u66f8\uff08\u4f59\u308a\uff09" },
		{ L"ZEB204R021", L"\u5225\u9001\u7528\u8ab2\u7a0e\u660e\u7d30\u66f8\uff08\u53e3\u5ea7\u5916\uff09" },
		{ L"ZEB204R021", L"\u5225\u9001\u7528\u8ab2\u7a0e\u660e\u7d30\u66f8\uff08\u53e3\u5ea7\uff09" },
		{ L"ZEB204R021", L"\u5225\u9001\u7528\u8ab2\u7a0e\u660e\u7d30\u66f8\uff08\u5171\u6709\uff09" },
		{ L"ZEB302R001", L"\u4e00\u62ec\u540d\u5bc4\u5e33(1\uff5e20000)" },
		{ L"ZEB302R001", L"\u4e00\u62ec\u540d\u5bc4\u5e33(20001\uff5e40000)" },
		{ L"ZEB302R001", L"\u4e00\u62ec\u540d\u5bc4\u5e33(40001\uff5e60000)" },
		{ L"ZEB302R001", L"\u4e00\u62ec\u540d\u5bc4\u5e33(60001\uff5e80000)" },
		{ L"ZEB302R001", L"\u4e00\u62ec\u540d\u5bc4\u5e33(80001\uff5e100000)" },
		{ L"ZEB302R001", L"\u4e00\u62ec\u540d\u5bc4\u5e33(100001\uff5e120000)" },
		{ L"ZEB302R001", L"\u4e00\u62ec\u540d\u5bc4\u5e33(120001\uff5e140000)" },
		{ L"ZEB302R001", L"\u4e00\u62ec\u540d\u5bc4\u5e33(140001\uff5e160000)" },
		{ L"ZEB302R001", L"\u4e00\u62ec\u540d\u5bc4\u5e33(160001\uff5e180000)" },
		{ L"ZEB302R001", L"\u4e00\u62ec\u540d\u5bc4\u5e33(180001\uff5e200000)" },
		{ L"ZEB303R001", L"\u540d\u5bc4\u5e33\u898b\u51fa\u3057\u7c3f" },
		{ L"ZEB304R001", L"\u511f\u5374\u8cc7\u7523\u8ab2\u7a0e\u53f0\u5e33" },
		{ L"ZEB304R002", L"\u511f\u5374\u8cc7\u7523\u7533\u544a\u66f8" },
		{ L"ZEB304R003", L"\u7a2e\u985e\u6bce\u660e\u7d30\u66f8" },
		{ L"ZEB304R004", L"\u7a2e\u985e\u6bce\u660e\u7d30\u66f8" },
		{ L"ZEB304R005", L"\u53ce\u767a\u7c3f" },
		{ L"ZEB304R006", L"\u7a2e\u985e\u5225\u660e\u7d30\u66f8\uff08\u5897\u6e1b\u8cc7\u7523\u7528\uff09" },
		{ L"ZEB304R007", L"\u511f\u5374\u7533\u544a\u6848\u5185" },
		{ L"ZEB305R001", L"\u7fa9\u52d9\u8005\u4e00\u89a7\u8868" },
		{ L"ZEB306R001", L"\u511f\u5374\u8cc7\u7523\u50ac\u544a\u66f8" },
		{ L"ZEB307R003", L"\uff38\uff2d\uff2c\u60c5\u5831\u78ba\u8a8d\u30ea\u30b9\u30c8" },
		{ L"ZEB308R001", L"\u65b0\u898f\u8ab2\u7a0e\u5bfe\u8c61\u8005\u4e00\u89a7" },
		{ L"ZEB309R001", L"\u8ab2\u7a0e\u5bfe\u8c61\u8005\u8ee2\u51fa\u6b7b\u4ea1\u4e00\u89a7" },
		{ L"ZEB309R002", L"\u5b9b\u540d\u30ef\u30c3\u30da\u30f3" },
		{ L"ZEB310R001", L"\u8ee2\u5165\u8005\u4e00\u89a7\u8868\uff08\u7fa9\u52d9\u8005\uff09" },
		{ L"ZEB310R002", L"\u8ee2\u5165\u8005\u4e00\u89a7\u8868\uff08\u7ba1\u7406\u4eba\uff09" },
		{ L"ZEB311R001", L"\u5171\u6709\u69cb\u6210\u54e1\u8cc7\u7523\u5272\u7a0e\u984d\u30ea\u30b9\u30c8" },
		{ L"ZEB312R001", L"\u56fd\u4fdd\u7528\u7a0e\u984d\u66f4\u6b63\u4e00\u89a7\u8868" },
		{ L"ZEB313R001", L"\u8ab2\u6a19\u4e0d\u4e00\u81f4\u5206\u78ba\u8a8d\u30ea\u30b9\u30c8" },
		{ L"ZEB316R001", L"\u4e8b\u7531\u5225\u7570\u52d5\u8abf\u3079" },
		{ L"ZEB316R002", L"\u4e8b\u7531\u5225\u7570\u52d5\u8abf\u3079(\u88dc\u8db3)" },
		{ L"ZEB316R003", L"\u4e8b\u7531\u5225\u7570\u52d5\u8abf\u3079\u660e\u7d30" },
		{ L"ZEB317R002", L"\u571f\u5730\u8ab2\u7a0e\u53f0\u5e33\u517c\u8a55\u4fa1\u8abf\u66f8" },
		{ L"ZEB317R004", L"\u571f\u5730\u8ab2\u7a0e\u53f0\u5e33\u517c\u8a55\u4fa1\u8abf\u66f8" },
		{ L"ZEB317R006", L"\u571f\u5730\u8ab2\u7a0e\u53f0\u5e33\u517c\u8a55\u4fa1\u8abf\u66f8" },
		{ L"ZEB318R002", L"\u5bb6\u5c4b\u4e00\u68df\u8a55\u4fa1\u8abf\u66f8" },
		{ L"ZEB318R004", L"\u5bb6\u5c4b\u8ab2\u7a0e\u53f0\u5e33\u517c\u8a55\u4fa1\u8abf\u66f8" },
		{ L"ZEB318R006", L"\u5bb6\u5c4b\u4e00\u68df\u8a55\u4fa1\u8abf\u66f8" },
		{ L"ZEB319R002", L"\u571f\u5730\u8ab2\u7a0e\u53f0\u5e33" },
		{ L"ZEB320R002", L"\u5bb6\u5c4b\u8ab2\u7a0e\u53f0\u5e33" },
		{ L"ZEB322R001", L"\u571f\u5730\u6cd5\u52d9\u5c40\u9023\u7d61\u8868" },
		{ L"ZEB325R001", L"\u5bb6\u5c4b\u6cd5\u52d9\u5c40\u9023\u7d61\u8868" },
		{ L"ZEB327R001", L"\u524d\u5e74\u7a0e\u984d\u6bd4\u8f03\u4e00\u89a7\u8868" },
		{ L"ZEB328R001", L"\u571f\u5730\u8ab2\u7a0e\u8a08\u7b97\u7d50\u679c\u4e00\u89a7\u8868" },
		{ L"ZEB330R002", L"\u6e1b\u514d\u901a\u77e5\u66f8" },
		{ L"ZEB331R001", L"\u571f\u5730\u6240\u6709\u72b6\u6cc1\u8abf\u67fb" },
		{ L"ZEB331R002", L"\u5bb6\u5c4b\u6240\u6709\u72b6\u6cc1\u8abf\u67fb" },
		{ L"ZEB332R001", L"\u511f\u5374\u8cc7\u7523\u8a55\u4fa1\u8abf\u66f8" },
		{ L"ZEB334R001", L"\u4f4f\u767b\u5916\u8cc7\u7523\u5272\u7a0e\u984d\u30ea\u30b9\u30c8" },
		{ L"ZEB335R001", L"\u56fd\u4fdd\u7528\u7a0e\u984d\u66f4\u65b0\u5bfe\u8c61\u30ea\u30b9\u30c8" },
		{ L"ZEB401R001", L"\u571f\u5730\u4e00\u7b46\u30ea\u30b9\u30c8" },
		{ L"ZEB402R001", L"\u5bb6\u5c4b\u4e00\u68df\u30ea\u30b9\u30c8" },
		{ L"ZEB403R001", L"\u571f\u5730\u7570\u52d5\u4e00\u89a7" },
		{ L"ZEB404R001", L"\u5bb6\u5c4b\u7570\u52d5\u4e00\u89a7" },
		{ L"ZEB405R001", L"\u571f\u5730\u30a8\u30e9\u30fc\u30ea\u30b9\u30c8" },
		{ L"ZEB406R001", L"\u5bb6\u5c4b\u30a8\u30e9\u30fc\u30ea\u30b9\u30c8" },
		{ L"ZEB407R001", L"\u511f\u5374\u8cc7\u7523\u30a8\u30e9\u30fc\u30ea\u30b9\u30c8" },
		{ L"ZEB408R001", L"\u571f\u5730\u5206\u5408\u7b46\u4e00\u89a7\u8868" },
		{ L"ZEB409R001", L"\u511f\u5374\u8cc7\u7523\u78ba\u8a8d\u7528\u30ea\u30b9\u30c8" },
		{ L"ZEB410R001", L"\u511f\u5374\u8cc7\u7523\u7570\u52d5\u4e00\u54c1\u30ea\u30b9\u30c8" },
		{ L"ZEB410R002", L"\u511f\u5374\u8cc7\u7523\u7570\u52d5\u4e00\u54c1\u30ea\u30b9\u30c8\uff08\u7570\u52d5\u524d\u5f8c\uff09" },
		{ L"ZEB411R001", L"\u571f\u5730\u672a\u5165\u529b\u4e00\u89a7\u8868" },
		{ L"ZEB412R001", L"\u5bb6\u5c4b\u672a\u5165\u529b\u4e00\u89a7\u8868" },
		{ L"ZEB413R002", L"\u5171\u6709\u8005\u5165\u529b\u8868" },
		{ L"ZEB413R003", L"\u6240\u6709\u6a29\u79fb\u8ee2\u7d50\u679c\u4e00\u89a7\u8868" },
		{ L"ZEB414R003", L"\u66f4\u6b63\u6c7a\u5b9a\u901a\u77e5\u66f8" },
		{ L"ZEB414R002", L"\u66f4\u6b63\u6c7a\u5b9a\u6c7a\u8b70\u66f8\uff08\u4f3a\uff09" },
		{ L"ZEB414R001", L"\u7a0e\u984d\u5909\u66f4\u5bfe\u8c61\u30ea\u30b9\u30c8" },
		{ L"ZEB415R001", L"\u968f\u6642\u8abf\u5b9a\u8868" },
		{ L"ZEB416R001", L"\u5171\u6709\u8005\u53f0\u5e33" },
		{ L"ZEB417R001", L"\u7ba1\u7406\u4eba\u4e00\u89a7\u8868" },
		{ L"ZEB418R001", L"\u6b7b\u4ea1\u8ee2\u51fa\u8005\u30ea\u30b9\u30c8\uff08\u7fa9\u52d9\u8005\uff09" },
		{ L"ZEB418R002", L"\u6b7b\u4ea1\u8ee2\u51fa\u8005\u30ea\u30b9\u30c8\uff08\u7ba1\u7406\u4eba\uff09" },
		{ L"ZEB419R001", L"\u4f4f\u5b85\u7528\u5730\u78ba\u8a8d\u7528\u4e00\u89a7\u8868" },
		{ L"ZEB420R001", L"\u4e00\u753b\u5730\u4f4f\u5b85\u7528\u5730\u4e00\u89a7\u8868" },
		{ L"ZEB421R001", L"\u5171\u6709\u540d\u7fa9\u7528\u571f\u5730\u4e00\u7b46\u30ea\u30b9\u30c8" },
		{ L"ZEB422R001", L"\u6240\u5728\u5730\u5730\u76ee\u6bce\u7d71\u8a08\u8868" },
		{ L"ZEB424R001", L"\u4ed6\u8ab2\u7528\u4e00\u7b46\u30ea\u30b9\u30c8" },
		{ L"ZEB426R001", L"\u5bb6\u5c4b\u8efd\u6e1b\u5bfe\u8c61\u4e00\u89a7\u8868" },
		{ L"ZEB427R001", L"\u65b0\u7bc9\u8efd\u6e1b\u5207\u308c\u30ea\u30b9\u30c8" },
		{ L"ZEB428R001", L"\u6ec5\u5931\u5206\u4e00\u68df\u30ea\u30b9\u30c8" },
		{ L"ZEB429R002", L"\u4e0d\u52d5\u7523\u53d6\u5f97\u7a0e\u5165\u529b\u8868" },
		{ L"ZEB430R001", L"\u69cb\u9020\u7a2e\u985e\u6bce\u7d71\u8a08\u8868" },
		{ L"ZEB431R001", L"\u511f\u5374\u8cc7\u7523\u7279\u4f8b\u30ea\u30b9\u30c8" },
		{ L"ZEB432R001", L"\u7279\u4f8b\u4e8b\u696d\u6240\u5bfe\u8c61\u30ea\u30b9\u30c8" },
		{ L"ZEB434R001", L"\u8ce6\u8ab2\u66f4\u6b63\u5bfe\u8c61\u4e00\u89a7" },
		{ L"ZEB437R001", L"\u5171\u6709\u8005\u6c0f\u540d\u8868" },
		{ L"ZEB507R001", L"\u753b\u5730\u8a08\u7b97\u30c1\u30a7\u30c3\u30af\u30ea\u30b9\u30c8" },
		{ L"ZEB509R001", L"\u571f\u5730\u8a55\u4fa1\u66ff\u30a8\u30e9\u30fc\u30ea\u30b9\u30c8" },
		{ L"ZEB511R001", L"\u571f\u5730\u57fa\u790e\u8abf\u67fb\u8868\u53c2\u8003\u8cc7\u6599" },
		{ L"ZEB515R001", L"\u5bb6\u5c4b\u8a55\u4fa1\u66ff\u30a8\u30e9\u30fc\u30ea\u30b9\u30c8" },
		{ L"ZEB516R001", L"\u5bb6\u5c4b\u8a55\u4fa1\u66ff\u7d50\u679c\u78ba\u8a8d\u30ea\u30b9\u30c8" },
		{ L"ZEB517R001", L"\u5bb6\u5c4b\u8a55\u4fa1\u66ff\u96c6\u8a08\u8868" },
		{ L"ZEB517R002", L"\u5bb6\u5c4b\u8a55\u4fa1\u66ff\uff13\u5e74\u6bce\u68df\u6570\u8abf\u3079" },
		{ L"ZEB518R001", L"\u5bb6\u5c4b\u8a55\u4fa1\u66ff\u898b\u8fbc\u984d\u8abf\u3079" },
		{ L"ZEB519R001", L"\u5bb6\u5c4b\u8a55\u4fa1\u66ff\u7406\u8ad6\u8a55\u4fa1\u984d\u8a08\u7b97\u66f8" },
		{ L"ZEB520R001", L"\u5bb6\u5c4b\u8a55\u4fa1\u66ff\u6539\u7bc9\u5206\u78ba\u8a8d\u30ea\u30b9\u30c8" },
		{ L"ZEB601R001", L"\u4e0d\u52d5\u7523\u9451\u5b9a\u6240\u30c7\u30fc\u30bf\u53d6\u308a\u8fbc\u307f\u4e0d\u80fd\u30ea\u30b9\u30c8" },
		{ L"ZEB602R001", L"\u5bb6\u5c4b\u8a55\u4fa1\u53d6\u308a\u8fbc\u307f\u78ba\u8a8d\u30ea\u30b9\u30c8" },
		{ L"ZEB603R001", L"\u511f\u5374\u8cc7\u7523\u7533\u544a\u53d6\u308a\u8fbc\u307f\u30a8\u30e9\u30fc\u30ea\u30b9\u30c8\uff08eLTAX\uff09" },
		{ L"ZEB603R002", L"\u511f\u5374\u8cc7\u7523\u660e\u7d30\u53d6\u308a\u8fbc\u307f\u30a8\u30e9\u30fc\u30ea\u30b9\u30c8\uff08eLTAX\uff09" },
		{ L"ZEB607R001", L"\u6cd5\u52d9\u5c40\u30c7\u30fc\u30bf\u78ba\u8a8d\u30c1\u30a7\u30c3\u30af\u30ea\u30b9\u30c8" },
		{ L"ZEB609R001", L"\u6cd5\u52d9\u5c40\u30c7\u30fc\u30bf\u78ba\u8a8d\u30c1\u30a7\u30c3\u30af\u30ea\u30b9\u30c8\u3010\u5bb6\u5c4b\u3011" },
		{ L"ZEB610R001", L"\u66f4\u65b0\u5bfe\u8c61\u5bb6\u5c4b\u30ea\u30b9\u30c8" },
		{ L"ZEB802R001", L"\u5b9b\u540d\u540c\u5b9a\u78ba\u8a8d\u30ea\u30b9\u30c8" },
		{ L"ZEB902R001", L"\u6708\u5225\u660e\u7d30\u7968\uff08\u571f\u5730\uff09" },
		{ L"ZEB902R002", L"\u6708\u5225\u96c6\u8a08\u8868\uff08\u571f\u5730\uff09" },
		{ L"ZEB903R001", L"\u6708\u5225\u660e\u7d30\u7968\uff08\u5bb6\u5c4b\uff09" },
		{ L"ZEB903R002", L"\u6708\u5225\u96c6\u8a08\u8868\uff08\u5bb6\u5c4b\uff09" },
		{ L"ZEB904R001", L"\u6708\u5225\u660e\u7d30\u7968\uff08\u511f\u5374\uff09" },
		{ L"ZEB904R002", L"\u6708\u5225\u96c6\u8a08\u8868\uff08\u511f\u5374\uff09" },
		{ L"ZEB905R001", L"\u6708\u5225\u660e\u7d30\u7968\uff08\u8abf\u5b9a\uff09" },
		{ L"ZEB905R002", L"\u6708\u5225\u96c6\u8a08\u8868\uff08\u8abf\u5b9a\uff09" },
	};

} // namespace

bool Merge20FormMasterParseText(const std::wstring& text,
	Merge20FormMaster& out, int& badLine)
{
	out.entries.clear();
	badLine = 0;
	std::set<std::pair<std::wstring, std::wstring>> seen;
	size_t pos = 0;
	int lineNo = 0;
	while (pos <= text.size())
	{
		const size_t eol = text.find(L'\n', pos);
		const std::wstring line = (eol == std::wstring::npos)
			? text.substr(pos) : text.substr(pos, eol - pos);
		++lineNo;
		pos = (eol == std::wstring::npos) ? text.size() + 1 : eol + 1;
		const std::wstring trimmed = MasterTrim(line);
		if (trimmed.empty())
			continue;
		// A line without a tab is an error (design section 8.2):
		// the caller warns and aborts the save.
		const size_t tab = line.find(L'\t');
		if (tab == std::wstring::npos)
		{
			out.entries.clear();
			badLine = lineNo;
			return false;
		}
		const std::wstring id = MasterTrim(line.substr(0, tab));
		std::wstring name = line.substr(tab + 1);
		// Any further tab inside the name is treated like the
		// HTML tool's split-and-join: a space separator.
		std::replace(name.begin(), name.end(), L'\t', L' ');
		name = MasterTrim(name);
		if (id.empty() || name.empty())
		{
			out.entries.clear();
			badLine = lineNo;
			return false;
		}
		// A duplicated (id, name) row is removed (first
		// occurrence kept); a different name for the same id is
		// not a duplicate (design section 8.2).
		if (!seen.insert(std::make_pair(id, name)).second)
			continue;
		out.entries.push_back(Merge20FormMasterEntry{ id, name });
	}
	return true;
}

std::wstring Merge20FormMasterToText(const Merge20FormMaster& master)
{
	std::wstring out;
	for (const Merge20FormMasterEntry& entry : master.entries)
	{
		out += entry.id;
		out += L'\t';
		out += entry.name;
		out += L"\r\n";
	}
	return out;
}

Merge20FormMaster Merge20FormMasterGetDefault()
{
	Merge20FormMaster master;
	for (const auto& row : kDefaultMaster)
		master.entries.push_back(Merge20FormMasterEntry{ row[0], row[1] });
	return master;
}

std::wstring Merge20FormMasterResolve(const Merge20FormMaster& master,
	const std::wstring& fileName)
{
	// Base name: the file part without its directory and without
	// its extension (HTML `getFormName`: split on '/', strip
	// /\.[^/.]+$/). Backslash is a directory separator too.
	size_t start = fileName.find_last_of(L"/\\");
	const std::wstring baseWithExt = (start == std::wstring::npos)
		? fileName : fileName.substr(start + 1);
	const size_t dot = baseWithExt.find_last_of(L'.');
	const std::wstring base = (dot != std::wstring::npos && dot > 0)
		? baseWithExt.substr(0, dot) : baseWithExt;
	const std::vector<std::pair<std::wstring, std::vector<std::wstring>>> groups =
		MasterGroupById(master);
	for (const auto& group : groups)
	{
		if (group.first == base)
			return MasterFormatNames(group.second);
	}
	for (const auto& group : groups)
	{
		if (base.find(group.first) != std::wstring::npos)
			return MasterFormatNames(group.second);
	}
	return Merge20FormMasterUnregistered();
}

int Merge20FormMasterCountUniqueIds(const Merge20FormMaster& master)
{
	std::set<std::wstring> ids;
	for (const Merge20FormMasterEntry& entry : master.entries)
		ids.insert(entry.id);
	return static_cast<int>(ids.size());
}

std::wstring Merge20FormMasterUnregistered()
{
	return L"\uff08\u540d\u79f0\u672a\u767b\u9332\uff09";
}
