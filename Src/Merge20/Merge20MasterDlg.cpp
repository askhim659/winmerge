// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20MasterDlg.cpp
 *
 * @brief Implementation of the master management dialog (Merge20
 * Phase 4 D8, design sections 4.4 and 5.2/D8, spec sections
 * 3 (4/4) and 6).
 *
 * The dialog follows the Grep dialog (D7) for the mechanics:
 * controls are created in code in OnInitDialog (the template is
 * empty), LayoutAll() runs on resize, and the list is a report
 * list control. It is modal (the learning dialog precedent):
 * the working copy is written to the live stores only on Save.
 * The sharing-notice confirmation before export is the L5
 * precedent (Merge20LearnDlg::AskExportConfirm), worded for
 * master data: the patterns can contain source-code fragments.
 *
 * The editor is shared by the three masters: the pattern edit
 * holds an exclude pattern, a highlight pattern or a risk
 * keyword; the name edit and the regex / ignore-case check
 * boxes apply only where the master has that field (they are
 * disabled otherwise). Risk keywords are kept as a list in the
 * dialog and joined with ';' on save (Merge20ParseRiskKeywords
 * is the S4 parser for the same form).
 */
#include "StdAfx.h"
#include "Merge20/Merge20MasterDlg.h"
#include "Merge20/Merge20MasterDoc.h"
#include "Merge20/Merge20Classify.h"
#include "Merge20/Merge20Session.h"
#include "FileOrFolderSelect.h"
#include "UnicodeString.h"
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20MasterDlg, CTrDialog)

BEGIN_MESSAGE_MAP(CMerge20MasterDlg, CTrDialog)
	//{{AFX_MSG_MAP(CMerge20MasterDlg)
	ON_WM_SIZE()
	ON_CBN_SELCHANGE(IDC_MERGE20_MASTER_SELECT, OnMasterChanged)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_MERGE20_MASTER_LIST, OnSelChangeList)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MERGE20_MASTER_BTN_ADD, IDC_MERGE20_MASTER_BTN_IMPORT, OnButtonClicked)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

namespace
{
/** @brief Margins and row heights in pixels (the Grep dialog's layout idiom). */
constexpr int MasterMargin = 8;
constexpr int MasterRowGap = 6;
constexpr int MasterTopRowHeight = 24;
constexpr int MasterEditRowHeight = 24;
constexpr int MasterCheckRowHeight = 20;
constexpr int MasterButtonRowHeight = 24;
constexpr int MasterButtonWidth = 76;
constexpr int MasterWideButtonWidth = 84;
constexpr int MasterHintHeight = 30;
/** @brief Checked mark in the list (the S2 output column uses ballot-box characters). */
constexpr wchar_t MasterCheckedMark[] = L"\x2713";

/** @brief Trim leading/trailing spaces and tabs (no locale involved). */
String MasterTrim(const String& s)
{
	const size_t first = s.find_first_not_of(_T(" \t"));
	if (first == String::npos)
		return String();
	const size_t last = s.find_last_not_of(_T(" \t"));
	return s.substr(first, last - first + 1);
}

/** @brief True when two exclude-rule lists hold the same rules in the same order. */
bool MasterExcludeRulesEqual(const std::vector<Merge20ExcludeRule>& a, const std::vector<Merge20ExcludeRule>& b)
{
	if (a.size() != b.size())
		return false;
	for (size_t i = 0; i < a.size(); ++i)
	{
		if (a[i].pattern != b[i].pattern || a[i].useRegExp != b[i].useRegExp ||
			a[i].ignoreCase != b[i].ignoreCase || a[i].enabled != b[i].enabled)
			return false;
	}
	return true;
}

/** @brief Decode UTF-8 bytes into a wide string (the L1 helper idiom). */
String MasterUtf8ToString(const std::string& utf8)
{
	String out;
	Merge20Utf8Decode(utf8, out);
	return out;
}

/** @brief Join risk keywords with ';' (the OPT_MERGE20_RISK_KEYWORDS form). */
String MasterJoinRiskKeywords(const std::vector<String>& keywords)
{
	String joined;
	for (const String& keyword : keywords)
	{
		if (!joined.empty())
			joined += _T(";");
		joined += keyword;
	}
	return joined;
}
}

CMerge20MasterDlg::CMerge20MasterDlg()
	: CTrDialog(IDD_MERGE20_MASTER)
	, m_nMaster(MASTER_EXCLUDE)
{
}

