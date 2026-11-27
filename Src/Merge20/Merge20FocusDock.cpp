// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FocusDock.cpp
 *
 * @brief Implementation of the focus dock (Merge20 Phase 4 D4).
 *
 * The dock follows the CMergeResultContainer precedent (a plain
 * CWnd holding several children, because the sizing bar only sizes
 * one child) and the CMerge20DedBar precedent (buttons created in
 * code in OnCreate, laid out in OnSize, texts set in
 * UpdateResources with string literals so the .po extraction finds
 * them). It owns no state: ShowDiff is driven by the document.
 *
 * U5 (UI redesign, design sections 2.1 and 4/U5) adds a 2px accent
 * underline at the bottom of the header strip (OnPaint,
 * Merge20Theme::GetAccentColor). The dock behaviour is unchanged.
 */
#include "StdAfx.h"
#include "Merge20/Merge20FocusDock.h"
#include "Merge20/Merge20Theme.h"
#include "Merge20/Merge20FocusResultView.h"
#include "MergeEditSplitterView.h"
#include "MergeEditView.h"
#include "MergeDoc.h"
#include "MergeResultPane.h"
#include "resource.h"
#include "UnicodeString.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20FocusDock, CWnd)

namespace
{
	/** @brief Header height: one row of 22px buttons with 4px margins. */
	constexpr int Merge20FocusHeaderHeight = 30;
	/** @brief Button height and vertical position in the header. */
	constexpr int Merge20FocusButtonHeight = 22;
	constexpr int Merge20FocusButtonTop = 4;
	/** @brief Left margin and gap between header items. */
	constexpr int Merge20FocusMargin = 8;
	constexpr int Merge20FocusGap = 4;

	/** @brief Fixed header widths, in order: the info label, then
	 * the seven buttons in header order. Wide enough for the
	 * Japanese labels (see Japanese.po); buttons past the right
	 * edge are clipped, like the dedicated bar's. The info label
	 * is wider since B1: it also shows the conclusion word and the
	 * judged/total progress. */
	constexpr int Merge20FocusWidths[] =
	{
		320, // Info label (diff position + conclusion + progress)
		108, // Insert old
		108, // Reject
		84,  // New
		84,  // Old
		96,  // Both
		116, // Split
		116, // Regroup
	};

	/** @brief Control IDs of the buttons, in header order. */
	constexpr UINT Merge20FocusControlIDs[] =
	{
		IDC_MERGE20_FOCUS_INSERT_OLD,
		IDC_MERGE20_FOCUS_REJECT,
		IDC_MERGE20_FOCUS_NEW,
		IDC_MERGE20_FOCUS_OLD,
		IDC_MERGE20_FOCUS_BOTH,
		IDC_MERGE20_FOCUS_SPLIT,
		IDC_MERGE20_FOCUS_REGROUP,
	};

	/** @brief Judgement actions, in control-ID order. */
	constexpr Merge20FocusAction Merge20FocusActions[] =
	{
		Merge20FocusAction::InsertOld,
		Merge20FocusAction::Reject,
		Merge20FocusAction::ChooseNew,
		Merge20FocusAction::ChooseOld,
		Merge20FocusAction::ChooseBoth,
		Merge20FocusAction::Split,
		Merge20FocusAction::Regroup,
	};
} // namespace

BEGIN_MESSAGE_MAP(CMerge20FocusDock, CWnd)
	//{{AFX_MSG_MAP(CMerge20FocusDock)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_SYSCOLORCHANGE()
	ON_WM_SETTINGCHANGE()
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MERGE20_FOCUS_INSERT_OLD, IDC_MERGE20_FOCUS_REGROUP, OnButtonClicked)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CMerge20FocusDock::CMerge20FocusDock()
: m_pDoc(nullptr)
, m_pDetailSplitterView(nullptr)
, m_pResultView(nullptr)
, m_nCurrentDiff(-1)
, m_nCurrentOp(Merge20FocusOpNone)
, m_nSegmentState(-1)
, m_nDiffCount(0)
, m_nBuffers(0)
, m_bResultBuilt(false)
, m_bSyncingScroll(false)
{
}

