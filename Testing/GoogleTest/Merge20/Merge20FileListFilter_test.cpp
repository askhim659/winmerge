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
		ActionCategory action = ActionCategory::NO_ACTION_IDENTICAL,
		RiskLevel risk = RiskLevel::NONE)
	{
		Merge20FileListFilterRow row;
		row.relPath = relPath;
		row.isDirectory = false;
		row.depth = depth;
		row.classified = classified;
		row.action = action;
		row.risk = risk;
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
		const wchar_t* searchText = L"", int actionFilter = -1, int riskFilter = -1)
	{
		return Merge20ComputeFileListVisibility(rows, searchText, actionFilter, riskFilter);
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

TEST(Merge20FileListFilter, RiskFilterShowsOnlyHigh)
{
	// "High only" shows only the High file; a directory stays
	// when a High file remains below it (acceptance (c)).
	const std::vector<Merge20FileListFilterRow> rows =
	{
		DirRow(L"src", 0),
		FileRow(L"src\\a.txt", 1, true, ActionCategory::REVIEW_MIXED, RiskLevel::HIGH),
		FileRow(L"src\\b.txt", 1, true, ActionCategory::REVIEW_MOD_ONLY, RiskLevel::MEDIUM),
		FileRow(L"c.txt", 0, true, ActionCategory::NO_MERGE_NEW_ADD, RiskLevel::NONE),
	};
	const std::vector<bool> visible = Visible(rows, L"", -1,
		static_cast<int>(RiskLevel::HIGH));
	EXPECT_TRUE(visible[0]); // src: a.txt remains below it
	EXPECT_TRUE(visible[1]);
	EXPECT_FALSE(visible[2]);
	EXPECT_FALSE(visible[3]);
	// "Medium only" and "None only" pick their own level.
	const std::vector<bool> medium = Visible(rows, L"", -1,
		static_cast<int>(RiskLevel::MEDIUM));
	EXPECT_FALSE(medium[1]);
	EXPECT_TRUE(medium[2]);
	EXPECT_FALSE(medium[3]);
	const std::vector<bool> none = Visible(rows, L"", -1,
		static_cast<int>(RiskLevel::NONE));
	EXPECT_FALSE(none[1]);
	EXPECT_FALSE(none[2]);
	EXPECT_TRUE(none[3]);
}

TEST(Merge20FileListFilter, UnclassifiedFileHiddenByRiskFilter)
{
	// An unclassified file (never opened) matches no risk level,
	// only "all" (same rule as the action tabs).
	const std::vector<Merge20FileListFilterRow> rows =
	{
		FileRow(L"a.txt", 0, false, ActionCategory::NO_ACTION_IDENTICAL, RiskLevel::HIGH),
		FileRow(L"b.txt", 0, true, ActionCategory::REVIEW_MOD_ONLY, RiskLevel::HIGH),
	};
	const std::vector<bool> byRisk = Visible(rows, L"", -1,
		static_cast<int>(RiskLevel::HIGH));
	EXPECT_FALSE(byRisk[0]);
	EXPECT_TRUE(byRisk[1]);
	const std::vector<bool> all = Visible(rows);
	EXPECT_TRUE(all[0]);
	EXPECT_TRUE(all[1]);
}

TEST(Merge20FileListFilter, RiskFilterCombinesWithSearchAndActionTab)
{
	// The risk filter intersects with the search and the action
	// tab: a file shows only when all three match.
	const std::vector<Merge20FileListFilterRow> rows =
	{
		FileRow(L"tax.cpp", 0, true, ActionCategory::REVIEW_MIXED, RiskLevel::HIGH),
		FileRow(L"tax2.cpp", 0, true, ActionCategory::REVIEW_MOD_ONLY, RiskLevel::HIGH),
		FileRow(L"util.cpp", 0, true, ActionCategory::REVIEW_MIXED, RiskLevel::HIGH),
		FileRow(L"tax3.cpp", 0, true, ActionCategory::REVIEW_MIXED, RiskLevel::MEDIUM),
	};
	// Search "tax" + High: tax.cpp and tax2.cpp.
	const std::vector<bool> searched = Visible(rows, L"tax", -1,
		static_cast<int>(RiskLevel::HIGH));
	EXPECT_TRUE(searched[0]);
	EXPECT_TRUE(searched[1]);
	EXPECT_FALSE(searched[2]);
	EXPECT_FALSE(searched[3]);
	// Action tab REVIEW_MIXED + High: tax.cpp and util.cpp.
	const std::vector<bool> byTab = Visible(rows, L"",
		static_cast<int>(ActionCategory::REVIEW_MIXED),
		static_cast<int>(RiskLevel::HIGH));
	EXPECT_TRUE(byTab[0]);
	EXPECT_FALSE(byTab[1]);
	EXPECT_TRUE(byTab[2]);
	EXPECT_FALSE(byTab[3]);
	// All three: only tax.cpp.
	const std::vector<bool> allThree = Visible(rows, L"tax",
		static_cast<int>(ActionCategory::REVIEW_MIXED),
		static_cast<int>(RiskLevel::HIGH));
	EXPECT_TRUE(allThree[0]);
	EXPECT_FALSE(allThree[1]);
	EXPECT_FALSE(allThree[2]);
	EXPECT_FALSE(allThree[3]);
}

TEST(Merge20FileListFilter, CollapsedJoinEmptyIsEmpty)
{
	// Nothing folded: the stored option value is the empty string
	// (the same as the key's default).
	EXPECT_TRUE(Merge20JoinCollapsedPaths(std::set<String>()).empty());
}

TEST(Merge20FileListFilter, CollapsedJoinSplitsRoundTrip)
{
	// Join then split returns the same set; the joined text is in
	// sorted order, so the same set always stores the same string.
	const std::set<String> paths = { L"tax", L"src\\deep", L"src" };
	const String joined = Merge20JoinCollapsedPaths(paths);
	EXPECT_EQ(joined, String(L"src\nsrc\\deep\ntax"));
	const std::set<String> back = Merge20SplitCollapsedPaths(joined);
	EXPECT_EQ(back, paths);
}

TEST(Merge20FileListFilter, CollapsedSplitSkipsEmptyPieces)
{
	// A leading, trailing or doubled line break makes no phantom
	// empty path (a hand-edited or truncated value stays safe).
	const std::set<String> paths = Merge20SplitCollapsedPaths(L"\nsrc\n\ntax\n");
	ASSERT_EQ(paths.size(), static_cast<size_t>(2));
	EXPECT_TRUE(paths.count(L"src") != 0);
	EXPECT_TRUE(paths.count(L"tax") != 0);
}

TEST(Merge20FileListFilter, CollapsedSplitEmptyIsEmpty)
{
	// The default (never saved) value splits to no paths.
	EXPECT_TRUE(Merge20SplitCollapsedPaths(L"").empty());
}

TEST(Merge20FileListFilter, CollapsedJoinKeepsBackslashPaths)
{
	// Relative paths carry backslashes; they survive the round
	// trip unchanged (the separator is the line break, not the
	// backslash).
	const std::set<String> paths = { L"tax\\deep\\b.txt", L"src\\a.txt" };
	const std::set<String> back =
		Merge20SplitCollapsedPaths(Merge20JoinCollapsedPaths(paths));
	EXPECT_EQ(back, paths);
}
