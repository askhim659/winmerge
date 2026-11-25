// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20CsvExport.cpp
 *
 * @brief Implementation of the Merge20 diff-list CSV export (S6).
 */
#include "StdAfx.h"
#include <vector>
#include "Merge20/Merge20CsvExport.h"
#include "Merge20/Merge20Csv.h"
#include "Merge20/Merge20FileStates.h"
#include "DiffContext.h"
#include "DiffItem.h"
#include "DirViewColItems.h"
#include "UniFile.h"
#include "MergeApp.h"
#include "I18n.h"

bool Merge20CsvHasDiff(const DIFFITEM& di)
{
	return di.diffcode.isResultDiff() ||
		di.diffcode.isSideFirstOnly() ||
		di.diffcode.isSideSecondOnly() ||
		di.diffcode.isSideThirdOnly();
}

void Merge20CsvCountRanges(const CDiffContext& ctxt, int& selectedOnly, int& allDiffs)
{
	selectedOnly = 0;
	allDiffs = 0;
	for (DIFFITEM* pPos = ctxt.GetFirstDiffPosition(); pPos != nullptr;)
	{
		const DIFFITEM& di = ctxt.GetNextDiffPosition(pPos);
		if (di.diffcode.isDirectory())
			continue;
		if (ctxt.GetMerge20IncludeInOutput(di))
			++selectedOnly;
		if (Merge20CsvHasDiff(di))
			++allDiffs;
	}
}

namespace
{

	// Header line: the 13 column names of design section 3.5, in
	// order. Joined bare (no quotes), as in the HTML tool's `rs[0]`;
	// none of the names contains a comma, a quote or a newline.
	String Merge20CsvHeaderLine()
	{
		const std::vector<String> headers =
		{
			_("Directory"), _("File name"), _("Full path"),
			_("Existence category"), _("Action Category"), _("Risk"),
			_("Review State"), _("Total diffs"), _("Missing lines"),
			_("Changed lines"), _("Added lines"), _("Summary"),
			_("Memo")
		};
		return strutils::join(headers.begin(), headers.end(), String(_T(",")));
	}

} // namespace

bool Merge20CsvExportToFile(CDiffContext& ctxt, Merge20CsvExportRange range, const String& filePath, String& errStr, int& exportedCount)
{
	exportedCount = 0;

	UniStdioFile file;
	if (!file.OpenCreateUtf8(filePath))
	{
		String errMsg = GetSysError(GetLastError());
		errStr = strutils::format_string1(_("Error creating the report:\n%1"), errMsg);
		return false;
	}
	// BOM-prefixed UTF-8 (design section 3.5): DirCmpReport's
	// GenerateReportToFile opens the file the same way but never calls
	// WriteBom, so a CSV written by it opens garbled in Excel.
	file.SetBom(true);
	file.WriteBom();

	file.WriteString(Merge20CsvHeaderLine());
	file.WriteString(_T("\r\n"));

	for (DIFFITEM* pPos = ctxt.GetFirstDiffPosition(); pPos != nullptr;)
	{
		const DIFFITEM& di = ctxt.GetNextDiffPosition(pPos);
		if (di.diffcode.isDirectory())
			continue;
		if (range == Merge20CsvExportRange::SelectedOnly)
		{
			if (!ctxt.GetMerge20IncludeInOutput(di))
				continue;
		}
		else
		{
			if (!Merge20CsvHasDiff(di))
				continue;
		}
		const String relPath = di.getItemRelativePath();
		const Merge20FileState& state = ctxt.GetMerge20FileStates().GetState(relPath);
		Merge20CsvRow row;
		row.dir = Merge20CsvGetDirText(&ctxt, di);
		row.fileName = Merge20CsvGetFileNameText(&ctxt, di);
		row.fullPath = relPath;
		row.status = Merge20CsvGetStatusText(&ctxt, di);
		row.action = Merge20CsvGetActionText(&ctxt, di);
		row.risk = Merge20CsvGetRiskText(&ctxt, di);
		row.review = Merge20CsvGetReviewText(&ctxt, di);
		row.missing = state.nMissing;
		row.modified = state.nModified;
		row.added = state.nAdded;
		row.diffTotal = state.nMissing + state.nModified + state.nAdded;
		row.summary = Merge20CsvGetSummaryText(&ctxt, di);
		row.memo = state.memo;
		file.WriteString(Merge20CsvFormatRow(row));
		file.WriteString(_T("\r\n"));
		++exportedCount;
	}
	file.Close();
	return true;
}
