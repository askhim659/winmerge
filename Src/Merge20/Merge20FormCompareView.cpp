// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCompareView.cpp
 *
 * @brief Implementation of the FORM compare view (Wave 5 F2).
 * Child controls follow the Merge20FileListBar precedent
 * (DEFAULT_GUI_FONT, dummy rect at creation, layout in OnSize,
 * literal _() calls for the .pot generator).
 */
#include "StdAfx.h"
#include "Merge20/Merge20FormCompareView.h"
#include "resource.h"
#include <cmath>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{

	// Layout constants (pixels).
	const int kFormMargin = 6;
	const int kFormRowGap = 4;
	const int kFormHeaderHeight = 18;
	const int kFormKpiHeight = 18;
	const int kFormControlHeight = 26;
	const int kFormNavWidth = 250;

	// Column widths of the card list (headers only in F2).
	const int kFormColFileName = 180;
	const int kFormColFormName = 160;
	const int kFormColTendency = 200;
	const int kFormColSummary = 320;
	const int kFormColNumeric = 110;
	const int kFormColCount = 100;
	const int kFormColSeverity = 110;
	const int kFormColWarning = 90;

	// Rounded integer display of a numeric total (HTML rounds the
	// per-file total before display).
	String FormRoundedTotal(double totalNumericSum)
	{
		return std::to_wstring(static_cast<int>(std::round(totalNumericSum)));
	}

} // namespace

IMPLEMENT_DYNCREATE(CFormCompareView, CView)