bool CMerge20MasterDlg::ExcludeChanged() const
{
	return m_masters.excludeIgnoreL2 != m_originalIgnoreL2 ||
		!MasterExcludeRulesEqual(m_masters.excludeRules, m_originalExcludeRules);
}

BOOL CMerge20MasterDlg::OnInitDialog()
{
	__super::OnInitDialog();

	SetWindowText(_("Master Management").c_str());

	String loadError;
	(void)Merge20MasterLoadStore(m_masters, loadError);
	m_originalExcludeRules = m_masters.excludeRules;
	m_originalIgnoreL2 = m_masters.excludeIgnoreL2;
	m_riskKeywords = Merge20ParseRiskKeywords(m_masters.riskKeywords);
	if (!loadError.empty())
		AfxMessageBox(loadError.c_str(), MB_ICONSTOP);

	const DWORD dwLabelStyle = WS_CHILD | WS_VISIBLE | SS_LEFT;
	m_stLabelMaster.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);
	m_stLabelMaster.SetWindowText(_("Master:").c_str());

	const DWORD dwComboStyle = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL;
	if (!m_cbMaster.Create(dwComboStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_SELECT))
		return FALSE;
	m_cbMaster.AddString(_("Exclude Words Master").c_str());
	m_cbMaster.AddString(_("Highlight Master").c_str());
	m_cbMaster.AddString(_("Risk Keywords").c_str());
	m_cbMaster.SetCurSel(MASTER_EXCLUDE);

	const DWORD dwListStyle = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
	if (!m_list.Create(dwListStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_LIST))
		return FALSE;
	m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	m_stLabelPattern.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);
	m_stLabelPattern.SetWindowText(_("Pattern").c_str());
	m_stLabelName.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);
	m_stLabelName.SetWindowText(_("Display Name").c_str());

	const DWORD dwEditStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
	if (!m_edPattern.Create(dwEditStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_PATTERN))
		return FALSE;
	if (!m_edName.Create(dwEditStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_NAME))
		return FALSE;

	const DWORD dwCheckStyle = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX;
	m_chkRegex.Create(_T(""), dwCheckStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_REGEX);
	m_chkRegex.SetWindowText(_("Regular expression").c_str());
	m_chkIgnoreCase.Create(_T(""), dwCheckStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_IGNORECASE);
	m_chkIgnoreCase.SetWindowText(_("Ignore case").c_str());
	m_chkEnabled.Create(_T(""), dwCheckStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_ENABLED);
	m_chkEnabled.SetWindowText(_("Enabled").c_str());
	m_chkIgnoreL2.Create(_T(""), dwCheckStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_IGNORE_L2);
	m_chkIgnoreL2.SetWindowText(_("Ignore 2nd line version number").c_str());

	const DWORD dwBtnStyle = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
	m_btnAdd.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_BTN_ADD);
	m_btnAdd.SetWindowText(_("Add").c_str());
	m_btnUpdate.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_BTN_UPDATE);
	m_btnUpdate.SetWindowText(_("Update").c_str());
	m_btnDelete.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_BTN_DELETE);
	m_btnDelete.SetWindowText(_("Delete").c_str());
	m_btnUp.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_BTN_UP);
	m_btnUp.SetWindowText(_("Move up").c_str());
	m_btnDown.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_BTN_DOWN);
	m_btnDown.SetWindowText(_("Move down").c_str());
	m_btnCheck.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_BTN_CHECK);
	m_btnCheck.SetWindowText(_("Check regular expression").c_str());
	m_btnExport.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_BTN_EXPORT);
	m_btnExport.SetWindowText(_("Export...").c_str());
	m_btnImport.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_BTN_IMPORT);
	m_btnImport.SetWindowText(_("Import...").c_str());
	m_btnSave.Create(_T(""), dwBtnStyle | BS_DEFPUSHBUTTON, CRect(0, 0, 1, 1), this, IDOK);
	m_btnSave.SetWindowText(_("Save").c_str());
	m_btnCancel.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDCANCEL);
	m_btnCancel.SetWindowText(_("Cancel").c_str());

	m_stHint.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_MASTER_HINT);

	RefreshMaster();
	LayoutAll();
	return TRUE;
}

