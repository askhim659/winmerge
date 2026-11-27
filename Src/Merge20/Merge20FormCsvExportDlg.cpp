// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCsvExportDlg.cpp
 *
 * @brief Implementation of the FORM compare CSV range-selection
 * dialog (Wave 5 F7, S6 `Merge20CsvExportDlg` precedent).
 */
#include "StdAfx.h"
#include "Merge20FormCsvExportDlg.h"

IMPLEMENT_DYNAMIC(CMerge20FormCsvExportDlg, CTrDialog)

CMerge20FormCsvExportDlg::CMerge20FormCsvExportDlg(CWnd* pParent, int filteredOnlyCount, int allChangedCount)
	: CTrDialog(CMerge20FormCsvExportDlg::IDD, pParent)
	, m_filteredOnlyCount(filteredOnlyCount)
	, m_allChangedCount(allChangedCount)
{
}

void CMerge20FormCsvExportDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMerge20FormCsvExportDlg, CTrDialog)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MERGE20_FORM_CSV_FILTERED, IDC_MERGE20_FORM_CSV_ALL, OnRangeClicked)
END_MESSAGE_MAP()

BOOL CMerge20FormCsvExportDlg::OnInitDialog()
{
	__super::OnInitDialog();

	// Both ranges show their file count before anything runs (as in
	// the S3 batch-check dialog). The "filtered only" radio is
	// disabled when the filter shows nothing, so the dialog cannot
	// run an empty export that way.
	SetDlgItemText(IDC_MERGE20_FORM_CSV_FILTERED, strutils::format_string1(_("Filtered results only (%1 files)"), strutils::to_str(m_filteredOnlyCount)));
	SetDlgItemText(IDC_MERGE20_FORM_CSV_ALL, strutils::format_string1(_("All changed files (%1 files)"), strutils::to_str(m_allChangedCount)));
	if (m_filteredOnlyCount == 0)
	{
		GetDlgItem(IDC_MERGE20_FORM_CSV_FILTERED)->EnableWindow(FALSE);
		m_range = Range::AllChanged;
	}
	else if (m_allChangedCount == 0)
	{
		m_range = Range::FilteredOnly;
	}
	CheckDlgButton(m_range == Range::FilteredOnly ? IDC_MERGE20_FORM_CSV_FILTERED : IDC_MERGE20_FORM_CSV_ALL, BST_CHECKED);
	return TRUE;
}

void CMerge20FormCsvExportDlg::OnRangeClicked(UINT nID)
{
	m_range = (nID == IDC_MERGE20_FORM_CSV_FILTERED) ? Range::FilteredOnly : Range::AllChanged;
}
