// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20FormCsv.cpp
 *
 * @brief Implementation of the FORM compare CSV row building
 * (Wave 5 F7, design sections 9 and 10 F7).
 *
 * This file stays ASCII: the Japanese literals are written with
 * \u escapes (see the note in Merge20FormDiff.h). The conversion
 * from the authored UTF-8 text is mechanical; the escaped form
 * is what is committed.
 *
 * Column layout (design section 9; the HTML `exportToExcel`
 * columns plus the F7 additions):
 * - Part 1 (summary list, 9 columns): No, warning, file name,
 * form name, change tendency, numeric change total (integer),
 * change count, severity, one-line summary.
 * - Part 2 (detail list, 14 columns): No, file name, form name,
 * warning, change type, target element, changed attribute,
 * old value, new value, numeric change (integer), concrete
 * change, impact analysis, group, minor.
 * The title line is the HTML one (report title + output
 * date/time). The warning words are the HTML ones
 * ("\u2605\u8981\u6ce8\u610f(\u5927\u5e45\u5909\u52d5)" / "\u901a\u5e38" in part 1, "\u2605\u8981\u6ce8\u610f" /
 * "\u901a\u5e38" in part 2). Numeric totals and change counts are bare
 * numbers (S6 rule); the HTML tool quotes every cell.
*/
#include "pch.h"
#include <cmath>
#include "Merge20/Merge20Csv.h"
#include "Merge20/Merge20FormCsv.h"
#include "Merge20/Merge20FormTable.h"

namespace
{

	// One text field, always double-quoted (S6 rule).
	std::wstring Quoted(const std::wstring& value)
	{
		return Merge20CsvEscapeField(value);
	}

	// One bare integer (S6 rule: counts stay unquoted).
	std::wstring BareInt(long long value)
	{
		return std::to_wstring(value);
	}

	// A numeric change rounded to an integer (HTML rule: the
	// CSV shows integers; the one-decimal display of design
	// section 5.2 is for the on-screen table, not the CSV).
	std::wstring RoundedInt(double value)
	{
		return std::to_wstring(static_cast<long long>(std::llround(value)));
	}

	// Join header names bare (no quotes), like the S6 header:
	// none of the names contains a comma, a quote or a newline.
	std::wstring JoinBare(const std::vector<std::wstring>& fields)
	{
		std::wstring out;
		for (size_t i = 0; i < fields.size(); ++i)
		{
			if (i > 0)
				out += L",";
			out += fields[i];
		}
		return out;
	}

	// The group a record belongs to, or nullptr (no group, or
	// the id names a group that is not in the list).
	const Merge20FormDiffGroup* FindGroup(
		const std::vector<Merge20FormDiffGroup>& groups,
		const Merge20FormDiffRecord& record)
	{
		if (record.groupId.empty())
			return nullptr;
		for (const Merge20FormDiffGroup& group : groups)
		{
			if (group.id == record.groupId)
				return &group;
		}
		return nullptr;
	}

	// Part 1 header (HTML columns + severity + one-line summary).
	std::wstring SummaryHeaderLine()
	{
		return JoinBare({
			L"No",
			L"\u8981\u6ce8\u610f\u8b66\u544a",
			L"\u30d5\u30a1\u30a4\u30eb\u540d",
			L"\u5e33\u7968\u540d\u79f0",
			L"\u5909\u66f4\u50be\u5411\u6982\u8981",
			L"\u6570\u5024\u5909\u52d5\u7dcf\u5408\u8a08(\u6574\u6570)",
			L"\u5909\u66f4\u7b87\u6240\u6570",
			L"\u6df1\u523b\u5ea6",
			L"\u4e00\u884c\u8981\u7d04",
		});
	}

	// Part 2 header (HTML columns + group + minor).
	std::wstring DetailHeaderLine()
	{
		return JoinBare({
			L"No",
			L"\u30d5\u30a1\u30a4\u30eb\u540d",
			L"\u5e33\u7968\u540d\u79f0",
			L"\u8981\u6ce8\u610f\u8b66\u544a",
			L"\u5909\u66f4\u7a2e\u5225",
			L"\u5bfe\u8c61\u8981\u7d20",
			L"\u5909\u66f4\u5c5e\u6027",
			L"\u65e7\u5024",
			L"\u65b0\u5024",
			L"\u5909\u52d5\u6570\u5024(\u6574\u6570)",
			L"\u5177\u4f53\u7684\u306a\u5909\u66f4\u5185\u5bb9",
			L"\u53ca\u307c\u3059\u5f71\u97ff\u5206\u6790",
			L"\u30b0\u30eb\u30fc\u30d7",
			L"\u5fae\u5c0f\u5dee",
		});
	}

} // namespace

std::wstring Merge20FormCsvSeverityText(Merge20FormSeverity severity)
{
	switch (severity)
	{
	case Merge20FormSeverity::LayoutBreaking:
		return L"\u30ec\u30a4\u30a2\u30a6\u30c8\u7834\u58ca";
	case Merge20FormSeverity::VisualOnly:
		return L"\u898b\u305f\u76ee\u3060\u3051";
	case Merge20FormSeverity::Info:
	default:
		return L"\u60c5\u5831";
	}
}

