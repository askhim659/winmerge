// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FileListBar.h
 *
 * @brief Declaration of CMerge20FileListBar class
 *
 * Merge20 (Panel P0/P1, design section 12.4/12.7): the persistent
 * file-list panel, docked at the left of the file compare frame.
 * It follows the CDiffViewBar / Merge20DedBar precedent
 * (CSizingControlBarCF). The bar owns three children: the count
 * line (P1, design section 12.3 P-R6), the virtual list
 * (CMerge20FileListCtrl) and the empty-state label shown when the
 * file compare was not opened from a folder compare
 * ("No folder comparison", design section 12.5).
 *
 * The bar owns no folder state: the frame hands it the CDirDoc
 * (SetDirDoc) and calls Refresh() when the list may have changed
 * (frame creation, View-menu toggle, MDI activation, and the
 * MSG_MERGE20_FILELIST_REFRESH notification posted by CDiffContext).
 * The width is clamped to 160-600px (D2 lesson: a saved or dragged
 * size must never steal client area), and the initial width is set
 * explicitly (D1 lesson: an empty sizing bar must not take its size
 * from layout defaults).
 *
 * P0 scope: one file-name column, no row operations. P1 adds the
 * six columns, the row colours and the count line. Search,
 * hierarchy, batch check and the panel <-> compare navigation
 * are P2-P3.
 */
#pragma once

#include "scbarcf.h"
#include "resource.h"
#include "Merge20/Merge20FileListCtrl.h"

class CDirDoc;

class CMerge20FileListBar : public CSizingControlBarCF
{
	DECLARE_DYNAMIC(CMerge20FileListBar);
public:
	CMerge20FileListBar();
	virtual ~CMerge20FileListBar();
	virtual BOOL Create(
		CWnd* pParentWnd,			// mandatory
		LPCTSTR lpszWindowName = nullptr,
		DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_TOP,
		UINT nID = AFX_IDW_PANE_FIRST);

	void SetFrameHwnd(HWND hwndFrame);
	/** @brief Folder document to list (nullptr = no folder compare). */
	void SetDirDoc(CDirDoc* pDirDoc) { m_pDirDoc = pDirDoc; }
	/** @brief Rebuild the list from the folder document. No-op while hidden. */
	void Refresh();
	void UpdateResources();

	// Merge20 (Panel P0, D2 lesson): clamp the persisted width so a
	// saved abnormal value can never come back and steal client area.
	virtual void LoadState(LPCTSTR lpszProfileName) override;
	// Merge20 (Panel P0, D2 lesson): clamp the laid-out width, so
	// dragging the bar's edge can never take the file panes' area.
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz) override;

	//{{AFX_MSG(CMerge20FileListBar)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void LayoutChildren();

	HWND m_hwndFrame; //*< Frame window handle */
	CDirDoc* m_pDirDoc; /**< Folder document (owned by the folder compare, not by this bar) */
	CStatic m_stCount; /**< Count line (P1, P-R6): "N displayed, M checked, K unreviewed" */
	CMerge20FileListCtrl m_wndList; /**< Virtual file list */
	CStatic m_stEmpty; /**< Empty state: "No folder comparison" */
};
