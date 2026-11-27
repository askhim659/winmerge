// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FileListCtrl.h
 *
 * @brief Declaration of CMerge20FileListCtrl class
 *
 * Merge20 (Panel P0/P1, design section 12.4/12.7): the file list
 * inside the persistent file-list panel (CMerge20FileListBar). A
 * CListCtrl in virtual-list mode (LVS_OWNERDATA, the same approach
 * as CDirView, confirmed in T-2-4), so 100-1000 files stay cheap to
 * draw. The rows are the DIFFITEMs of the folder compare that
 * opened this file compare, in compare order, read through
 * CDirDoc::GetDiffContext() (design section 12.5); the control
 * never touches CDiffContext internals.
 *
 * P1 columns (design section 12.3 P-R1): selection (output
 * checkbox glyph), file name, action category, risk, old/changed/
 * new counts (three columns, H-LIST), review state. Every display
 * value reuses the S6 wrappers (Merge20CsvGet*Text), so the panel
 * shows exactly what the folder compare list shows. The three
 * count columns reuse the folder compare list's new columns
 * (Merge20CsvGetMissingText / Merge20CsvGetModifiedText /
 * Merge20CsvGetAddedText): one locale-formatted number per column,
 * "Not calculated" for never-opened files, empty for directories.
 * They replace the P1 provisional single column
 * (Merge20CsvGetOldChangedNewText, removed by H-LIST).
 *
 * Rows are cached by value (display strings plus the review state
 * and risk level the drawing needs), never by DIFFITEM pointer:
 * a rescan rebuilds the DIFFITEMs, so a cached pointer could
 * dangle, while cached strings merely go stale until the next
 * Refresh() (frame activation or a state-change notification).
 * LVN_GETDISPINFO therefore only hands out pointers to strings
 * that live until the next Refresh().
 *
 * P3 adds the linkage (design section 12.7): a single click on a
 * row reports to the bar (which opens the file, toggles the output
 * flag, or cycles the review state, depending on the column), and
 * the bar re-selects the compare window's file after every Refresh()
 * through SelectRowByRelPath, so the panel selection follows file
 * moves in the compare window. Keyboard review actions (1-5/Space)
 * are handled in the frame's PreTranslateMessage, which calls the
 * bar: the list itself cannot swallow keys.
 *
 * Row colours follow the folder compare list (S2, design section
 * 12.3 P-R2): SPECIAL = light red, HOLD = light yellow, KEEP =
 * grey. The review-state text itself is the "needs manual work"
 * tag for SPECIAL and "Not reviewed" for unreviewed files. A High
 * risk is emphasized with red text in the Risk cell (P-R2); the
 * folder list has no such emphasis (verified: no bold or colour
 * handling for RiskLevel::HIGH in DirView.cpp), so this is a
 * deliberate panel-only addition, recorded in MERGE20_NOTES.md
 * section 39.
 */
#pragma once

#include <set>
#include <vector>
#include "UnicodeString.h"
#include "Merge20Types.h"

class CDiffContext;

/** @brief Message the control sends its parent (CMerge20FileListBar)
 * when a row is right-clicked (NM_RCLICK). wParam = displayed row
 * index; the bar reads the cursor position itself. WM_USER + 120
 * is free in this window: no WM_USER message is used anywhere in
 * the Merge20 panel code (verified on P2). */
#define WM_MERGE20_FILELIST_ROW_RCLICK (WM_USER + 120)
/** @brief Message the control sends its parent when a row is
 * single-clicked (NM_CLICK, Panel P3, P-R7/P-R8). wParam = displayed
 * row index, lParam = subitem index (the column that was clicked):
 * the bar opens the file, toggles the output flag, or cycles the
 * review state, depending on the column. The bar only posts the
 * open request to the frame (async), so this synchronous send never
 * outlives the control: opening a file closes this frame, but only
 * after the posted message is pumped.
 * WM_USER + 121 is free (see WM_MERGE20_FILELIST_ROW_RCLICK). */
