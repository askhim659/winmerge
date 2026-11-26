// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FilterDeckBar.cpp
 *
 * @brief Implementation of the folder-compare filter deck bar
 * (H-FILTER, coverage matrix H-11/H-13/H-39). See
 * Merge20FilterDeckBar.h for the layout and the ownership rules.
 *
 * The bar is a thin shell over the option keys: a checkbox change
 * writes the keys (plus the HTML tgSt/tgAc coupling, add-only),
 * re-reads them into the checkboxes, and asks the folder view to
 * rebuild. The preset writes the HTML acPreset state the same way.
 */
#include "stdafx.h"
#include "Merge20/Merge20FilterDeckBar.h"
#include "DirView.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20FilterDeckBar, CSizingControlBarCF);

/** @brief Fixed bar height: two rows of controls (DedBar precedent). */
constexpr int Merge20FilterDeckBarHeight = 52;
/** @brief Control height and the two row tops. */
constexpr int Merge20FilterDeckRowHeight = 22;
constexpr int Merge20FilterDeckRowTop0 = 3;
constexpr int Merge20FilterDeckRowTop1 = 27;
/** @brief Left margin, gap between controls, extra gap between groups. */
constexpr int Merge20FilterDeckMargin = 8;
constexpr int Merge20FilterDeckGap = 6;
constexpr int Merge20FilterDeckGroupGap = 6;

// Merge20 (H-FILTER): option key of an existence-filter toggle, by
// Merge20FileStatus index. Same mapping as Merge20ShowStatusKey in
// DirView.cpp (kept in both places: the view owns the rebuild, the
// bar owns the checkboxes, and neither may depend on the other for
// a two-line switch).
static const String& Merge20DeckStatusKey(int index)
{
	switch (index)
	{
	case 1: return OPT_MERGE20_SHOW_STATUS_OLD_ONLY;
	case 2: return OPT_MERGE20_SHOW_STATUS_NEW_ONLY;
	case 3: return OPT_MERGE20_SHOW_STATUS_IDENTICAL;
	case 0:
	default: return OPT_MERGE20_SHOW_STATUS_MODIFIED;
	}
}

// Merge20 (H-FILTER): option key of an action-filter toggle, by
// ActionCategory index. Same mapping as Merge20ShowActionKey in
// DirView.cpp (see Merge20DeckStatusKey).
static const String& Merge20DeckActionKey(int index)
{
	switch (index)
	{
	case 1: return OPT_MERGE20_SHOW_ACTION_REVIEW_MIXED;
	case 2: return OPT_MERGE20_SHOW_ACTION_REVIEW_MOD_ONLY;
	case 3: return OPT_MERGE20_SHOW_ACTION_NO_MERGE_NEW_ADD;
	case 4: return OPT_MERGE20_SHOW_ACTION_COPY_OLD_FILE;
	case 5: return OPT_MERGE20_SHOW_ACTION_SKIP_NEW_FILE;
	case 6: return OPT_MERGE20_SHOW_ACTION_NO_ACTION_IDENTICAL;
	case 0:
	default: return OPT_MERGE20_SHOW_ACTION_MERGE_CLEAN_OLD;
	}
}

// Merge20 (H-FILTER): option key of a review-filter toggle, by
// ReviewState index. Same mapping as Merge20ShowReviewKey in
// DirView.cpp (see Merge20DeckStatusKey).
static const String& Merge20DeckReviewKey(int index)
{
	switch (index)
	{
	case 1: return OPT_MERGE20_SHOW_REVIEW_DONE;
	case 2: return OPT_MERGE20_SHOW_REVIEW_HOLD;
	case 3: return OPT_MERGE20_SHOW_REVIEW_SPECIAL;
	case 4: return OPT_MERGE20_SHOW_REVIEW_KEEP;
	case 0:
	default: return OPT_MERGE20_SHOW_REVIEW_TODO;
	}
}

BEGIN_MESSAGE_MAP(CMerge20FilterDeckBar, CSizingControlBarCF)
	//{{AFX_MSG_MAP(CMerge20FilterDeckBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_COMMAND_RANGE(IDC_MERGE20_FILTERDECK_STATUS_MODIFIED, IDC_MERGE20_FILTERDECK_REVIEW_KEEP, OnDeckCheck)
	ON_BN_CLICKED(IDC_MERGE20_FILTERDECK_PRESET, OnPreset)
	ON_BN_CLICKED(IDC_MERGE20_FILTERDECK_SHOW_ALL, OnShowAll)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CMerge20FilterDeckBar::CMerge20FilterDeckBar()
{
}