BEGIN_MESSAGE_MAP(CFormCompareView, CView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_NOTIFY(TCN_SELCHANGE, IDC_MERGE20_FORM_TABS, OnFilterTabChanged)
	ON_EN_CHANGE(IDC_MERGE20_FORM_SEARCH, OnSearchChanged)
	ON_CBN_SELCHANGE(IDC_MERGE20_FORM_SORT, OnSortChanged)
	ON_BN_CLICKED(IDC_MERGE20_FORM_RERUN, OnRerun)
	ON_BN_CLICKED(IDC_MERGE20_FORM_PREV, OnPrevForm)
	ON_BN_CLICKED(IDC_MERGE20_FORM_NEXT, OnNextForm)
	ON_BN_CLICKED(IDC_MERGE20_FORM_NAV_TOGGLE, OnNavToggle)
	ON_LBN_SELCHANGE(IDC_MERGE20_FORM_NAV, OnNavSelChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_MERGE20_FORM_LIST, OnListItemChanged)
END_MESSAGE_MAP()

CFormCompareView::CFormCompareView()
{
}

CFormCompareView::~CFormCompareView()
{
}

CFormCompareDoc* CFormCompareView::GetDocument() const
{
	return reinterpret_cast<CFormCompareDoc*>(m_pDocument);
}

int CFormCompareView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	CFont* pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	CRect rc(0, 0, 0, 0);
	if (!m_stHeader.Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE | SS_ENDELLIPSIS,
		rc, this, IDC_MERGE20_FORM_HEADER))
	{
		TRACE0("Failed to create Merge20FormCompareView header\n");
		return -1;
	}
	m_stHeader.SetFont(pFont);
	if (!m_stNotice.Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
		rc, this, IDC_MERGE20_FORM_NOTICE))
	{
		TRACE0("Failed to create Merge20FormCompareView notice\n");
		return -1;
	}
	m_stNotice.SetFont(pFont);
	for (int i = 0; i < 7; ++i)
	{
		if (!m_stKpi[i].Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
			rc, this, IDC_MERGE20_FORM_KPI_TOTAL + i))
		{
			TRACE0("Failed to create Merge20FormCompareView KPI label\n");
			return -1;
		}
		m_stKpi[i].SetFont(pFont);
	}
	if (!m_wndTabs.Create(WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
		rc, this, IDC_MERGE20_FORM_TABS))
	{
		TRACE0("Failed to create Merge20FormCompareView filter tabs\n");
		return -1;
	}
	m_wndTabs.SetFont(pFont);
	if (!m_stSearchLabel.Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
		rc, this, IDC_MERGE20_FORM_SEARCH_LABEL))
	{
		TRACE0("Failed to create Merge20FormCompareView search label\n");
		return -1;
	}
	m_stSearchLabel.SetFont(pFont);
	if (!m_wndSearch.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
		rc, this, IDC_MERGE20_FORM_SEARCH))
	{
		TRACE0("Failed to create Merge20FormCompareView search box\n");
		return -1;
	}
	m_wndSearch.SetFont(pFont);
	if (!m_stSortLabel.Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
		rc, this, IDC_MERGE20_FORM_SORT_LABEL))
	{
		TRACE0("Failed to create Merge20FormCompareView sort label\n");
		return -1;
	}
	m_stSortLabel.SetFont(pFont);
	if (!m_wndSort.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
		rc, this, IDC_MERGE20_FORM_SORT))
	{
		TRACE0("Failed to create Merge20FormCompareView sort box\n");
		return -1;
	}
	m_wndSort.SetFont(pFont);
	if (!m_btnRerun.Create(_T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_RERUN))
	{
		TRACE0("Failed to create Merge20FormCompareView re-run button\n");
		return -1;
	}
	m_btnRerun.SetFont(pFont);
	if (!m_btnPrev.Create(_T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_PREV))
	{
		TRACE0("Failed to create Merge20FormCompareView previous button\n");
		return -1;
	}
	m_btnPrev.SetFont(pFont);
	if (!m_btnNext.Create(_T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_NEXT))
	{
		TRACE0("Failed to create Merge20FormCompareView next button\n");
		return -1;
	}
	m_btnNext.SetFont(pFont);
	if (!m_btnNavToggle.Create(_T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_NAV_TOGGLE))
	{
		TRACE0("Failed to create Merge20FormCompareView navigator toggle\n");
		return -1;
	}
	m_btnNavToggle.SetFont(pFont);
	if (!m_wndNav.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY,
		rc, this, IDC_MERGE20_FORM_NAV))
	{
		TRACE0("Failed to create Merge20FormCompareView navigator\n");
		return -1;
	}
	m_wndNav.SetFont(pFont);
	if (!m_wndList.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
		rc, this, IDC_MERGE20_FORM_LIST))
	{
		TRACE0("Failed to create Merge20FormCompareView card list\n");
		return -1;
	}
	m_wndList.SetFont(pFont);
	m_wndList.SetExtendedStyle(m_wndList.GetExtendedStyle() | LVS_EX_FULLROWSELECT);
	if (!m_stEmpty.Create(_T(""), WS_CHILD | SS_CENTER | SS_CENTERIMAGE,
		rc, this, IDC_MERGE20_FORM_EMPTY))
	{
		TRACE0("Failed to create Merge20FormCompareView empty message\n");
		return -1;
	}
	m_stEmpty.SetFont(pFont);
	return 0;
}

void CFormCompareView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
	SetupColumns();
	SetupTabs();
	SetupSort();
	UpdateResources();
	RefreshAll();
}

void CFormCompareView::OnUpdate(CView* /*pSender*/, LPARAM /*lHint*/, CObject* /*pHint*/)
{
	RefreshAll();
}

void CFormCompareView::OnDraw(CDC* /*pDC*/)
{
	// The child controls paint themselves.
}

void CFormCompareView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);
	LayoutControls();
}