#define WM_MERGE20_FILELIST_ROW_CLICK (WM_USER + 121)

class CMerge20FileListCtrl : public CListCtrl
{
	DECLARE_DYNAMIC(CMerge20FileListCtrl);
public:
	CMerge20FileListCtrl();
	virtual ~CMerge20FileListCtrl();

	/** @brief One cached row: display strings plus the enums the
	 * custom draw and the count line need. Cached by value, never
	 * by DIFFITEM pointer (see the file header comment). Public so
	 * the bar can read the displayed rows for the batch check and
	 * the directory operations (P2); the bar never writes them.
	 * Declared first: member declarations below use Row in their
	 * signatures, and a nested type is only visible after its
	 * definition. */
	struct Row
	{
		String output; /**< Output checkbox glyph (empty for directories) */
		String fileName;
		String action;
		String risk;
		String missing; /**< Missing-in-new (deleted) lines (H-LIST) */
	String modified; /**< Changed lines (H-LIST) */
	String added; /**< Newly added lines (H-LIST) */
		String review;
		ReviewState reviewState = ReviewState::TODO; /**< Row tint (S2 rule) */
		RiskLevel riskLevel = RiskLevel::NONE; /**< High-risk emphasis */
		bool isDirectory = false; /**< Skipped by the count line (S3 rule) */
		bool includeInOutput = false; /**< Counted as checked (S3 rule) */
		String relPath; /**< P2: DIFFITEM::getItemRelativePath(), key for collapse and for resolving DIFFITEMs */
		int depth = 0; /**< P2: DIFFITEM::GetDepth(), files directly under the root are 0 */
		ActionCategory actionCategory = ActionCategory::NO_ACTION_IDENTICAL; /**< P2: filter-tab value (meaningful only when classified) */
		bool classified = false; /**< P2: S4 classification ran for this file (unclassified files match no tab) */
		bool hasChildren = false; /**< P2: DIFFITEM::HasChildren(), whether the row can be folded */
	};