void CMerge20MasterDlg::OnOK()
{
	// Save applies the working copy to the live stores; a failed
	// save keeps the dialog open (spec section 7: a failed save
	// is reported, never lost silently).
	SyncRiskToMasters();
	if (m_nMaster == MASTER_EXCLUDE)
		m_masters.excludeIgnoreL2 = m_chkIgnoreL2.GetCheck() == BST_CHECKED;
	String errStr;
	if (!Merge20MasterSaveStore(m_masters, errStr))
	{
		AfxMessageBox(errStr.c_str(), MB_ICONSTOP);
		return;
	}
	CTrDialog::OnOK();
}

void CMerge20MasterDlg::OnSize(UINT nType, int cx, int cy)
{
	CTrDialog::OnSize(nType, cx, cy);
	LayoutAll();
}

void CMerge20MasterDlg::OnMasterChanged()
{
	// The IgnoreL2 check box is only committed into the working
	// copy here and on Save/Add/Update; commit it before leaving
	// the exclude master so the change is not lost on switch.
	if (m_nMaster == MASTER_EXCLUDE)
		m_masters.excludeIgnoreL2 = m_chkIgnoreL2.GetCheck() == BST_CHECKED;
	const int sel = m_cbMaster.GetCurSel();
	m_nMaster = (sel < 0) ? MASTER_EXCLUDE : sel;
	RefreshMaster();
	LayoutAll();
}

void CMerge20MasterDlg::OnSelChangeList(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	const NM_LISTVIEW* pNM = reinterpret_cast<NM_LISTVIEW*>(pNMHDR);
	if ((pNM->uChanged & LVIF_STATE) != 0 && (pNM->uNewState & LVIS_SELECTED) != 0)
		RefreshEditorFromSelection();
}

void CMerge20MasterDlg::OnButtonClicked(UINT nID)
{
	switch (nID)
	{
	case IDC_MERGE20_MASTER_BTN_ADD:
		AddRule();
		break;
	case IDC_MERGE20_MASTER_BTN_UPDATE:
		UpdateRule();
		break;
	case IDC_MERGE20_MASTER_BTN_DELETE:
		DeleteRule();
		break;
	case IDC_MERGE20_MASTER_BTN_UP:
		MoveRule(-1);
		break;
	case IDC_MERGE20_MASTER_BTN_DOWN:
		MoveRule(1);
		break;
	case IDC_MERGE20_MASTER_BTN_CHECK:
		CheckRegex();
		break;
	case IDC_MERGE20_MASTER_BTN_EXPORT:
		ExportMasters();
		break;
	case IDC_MERGE20_MASTER_BTN_IMPORT:
		ImportMasters();
		break;
	}
}

