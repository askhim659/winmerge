// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FileListCtrl.cpp
 *
 * @brief Implementation file for CMerge20FileListCtrl
 *
 * Merge20 (Panel P0/P1, design section 12.4/12.7): virtual list of
 * the folder compare's files. P1 shows six columns (selection,
 * file name, action category, risk, old/changed/new counts,
 * review state). Every display value comes from the S6 wrappers
 * (Merge20CsvGet*Text in DirViewColItems.h), which forward to the
 * same static getters the folder compare list uses, so the panel
 * shows exactly what the list shows. The old/changed/new column
 * is the P-R1 provisional form (one column with the counts part
 * of the summary text) until H-LIST adds the three columns.
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
#include "DiffContext.h"
#include "DirViewColItems.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20FileListCtrl, CListCtrl);

namespace
{
	/** @brief Column widths (px), in display order. The panel is
	 * 480px wide by default, so the list scrolls horizontally
	 * until the user widens it; the file name and the counts
	 * column get the most room. */
	const int Merge20FileListColWidths[6] =
	{
		48, // selection (checkbox glyph, centred)
		160, // file name
		140, // action category
		64, // risk (centred)
		220, // old/changed/new counts (provisional)
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
 * @brief Insert the six P1 columns (call once, after Create).
 *
 * Header texts reuse the folder compare list's msgids
 * ("Include in Output", "Filename", "Action Category", "Risk",
 * "Review State"), so the headers match the list exactly,
 * including the [Wiz] mark W1 put on the Merge20 headers. The
 * old/changed/new column is the exception: it has no folder-list
 * counterpart yet (H-LIST), so it uses the new msgid
 * "Old/Changed/New" ([Wiz] old/changed/new in Japanese).
 */
void CMerge20FileListCtrl::SetupColumns()
{
	// Narrow literals: _() takes the msgid as const char* (the
	// msgids themselves are ASCII, so the table below is exact).
	static const char* const headers[COL_COUNT] =
	{
		"Include in Output",
		"Filename",
		"Action Category",
		"Risk",
		"Old/Changed/New",
		"Review State",
	};
	static const int formats[COL_COUNT] =
	{
		LVCFMT_CENTER,
		LVCFMT_LEFT,
		LVCFMT_LEFT,
		LVCFMT_CENTER,
		LVCFMT_LEFT,
		LVCFMT_LEFT,
	};
	for (int col = 0; col < COL_COUNT; ++col)
	{
		String sHeader = _(headers[col]);
		InsertColumn(col, sHeader.c_str(), formats[col], Merge20FileListColWidths[col]);
	}
}

/**
 * @brief Re-apply the column header texts (language change).
 *
 * The same msgid table as SetupColumns: the headers must follow
 * the GUI language exactly like the folder compare list does.
 */
void CMerge20FileListCtrl::UpdateColumnHeaders()
{
	static const char* const headers[COL_COUNT] =
	{
		"Include in Output",
		"Filename",
		"Action Category",
		"Risk",
		"Old/Changed/New",
		"Review State",
	};
	for (int col = 0; col < COL_COUNT; ++col)
	{
		LVCOLUMN lvc = {};
		lvc.mask = LVCF_TEXT;
		String sHeader = _(headers[col]);
		lvc.pszText = const_cast<LPTSTR>(sHeader.c_str());
		SetColumn(col, &lvc);
	}
}

/**
 * @brief Rebuild the cached rows from the folder context.
 *
 * The iteration is the flat depth-first walk (GetFirstDiffPosition /
 * GetNextDiffPosition), the same pattern CDirView uses to visit every
 * item: directory rows are included, in compare order, as in the
 * folder compare list. P2 adds the hierarchy (expand/collapse).
 * Every display value comes from the S6 wrappers, so the panel and
 * the folder list can never disagree.
 */
void CMerge20FileListCtrl::Refresh()
{
	m_rows.clear();
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
			row.oldChangedNew = Merge20CsvGetOldChangedNewText(m_pCtxt, di);
			row.review = Merge20CsvGetReviewText(m_pCtxt, di);
			const String relPath = di.getItemRelativePath();
			row.reviewState = m_pCtxt->GetMerge20ReviewState(relPath);
			row.riskLevel = m_pCtxt->GetMerge20FileStates().GetState(relPath).risk;
			row.isDirectory = di.diffcode.isDirectory();
			row.includeInOutput = m_pCtxt->GetMerge20IncludeInOutput(di);
			m_rows.push_back(std::move(row));
		}
	}
	SetItemCount(static_cast<int>(m_rows.size()));
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
	case COL_OLD_CHANGED_NEW:
		pText = &row.oldChangedNew;
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