	/** @brief Folder context to list (nullptr = no folder compare). */
	void SetDiffContext(const CDiffContext* pCtxt) { m_pCtxt = pCtxt; }
	/** @brief Rebuild the cached rows from the context. */
	void Refresh();
	/** @brief Insert the eight P1 columns (call once, after Create). */
	void SetupColumns();
	/** @brief Re-apply the column header texts (language change). */
	void UpdateColumnHeaders();
	/** @brief Count line for the panel header, same definition as
	 * CDirView::GetMerge20StatusCountText (S3): displayed files,
	 * checked files, unreviewed files. Directory rows are skipped. */
	String GetCountText() const;
	/** @brief Count displayed files per review state (P2, for the
	 * batch-check dialog): the same definition as
	 * CDirView::Merge20CountDisplayedByReviewState, but over the
	 * panel's displayed rows (search / tab / collapse applied).
	 * Directory rows are skipped. */
	void CountByReviewState(int counts[5]) const;
	/** @brief Search text (P2, P-R4): files whose relative path
	 * does not contain it are hidden. Empty text shows all rows. */
	void SetSearchText(const String& searchText);
	/** @brief Action-category filter (P2, P-R10): -1 shows every
	 * category, otherwise static_cast<int>(ActionCategory) of the
	 * selected tab. Unclassified files match no tab, only -1. */
	void SetActionFilter(int actionFilter);
	/** @brief Risk filter (B3, gap analysis section 4.2 B3 (c)):
	 * -1 shows every risk level, otherwise
	 * static_cast<int>(RiskLevel) of the selected combo item.
	 * Unclassified files match no level, only -1. Panel-only: the
	 * folder compare list is not filtered. */
	void SetRiskFilter(int riskFilter);
	/** @brief Fold or unfold one directory row (P2, P-R5), keyed
	 * by its relative path. The bar drives this from the buttons,
	 * the double-click and the context menu. */
	void SetCollapsed(const String& relPath, bool collapsed);
	/** @brief True when the displayed row is a folded directory. */
	bool IsRowCollapsed(int index) const;
	/** @brief Unfold every directory row (P2, "expand all"). */
	void ExpandAll();
	/** @brief Fold every directory row that has children (P2,
	 * "collapse all"). */
	void CollapseAll();
	/** @brief Folded directories, as relative paths (Panel P4):
	 * the set behind m_collapsed, for saving. */
	std::set<String> GetCollapsedPaths() const { return m_collapsed; }
	/** @brief Replace the folded directories (Panel P4, restore):
	 * stale paths are kept (they only match rows that exist).
	 * Does not save: the bar saves the restored set with the
	 * rest of the panel state. */
	void SetCollapsedPaths(const std::set<String>& paths);
	/** @brief Slim display (Panel P4, design section 12.4): hide
	 * every column except file name and review state (the
	 * "file name + review only" view). The hidden columns'
	 * widths are remembered and restored on un-slim, so a
	 * manually widened column survives a slim round trip. */
	void ApplySlim(bool slim);
	/** @brief Number of displayed rows (after search / tab /
	 * collapse). */
	int GetDisplayedRowCount() const { return static_cast<int>(m_rows.size()); }
	/** @brief Displayed row by index (nullptr when out of range).
	 * The pointer stays valid until the next Refresh(). */
	const Row* GetDisplayedRow(int index) const;
	/** @brief Select the displayed row with this relative path and
	 * scroll it into view (Panel P3, P-R7: the panel selection
	 * follows the file the compare window shows). No-op when the
	 * path is empty; when the row is not displayed (hidden by the
	 * search, the filter tab or a collapsed folder, or gone after a
	 * rescan), the selection is cleared instead of leaving a stale
	 * row selected. */
	void SelectRowByRelPath(const String& relPath);
	/** @brief Selected displayed row, or -1 when none is selected
	 * (Panel P3, P-R8: the keyboard actions apply to this row). */
	int GetSelectedRow() const;
	/** @brief Panel columns, in display order (design section 12.3
	 * P-R1). Public (Panel P3): the bar dispatches single clicks by
	 * column (output column toggles the flag, review column cycles
	 * the state, other columns open the file). */
	enum Column
	{
		COL_OUTPUT = 0, /**< Selection: output checkbox glyph */
		COL_FILENAME, /**< File name */
		COL_ACTION, /**< Action category */
		COL_RISK, /**< Risk level */
		COL_MISSING, /**< Missing-in-new (deleted) lines (H-LIST) */
	COL_MODIFIED, /**< Changed lines (H-LIST) */
	COL_ADDED, /**< Newly added lines (H-LIST) */
		COL_REVIEW, /**< Review state */
		COL_COUNT /**< Number of columns */
	};

	//{{AFX_MSG(CMerge20FileListCtrl)
	afx_msg void OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclk(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnClick(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	/** @brief Persist the folded directories at once (Panel P4):
	 * every collapse change (button, menu, double-click) saves
	 * immediately, so the state survives even a crash (the D11
	 * dual-save precedent: the dock-hidden key is saved both on
	 * toggle and at SavePosition). */
	void SaveCollapsedState();

	const CDiffContext* m_pCtxt; /**< Folder context (owned by CDirDoc, not by this control) */
	std::vector<Row> m_allRows; /**< P2: all rows, in compare order (before search / tab / collapse) */
	std::vector<Row> m_rows; /**< Cached displayed rows, in compare order */
	std::set<String> m_collapsed; /**< P2: relative paths of folded directories (default: all unfolded) */
	String m_searchText; /**< P2: raw search text (trimmed and lower-cased by the filter module) */
	int m_actionFilter = -1; /**< P2: -1 = all action categories, otherwise static_cast<int>(ActionCategory) */
	int m_riskFilter = -1; /**< B3: -1 = all risk levels, otherwise static_cast<int>(RiskLevel) */
	bool m_bSlim = false; /**< P4: slim display (file name + review state only) */
	int m_savedColWidths[COL_COUNT] = {}; /**< P4: column widths remembered while slim */
};