CMerge20FilterDeckBar::~CMerge20FilterDeckBar()
{
}

BOOL CMerge20FilterDeckBar::Create(
	CWnd* pParentWnd,
	LPCTSTR lpszWindowName /*= nullptr*/,
	DWORD dwStyle /*= WS_CHILD | WS_VISIBLE | CBRS_TOP*/,
	UINT nID /*= AFX_IDW_PANE_FIRST*/)
{
	return CSizingControlBarCF::Create(
		lpszWindowName,
		pParentWnd,
		nID,
		dwStyle);
}

int CMerge20FilterDeckBar::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CSizingControlBarCF::OnCreate(lpCreateStruct) == -1)
		return -1;

	// Top-docked sizing edges (DedBar precedent). No SCBS_SIZECHILD:
	// the height is fixed (see LoadState / CalcFixedLayout).
	SetSCBStyle(SCBS_EDGETOP | SCBS_EDGEBOTTOM | SCBS_SIZECHILD);

	// Explicit initial size (D1 lesson): an empty sizing bar must
	// not take its size from layout defaults.
	m_szHorz = CSize(120, Merge20FilterDeckBarHeight);
	m_szMinHorz = CSize(33, Merge20FilterDeckBarHeight);
	m_szFloat = CSize(600, Merge20FilterDeckBarHeight);
	m_szMinFloat = CSize(200, Merge20FilterDeckBarHeight);

	CFont* pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	const DWORD dwLabelStyle = WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_CENTERIMAGE;
	const DWORD dwCheckStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;
	const DWORD dwPushStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
	CRect rc(0, 0, 0, 0);
	// Group labels. Literal _() calls (see SetupFilterTabs in the
	// file-list bar): the .pot generator only scans literal arguments.
	if (!m_stStatusLabel.Create(_("Existence:").c_str(), dwLabelStyle, rc, this))
	{
		TRACE0("Failed to create Merge20FilterDeckBar status label\n");
		return -1;
	}
	m_stStatusLabel.SetFont(pFont);
	if (!m_stActionLabel.Create(_("Action category:").c_str(), dwLabelStyle, rc, this))
	{
		TRACE0("Failed to create Merge20FilterDeckBar action label\n");
		return -1;
	}
	m_stActionLabel.SetFont(pFont);
	if (!m_stReviewLabel.Create(_("Review state:").c_str(), dwLabelStyle, rc, this))
	{
		TRACE0("Failed to create Merge20FilterDeckBar review label\n");
		return -1;
	}
	m_stReviewLabel.SetFont(pFont);
	// Existence checkboxes (Merge20FileStatus order). The labels
	// reuse the S0 status msgids, so no new strings are needed.
	const String statusLabels[4] =
	{
		_("Changed"),
		_("Old only"),
		_("New only"),
		_("Identical"),
	};
	for (int i = 0; i < 4; ++i)
	{
		if (!m_btnStatus[i].Create(statusLabels[static_cast<size_t>(i)].c_str(),
			dwCheckStyle, rc, this, IDC_MERGE20_FILTERDECK_STATUS_MODIFIED + i))
		{
			TRACE0("Failed to create Merge20FilterDeckBar status checkbox\n");
			return -1;
		}
		m_btnStatus[i].SetFont(pFont);
	}
	// Action-category checkboxes (ActionCategory order). The labels
	// reuse the S4 column msgids (same literals as the file-list
	// panel's SetupFilterTabs), so no new strings are needed.
	const String actionLabels[7] =
	{
		_("Merge (missing in new)"),
		_("Review (missing + changed)"),
		_("Review (modified)"),
		_("New additions only"),
		_("Old-only file"),
		_("New-only file"),
		_("Identical"),
	};
	for (int i = 0; i < 7; ++i)
	{
		if (!m_btnAction[i].Create(actionLabels[static_cast<size_t>(i)].c_str(),
			dwCheckStyle, rc, this, IDC_MERGE20_FILTERDECK_ACTION_MERGE_CLEAN_OLD + i))
		{
			TRACE0("Failed to create Merge20FilterDeckBar action checkbox\n");
			return -1;
		}
		m_btnAction[i].SetFont(pFont);
	}
	// Review-state checkboxes (ReviewState order). The labels reuse
	// the D11 submenu msgids (Merge20ReviewStateMenuName), so no new
	// strings are needed.
	const String reviewLabels[5] =
	{
		_("Not reviewed"),
		_("Done"),
		_("On hold"),
		_("Needs manual work"),
		_("Keep as is"),
	};
	for (int i = 0; i < 5; ++i)
	{
		if (!m_btnReview[i].Create(reviewLabels[static_cast<size_t>(i)].c_str(),
			dwCheckStyle, rc, this, IDC_MERGE20_FILTERDECK_REVIEW_TODO + i))
		{
			TRACE0("Failed to create Merge20FilterDeckBar review checkbox\n");
			return -1;
		}
		m_btnReview[i].SetFont(pFont);
	}
	// Preset and show-all buttons (HTML acPreset / show everything).
	if (!m_btnPreset.Create(_("Only files needing action").c_str(),
		dwPushStyle, rc, this, IDC_MERGE20_FILTERDECK_PRESET))
	{
		TRACE0("Failed to create Merge20FilterDeckBar preset button\n");
		return -1;
	}
	m_btnPreset.SetFont(pFont);
	if (!m_btnShowAll.Create(_("Show all").c_str(),
		dwPushStyle, rc, this, IDC_MERGE20_FILTERDECK_SHOW_ALL))
	{
		TRACE0("Failed to create Merge20FilterDeckBar show-all button\n");
		return -1;
	}
	m_btnShowAll.SetFont(pFont);

	SyncCheckboxes();
	LayoutChildren();

	return 0;
}

