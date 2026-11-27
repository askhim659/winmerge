// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCompareView.cpp
 *
 * @brief Implementation of the FORM compare view (Wave 5 F2).
 * Child controls follow the Merge20FileListBar precedent
 * (DEFAULT_GUI_FONT, dummy rect at creation, layout in OnSize,
 * literal _() calls for the .pot generator).
 * F5 adds the structured diff table under the preview pane
 * (Merge20FormTable rows; group rows expand/collapse on click,
 * record rows spotlight through FocusDiffRecord).
 * F6 adds the name-master edit button to the top bar
 * (design sections 7.1 and 8): the button text carries the
 * count badge, and after a save the document re-resolves every
 * form name. F7 adds the CSV export button to the top bar
 * (design sections 7.1 and 9): it opens the range-selection
 * dialog and writes the two-part CSV (BOM-prefixed UTF-8).
 */
#include "StdAfx.h"
#include "Merge20/Merge20FormCompareView.h"
#include "Merge20/Merge20FormMasterDlg.h"
#include "Merge20/Merge20FormCsvExport.h"
#include "Merge20/Merge20FormCsvExportDlg.h"
#include "FileOrFolderSelect.h"
#include "resource.h"
#include "UnicodeString.h"
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
	// Preview pane (F3): the HTML viewport is 640 px fixed, or
	// fully expanded (toggleViewportHeight); the list keeps at
	// least this height when the preview expands.
	const int kFormPreviewFixedHeight = 640;
	const int kFormMinListHeight = 120;
	// Spotlight inspector banner (F4): old/new previews side by
	// side (150 px each) with labels above them, the banner text
	// and the four buttons.
	const int kFormSpotlightHeight = 132;
	const int kFormSpotPreviewWidth = 150;
	const int kFormSpotLabelHeight = 18;
	// Diff table (F5): label + list strip at the bottom of the
	// bottom area (the preview pane gives way to it).
	const int kFormTableHeight = 200;

	// Column widths of the diff table (F5; the last column takes
	// the remaining width and scrolls horizontally).
	const int kFormTableColType = 110;
	const int kFormTableColElement = 280;
	const int kFormTableColValue = 280;
	const int kFormTableColDesc = 600;

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
	ON_BN_CLICKED(IDC_MERGE20_FORM_MASTER, OnMasterEdit)
	ON_BN_CLICKED(IDC_MERGE20_FORM_CSV, OnCsvExport)
	ON_BN_CLICKED(IDC_MERGE20_FORM_PREV, OnPrevForm)
	ON_BN_CLICKED(IDC_MERGE20_FORM_NEXT, OnNextForm)
	ON_BN_CLICKED(IDC_MERGE20_FORM_NAV_TOGGLE, OnNavToggle)
	ON_LBN_SELCHANGE(IDC_MERGE20_FORM_NAV, OnNavSelChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_MERGE20_FORM_LIST, OnListItemChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_MERGE20_FORM_TABLE, OnTableItemChanged)
	ON_NOTIFY(NM_CLICK, IDC_MERGE20_FORM_TABLE, OnTableClick)
	ON_BN_CLICKED(IDC_MERGE20_FORM_ZOOM_100, OnZoom100)
	ON_BN_CLICKED(IDC_MERGE20_FORM_ZOOM_25, OnZoom25)
	ON_BN_CLICKED(IDC_MERGE20_FORM_ZOOM_FIT, OnZoomFit)
	ON_BN_CLICKED(IDC_MERGE20_FORM_VIEWPORT_TOGGLE, OnViewportToggle)
	ON_BN_CLICKED(IDC_MERGE20_FORM_SPOT_PREV, OnSpotPrev)
	ON_BN_CLICKED(IDC_MERGE20_FORM_SPOT_NEXT, OnSpotNext)
	ON_BN_CLICKED(IDC_MERGE20_FORM_SPOT_CLEAR, OnSpotClear)
	ON_BN_CLICKED(IDC_MERGE20_FORM_SPOT_GOTO, OnSpotGoto)
	ON_CONTROL(MERGE20_FORM_CANVAS_SELCHANGE, IDC_MERGE20_FORM_CANVAS, OnCanvasShapeClicked)
	ON_WM_HSCROLL()
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
	// Name-master edit button (F6, design section 7.1): the top
	// bar holds the folder display, the re-run button and this
	// button.
	if (!m_btnMaster.Create(_T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_MASTER))
	{
		TRACE0("Failed to create Merge20FormCompareView name-master button\n");
		return -1;
	}
	m_btnMaster.SetFont(pFont);
	// CSV export button (F7, design sections 7.1 and 9).
	if (!m_btnCsv.Create(_T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_CSV))
	{
		TRACE0("Failed to create Merge20FormCompareView CSV button\n");
		return -1;
	}
	m_btnCsv.SetFont(pFont);
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
	if (!m_stPreviewLabel.Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE | SS_ENDELLIPSIS,
		rc, this, IDC_MERGE20_FORM_PREVIEW_LABEL))
	{
		TRACE0("Failed to create Merge20FormCompareView preview label\n");
		return -1;
	}
	m_stPreviewLabel.SetFont(pFont);
	if (!m_btnZoom100.Create(_T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_ZOOM_100))
	{
		TRACE0("Failed to create Merge20FormCompareView zoom 100% button\n");
		return -1;
	}
	m_btnZoom100.SetFont(pFont);
	if (!m_btnZoom25.Create(_T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_ZOOM_25))
	{
		TRACE0("Failed to create Merge20FormCompareView zoom 25% button\n");
		return -1;
	}
	m_btnZoom25.SetFont(pFont);
	if (!m_btnZoomFit.Create(_T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_ZOOM_FIT))
	{
		TRACE0("Failed to create Merge20FormCompareView zoom fit button\n");
		return -1;
	}
	m_btnZoomFit.SetFont(pFont);
	if (!m_wndZoomSlider.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_AUTOTICKS,
		rc, this, IDC_MERGE20_FORM_ZOOM_SLIDER))
	{
		TRACE0("Failed to create Merge20FormCompareView zoom slider\n");
		return -1;
	}
	m_wndZoomSlider.SetRange(10, 200, TRUE);
	m_wndZoomSlider.SetPos(100);
	m_wndZoomSlider.SetPageSize(5);
	m_wndZoomSlider.SetTicFreq(10);
	if (!m_stZoomLabel.Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
		rc, this, IDC_MERGE20_FORM_ZOOM_LABEL))
	{
		TRACE0("Failed to create Merge20FormCompareView zoom label\n");
		return -1;
	}
	m_stZoomLabel.SetFont(pFont);
	if (!m_btnViewportToggle.Create(_T(""), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_VIEWPORT_TOGGLE))
	{
		TRACE0("Failed to create Merge20FormCompareView viewport toggle\n");
		return -1;
	}
	m_btnViewportToggle.SetFont(pFont);
	if (!m_wndCanvas.Create(this, IDC_MERGE20_FORM_CANVAS))
	{
		TRACE0("Failed to create Merge20FormCompareView canvas\n");
		return -1;
	}
	// Structured diff table (F5): label + list under the
	// preview pane.
	if (!m_stTableLabel.Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
		rc, this, IDC_MERGE20_FORM_TABLE_LABEL))
	{
		TRACE0("Failed to create Merge20FormCompareView diff table label\n");
		return -1;
	}
	m_stTableLabel.SetFont(pFont);
	if (!m_wndTable.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
		rc, this, IDC_MERGE20_FORM_TABLE))
	{
		TRACE0("Failed to create Merge20FormCompareView diff table\n");
		return -1;
	}
	m_wndTable.SetFont(pFont);
	m_wndTable.SetExtendedStyle(m_wndTable.GetExtendedStyle() | LVS_EX_FULLROWSELECT);
	// Spotlight inspector banner (F4): created hidden; the panel
	// is created first so it stays behind the other controls.
	if (!m_stSpotPanel.Create(_T(""), WS_CHILD | WS_BORDER,
		rc, this, IDC_MERGE20_FORM_SPOT_PANEL))
	{
		TRACE0("Failed to create Merge20FormCompareView spotlight panel\n");
		return -1;
	}
	if (!m_stSpotOldLabel.Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
		rc, this, IDC_MERGE20_FORM_SPOT_OLD_LABEL))
	{
		TRACE0("Failed to create Merge20FormCompareView spotlight old label\n");
		return -1;
	}
	m_stSpotOldLabel.SetFont(pFont);
	if (!m_wndSpotOld.Create(this, IDC_MERGE20_FORM_SPOT_OLD))
	{
		TRACE0("Failed to create Merge20FormCompareView spotlight old preview\n");
		return -1;
	}
	if (!m_stSpotNewLabel.Create(_T(""), WS_CHILD | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE,
		rc, this, IDC_MERGE20_FORM_SPOT_NEW_LABEL))
	{
		TRACE0("Failed to create Merge20FormCompareView spotlight new label\n");
		return -1;
	}
	m_stSpotNewLabel.SetFont(pFont);
	if (!m_wndSpotNew.Create(this, IDC_MERGE20_FORM_SPOT_NEW))
	{
		TRACE0("Failed to create Merge20FormCompareView spotlight new preview\n");
		return -1;
	}
	if (!m_stSpotText.Create(_T(""), WS_CHILD | SS_LEFT,
		rc, this, IDC_MERGE20_FORM_SPOT_TEXT))
	{
		TRACE0("Failed to create Merge20FormCompareView spotlight text\n");
		return -1;
	}
	m_stSpotText.SetFont(pFont);
	if (!m_btnSpotPrev.Create(_T(""), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_SPOT_PREV))
	{
		TRACE0("Failed to create Merge20FormCompareView spotlight previous button\n");
		return -1;
	}
	m_btnSpotPrev.SetFont(pFont);
	if (!m_btnSpotNext.Create(_T(""), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_SPOT_NEXT))
	{
		TRACE0("Failed to create Merge20FormCompareView spotlight next button\n");
		return -1;
	}
	m_btnSpotNext.SetFont(pFont);
	if (!m_btnSpotClear.Create(_T(""), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_SPOT_CLEAR))
	{
		TRACE0("Failed to create Merge20FormCompareView spotlight clear button\n");
		return -1;
	}
	m_btnSpotClear.SetFont(pFont);
	if (!m_btnSpotGoto.Create(_T(""), WS_CHILD | WS_TABSTOP | BS_PUSHBUTTON,
		rc, this, IDC_MERGE20_FORM_SPOT_GOTO))
	{
		TRACE0("Failed to create Merge20FormCompareView spotlight go-to button\n");
		return -1;
	}
	m_btnSpotGoto.SetFont(pFont);
	return 0;
}