void CFormCompareView::LayoutControls()
{
	if (m_wndList.GetSafeHwnd() == nullptr)
		return;
	CRect rcClient;
	GetClientRect(&rcClient);
	const int width = rcClient.Width();
	const int height = rcClient.Height();
	int y = kFormMargin;
	// Header: old/new folder paths.
	m_stHeader.MoveWindow(kFormMargin, y, width - 2 * kFormMargin, kFormHeaderHeight);
	y += kFormHeaderHeight + kFormRowGap;
	// View-only notice.
	m_stNotice.MoveWindow(kFormMargin, y, width - 2 * kFormMargin, kFormHeaderHeight);
	y += kFormHeaderHeight + kFormRowGap;
	// KPI dashboard: seven labels in one row.
	const int kpiWidth = (width - 2 * kFormMargin) / 7;
	for (int i = 0; i < 7; ++i)
		m_stKpi[i].MoveWindow(kFormMargin + i * kpiWidth, y, kpiWidth, kFormKpiHeight);
	y += kFormKpiHeight + kFormRowGap;
	// Filter tabs (left) + previous / next / navigator toggle (right).
	const int tabWidth = (std::min)(520, width / 2);
	m_wndTabs.MoveWindow(kFormMargin, y, tabWidth, kFormControlHeight);
	const int btnWidth = 90;
	const int toggleWidth = 140;
	int xRight = width - kFormMargin;
	m_btnNavToggle.MoveWindow(xRight - toggleWidth, y, toggleWidth, kFormControlHeight);
	xRight -= toggleWidth + kFormRowGap;
	m_btnNext.MoveWindow(xRight - btnWidth, y, btnWidth, kFormControlHeight);
	xRight -= btnWidth + kFormRowGap;
	m_btnPrev.MoveWindow(xRight - btnWidth, y, btnWidth, kFormControlHeight);
	y += kFormControlHeight + kFormRowGap;
	// Search + sort + re-run row.
	int x = kFormMargin;
	m_stSearchLabel.MoveWindow(x, y, 50, kFormControlHeight);
	x += 50 + kFormRowGap;
	m_wndSearch.MoveWindow(x, y, 220, kFormControlHeight);
	x += 220 + kFormRowGap;
	m_stSortLabel.MoveWindow(x, y, 80, kFormControlHeight);
	x += 80 + kFormRowGap;
	m_wndSort.MoveWindow(x, y, 220, kFormControlHeight);
	x += 220 + kFormRowGap;
	m_btnRerun.MoveWindow(x, y, 90, kFormControlHeight);
	y += kFormControlHeight + kFormMargin;
	// Bottom area: navigator (left) + card list (rest).
	const int navWidth = m_bNavVisible ? kFormNavWidth : 0;
	const int listTop = y;
	const int listHeight = (std::max)(0, height - listTop - kFormMargin);
	if (m_bNavVisible)
		m_wndNav.MoveWindow(kFormMargin, listTop, navWidth, listHeight);
	else
		m_wndNav.MoveWindow(kFormMargin, listTop, 0, listHeight);
	const int listLeft = kFormMargin + navWidth + (m_bNavVisible ? kFormRowGap : 0);
	const int listWidth = (std::max)(0, width - listLeft - kFormMargin);
	m_wndList.MoveWindow(listLeft, listTop, listWidth, listHeight);
	m_stEmpty.MoveWindow(listLeft, listTop, listWidth, listHeight);
}

void CFormCompareView::SetupColumns()
{
	// Literal _() calls (FileListBar precedent): the .pot
	// generator only scans literal arguments. "File name" reuses
	// the existing msgid (no [Wiz]); the rest are new F2 msgids.
	m_wndList.InsertColumn(0, _("File name").c_str(), LVCFMT_LEFT, kFormColFileName);
	m_wndList.InsertColumn(1, _("Form name").c_str(), LVCFMT_LEFT, kFormColFormName);
	m_wndList.InsertColumn(2, _("Change tendency").c_str(), LVCFMT_LEFT, kFormColTendency);
	m_wndList.InsertColumn(3, _("One-line summary").c_str(), LVCFMT_LEFT, kFormColSummary);
	m_wndList.InsertColumn(4, _("Numeric change total").c_str(), LVCFMT_RIGHT, kFormColNumeric);
	m_wndList.InsertColumn(5, _("Change count").c_str(), LVCFMT_RIGHT, kFormColCount);
	m_wndList.InsertColumn(6, _("Severity").c_str(), LVCFMT_LEFT, kFormColSeverity);
	m_wndList.InsertColumn(7, _("Warning").c_str(), LVCFMT_LEFT, kFormColWarning);
}

