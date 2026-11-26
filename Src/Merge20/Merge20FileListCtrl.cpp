// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FileListCtrl.cpp
 *
 * @brief Implementation file for CMerge20FileListCtrl
 *
 * Merge20 (Panel P0/P1, design section 12.4/12.7): virtual list of
 * the folder compare's files. P1 shows eight columns (selection,
 * file name, action category, risk, missing / changed / added
 * counts, review state). Every display value comes from the S6
 * wrappers (Merge20CsvGet*Text in DirViewColItems.h), which forward
 * to the same static getters the folder compare list uses, so the
 * panel shows exactly what the list shows. The three count columns
 * are the H-LIST columns (one locale-formatted number each); they
 * replaced the P1 provisional single column, which showed the
 * counts part of the summary text in one column.
 *
 * Rows are cached by value in Refresh(), so LVN_GETDISPINFO only
 * hands out pointers to strings that live until the next Refresh()
 * (the CDirView equivalent uses AllocDispinfoText because its text
 * is computed on the fly). Only visible rows are drawn (virtual
 * list), so 100-1000 files stay cheap to draw; Refresh() itself
 * rebuilds one small struct per row.
 */

#include "StdAfx.h"
#include "Merge20/Merge20FileListCtrl.h"
#include "Merge20/Merge20FileListFilter.h"
#include "DiffContext.h"
#include "DirViewColItems.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20FileListCtrl, CListCtrl);

namespace
{
	/** @brief Column widths (px), in display order. The panel is
	 * 480px wide by default, so the list scrolls horizontally
	 * until the user widens it; the file name gets the most room,
	 * the three count columns are narrow numbers. The eight
	 * columns (714px) are slightly narrower than the six P1
	 * columns were (742px), so the panel does not get wider. */
	const int Merge20FileListColWidths[8] =
	{
		48, // selection (checkbox glyph, centred)
		160, // file name
		140, // action category
		64, // risk (centred)
		64, // missing lines (H-LIST)
		64, // changed lines (H-LIST)
		64, // added lines (H-LIST)
		110, // review state
	};
	/** @brief High-risk emphasis (P-R2): red text in the Risk cell.
	 * The folder list has no such emphasis (see the header comment). */
	const COLORREF Merge20FileListHighRiskText = RGB(192, 0, 0);
} // namespace

BEGIN_MESSAGE_MAP(CMerge20FileListCtrl, CListCtrl)
	//{{AFX_MSG_MAP(CMerge20FileListCtrl)
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnGetdispinfo)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, OnCustomDraw)
	ON_NOTIFY_REFLECT(NM_RCLICK, OnRClick)
	ON_NOTIFY_REFLECT(NM_DBLCLK, OnDblclk)
	ON_NOTIFY_REFLECT(NM_CLICK, OnClick)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CMerge20FileListCtrl::CMerge20FileListCtrl()
: m_pCtxt(nullptr)
{
}

CMerge20FileListCtrl::~CMerge20FileListCtrl()
{
}

/**
 * @brief Insert the eight P1 columns (call once, after Create).
 *
 * Header texts reuse the folder compare list's msgids
 * ("Include in Output", "Filename", "Action Category", "Risk",
 * "Missing lines", "Changed lines", "Added lines", "Review
 * State"), so the headers match the list exactly, including the
 * [Wiz] mark W1 put on the Merge20 headers. The three count
 * columns reuse the CSV header msgids, so they carry no [Wiz]
 * mark (see DirViewColItems.cpp: the same msgids render the CSV
 * header line).
 */