CMerge20FocusDock::~CMerge20FocusDock()
{
	// The document must not call a dock that is gone (the frame is
	// destroyed before its document, like the detail splitter view's
	// RemoveMergeViews in its destructor).
	if (m_pDoc != nullptr)
	{
		m_pDoc->SetMerge20FocusDock(nullptr);
		m_pDoc = nullptr;
	}
}

BOOL CMerge20FocusDock::Create(CWnd* pParentWnd)
{
	CString wndclass = ::AfxRegisterWndClass(CS_DBLCLKS,
		::LoadCursor(nullptr, IDC_ARROW),
		::GetSysColorBrush(COLOR_BTNFACE), 0);
	DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
	if (!CWnd::Create(wndclass, _T("Merge20FocusDock"), dwStyle,
		CRect(0, 0, 0, 0), pParentWnd, 0))
		return FALSE;
	return TRUE;
}

int CMerge20FocusDock::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	CFont* pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	const DWORD dwPushStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
	CRect rc(0, 0, 0, 0);
	if (!m_stInfo.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_ENDELLIPSIS | SS_CENTERIMAGE,
		rc, this, IDC_MERGE20_FOCUS_INFO))
	{
		TRACE0("Failed to create Merge20FocusDock info label\n");
		return -1;
	}
	m_stInfo.SetFont(pFont);
	CButton* pButtons[] =
	{
		&m_btnInsertOld, &m_btnReject, &m_btnNew, &m_btnOld, &m_btnBoth,
		&m_btnSplit, &m_btnRegroup,
	};
	for (size_t i = 0; i < std::size(pButtons); ++i)
	{
		if (!pButtons[i]->Create(_T(""), dwPushStyle, rc, this, Merge20FocusControlIDs[i]))
		{
			TRACE0("Failed to create Merge20FocusDock button\n");
			return -1;
		}
		pButtons[i]->SetFont(pFont);
	}
	UpdateResources();
	LayoutHeader();
	return 0;
}

void CMerge20FocusDock::SetViews(CMergeEditSplitterView* pSplitterView, CMerge20FocusResultView* pResultView)
{
	m_pDetailSplitterView = pSplitterView;
	m_pResultView = pResultView;
	CRect rc;
	GetClientRect(&rc);
	LayoutViews(rc.Width(), rc.Height());
}

void CMerge20FocusDock::SetMergeDoc(CMergeDoc* pDoc)
{
	m_pDoc = pDoc;
	if (m_pDoc != nullptr)
		m_pDoc->SetMerge20FocusDock(this);
}

void CMerge20FocusDock::ShowDiff(int nDiff)
{
	if (m_pDoc == nullptr)
		return;
	m_nCurrentDiff = nDiff;
	m_nBuffers = m_pDoc->m_nBuffers;
	m_nDiffCount = m_pDoc->m_diffList.GetSize();
	m_bResultBuilt = m_pDoc->GetMergeResultBuildState();
	m_nCurrentOp = Merge20FocusOpNone;
	m_nSegmentState = -1;
	String sText;
	if (m_bResultBuilt && nDiff >= 0 && nDiff < m_nDiffCount)
	{
		int nState = -1, nOp = Merge20FocusOpNone;
		if (m_pDoc->Merge20GetFocusResultText(nDiff, sText, nState, nOp))
		{
			m_nSegmentState = nState;
			m_nCurrentOp = nOp;
		}
	}
	if (m_pResultView != nullptr && m_pResultView->GetSafeHwnd() != nullptr)
	{
		if (sText.empty())
		{
			// Placeholder line: explain why there is nothing to show.
			if (!m_bResultBuilt)
				sText = _("No merge session");
			else if (nDiff < 0)
				sText = _("No difference selected");
			else
				sText = _("(No lines)");
		}
		m_pResultView->SetSegmentText(sText);
	}
	UpdateHeader();
}