void CFormCompareView::OnInitialUpdate()
{
	CView::OnInitialUpdate();
	SetupColumns();
	SetupTableColumns();
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
	x += 90 + kFormRowGap;
	// Name-master edit button (F6): the text carries the count
	// badge (HTML `masterCountBadge`), so it is wider than the
	// other buttons in this row.
	const int masterBtnWidth = 210;
	m_btnMaster.MoveWindow(x, y, masterBtnWidth, kFormControlHeight);
	x += masterBtnWidth + kFormRowGap;
	// CSV export button (F7).
	const int csvBtnWidth = 120;
	m_btnCsv.MoveWindow(x, y, csvBtnWidth, kFormControlHeight);
	y += kFormControlHeight + kFormMargin;
	// Bottom area: navigator (left) + card list (top) + preview pane (bottom, F3).
	const int navWidth = m_bNavVisible ? kFormNavWidth : 0;
	const int bottomTop = y;
	const int bottomHeight = (std::max)(0, height - bottomTop - kFormMargin);
	const int listLeft = kFormMargin + navWidth + (m_bNavVisible ? kFormRowGap : 0);
	const int listWidth = (std::max)(0, width - listLeft - kFormMargin);
	// Diff table (F5): a fixed strip at the bottom of the bottom
	// area; the preview pane gives way to it.
	const int tableTotal = (std::min)(kFormTableHeight,
		(std::max)(0, bottomHeight - kFormMinListHeight));
	// Preview pane height: 640 px fixed (HTML toggleViewportHeight)
	// or the full bottom area minus a minimal list strip and the
	// diff table.
	const int previewTotal = (std::max)(0, (std::min)(
		m_bCanvasFullHeight ? bottomHeight - kFormMinListHeight - tableTotal :
			kFormPreviewFixedHeight + kFormControlHeight + kFormRowGap,
		bottomHeight - kFormMinListHeight - tableTotal));
	const int listHeight = (std::max)(0, bottomHeight - previewTotal - tableTotal -
		(previewTotal > 0 ? kFormRowGap : 0) - (tableTotal > 0 ? kFormRowGap : 0));
	if (m_bNavVisible)
		m_wndNav.MoveWindow(kFormMargin, bottomTop, navWidth, bottomHeight);
	else
		m_wndNav.MoveWindow(kFormMargin, bottomTop, 0, bottomHeight);
	m_wndList.MoveWindow(listLeft, bottomTop, listWidth, listHeight);
	m_stEmpty.MoveWindow(listLeft, bottomTop, listWidth, listHeight);
	// Preview pane (F3): label + zoom row, then the canvas.
	const int previewTop = bottomTop + listHeight + (previewTotal > 0 ? kFormRowGap : 0);
	const int zoomBtn100Width = 56;
	const int zoomBtn25Width = 56;
	const int zoomBtnFitWidth = 72;
	const int zoomSliderWidth = 130;
	const int zoomLabelWidth = 52;
	const int viewportToggleWidth = 150;
	const int zoomRowWidth = zoomBtn100Width + zoomBtn25Width + zoomBtnFitWidth +
		zoomSliderWidth + zoomLabelWidth + viewportToggleWidth + 5 * kFormRowGap;
	const int labelWidth = (std::max)(0, listWidth - zoomRowWidth - kFormRowGap);
	m_stPreviewLabel.MoveWindow(listLeft, previewTop, labelWidth, kFormControlHeight);
	int xZoom = listLeft + labelWidth + kFormRowGap;
	m_btnZoom100.MoveWindow(xZoom, previewTop, zoomBtn100Width, kFormControlHeight);
	xZoom += zoomBtn100Width + kFormRowGap;
	m_btnZoom25.MoveWindow(xZoom, previewTop, zoomBtn25Width, kFormControlHeight);
	xZoom += zoomBtn25Width + kFormRowGap;
	m_btnZoomFit.MoveWindow(xZoom, previewTop, zoomBtnFitWidth, kFormControlHeight);
	xZoom += zoomBtnFitWidth + kFormRowGap;
	m_wndZoomSlider.MoveWindow(xZoom, previewTop, zoomSliderWidth, kFormControlHeight);
	xZoom += zoomSliderWidth + kFormRowGap;
	m_stZoomLabel.MoveWindow(xZoom, previewTop, zoomLabelWidth, kFormControlHeight);
	xZoom += zoomLabelWidth + kFormRowGap;
	m_btnViewportToggle.MoveWindow(xZoom, previewTop, viewportToggleWidth, kFormControlHeight);
	const int spotlightHeight = m_bSpotlightVisible ? kFormSpotlightHeight : 0;
	const int bannerTop = previewTop + kFormControlHeight + kFormRowGap;
	const int canvasTop = bannerTop + (m_bSpotlightVisible ? spotlightHeight + kFormRowGap : 0);
	const int canvasHeight = (std::max)(0, previewTotal - kFormControlHeight - kFormRowGap -
		(m_bSpotlightVisible ? spotlightHeight + kFormRowGap : 0));
	if (m_bSpotlightVisible)
		LayoutSpotlightBanner(listLeft, bannerTop, listWidth);
	const int spotShow = m_bSpotlightVisible ? SW_SHOW : SW_HIDE;
	if (m_stSpotPanel.GetSafeHwnd() != nullptr)
	{
		m_stSpotPanel.ShowWindow(spotShow);
		m_stSpotOldLabel.ShowWindow(spotShow);
		m_wndSpotOld.ShowWindow(spotShow);
		m_stSpotNewLabel.ShowWindow(spotShow);
		m_wndSpotNew.ShowWindow(spotShow);
		m_stSpotText.ShowWindow(spotShow);
		m_btnSpotPrev.ShowWindow(spotShow);
		m_btnSpotNext.ShowWindow(spotShow);
		m_btnSpotClear.ShowWindow(spotShow);
		m_btnSpotGoto.ShowWindow(spotShow);
	}
	m_wndCanvas.MoveWindow(listLeft, canvasTop, listWidth, canvasHeight);
	// Diff table (F5): label + list under the preview pane.
	const int tableTop = previewTop + previewTotal + (previewTotal > 0 ? kFormRowGap : 0);
	if (m_stTableLabel.GetSafeHwnd() != nullptr && m_wndTable.GetSafeHwnd() != nullptr)
	{
		m_stTableLabel.MoveWindow(listLeft, tableTop, listWidth, kFormControlHeight);
		m_wndTable.MoveWindow(listLeft, tableTop + kFormControlHeight + kFormRowGap,
			listWidth, (std::max)(0, tableTotal - kFormControlHeight - kFormRowGap));
		const int tableShow = tableTotal > 0 ? SW_SHOW : SW_HIDE;
		m_stTableLabel.ShowWindow(tableShow);
		m_wndTable.ShowWindow(tableShow);
	}
	UpdateZoomControls();
}

