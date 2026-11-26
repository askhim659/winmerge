// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FileListCtrl.cpp
 *
 * @brief Implementation file for CMerge20FileListCtrl
 *
 * Merge20 (Panel P0, design section 12.4/12.7): virtual list of the
 * folder compare's files. P0 shows the file name only; the full
 * columns are P1. Rows are cached display strings built in
 * Refresh(), so LVN_GETDISPINFO only hands out pointers to strings
 * that live until the next Refresh() (the CDirView equivalent uses
 * AllocDispinfoText because its text is computed on the fly; here
 * the cache already owns the text).
 */

#include "StdAfx.h"
#include "Merge20/Merge20FileListCtrl.h"
#include "DiffContext.h"
#include "DirViewColItems.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20FileListCtrl, CListCtrl);

BEGIN_MESSAGE_MAP(CMerge20FileListCtrl, CListCtrl)
	//{{AFX_MSG_MAP(CMerge20FileListCtrl)
	ON_NOTIFY_REFLECT(LVN_GETDISPINFO, OnGetdispinfo)
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
 * @brief Rebuild the cached rows from the folder context.
 *
 * The iteration is the flat depth-first walk (GetFirstDiffPosition /
 * GetNextDiffPosition), the same pattern CDirView uses to visit every
 * item: directory rows are included, in compare order, as in the
 * folder compare list. P1/P2 add the hierarchy (expand/collapse) and
 * the remaining columns; P0 lists every DIFFITEM flat.
 */
void CMerge20FileListCtrl::Refresh()
{
	m_items.clear();
	if (m_pCtxt != nullptr)
	{
		for (DIFFITEM* p = m_pCtxt->GetFirstDiffPosition(); p != nullptr;)
		{
			const DIFFITEM& di = m_pCtxt->GetNextDiffPosition(p);
			m_items.push_back(Merge20CsvGetFileNameText(m_pCtxt, di));
		}
	}
	SetItemCount(static_cast<int>(m_items.size()));
}

/**
 * @brief Supply the row text for the virtual list.
 *
 * Only LVIF_TEXT is served (P0 has one column). The text pointer
 * stays valid until the next Refresh(), which rebuilds m_items.
 */
void CMerge20FileListCtrl::OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	*pResult = 0;
	if ((pDispInfo->item.mask & LVIF_TEXT) == 0)
		return;
	const int iItem = pDispInfo->item.iItem;
	if (iItem < 0 || iItem >= static_cast<int>(m_items.size()))
		return;
	pDispInfo->item.pszText = const_cast<LPTSTR>(m_items[static_cast<size_t>(iItem)].c_str());
}
