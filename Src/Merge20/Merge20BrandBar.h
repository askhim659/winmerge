// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20BrandBar.h
 *
 * @brief Declaration of CMerge20BrandBar class
 *
 * Merge20 (UI U1, design sections 2.1 and 4/U1): the WizMerge brand
 * header, an app-internal header bar docked at the very top of the
 * main frame, above the menu/toolbar ReBar. It follows the
 * CMerge20DedBar precedent (CSizingControlBarCF, fixed height
 * enforced in LoadState / CalcFixedLayout). OnPaint draws the
 * glowing cyan dot + bold "WizMerge" wordmark + product version on
 * the left (HTML top bar) and the active folder compare's progress
 * summary (S3 GetMerge20StatusCountText, same definition) on the
 * right; a file compare shows "File Compare", anything else shows
 * nothing on the right. The summary is refreshed on a 500ms timer
 * (the main frame has no idle-update hook for its bars), so review
 * changes show up without wiring into every state-change site.
 * The bar owns no commands: the View-menu toggle is the plain
 * control-bar toggle (OnBarCheck) in CMainFrame, persisted in
 * OPT_MERGE20_BRAND_HEADER.
 */
#pragma once

#include "scbarcf.h"
#include "resource.h"

class CMerge20BrandBar : public CSizingControlBarCF
{
	DECLARE_DYNAMIC(CMerge20BrandBar);
public:
	CMerge20BrandBar();
	virtual ~CMerge20BrandBar();
	virtual BOOL Create(
		CWnd* pParentWnd,			// mandatory
		LPCTSTR lpszWindowName = nullptr,
		DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_TOP,
		UINT nID = AFX_IDW_PANE_FIRST);

	void UpdateResources();

	// Merge20 (UI U1): the bar is one fixed 30px row, so its height
	// is fixed to Merge20BrandBarHeight. LoadState forces that height
	// over any persisted value (D1 saved the 200px default, and a
	// dragged height persisted too), and CalcFixedLayout clamps the
	// height the layout gives the bar, so dragging the bar's edge can
	// never steal client area from the views. See
	// MERGE20_NOTES.md section 24.6.
	virtual void LoadState(LPCTSTR lpszProfileName) override;
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz) override;

	//{{AFX_MSG(CMerge20BrandBar)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnPaint();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	/** @brief Re-read the summary text; repaint only when it changed. */
	void RefreshSummary();
	/** @brief Current right-end text for the active document. */
	String GetSummaryText();

	CFont m_fontBold; /**< Bold wordmark font (Merge20Theme::CreateBoldFont) */
	String m_sVersion; /**< Product version (CVersionInfo), e.g. "2.16.60" */
	String m_sSummaryText; /**< Last progress summary shown on the right */
	bool m_bDark; /**< Dark mode flag the last refresh saw */
};
