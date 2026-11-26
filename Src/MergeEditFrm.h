/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  MergeEditFrm.h
 *
 * @brief interface of the CMergeEditFrame class
 *
 */
#pragma once

#include "SplitterWndEx.h"
#include "MergeEditSplitterView.h"
#include "MergeStatusBar.h"
#include "EditorFilepathBar.h"
#include "DiffViewBar.h"
#include "Merge20/Merge20FocusDock.h"
#include "Merge20/Merge20DedBar.h"
#include "MergeResultBar.h"
#include "LocationBar.h"
#include "LineFilterBar.h"
#include "MergeFrameCommon.h"

class CMergeDoc;
class CMergeResultView;

/** 
 * @brief Frame class for file compare, handles panes, statusbar etc.
 */
class CMergeEditFrame : public CMergeFrameCommon
{
	DECLARE_DYNCREATE(CMergeEditFrame)
public:
	CMergeEditFrame();

// Operations
public:
	void UpdateResources();
	IHeaderBar * GetHeaderInterface() override;
	CMergeDoc * GetMergeDoc() { return m_pMergeDoc; }

	void UpdateAutoPaneResize();
	void UpdateSplitter();
	void LoadOptions();
	void SaveOptions();
	CSplitterWndEx& GetSplitter() { return m_wndSplitter; };
	void ShowFilterBar();
	void HideFilterBar();
	CLineFilterBar* GetFilterBar() { return m_pFilterBar.get(); }
	void ShowMergeResultPane(bool bVisible = true);
	void SetDedicatedMode(bool bEnable);
	bool IsDedicatedMode() const { return m_bDedicatedMode; }
	void FlashDedBar();

// Attributes
protected:
	CSplitterWndEx m_wndSplitter;
	CEditorFilePathBar m_wndFilePathBar;
	CDiffViewBar m_wndDetailBar;
	CMerge20FocusDock m_wndFocusDock; /**< Focus dock (Merge20 Phase 4 D4): the Diff Pane's single child, holding the header strip, the detail splitter view and the result-segment view */
	CMerge20DedBar m_wndDedBar; /**< Dedicated-mode bar (Merge20 Phase 4 D1) */
	CMergeEditSplitterView *m_pwndDetailMergeEditSplitterView;
	CMergeResultBar m_wndResultBar;
	CMergeResultView *m_pMergeResultView;
	bool m_bResultBarVisible; /**< Last seen visibility of the merge result bar */
	bool m_bDedicatedMode; /**< Dedicated compare mode (Merge20 Phase 4 D1) is active */
	bool m_bDedWasMaximized; /**< Child frame was maximized before entering dedicated mode */
	bool m_bDedFilePathBarVisible; /**< Saved visibility of the bars hidden in dedicated mode */
	bool m_bDedLocationBarVisible;
	bool m_bDedDetailBarVisible;
	bool m_bDedResultBarVisible;
	bool m_bDedStatusBarVisible;
	int m_nDedPrevZoom; /**< OPT_VIEW_ZOOM before entering dedicated mode (D2: restored on exit) */
	ULONGLONG m_ullDedBarFlashUntil; /**< Tick until which the bar stays shown (D2 flash, 1.6s) */
	ULONGLONG m_ullDedBarLastMouseOver; /**< Last tick the cursor was over the bar (D2 auto-hide) */
	CMergeStatusBar m_wndStatusBar;
	CLocationBar m_wndLocationBar;
	std::unique_ptr<CLineFilterBar> m_pFilterBar;
	class CPreviewNumPageButton : public CButton
	{
		virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	} m_wndPreviewNumPage;

// Overrides
public:
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMergeEditFrame)
	public:
	virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
	virtual BOOL OnBarCheck(UINT nID);
	virtual void ActivateFrame(int nCmdShow = -1);
	virtual BOOL DestroyWindow();
	virtual BOOL PreTranslateMessage(MSG* pMsg) override;
	virtual void OnSetPreviewMode(BOOL bPreview, CPrintPreviewState* pState) override;
	protected:
	//}}AFX_VIRTUAL

// Implementation
private:
	void SavePosition();
	void EnterDedicatedMode();
	void ExitDedicatedMode();
	void UpdateDedBar();
	void SaveActivePane();
	virtual ~CMergeEditFrame();
	CSplitterWndEx& GetMergeEditSplitterWnd(int iRow)
	{ return static_cast<CMergeEditSplitterView *>(m_wndSplitter.GetPane(iRow, 0))->m_wndSplitter; }

// Generated message map functions
private:
	void UpdateHeaderSizes();
	CMergeDoc * m_pMergeDoc;

	//{{AFX_MSG(CMergeEditFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnClose();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnMDIActivate(BOOL bActivate, CWnd* pActivateWnd, CWnd* pDeactivateWnd);
	afx_msg void OnViewSplitVertically();
	afx_msg void OnUpdateViewSplitVertically(CCmdUI* pCmdUI);
	afx_msg void OnWindowPreserveSplitterPosition();
	afx_msg void OnUpdateWindowPreserveSplitterPosition(CCmdUI* pCmdUI);
	afx_msg void OnUpdateViewMergeResultBar(CCmdUI* pCmdUI);
	afx_msg void OnViewMerge20DedicatedMode();
	afx_msg void OnUpdateViewMerge20DedicatedMode(CCmdUI* pCmdUI);
	afx_msg void OnMerge20DedExit();
	afx_msg void OnMerge20DedPrevFile();
	afx_msg void OnMerge20DedNextFile();
	afx_msg void OnMerge20DedReview(UINT nID);
	afx_msg void OnMerge20DedPin();
	afx_msg void OnMerge20DedTodoOnly();
	afx_msg void OnMerge20DedSniper();
	afx_msg void OnMerge20DedZoomReset();
	afx_msg LRESULT OnStorePaneSizes(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnIdleUpdateCmdUI();
	afx_msg void OnViewDisplayFilterBar();
	afx_msg void OnUpdateDisplayViewFilterBar(CCmdUI* pCmdUI);
	afx_msg void OnDisplayFilterBarClose();
	afx_msg void OnDisplayFilterBarMenu();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
