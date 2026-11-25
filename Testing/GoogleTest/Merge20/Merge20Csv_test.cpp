/**
 * @file Merge20Csv_test.cpp
 *
 * @brief Merge20 (S6) unit tests for the diff-list CSV formatting.
 *
 * The expected values follow the HTML tool's `dlCsv` (17_v17.15.html)
 * exactly: text fields are always double-quoted with embedded quotes
 * doubled (""), the four counts are bare numbers, and the row has 13
 * fields in the design section 3.5 order (directory, file name, full
 * path, existence category, action category, risk, review state,
 * total diffs, missing, modified, added, summary, memo).
 *
 * This file stays ASCII: the Japanese test text is written with \\u
 * escapes, like Merge20Classify_test.cpp.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Csv.h"

namespace
{

	// Count the fields of a CSV line, ignoring commas inside quotes.
	int CountCsvFields(const String& line)
	{
		int fields = 1;
		bool inQuotes = false;
		for (size_t i = 0; i < line.length(); ++i)
		{
			if (line[i] == L'"')
			{
				if (inQuotes && i + 1 < line.length() && line[i + 1] == L'"')
					++i;
				else
					inQuotes = !inQuotes;
			}
			else if (line[i] == L',' && !inQuotes)
			{
				++fields;
			}
		}
		return fields;
	}

} // namespace

TEST(Merge20Csv, ColumnCount)
{
	EXPECT_EQ(kMerge20CsvColumnCount, 13);
}

TEST(Merge20Csv, EscapePlainField)
{
	EXPECT_EQ(Merge20CsvEscapeField(L"src"), String(L"\"src\""));
	EXPECT_EQ(Merge20CsvEscapeField(L""), String(L"\"\""));
}

TEST(Merge20Csv, EscapeCommaAndNewline)
{
	// A comma or a newline inside the value must not split the field:
	// the surrounding quotes keep it one field.
	EXPECT_EQ(Merge20CsvEscapeField(L"a,b"), String(L"\"a,b\""));
	EXPECT_EQ(Merge20CsvEscapeField(L"a\nb"), String(L"\"a\nb\""));
	EXPECT_EQ(Merge20CsvEscapeField(L"a\rb"), String(L"\"a\rb\""));
}

TEST(Merge20Csv, EscapeDoubleQuote)
{
	// HTML dlCsv rule: `"` becomes `""` inside a quoted field.
	EXPECT_EQ(Merge20CsvEscapeField(L"say \"hi\""), String(L"\"say \"\"hi\"\"\""));
	EXPECT_EQ(Merge20CsvEscapeField(L"\""), String(L"\"\"\"\""));
}

TEST(Merge20Csv, FormatRowSample)
{
	Merge20CsvRow row;
	row.dir = L"src";
	row.fileName = L"Foo.java";
	row.fullPath = L"src\\Foo.java";
	row.status = L"Changed";
	row.action = L"Review (modified)";
	row.risk = L"High";
	row.review = L"Done";
	row.diffTotal = 3;
	row.missing = 1;
	row.modified = 1;
	row.added = 1;
	row.summary = L"Missing: 1 lines / Changed: 1 lines / Added: 1 lines";
	// Japanese memo with a comma and quotes: must stay one field.
	row.memo = L"\u30e1\u30e2, \"\u5f15\u7528\"";
	EXPECT_EQ(Merge20CsvFormatRow(row),
		String(L"\"src\",\"Foo.java\",\"src\\Foo.java\",\"Changed\",\"Review (modified)\",\"High\",\"Done\",3,1,1,1,\"Missing: 1 lines / Changed: 1 lines / Added: 1 lines\",\"\u30e1\u30e2, \"\"\u5f15\u7528\"\"\""));
	EXPECT_EQ(CountCsvFields(Merge20CsvFormatRow(row)), 13);
}

TEST(Merge20Csv, FormatRowEmpty)
{
	// An empty row is 9 quoted empty fields and 4 bare zeros.
	const Merge20CsvRow row;
	EXPECT_EQ(Merge20CsvFormatRow(row),
		String(L"\"\",\"\",\"\",\"\",\"\",\"\",\"\",0,0,0,0,\"\",\"\""));
	EXPECT_EQ(CountCsvFields(Merge20CsvFormatRow(row)), 13);
}

TEST(Merge20Csv, FormatRowCountsAreBareNumbers)
{
	// Counts are written as plain digits (no quotes, no grouping),
	// so Excel reads them as numbers, as in the HTML tool.
	Merge20CsvRow row;
	row.diffTotal = 1234;
	row.missing = 100;
	row.modified = 20;
	row.added = 3;
	const String line = Merge20CsvFormatRow(row);
	EXPECT_TRUE(line.find(L",1234,100,20,3,") != String::npos);
	EXPECT_EQ(CountCsvFields(line), 13);
}
