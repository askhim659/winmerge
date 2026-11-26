// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Slim.h
 *
 * @brief Slim-list helpers (Merge20 Phase 4 D9, design sections 3.10
 * and 5.2/D9, spec sections 3 (4/4) and 5.1).
 *
 * Pure module (no MFC, no `_()`): the same code is compiled into
 * WinMergeU and into the GoogleTest runner.
 *
 * The HTML tool's slim list (`tgSlim()`, 17_v17.15.html line 1390,
 * CSS lines 133-141) hides the action-category, risk and
 * old/changed/new columns and keeps only the check box, the file
 * name and the review-state tag, the tag in a 126px column with
 * wrapping (`white-space: normal`) so a long tag is never clipped.
 * The fork maps that onto the folder-compare column registry
 * (`DirViewColItems`, regNames): the visible set is exactly
 * Name + Soutput (the check box, the HTML 24px column) + Sreview,
 * in that display order. The old/changed/new counts live in the
 * Ssummary text (and in the Snsdiffs/Snidiffs columns, hidden by
 * default), so hiding Ssummary hides the counts as specified.
 * The review column is 126px wide (`kReviewColumnWidth`).
 */
#pragma once

#include <string>
#include <vector>

namespace Merge20Slim
{
/** @brief Width of the review-state column in slim mode, px (HTML 126px). */
constexpr int kReviewColumnWidth = 126;
/** @brief Width of the output-check column in slim mode, px (HTML 24px + padding). */
constexpr int kOutputColumnWidth = 48;

/** @brief regNames visible in slim mode, in display order. */
std::vector<std::string> VisibleColumns();

/** @brief true when the regName stays visible in slim mode. */
bool IsVisibleColumn(const std::string& regName);

/**
 * @brief Column-orders string for the slim layout, in the named
 * format `DirViewColItems::LoadColumnOrders` accepts
 * ("0 | Name Soutput Sreview"). The integer prefix is ignored by
 * the named branch; it is kept so the string has the same shape
 * as `SaveColumnOrders()` output.
 */
std::string ColumnOrders();

/**
 * @brief true when a review-state label of the given pixel width
 * needs wrapping in the slim review column (i.e. it does not fit
 * on one line in `kReviewColumnWidth`). The current labels fit
 * (the longest English label, "Needs manual work", is about
 * 119px at the default font; the Japanese labels are at most
 * 5 characters), so wrapping is a safety net for longer future
 * labels, drawn by `CDirView::OnCustomDraw` with DT_WORDBREAK.
 */
bool LabelNeedsWrap(int labelWidthPx);
} // namespace Merge20Slim