void CMerge20MasterDlg::LayoutAll()
{
	if (m_hWnd == nullptr || m_list.GetSafeHwnd() == nullptr)
		return;
	CRect rc;
	GetClientRect(&rc);
	const int cx = rc.Width();
	const int cy = rc.Height();
	if (cx < 400 || cy < 300)
		return;

	// Rows from the bottom up: Save/Cancel row, hint, action
	// buttons, check boxes, name edit, pattern edit; the list
	// takes the remaining height.
	const int yBottom = cy - MasterMargin - MasterButtonRowHeight;
	const int yHint = yBottom - MasterRowGap - MasterHintHeight;
	const int yButtons = yHint - MasterRowGap - MasterButtonRowHeight;
	const int yChecks = yButtons - MasterRowGap - MasterCheckRowHeight;
	const int yName = yChecks - MasterRowGap - MasterEditRowHeight;
	const int yPattern = yName - MasterRowGap - MasterEditRowHeight;
	const int yList = MasterMargin + MasterTopRowHeight + MasterRowGap;
	const int listBottom = yPattern - MasterRowGap;

	m_stLabelMaster.MoveWindow(MasterMargin, MasterMargin + 4, 52, 16, TRUE);
	m_cbMaster.MoveWindow(MasterMargin + 58, MasterMargin, 220, 160, TRUE);
	m_list.MoveWindow(MasterMargin, yList, cx - 2 * MasterMargin, (std::max)(40, listBottom - yList), TRUE);

	m_stLabelPattern.MoveWindow(MasterMargin, yPattern + 4, 70, 16, TRUE);
	m_edPattern.MoveWindow(MasterMargin + 76, yPattern, cx - MasterMargin - (MasterMargin + 76), MasterEditRowHeight, TRUE);
	m_stLabelName.MoveWindow(MasterMargin, yName + 4, 70, 16, TRUE);
	m_edName.MoveWindow(MasterMargin + 76, yName, cx - MasterMargin - (MasterMargin + 76), MasterEditRowHeight, TRUE);

	m_chkRegex.MoveWindow(MasterMargin, yChecks, 110, MasterCheckRowHeight, TRUE);
	m_chkIgnoreCase.MoveWindow(MasterMargin + 116, yChecks, 190, MasterCheckRowHeight, TRUE);
	m_chkEnabled.MoveWindow(MasterMargin + 312, yChecks, 80, MasterCheckRowHeight, TRUE);
	m_chkIgnoreL2.MoveWindow(MasterMargin + 398, yChecks, (std::max)(60, cx - MasterMargin - (MasterMargin + 398)), MasterCheckRowHeight, TRUE);

	int xBtn = MasterMargin;
	m_btnAdd.MoveWindow(xBtn, yButtons, MasterButtonWidth, MasterButtonRowHeight, TRUE);
	xBtn += MasterButtonWidth + MasterRowGap;
	m_btnUpdate.MoveWindow(xBtn, yButtons, MasterButtonWidth, MasterButtonRowHeight, TRUE);
	xBtn += MasterButtonWidth + MasterRowGap;
	m_btnDelete.MoveWindow(xBtn, yButtons, MasterButtonWidth, MasterButtonRowHeight, TRUE);
	xBtn += MasterButtonWidth + MasterRowGap;
	m_btnUp.MoveWindow(xBtn, yButtons, MasterButtonWidth, MasterButtonRowHeight, TRUE);
	xBtn += MasterButtonWidth + MasterRowGap;
	m_btnDown.MoveWindow(xBtn, yButtons, MasterButtonWidth, MasterButtonRowHeight, TRUE);
	xBtn += MasterButtonWidth + MasterRowGap;
	m_btnCheck.MoveWindow(xBtn, yButtons, 150, MasterButtonRowHeight, TRUE);

	m_stHint.MoveWindow(MasterMargin, yHint, cx - 2 * MasterMargin, MasterHintHeight, TRUE);

	int xWide = cx - MasterMargin - MasterWideButtonWidth;
	m_btnCancel.MoveWindow(xWide, yBottom, MasterWideButtonWidth, MasterButtonRowHeight, TRUE);
	xWide -= MasterWideButtonWidth + MasterRowGap;
	m_btnSave.MoveWindow(xWide, yBottom, MasterWideButtonWidth, MasterButtonRowHeight, TRUE);
	xWide -= MasterWideButtonWidth + MasterRowGap;
	m_btnImport.MoveWindow(xWide, yBottom, MasterWideButtonWidth, MasterButtonRowHeight, TRUE);
	xWide -= MasterWideButtonWidth + MasterRowGap;
	m_btnExport.MoveWindow(xWide, yBottom, MasterWideButtonWidth, MasterButtonRowHeight, TRUE);

	// The pattern column takes the remaining list width.
	CRect rcList;
	m_list.GetClientRect(&rcList);
	const int listWidth = rcList.Width();
	if (m_nMaster == MASTER_RISK)
	{
		m_list.SetColumnWidth(0, (std::max)(80, listWidth - 4));
	}
	else if (m_nMaster == MASTER_HIGHLIGHT)
	{
		m_list.SetColumnWidth(0, 60);
		m_list.SetColumnWidth(2, 150);
		m_list.SetColumnWidth(3, 110);
		m_list.SetColumnWidth(1, (std::max)(80, listWidth - 60 - 150 - 110 - 4));
	}
	else
	{
		m_list.SetColumnWidth(0, 60);
		m_list.SetColumnWidth(2, 110);
		m_list.SetColumnWidth(3, 130);
		m_list.SetColumnWidth(1, (std::max)(80, listWidth - 60 - 110 - 130 - 4));
	}
}

void CMerge20MasterDlg::RefreshMaster()
{
	RefreshList();
	RefreshEditor();
	RefreshEditorFromSelection();
	m_stHint.SetWindowText(HintText().c_str());
}

