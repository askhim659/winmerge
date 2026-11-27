// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormTable.cpp
 *
 * @brief Implementation of the structured diff table rows (Wave
 * 5 F5, design sections 2.9, 5.2 and 5.3).
 *
 * This file stays ASCII: the Japanese row literals are written
 * with \u escapes (see the note in Merge20FormDiff.h). The
 * conversion from the authored UTF-8 text is mechanical; the
 * escaped form is what is committed.
 */
#include "pch.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include "Merge20/Merge20FormTable.h"

namespace
{

	// Tag part of a stable key (`structPath|tag|name`).
	std::wstring TagFromStableKey(const std::wstring& stableKey)
	{
		const size_t first = stableKey.find(L'|');
		if (first == std::wstring::npos)
			return stableKey;
		const size_t second = stableKey.find(L'|', first + 1);
		if (second == std::wstring::npos)
			return stableKey.substr(first + 1);
		return stableKey.substr(first + 1, second - first - 1);
	}


	// Amount without a sign (`12.5`, `12`): the direction word
	// already says which way the move goes.
	std::wstring FormatAmount(double value)
	{
		const double rounded = std::round(value * 10.0) / 10.0;
		if (rounded == std::trunc(rounded))
			return std::to_wstring(static_cast<long long>(rounded));
		wchar_t buf[64];
		std::swprintf(buf, 64, L"%.1f", rounded);
		return std::wstring(buf);
	}

	// Name part of a stable key (`structPath|tag|name`).
	std::wstring NameFromStableKey(const std::wstring& stableKey)
	{
		const size_t first = stableKey.find(L'|');
		if (first == std::wstring::npos)
			return std::wstring();
		const size_t second = stableKey.find(L'|', first + 1);
		if (second == std::wstring::npos)
			return std::wstring();
		return stableKey.substr(second + 1);
	}

	// Most frequent tag of a group's records (the same rule as
	// Merge20BuildGroupSummary, F1).
	std::wstring GroupTag(const Merge20FormFileResult& result,
		const Merge20FormDiffGroup& group)
	{
		std::map<std::wstring, int> tagCounts;
		for (size_t index : group.recordIndices)
		{
			if (index >= result.diffRecords.size())
				continue;
			++tagCounts[TagFromStableKey(result.diffRecords[index].element)];
		}
		std::wstring tag;
		int best = 0;
		for (const std::pair<const std::wstring, int>& entry : tagCounts)
		{
			if (entry.second > best)
			{
				best = entry.second;
				tag = entry.first;
			}
		}
		return tag;
	}

	// Move text of a group (`down 12 units`, `right 8 units`, ...).
	std::wstring GroupMoveText(const Merge20FormDiffGroup& group)
	{
		std::wstring text;
		if (group.dx > 0.0)
			text += L"\u53f3\u306b" + FormatAmount(group.dx) + L"\u5358\u4f4d";
		if (group.dx < 0.0)
			text += L"\u5de6\u306b" + FormatAmount(-group.dx) + L"\u5358\u4f4d";
		if (group.dy > 0.0)
		{
			if (!text.empty())
				text += L"\u30fb";
			text += L"\u4e0b\u306b" + FormatAmount(group.dy) + L"\u5358\u4f4d";
		}
		if (group.dy < 0.0)
		{
			if (!text.empty())
				text += L"\u30fb";
			text += L"\u4e0a\u306b" + FormatAmount(-group.dy) + L"\u5358\u4f4d";
		}
		return text;
	}

	// One display row for a single record (ungrouped, or a group
	// member). Child rows are indented with two spaces.
	Merge20FormTableRow BuildRecordRow(const Merge20FormFileResult& result,
		size_t recordIndex, bool child)
	{
		const Merge20FormDiffRecord& record = result.diffRecords[recordIndex];
		Merge20FormTableRow row;
		row.groupId = record.groupId;
		row.recordIndex = static_cast<int>(recordIndex);
		row.typeText = Merge20FormTableTypeText(record.type);
		std::wstring element = Merge20FormTableDisplayName(record.element);
		if (child)
			element = L"  " + element;
		if (!record.attr.empty())
			element += L" / " + record.attr;
		row.elementText = element;
		std::wstring value = record.oldVal + L" \u2192 " + record.newVal;
		if (record.type == Merge20FormDiffType::Coord)
		{
			double oldNum = 0.0;
			double newNum = 0.0;
			if (Merge20FormParseNumber(record.oldVal, oldNum) &&
				Merge20FormParseNumber(record.newVal, newNum) &&
				std::isfinite(oldNum) && std::isfinite(newNum))
			{
				value += L" (" + Merge20FormatFormDelta(newNum - oldNum) + L"\u5358\u4f4d)";
			}
		}
		row.valueText = value;
		row.descText = record.desc;
		if (!record.impact.empty())
			row.descText += L" / " + record.impact;
		return row;
	}

} // namespace

