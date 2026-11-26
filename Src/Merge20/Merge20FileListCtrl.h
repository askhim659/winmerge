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
 * new counts (provisional single column, see below), review state.
 * Every display value reuses the S6 wrappers (Merge20CsvGet*Text),
 * so the panel shows exactly what the folder compare list shows.
 * The old/changed/new column is provisional: H-LIST (coverage
 * matrix section 4) will add the three separate columns; until
 * then this one column shows the counts part of the summary text
 * ("Missing: N lines / Changed: M lines / Added: K lines",
 * Merge20CsvGetOldChangedNewText).
 *
 * Rows are cached by value (display strings plus the review state
 * and risk level the drawing needs), never by DIFFITEM pointer:
 * a rescan rebuilds the DIFFITEMs, so a cached pointer could
 * dangle, while cached strings merely go stale until the next
 * Refresh() (frame activation or a state-change notification).
 * LVN_GETDISPINFO therefore only hands out pointers to strings
 * that live until the next Refresh().
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

#include <vector>
#include "UnicodeString.h"
#include "Merge20Types.h"

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
	/** @brief Insert the six P1 columns (call once, after Create). */
	void SetupColumns();
	/** @brief Re-apply the column header texts (language change). */
	void UpdateColumnHeaders();
	/** @brief Count line for the panel header, same definition as
	 * CDirView::GetMerge20StatusCountText (S3): displayed files,
	 * checked files, unreviewed files. Directory rows are skipped. */
	String GetCountText() const;

	//{{AFX_MSG(CMerge20FileListCtrl)
	afx_msg void OnGetdispinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnCustomDraw(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	/** @brief Panel columns, in display order (design section 12.3 P-R1). */
	enum Column
	{
		COL_OUTPUT = 0, /**< Selection: output checkbox glyph */
		COL_FILENAME, /**< File name */
		COL_ACTION, /**< Action category */
		COL_RISK, /**< Risk level */
		COL_OLD_CHANGED_NEW, /**< Old/changed/new counts (provisional, one column) */
		COL_REVIEW, /**< Review state */
		COL_COUNT /**< Number of columns */
	};

	/** @brief One cached row: display strings plus the enums the
	 * custom draw and the count line need. Cached by value, never
	 * by DIFFITEM pointer (see the file header comment). */
	struct Row
	{
		String output; /**< Output checkbox glyph (empty for directories) */
		String fileName;
		String action;
		String risk;
		String oldChangedNew; /**< Provisional counts column */
		String review;
		ReviewState reviewState = ReviewState::TODO; /**< Row tint (S2 rule) */
		RiskLevel riskLevel = RiskLevel::NONE; /**< High-risk emphasis */
		bool isDirectory = false; /**< Skipped by the count line (S3 rule) */
		bool includeInOutput = false; /**< Counted as checked (S3 rule) */
	};

	const CDiffContext* m_pCtxt; /**< Folder context (owned by CDirDoc, not by this control) */
	std::vector<Row> m_rows; /**< Cached rows, in compare order */
};
