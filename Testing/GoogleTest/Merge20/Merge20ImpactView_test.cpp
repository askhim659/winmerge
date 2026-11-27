// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Merge20ImpactView_test.cpp
 *
 * @brief Merge20 (best-workflow B5, PR2) unit tests for the
 * impact-list display assembly.
 *
 * What is fixed here: a list row keeps the hit's symbol, relative
 * path, pane and excerpt and shows the line 1-based (the hit
 * stores the 0-based real file line, the list shows the line the
 * user sees in the editor, the same convention the D7 Grep list
 * uses); the jump target is the D7 record with the same path,
 * pane, line (0-based, as the jump takes it) and excerpt, and no
 * symbol (the diff index is resolved at click time from the
 * opened compare document, so the jump needs nothing else).
 *
 * This file is ASCII; the pure module is MFC-free, so these tests
 * run in the UnitTests runner on CI.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Impact.h"
#include "Merge20/Merge20ImpactView.h"

namespace
{

	Merge20ImpactHit Hit(const wchar_t* symbol, const wchar_t* relPath, int pane, int line, const wchar_t* excerpt)
	{
		Merge20ImpactHit hit;
		hit.symbol = symbol;
		hit.relPath = relPath;
		hit.pane = pane;
		hit.line = line;
		hit.excerpt = excerpt;
		return hit;
	}

} // namespace

TEST(Merge20ImpactView, RowKeepsHitFields)
{
	const Merge20ImpactViewRow row =
		Merge20ImpactViewRowFromHit(Hit(L"calcTax", L"src/b.cpp", 1, 41, L"long v = calcTax(base);"));
	EXPECT_EQ(row.symbol, String(L"calcTax"));
	EXPECT_EQ(row.relPath, String(L"src/b.cpp"));
	EXPECT_EQ(row.pane, 1);
	EXPECT_EQ(row.excerpt, String(L"long v = calcTax(base);"));
}

TEST(Merge20ImpactView, RowLineIsOneBased)
{
	EXPECT_EQ(Merge20ImpactViewRowFromHit(Hit(L"calcTax", L"src/b.cpp", 0, 0, L"calcTax(x);")).lineNumber,
		String(L"1"));
	EXPECT_EQ(Merge20ImpactViewRowFromHit(Hit(L"calcTax", L"src/b.cpp", 0, 41, L"calcTax(x);")).lineNumber,
		String(L"42"));
}

TEST(Merge20ImpactView, RowKeepsOldPane)
{
	const Merge20ImpactViewRow row =
		Merge20ImpactViewRowFromHit(Hit(L"calcTax", L"src/b.cpp", 0, 7, L"calcTax(x);"));
	EXPECT_EQ(row.pane, 0);
	EXPECT_EQ(row.lineNumber, String(L"8"));
}

TEST(Merge20ImpactView, JumpTargetKeepsPathPaneLineExcerpt)
{
	const Merge20GrepHit target =
		Merge20ImpactHitToGrepHit(Hit(L"calcTax", L"src/b.cpp", 1, 41, L"long v = calcTax(base);"));
	EXPECT_EQ(target.relPath, String(L"src/b.cpp"));
	EXPECT_EQ(target.pane, 1);
	// The jump takes the 0-based real file line, not the display line.
	EXPECT_EQ(target.line, 41);
	EXPECT_EQ(target.excerpt, String(L"long v = calcTax(base);"));
}

TEST(Merge20ImpactView, JumpTargetForFirstLine)
{
	const Merge20GrepHit target =
		Merge20ImpactHitToGrepHit(Hit(L"calcTax", L"src/b.cpp", 0, 0, L"calcTax(x);"));
	EXPECT_EQ(target.line, 0);
	EXPECT_EQ(target.pane, 0);
}

TEST(Merge20ImpactView, RowAndJumpTargetAgreeOnLine)
{
	// The row shows line + 1, the jump target carries line as-is:
	// both describe the same file line.
	const Merge20ImpactHit hit = Hit(L"calcTax", L"src/b.cpp", 1, 99, L"calcTax(x);");
	const Merge20ImpactViewRow row = Merge20ImpactViewRowFromHit(hit);
	const Merge20GrepHit target = Merge20ImpactHitToGrepHit(hit);
	EXPECT_EQ(row.lineNumber, String(L"100"));
	EXPECT_EQ(target.line, 99);
	EXPECT_EQ(row.relPath, target.relPath);
	EXPECT_EQ(row.pane, target.pane);
}