void CFormCompareView::SetupTabs()
{
	// Literal _() calls; the tab order matches Merge20FormFilter.
	m_wndTabs.DeleteAllItems();
	m_wndTabs.InsertItem(0, _("Show all forms").c_str());
	m_wndTabs.InsertItem(1, _("Large changes").c_str());
	m_wndTabs.InsertItem(2, _("Coordinate/size changes").c_str());
	m_wndTabs.InsertItem(3, _("Element/file changes").c_str());
	m_wndTabs.InsertItem(4, _("Layout-breaking only").c_str());
}

void CFormCompareView::SetupSort()
{
	// Literal _() calls; the item order matches Merge20FormSort.
	m_wndSort.ResetContent();
	m_wndSort.AddString(_("File name order").c_str());
	m_wndSort.AddString(_("Numeric change (ascending)").c_str());
	m_wndSort.AddString(_("Numeric change (descending)").c_str());
	m_wndSort.AddString(_("Change count (descending)").c_str());
	m_wndSort.AddString(_("Severity order").c_str());
}

void CFormCompareView::UpdateResources()
{
	// Re-apply every _() text (language change). The search box
	// keeps its own text; the sort selection is restored below.
	m_stSearchLabel.SetWindowText(_("Search").c_str());
	m_stSortLabel.SetWindowText(_("Sort by").c_str());
	m_btnRerun.SetWindowText(_("Re-run analysis").c_str());
	m_btnPrev.SetWindowText(_("Previous form").c_str());
	m_btnNext.SetWindowText(_("Next form").c_str());
	m_btnNavToggle.SetWindowText((m_bNavVisible ? _("Hide navigator") : _("Show navigator")).c_str());
	SetupTabs();
	SetupSort();
	const CFormCompareDoc* pDoc = GetDocument();
	if (pDoc != nullptr)
	{
		m_wndTabs.SetCurSel(static_cast<int>(pDoc->GetFilter()));
		m_wndSort.SetCurSel(static_cast<int>(pDoc->GetSortOrder()));
	}
	// Column headers (delete + re-insert keeps the order).
	if (m_wndList.GetSafeHwnd() != nullptr)
	{
		while (m_wndList.DeleteColumn(0))
			;
		SetupColumns();
	}
	RefreshAll();
}

void CFormCompareView::RefreshAll()
{
	RefreshHeader();
	RefreshKpi();
	RefreshList();
	RefreshNavigator();
}

void CFormCompareView::RefreshHeader()
{
	CFormCompareDoc* pDoc = GetDocument();
	if (pDoc == nullptr)
		return;
	String header;
	if (pDoc->HasFolders())
	{
		header = _("Old folder:") + _T(" ") + pDoc->GetLeftFolder() +
			_T("    ") + _("New folder:") + _T(" ") + pDoc->GetRightFolder();
	}
	m_stHeader.SetWindowText(header.c_str());
	m_stNotice.SetWindowText(_("View only: this window shows differences and never merges.").c_str());
}

void CFormCompareView::RefreshKpi()
{
	CFormCompareDoc* pDoc = GetDocument();
	const Merge20FormKpi kpi = (pDoc != nullptr) ? pDoc->GetKpi() : Merge20FormKpi();
	// Literal _() calls; the label order matches m_stKpi.
	const String labels[7] =
	{
		_("Total files"),
		_("Changed files"),
		_("Identical files (hidden)"),
		_("Average numeric change"),
		_("Large changes"),
		_("Files with coordinate changes"),
		_("Files with element/file changes"),
	};
	const int values[7] =
	{
		kpi.total, kpi.mismatch, kpi.matched, kpi.avgNumeric,
		kpi.warning, kpi.coord, kpi.addDel,
	};
	for (int i = 0; i < 7; ++i)
	{
		const String text = labels[static_cast<size_t>(i)] + _T(": ") + std::to_wstring(values[i]);
		m_stKpi[i].SetWindowText(text.c_str());
	}
}