void CMerge20MasterDlg::RefreshList()
{
	m_list.DeleteAllItems();
	while (m_list.DeleteColumn(0))
	{
	}
	if (m_nMaster == MASTER_RISK)
	{
		m_list.InsertColumn(0, _("Risk Keyword").c_str(), LVCFMT_LEFT, 200);
		for (size_t i = 0; i < m_riskKeywords.size(); ++i)
			m_list.InsertItem(static_cast<int>(i), m_riskKeywords[i].c_str());
		return;
	}
	m_list.InsertColumn(0, _("Enabled").c_str(), LVCFMT_LEFT, 60);
	m_list.InsertColumn(1, _("Pattern").c_str(), LVCFMT_LEFT, 200);
	if (m_nMaster == MASTER_HIGHLIGHT)
	{
		m_list.InsertColumn(2, _("Display Name").c_str(), LVCFMT_LEFT, 150);
		m_list.InsertColumn(3, _("Kind").c_str(), LVCFMT_LEFT, 110);
		for (size_t i = 0; i < m_masters.highlightRules.size(); ++i)
		{
			const Merge20HighlightRule& rule = m_masters.highlightRules[i];
			const int row = static_cast<int>(i);
			m_list.InsertItem(row, rule.enabled ? MasterCheckedMark : _T(""));
			m_list.SetItemText(row, 1, rule.pattern.c_str());
			m_list.SetItemText(row, 2, rule.name.c_str());
			m_list.SetItemText(row, 3, rule.useRegExp ? _("Regular expression").c_str() : _("Plain match").c_str());
		}
		return;
	}
	m_list.InsertColumn(2, _("Kind").c_str(), LVCFMT_LEFT, 110);
	m_list.InsertColumn(3, _("Ignore case").c_str(), LVCFMT_LEFT, 130);
	for (size_t i = 0; i < m_masters.excludeRules.size(); ++i)
	{
		const Merge20ExcludeRule& rule = m_masters.excludeRules[i];
		const int row = static_cast<int>(i);
		m_list.InsertItem(row, rule.enabled ? MasterCheckedMark : _T(""));
		m_list.SetItemText(row, 1, rule.pattern.c_str());
		m_list.SetItemText(row, 2, rule.useRegExp ? _("Regular expression").c_str() : _("Plain match").c_str());
		m_list.SetItemText(row, 3, rule.ignoreCase ? MasterCheckedMark : _T(""));
	}
}

void CMerge20MasterDlg::RefreshEditor()
{
	const bool isExclude = (m_nMaster == MASTER_EXCLUDE);
	const bool isHighlight = (m_nMaster == MASTER_HIGHLIGHT);
	m_edName.EnableWindow(isHighlight ? TRUE : FALSE);
	m_stLabelName.EnableWindow(isHighlight ? TRUE : FALSE);
	m_chkRegex.EnableWindow(isExclude || isHighlight ? TRUE : FALSE);
	m_chkIgnoreCase.EnableWindow(isExclude ? TRUE : FALSE);
	m_chkEnabled.EnableWindow(isExclude || isHighlight ? TRUE : FALSE);
	m_chkIgnoreL2.EnableWindow(isExclude ? TRUE : FALSE);
	m_chkIgnoreL2.SetCheck(m_masters.excludeIgnoreL2 ? BST_CHECKED : BST_UNCHECKED);
}

void CMerge20MasterDlg::RefreshEditorFromSelection()
{
	const int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
	m_edPattern.SetWindowText(_T(""));
	m_edName.SetWindowText(_T(""));
	m_chkRegex.SetCheck(BST_UNCHECKED);
	m_chkIgnoreCase.SetCheck(BST_UNCHECKED);
	m_chkEnabled.SetCheck(BST_CHECKED);
	if (sel < 0)
		return;
	if (m_nMaster == MASTER_RISK)
	{
		if (sel < static_cast<int>(m_riskKeywords.size()))
			m_edPattern.SetWindowText(m_riskKeywords[static_cast<size_t>(sel)].c_str());
		return;
	}
	if (m_nMaster == MASTER_HIGHLIGHT)
	{
		if (sel < static_cast<int>(m_masters.highlightRules.size()))
		{
			const Merge20HighlightRule& rule = m_masters.highlightRules[static_cast<size_t>(sel)];
			m_edPattern.SetWindowText(rule.pattern.c_str());
			m_edName.SetWindowText(rule.name.c_str());
			m_chkRegex.SetCheck(rule.useRegExp ? BST_CHECKED : BST_UNCHECKED);
			m_chkEnabled.SetCheck(rule.enabled ? BST_CHECKED : BST_UNCHECKED);
		}
		return;
	}
	if (sel < static_cast<int>(m_masters.excludeRules.size()))
	{
		const Merge20ExcludeRule& rule = m_masters.excludeRules[static_cast<size_t>(sel)];
		m_edPattern.SetWindowText(rule.pattern.c_str());
		m_chkRegex.SetCheck(rule.useRegExp ? BST_CHECKED : BST_UNCHECKED);
		m_chkIgnoreCase.SetCheck(rule.ignoreCase ? BST_CHECKED : BST_UNCHECKED);
		m_chkEnabled.SetCheck(rule.enabled ? BST_CHECKED : BST_UNCHECKED);
	}
}