void CMerge20FilterDeckBar::OnSize(UINT nType, int cx, int cy)
{
	CSizingControlBarCF::OnSize(nType, cx, cy);
	LayoutChildren();
}

/**
 * @brief Load the saved bar size, then clamp the height.
 *
 * The base LoadState reads the persisted sizeHorzCY. Without the
 * clamp, a saved abnormal height (or a dragged one) would come back
 * on the next frame and steal client area from the list (D2 lesson,
 * same shape as CMerge20DedBar::LoadState).
 */
void CMerge20FilterDeckBar::LoadState(LPCTSTR lpszProfileName)
{
	CSizingControlBarCF::LoadState(lpszProfileName);
	m_szHorz.cy = Merge20FilterDeckBarHeight;
	m_szVert.cx = Merge20FilterDeckBarHeight;
	m_szFloat.cy = Merge20FilterDeckBarHeight;
}

/**
 * @brief Clamp the height the layout gives the bar to the two fixed
 * rows (DedBar precedent: the dragged m_szHorz.cy is never used for
 * layout, and LoadState resets it on the next frame).
 */
CSize CMerge20FilterDeckBar::CalcFixedLayout(BOOL bStretch, BOOL bHorz)
{
	CSize size = CSizingControlBarCF::CalcFixedLayout(bStretch, bHorz);
	if (bHorz)
		size.cy = Merge20FilterDeckBarHeight;
	return size;
}

void CMerge20FilterDeckBar::SyncCheckboxes()
{
	if (GetSafeHwnd() == nullptr)
		return;
	const Merge20FilterDeckState state = ReadDeckStateFromOptions();
	for (int i = 0; i < 4; ++i)
		SetButtonCheck(m_btnStatus[i], state.showStatus[i]);
	for (int i = 0; i < 7; ++i)
		SetButtonCheck(m_btnAction[i], state.showAction[i]);
	for (int i = 0; i < 5; ++i)
		SetButtonCheck(m_btnReview[i], state.showReview[i]);
}

