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
 * six columns, the row colours and the count line. P2 adds the
 * operations (design section 12.7): the search box, the
 * action-category filter tabs, the batch-check / expand-all /
 * collapse-all buttons, and the directory-row context menu
 * (fold/unfold, check/uncheck everything below the folder).
 * Search, hierarchy, batch check and the panel <-> compare
 * navigation are P2-P3.
 *
 * P3 adds the linkage (design section 12.7): a single click on a
 * row opens the file (or activates its frame when already open),
 * the output column toggles the flag, the review column cycles the
 * state, keys 1-5 / Space and the file-row context menu set the
 * review state, and the panel selection follows the file the
 * compare window shows (SetCurrentFileRelPath, called by the frame
 * after every Refresh()). Panel-initiated changes are also pushed
 * to the folder view (row repaint + status-bar count) through the
 * CDirDoc forwarders, closing the P2 known limitation.
 *
 * P4 adds the save/restore (design section 12.7): the search
 * string, the selected filter tab, the folded directories and the
 * slim flag are persisted in the OPT_MERGE20_FILELIST_* keys and
 * restored by the frame at creation (RestorePanelState); the
 * display toggle and the width were already persisted by P0
 * (OPT_MERGE20_FILELIST_VISIBLE and the bar's SaveState/
 * LoadState). Slim is a toggle button in the button row: file
 * name + review state only (design section 12.4), the search /
 * filter / batch rows folding away with it.
 */
#pragma once

#include "scbarcf.h"
#include "resource.h"
#include "Merge20/Merge20FileListCtrl.h"