void CFormCompareView::LayoutSpotlightBanner(int left, int top, int width)
{
	if (m_stSpotPanel.GetSafeHwnd() == nullptr)
		return;
	m_stSpotPanel.MoveWindow(left, top, width, kFormSpotlightHeight);
	const int btnPrevWidth = 96;
	const int btnNextWidth = 96;
	const int btnClearWidth = 130;
	const int btnGotoWidth = 110;
	const int btnHeight = 26;
	const int btnTop = top + (kFormSpotlightHeight - btnHeight) / 2;
	int xRight = left + width - kFormMargin;
	m_btnSpotGoto.MoveWindow(xRight - btnGotoWidth, btnTop, btnGotoWidth, btnHeight);
	xRight -= btnGotoWidth + kFormRowGap;
	m_btnSpotClear.MoveWindow(xRight - btnClearWidth, btnTop, btnClearWidth, btnHeight);
	xRight -= btnClearWidth + kFormRowGap;
	m_btnSpotNext.MoveWindow(xRight - btnNextWidth, btnTop, btnNextWidth, btnHeight);
	xRight -= btnNextWidth + kFormRowGap;
	m_btnSpotPrev.MoveWindow(xRight - btnPrevWidth, btnTop, btnPrevWidth, btnHeight);
	int x = left + kFormMargin;
	m_stSpotOldLabel.MoveWindow(x, top + 2, kFormSpotPreviewWidth, kFormSpotLabelHeight);
	m_wndSpotOld.MoveWindow(x, top + 2 + kFormSpotLabelHeight + 2,
		kFormSpotPreviewWidth, kFormSpotlightHeight - kFormSpotLabelHeight - 8);
	x += kFormSpotPreviewWidth + kFormRowGap;
	m_stSpotNewLabel.MoveWindow(x, top + 2, kFormSpotPreviewWidth, kFormSpotLabelHeight);
	m_wndSpotNew.MoveWindow(x, top + 2 + kFormSpotLabelHeight + 2,
		kFormSpotPreviewWidth, kFormSpotlightHeight - kFormSpotLabelHeight - 8);
	x += kFormSpotPreviewWidth + kFormRowGap;
	// xRight already points at the previous button's left edge
	// (it was moved past all four buttons above).
	const int textWidth = (std::max)(0, xRight - x - kFormRowGap);
	m_stSpotText.MoveWindow(x, top + 4, textWidth, kFormSpotlightHeight - 8);
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
	m_btnCsv.SetWindowText(_("Export to CSV").c_str());
	RefreshMasterButton();
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
	// Preview pane texts (F3). "100%" / "25%" need no translation.
	m_btnZoom100.SetWindowText(_T("100%"));
	m_btnZoom25.SetWindowText(_T("25%"));
	m_btnZoomFit.SetWindowText(_("Fit").c_str());
	m_btnViewportToggle.SetWindowText((m_bCanvasFullHeight ? _("Fixed height (640px)") : _("Expand to full height")).c_str());
	// Spotlight banner texts (F4).
	m_stSpotOldLabel.SetWindowText(_("Old (before)").c_str());
	m_stSpotNewLabel.SetWindowText(_("New (after)").c_str());
	m_btnSpotPrev.SetWindowText(_("Previous change").c_str());
	m_btnSpotNext.SetWindowText(_("Next change").c_str());
	m_btnSpotClear.SetWindowText(_("Clear spotlight").c_str());
	m_btnSpotGoto.SetWindowText(_("Go to changed area").c_str());
	if (m_bSpotlightVisible)
		UpdateSpotlightBanner();
	// Diff-table texts (F5).
	m_stTableLabel.SetWindowText(_("Structured diff table").c_str());
	if (m_wndTable.GetSafeHwnd() != nullptr)
	{
		while (m_wndTable.DeleteColumn(0))
			;
		SetupTableColumns();
	}
	RefreshTableLabel();
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
	RefreshCanvas();
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
	RefreshCanvas();
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
	// Re-analysis replaces the results, so every per-file
	// spotlight state (and the borrowed record pointer in the
	// canvas) is dropped first.
	m_spotlightByFile.clear();
	m_expandedTableGroups.clear();
	HideSpotlightBanner();
	CWaitCursor wait;
	pDoc->RunAnalysis();
}

