// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FocusDock.h
 *
 * @brief Declaration of CMerge20FocusDock class
 *
 * Merge20 (Phase 4 D4, design sections 3.5 and 5.2/D4, spec
 * sections 3 (3/4) and 5.2): the focus dock. It is the single child
 * of the Diff Pane bar (CDiffViewBar uses SCBS_SIZECHILD, which
 * only allows one child window; the container pattern follows
 * CMergeResultContainer), and it holds the bar's three parts:
 * the header strip (diff position label + judgement buttons +
 * split/regroup buttons), the existing detail splitter view
 * (old/new panes), and the read-only result-segment view
 * (CMerge20FocusResultView).
 *
 * The dock owns no state of its own: the document tells it the
 * current diff (CMergeDoc::SetCurrentDiff, ResultChooseSources,
 * BuildMergeResult), and it asks the document for the segment text.
 * Button clicks apply through CMergeDoc::Merge20FocusApplyJudgment
 * (ResultChooseSources + the Phase 3 L2 learning record). The
 * split/regroup buttons are display only in D4 (D5 wires the body).
 * Folding and height adjustment are the docking bar's standard
 * resize and ShowControlBar display switch (design section 3.5,
 * T-4-7): the dock lays its two panes out proportionally, so a
 * dragged bar height still works.
 */
#pragma once

#include "Merge20/Merge20Focus.h"

class CMergeDoc;
class CMergeEditSplitterView;
class CMerge20FocusResultView;

class CMerge20FocusDock : public CWnd
{
	DECLARE_DYNAMIC(CMerge20FocusDock);
public:
	CMerge20FocusDock();
	virtual ~CMerge20FocusDock();
	virtual BOOL Create(CWnd* pParentWnd);

	/**
	 * @brief Attach the views the frame created (the dock does not
	 * own them: both are CViews and delete themselves on window
	 * destruction). Called once, right after creation.
	 */
	void SetViews(CMergeEditSplitterView* pSplitterView, CMerge20FocusResultView* pResultView);
	/**
	 * @brief Attach the document and register this dock with it, so
	 * diff changes and judgements refresh the dock.
	 */
	void SetMergeDoc(CMergeDoc* pDoc);
	void UpdateResources();

	/**
	 * @brief Show the given diff's three segments: refresh the
	 * result view's text, the header label and the buttons. The
	 * old/new panes follow through their own OnDisplayDiff; this
	 * only handles the third segment and the header.
	 */
	void ShowDiff(int nDiff);

	/** @brief Reentrancy flag for the explicit scroll sync (design section 3.3). */
	bool IsSyncingScroll() const { return m_bSyncingScroll; }
	/**
	 * @brief Scroll the result view to the given offset from the
	 * diff start (called from a detail pane's ScrollToSubLine).
	 */
	void SyncFocusFromDetail(int nDetailTopOffset);
	/**
	 * @brief Scroll the detail panes so the diff start plus the
	 * given focus-view offset is at the top (called from the result
	 * view's ScrollToSubLine).
	 */
	void SyncDetailFromFocus(int nFocusTopLine);

	//{{AFX_MSG(CMerge20FocusDock)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnButtonClicked(UINT nID);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void LayoutHeader();
	void LayoutViews(int cx, int cy);
	void UpdateHeader();
	String StateWord(int nState) const;

	CStatic m_stInfo; /**< Diff position + state label, first in the header */
	// Judgement buttons, in header order (HTML focus dock order):
	// old-only blocks get InsertOld/Reject, changed blocks get
	// New/Old/Both; Split/Regroup are always shown.
	CButton m_btnInsertOld;
	CButton m_btnReject;
	CButton m_btnNew;
	CButton m_btnOld;
	CButton m_btnBoth;
	CButton m_btnSplit;
	CButton m_btnRegroup;

	CMergeDoc* m_pDoc; /**< Owning document, or nullptr */
	CMergeEditSplitterView* m_pDetailSplitterView; /**< Old/new panes (frame-owned) */
	CMerge20FocusResultView* m_pResultView; /**< Result segment view (frame-owned) */
	int m_nCurrentDiff; /**< Diff shown, or -1 */
	int m_nCurrentOp; /**< Its DIFFRANGE op (Merge20FocusOp* value) */
	int m_nSegmentState; /**< Its ResultSegmentState value, or -1 */
	int m_nDiffCount; /**< Diffs in the document */
	int m_nBuffers; /**< Compared panes */
	bool m_bResultBuilt; /**< A merge session is active */
	bool m_bSyncingScroll; /**< True while a sync scroll is applied */
};