void CMerge20FocusDock::SyncFocusFromDetail(int nDetailTopOffset)
{
	if (m_bSyncingScroll || m_pResultView == nullptr || m_pResultView->GetSafeHwnd() == nullptr)
		return;
	const int nTop = Merge20FocusClampTopLine(nDetailTopOffset, m_pResultView->GetLineCount());
	m_bSyncingScroll = true;
	// ScrollToLine is protected in CCrystalTextView; the public
	// ScrollToSubLine override is the entry point (with word
	// wrap off a subline is a line, so this is the same scroll).
	m_pResultView->ScrollToSubLine(nTop);
	m_bSyncingScroll = false;
}

void CMerge20FocusDock::SyncDetailFromFocus(int nFocusTopLine)
{
	if (m_bSyncingScroll || m_pDoc == nullptr || m_pDetailSplitterView == nullptr)
		return;
	const int nDiff = m_pDoc->GetCurrentDiff();
	DIFFRANGE di;
	if (nDiff < 0 || !m_pDoc->m_diffList.GetDiff(nDiff, di))
		return;
	m_bSyncingScroll = true;
	// Scroll every detail pane; their ScrollToSubLine hook calls
	// back into SyncFocusFromDetail, which returns early on the flag.
	auto& splitter = m_pDetailSplitterView->m_wndSplitter;
	for (int nRow = 0; nRow < splitter.GetRowCount(); ++nRow)
	{
		for (int nCol = 0; nCol < splitter.GetColumnCount(); ++nCol)
		{
			if (auto* pView = dynamic_cast<CMergeEditView*>(splitter.GetPane(nRow, nCol)))
				pView->ScrollToSubLine(di.dbegin + nFocusTopLine);
		}
	}
	m_bSyncingScroll = false;
}

void CMerge20FocusDock::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	LayoutHeader();
	LayoutViews(cx, cy);
}

/**
 * @brief Paint the 2px accent underline at the bottom of the
 * header strip (U5). The header is Merge20FocusHeaderHeight px
 * tall; its last 2px are free (the info label and the buttons
 * end at ButtonTop + ButtonHeight = 26). The rest of the dock
 * background is the window class brush, as before; the views
 * below paint themselves.
 */
void CMerge20FocusDock::OnPaint()
{
	CPaintDC dc(this);
	CRect rect;
	GetClientRect(&rect);
	if (rect.IsRectEmpty())
		return;
	CRect rcLine(rect.left, Merge20FocusHeaderHeight - 2,
		rect.right, Merge20FocusHeaderHeight);
	if (rcLine.top >= rect.top && rcLine.bottom <= rect.bottom)
		dc.FillSolidRect(&rcLine,
			Merge20Theme::GetAccentColor(Merge20Theme::IsDarkMode()));
}

/**
 * @brief Repaint the header underline after a system colour
 * change (U5).
 */
void CMerge20FocusDock::OnSysColorChange()
{
	CWnd::OnSysColorChange();
	Invalidate(FALSE);
}

/**
 * @brief Same repaint as OnSysColorChange, for theme changes
 * announced through WM_SETTINGCHANGE (U5).
 */
void CMerge20FocusDock::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
{
	CWnd::OnSettingChange(uFlags, lpszSection);
	Invalidate(FALSE);
}