std::wstring Merge20FormCsvTypeText(Merge20FormDiffType type)
{
	// Moved / Renamed use the design section 4.2 words; the
	// other types keep the HTML vocabulary of the on-screen
	// table (Merge20FormTableTypeText).
	switch (type)
	{
	case Merge20FormDiffType::Moved:
		return L"\u79fb\u52d5";
	case Merge20FormDiffType::Renamed:
		return L"\u6539\u540d";
	default:
		return Merge20FormTableTypeText(type);
	}
}

std::wstring Merge20FormCsvGroupText(
	const std::vector<Merge20FormDiffGroup>& groups,
	const Merge20FormDiffRecord& record)
{
	const Merge20FormDiffGroup* group = FindGroup(groups, record);
	return group != nullptr ? group->summary : std::wstring();
}

bool Merge20FormCsvIsMinor(
	const std::vector<Merge20FormDiffGroup>& groups,
	const Merge20FormDiffRecord& record)
{
	const Merge20FormDiffGroup* group = FindGroup(groups, record);
	return group != nullptr && group->minor;
}

std::wstring Merge20FormCsvBuild(
	const std::vector<Merge20FormFileResult>& results,
	const std::vector<std::vector<Merge20FormDiffGroup>>& groups,
	const std::vector<size_t>& indices,
	const std::wstring& timestampText)
{
	std::vector<std::wstring> lines;
	// Title line (HTML `exportToExcel`): report title and the
	// output date/time, both quoted.
	lines.push_back(Quoted(L"\u25a0 XML\u30d5\u30a9\u30fc\u30e0\u5b9a\u7fa9 \u5909\u66f4\u30fb\u5dee\u7570\u6bd4\u8f03 \u7dcf\u5408\u30ec\u30dd\u30fc\u30c8") +
		L"," + Quoted(L"\u51fa\u529b\u65e5\u6642: " + timestampText));
	lines.push_back(std::wstring());

	// Part 1: the per-form summary list (HTML columns + the
	// section 6 (4) severity and one-line summary columns).
	lines.push_back(Quoted(L"\u5909\u66f4\u30fb\u5dee\u7570\u691c\u51fa \u5e33\u7968\u30b5\u30de\u30ea\u4e00\u89a7"));
	lines.push_back(SummaryHeaderLine());
	long long summaryNo = 0;
	for (size_t index : indices)
	{
		if (index >= results.size())
			continue;
		const Merge20FormFileResult& result = results[index];
		++summaryNo;
		std::wstring line;
		line += BareInt(summaryNo);
		line += L",";
		line += Quoted(result.isWarning ? L"\u2605\u8981\u6ce8\u610f(\u5927\u5e45\u5909\u52d5)" : L"\u901a\u5e38");
		line += L",";
		line += Quoted(result.fileName);
		line += L",";
		line += Quoted(result.formName);
		line += L",";
		line += Quoted(result.tendency);
		line += L",";
		line += RoundedInt(result.totalNumericSum);
		line += L",";
		line += BareInt(result.changeCount);
		line += L",";
		line += Quoted(Merge20FormCsvSeverityText(result.severity));
		line += L",";
		line += Quoted(result.summary);
		lines.push_back(line);
	}
	lines.push_back(std::wstring());
	lines.push_back(std::wstring());

	// Part 2: the per-change detail list (HTML columns + the
	// group and minor columns). Every record of every included
	// file is a row, including group members (the CSV is flat;
	// the group column says which group a row belongs to).
	lines.push_back(Quoted(L"\u5909\u66f4\u7b87\u6240\u5225 \u8a73\u7d30\u6bd4\u8f03\u30fb\u5f71\u97ff\u5206\u6790\u4e00\u89a7\uff08Excel\u30aa\u30fc\u30c8\u30d5\u30a3\u30eb\u30bf\u5bfe\u5fdc\uff09"));
	lines.push_back(DetailHeaderLine());
	long long detailNo = 0;
	for (size_t index : indices)
	{
		if (index >= results.size())
			continue;
		const Merge20FormFileResult& result = results[index];
		const std::vector<Merge20FormDiffGroup> noGroups;
		const std::vector<Merge20FormDiffGroup>& fileGroups =
			index < groups.size() ? groups[index] : noGroups;
		for (const Merge20FormDiffRecord& record : result.diffRecords)
		{
			++detailNo;
			std::wstring line;
			line += BareInt(detailNo);
			line += L",";
			line += Quoted(result.fileName);
			line += L",";
			line += Quoted(result.formName);
			line += L",";
			line += Quoted(result.isWarning ? L"\u2605\u8981\u6ce8\u610f" : L"\u901a\u5e38");
			line += L",";
			line += Quoted(Merge20FormCsvTypeText(record.type));
			line += L",";
			line += Quoted(Merge20FormTableDisplayName(record.element));
			line += L",";
			line += Quoted(record.attr);
			line += L",";
			line += Quoted(record.oldVal);
			line += L",";
			line += Quoted(record.newVal);
			line += L",";
			line += RoundedInt(record.diffVal);
			line += L",";
			line += Quoted(record.desc);
			line += L",";
			line += Quoted(record.impact);
			line += L",";
			line += Quoted(Merge20FormCsvGroupText(fileGroups, record));
			line += L",";
			line += Quoted(Merge20FormCsvIsMinor(fileGroups, record) ? L"\u5fae\u5c0f" : L"");
			lines.push_back(line);
		}
	}

	std::wstring out;
	for (const std::wstring& line : lines)
	{
		out += line;
		out += L"\r\n";
	}
	return out;
}