void CMerge20FileListCtrl::SetupColumns()
{
	// The headers are written as literal _() calls, not taken from
	// a table of string pointers: the .pot generator
	// (Translations/WinMerge/CreateMasterPotFile.js) only scans
	// literal _()/N_()/NC_() arguments, so a variable argument
	// would silently drop the msgid from the next regenerated .pot.
	static const int formats[COL_COUNT] =
	{
		LVCFMT_CENTER,
		LVCFMT_LEFT,
		LVCFMT_LEFT,
		LVCFMT_CENTER,
		LVCFMT_RIGHT,
		LVCFMT_RIGHT,
		LVCFMT_RIGHT,
		LVCFMT_LEFT,
	};
	const String headers[COL_COUNT] =
	{
		_("Include in Output"),
		_("Filename"),
		_("Action Category"),
		_("Risk"),
		_("Missing lines"),
		_("Changed lines"),
		_("Added lines"),
		_("Review State"),
	};
	for (int col = 0; col < COL_COUNT; ++col)
		InsertColumn(col, headers[col].c_str(), formats[col], Merge20FileListColWidths[col]);
}

/**
 * @brief Re-apply the column header texts (language change).
 *
 * The same msgid table as SetupColumns: the headers must follow
 * the GUI language exactly like the folder compare list does.
 */
void CMerge20FileListCtrl::UpdateColumnHeaders()
{
	// Literal _() calls again (see SetupColumns): the .pot
	// generator only scans literal arguments.
	const String headers[COL_COUNT] =
	{
		_("Include in Output"),
		_("Filename"),
		_("Action Category"),
		_("Risk"),
		_("Missing lines"),
		_("Changed lines"),
		_("Added lines"),
		_("Review State"),
	};
	for (int col = 0; col < COL_COUNT; ++col)
	{
		LVCOLUMN lvc = {};
		lvc.mask = LVCF_TEXT;
		lvc.pszText = const_cast<LPTSTR>(headers[col].c_str());
		SetColumn(col, &lvc);
	}
}

/**
 * @brief Rebuild the cached rows from the folder context.
 *
 * The iteration is the flat depth-first walk (GetFirstDiffPosition /
 * GetNextDiffPosition), the same pattern CDirView uses to visit every
 * item: directory rows are included, in compare order, as in the
 * folder compare list. P2 adds the hierarchy (expand/collapse), the
 * search and the action-category filter: Refresh() first enumerates
 * all rows into m_allRows, then asks the pure module
 * (Merge20ComputeFileListVisibility) which of them are displayed,
 * and keeps only those in m_rows. Every display value comes from
 * the S6 wrappers, so the panel and the folder list can never
 * disagree.
 */
