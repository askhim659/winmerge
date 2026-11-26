// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20SniperDlg.cpp
 *
 * @brief Implementation of the auto sniper overlay dialog
 * (Merge20 Phase 4 D6).
 */
#include "StdAfx.h"
#include "Merge20/Merge20SniperDlg.h"
#include "Merge20/Merge20SniperView.h"
#include "Merge20/Merge20Sniper.h"
#include "Merge20/Merge20Focus.h"
#include "MergeDoc.h"
#include "MergeEditView.h"
#include "MergeResultPane.h"
#include "Merge.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20SniperDlg, CTrDialog)

BEGIN_MESSAGE_MAP(CMerge20SniperDlg, CTrDialog)
	//{{AFX_MSG_MAP(CMerge20SniperDlg)
	ON_WM_SIZE()
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MERGE20_SNIPER_PREV, IDC_MERGE20_SNIPER_CLOSE, OnButtonClicked)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

namespace
{
/** @brief Header strip height in pixels. */
constexpr int SniperHeaderHeight = 34;
/** @brief Panel label height in pixels. */
constexpr int SniperLabelHeight = 20;
/** @brief Minimum line height fallback, in pixels, when the panel
 * has no font yet (the scroll conversion needs a divisor). */
constexpr int SniperFallbackLineHeight = 24;
}

CMerge20SniperDlg::CMerge20SniperDlg()
	: CTrDialog(IDD_MERGE20_SNIPER)
	, m_pDoc(nullptr)
	, m_nCurrentDiff(-1)
	, m_nDiffCount(0)
	, m_nBuffers(2)
	, m_bResultBuilt(false)
	, m_nSegmentState(-1)
	, m_layout(Merge20SniperLayout::Columns)
	, m_bSyncingScroll(false)
{
	m_pView[0] = nullptr;
	m_pView[1] = nullptr;
	m_pView[2] = nullptr;
}

CMerge20SniperDlg::~CMerge20SniperDlg()
{
	// The panels are CViews: they delete themselves in
	// PostNcDestroy when the dialog window is destroyed.
}

BOOL CMerge20SniperDlg::Create(CWnd* pParentWnd)
{
	return CDialog::Create(IDD_MERGE20_SNIPER, pParentWnd);
}

BOOL CMerge20SniperDlg::OnInitDialog()
{
	__super::OnInitDialog();

	const DWORD dwBtnStyle = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
	const DWORD dwLabelStyle = WS_CHILD | WS_VISIBLE | SS_LEFT;
	m_stTitle.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);
	m_stTitle.SetWindowText(_("Auto Sniper").c_str());
	m_stPos.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);
	m_stLabel[0].Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);
	m_stLabel[0].SetWindowText(_("Old").c_str());
	m_stLabel[1].Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);
	m_stLabel[1].SetWindowText(_("New side").c_str());
	m_stLabel[2].Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);
	m_stLabel[2].SetWindowText(_("Merged result").c_str());

	m_btnPrev.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_SNIPER_PREV);
	m_btnPrev.SetWindowText(_("Previous Diff").c_str());
	m_btnNext.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_SNIPER_NEXT);
	m_btnNext.SetWindowText(_("Next Diff").c_str());
	m_btnLayout.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_SNIPER_LAYOUT);
	m_btnLayout.SetWindowText(_("3 Rows").c_str());
	m_btnFontDown.Create(_T("A-"), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_SNIPER_FONT_DOWN);
	m_btnFontUp.Create(_T("A+"), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_SNIPER_FONT_UP);
	m_btnSplit.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_SNIPER_SPLIT);
	m_btnSplit.SetWindowText(_("Split").c_str());
	m_btnRegroup.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_SNIPER_REGROUP);
	m_btnRegroup.SetWindowText(_("Regroup").c_str());
	m_btnClose.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_SNIPER_CLOSE);
	m_btnClose.SetWindowText(_("Close Sniper").c_str());

	// The panels are read-only CGhostTextViews with their own
	// buffers (D4 precedent: AFX_WS_DEFAULT_VIEW without border,
	// nullptr context, WM_INITIALUPDATE sent explicitly).
	const DWORD dwViewStyle = AFX_WS_DEFAULT_VIEW & ~WS_BORDER;
	static const UINT nViewIds[3] =
	{
		IDC_MERGE20_SNIPER_VIEW_OLD,
		IDC_MERGE20_SNIPER_VIEW_NEW,
		IDC_MERGE20_SNIPER_VIEW_MERGED,
	};
	for (int i = 0; i < 3; ++i)
	{
		m_pView[i] = new CMerge20SniperView();
		if (!m_pView[i]->Create(nullptr, nullptr, dwViewStyle, CRect(0, 0, 1, 1), this, nViewIds[i], nullptr))
		{
			TRACE0("Failed to create CMerge20SniperView\n");
			delete m_pView[i];
			m_pView[i] = nullptr;
			return FALSE;
		}
		m_pView[i]->SendMessage(WM_INITIALUPDATE);
	}

	ApplyFont();
	LayoutAll();
	UpdateHeader();
	return TRUE;
}