void CMerge20FilterDeckBar::UpdateResources()
{
	String sCaption = _("Filter Deck");
	SetWindowText(sCaption.c_str());
	if (m_stStatusLabel.GetSafeHwnd() != nullptr)
		m_stStatusLabel.SetWindowText(_("Existence:").c_str());
	if (m_stActionLabel.GetSafeHwnd() != nullptr)
		m_stActionLabel.SetWindowText(_("Action category:").c_str());
	if (m_stReviewLabel.GetSafeHwnd() != nullptr)
		m_stReviewLabel.SetWindowText(_("Review state:").c_str());
	const String statusLabels[4] =
	{
		_("Changed"),
		_("Old only"),
		_("New only"),
		_("Identical"),
	};
	for (int i = 0; i < 4; ++i)
	{
		if (m_btnStatus[i].GetSafeHwnd() != nullptr)
			m_btnStatus[i].SetWindowText(statusLabels[static_cast<size_t>(i)].c_str());
	}
	const String actionLabels[7] =
	{
		_("Merge (missing in new)"),
		_("Review (missing + changed)"),
		_("Review (modified)"),
		_("New additions only"),
		_("Old-only file"),
		_("New-only file"),
		_("Identical"),
	};
	for (int i = 0; i < 7; ++i)
	{
		if (m_btnAction[i].GetSafeHwnd() != nullptr)
			m_btnAction[i].SetWindowText(actionLabels[static_cast<size_t>(i)].c_str());
	}
	const String reviewLabels[5] =
	{
		_("Not reviewed"),
		_("Done"),
		_("On hold"),
		_("Needs manual work"),
		_("Keep as is"),
	};
	for (int i = 0; i < 5; ++i)
	{
		if (m_btnReview[i].GetSafeHwnd() != nullptr)
			m_btnReview[i].SetWindowText(reviewLabels[static_cast<size_t>(i)].c_str());
	}
	if (m_btnPreset.GetSafeHwnd() != nullptr)
		m_btnPreset.SetWindowText(_("Only files needing action").c_str());
	if (m_btnShowAll.GetSafeHwnd() != nullptr)
		m_btnShowAll.SetWindowText(_("Show all").c_str());
	LayoutChildren();
}

void CMerge20FilterDeckBar::LayoutChildren()
{
	if (GetSafeHwnd() == nullptr)
		return;
	CClientDC dc(this);
	CFont* pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	CFont* pOldFont = dc.SelectObject(pFont);
	// Width of a control's text, in the bar font.
	auto textWidth = [&dc](CWnd& wnd) -> int
	{
		CString text;
		wnd.GetWindowText(text);
		const CSize size = dc.GetTextExtent(text, text.GetLength());
		return size.cx;
	};
	// Place one control at the current x, row y, and return the x
	// for the next control.
	int x = Merge20FilterDeckMargin;
	auto place = [&](CWnd& wnd, int y, int width) -> int
	{
		if (wnd.GetSafeHwnd() != nullptr)
			wnd.MoveWindow(x, y, width, Merge20FilterDeckRowHeight);
		return x + width + Merge20FilterDeckGap;
	};
	// Row 1: existence group, action group, preset button. The
	// checkbox pad (24) covers the box itself plus the text gap;
	// the button pad (20) covers the button frame.
	x = place(m_stStatusLabel, Merge20FilterDeckRowTop0, textWidth(m_stStatusLabel) + 4);
	x += Merge20FilterDeckGroupGap;
	for (int i = 0; i < 4; ++i)
		x = place(m_btnStatus[i], Merge20FilterDeckRowTop0, textWidth(m_btnStatus[i]) + 24);
	x += Merge20FilterDeckGroupGap;
	x = place(m_stActionLabel, Merge20FilterDeckRowTop0, textWidth(m_stActionLabel) + 4);
	x += Merge20FilterDeckGroupGap;
	for (int i = 0; i < 7; ++i)
		x = place(m_btnAction[i], Merge20FilterDeckRowTop0, textWidth(m_btnAction[i]) + 24);
	x += Merge20FilterDeckGroupGap;
	(void)place(m_btnPreset, Merge20FilterDeckRowTop0, textWidth(m_btnPreset) + 20);
	// Row 2: review group, show-all button.
	x = Merge20FilterDeckMargin;
	x = place(m_stReviewLabel, Merge20FilterDeckRowTop1, textWidth(m_stReviewLabel) + 4);
	x += Merge20FilterDeckGroupGap;
	for (int i = 0; i < 5; ++i)
		x = place(m_btnReview[i], Merge20FilterDeckRowTop1, textWidth(m_btnReview[i]) + 24);
	x += Merge20FilterDeckGroupGap;
	(void)place(m_btnShowAll, Merge20FilterDeckRowTop1, textWidth(m_btnShowAll) + 20);
	dc.SelectObject(pOldFont);
}