void CFormCompareView::OnMasterEdit()
{
	CMerge20FormMasterDlg dlg(this);
	if (dlg.DoModal() != IDOK)
		return;
	// The dialog saved (and validated) the master: re-resolve
	// every form name from the file and refresh the list.
	CFormCompareDoc* pDoc = GetDocument();
	if (pDoc != nullptr)
		pDoc->RefreshFormNames();
	RefreshMasterButton();
}

void CFormCompareView::OnCsvExport()
{
	CFormCompareDoc* pDoc = GetDocument();
	if (pDoc == nullptr || !pDoc->IsAnalyzed())
	{
		AfxMessageBox(_("There are no comparison results to export. Run the analysis first.").c_str(), MB_ICONINFORMATION);
		return;
	}
	const std::vector<Merge20FormFileResult>& results = pDoc->GetResults();
	if (results.empty())
	{
		AfxMessageBox(_("There are no files with differences to export.").c_str(), MB_ICONINFORMATION);
		return;
	}
	// Range (design section 9): the filtered view or all changed
	// files, each labelled with its file count.
	CMerge20FormCsvExportDlg dlg(this,
		static_cast<int>(pDoc->GetVisibleIndices().size()),
		static_cast<int>(results.size()));
	if (dlg.DoModal() != IDOK)
		return;
	std::vector<size_t> indices;
	if (dlg.GetRange() == CMerge20FormCsvExportDlg::Range::FilteredOnly)
		indices = pDoc->GetVisibleIndices();
	else
	{
		indices.reserve(results.size());
		for (size_t i = 0; i < results.size(); ++i)
			indices.push_back(i);
	}
	// Default file name, as in the HTML tool
	// (XML_Diff_Report_YYYY-MM-DD.csv).
	const String defaultName = _T("XML_Diff_Report_") + String(CTime::GetCurrentTime().Format(_T("%Y-%m-%d")).GetString()) + _T(".csv");
	String filePath;
	if (!SelectFile(GetSafeHwnd(), filePath, false, defaultName.c_str(), _T(""),
		_T("CSV Files (*.csv)|*.csv|All Files (*.*)|*.*||"), _T("csv")))
		return;
	// Output date/time for the CSV title line (HTML `exportToExcel`).
	const String timestamp = String(CTime::GetCurrentTime().Format(_T("%Y-%m-%d %H:%M:%S")).GetString());
	String errStr;
	int exported = 0;
	if (!Merge20FormCsvExportToFile(results, pDoc->GetGroups(), indices, timestamp, filePath, errStr, exported))
	{
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
		return;
	}
	AfxMessageBox(strutils::format_string2(_("%1 files exported to %2"), strutils::to_str(exported), filePath).c_str(), MB_ICONINFORMATION);
}

