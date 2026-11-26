/**
 * @file  Merge20ExcludedLines.cpp
 *
 * @brief Implementation of the prediffer-blanked line count
 * (Merge20 S0-F1, coverage matrix S0-F1/H-08/H-28).
 *
 * This file stays ASCII and uses no MFC: it is compiled into
 * WinMergeU (pch.h group) and into the GoogleTest runner.
 */
#include "pch.h"
#include <string_view>
#include <vector>
#include "Merge20/Merge20ExcludedLines.h"

namespace
{

bool IsBlank(std::wstring_view line)
{
	for (wchar_t c : line)
	{
		if (c != L' ' && c != L'\t')
			return false;
	}
	return true;
}

// Split a text into lines. CRLF, LF and CR are all line breaks.
// A trailing line break does not create a phantom extra line
// ("a\n" is one line, "a\n\n" is two lines, "" is zero lines),
// so the empty segment a prediffer-style split would produce
// after the last break is never counted as a line.
std::vector<std::wstring_view> SplitLines(std::wstring_view text)
{
	std::vector<std::wstring_view> lines;
	size_t start = 0;
	while (start < text.size())
	{
		size_t end = start;
		while (end < text.size() && text[end] != L'\r' && text[end] != L'\n')
			++end;
		lines.push_back(text.substr(start, end - start));
		if (end < text.size() && text[end] == L'\r' && end + 1 < text.size() && text[end + 1] == L'\n')
			end += 2;
		else
			end += 1;
		start = end;
	}
	return lines;
}

} // namespace

int Merge20CountExcludedLines(std::wstring_view before, std::wstring_view after)
{
	const std::vector<std::wstring_view> beforeLines = SplitLines(before);
	const std::vector<std::wstring_view> afterLines = SplitLines(after);
	int count = 0;
	for (size_t i = 0; i < beforeLines.size(); ++i)
	{
		if (IsBlank(beforeLines[i]))
			continue; // already blank: the prediffer did not exclude it
		if (i >= afterLines.size() || IsBlank(afterLines[i]))
			++count; // blanked, or deleted outright, by the prediffer
	}
	return count;
}