void CMerge20FilterDeckBar::OnDeckCheck(UINT nID)
{
	Merge20FilterDeckState state = ReadDeckStateFromControls();
	// HTML tgSt / tgAc coupling, add-only: when the clicked box was
	// turned ON, also turn on its counterpart. The clicked box has
	// already toggled when BN_CLICKED fires, so a checked box here
	// means "just turned on". Review boxes have no counterpart.
	const int id = static_cast<int>(nID);
	if (id >= IDC_MERGE20_FILTERDECK_STATUS_MODIFIED &&
		id <= IDC_MERGE20_FILTERDECK_STATUS_IDENTICAL)
	{
		const int index = id - IDC_MERGE20_FILTERDECK_STATUS_MODIFIED;
		if (state.showStatus[index])
			Merge20FilterDeckSyncStatusOn(state, static_cast<Merge20FileStatus>(index));
	}
	else if (id >= IDC_MERGE20_FILTERDECK_ACTION_MERGE_CLEAN_OLD &&
		id <= IDC_MERGE20_FILTERDECK_ACTION_NO_ACTION_IDENTICAL)
	{
		const int index = id - IDC_MERGE20_FILTERDECK_ACTION_MERGE_CLEAN_OLD;
		if (state.showAction[index])
			Merge20FilterDeckSyncActionOn(state, static_cast<ActionCategory>(index));
	}
	WriteDeckStateToOptions(state);
	SyncCheckboxes();
	RefreshView();
}

void CMerge20FilterDeckBar::OnPreset()
{
	// HTML acPreset: existence {MODIFIED, OLD_ONLY}, action
	// {MERGE_CLEAN_OLD, REVIEW_MIXED, REVIEW_MOD_ONLY,
	// COPY_OLD_FILE}. The review group is read from the controls
	// and written back unchanged.
	Merge20FilterDeckState state = ReadDeckStateFromControls();
	Merge20FilterDeckApplyActionPreset(state);
	WriteDeckStateToOptions(state);
	SyncCheckboxes();
	RefreshView();
}

void CMerge20FilterDeckBar::OnShowAll()
{
	Merge20FilterDeckState state = ReadDeckStateFromControls();
	Merge20FilterDeckShowAll(state);
	WriteDeckStateToOptions(state);
	SyncCheckboxes();
	RefreshView();
}

Merge20FilterDeckState CMerge20FilterDeckBar::ReadDeckStateFromControls() const
{
	Merge20FilterDeckState state;
	for (int i = 0; i < 4; ++i)
		state.showStatus[i] = GetButtonCheck(m_btnStatus[i]);
	for (int i = 0; i < 7; ++i)
		state.showAction[i] = GetButtonCheck(m_btnAction[i]);
	for (int i = 0; i < 5; ++i)
		state.showReview[i] = GetButtonCheck(m_btnReview[i]);
	return state;
}

Merge20FilterDeckState CMerge20FilterDeckBar::ReadDeckStateFromOptions() const
{
	Merge20FilterDeckState state;
	for (int i = 0; i < 4; ++i)
		state.showStatus[i] = GetOptionsMgr()->GetBool(Merge20DeckStatusKey(i));
	for (int i = 0; i < 7; ++i)
		state.showAction[i] = GetOptionsMgr()->GetBool(Merge20DeckActionKey(i));
	for (int i = 0; i < 5; ++i)
		state.showReview[i] = GetOptionsMgr()->GetBool(Merge20DeckReviewKey(i));
	return state;
}

void CMerge20FilterDeckBar::WriteDeckStateToOptions(const Merge20FilterDeckState& state) const
{
	for (int i = 0; i < 4; ++i)
		GetOptionsMgr()->SaveOption(Merge20DeckStatusKey(i), state.showStatus[i]);
	for (int i = 0; i < 7; ++i)
		GetOptionsMgr()->SaveOption(Merge20DeckActionKey(i), state.showAction[i]);
	for (int i = 0; i < 5; ++i)
		GetOptionsMgr()->SaveOption(Merge20DeckReviewKey(i), state.showReview[i]);
}

void CMerge20FilterDeckBar::RefreshView()
{
	CFrameWnd* pFrame = GetParentFrame();
	if (pFrame == nullptr)
		return;
	if (CDirView* pView = dynamic_cast<CDirView*>(pFrame->GetActiveView()))
		pView->Merge20RefreshFilterDeck();
}

void CMerge20FilterDeckBar::SetButtonCheck(CButton& button, bool checked)
{
	if (button.GetSafeHwnd() != nullptr)
		button.SetCheck(checked ? BST_CHECKED : BST_UNCHECKED);
}

bool CMerge20FilterDeckBar::GetButtonCheck(const CButton& button)
{
	if (button.GetSafeHwnd() == nullptr)
		return false;
	return button.GetCheck() == BST_CHECKED;
}