void CFormCompareView::RefreshMasterButton()
{
	if (m_btnMaster.GetSafeHwnd() == nullptr)
		return;
	// The count badge (HTML `masterCountBadge`): unique ids in
	// the master file, or the default list when the file is
	// missing or unparsable.
	Merge20FormMaster master;
	String text;
	if (Merge20FormMasterLoadText(text))
	{
		int badLine = 0;
		if (!Merge20FormMasterParseText(text, master, badLine))
			master = Merge20FormMasterGetDefault();
	}
	else
		master = Merge20FormMasterGetDefault();
	const String kinds = strutils::to_str(Merge20FormMasterCountUniqueIds(master));
	m_btnMaster.SetWindowText(
		strutils::format_string1(_("Name master (%1 types)"), kinds).c_str());
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
		RefreshCanvas();
	}
	if (pResult != nullptr)
		*pResult = 0;
}

void CFormCompareView::RefreshCanvas()
{
	if (m_wndCanvas.GetSafeHwnd() == nullptr)
		return;
	const Merge20FormFileResult* pResult = GetSelectedResult();
	m_wndCanvas.SetResult(pResult);
	String label;
	if (pResult != nullptr)
		label = _("Visual preview") + _T(" \u2014 ") + pResult->fileName;
	else
		label = _("Select a form to preview it");
	m_stPreviewLabel.SetWindowText(label.c_str());
	UpdateZoomControls();
	ApplySpotlight();
	RefreshTable();
}

void CFormCompareView::UpdateZoomControls()
{
	if (m_wndZoomSlider.GetSafeHwnd() == nullptr || m_stZoomLabel.GetSafeHwnd() == nullptr)
		return;
	const double scale = m_wndCanvas.GetScale();
	const int percent = static_cast<int>(std::lround(scale * 100.0));
	m_wndZoomSlider.SetPos(static_cast<int>(Merge20FormCanvasClampSliderPercent(static_cast<double>(percent))));
	m_stZoomLabel.SetWindowText((std::to_wstring(percent) + L"%").c_str());
}

const Merge20FormFileResult* CFormCompareView::GetSelectedResult() const
{
	CFormCompareDoc* pDoc = GetDocument();
	if (pDoc == nullptr)
		return nullptr;
	const int selected = GetSelectedVisibleIndex();
	if (selected < 0)
		return nullptr;
	const std::vector<size_t>& visible = pDoc->GetVisibleIndices();
	if (selected >= static_cast<int>(visible.size()))
		return nullptr;
	const std::vector<Merge20FormFileResult>& results = pDoc->GetResults();
	const size_t index = visible[static_cast<size_t>(selected)];
	if (index >= results.size())
		return nullptr;
	return &results[index];
}

void CFormCompareView::OnZoom100()
{
	m_wndCanvas.SetZoomMode(Merge20FormZoomMode::Percent100);
	UpdateZoomControls();
}

void CFormCompareView::OnZoom25()
{
	m_wndCanvas.SetZoomMode(Merge20FormZoomMode::Percent25);
	UpdateZoomControls();
}

void CFormCompareView::OnZoomFit()
{
	m_wndCanvas.SetZoomMode(Merge20FormZoomMode::Fit);
	UpdateZoomControls();
}

void CFormCompareView::OnViewportToggle()
{
	m_bCanvasFullHeight = !m_bCanvasFullHeight;
	m_btnViewportToggle.SetWindowText((m_bCanvasFullHeight ? _("Fixed height (640px)") : _("Expand to full height")).c_str());
	LayoutControls();
}

void CFormCompareView::OnCanvasShapeClicked()
{
	// A canvas click selects the shape; F4 turns it into a
	// spotlight on the first visual record of that element.
	// A click on empty space sends no notification, so the
	// spotlight is left alone.
	const std::wstring key = m_wndCanvas.GetSelectedShapeKey();
	if (key.empty())
		return;
	const int recordIndex = FindRecordIndexForShapeKey(key);
	if (recordIndex >= 0)
		FocusDiffRecord(recordIndex);
}

void CFormCompareView::OnSpotPrev()
{
	StepSpotlight(-1);
}

void CFormCompareView::OnSpotNext()
{
	StepSpotlight(1);
}

void CFormCompareView::OnSpotClear()
{
	const Merge20FormFileResult* pResult = GetSelectedResult();
	if (pResult != nullptr)
		m_spotlightByFile.erase(pResult->fileName);
	HideSpotlightBanner();
}

void CFormCompareView::OnSpotGoto()
{
	// The spotlighted shape when there is one, otherwise the
	// first changed shape (HTML `scrollToChangedArea`).
	m_wndCanvas.ScrollToChangedArea();
}

