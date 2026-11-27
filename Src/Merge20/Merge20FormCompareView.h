// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCompareView.h
 *
 * @brief FORM/XML comparison (Wave 5 F2): the FORM compare view
 * (design merge20-form-compare-design.md sections 3.2, 7.1 and
 * 10 F2).
 *
 * One CView that owns all F2 controls: the folder header, the
 * view-only notice, the seven-KPI dashboard, the five filter
 * tabs, the search box, the sort box, the re-run button, the
 * previous / next card buttons, the navigator toggle, the left
 * navigator (changed forms) and the card list (headers only in
 * F2: file name, form name, change tendency, one-line summary,
 * numeric change total, change count, severity, warning badge).
 * The canvas (F3) and the diff table (F5) replace the card list
 * rows with card windows later; the document and the list module
 * stay the same.
 *
 * The view never changes the document except through the
 * document's SetFilter / SetSearchText / SetSortOrder (which
 * refresh the visible list and notify the views). The list <->
 * navigator selection is kept in sync with a reentrancy guard.
 * All labels go through literal _() calls so the .pot generator
 * picks them up (FileListBar precedent).
 */
#pragma once

#include "Merge20/Merge20FormCompareDoc.h"
#include "Merge20/Merge20FormList.h"

class CFormCompareView : public CView
{
	DECLARE_DYNCREATE(CFormCompareView)
protected:
	CFormCompareView();
public:
	virtual ~CFormCompareView();
	CFormCompareDoc* GetDocument() const;

	/** @brief Re-apply all _() texts (language change). */
	void UpdateResources();

protected:
	virtual void OnInitialUpdate() override;
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;
	virtual void OnDraw(CDC* pDC) override;

	//{{AFX_MSG(CFormCompareView)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnFilterTabChanged(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSearchChanged();
	afx_msg void OnSortChanged();
	afx_msg void OnRerun();
	afx_msg void OnPrevForm();
	afx_msg void OnNextForm();
	afx_msg void OnNavToggle();
	afx_msg void OnNavSelChanged();
	afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void LayoutControls();
	void SetupColumns();
	void SetupTabs();
	void SetupSort();
	void RefreshAll();
	void RefreshHeader();
	void RefreshKpi();
	void RefreshList();
	void RefreshNavigator();
	/** @brief Select a visible position (list row + navigator row). */
	void SelectVisibleIndex(int visibleIndex);
	/** @brief Selected visible position, or -1 when nothing is selected. */
	int GetSelectedVisibleIndex() const;
	static String SeverityDisplayText(Merge20FormSeverity severity);

	CStatic m_stHeader; /**< Old/new folder paths. */
	CStatic m_stNotice; /**< View-only notice (design section 1.4). */
	CStatic m_stKpi[7]; /**< The seven KPI numbers. */
	CTabCtrl m_wndTabs; /**< The five filter tabs. */
	CStatic m_stSearchLabel; /**< Search label ("Search"). */
	CEdit m_wndSearch; /**< Search box. */
	CStatic m_stSortLabel; /**< Sort label ("Sort by"). */
	CComboBox m_wndSort; /**< The five sort orders. */
	CButton m_btnRerun; /**< Re-run analysis. */
	CButton m_btnPrev; /**< Previous form. */
	CButton m_btnNext; /**< Next form. */
	CButton m_btnNavToggle; /**< Show/hide the navigator. */
	CListBox m_wndNav; /**< Left navigator (changed forms). */
	CListCtrl m_wndList; /**< Card list (headers only in F2). */
	CStatic m_stEmpty; /**< Empty-state message. */
	bool m_bNavVisible = true; /**< Navigator shown. */
	bool m_bSyncingSelection = false; /**< List <-> navigator sync guard. */
};