void CMerge20FileListCtrl::Refresh()
{
	m_allRows.clear();
	if (m_pCtxt != nullptr)
	{
		for (DIFFITEM* p = m_pCtxt->GetFirstDiffPosition(); p != nullptr;)
		{
			const DIFFITEM& di = m_pCtxt->GetNextDiffPosition(p);
			Row row;
			row.output = Merge20CsvGetOutputText(m_pCtxt, di);
			row.fileName = Merge20CsvGetFileNameText(m_pCtxt, di);
			row.action = Merge20CsvGetActionText(m_pCtxt, di);
			row.risk = Merge20CsvGetRiskText(m_pCtxt, di);
			row.missing = Merge20CsvGetMissingText(m_pCtxt, di);
		row.modified = Merge20CsvGetModifiedText(m_pCtxt, di);
		row.added = Merge20CsvGetAddedText(m_pCtxt, di);
			row.review = Merge20CsvGetReviewText(m_pCtxt, di);
			const String relPath = di.getItemRelativePath();
			row.reviewState = m_pCtxt->GetMerge20ReviewState(relPath);
			const Merge20FileState& state = m_pCtxt->GetMerge20FileStates().GetState(relPath);
			row.riskLevel = state.risk;
			row.isDirectory = di.diffcode.isDirectory();
			row.includeInOutput = m_pCtxt->GetMerge20IncludeInOutput(di);
			// P2: the fields the visibility decision and the bar
			// need (relPath keys the collapse set and resolves
			// DIFFITEMs for the batch check; depth drives the
			// collapse; action/classified drive the filter tabs).
			row.relPath = relPath;
			row.depth = di.GetDepth();
			row.classified = state.classified;
			row.actionCategory = state.action;
			row.hasChildren = di.HasChildren();
			// P2: fold markers on directory rows with children
			// (U+25BE unfolded, U+25B8 folded), as the panel's own
			// display prefix; the S6 wrapper value is untouched.
			if (row.isDirectory && row.hasChildren)
			{
				const bool collapsed = m_collapsed.count(relPath) != 0;
				row.fileName = (collapsed ? _T("\x25B8 ") : _T("\x25BE ")) + row.fileName;
			}
			m_allRows.push_back(std::move(row));
		}
	}
	// P2: search / tab / collapse visibility is decided by the pure
	// module (unit-tested in Merge20FileListFilter_test.cpp).
	std::vector<Merge20FileListFilterRow> filterRows;
	filterRows.reserve(m_allRows.size());
	for (const Row& row : m_allRows)
	{
		Merge20FileListFilterRow filterRow;
		filterRow.relPath = row.relPath;
		filterRow.isDirectory = row.isDirectory;
		filterRow.depth = row.depth;
		filterRow.classified = row.classified;
		filterRow.action = row.actionCategory;
		filterRow.collapsed = m_collapsed.count(row.relPath) != 0;
		filterRows.push_back(std::move(filterRow));
	}
	const std::vector<bool> visible =
		Merge20ComputeFileListVisibility(filterRows, m_searchText, m_actionFilter);
	m_rows.clear();
	for (size_t i = 0; i < m_allRows.size(); ++i)
	{
		if (i < visible.size() && visible[i])
			m_rows.push_back(m_allRows[i]);
	}
	SetItemCount(static_cast<int>(m_rows.size()));
}

/**
 * @brief Count displayed files per review state (P2, for the
 * batch-check dialog).
 *
 * Same definition as CDirView::Merge20CountDisplayedByReviewState
 * (S3), but over the panel's displayed rows: a file hidden by the
 * search, the filter tab or a collapsed folder is not counted, so
 * the dialog's counts match what the panel shows.
 */
void CMerge20FileListCtrl::CountByReviewState(int counts[5]) const
{
	for (int i = 0; i < 5; ++i)
		counts[i] = 0;
	for (const Row& row : m_rows)
	{
		if (row.isDirectory)
			continue;
		++counts[static_cast<int>(row.reviewState)];
	}
}

/**
 * @brief Set the search text (P2, P-R4) and refresh.
 *
 * The raw text is stored; the filter module trims it and matches
 * case-insensitively (HTML qFile definition). Saving and restoring
 * the text is P4.
 */
void CMerge20FileListCtrl::SetSearchText(const String& searchText)
{
	if (m_searchText == searchText)
		return;
	m_searchText = searchText;
	Refresh();
}

/**
 * @brief Set the action-category filter (P2, P-R10) and refresh.
 *
 * -1 shows every category; otherwise the tab's ActionCategory.
 * Saving and restoring the selected tab is P4.
 */
void CMerge20FileListCtrl::SetActionFilter(int actionFilter)
{
	if (m_actionFilter == actionFilter)
		return;
	m_actionFilter = actionFilter;
	Refresh();
}

/**
 * @brief Fold or unfold one directory row (P2, P-R5).
 */
void CMerge20FileListCtrl::SetCollapsed(const String& relPath, bool collapsed)
{
	if (collapsed)
		m_collapsed.insert(relPath);
	else
		m_collapsed.erase(relPath);
	Refresh();
	// P4: the folded set is saved at once (see SaveCollapsedState).
	SaveCollapsedState();
}

/**
 * @brief Replace the folded directories (Panel P4, restore).
 *
 * The set is taken as-is (stale paths are harmless: they only
 * match rows that exist). No save here: the bar saves the
 * restored set with the rest of the panel state.
 */
