// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Csv.cpp
 *
 * @brief Implementation of the Merge20 diff-list CSV formatting (S6).
 *
 * Port of the HTML tool's `dlCsv` row building (17_v17.15.html): every
 * text field is double-quoted and an embedded `"` becomes `""`, while
 * the four counts stay bare numbers. The HTML tool builds the header
 * unquoted (`rs[0]` is a plain comma-joined line); the caller does
 * the same for the header, so only row formatting lives here.
 *
 * This file stays ASCII: no Japanese literal is needed here (column
 * names and value words are composed by the caller through `_()`).
 */
#include "pch.h"
#include <string>
#include "Merge20/Merge20Csv.h"

String Merge20CsvEscapeField(const String& value)
{
	String out;
	out += L'"';
	for (wchar_t ch : value)
	{
		if (ch == L'"')
			out += L"\"\"";
		else
			out += ch;
	}
	out += L'"';
	return out;
}

String Merge20CsvFormatRow(const Merge20CsvRow& row)
{
	String line;
	line += Merge20CsvEscapeField(row.dir);
	line += L',';
	line += Merge20CsvEscapeField(row.fileName);
	line += L',';
	line += Merge20CsvEscapeField(row.fullPath);
	line += L',';
	line += Merge20CsvEscapeField(row.status);
	line += L',';
	line += Merge20CsvEscapeField(row.action);
	line += L',';
	line += Merge20CsvEscapeField(row.risk);
	line += L',';
	line += Merge20CsvEscapeField(row.review);
	line += L',';
	line += std::to_wstring(row.diffTotal);
	line += L',';
	line += std::to_wstring(row.missing);
	line += L',';
	line += std::to_wstring(row.modified);
	line += L',';
	line += std::to_wstring(row.added);
	line += L',';
	line += Merge20CsvEscapeField(row.summary);
	line += L',';
	line += Merge20CsvEscapeField(row.memo);
	return line;
}