void CFormCompareView::FocusDiffRecord(int recordIndex)
{
	const Merge20FormFileResult* pResult = GetSelectedResult();
	if (pResult == nullptr)
		return;
	Merge20FormSpotlightState state;
	const auto it = m_spotlightByFile.find(pResult->fileName);
	if (it != m_spotlightByFile.end())
		state = it->second;
	if (!Merge20FormSpotlightFocus(pResult->diffRecords, recordIndex, state))
		return;
	m_spotlightByFile[pResult->fileName] = state;
	m_wndCanvas.SetSpotlight(&pResult->diffRecords[static_cast<size_t>(state.recordIndex)]);
	m_bSpotlightVisible = true;
	UpdateSpotlightBanner();
	LayoutControls();
	m_wndCanvas.ScrollToShapeKey(state.shapeKey);
	EnsureTableRowVisible(state.recordIndex);
}

void CFormCompareView::ApplySpotlight()
{
	const Merge20FormFileResult* pResult = GetSelectedResult();
	if (pResult == nullptr)
	{
		HideSpotlightBanner();
		return;
	}
	const auto it = m_spotlightByFile.find(pResult->fileName);
	if (it == m_spotlightByFile.end() || !Merge20FormSpotlightIsActive(it->second))
	{
		HideSpotlightBanner();
		return;
	}
	// The saved state must still match the current result
	// (re-analysis is cleared in OnRerun, but be safe).
	const Merge20FormSpotlightState& state = it->second;
	if (state.recordIndex < 0 ||
		state.recordIndex >= static_cast<int>(pResult->diffRecords.size()) ||
		pResult->diffRecords[static_cast<size_t>(state.recordIndex)].element != state.shapeKey ||
		!pResult->diffRecords[static_cast<size_t>(state.recordIndex)].hasVisual)
	{
		m_spotlightByFile.erase(pResult->fileName);
		HideSpotlightBanner();
		return;
	}
	m_wndCanvas.SetSpotlight(&pResult->diffRecords[static_cast<size_t>(state.recordIndex)]);
	m_bSpotlightVisible = true;
	UpdateSpotlightBanner();
	LayoutControls();
	m_wndCanvas.ScrollToShapeKey(state.shapeKey);
}

void CFormCompareView::HideSpotlightBanner()
{
	m_wndCanvas.ClearSpotlight();
	if (m_wndSpotOld.GetSafeHwnd() != nullptr)
	{
		m_wndSpotOld.SetShape(nullptr, true);
		m_wndSpotNew.SetShape(nullptr, false);
	}
	m_bSpotlightVisible = false;
	LayoutControls();
}

void CFormCompareView::UpdateSpotlightBanner()
{
	const Merge20FormFileResult* pResult = GetSelectedResult();
	if (pResult == nullptr)
		return;
	const auto it = m_spotlightByFile.find(pResult->fileName);
	if (it == m_spotlightByFile.end())
		return;
	const Merge20FormSpotlightState& state = it->second;
	if (state.recordIndex < 0 ||
		state.recordIndex >= static_cast<int>(pResult->diffRecords.size()))
		return;
	const Merge20FormDiffRecord& record =
		pResult->diffRecords[static_cast<size_t>(state.recordIndex)];
	// Old/new side-by-side previews (design section 6 (4)).
	const int oldIndex = Merge20FormSpotlightFindShapeIndex(pResult->oldShapes, state.shapeKey);
	const int newIndex = Merge20FormSpotlightFindShapeIndex(pResult->newShapes, state.shapeKey);
	const Merge20FormShape* pOldShape = (oldIndex >= 0) ?
		&pResult->oldShapes[static_cast<size_t>(oldIndex)] : nullptr;
	const Merge20FormShape* pNewShape = (newIndex >= 0) ?
		&pResult->newShapes[static_cast<size_t>(newIndex)] : nullptr;
	if (m_wndSpotOld.GetSafeHwnd() != nullptr)
	{
		m_wndSpotOld.SetShape(pOldShape, true);
		m_wndSpotNew.SetShape(pNewShape, false);
	}
	// Banner text: element + description, old -> new coordinates
	// with deltas, then every attribute of the element.
	String text = _("Spotlight") + _T("  ") + record.element +
		_T("\r\n") + record.desc;
	String coordLine;
	if (pOldShape != nullptr && pNewShape != nullptr)
	{
		const int dx = static_cast<int>(std::lround(pNewShape->x - pOldShape->x));
		const int dy = static_cast<int>(std::lround(pNewShape->y - pOldShape->y));
		const int dw = static_cast<int>(std::lround(
			(pNewShape->x2 - pNewShape->x) - (pOldShape->x2 - pOldShape->x)));
		const int dh = static_cast<int>(std::lround(
			(pNewShape->y2 - pNewShape->y) - (pOldShape->y2 - pOldShape->y)));
		coordLine = _("Old (before)") + _T(" X:") +
			std::to_wstring(static_cast<int>(std::lround(pOldShape->x))) + _T(" Y:") +
			std::to_wstring(static_cast<int>(std::lround(pOldShape->y))) +
			_T("  \u2192  ") + _("New (after)") + _T(" X:") +
			std::to_wstring(static_cast<int>(std::lround(pNewShape->x))) + _T(" Y:") +
			std::to_wstring(static_cast<int>(std::lround(pNewShape->y))) +
			_T("  \u0394X:") + (dx >= 0 ? _T("+") : _T("")) + std::to_wstring(dx) +
			_T(" \u0394Y:") + (dy >= 0 ? _T("+") : _T("")) + std::to_wstring(dy);
		if (dw != 0 || dh != 0)
		{
			coordLine += String(_T("  \u0394W:")) + (dw >= 0 ? _T("+") : _T("")) + std::to_wstring(dw) +
				_T(" \u0394H:") + (dh >= 0 ? _T("+") : _T("")) + std::to_wstring(dh);
		}
	}
	else if (pNewShape != nullptr)
	{
		coordLine = _("New (after)") + _T(" X:") +
			std::to_wstring(static_cast<int>(std::lround(pNewShape->x))) + _T(" Y:") +
			std::to_wstring(static_cast<int>(std::lround(pNewShape->y)));
	}
	else if (pOldShape != nullptr)
	{
		coordLine = _("Old (before)") + _T(" X:") +
			std::to_wstring(static_cast<int>(std::lround(pOldShape->x))) + _T(" Y:") +
			std::to_wstring(static_cast<int>(std::lround(pOldShape->y)));
	}
	if (!coordLine.empty())
		text += _T("\r\n") + coordLine;
	String attrLine;
	bool firstAttr = true;
	for (const Merge20FormDiffRecord& elemRecord : pResult->diffRecords)
	{
		if (elemRecord.element != state.shapeKey)
			continue;
		if (!firstAttr)
			attrLine += _T("   ");
		firstAttr = false;
		attrLine += elemRecord.attr + _T(": ") + elemRecord.oldVal +
			_T(" \u2192 ") + elemRecord.newVal;
	}
	if (!attrLine.empty())
		text += _T("\r\n") + _("Changed attributes") + _T(": ") + attrLine;
	m_stSpotText.SetWindowText(text.c_str());
}