void CMerge20FocusDock::OnButtonClicked(UINT nID)
{
	const size_t idx = static_cast<size_t>(nID) - IDC_MERGE20_FOCUS_INSERT_OLD;
	if (idx >= std::size(Merge20FocusActions) || m_pDoc == nullptr)
		return;
	const Merge20FocusAction action = Merge20FocusActions[idx];
	if (action == Merge20FocusAction::Split)
	{
		// Merge20 (Phase 4 D5): split the current diff group into
		// per-row hunks (the body lives in CMergeDoc).
		if (m_nCurrentDiff >= 0)
			m_pDoc->Merge20SplitDiff(m_nCurrentDiff);
		return;
	}
	if (action == Merge20FocusAction::Regroup)
	{
		// Merge20 (Phase 4 D5): regroup the split group containing
		// the current diff (the body lives in CMergeDoc).
		if (m_nCurrentDiff >= 0)
			m_pDoc->Merge20RegroupDiff(m_nCurrentDiff);
		return;
	}
	if (m_nCurrentDiff < 0)
		return;
	// Merge20 (best-workflow B1): a dock judgement is a per-diff
	// conclusion, applied through the single entry point (result
	// rewrite + learning record + file linkage). The buttons are
	// only shown and enabled for the diff kinds they apply to
	// (UpdateHeader), so no per-kind guard is needed here.
	Merge20DiffConclusion conclusion;
	switch (action)
	{
	case Merge20FocusAction::InsertOld:
	case Merge20FocusAction::ChooseOld:
		conclusion = Merge20DiffConclusion::UseOld;
		break;
	case Merge20FocusAction::Reject:
		conclusion = Merge20DiffConclusion::Reject;
		break;
	case Merge20FocusAction::ChooseNew:
		conclusion = Merge20DiffConclusion::UseNew;
		break;
	case Merge20FocusAction::ChooseBoth:
		conclusion = Merge20DiffConclusion::UseBoth;
		break;
	default:
		return; // Split/Regroup are handled above; unknown actions do nothing.
	}
	// Merge20 (best-workflow B14): a destructive judgment on a
	// high-risk diff needs one confirmation; a cancel applies
	// nothing.
	if (!m_pDoc->Merge20ConfirmHighRiskDiffs({{m_nCurrentDiff, conclusion}}))
		return;
	m_pDoc->Merge20SetDiffConclusion(m_nCurrentDiff, conclusion);
}

void CMerge20FocusDock::UpdateResources()
{
	if (m_btnInsertOld.GetSafeHwnd() == nullptr)
		return;
	// The msgids are string literals, so the .po extraction finds
	// them; do not build them from a table.
	String sText = _("Insert old lines");
	m_btnInsertOld.SetWindowText(sText.c_str());
	sText = _("Reject");
	m_btnReject.SetWindowText(sText.c_str());
	sText = _("Use new");
	m_btnNew.SetWindowText(sText.c_str());
	sText = _("Use old");
	m_btnOld.SetWindowText(sText.c_str());
	sText = _("Use both");
	m_btnBoth.SetWindowText(sText.c_str());
	sText = _("Split");
	m_btnSplit.SetWindowText(sText.c_str());
	sText = _("Regroup");
	m_btnRegroup.SetWindowText(sText.c_str());
	UpdateHeader();
	// U5: repaint the header accent underline.
	Invalidate(FALSE);
}

void CMerge20FocusDock::LayoutHeader()
{
	if (m_stInfo.GetSafeHwnd() == nullptr)
		return;
	int x = Merge20FocusMargin;
	m_stInfo.SetWindowPos(nullptr, x, Merge20FocusButtonTop,
		Merge20FocusWidths[0], Merge20FocusButtonHeight,
		SWP_NOZORDER | SWP_NOACTIVATE);
	x += Merge20FocusWidths[0] + Merge20FocusGap;
	CButton* pButtons[] =
	{
		&m_btnInsertOld, &m_btnReject, &m_btnNew, &m_btnOld, &m_btnBoth,
		&m_btnSplit, &m_btnRegroup,
	};
	for (size_t i = 0; i < std::size(pButtons); ++i)
	{
		if (!pButtons[i]->IsWindowVisible())
			continue;
		pButtons[i]->SetWindowPos(nullptr, x, Merge20FocusButtonTop,
			Merge20FocusWidths[i + 1], Merge20FocusButtonHeight,
			SWP_NOZORDER | SWP_NOACTIVATE);
		x += Merge20FocusWidths[i + 1] + Merge20FocusGap;
	}
}

void CMerge20FocusDock::LayoutViews(int cx, int cy)
{
	if (m_pDetailSplitterView == nullptr || m_pResultView == nullptr)
		return; // Views are attached after creation; OnSize can fire first.
	int nRest = cy - Merge20FocusHeaderHeight;
	if (nRest < 0)
		nRest = 0;
	// Old/new panes take three fifths, the result segment two fifths.
	const int nSplitterH = nRest * 3 / 5;
	const int nResultH = nRest - nSplitterH;
	m_pDetailSplitterView->MoveWindow(0, Merge20FocusHeaderHeight, cx, nSplitterH);
	m_pResultView->MoveWindow(0, Merge20FocusHeaderHeight + nSplitterH, cx, nResultH);
}

