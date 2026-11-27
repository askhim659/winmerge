// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20SummaryDlg.cpp
 *
 * @brief Implementation of the end-of-session summary dialog (B9).
 *
 * The two count lines and the warning are set in OnInitDialog from
 * the counts the caller computed; the list is filled from the rows
 * the caller built (same display strings as the folder list and the
 * S6 CSV). The "show unreviewed only" button is disabled when
 * nothing is unreviewed, and "confirm and export" is disabled when
 * no file is an output target, so neither choice can run on an
 * empty set (as in the S6 range dialog, which disables its empty
 * range). Back (IDCANCEL, including the close button) leaves the
 * result at Back, so the caller changes nothing.
 */
#include "StdAfx.h"
#include "Merge20SummaryDlg.h"

IMPLEMENT_DYNAMIC(Merge20SummaryDlg, CTrDialog)

Merge20SummaryDlg::Merge20SummaryDlg(CWnd* pParent, const std::vector<Merge20SummaryRow>& rows, const Merge20SummaryCounts& counts)
	: CTrDialog(Merge20SummaryDlg::IDD, pParent)
	, m_rows(rows)
	, m_counts(counts)
{
}

void Merge20SummaryDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MERGE20_SUMMARY_LIST, m_list);
}

BEGIN_MESSAGE_MAP(Merge20SummaryDlg, CTrDialog)
	ON_BN_CLICKED(IDC_MERGE20_SUMMARY_TODO_ONLY, OnTodoOnly)
	ON_BN_CLICKED(IDC_MERGE20_SUMMARY_CONFIRM, OnConfirm)
END_MESSAGE_MAP()

BOOL Merge20SummaryDlg::OnInitDialog()
{
	__super::OnInitDialog();

	// H-DASH counts (gap analysis section 4.2 B9): the seven kinds
	// on the first line, unreviewed and output targets on the second.
	const String total = strutils::to_str(m_counts.total);
	const String diffCount = strutils::to_str(m_counts.diffCount);
	const String modified = strutils::to_str(m_counts.modified);
	const String oldOnly = strutils::to_str(m_counts.oldOnly);
	const String newOnly = strutils::to_str(m_counts.newOnly);
	const String identical = strutils::to_str(m_counts.identical);
	const String special = strutils::to_str(m_counts.special);
	const String* countArgs[] = { &total, &diffCount, &modified, &oldOnly, &newOnly, &identical, &special };
	SetDlgItemText(IDC_MERGE20_SUMMARY_COUNTS,
		strutils::format_strings(_("Total: %1 | Diffs: %2 | Changed: %3 | Old only: %4 | New only: %5 | Identical: %6 | Needs manual work: %7"), countArgs, 7).c_str());
	const String undecided = strutils::to_str(m_counts.undecided);
	const String outputTarget = strutils::to_str(m_counts.outputTarget);
	SetDlgItemText(IDC_MERGE20_SUMMARY_SUBCOUNTS,
		strutils::format_string2(_("Undecided: %1 | Output targets: %2"), undecided, outputTarget).c_str());

	// Warning when unreviewed files remain (acceptance (c)); the
	// line stays hidden once everything is reviewed.
	if (Merge20SummaryHasUndecided(m_counts))
	{
		SetDlgItemText(IDC_MERGE20_SUMMARY_WARNING,
			strutils::format_string1(_("Warning: %1 unreviewed file(s) remain. Review them before confirming."), undecided).c_str());
	}
	else
	{
		GetDlgItem(IDC_MERGE20_SUMMARY_WARNING)->ShowWindow(SW_HIDE);
	}

	// One line per file (acceptance (a)): the six columns of gap
	// analysis section 4.2 B9, with the same headers as the folder
	// compare list and the S6 CSV.
	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list.InsertColumn(0, _("File name").c_str(), LVCFMT_LEFT, 130);
	m_list.InsertColumn(1, _("Review State").c_str(), LVCFMT_LEFT, 65);
	m_list.InsertColumn(2, _("Action Category").c_str(), LVCFMT_LEFT, 85);
	m_list.InsertColumn(3, _("Risk").c_str(), LVCFMT_CENTER, 45);
	m_list.InsertColumn(4, _("Include in Output").c_str(), LVCFMT_CENTER, 50);
	m_list.InsertColumn(5, _("Summary").c_str(), LVCFMT_LEFT, 131);
	for (size_t i = 0; i < m_rows.size(); ++i)
	{
		const Merge20SummaryRow& row = m_rows[i];
		const int item = m_list.InsertItem(static_cast<int>(i), row.fileName.c_str());
		m_list.SetItemText(item, 1, row.review.c_str());
		m_list.SetItemText(item, 2, row.action.c_str());
		m_list.SetItemText(item, 3, row.risk.c_str());
		m_list.SetItemText(item, 4, row.output.c_str());
		m_list.SetItemText(item, 5, row.summary.c_str());
	}

	// Neither choice can run on an empty set.
	GetDlgItem(IDC_MERGE20_SUMMARY_TODO_ONLY)->EnableWindow(m_counts.undecided > 0);
	GetDlgItem(IDC_MERGE20_SUMMARY_CONFIRM)->EnableWindow(m_counts.outputTarget > 0);
	return TRUE;
}

void Merge20SummaryDlg::OnTodoOnly()
{
	m_result = Result::TodoOnly;
	EndDialog(IDOK);
}

void Merge20SummaryDlg::OnConfirm()
{
	m_result = Result::Confirm;
	EndDialog(IDOK);
}