void CMerge20SniperDlg::OpenSniper(CMergeDoc* pDoc, int nDiff)
{
	if (pDoc == nullptr || m_hWnd == nullptr)
		return;
	m_pDoc = pDoc;
	m_layout = static_cast<Merge20SniperLayout>(GetOptionsMgr()->GetInt(OPT_MERGE20_SNIPER_LAYOUT));

	// Full-screen overlay on the monitor showing the frame (HTML
	// `#sniperOv`: position fixed, inset 0).
	CRect rc(0, 0, 800, 600);
	if (CWnd* pParent = GetParent())
	{
		HMONITOR hMon = ::MonitorFromWindow(pParent->GetSafeHwnd(), MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi;
		mi.cbSize = sizeof(mi);
		if (hMon != nullptr && ::GetMonitorInfo(hMon, &mi))
			rc = mi.rcMonitor;
		else
			pParent->GetWindowRect(&rc);
	}
	MoveWindow(&rc);
	ApplyFont();
	LayoutAll();
	ShowDiff(nDiff);
	ShowWindow(SW_SHOW);
	// The focus goes to the first panel so the sniper keys work
	// at once (the view forwards them to HandleSniperKey).
	if (m_pView[0] != nullptr && m_pView[0]->GetSafeHwnd() != nullptr)
		m_pView[0]->SetFocus();
}

void CMerge20SniperDlg::CloseSniper()
{
	if (m_hWnd == nullptr || !IsWindowVisible())
		return;
	ShowWindow(SW_HIDE);
	if (CWnd* pParent = GetParent())
		pParent->SetFocus();
}

void CMerge20SniperDlg::OnCancel()
{
	// Modeless dialog: never destroy the window on Esc/Cancel,
	// only hide it (the frame owns this dialog as a member).
	CloseSniper();
}

BOOL CMerge20SniperDlg::PreTranslateMessage(MSG* pMsg)
{
	// Keys while the focus is on a header button: the panels
	// handle their own keys in CMerge20SniperView::OnKeyDown, and
	// the frame forwards here too (CMergeEditFrame::
	// PreTranslateMessage), so a key is never lost to focus.
	if (pMsg->message == WM_KEYDOWN && HandleSniperKey(static_cast<UINT>(pMsg->wParam)))
		return TRUE;
	return CTrDialog::PreTranslateMessage(pMsg);
}

bool CMerge20SniperDlg::HandleSniperKey(UINT nChar)
{
	switch (nChar)
	{
	case VK_ESCAPE:
		CloseSniper();
		return true;
	case 'J':
	case VK_RIGHT:
		// HTML `initKeys`: J / Right is the next diff.
		MoveDiff(+1);
		return true;
	case 'K':
	case VK_LEFT:
		// HTML `initKeys`: K / Left is the previous diff.
		MoveDiff(-1);
		return true;
	case VK_UP:
	case VK_DOWN:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_SPACE:
		{
			// Spec section 4(g): Up/Down scroll 120px, PgUp/PgDn
			// and Space scroll 600px, converted to whole lines.
			const bool bPage = (nChar == VK_PRIOR || nChar == VK_NEXT || nChar == VK_SPACE);
			const bool bDown = (nChar == VK_DOWN || nChar == VK_NEXT || nChar == VK_SPACE);
			const int nPixels = bPage ?
				Merge20SniperScrollPagePixels : Merge20SniperScrollLinePixels;
			int nLineHeight = SniperFallbackLineHeight;
			if (m_pView[0] != nullptr && m_pView[0]->GetSafeHwnd() != nullptr)
			{
				const int nHeight = m_pView[0]->GetSniperLineHeight();
				if (nHeight > 0)
					nLineHeight = nHeight;
			}
			const int nLines = Merge20SniperScrollLines(nPixels, nLineHeight);
			ScrollByLines(bDown ? nLines : -nLines);
		}
		return true;
	default:
		return false;
	}
}

void CMerge20SniperDlg::ShowDiff(int nDiff)
{
	if (m_pDoc == nullptr)
		return;
	m_nCurrentDiff = nDiff;
	m_nBuffers = m_pDoc->m_nBuffers;
	m_nDiffCount = m_pDoc->GetDiffCount();
	m_bResultBuilt = m_pDoc->GetMergeResultBuildState();
	m_nSegmentState = -1;

	String sOld, sNew, sMerged;
	if (m_bResultBuilt && nDiff >= 0)
	{
		DIFFRANGE di;
		if (m_pDoc->m_diffList.GetDiff(nDiff, di))
		{
			// OLD is pane 0, NEW is the last pane (for a 3-way
			// diff the base pane is not shown: the overlay is
			// OLD / NEW / MERGED, like the HTML tool).
			sOld = m_pDoc->GetPaneApparentLinesText(0, di.dbegin, di.dend, nullptr);
			const int nNewPane = m_nBuffers - 1;
			sNew = m_pDoc->GetPaneApparentLinesText(nNewPane, di.dbegin, di.dend, nullptr);
			int nState = -1, nOp = 0;
			if (m_pDoc->Merge20GetFocusResultText(nDiff, sMerged, nState, nOp))
				m_nSegmentState = nState;
		}
	}
	// Placeholders reuse the D4 focus-dock msgids.
	if (sOld.empty())
		sOld = m_bResultBuilt ? (nDiff < 0 ? _("No difference selected") : _("(No lines)")) : _("No merge session");
	if (sNew.empty())
		sNew = m_bResultBuilt ? (nDiff < 0 ? _("No difference selected") : _("(No lines)")) : _("No merge session");
	if (sMerged.empty())
		sMerged = m_bResultBuilt ? (nDiff < 0 ? _("No difference selected") : _("(No lines)")) : _("No merge session");

	// Opening a diff resets all three panels to the top (HTML
	// `showSniper` resets scrollTop/scrollLeft). SetSegmentText
	// re-attaches the buffer, which resets the top line.
	if (m_pView[0] != nullptr && m_pView[0]->GetSafeHwnd() != nullptr)
		m_pView[0]->SetSegmentText(sOld);
	if (m_pView[1] != nullptr && m_pView[1]->GetSafeHwnd() != nullptr)
		m_pView[1]->SetSegmentText(sNew);
	if (m_pView[2] != nullptr && m_pView[2]->GetSafeHwnd() != nullptr)
		m_pView[2]->SetSegmentText(sMerged);
	UpdateHeader();
}

void CMerge20SniperDlg::MoveDiff(int nDirection)
{
	if (m_pDoc == nullptr || m_nDiffCount <= 0)
		return;
	int nNew = -1;
	if (nDirection > 0)
	{
		nNew = Merge20SniperWrapNext(
			m_pDoc->m_diffList.NextSignificantDiff(m_nCurrentDiff),
			m_pDoc->m_diffList.FirstSignificantDiff());
	}
	else
	{
		nNew = Merge20SniperWrapPrev(
			m_pDoc->m_diffList.PrevSignificantDiff(m_nCurrentDiff),
			m_pDoc->m_diffList.LastSignificantDiff());
	}
	if (nNew < 0 || nNew == m_nCurrentDiff)
		return;
	// The main window follows: the current diff moves with the
	// overlay, so closing the overlay leaves the main window on
	// the diff the sniper ended on.
	if (CMergeEditView* pView = m_pDoc->GetActiveMergeView())
		pView->SelectDiff(nNew, true, false);
	ShowDiff(nNew);
}

void CMerge20SniperDlg::ScrollByLines(int nLines)
{
	if (nLines == 0 || m_bSyncingScroll)
		return;
	m_bSyncingScroll = true;
	for (int i = 0; i < 3; ++i)
	{
		if (m_pView[i] == nullptr || m_pView[i]->GetSafeHwnd() == nullptr)
			continue;
		const int nTop = Merge20FocusClampTopLine(
			m_pView[i]->GetTopLine() + nLines, m_pView[i]->GetLineCount());
		m_pView[i]->ScrollToSubLine(nTop);
	}
	m_bSyncingScroll = false;
}

void CMerge20SniperDlg::SyncFromView(CMerge20SniperView* pSource, int nTopLine)
{
	if (m_bSyncingScroll || pSource == nullptr)
		return;
	m_bSyncingScroll = true;
	for (int i = 0; i < 3; ++i)
	{
		if (m_pView[i] == nullptr || m_pView[i] == pSource ||
			m_pView[i]->GetSafeHwnd() == nullptr)
			continue;
		const int nTop = Merge20FocusClampTopLine(nTopLine, m_pView[i]->GetLineCount());
		m_pView[i]->ScrollToSubLine(nTop);
	}
	m_bSyncingScroll = false;
}

void CMerge20SniperDlg::ApplyFont()
{
	const int nFontPt = Merge20SniperClampFont(
		GetOptionsMgr()->GetInt(OPT_MERGE20_SNIPER_FONT));
	CDC* pDC = GetDC();
	const int nLogPixelsY = (pDC != nullptr) ? pDC->GetDeviceCaps(LOGPIXELSY) : 96;
	if (pDC != nullptr)
		ReleaseDC(pDC);
	LOGFONT lf = theApp.m_lfDiff;
	lf.lfHeight = -MulDiv(nFontPt, nLogPixelsY, 72);
	for (int i = 0; i < 3; ++i)
	{
		if (m_pView[i] != nullptr && m_pView[i]->GetSafeHwnd() != nullptr)
			m_pView[i]->SetFont(lf);
	}
}

void CMerge20SniperDlg::OnSize(UINT nType, int cx, int cy)
{
	CTrDialog::OnSize(nType, cx, cy);
	LayoutAll();
}

void CMerge20SniperDlg::LayoutAll()
{
	if (m_hWnd == nullptr)
		return;
	CRect rcClient;
	GetClientRect(&rcClient);
	LayoutHeader(rcClient.Width());
	LayoutPanels(rcClient.Width(), rcClient.Height());
}

void CMerge20SniperDlg::LayoutHeader(int cx)
{
	if (m_stTitle.GetSafeHwnd() == nullptr)
		return;
	const int nY = 5, nH = 24;
	m_stTitle.MoveWindow(8, nY, 140, nH);
	// The position label takes what is left between the title and
	// the right-aligned buttons; it hides on a narrow overlay.
	const int nButtonsWidth = 110 + 110 + 90 + 44 + 44 + 90 + 90 + 110 + 8 * 6;
	const int nPosWidth = cx - 8 - 140 - 8 - nButtonsWidth - 8;
	if (nPosWidth > 60)
		m_stPos.MoveWindow(156, nY, nPosWidth, nH);
	else
		m_stPos.MoveWindow(156, nY, 0, nH);

	// Buttons, right-aligned, in header order.
	int nX = cx - 8;
	auto PlaceButton = [&](CButton& btn, int nW)
	{
		nX -= nW;
		btn.MoveWindow(nX, nY, nW, nH);
		nX -= 6;
	};
	PlaceButton(m_btnClose, 110);
	PlaceButton(m_btnRegroup, 90);
	PlaceButton(m_btnSplit, 90);
	PlaceButton(m_btnFontUp, 44);
	PlaceButton(m_btnFontDown, 44);
	PlaceButton(m_btnLayout, 90);
	PlaceButton(m_btnNext, 110);
	PlaceButton(m_btnPrev, 110);
}

void CMerge20SniperDlg::LayoutPanels(int cx, int cy)
{
	if (m_pView[0] == nullptr || m_pView[0]->GetSafeHwnd() == nullptr)
		return;
	const int nTop = SniperHeaderHeight;
	if (m_layout == Merge20SniperLayout::Columns)
	{
		const int nColWidth = cx / 3;
		for (int i = 0; i < 3; ++i)
		{
			const int nX = i * nColWidth;
			const int nW = (i == 2) ? (cx - nX) : nColWidth;
			m_stLabel[i].MoveWindow(nX + 4, nTop, nW - 8, SniperLabelHeight);
			m_pView[i]->MoveWindow(nX, nTop + SniperLabelHeight, nW, cy - nTop - SniperLabelHeight);
		}
	}
	else
	{
		const int nRowHeight = (cy - nTop) / 3;
		for (int i = 0; i < 3; ++i)
		{
			const int nY = nTop + i * nRowHeight;
			const int nH = (i == 2) ? (cy - nY) : nRowHeight;
			m_stLabel[i].MoveWindow(4, nY, cx - 8, SniperLabelHeight);
			m_pView[i]->MoveWindow(0, nY + SniperLabelHeight, cx, nH - SniperLabelHeight);
		}
	}
}

void CMerge20SniperDlg::UpdateHeader()
{
	if (m_stPos.GetSafeHwnd() == nullptr)
		return;
	String sLabel;
	if (!m_bResultBuilt)
		sLabel = _("No merge session");
	else if (m_nCurrentDiff < 0)
		sLabel = _("No difference selected");
	else
		sLabel = strutils::format_string3(_("Diff %1 of %2 (%3)"),
			strutils::to_str(m_nCurrentDiff + 1), strutils::to_str(m_nDiffCount),
			StateWord(m_nSegmentState));
	m_stPos.SetWindowText(sLabel.c_str());

	// The layout button shows the layout it switches to (HTML
	// "3段" toggle), not the current one.
	const String sLayout = (m_layout == Merge20SniperLayout::Columns) ?
		_("3 Rows") : _("3 Columns");
	m_btnLayout.SetWindowText(sLayout.c_str());

	const bool bHasDiff = m_bResultBuilt && m_nCurrentDiff >= 0;
	m_btnPrev.EnableWindow(m_nDiffCount > 1);
	m_btnNext.EnableWindow(m_nDiffCount > 1);
	m_btnSplit.EnableWindow(bHasDiff && m_pDoc != nullptr &&
		m_pDoc->Merge20CanSplitDiff(m_nCurrentDiff));
	m_btnRegroup.EnableWindow(bHasDiff && m_pDoc != nullptr &&
		m_pDoc->Merge20CanRegroupDiff(m_nCurrentDiff));
}

String CMerge20SniperDlg::StateWord(int nState) const
{
	// Same words as CMerge20FocusDock::StateWord (D4).
	switch (static_cast<ResultSegmentState>(nState))
	{
	case ResultSegmentState::Unresolved:
		return _("Unresolved");
	case ResultSegmentState::Conflict:
		return _("Conflict");
	case ResultSegmentState::Chosen:
		return _("Chosen");
	case ResultSegmentState::Auto:
		return _("Auto");
	case ResultSegmentState::Edited:
		return _("Edited");
	case ResultSegmentState::Common:
	default:
		return _("Unresolved");
	}
}

void CMerge20SniperDlg::OnButtonClicked(UINT nID)
{
	switch (nID)
	{
	case IDC_MERGE20_SNIPER_PREV:
		MoveDiff(-1);
		break;
	case IDC_MERGE20_SNIPER_NEXT:
		MoveDiff(+1);
		break;
	case IDC_MERGE20_SNIPER_LAYOUT:
		m_layout = Merge20SniperToggleLayout(m_layout);
		GetOptionsMgr()->SaveOption(OPT_MERGE20_SNIPER_LAYOUT, static_cast<int>(m_layout));
		LayoutAll();
		UpdateHeader();
		break;
	case IDC_MERGE20_SNIPER_FONT_DOWN:
	case IDC_MERGE20_SNIPER_FONT_UP:
		{
			const int nDelta = (nID == IDC_MERGE20_SNIPER_FONT_UP) ?
				Merge20SniperFontStep : -Merge20SniperFontStep;
			const int nFont = Merge20SniperStepFont(
				GetOptionsMgr()->GetInt(OPT_MERGE20_SNIPER_FONT), nDelta);
			GetOptionsMgr()->SaveOption(OPT_MERGE20_SNIPER_FONT, nFont);
			ApplyFont();
		}
		break;
	case IDC_MERGE20_SNIPER_SPLIT:
		// D5 body (public): it warns itself when the result has
		// hand-edited lines and refreshes the main views. The
		// current diff after the call is the first row of the
		// split group, so re-read it from the document.
		if (m_pDoc != nullptr && m_pDoc->Merge20SplitDiff(m_nCurrentDiff))
		{
			m_nDiffCount = m_pDoc->GetDiffCount();
			ShowDiff(m_pDoc->GetCurrentDiff());
		}
		break;
	case IDC_MERGE20_SNIPER_REGROUP:
		// D5 body (public): the current diff after the call is the
		// regrouped diff (first row's judgment carried over).
		if (m_pDoc != nullptr && m_pDoc->Merge20RegroupDiff(m_nCurrentDiff))
		{
			m_nDiffCount = m_pDoc->GetDiffCount();
			ShowDiff(m_pDoc->GetCurrentDiff());
		}
		break;
	case IDC_MERGE20_SNIPER_CLOSE:
		CloseSniper();
		break;
	default:
		break;
	}
}
