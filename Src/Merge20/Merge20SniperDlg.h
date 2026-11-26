// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20SniperDlg.h
 *
 * @brief Declaration of CMerge20SniperDlg class
 *
 * Merge20 (Phase 4 D6, design sections 3.3, 3.6 and 5.2/D6, spec
 * sections 3 (4/4), 4(g), 5.4): the auto sniper overlay. A modeless
 * dialog that covers the whole monitor and shows only the current
 * diff in three read-only panels (OLD / NEW / MERGED).
 *
 * The dialog is owned by the frame (CMergeEditFrame::m_wndSniper):
 * it is created once, shown on demand and hidden on close, never
 * deleted on close, so no PostNcDestroy self-delete is needed
 * (there is no modeless-dialog precedent in the codebase to copy,
 * and a frame-owned member is the conservative choice). It opens
 * only from the result view's double-click while sniper mode is ON
 * and dedicated mode is active; leaving dedicated mode closes it
 * and turns the sniper off (ExitDedicatedMode already clears
 * OPT_MERGE20_SNIPER_ON; the frame also closes this dialog there).
 *
 * The three panels are CMerge20SniperView (own buffers, D4
 * precedent), not CMergeEditView (T-4-2: a CMergeEditView is
 * registered in CMergeDoc::m_pView[group][buffer] and takes part
 * in UpdateAllViews / SelectDiff, which these display-only panels
 * must not do). The OLD/NEW texts come from
 * CMergeDoc::GetPaneApparentLinesText and the MERGED text from
 * CMergeDoc::Merge20GetFocusResultText (the D4 focus-dock API).
 */
#pragma once

#include "TrDialogs.h"
#include "Merge20/Merge20Sniper.h"
#include "resource.h"

class CMergeDoc;
class CMerge20SniperView;

class CMerge20SniperDlg : public CTrDialog
{
	DECLARE_DYNAMIC(CMerge20SniperDlg);
public:
	CMerge20SniperDlg();
	virtual ~CMerge20SniperDlg();

	BOOL Create(CWnd* pParentWnd);
	/**
	 * @brief Show the overlay for the given diff of the given doc.
	 * Creates the dialog on first use, sizes it to the monitor and
	 * gives the focus to the first panel.
	 */
	void OpenSniper(CMergeDoc* pDoc, int nDiff);
	/** @brief Hide the overlay and return the focus to the frame. */
	void CloseSniper();
	/** @brief True while the overlay is shown. */
	bool IsOpen() const { return m_hWnd != nullptr && !!IsWindowVisible(); }
	/** @brief Show the given diff's three panels. */
	void ShowDiff(int nDiff);
	/**
	 * @brief Move to the next (+1) or previous (-1) significant
	 * diff, wrapping around both ends (spec section 4(g)). The
	 * main window follows through SelectDiff.
	 */
	void MoveDiff(int nDirection);
	/** @brief Scroll all three panels by the given line count. */
	void ScrollByLines(int nLines);
	/**
	 * @brief Scroll the other two panels to the given top line
	 * (called from a panel's ScrollToSubLine; the reentrancy flag
	 * makes the callback a no-op).
	 */
	void SyncFromView(CMerge20SniperView* pSource, int nTopLine);
	/**
	 * @brief Handle one sniper key (spec section 4(g)): Esc closes,
	 * J / Right moves to the next diff, K / Left to the previous
	 * one, Up/Down scrolls 120px, PgUp/PgDn/Space scrolls 600px.
	 * @return True when the key was handled.
	 */
	bool HandleSniperKey(UINT nChar);

protected:
	virtual BOOL OnInitDialog() override;
	virtual void OnCancel() override;
	virtual BOOL PreTranslateMessage(MSG* pMsg) override;

	//{{AFX_MSG(CMerge20SniperDlg)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnButtonClicked(UINT nID);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void LayoutAll();
	void LayoutHeader(int cx);
	void LayoutPanels(int cx, int cy);
	void UpdateHeader();
	void ApplyFont();
	String StateWord(int nState) const;

	// Header strip (button row), in order: title, position,
	// previous/next diff, layout toggle, font down/up, split,
	// regroup, close.
	CStatic m_stTitle;
	CStatic m_stPos;
	CStatic m_stLabel[3]; /**< Panel labels: OLD / NEW / MERGED */
	CButton m_btnPrev;
	CButton m_btnNext;
	CButton m_btnLayout;
	CButton m_btnFontDown;
	CButton m_btnFontUp;
	CButton m_btnSplit;
	CButton m_btnRegroup;
	CButton m_btnClose;
	CMerge20SniperView* m_pView[3]; /**< OLD / NEW / MERGED panels */

	CMergeDoc* m_pDoc; /**< Owning document, or nullptr */
	int m_nCurrentDiff; /**< Diff shown, or -1 */
	int m_nDiffCount; /**< Significant diffs in the document */
	int m_nBuffers; /**< Compared panes */
	bool m_bResultBuilt; /**< A merge session is active */
	int m_nSegmentState; /**< Its ResultSegmentState value, or -1 */
	Merge20SniperLayout m_layout; /**< Current panel layout */
	bool m_bSyncingScroll; /**< True while a sync scroll is applied */
};