void CFormCompareView::StepSpotlight(int dir)
{
	const Merge20FormFileResult* pResult = GetSelectedResult();
	if (pResult == nullptr)
		return;
	Merge20FormSpotlightState state;
	const auto it = m_spotlightByFile.find(pResult->fileName);
	if (it != m_spotlightByFile.end())
		state = it->second;
	const int next = Merge20FormSpotlightStep(pResult->diffRecords, state, dir);
	if (next >= 0)
		FocusDiffRecord(next);
}

int CFormCompareView::FindRecordIndexForShapeKey(const std::wstring& key) const
{
	const Merge20FormFileResult* pResult = GetSelectedResult();
	if (pResult == nullptr)
		return -1;
	for (size_t i = 0; i < pResult->diffRecords.size(); ++i)
	{
		const Merge20FormDiffRecord& record = pResult->diffRecords[i];
		if (record.element == key && record.hasVisual)
			return static_cast<int>(i);
	}
	return -1;
}

void CFormCompareView::OnHScroll(UINT /*nCode*/, UINT /*nPos*/, CScrollBar* pScrollBar)
{
	if (pScrollBar != nullptr && pScrollBar->GetDlgCtrlID() == IDC_MERGE20_FORM_ZOOM_SLIDER)
	{
		m_wndCanvas.SetSliderPercent(static_cast<double>(m_wndZoomSlider.GetPos()));
		UpdateZoomControls();
		return;
	}
	CView::OnHScroll(0, 0, pScrollBar);
}

int CFormCompareView::GetSelectedResultIndex() const
{
	CFormCompareDoc* pDoc = GetDocument();
	if (pDoc == nullptr)
		return -1;
	const int selected = GetSelectedVisibleIndex();
	if (selected < 0)
		return -1;
	const std::vector<size_t>& visible = pDoc->GetVisibleIndices();
	if (selected >= static_cast<int>(visible.size()))
		return -1;
	return static_cast<int>(visible[static_cast<size_t>(selected)]);
}

void CFormCompareView::SetupTableColumns()
{
	// Literal _() calls (FileListBar precedent); the column order
	// is the HTML tool's (design section 2.9).
	m_wndTable.InsertColumn(0, _("Change type").c_str(), LVCFMT_LEFT, kFormTableColType);
	m_wndTable.InsertColumn(1, _("Element / attribute").c_str(), LVCFMT_LEFT, kFormTableColElement);
	m_wndTable.InsertColumn(2, _("Old value -> New value").c_str(), LVCFMT_LEFT, kFormTableColValue);
	m_wndTable.InsertColumn(3, _("Change description & impact analysis").c_str(), LVCFMT_LEFT, kFormTableColDesc);
}

