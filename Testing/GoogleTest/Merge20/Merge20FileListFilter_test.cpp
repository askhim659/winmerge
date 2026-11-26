// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20FileListFilter_test.cpp
 *
 * @brief Merge20 (Panel P2) unit tests for the file-list panel
 * visibility decision (Merge20ComputeFileListVisibility, design
 * section 12.7): search (HTML qFile definition), the
 * action-category filter tabs, and the per-directory
 * expand/collapse state.
 *
 * This file is ASCII; the module is MFC-free, so these tests run
 * in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20FileListFilter.h"
#include "Merge20/Merge20Types.h"

namespace
{
	Merge20FileListFilterRow FileRow(const wchar_t* relPath, int depth,
		bool classified = true,
		ActionCategory action = ActionCategory::NO_ACTION_IDENTICAL)
	{
		Merge20FileListFilterRow row;
		row.relPath = relPath;
		row.isDirectory = false;
		row.depth = depth;
		row.classified = classified;
		row.action = action;
		return row;
	}

	Merge20FileListFilterRow DirRow(const wchar_t* relPath, int depth, bool collapsed = false)
	{
		Merge20FileListFilterRow row;
		row.relPath = relPath;
		row.isDirectory = true;
		row.depth = depth;
		row.collapsed = collapsed;
		return row;
	}

	std::vector<bool> Visible(const std::vector<Merge20FileListFilterRow>& rows,
		const wchar_t* searchText = L"", int actionFilter = -1)
	{
		return Merge20ComputeFileListVisibility(rows, searchText, actionFilter);
	}
}

TEST(Merge20FileListFilter, NoFilterShowsAllRows)
{
	// No search, "all" tab, nothing collapsed: every row shows.
	const std::vector<Merge20FileListFilterRow> rows =
	{
		DirRow(L"src", 0),
		FileRow(L"src\\a.txt", 1),
		FileRow(L"src\\b.txt", 1),
		FileRow(L"c.txt", 0),
	};
	const std::vector<bool> visible = Visible(rows);
	ASSERT_EQ(visible.size(), rows.size());
	for (size_t i = 0; i < rows.size(); ++i)
		EXPECT_TRUE(visible[i]) << "row " << i;
}

TEST(Merge20FileListFilter, SearchMatchesFileNamePart)
{
	// Substring match on the file name: only the matching file
	// (and its ancestor directory) stays.
	const std::vector<Merge20FileListFilterRow> rows =
	{
		DirRow(L"src", 0),
		FileRow(L"src\\tax.cpp", 1),
		FileRow(L"src\\util.cpp", 1),
		FileRow(L"readme.txt", 0),
	};
	const std::vector<bool> visible = Visible(rows, L"tax");
	EXPECT_TRUE(visible[0]); // src: has a visible descendant
	EXPECT_TRUE(visible[1]); // src\tax.cpp
	EXPECT_FALSE(visible[2]);
	EXPECT_FALSE(visible[3]);
}

TEST(Merge20FileListFilter, SearchIgnoresCase)
{
	// "Foo" matches "foo/bar.txt": the match is case-insensitive
	// (HTML qFile lower-cases both sides).
	const std::vector<Merge20FileListFilterRow> rows =
	{
		DirRow(L"foo", 0),
		FileRow(L"foo\\bar.txt", 1),
		FileRow(L"foo\\baz.txt", 1),
	};
	const std::vector<bool> visible = Visible(rows, L"FOO\\BAR");
	EXPECT_TRUE(visible[0]);
	EXPECT_TRUE(visible[1]);
	EXPECT_FALSE(visible[2]);
}

TEST(Merge20FileListFilter, SearchMatchesFolderName)
{
	// Searching a folder name shows the files below it: the match
	// is against the whole relative path, folder part included.
	const std::vector<Merge20FileListFilterRow> rows =
	{
		DirRow(L"tax", 0),
		FileRow(L"tax\\a.txt", 1),
		FileRow(L"tax\\b.txt", 1),
		DirRow(L"other", 0),
		FileRow(L"other\\c.txt", 1),
	};
	const std::vector<bool> visible = Visible(rows, L"tax");
	EXPECT_TRUE(visible[0]);
	EXPECT_TRUE(visible[1]);
	EXPECT_TRUE(visible[2]);
	EXPECT_FALSE(visible[3]);
	EXPECT_FALSE(visible[4]);
}

