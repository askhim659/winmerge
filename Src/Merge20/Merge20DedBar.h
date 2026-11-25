// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20DedBar.h
 *
 * @brief Declaration of CMerge20DedBar class
 *
 * Merge20 (Phase 4 D1, design sections 3.1 and 5.2/D1, spec section 3
 * (4/4)): the dedicated compare mode bar. It follows the CDiffViewBar
 * precedent (CSizingControlBarCF docked to the CMergeEditFrame): D1
 * only wires its show/hide to the dedicated mode. The bar is empty
 * for now (caption only); D2 adds the Prev/Next, review-state and
 * exit buttons, the auto-hide + pin, and the zoom display.
 */
#pragma once

#include "scbarcf.h"

class CMerge20DedBar : public CSizingControlBarCF
{
	DECLARE_DYNAMIC(CMerge20DedBar);
public:
	CMerge20DedBar();
	virtual ~CMerge20DedBar();
	virtual BOOL Create(
		CWnd* pParentWnd,			// mandatory
		LPCTSTR lpszWindowName = nullptr,
		DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_TOP,
		UINT nID = AFX_IDW_PANE_FIRST);

	void SetFrameHwnd(HWND hwndFrame);
	void UpdateResources();

	//{{AFX_MSG(CMerge20DedBar)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	HWND m_hwndFrame; //*< Frame window handle */
};