void CMerge20FileListCtrl::SetCollapsedPaths(const std::set<String>& paths)
{
	m_collapsed = paths;
	Refresh();
}

/**
 * @brief Persist the folded directories at once (Panel P4).
 *
 * Called after every collapse change (SetCollapsed, ExpandAll,
 * CollapseAll; the double-click path goes through SetCollapsed),
 * so the state survives even a crash. The option holds the
 * newline-joined relative paths (Merge20JoinCollapsedPaths).
 */
void CMerge20FileListCtrl::SaveCollapsedState()
{
	COptionsMgr* pOptions = GetOptionsMgr();
	if (pOptions != nullptr)
	{
		pOptions->SaveOption(OPT_MERGE20_FILELIST_COLLAPSED,
			Merge20JoinCollapsedPaths(m_collapsed));
	}
}

/**
 * @brief Slim display (Panel P4, design section 12.4): hide every
 * column except file name and review state.
 *
 * Hiding is by column width 0 (the headers stay, squeezed to
 * nothing, as in the folder list's slim mode the columns are
 * hidden the same way). The widths are remembered first, so
 * un-slimming restores exactly what was there, including a
 * manually widened column. No-op when the state does not change
 * or the list has no columns yet (SetupColumns runs in the bar's
 * OnCreate, before any slim call).
 */
void CMerge20FileListCtrl::ApplySlim(bool slim)
{
	if (GetSafeHwnd() == nullptr || GetHeaderCtrl() == nullptr)
		return;
	if (slim == m_bSlim)
		return;
	m_bSlim = slim;
	if (slim)
	{
		for (int col = 0; col < COL_COUNT; ++col)
			m_savedColWidths[col] = GetColumnWidth(col);
		for (int col = 0; col < COL_COUNT; ++col)
		{
			if (col != COL_FILENAME && col != COL_REVIEW)
				SetColumnWidth(col, 0);
		}
	}
	else
	{
		for (int col = 0; col < COL_COUNT; ++col)
			SetColumnWidth(col, m_savedColWidths[col]);
	}
}

/**
 * @brief True when the displayed row is a folded directory.
 */
bool CMerge20FileListCtrl::IsRowCollapsed(int index) const
{
	const Row* pRow = GetDisplayedRow(index);
	if (pRow == nullptr || !pRow->isDirectory)
		return false;
	return m_collapsed.count(pRow->relPath) != 0;
}

/**
 * @brief Unfold every directory row (P2, "expand all").
 */
void CMerge20FileListCtrl::ExpandAll()
{
	if (m_collapsed.empty())
		return;
	m_collapsed.clear();
	Refresh();
	// P4: the folded set is saved at once (see SaveCollapsedState).
	SaveCollapsedState();
}

/**
 * @brief Fold every directory row that has children (P2,
 * "collapse all").
 */
void CMerge20FileListCtrl::CollapseAll()
{
	for (const Row& row : m_allRows)
	{
		if (row.isDirectory && row.hasChildren)
			m_collapsed.insert(row.relPath);
	}
	Refresh();
	// P4: the folded set is saved at once (see SaveCollapsedState).
	SaveCollapsedState();
}

/**
 * @brief Displayed row by index (nullptr when out of range).
 */
const CMerge20FileListCtrl::Row* CMerge20FileListCtrl::GetDisplayedRow(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_rows.size()))
		return nullptr;
	return &m_rows[static_cast<size_t>(index)];
}

/**
 * @brief Select the displayed row with this relative path and scroll
 * it into view (Panel P3, P-R7).
 *
 * Called by the bar after every Refresh(), so the panel selection
 * always mirrors the file the compare window shows, even when a
 * rescan moved the row. When the row is not displayed (hidden by
 * the search, the filter tab or a collapsed folder, or gone after a
 * rescan), the selection is cleared instead of leaving a stale row
 * selected: the index a stale selection points at may be another
 * file after the rebuild.
 */