TEST(Merge20FileListFilter, SearchTrimsSurroundingWhitespace)
{
	// " foo " behaves like "foo" (HTML qFile trims the input).
	const std::vector<Merge20FileListFilterRow> rows =
	{
		FileRow(L"foo.txt", 0),
		FileRow(L"bar.txt", 0),
	};
	const std::vector<bool> trimmed = Visible(rows, L" foo ");
	const std::vector<bool> plain = Visible(rows, L"foo");
	ASSERT_EQ(trimmed.size(), plain.size());
	for (size_t i = 0; i < rows.size(); ++i)
		EXPECT_EQ(trimmed[i], plain[i]) << "row " << i;
	EXPECT_TRUE(trimmed[0]);
	EXPECT_FALSE(trimmed[1]);
}

TEST(Merge20FileListFilter, DirectoryShownForVisibleDescendant)
{
	// A directory that does not match the search itself stays
	// visible when it has a visible descendant file (it is the
	// path to the match).
	const std::vector<Merge20FileListFilterRow> rows =
	{
		DirRow(L"deep", 0),
		DirRow(L"deep\\inner", 1),
		FileRow(L"deep\\inner\\target.txt", 2),
		FileRow(L"deep\\inner\\other.txt", 2),
	};
	const std::vector<bool> visible = Visible(rows, L"target");
	EXPECT_TRUE(visible[0]); // deep: visible file two levels down
	EXPECT_TRUE(visible[1]); // deep\inner: visible file below it
	EXPECT_TRUE(visible[2]);
	EXPECT_FALSE(visible[3]);
}

TEST(Merge20FileListFilter, ActionTabShowsOnlyMatchingCategory)
{
	// The category tab shows only files of that category; a
	// directory stays when a matching file remains below it.
	const std::vector<Merge20FileListFilterRow> rows =
	{
		DirRow(L"src", 0),
		FileRow(L"src\\a.txt", 1, true, ActionCategory::MERGE_CLEAN_OLD),
		FileRow(L"src\\b.txt", 1, true, ActionCategory::REVIEW_MOD_ONLY),
		FileRow(L"c.txt", 0, true, ActionCategory::MERGE_CLEAN_OLD),
	};
	const std::vector<bool> visible = Visible(rows, L"",
		static_cast<int>(ActionCategory::MERGE_CLEAN_OLD));
	EXPECT_TRUE(visible[0]); // src: a.txt remains below it
	EXPECT_TRUE(visible[1]);
	EXPECT_FALSE(visible[2]);
	EXPECT_TRUE(visible[3]);
}

TEST(Merge20FileListFilter, UnclassifiedFileHiddenByCategoryTab)
{
	// An unclassified file (never opened) matches no category
	// tab, only "all".
	const std::vector<Merge20FileListFilterRow> rows =
	{
		FileRow(L"a.txt", 0, false),
		FileRow(L"b.txt", 0, true, ActionCategory::REVIEW_MOD_ONLY),
	};
	const std::vector<bool> byTab = Visible(rows, L"",
		static_cast<int>(ActionCategory::REVIEW_MOD_ONLY));
	EXPECT_FALSE(byTab[0]);
	EXPECT_TRUE(byTab[1]);
	const std::vector<bool> all = Visible(rows);
	EXPECT_TRUE(all[0]);
	EXPECT_TRUE(all[1]);
}

TEST(Merge20FileListFilter, CollapseHidesDescendants)
{
	// A collapsed directory hides its descendants; the
	// directory row itself stays.
	const std::vector<Merge20FileListFilterRow> rows =
	{
		DirRow(L"src", 0, true),
		FileRow(L"src\\a.txt", 1),
		FileRow(L"src\\b.txt", 1),
		FileRow(L"c.txt", 0),
	};
	const std::vector<bool> visible = Visible(rows);
	EXPECT_TRUE(visible[0]); // src itself stays
	EXPECT_FALSE(visible[1]);
	EXPECT_FALSE(visible[2]);
	EXPECT_TRUE(visible[3]);
}

TEST(Merge20FileListFilter, SearchIgnoresCollapse)
{
	// While searching, collapse is ignored: a matching file
	// inside a collapsed folder is shown.
	const std::vector<Merge20FileListFilterRow> rows =
	{
		DirRow(L"src", 0, true),
		FileRow(L"src\\tax.cpp", 1),
		FileRow(L"src\\util.cpp", 1),
	};
	const std::vector<bool> visible = Visible(rows, L"tax");
	EXPECT_TRUE(visible[0]); // src: has a visible descendant
	EXPECT_TRUE(visible[1]);
	EXPECT_FALSE(visible[2]);
}
