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
 * F3 adds the visual-preview pane under the card list: the
 * selected file's canvas (Merge20FormCanvas) with the zoom
 * controls (100% / 25% / fit / slider 10-200%) and the viewport
 * height toggle (640 px fixed / full). F4 adds the spotlight
 * inspector banner under the zoom row (old/new side-by-side
 * previews, previous / next stepping, clear, go-to-changed-area)
 * and the public FocusDiffRecord hook the F5 diff table calls
 * when a row is clicked. F5 adds the structured diff table
 * under the preview pane (design sections 2.9, 5.2, 5.3): the
 * four HTML columns, group rows collapsed by default with
 * expand on click, and row click -> spotlight for records with
 * a visual. F6 adds the name-master edit button to the top bar
 * (design sections 7.1 and 8): it opens the edit dialog, and
 * after a save the document re-resolves every form name. F7 adds
 * the CSV export button to the top bar (design sections 7.1 and 9):
 * it opens the range-selection dialog and writes the two-part CSV
 * (BOM-prefixed UTF-8) for the filtered view or all changed files.
 * The document and the list module stay the same.
 *
 * The view never changes the document except through the
 * document's SetFilter / SetSearchText / SetSortOrder (which
 * refresh the visible list and notify the views). The list <->
 * navigator selection is kept in sync with a reentrancy guard.
 * All labels go through literal _() calls so the .pot generator
 * picks them up (FileListBar precedent).
 */
#pragma once