std::wstring Merge20FormTableTypeText(Merge20FormDiffType type)
{
	switch (type)
	{
	case Merge20FormDiffType::FileAdd:
		return L"FILE_ADD";
	case Merge20FormDiffType::FileDel:
		return L"FILE_DEL";
	case Merge20FormDiffType::Error:
		return L"ERROR";
	case Merge20FormDiffType::Add:
		return L"ADD";
	case Merge20FormDiffType::Del:
		return L"DEL";
	case Merge20FormDiffType::Coord:
		return L"COORD";
	case Merge20FormDiffType::Mod:
		return L"MOD";
	case Merge20FormDiffType::Moved:
		return L"MOVED";
	case Merge20FormDiffType::Renamed:
		return L"RENAMED";
	default:
		return L"MOD";
	}
}

std::wstring Merge20FormTableDisplayName(const std::wstring& stableKey)
{
	const std::wstring name = NameFromStableKey(stableKey);
	// An attribute signature (it contains `=`) is not a name.
	if (!name.empty() && name.find(L'=') == std::wstring::npos)
		return name;
	const std::wstring tag = TagFromStableKey(stableKey);
	if (!tag.empty())
		return tag;
	return stableKey;
}

std::vector<Merge20FormTableRow> Merge20BuildFormDiffTableRows(
	const Merge20FormFileResult& result,
	const std::vector<Merge20FormDiffGroup>& groups,
	const std::set<std::wstring>& expandedGroupIds)
{
	// Groups by id, and the record index that first opens each
	// group (the group row sits there).
	std::map<std::wstring, const Merge20FormDiffGroup*> groupById;
	for (const Merge20FormDiffGroup& group : groups)
		groupById[group.id] = &group;
	std::map<std::wstring, bool> groupEmitted;
	std::vector<Merge20FormTableRow> rows;
	for (size_t i = 0; i < result.diffRecords.size(); ++i)
	{
		const Merge20FormDiffRecord& record = result.diffRecords[i];
		const std::map<std::wstring, const Merge20FormDiffGroup*>::const_iterator found =
			record.groupId.empty() ? groupById.end() : groupById.find(record.groupId);
		if (found == groupById.end())
		{
			rows.push_back(BuildRecordRow(result, i, false));
			continue;
		}
		const Merge20FormDiffGroup& group = *found->second;
		if (groupEmitted[group.id])
			continue;
		groupEmitted[group.id] = true;
		const bool expanded = expandedGroupIds.find(group.id) != expandedGroupIds.end();
		Merge20FormTableRow groupRow;
		groupRow.isGroup = true;
		groupRow.groupId = group.id;
		groupRow.expanded = expanded;
		groupRow.minor = group.minor;
		const std::wstring count = std::to_wstring(group.recordIndices.size());
		groupRow.typeText = group.minor ?
			(expanded ? L"\u25be \u5fae\u5c0f\u306a\u5dee" : L"\u25b8 \u5fae\u5c0f\u306a\u5dee") :
			(expanded ? L"\u25be \u30b0\u30eb\u30fc\u30d7" : L"\u25b8 \u30b0\u30eb\u30fc\u30d7");
		const std::wstring tag = GroupTag(result, group);
		groupRow.elementText = tag.empty() ?
			(count + L"\u4ef6") : (tag + L" " + count + L"\u9805\u76ee");
		groupRow.valueText = group.minor ? std::wstring() : GroupMoveText(group);
		groupRow.descText = group.summary;
		rows.push_back(groupRow);
		if (expanded)
		{
			for (size_t memberIndex : group.recordIndices)
			{
				if (memberIndex >= result.diffRecords.size())
					continue;
				rows.push_back(BuildRecordRow(result, memberIndex, true));
			}
		}
	}
	return rows;
}