void CFormCompareView::RefreshTable()
{
	if (m_wndTable.GetSafeHwnd() == nullptr)
		return;
	m_bSyncingTable = true;
	m_wndTable.SetRedraw(FALSE);
	m_wndTable.DeleteAllItems();
	m_tableRows.clear();
	const Merge20FormFileResult* pResult = GetSelectedResult();
	if (pResult != nullptr)
	{
		static const std::vector<Merge20FormDiffGroup> emptyGroups;
		const std::vector<Merge20FormDiffGroup>* pGroups = &emptyGroups;
		CFormCompareDoc* pDoc = GetDocument();
		const int resultIndex = GetSelectedResultIndex();
		if (pDoc != nullptr && resultIndex >= 0 &&
			resultIndex < static_cast<int>(pDoc->GetGroups().size()))
			pGroups = &pDoc->GetGroups()[static_cast<size_t>(resultIndex)];
		const std::set<std::wstring>& expanded = m_expandedTableGroups[pResult->fileName];
		m_tableRows = Merge20BuildFormDiffTableRows(*pResult, *pGroups, expanded);
		int row = 0;
		for (const Merge20FormTableRow& tableRow : m_tableRows)
		{
			const int item = m_wndTable.InsertItem(row, tableRow.typeText.c_str());
			if (item < 0)
				continue;
			m_wndTable.SetItemText(item, 1, tableRow.elementText.c_str());
			m_wndTable.SetItemText(item, 2, tableRow.valueText.c_str());
			m_wndTable.SetItemText(item, 3, tableRow.descText.c_str());
			++row;
		}
	}
	m_wndTable.SetRedraw(TRUE);
	m_bSyncingTable = false;
	RefreshTableLabel();
	// Keep the spotlighted record selected in the table when its
	// row is visible (a collapsed group stays collapsed here;
	// FocusDiffRecord expands it through EnsureTableRowVisible).
	if (pResult != nullptr)
	{
		const std::map<std::wstring, Merge20FormSpotlightState>::const_iterator spotIt =
			m_spotlightByFile.find(pResult->fileName);
		if (spotIt != m_spotlightByFile.end() && Merge20FormSpotlightIsActive(spotIt->second))
		{
			const int spotRow = FindTableRowForRecord(spotIt->second.recordIndex);
			if (spotRow >= 0)
			{
				m_bSyncingTable = true;
				m_wndTable.SetItemState(spotRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				m_wndTable.EnsureVisible(spotRow, FALSE);
				m_bSyncingTable = false;
			}
		}
	}
}

void CFormCompareView::RefreshTableLabel()
{
	if (m_stTableLabel.GetSafeHwnd() == nullptr)
		return;
	String label = _("Structured diff table");
	CFormCompareDoc* pDoc = GetDocument();
	const int resultIndex = GetSelectedResultIndex();
	if (pDoc != nullptr && resultIndex >= 0)
	{
		// Ignored-noise count (design sections 5.2 and 7.2).
		const int ignored = pDoc->GetIgnoredNoiseCount(static_cast<size_t>(resultIndex));
		if (ignored > 0)
		{
			label += _T(" (") + _("Minor differences ignored") + _T(": ") +
				std::to_wstring(ignored) + _T("\u4ef6)");
		}
	}
	m_stTableLabel.SetWindowText(label.c_str());
}

int CFormCompareView::FindTableRowForRecord(int recordIndex) const
{
	for (size_t i = 0; i < m_tableRows.size(); ++i)
	{
		if (!m_tableRows[i].isGroup && m_tableRows[i].recordIndex == recordIndex)
			return static_cast<int>(i);
	}
	return -1;
}

void CFormCompareView::EnsureTableRowVisible(int recordIndex)
{
	const Merge20FormFileResult* pResult = GetSelectedResult();
	if (pResult == nullptr || m_wndTable.GetSafeHwnd() == nullptr ||
		recordIndex < 0 || recordIndex >= static_cast<int>(pResult->diffRecords.size()))
		return;
	const int row = FindTableRowForRecord(recordIndex);
	if (row < 0)
	{
		// Inside a collapsed group: expand it and rebuild once
		// (RefreshTable's spotlight sync then selects the row).
		const std::wstring& groupId =
			pResult->diffRecords[static_cast<size_t>(recordIndex)].groupId;
		if (groupId.empty())
			return;
		m_expandedTableGroups[pResult->fileName].insert(groupId);
		RefreshTable();
		return;
	}
	m_bSyncingTable = true;
	m_wndTable.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	m_wndTable.EnsureVisible(row, FALSE);
	m_bSyncingTable = false;
}

void CFormCompareView::OnTableItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
	if (m_bSyncingTable)
	{
		if (pResult != nullptr)
			*pResult = 0;
		return;
	}
	const NMLISTVIEW* pInfo = reinterpret_cast<NMLISTVIEW*>(pNMHDR);
	if ((pInfo->uChanged & LVIF_STATE) == 0 ||
		(pInfo->uNewState & LVIS_SELECTED) == 0)
	{
		if (pResult != nullptr)
			*pResult = 0;
		return;
	}
	const int item = pInfo->iItem;
	if (item < 0 || item >= static_cast<int>(m_tableRows.size()))
	{
		if (pResult != nullptr)
			*pResult = 0;
		return;
	}
	// Group rows are handled by OnTableClick (a click on the
	// already-selected group row fires no LVN_ITEMCHANGED).
	const Merge20FormTableRow tableRow = m_tableRows[static_cast<size_t>(item)];
	if (tableRow.isGroup || tableRow.recordIndex < 0)
	{
		if (pResult != nullptr)
			*pResult = 0;
		return;
	}
	const Merge20FormFileResult* pFileResult = GetSelectedResult();
	if (pFileResult == nullptr ||
		tableRow.recordIndex >= static_cast<int>(pFileResult->diffRecords.size()))
	{
		if (pResult != nullptr)
			*pResult = 0;
		return;
	}
	// A record row click spotlights it when it has a visual;
	// records without one (file add/delete, XML error, an element
	// without a shape) are shown but never spotlighted (design
	// section 2.9).
	if (pFileResult->diffRecords[static_cast<size_t>(tableRow.recordIndex)].hasVisual)
		FocusDiffRecord(tableRow.recordIndex);
	if (pResult != nullptr)
		*pResult = 0;
}

void CFormCompareView::OnTableClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	if (m_wndTable.GetSafeHwnd() == nullptr)
	{
		if (pResult != nullptr)
			*pResult = 0;
		return;
	}
	CPoint point;
	::GetCursorPos(&point);
	m_wndTable.ScreenToClient(&point);
	const int item = m_wndTable.HitTest(point);
	if (item < 0 || item >= static_cast<int>(m_tableRows.size()))
	{
		if (pResult != nullptr)
			*pResult = 0;
		return;
	}
	const Merge20FormTableRow tableRow = m_tableRows[static_cast<size_t>(item)];
	if (!tableRow.isGroup)
	{
		if (pResult != nullptr)
			*pResult = 0;
		return;
	}
	const Merge20FormFileResult* pFileResult = GetSelectedResult();
	if (pFileResult == nullptr)
	{
		if (pResult != nullptr)
			*pResult = 0;
		return;
	}
	// A group row click expands / collapses the group (a
	// selection change alone cannot do this: clicking the
	// already-selected group row fires no LVN_ITEMCHANGED).
	std::set<std::wstring>& expanded = m_expandedTableGroups[pFileResult->fileName];
	if (tableRow.expanded)
		expanded.erase(tableRow.groupId);
	else
		expanded.insert(tableRow.groupId);
	RefreshTable();
	// Keep the group row selected after the rebuild.
	for (size_t i = 0; i < m_tableRows.size(); ++i)
	{
		if (m_tableRows[i].isGroup && m_tableRows[i].groupId == tableRow.groupId)
		{
			m_bSyncingTable = true;
			m_wndTable.SetItemState(static_cast<int>(i), LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			m_wndTable.EnsureVisible(static_cast<int>(i), FALSE);
			m_bSyncingTable = false;
			break;
		}
	}
	if (pResult != nullptr)
		*pResult = 0;
}