#include <map>
#include <set>
#include "Merge20/Merge20FormCanvas.h"
#include "Merge20/Merge20FormCompareDoc.h"
#include "Merge20/Merge20FormList.h"
#include "Merge20/Merge20FormSpotlight.h"
#include "Merge20/Merge20FormTable.h"
#include "Merge20/Merge20FormSpotPreview.h"

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

	/**
	 * @brief Focus a diff record for the spotlight (F4).
	 * Public hook for F5: the structured diff table calls this
	 * when a row is clicked (design section 2.7). Only a record
	 * with `hasVisual` is focused; other indices are ignored.
	 * The selection is kept per file card, so switching files
	 * and coming back restores it.
	 */
	void FocusDiffRecord(int recordIndex);

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
	/** @brief Name-master edit button (F6): open the edit dialog. */
	afx_msg void OnMasterEdit();
	/** @brief CSV export button (F7): open the range dialog and write the CSV. */
	afx_msg void OnCsvExport();
	/** @brief Zoom 100% button (F3). */
	afx_msg void OnZoom100();
	/** @brief Zoom 25% button (F3). */
	afx_msg void OnZoom25();
	/** @brief Zoom fit button (F3). */
	afx_msg void OnZoomFit();
	/** @brief Viewport height toggle button (F3). */
	afx_msg void OnViewportToggle();
	/** @brief Canvas shape click (F3 selection; F4 spotlight reads the key). */
	afx_msg void OnCanvasShapeClicked();
	/** @brief Spotlight: previous change (F4). */
	afx_msg void OnSpotPrev();
	/** @brief Spotlight: next change (F4). */
	afx_msg void OnSpotNext();
	/** @brief Spotlight: clear (F4). */
	afx_msg void OnSpotClear();
	/** @brief Spotlight: go to the changed area (F4). */
	afx_msg void OnSpotGoto();
	/** @brief Zoom slider (F3). */
	afx_msg void OnHScroll(UINT nCode, UINT nPos, CScrollBar* pScrollBar);
	/** @brief Diff table row clicked (F5). */
	afx_msg void OnTableItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
	/** @brief Diff table clicked: group rows expand/collapse (F5). */
	afx_msg void OnTableClick(NMHDR* pNMHDR, LRESULT* pResult);
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
	/** @brief Show the selected file in the preview canvas (F3). */
	void RefreshCanvas();
	/** @brief Apply the selected file's spotlight state to the canvas and the banner (F4). */
	void ApplySpotlight();
	/** @brief Hide the spotlight banner and clear the canvas spotlight (F4). */
	void HideSpotlightBanner();
	/** @brief Rebuild the banner text and the old/new previews from the current spotlight state (F4). */
	void UpdateSpotlightBanner();
	/** @brief Step the spotlight to the previous (-1) or next (+1) visual record (F4). */
	void StepSpotlight(int dir);
	/** @brief Lay out the spotlight banner controls (F4). */
	void LayoutSpotlightBanner(int left, int top, int width);
	/** @brief First visual record whose element is this shape key, or -1 (F4). */
	int FindRecordIndexForShapeKey(const std::wstring& key) const;
	/** @brief Sync the slider position and the zoom % label with the canvas scale (F3). */
	void UpdateZoomControls();
	/** @brief Selected file result, or nullptr when nothing is selected (F3). */
	const Merge20FormFileResult* GetSelectedResult() const;
	/** @brief Index of the selected file result, or -1 (F5). */
	int GetSelectedResultIndex() const;
	/** @brief Set up the four diff-table columns (F5). */
	void SetupTableColumns();
	/** @brief Rebuild the diff table of the selected file (F5). */
	void RefreshTable();
	/** @brief Update the name-master button text (F6; count badge). */
	void RefreshMasterButton();
	/** @brief Update the diff-table label (F5; ignored-noise count). */
	void RefreshTableLabel();
	/** @brief Make the row of this record visible and selected (F5). */
	void EnsureTableRowVisible(int recordIndex);
	/** @brief Table row index of this record, or -1 (F5). */
	int FindTableRowForRecord(int recordIndex) const;
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
	CButton m_btnMaster; /**< Name-master edit button (F6). */
	CButton m_btnCsv; /**< CSV export button (F7). */
	CButton m_btnPrev; /**< Previous form. */
	CButton m_btnNext; /**< Next form. */
	CButton m_btnNavToggle; /**< Show/hide the navigator. */
	CListBox m_wndNav; /**< Left navigator (changed forms). */
	CListCtrl m_wndList; /**< Card list (headers only in F2). */
	CStatic m_stEmpty; /**< Empty-state message. */
	CStatic m_stPreviewLabel; /**< Preview label ("Visual preview - file"). */
	CButton m_btnZoom100; /**< Zoom 100% button (F3). */
	CButton m_btnZoom25; /**< Zoom 25% button (F3). */
	CButton m_btnZoomFit; /**< Zoom fit button (F3). */
	CSliderCtrl m_wndZoomSlider; /**< Zoom slider 10-200% (F3). */
	CStatic m_stZoomLabel; /**< Zoom % label (F3). */
	CButton m_btnViewportToggle; /**< Viewport height toggle (F3). */
	CMerge20FormCanvas m_wndCanvas; /**< Visual-preview canvas (F3). */
	CStatic m_stSpotPanel; /**< Spotlight banner background (F4). */
	CStatic m_stSpotOldLabel; /**< Old preview label (F4). */
	CMerge20FormSpotPreview m_wndSpotOld; /**< Old-shape preview (F4). */
	CStatic m_stSpotNewLabel; /**< New preview label (F4). */
	CMerge20FormSpotPreview m_wndSpotNew; /**< New-shape preview (F4). */
	CStatic m_stSpotText; /**< Spotlight banner text (F4). */
	CButton m_btnSpotPrev; /**< Previous change (F4). */
	CButton m_btnSpotNext; /**< Next change (F4). */
	CButton m_btnSpotClear; /**< Clear spotlight (F4). */
	CButton m_btnSpotGoto; /**< Go to changed area (F4). */
	bool m_bSpotlightVisible = false; /**< Spotlight banner shown (F4). */
	CStatic m_stTableLabel; /**< Diff-table label (F5). */
	CListCtrl m_wndTable; /**< Structured diff table (F5). */
	/** @brief Current diff-table rows (F5; list index -> row). */
	std::vector<Merge20FormTableRow> m_tableRows;
	/** @brief Expanded diff-table groups per file card (F5). */
	std::map<std::wstring, std::set<std::wstring>> m_expandedTableGroups;
	bool m_bSyncingTable = false; /**< Diff-table rebuild/selection guard (F5). */
	/** @brief Spotlight state per file card, keyed by file name (design section 2.7, F4). */
	std::map<std::wstring, Merge20FormSpotlightState> m_spotlightByFile;
	bool m_bNavVisible = true; /**< Navigator shown. */
	bool m_bCanvasFullHeight = false; /**< Canvas takes the full bottom area (F3). */
	bool m_bSyncingSelection = false; /**< List <-> navigator sync guard. */
};