void CMerge20FileListCtrl::SelectRowByRelPath(const String& relPath)
{
	if (relPath.empty())
		return;
	for (int i = 0; i < static_cast<int>(m_rows.size()); ++i)
	{
		if (m_rows[static_cast<size_t>(i)].relPath == relPath)
		{
			SetItemState(i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			EnsureVisible(i, FALSE);
			return;
		}
	}
	// Not displayed: clear the selection (nItem -1 applies to all
	// items in CListCtrl::SetItemState).
	SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
}

/**
 * @brief Selected displayed row, or -1 when none is selected.
 */
int CMerge20FileListCtrl::GetSelectedRow() const
{
	return GetNextItem(-1, LVNI_SELECTED);
}

/**
 * @brief Count line for the panel header, same definition as
 * CDirView::GetMerge20StatusCountText (S3, design section 12.3
 * P-R6): displayed files, checked files, unreviewed files.
 * Directory rows have no output flag and are not counted; the
 * denominator is the number of displayed file rows. The same
 * msgid is used, so the wording matches the folder compare
 * status bar exactly.
 */
String CMerge20FileListCtrl::GetCountText() const
{
	int displayed = 0;
	int checked = 0;
	int unreviewed = 0;
	for (const Row& row : m_rows)
	{
		if (row.isDirectory)
			continue;
		++displayed;
		if (row.includeInOutput)
			++checked;
		if (row.reviewState == ReviewState::TODO)
			++unreviewed;
	}
	return strutils::format_string3(_("%1 displayed, %2 checked, %3 unreviewed"),
		strutils::to_str(displayed), strutils::to_str(checked), strutils::to_str(unreviewed));
}

/**
 * @brief Supply the row text for the virtual list.
 *
 * LVIF_TEXT only; one cached string per column. The text pointer
 * stays valid until the next Refresh(), which rebuilds m_rows.
 */
void CMerge20FileListCtrl::OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	*pResult = 0;
	if ((pDispInfo->item.mask & LVIF_TEXT) == 0)
		return;
	const int iItem = pDispInfo->item.iItem;
	const int iSubItem = pDispInfo->item.iSubItem;
	if (iItem < 0 || iItem >= static_cast<int>(m_rows.size()))
		return;
	if (iSubItem < 0 || iSubItem >= COL_COUNT)
		return;
	const Row& row = m_rows[static_cast<size_t>(iItem)];
	const String* pText = nullptr;
	switch (iSubItem)
	{
	case COL_OUTPUT:
		pText = &row.output;
		break;
	case COL_FILENAME:
		pText = &row.fileName;
		break;
	case COL_ACTION:
		pText = &row.action;
		break;
	case COL_RISK:
		pText = &row.risk;
		break;
	case COL_MISSING:
		pText = &row.missing;
		break;
	case COL_MODIFIED:
		pText = &row.modified;
		break;
	case COL_ADDED:
		pText = &row.added;
		break;
	case COL_REVIEW:
		pText = &row.review;
		break;
	default:
		return;
	}
	pDispInfo->item.pszText = const_cast<LPTSTR>(pText->c_str());
}

/**
 * @brief Tint rows by review state and emphasize High risk.
 *
 * Same rule as CDirView::GetColors (S2): SPECIAL = light red,
 * HOLD = light yellow, KEEP = grey; TODO/DONE keep the default
 * background. Only the background is tinted, so the difference
 * status text stays readable (the panel has no diff-status base
 * colours of its own). A High risk gets red text in the Risk cell
 * (P-R2); selected rows keep the stock selection highlight, as in
 * the folder list. The folder list itself has no High-risk
 * emphasis (see the header comment), so this cell emphasis is a
 * deliberate panel-only addition.
 */