bool CMerge20MasterDlg::ReadEditor(String& pattern, String& name, bool& useRegExp, bool& ignoreCase, bool& enabled) const
{
	CString sPattern;
	m_edPattern.GetWindowText(sPattern);
	pattern = MasterTrim(static_cast<LPCTSTR>(sPattern));
	CString sName;
	m_edName.GetWindowText(sName);
	name = MasterTrim(static_cast<LPCTSTR>(sName));
	useRegExp = m_chkRegex.GetCheck() == BST_CHECKED;
	ignoreCase = m_chkIgnoreCase.GetCheck() == BST_CHECKED;
	enabled = m_chkEnabled.GetCheck() == BST_CHECKED;
	return true;
}

bool CMerge20MasterDlg::ValidateEditorPattern(const String& pattern, bool useRegExp) const
{
	if (pattern.empty())
	{
		AfxMessageBox(_("Enter a pattern.").c_str(), MB_ICONSTOP);
		return false;
	}
	if (useRegExp && !Merge20MasterIsValidRegex(pattern))
	{
		AfxMessageBox(_("Invalid regular expression.").c_str(), MB_ICONSTOP);
		return false;
	}
	return true;
}

bool CMerge20MasterDlg::PatternExists(const String& pattern, int skipRow) const
{
	if (m_nMaster == MASTER_RISK)
	{
		for (size_t i = 0; i < m_riskKeywords.size(); ++i)
		{
			if (static_cast<int>(i) != skipRow && m_riskKeywords[i] == pattern)
				return true;
		}
		return false;
	}
	if (m_nMaster == MASTER_HIGHLIGHT)
	{
		for (size_t i = 0; i < m_masters.highlightRules.size(); ++i)
		{
			if (static_cast<int>(i) != skipRow && m_masters.highlightRules[i].pattern == pattern)
				return true;
		}
		return false;
	}
	for (size_t i = 0; i < m_masters.excludeRules.size(); ++i)
	{
		if (static_cast<int>(i) != skipRow && m_masters.excludeRules[i].pattern == pattern)
			return true;
	}
	return false;
}

