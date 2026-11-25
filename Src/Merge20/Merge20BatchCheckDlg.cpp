// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20BatchCheckDlg.cpp
 *
 * @brief Implementation of the Merge20 batch-check dialog (S3).
 */
#include "StdAfx.h"
#include "Merge20BatchCheckDlg.h"

IMPLEMENT_DYNAMIC(Merge20BatchCheckDlg, CTrDialog)

namespace
{
	/** @brief Check-box control IDs, in ReviewState order (TODO..KEEP). */
	constexpr int Merge20BatchCheckIDs[5] =
	{
		IDC_MERGE20_BATCH_TODO, IDC_MERGE20_BATCH_DONE, IDC_MERGE20_BATCH_HOLD,
		IDC_MERGE20_BATCH_SPECIAL, IDC_MERGE20_BATCH_KEEP
	};
}

Merge20BatchCheckDlg::Merge20BatchCheckDlg(CWnd* pParent, const int countsByState[5])
	: CTrDialog(Merge20BatchCheckDlg::IDD, pParent)
{
	for (int i = 0; i < 5; ++i)
		m_countsByState[i] = countsByState[i];
}

void Merge20BatchCheckDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(Merge20BatchCheckDlg, CTrDialog)
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MERGE20_BATCH_TODO, IDC_MERGE20_BATCH_KEEP, OnReviewStateClicked)
	ON_BN_CLICKED(IDC_MERGE20_BATCH_CHECK_ALL, OnCheckAllDisplayed)
	ON_BN_CLICKED(IDC_MERGE20_BATCH_CLEAR_ALL, OnClearAll)
END_MESSAGE_MAP()

BOOL Merge20BatchCheckDlg::OnInitDialog()
{
	__super::OnInitDialog();

	// Start with nothing selected, as in the HTML popup (selRev starts empty).
	for (int i = 0; i < 5; ++i)
		CheckDlgButton(Merge20BatchCheckIDs[i], BST_UNCHECKED);
	UpdateCount();
	return TRUE;
}

void Merge20BatchCheckDlg::OnReviewStateClicked(UINT nID)
{
	const int idx = static_cast<int>(nID) - IDC_MERGE20_BATCH_TODO;
	if (idx >= 0 && idx < 5)
		m_selected[idx] = (IsDlgButtonChecked(nID) == BST_CHECKED);
	UpdateCount();
}

void Merge20BatchCheckDlg::UpdateCount()
{
	int total = 0;
	for (int i = 0; i < 5; ++i)
	{
		if (m_selected[i])
			total += m_countsByState[i];
	}
	// The count is shown before execution, in the OK button itself (as in
	// the HTML popup) and in the count line. OK stays disabled while
	// nothing is selected, so the dialog cannot run a no-op.
	SetDlgItemText(IDOK, strutils::format_string1(_("Check selected (%1 items)"), strutils::to_str(total)));
	SetDlgItemText(IDC_MERGE20_BATCH_COUNT, strutils::format_string1(_("%1 files will be checked"), strutils::to_str(total)));
	GetDlgItem(IDOK)->EnableWindow(total > 0);
}

void Merge20BatchCheckDlg::OnOK()
{
	m_action = Action::Selected;
	__super::OnOK();
}

void Merge20BatchCheckDlg::OnCheckAllDisplayed()
{
	m_action = Action::CheckAllDisplayed;
	EndDialog(IDOK);
}

void Merge20BatchCheckDlg::OnClearAll()
{
	m_action = Action::ClearAll;
	EndDialog(IDOK);
}
