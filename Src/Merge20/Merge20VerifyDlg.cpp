// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20VerifyDlg.cpp
 *
 * @brief Implementation of the B7 problem-list dialog.
 *
 * The list is filled in OnInitDialog from the rows the caller
 * built (same list-control pattern as the B9 summary dialog:
 * full-row select, grid lines, one row per problem). The check
 * names, locations and details are composed by the shared gate
 * helpers (Merge20VerifyCheckDisplayName /
 * Merge20VerifyFormatIssueLocation / Merge20VerifyFormatIssueDetail)
 * from the engine's structured issues with `_()` format strings,
 * so the Japanese translation stays in step with the dialog (see
 * Merge20VerifyDlg.h for the button contract) and the B8 report
 * uses the same words.
 */
#include "StdAfx.h"
#include <set>
#include "Merge20VerifyDlg.h"
#include "Merge20VerifyGate.h"

IMPLEMENT_DYNAMIC(Merge20VerifyDlg, CTrDialog)

Merge20VerifyDlg::Merge20VerifyDlg(CWnd* pParent, const std::vector<Merge20VerifyIssueRow>& rows)
	: CTrDialog(Merge20VerifyDlg::IDD, pParent)
	, m_rows(rows)
{
}

void Merge20VerifyDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MERGE20_VERIFY_LIST, m_list);
}

BEGIN_MESSAGE_MAP(Merge20VerifyDlg, CTrDialog)
	ON_BN_CLICKED(IDC_MERGE20_VERIFY_JUMP, OnJump)
	ON_BN_CLICKED(IDC_MERGE20_VERIFY_PROCEED, OnProceed)
	ON_NOTIFY(NM_DBLCLK, IDC_MERGE20_VERIFY_LIST, OnDblclkList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_MERGE20_VERIFY_LIST, OnItemchangedList)
END_MESSAGE_MAP()

String Merge20VerifyDlg::FormatLocation(const Merge20VerifyIssue& issue) const
{
	// B8: shared with the verification report (same words).
	return Merge20VerifyFormatIssueLocation(issue);
}

String Merge20VerifyDlg::FormatDetail(const Merge20VerifyIssue& issue) const
{
	// B8: shared with the verification report (same words).
	return Merge20VerifyFormatIssueDetail(issue);
}

BOOL Merge20VerifyDlg::OnInitDialog()
{
	__super::OnInitDialog();

	// Summary: how many problems, in how many files.
	std::set<int> files;
	for (const Merge20VerifyIssueRow& row : m_rows)
		files.insert(row.fileIndex);
	const String problemCount = strutils::to_str(static_cast<int>(m_rows.size()));
	const String fileCount = strutils::to_str(static_cast<int>(files.size()));
	const String* summaryArgs[] = { &problemCount, &fileCount };
	SetDlgItemText(IDC_MERGE20_VERIFY_SUMMARY,
		strutils::format_strings(_("Found %1 problem(s) in %2 file(s). Output is stopped."), summaryArgs, 2).c_str());

	// The simple-check note: shown only when a syntax issue is
	// present, so the limitation is visible exactly where it
	// matters (acceptance: the simple check must not be mistaken
	// for a syntax guarantee).
	bool hasSyntax = false;
	for (const Merge20VerifyIssueRow& row : m_rows)
	{
		if (row.issue.check == Merge20VerifyCheck::Syntax)
		{
			hasSyntax = true;
			break;
		}
	}
	if (hasSyntax)
	{
		SetDlgItemText(IDC_MERGE20_VERIFY_NOTE,
			_("Note: the syntax check only checks bracket pairing (a simple check, not a full syntax guarantee)."));
	}
	else
	{
		GetDlgItem(IDC_MERGE20_VERIFY_NOTE)->ShowWindow(SW_HIDE);
	}

	// One row per problem (acceptance (c): the file and the
	// location are visible at a glance, and Jump goes there).
	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list.InsertColumn(0, _("Check").c_str(), LVCFMT_LEFT, 110);
	m_list.InsertColumn(1, _("File name").c_str(), LVCFMT_LEFT, 130);
	m_list.InsertColumn(2, _("Location").c_str(), LVCFMT_LEFT, 110);
	m_list.InsertColumn(3, _("Detail").c_str(), LVCFMT_LEFT, 290);
	for (size_t i = 0; i < m_rows.size(); ++i)
	{
		const Merge20VerifyIssueRow& row = m_rows[i];
		const String checkName = Merge20VerifyCheckDisplayName(row.issue.check);
		const int item = m_list.InsertItem(static_cast<int>(i), checkName.c_str());
		m_list.SetItemText(item, 1, row.fileName.c_str());
		m_list.SetItemText(item, 2, FormatLocation(row.issue).c_str());
		m_list.SetItemText(item, 3, FormatDetail(row.issue).c_str());
	}
	if (!m_rows.empty())
	{
		m_list.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
		m_list.EnsureVisible(0, FALSE);
	}
	UpdateJumpButton();
	return TRUE;
}

void Merge20VerifyDlg::UpdateJumpButton()
{
	CWnd* pJump = GetDlgItem(IDC_MERGE20_VERIFY_JUMP);
	if (pJump == nullptr)
		return;
	const int selected = m_list.GetNextItem(-1, LVNI_SELECTED);
	pJump->EnableWindow(selected >= 0);
}

void Merge20VerifyDlg::OnJump()
{
	const int selected = m_list.GetNextItem(-1, LVNI_SELECTED);
	if (selected < 0 || selected >= static_cast<int>(m_rows.size()))
		return;
	m_jumpRow = selected;
	m_result = Result::Jump;
	EndDialog(IDOK);
}

void Merge20VerifyDlg::OnProceed()
{
	m_result = Result::Proceed;
	EndDialog(IDOK);
}

void Merge20VerifyDlg::OnDblclkList(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	*pResult = 0;
	OnJump();
}

void Merge20VerifyDlg::OnItemchangedList(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	*pResult = 0;
	UpdateJumpButton();
}