void CMerge20FileListCtrl::OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = CDRF_DODEFAULT;
	LPNMLVCUSTOMDRAW lpC = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
	if (lpC->nmcd.dwDrawStage == CDDS_PREPAINT)
	{
		*pResult = CDRF_NOTIFYITEMDRAW;
		return;
	}
	if (lpC->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
	{
		*pResult = CDRF_NOTIFYSUBITEMDRAW;
		return;
	}
	if (lpC->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
	{
		const int nRow = static_cast<int>(lpC->nmcd.dwItemSpec);
		if (nRow < 0 || nRow >= static_cast<int>(m_rows.size()))
			return;
		const Row& row = m_rows[static_cast<size_t>(nRow)];
		switch (row.reviewState)
		{
		case ReviewState::SPECIAL:
			lpC->clrTextBk = RGB(255, 200, 200);
			break;
		case ReviewState::HOLD:
			lpC->clrTextBk = RGB(255, 255, 180);
			break;
		case ReviewState::KEEP:
			lpC->clrTextBk = RGB(230, 230, 230);
			break;
		case ReviewState::TODO:
		case ReviewState::DONE:
		default:
			break;
		}
		if (lpC->iSubItem == COL_RISK && row.riskLevel == RiskLevel::HIGH)
			lpC->clrText = Merge20FileListHighRiskText;
	}
}

/**
 * @brief Right-click on a row (P2): ask the bar to show the row
 * context menu.
 *
 * The row under the cursor is selected first, so the menu acts on
 * it. The bar owns the menu (it has the CDirDoc for the directory
 * operations); the control only reports the row index, through
 * WM_MERGE20_FILELIST_ROW_RCLICK to its parent.
 */
void CMerge20FileListCtrl::OnRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	LPNMITEMACTIVATE pNM = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	const int iItem = pNM->iItem;
	if (iItem < 0 || iItem >= static_cast<int>(m_rows.size()))
		return;
	SetItemState(iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	CWnd* pParent = GetParent();
	if (pParent != nullptr && pParent->GetSafeHwnd() != nullptr)
		pParent->SendMessage(WM_MERGE20_FILELIST_ROW_RCLICK, static_cast<WPARAM>(iItem), 0);
}

/**
 * @brief Double-click on a directory row (P2, P-R5): fold or
 * unfold that folder only. File rows do nothing (single-click
 * navigation is P3's, so the double-click is free for P2).
 */
void CMerge20FileListCtrl::OnDblclk(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	LPNMITEMACTIVATE pNM = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	const int iItem = pNM->iItem;
	if (iItem < 0 || iItem >= static_cast<int>(m_rows.size()))
		return;
	const Row& row = m_rows[static_cast<size_t>(iItem)];
	if (!row.isDirectory || !row.hasChildren)
		return;
	SetCollapsed(row.relPath, m_collapsed.count(row.relPath) == 0);
}

/**
 * @brief Single click on a row (P3, P-R7/P-R8): report the row and
 * the clicked column to the bar, which dispatches by column (output
 * column toggles the flag, review column cycles the state, other
 * columns open the file). The row is already selected by the list
 * itself (selection happens on mouse-down, NM_CLICK on mouse-up).
 * Clicks on empty area (iItem -1) are ignored. The bar only posts
 * the open request to the frame (async), so this synchronous send
 * returns while the control still exists, even though opening a
 * file closes this frame once the posted message is pumped.
 */
void CMerge20FileListCtrl::OnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	LPNMITEMACTIVATE pNM = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	const int iItem = pNM->iItem;
	const int iSubItem = pNM->iSubItem;
	if (iItem < 0 || iItem >= static_cast<int>(m_rows.size()))
		return;
	if (iSubItem < 0 || iSubItem >= COL_COUNT)
		return;
	CWnd* pParent = GetParent();
	if (pParent != nullptr && pParent->GetSafeHwnd() != nullptr)
		pParent->SendMessage(WM_MERGE20_FILELIST_ROW_CLICK, static_cast<WPARAM>(iItem), static_cast<LPARAM>(iSubItem));
}