void CMerge20MasterDlg::AddRule()
{
	String pattern, name;
	bool useRegExp = false, ignoreCase = false, enabled = true;
	(void)ReadEditor(pattern, name, useRegExp, ignoreCase, enabled);
	if (!ValidateEditorPattern(pattern, m_nMaster == MASTER_RISK ? false : useRegExp))
		return;
	if (PatternExists(pattern, -1))
	{
		AfxMessageBox(_("A rule with the same pattern already exists.").c_str(), MB_ICONSTOP);
		return;
	}
	int newRow = 0;
	if (m_nMaster == MASTER_RISK)
	{
		m_riskKeywords.push_back(pattern);
		newRow = static_cast<int>(m_riskKeywords.size()) - 1;
	}
	else if (m_nMaster == MASTER_HIGHLIGHT)
	{
		Merge20HighlightRule rule;
		rule.pattern = pattern;
		rule.name = name;
		rule.useRegExp = useRegExp;
		rule.enabled = enabled;
		m_masters.highlightRules.push_back(rule);
		newRow = static_cast<int>(m_masters.highlightRules.size()) - 1;
	}
	else
	{
		Merge20ExcludeRule rule;
		rule.pattern = pattern;
		rule.useRegExp = useRegExp;
		rule.ignoreCase = ignoreCase;
		rule.enabled = enabled;
		m_masters.excludeRules.push_back(rule);
		m_masters.excludeIgnoreL2 = m_chkIgnoreL2.GetCheck() == BST_CHECKED;  // committed here too, so a switch keeps it
		newRow = static_cast<int>(m_masters.excludeRules.size()) - 1;
	}
	RefreshList();
	m_list.SetItemState(newRow, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	m_list.EnsureVisible(newRow, FALSE);
	m_edPattern.SetWindowText(_T(""));
	m_edPattern.SetFocus();
}

void CMerge20MasterDlg::UpdateRule()
{
	const int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
	if (sel < 0)
	{
		AfxMessageBox(_("Select a rule to update or delete.").c_str(), MB_ICONSTOP);
		return;
	}
	String pattern, name;
	bool useRegExp = false, ignoreCase = false, enabled = true;
	(void)ReadEditor(pattern, name, useRegExp, ignoreCase, enabled);
	if (!ValidateEditorPattern(pattern, m_nMaster == MASTER_RISK ? false : useRegExp))
		return;
	if (PatternExists(pattern, sel))
	{
		AfxMessageBox(_("A rule with the same pattern already exists.").c_str(), MB_ICONSTOP);
		return;
	}
	if (m_nMaster == MASTER_RISK)
	{
		m_riskKeywords[static_cast<size_t>(sel)] = pattern;
	}
	else if (m_nMaster == MASTER_HIGHLIGHT)
	{
		Merge20HighlightRule& rule = m_masters.highlightRules[static_cast<size_t>(sel)];
		rule.pattern = pattern;
		rule.name = name;
		rule.useRegExp = useRegExp;
		rule.enabled = enabled;
	}
	else
	{
		Merge20ExcludeRule& rule = m_masters.excludeRules[static_cast<size_t>(sel)];
		rule.pattern = pattern;
		rule.useRegExp = useRegExp;
		rule.ignoreCase = ignoreCase;
		rule.enabled = enabled;
		m_masters.excludeIgnoreL2 = m_chkIgnoreL2.GetCheck() == BST_CHECKED;
	}
	RefreshList();
	m_list.SetItemState(sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void CMerge20MasterDlg::DeleteRule()
{
	const int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
	if (sel < 0)
	{
		AfxMessageBox(_("Select a rule to update or delete.").c_str(), MB_ICONSTOP);
		return;
	}
	if (m_nMaster == MASTER_RISK)
		m_riskKeywords.erase(m_riskKeywords.begin() + sel);
	else if (m_nMaster == MASTER_HIGHLIGHT)
		m_masters.highlightRules.erase(m_masters.highlightRules.begin() + sel);
	else
		m_masters.excludeRules.erase(m_masters.excludeRules.begin() + sel);
	RefreshList();
	RefreshEditorFromSelection();
}

void CMerge20MasterDlg::MoveRule(int delta)
{
	const int sel = m_list.GetNextItem(-1, LVNI_SELECTED);
	if (sel < 0)
	{
		AfxMessageBox(_("Select a rule to update or delete.").c_str(), MB_ICONSTOP);
		return;
	}
	const int target = sel + delta;
	if (m_nMaster == MASTER_RISK)
	{
		if (target < 0 || target >= static_cast<int>(m_riskKeywords.size()))
			return;
		std::swap(m_riskKeywords[static_cast<size_t>(sel)], m_riskKeywords[static_cast<size_t>(target)]);
	}
	else if (m_nMaster == MASTER_HIGHLIGHT)
	{
		if (target < 0 || target >= static_cast<int>(m_masters.highlightRules.size()))
			return;
		std::swap(m_masters.highlightRules[static_cast<size_t>(sel)], m_masters.highlightRules[static_cast<size_t>(target)]);
	}
	else
	{
		if (target < 0 || target >= static_cast<int>(m_masters.excludeRules.size()))
			return;
		std::swap(m_masters.excludeRules[static_cast<size_t>(sel)], m_masters.excludeRules[static_cast<size_t>(target)]);
	}
	RefreshList();
	m_list.SetItemState(target, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	m_list.EnsureVisible(target, FALSE);
}

void CMerge20MasterDlg::CheckRegex()
{
	CString sPattern;
	m_edPattern.GetWindowText(sPattern);
	const String pattern = MasterTrim(static_cast<LPCTSTR>(sPattern));
	if (pattern.empty())
	{
		AfxMessageBox(_("Enter a pattern.").c_str(), MB_ICONSTOP);
		return;
	}
	if (Merge20MasterIsValidRegex(pattern))
		AfxMessageBox(_("Regular expression is valid.").c_str(), MB_ICONINFORMATION);
	else
		AfxMessageBox(_("Invalid regular expression.").c_str(), MB_ICONSTOP);
}

void CMerge20MasterDlg::ExportMasters()
{
	SyncRiskToMasters();
	const String defaultName = Merge20MasterDefaultExportFileName();
	String filePath;
	if (!SelectFile(GetSafeHwnd(), filePath, false, defaultName.c_str(), _T(""),
			_T("JSON Files (*.json)|*.json|All Files (*.*)|*.*||"), _T("json")))
		return;
	if (!AskExportConfirm(filePath))
		return;
	std::string errDetail;
	if (!Merge20MasterExportToFile(m_masters, filePath, errDetail))
	{
		AfxMessageBox(
			strutils::format_string1(_("Could not export the master data: %1"), MasterUtf8ToString(errDetail)).c_str(),
			MB_ICONSTOP);
		return;
	}
	const String excludeCount = strutils::to_str(static_cast<int>(m_masters.excludeRules.size()));
	const String highlightCount = strutils::to_str(static_cast<int>(m_masters.highlightRules.size()));
	const String* args[] = { &excludeCount, &highlightCount, &filePath };
	AfxMessageBox(
		strutils::format_strings(_("Exported the master data (%1 exclude rules, %2 highlight rules) to:\n%3"), args, 3).c_str(),
		MB_ICONINFORMATION);
}

void CMerge20MasterDlg::ImportMasters()
{
	String filePath;
	if (!SelectFile(GetSafeHwnd(), filePath, true, nullptr, _T(""),
			_T("JSON Files (*.json)|*.json|All Files (*.*)|*.*||"), _T("json")))
		return;
	Merge20Masters imported;
	std::string errDetail;
	int skippedRules = 0;
	if (!Merge20MasterImportFromFile(filePath, imported, errDetail, skippedRules))
	{
		AfxMessageBox(
			strutils::format_string1(_("Could not import the master data: %1"), MasterUtf8ToString(errDetail)).c_str(),
			MB_ICONSTOP);
		return;
	}
	m_masters = imported;
	m_riskKeywords = Merge20ParseRiskKeywords(m_masters.riskKeywords);
	RefreshMaster();
	LayoutAll();
	const String excludeCount = strutils::to_str(static_cast<int>(m_masters.excludeRules.size()));
	const String highlightCount = strutils::to_str(static_cast<int>(m_masters.highlightRules.size()));
	const String riskCount = strutils::to_str(static_cast<int>(m_riskKeywords.size()));
	const String* args[] = { &excludeCount, &highlightCount, &riskCount };
	String msg = strutils::format_strings(_("Imported the master data (%1 exclude rules, %2 highlight rules, %3 risk keywords)"), args, 3);
	if (skippedRules > 0)
	{
		msg += _T("\n");
		msg += strutils::format_string1(_("Skipped %1 rules (invalid format)."), strutils::to_str(skippedRules));
	}
	msg += _T("\n");
	msg += _("Press Save to apply the imported masters.");
	AfxMessageBox(msg.c_str(), MB_ICONINFORMATION);
}

bool CMerge20MasterDlg::AskExportConfirm(const String& filePath)
{
	// The L5 sharing notice, worded for master data: the patterns
	// can contain source-code fragments.
	const String msg = _("Master data contains source code fragments. Share only through approved internal channels. Do not take it outside the company or save it to a personal cloud.") +
		_T("\n\n") +
		strutils::format_string1(_("Export the master data to:\n%1"), filePath);
	return AfxMessageBox(msg.c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void CMerge20MasterDlg::SyncRiskToMasters()
{
	m_masters.riskKeywords = MasterJoinRiskKeywords(m_riskKeywords);
}

String CMerge20MasterDlg::HintText() const
{
	if (m_nMaster == MASTER_RISK)
		return _("Risk keywords are separated by ';'. They are used the next time a file is classified.");
	if (m_nMaster == MASTER_HIGHLIGHT)
		return _("Highlight rules are stored in the master file. Showing them in the compare views is not implemented yet.");
	return _("In a plain (non-RegExp) pattern, % matches any text. Excluded lines are blanked out before diffing; the line count is kept.");
}