void CFormCompareView::RefreshList()
{
	CFormCompareDoc* pDoc = GetDocument();
	m_wndList.SetRedraw(FALSE);
	m_wndList.DeleteAllItems();
	if (pDoc != nullptr && pDoc->IsAnalyzed())
	{
		const std::vector<Merge20FormFileResult>& results = pDoc->GetResults();
		const std::vector<size_t>& visible = pDoc->GetVisibleIndices();
		int row = 0;
		for (size_t index : visible)
		{
			const Merge20FormFileResult& result = results[index];
			const int item = m_wndList.InsertItem(row, result.fileName.c_str());
			if (item < 0)
				continue;
			m_wndList.SetItemText(item, 1, result.formName.c_str());
			m_wndList.SetItemText(item, 2, result.tendency.c_str());
			m_wndList.SetItemText(item, 3, result.summary.c_str());
			m_wndList.SetItemText(item, 4, FormRoundedTotal(result.totalNumericSum).c_str());
			m_wndList.SetItemText(item, 5, std::to_wstring(result.changeCount).c_str());
			m_wndList.SetItemText(item, 6, SeverityDisplayText(result.severity).c_str());
			m_wndList.SetItemText(item, 7, result.isWarning ? _("Needs attention").c_str() : _T(""));
			++row;
		}
	}
	m_wndList.SetRedraw(TRUE);
	// Empty state: no changed files at all, or nothing matches
	// the current filter / search.
	bool showEmpty = false;
	String emptyText;
	if (pDoc != nullptr && pDoc->IsAnalyzed())
	{
		if (pDoc->GetResults().empty())
		{
			showEmpty = true;
			emptyText = _("No changed files");
		}
		else if (pDoc->GetVisibleIndices().empty())
		{
			showEmpty = true;
			emptyText = _("No files match the filter or search text.");
		}
	}
	m_stEmpty.SetWindowText(emptyText.c_str());
	m_stEmpty.ShowWindow(showEmpty ? SW_SHOW : SW_HIDE);
	m_wndList.ShowWindow(showEmpty ? SW_HIDE : SW_SHOW);
	// Keep a selection: first row when nothing is selected.
	if (!showEmpty && GetSelectedVisibleIndex() < 0 && m_wndList.GetItemCount() > 0)
		SelectVisibleIndex(0);
}

void CFormCompareView::RefreshNavigator()
{
	CFormCompareDoc* pDoc = GetDocument();
	const int oldSel = m_wndNav.GetCurSel();
	m_wndNav.SetRedraw(FALSE);
	m_wndNav.ResetContent();
	if (pDoc != nullptr && pDoc->IsAnalyzed())
	{
		const std::vector<Merge20FormFileResult>& results = pDoc->GetResults();
		const std::vector<size_t>& visible = pDoc->GetVisibleIndices();
		for (size_t index : visible)
		{
			const Merge20FormFileResult& result = results[index];
			const String text = result.formName + _T(" (") + result.fileName + _T(")");
			m_wndNav.AddString(text.c_str());
		}
	}
	m_wndNav.SetRedraw(TRUE);
	if (oldSel >= 0 && oldSel < m_wndNav.GetCount())
		m_wndNav.SetCurSel(oldSel);
	const int selected = GetSelectedVisibleIndex();
	if (selected >= 0)
		m_wndNav.SetCurSel(selected);
}

