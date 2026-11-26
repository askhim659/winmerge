// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FilterDeckBar.h
 *
 * @brief Declaration of CMerge20FilterDeckBar class
 *
 * Merge20 (H-FILTER, coverage matrix H-11/H-13/H-39): the filter
 * deck bar, docked at the top of the folder compare frame
 * (CDirFrame). It follows the CMerge20DedBar / CMerge20FileListBar
 * precedent (CSizingControlBarCF). The bar owns three checkbox
 * groups (existence 4, action category 7, review state 5) plus
 * the "only files needing action" preset button and the "show all"
 * button, in two fixed rows:
 *
 *   row 1: Existence: [Changed][Old only][New only][Identical]
 *          Action category: [Merge...][Review...]... [Only files needing action]
 *   row 2: Review state: [Not reviewed][Done][On hold][Needs manual work][Keep as is]
 *          [Show all]
 *
 * The bar owns no filter state: every checkbox reads and writes
 * the option keys (OPT_MERGE20_SHOW_STATUS_*,
 * OPT_MERGE20_SHOW_ACTION_*, OPT_MERGE20_SHOW_REVIEW_*), and a
 * change asks the folder view to rebuild (CDirView::
 * Merge20RefreshFilterDeck). The review group shares its keys
 * with the D11 context-menu submenu ("Show review states"), so
 * SyncCheckboxes() re-reads the keys whenever the view reports a
 * change from elsewhere (submenu toggle, session restore).
 *
 * The checkbox labels reuse existing msgids (the S0 status names,
 * the S4 action-category names, the D11 review-state menu names),
 * so no new strings are needed for them; only the bar caption,
 * the three group labels and the two buttons are new.
 *
 * Layout: two fixed rows, 52px high (DedBar precedent: the height
 * is fixed in LoadState / CalcFixedLayout, never derived from a
 * saved size or a drag). The row contents are measured from the
 * text at layout time; the two rows fit a window about 1024px
 * wide or wider. A narrower window may clip the right end of row
 * 1; the bar is hideable from the View menu (see MERGE20_NOTES.md
 * section 43).
 */
#pragma once

#include "scbarcf.h"
#include "resource.h"
#include "Merge20/Merge20FilterDeck.h"

class CMerge20FilterDeckBar : public CSizingControlBarCF
{
	DECLARE_DYNAMIC(CMerge20FilterDeckBar);
public:
	CMerge20FilterDeckBar();
	virtual ~CMerge20FilterDeckBar();
	virtual BOOL Create(
		CWnd* pParentWnd,			// mandatory
		LPCTSTR lpszWindowName = nullptr,
		DWORD dwStyle = WS_CHILD | WS_VISIBLE | CBRS_TOP,
		UINT nID = AFX_IDW_PANE_FIRST);

	/** @brief Re-read the option keys into the checkboxes. No-op
	 * while the bar window does not exist yet. Called from
	 * OnCreate and from CDirView::Merge20SyncFilterDeckBar (the
	 * review submenu and session restores change the same keys). */
	void SyncCheckboxes();
	void UpdateResources();

	// Merge20 (H-FILTER, DedBar lesson): clamp the persisted height
	// so a saved or dragged size must never steal client area.
	virtual void LoadState(LPCTSTR lpszProfileName) override;
	// Merge20 (H-FILTER, DedBar lesson): clamp the laid-out height
	// to the two fixed rows.
	virtual CSize CalcFixedLayout(BOOL bStretch, BOOL bHorz) override;

	//{{AFX_MSG(CMerge20FilterDeckBar)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDeckCheck(UINT nID);
	afx_msg void OnPreset();
	afx_msg void OnShowAll();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void LayoutChildren();
	/** @brief Deck state as the checkboxes currently show it
	 * (the clicked box has already toggled when BN_CLICKED fires). */
	Merge20FilterDeckState ReadDeckStateFromControls() const;
	/** @brief Deck state as the option keys currently store it. */
	Merge20FilterDeckState ReadDeckStateFromOptions() const;
	/** @brief Write a deck state back to the option keys. */
	void WriteDeckStateToOptions(const Merge20FilterDeckState& state) const;
	/** @brief Ask the folder view to rebuild from the option keys
	 * (CDirView::Merge20RefreshFilterDeck). No-op without a view. */
	void RefreshView();
	/** @brief Set the check of one button, when its window exists. */
	static void SetButtonCheck(CButton& button, bool checked);
	/** @brief Read the check of one button (false when its window
	 * does not exist yet). */
	static bool GetButtonCheck(const CButton& button);

	CStatic m_stStatusLabel; /**< "Existence:" group label */
	CStatic m_stActionLabel; /**< "Action category:" group label */
	CStatic m_stReviewLabel; /**< "Review state:" group label */
	CButton m_btnStatus[4]; /**< Existence checkboxes, Merge20FileStatus order */
	CButton m_btnAction[7]; /**< Action checkboxes, ActionCategory order */
	CButton m_btnReview[5]; /**< Review checkboxes, ReviewState order */
	CButton m_btnPreset; /**< "Only files needing action" (HTML acPreset) */
	CButton m_btnShowAll; /**< "Show all" (all three groups fully on) */
};