class CDirDoc;
class DIFFITEM;

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
	/** @brief Folder document the panel lists (nullptr = none attached).
	 * The frame reads it to attach only once (see
	 * CMergeEditFrame::RefreshFileListPanel). */
	CDirDoc* GetDirDoc() const { return m_pDirDoc; }
	/** @brief Rebuild the list from the folder document. No-op while hidden. */
	void Refresh();
	void UpdateResources();
	/** @brief Relative path of the file the compare window shows
	 * (Panel P3, P-R7). The frame sets it before every Refresh();
	 * Refresh() re-selects that row, so the panel selection follows
	 * file moves in the compare window. Empty when the file belongs
	 * to no folder item (single-file compare, or a path switched
	 * outside the compare). */
	void SetCurrentFileRelPath(const String& relPath) { m_sCurrentRelPath = relPath; }
	/** @brief Take the pending open request (Panel P3, P-R7): the
	 * relative path stashed by OpenRowFile, cleared on take. The
	 * frame reads it when MSG_MERGE20_FILELIST_OPEN_FILE arrives.
	 * Empty when nothing is pending. */
	String TakePendingOpenRelPath()
	{
		String relPath = m_sPendingOpenRelPath;
		m_sPendingOpenRelPath.clear();
		return relPath;
	}
	/** @brief Handle a bare 1-5 / Space key while the panel list has
	 * the focus (Panel P3, P-R8). Called from the frame's
	 * PreTranslateMessage (the list itself cannot swallow keys).
	 * Returns true when the key was handled. */
	bool HandleListKey(UINT nVKey);
	/** @brief Set the slim display (Panel P4, design section
	 * 12.4): file name + review state only. Applies at once and
	 * saves the flag (OPT_MERGE20_FILELIST_SLIM). */
	void SetSlim(bool slim);
	/** @brief Save the panel state (Panel P4): the slim flag, the
	 * selected filter tab, the search string and the folded
	 * directories, to the OPT_MERGE20_FILELIST_* keys. Called by
	 * the frame from SavePosition (close, or the delayed
	 * pane-size save after hiding a bar). */
	void SavePanelState();
	/** @brief Restore the panel state (Panel P4): read the
	 * OPT_MERGE20_FILELIST_* keys and apply them (slim, tab,
	 * folded directories, search string). Called by the frame
	 * from OnCreate, after LoadState and before the first
	 * RefreshFileListPanel(), so the first list already shows
	 * the restored state. Applies without saving. */
	void RestorePanelState();

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
	afx_msg void OnSearchChanged();
	afx_msg void OnFilterTabChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMerge20BatchCheck();
	afx_msg void OnExpandAll();
	afx_msg void OnCollapseAll();
	afx_msg void OnSlimToggle();
	afx_msg LRESULT OnRowRClick(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnRowClick(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void LayoutChildren();
	/** @brief (Re-)insert the eight filter-tab labels (P2, P-R10):
	 * "all" plus the seven action categories, in ActionCategory
	 * enum order. Literal _() calls (see SetupColumns): the .pot
	 * generator only scans literal arguments. */
	void SetupFilterTabs();
	/** @brief Show the context menu for a displayed directory row
	 * (P2, P-R5): fold/unfold and check/uncheck everything below
	 * the folder. File rows get no menu. */
	void ShowRowContextMenu(int row);
	/** @brief Show the context menu for a displayed file row
	 * (P3, P-R8): the five review states (checked = current) and
	 * the output-flag toggle (checked = included). */
	void ShowFileRowContextMenu(int row);
	/** @brief Set the output flag of every file below one directory
	 * row, recursively (P2, P-R5): the S3 DIFFITEM tree walk, so
	 * files in collapsed subfolders are included too. */
	void SetOutputForDirectory(const DIFFITEM& dir, bool include);
	/** @brief Folder item for a displayed row (P3), or nullptr for
	 * a directory row or a row whose item is gone (rescan). The
	 * pointer is valid only for the synchronous call that uses it:
	 * a rescan rebuilds the items. */
	const DIFFITEM* FindRowItem(int row) const;
	/** @brief Apply a review state to one displayed file row (P3,
	 * P-R8): the S2 output-flag linkage, the folder-view sync, and
	 * a panel refresh. No-op for directory rows and unknown rows. */
	void ApplyRowReview(int row, ReviewState state);
	/** @brief Cycle the review state of one displayed file row
	 * (P3, P-R8): TODO -> DONE -> HOLD -> SPECIAL -> KEEP -> TODO,
	 * the same order as Space in the folder compare list (S2). */
	void CycleRowReview(int row);
	/** @brief Toggle the output flag of one displayed file row
	 * (P3, P-R8), with the folder-view sync and a panel refresh. */
	void ToggleRowOutput(int row);
	/** @brief Open the file of one displayed file row (P3, P-R7):
	 * stash its relative path and post MSG_MERGE20_FILELIST_OPEN_FILE
	 * to the frame. The frame resolves the item and moves to it;
	 * posting (not calling) keeps this bar alive: opening the file
	 * closes this frame. No-op for directory rows, for the file
	 * already shown, and without a frame window. */
	void OpenRowFile(int row);

	/** @brief Apply the slim display without saving (Panel P4):
	 * the button check, the list columns (ApplySlim) and the
	 * folded-away top rows (LayoutChildren). Used by SetSlim
	 * and by RestorePanelState. */
	void ApplySlimState(bool slim);

	HWND m_hwndFrame; //*< Frame window handle */
	CDirDoc* m_pDirDoc; /**< Folder document (owned by the folder compare, not by this bar) */
	CStatic m_stSearchLabel; /**< P2 search label: "Search file name or folder:" */
	CEdit m_wndSearch; /**< P2 search box (P-R4): filters the panel by file/folder name */
	CTabCtrl m_wndFilterTabs; /**< P2 filter tabs (P-R10): "all" plus the seven action categories */
	CButton m_btnBatch; /**< P2 "Batch check..." button (P-R3): opens the S3 dialog for the displayed rows */
	CButton m_btnExpandAll; /**< P2 "Expand all" button (P-R5) */
	CButton m_btnCollapseAll; /**< P2 "Collapse all" button (P-R5) */
	CButton m_btnSlim; /**< P4 slim toggle (BS_AUTOCHECKBOX): file name + review state only */
	bool m_bSlim = false; /**< P4: slim display is on */
	int m_nSelectedTab = 0; /**< P2: selected filter tab (0 = all categories) */
	CStatic m_stCount; /**< Count line (P1, P-R6): "N displayed, M checked, K unreviewed" */
	CMerge20FileListCtrl m_wndList; /**< Virtual file list */
	CStatic m_stEmpty; /**< Empty state: "No folder comparison" */
	String m_sCurrentRelPath; /**< P3: relative path of the file the compare window shows (panel selection follows it) */
	String m_sPendingOpenRelPath; /**< P3: relative path stashed by OpenRowFile for the frame's open message */
};