void CFormCompareView::SelectVisibleIndex(int visibleIndex)
{
	if (m_wndList.GetSafeHwnd() == nullptr)
		return;
	if (visibleIndex < 0 || visibleIndex >= m_wndList.GetItemCount())
		return;
	m_bSyncingSelection = true;
	m_wndList.SetItemState(visibleIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	m_wndList.EnsureVisible(visibleIndex, FALSE);
	if (m_wndNav.GetSafeHwnd() != nullptr && visibleIndex < m_wndNav.GetCount())
		m_wndNav.SetCurSel(visibleIndex);
	m_bSyncingSelection = false;
}

int CFormCompareView::GetSelectedVisibleIndex() const
{
	if (m_wndList.GetSafeHwnd() == nullptr)
		return -1;
	const int selected = m_wndList.GetNextItem(-1, LVNI_SELECTED);
	if (selected < 0)
		return -1;
	return selected;
}

String CFormCompareView::SeverityDisplayText(Merge20FormSeverity severity)
{
	switch (severity)
	{
	case Merge20FormSeverity::LayoutBreaking:
		return _("Layout-breaking");
	case Merge20FormSeverity::VisualOnly:
		return _("Visual only");
	case Merge20FormSeverity::Info:
	default:
		return _("Information");
	}
}

void CFormCompareView::OnFilterTabChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	CFormCompareDoc* pDoc = GetDocument();
	if (pDoc != nullptr)
	{
		const int sel = m_wndTabs.GetCurSel();
		if (sel >= 0 && sel <= static_cast<int>(Merge20FormFilter::LayoutBreaking))
			pDoc->SetFilter(static_cast<Merge20FormFilter>(sel));
	}
	if (pResult != nullptr)
		*pResult = 0;
}

void CFormCompareView::OnSearchChanged()
{
	CFormCompareDoc* pDoc = GetDocument();
	if (pDoc == nullptr || m_wndSearch.GetSafeHwnd() == nullptr)
		return;
	CString text;
	m_wndSearch.GetWindowText(text);
	pDoc->SetSearchText(static_cast<LPCTSTR>(text));
}

void CFormCompareView::OnSortChanged()
{
	CFormCompareDoc* pDoc = GetDocument();
	if (pDoc != nullptr)
	{
		const int sel = m_wndSort.GetCurSel();
		if (sel >= 0 && sel <= static_cast<int>(Merge20FormSort::Severity))
			pDoc->SetSortOrder(static_cast<Merge20FormSort>(sel));
	}
}

void CFormCompareView::OnRerun()
{
	CFormCompareDoc* pDoc = GetDocument();
	if (pDoc == nullptr || !pDoc->HasFolders())
		return;
	CWaitCursor wait;
	pDoc->RunAnalysis();
}

void CFormCompareView::OnPrevForm()
{
	const int selected = GetSelectedVisibleIndex();
	if (selected > 0)
		SelectVisibleIndex(selected - 1);
}

void CFormCompareView::OnNextForm()
{
	const int selected = GetSelectedVisibleIndex();
	if (selected >= 0 && selected + 1 < m_wndList.GetItemCount())
		SelectVisibleIndex(selected + 1);
}

void CFormCompareView::OnNavToggle()
{
	m_bNavVisible = !m_bNavVisible;
	m_btnNavToggle.SetWindowText((m_bNavVisible ? _("Hide navigator") : _("Show navigator")).c_str());
	LayoutControls();
}

void CFormCompareView::OnNavSelChanged()
{
	if (m_bSyncingSelection)
		return;
	const int sel = m_wndNav.GetCurSel();
	if (sel >= 0)
		SelectVisibleIndex(sel);
}

void CFormCompareView::OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_bSyncingSelection)
	{
		if (pResult != nullptr)
			*pResult = 0;
		return;
	}
	const NMLISTVIEW* pInfo = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	if ((pInfo->uChanged & LVIF_STATE) != 0 &&
		(pInfo->uNewState & LVIS_SELECTED) != 0)
	{
		m_bSyncingSelection = true;
		if (m_wndNav.GetSafeHwnd() != nullptr && pInfo->iItem < m_wndNav.GetCount())
			m_wndNav.SetCurSel(pInfo->iItem);
		m_bSyncingSelection = false;
	}
	if (pResult != nullptr)
		*pResult = 0;
}