void CMerge20FocusDock::UpdateHeader()
{
	if (m_stInfo.GetSafeHwnd() == nullptr)
		return;
	String sLabel;
	if (!m_bResultBuilt)
		sLabel = _("No merge session");
	else if (m_nCurrentDiff < 0)
	{
		sLabel = _("No difference selected");
		// Merge20 (best-workflow B1b): the progress is visible even
		// before a diff is selected (e.g. right after a session
		// restore, when the conclusions are back but no diff is
		// current yet).
		if (m_pDoc != nullptr)
		{
			int nJudged = 0, nTotal = 0;
			m_pDoc->Merge20GetConclusionProgress(nJudged, nTotal);
			sLabel += _T("・") + strutils::format_string2(_("Judged %1 of %2 diffs"),
				strutils::to_str(nJudged), strutils::to_str(nTotal));
		}
	}
	else
	{
		// Merge20 (best-workflow B1): the header shows the current
		// diff's conclusion (or the segment state when undecided)
		// and the judged/total progress, so the review state is
		// visible without opening the context menu.
		sLabel = strutils::format_string3(_("Diff %1 of %2 (%3)"),
			strutils::to_str(m_nCurrentDiff + 1), strutils::to_str(m_nDiffCount),
			ConclusionWord(m_pDoc->Merge20GetDiffConclusion(m_nCurrentDiff), m_nSegmentState));
		int nJudged = 0, nTotal = 0;
		m_pDoc->Merge20GetConclusionProgress(nJudged, nTotal);
		sLabel += _T("・") + strutils::format_string2(_("Judged %1 of %2 diffs"),
			strutils::to_str(nJudged), strutils::to_str(nTotal));
	}
	m_stInfo.SetWindowText(sLabel.c_str());

	const bool bJudge = m_bResultBuilt && m_nCurrentDiff >= 0;
	CButton* pButtons[] =
	{
		&m_btnInsertOld, &m_btnReject, &m_btnNew, &m_btnOld, &m_btnBoth,
		&m_btnSplit, &m_btnRegroup,
	};
	for (size_t i = 0; i < std::size(pButtons); ++i)
	{
		const bool bVisible = Merge20FocusActionVisible(Merge20FocusActions[i], m_nCurrentOp, m_nBuffers);
		pButtons[i]->ShowWindow(bVisible ? SW_SHOW : SW_HIDE);
		// Split/Regroup are enabled only when the current diff
		// supports them (D5): a splittable group for Split, a row of
		// a split group for Regroup.
		bool bEnabled = bVisible && bJudge;
		if (Merge20FocusActions[i] == Merge20FocusAction::Split)
			bEnabled = bEnabled && m_pDoc != nullptr && m_pDoc->Merge20CanSplitDiff(m_nCurrentDiff);
		else if (Merge20FocusActions[i] == Merge20FocusAction::Regroup)
			bEnabled = bEnabled && m_pDoc != nullptr && m_pDoc->Merge20CanRegroupDiff(m_nCurrentDiff);
		pButtons[i]->EnableWindow(bEnabled);
	}
	LayoutHeader();
}

String CMerge20FocusDock::ConclusionWord(Merge20DiffConclusion conclusion, int nState) const
{
	// The words reuse the Diff conclusion submenu msgids, so the
	// header and the menu always agree (see Japanese.po).
	switch (conclusion)
	{
	case Merge20DiffConclusion::UseOld:
		return _("Adopt old lines");
	case Merge20DiffConclusion::UseNew:
		return _("Adopt new lines");
	case Merge20DiffConclusion::UseBoth:
		return _("Adopt both");
	case Merge20DiffConclusion::Reject:
		return _("Reject this diff");
	case Merge20DiffConclusion::Keep:
		return _("Keep as is");
	case Merge20DiffConclusion::Undecided:
	default:
		return StateWord(nState);
	}
}

String CMerge20FocusDock::StateWord(int nState) const
{
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
		// A diff segment is never Common; -1 (no segment) never
		// reaches the label that shows the state word.
		return _("Unresolved");
	}
}
