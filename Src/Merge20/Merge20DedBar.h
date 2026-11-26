// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20DedBar.h
 *
 * @brief Declaration of CMerge20DedBar class
 *
 * Merge20 (Phase 4 D1/D2, design sections 3.1 and 5.2/D1/D2, spec
 * section 3 (4/4)): the dedicated compare mode bar. It follows the
 * CDiffViewBar precedent (CSizingControlBarCF docked to the
 * CMergeEditFrame). D2 adds the buttons: Prev/Next file, the five
 * review-state buttons (TODO/DONE/HOLD/SPECIAL/KEEP, same effect as
 * the S2 keys 1-5), the pin toggle, the unreviewed-only and sniper
 * toggles (display + state only; D3/D6 add the behaviour), the zoom
 * display (click to reset to 75%), and the exit button. D9 adds the diffs-only toggle button (F key; same effect as View > Diff Context > 0 Lines). D3 adds the
 * file-info label at the left end: the current file name and its
 * position (n/m) in the patrol set (all files, or only the TODO files
 * while unreviewed-only is on). The bar itself owns no state: button
 * clicks are forwarded as WM_COMMAND (ID_MERGE20_DED_*) to the parent
 * frame, which applies them and refreshes the button states through
 * the setters below.
 */
#pragma once

#include "scbarcf.h"
#include "resource.h"
#include "Merge20/Merge20Types.h"

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

	// Merge20 (Phase 4 D2, bug fix for the D1 report "dedicated mode
	// shows nothing"): the bar has one fixed row of buttons, so its
	// height is fixed to Merge20DedBarHeight. LoadState forces that
	// height over any persisted value (D1 saved the 200px default,
	// and a dragged height persisted too), and CalcFixedLayout
	// clamps the height the layout gives the bar, so dragging the
	// bar's edge can never steal client area from the file panes.
	// See MERGE20_NOTES.md section 24.6.
	virtual void LoadState(LPCTSTR lpszProfileName) override;
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz) override;

	// State refresh, called by CMergeEditFrame::OnIdleUpdateCmdUI.
	// Each setter is a no-op when the value did not change, so the
	// idle-time refresh does not flicker the buttons.
	void SetReviewState(ReviewState state);
	void SetReviewEnabled(bool bEnable);
	void SetPrevNextEnabled(bool bPrev, bool bNext);
	void SetPinned(bool bPinned);
	void SetTodoOnly(bool bTodoOnly);
	void SetSniperOn(bool bSniperOn);
	void SetDiffsOnly(bool bDiffsOnly);
	void SetZoomText(int nZoom);
	void SetFileInfo(const String& sText);

	//{{AFX_MSG(CMerge20DedBar)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnButtonClicked(UINT nID);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void LayoutButtons();

	HWND m_hwndFrame; //*< Frame window handle */
	// File-info label, first in the bar (spec section 5.2, D3): the
	// current file name and its (n/m) position in the patrol set.
	// It is a label, not a button, so it is created and laid out
	// separately from m_btn* in Merge20DedBar.cpp.
	CStatic m_stFileInfo;
	// Buttons, in bar order (spec section 5.2): Prev/Next file, the
	// five review states, pin, unreviewed-only, sniper, zoom, exit.
	CButton m_btnPrev;
	CButton m_btnNext;
	CButton m_btnReview[5];
	CButton m_btnPin;
	CButton m_btnTodoOnly;
	CButton m_btnSniper;
	CButton m_btnDiffsOnly;
	CButton m_btnZoom;
	CButton m_btnExit;
	ReviewState m_reviewState; /**< Last review state shown on the buttons */
	int m_nZoom; /**< Last zoom (thousandths) shown on the zoom button */
	String m_sFileInfo; /**< Last file-info text shown (avoids idle flicker) */
};
