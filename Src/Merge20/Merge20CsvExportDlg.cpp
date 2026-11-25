// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20CsvExportDlg.cpp
 *
 * @brief Implementation of the Merge20 CSV range-selection dialog (S6).
 */
#include "StdAfx.h"
#include "Merge20CsvExportDlg.h"

IMPLEMENT_DYNAMIC(Merge20CsvExportDlg, CTrDialog)

Merge20CsvExportDlg::Merge20CsvExportDlg(CWnd* pParent, int selectedOnlyCount, int allDiffsCount)
	: CTrDialog(Merge20CsvExportDlg::IDD, pParent)
	, m_selectedOnlyCount(selectedOnlyCount)
	, m_allDiffsCount(allDiffsCount)
{
}

void Merge20CsvExportDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(Merge20CsvExportDlg, CTrDialog)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MERGE20_CSV_SELECTED, IDC_MERGE20_CSV_ALL, OnRangeClicked)
END_MESSAGE_MAP()

BOOL Merge20CsvExportDlg::OnInitDialog()
{
	__super::OnInitDialog();

	// Both ranges show their file count before anything runs (as in
	// the S3 batch-check dialog). The "selected only" radio is
	// disabled when no file has its output checkbox on, so the dialog
	// cannot run an empty export that way.
	SetDlgItemText(IDC_MERGE20_CSV_SELECTED, strutils::format_string1(_("Checked files only (%1 files)"), strutils::to_str(m_selectedOnlyCount)));
	SetDlgItemText(IDC_MERGE20_CSV_ALL, strutils::format_string1(_("All files with differences (%1 files)"), strutils::to_str(m_allDiffsCount)));
	if (m_selectedOnlyCount == 0)
	{
		GetDlgItem(IDC_MERGE20_CSV_SELECTED)->EnableWindow(FALSE);
		m_range = Range::AllDiffs;
	}
	else if (m_allDiffsCount == 0)
	{
		m_range = Range::SelectedOnly;
	}
	CheckDlgButton(m_range == Range::SelectedOnly ? IDC_MERGE20_CSV_SELECTED : IDC_MERGE20_CSV_ALL, BST_CHECKED);
	return TRUE;
}

void Merge20CsvExportDlg::OnRangeClicked(UINT nID)
{
	m_range = (nID == IDC_MERGE20_CSV_SELECTED) ? Range::SelectedOnly : Range::AllDiffs;
}
