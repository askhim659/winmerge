// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FileListCtrl.h
 *
 * @brief Declaration of CMerge20FileListCtrl class
 *
 * Merge20 (Panel P0, design section 12.4/12.7): the file list inside
 * the persistent file-list panel (CMerge20FileListBar). A CListCtrl
 * in virtual-list mode (LVS_OWNERDATA, the same approach as CDirView,
 * confirmed in T-2-4), so 100-1000 files stay cheap to draw. The
 * rows are the DIFFITEMs of the folder compare that opened this file
 * compare, in compare order, read through CDirDoc::GetDiffContext()
 * (design section 12.5); the control never touches CDiffContext
 * internals. Display values reuse the S6 wrappers
 * (Merge20CsvGetFileNameText), so the panel shows the same file name
 * as the folder compare list.
 *
 * P0 shows one column only (file name). The remaining columns
 * (selection, action category, risk, old/changed/new counts, review
 * state), row colours, the count line and the hierarchy are P1/P2.
 */
#pragma once

#include <vector>
#include "UnicodeString.h"

class CDiffContext;

class CMerge20FileListCtrl : public CListCtrl
{
	DECLARE_DYNAMIC(CMerge20FileListCtrl);
public:
	CMerge20FileListCtrl();
	virtual ~CMerge20FileListCtrl();

	/** @brief Folder context to list (nullptr = no folder compare). */
	void SetDiffContext(const CDiffContext* pCtxt) { m_pCtxt = pCtxt; }
	/** @brief Rebuild the cached rows from the context. */
	void Refresh();

	//{{AFX_MSG(CMerge20FileListCtrl)
	afx_msg void OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	const CDiffContext* m_pCtxt; /**< Folder context (owned by CDirDoc, not by this control) */
	std::vector<String> m_items; /**< Cached file-name texts, in compare order */
};
