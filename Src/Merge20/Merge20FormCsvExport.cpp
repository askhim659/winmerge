// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCsvExport.cpp
 *
 * @brief Implementation of the FORM compare CSV file writing
 * (Wave 5 F7). The file creation is the S6 `Merge20CsvExport`
 * way (UniStdioFile::OpenCreateUtf8 + WriteBom); the text
 * itself comes from the pure module `Merge20FormCsvBuild`.
 */
#include "StdAfx.h"
#include "Merge20/Merge20FormCsv.h"
#include "Merge20/Merge20FormCsvExport.h"
#include "UniFile.h"
#include "MergeApp.h"
#include "I18n.h"

bool Merge20FormCsvExportToFile(
	const std::vector<Merge20FormFileResult>& results,
	const std::vector<std::vector<Merge20FormDiffGroup>>& groups,
	const std::vector<size_t>& indices,
	const String& timestampText,
	const String& filePath, String& errStr, int& exportedCount)
{
	exportedCount = 0;

	UniStdioFile file;
	if (!file.OpenCreateUtf8(filePath))
	{
		String errMsg = GetSysError(GetLastError());
		errStr = strutils::format_string1(_("Error creating the report:\n%1"), errMsg);
		return false;
	}
	// BOM-prefixed UTF-8 (design section 9): the same file
	// creation the S6 diff-list CSV uses, so the FORM CSV
	// opens directly in Excel without garbled Japanese.
	file.SetBom(true);
	file.WriteBom();

	const std::wstring text = Merge20FormCsvBuild(results, groups, indices, timestampText);
	file.WriteString(text);
	for (size_t index : indices)
	{
		if (index < results.size())
			++exportedCount;
	}
	file.Close();
	return true;
}
