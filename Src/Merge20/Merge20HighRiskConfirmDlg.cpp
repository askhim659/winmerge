// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20HighRiskConfirmDlg.cpp
 *
 * @brief Implementation of the high-risk confirmation dialog (B14).
 *
 * The counts line and the detail lines are set in OnInitDialog
 * from the texts the caller built (the B9 pattern: the same
 * format_strings with %1/%2/%3, so the dialog, the texts and the
 * Japanese translation stay in step). Cancel (IDCANCEL, including
 * the close button and Esc) leaves the result at Cancel, so the
 * caller changes nothing; Execute is a plain pushbutton, never
 * the default, so an accidental Enter cannot commit the judgment.
 */
#include "StdAfx.h"
#include "Merge20/Merge20HighRiskConfirm.h"
#include "Merge20/Merge20HighRiskConfirmDlg.h"

IMPLEMENT_DYNAMIC(Merge20HighRiskConfirmDlg, CTrDialog)

Merge20HighRiskConfirmDlg::Merge20HighRiskConfirmDlg(CWnd* pParent, const String& countsText, const std::vector<String>& detailLines)
	: CTrDialog(Merge20HighRiskConfirmDlg::IDD, pParent)
	, m_countsText(countsText)
	, m_detailLines(detailLines)
{
}

void Merge20HighRiskConfirmDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(Merge20HighRiskConfirmDlg, CTrDialog)
	ON_BN_CLICKED(IDC_MERGE20_HIGHRISK_EXECUTE, OnExecute)
END_MESSAGE_MAP()

BOOL Merge20HighRiskConfirmDlg::OnInitDialog()
{
	__super::OnInitDialog();

	// Counts line (acceptance (b)): how many high-risk items this
	// judgment would commit, and which file they belong to.
	SetDlgItemText(IDC_MERGE20_HIGHRISK_COUNTS, m_countsText.c_str());

	// One line per item: the risk reason (S4 summary) and, for a
	// diff with a learning candidate, the candidate text. The
	// read-only edit control scrolls when a range judgment lists
	// several diffs.
	String detail;
	for (size_t i = 0; i < m_detailLines.size(); ++i)
	{
		if (i > 0)
			detail += _T("\r\n");
		detail += m_detailLines[i];
	}
	SetDlgItemText(IDC_MERGE20_HIGHRISK_DETAIL, detail.c_str());

	// Cancelling changes nothing (acceptance (c)); the same
	// "cancel changes nothing" shape as the B9/B2 dialogs.
	SetDlgItemText(IDC_MERGE20_HIGHRISK_WARNING, _("Cancelling changes nothing.").c_str());
	return TRUE;
}

void Merge20HighRiskConfirmDlg::OnExecute()
{
	m_result = Result::Execute;
	EndDialog(IDOK);
}

bool Merge20ConfirmHighRiskFileReview(CWnd* pParent, const String& relPath,
	const Merge20FileState& state, ReviewState fromState, ReviewState toState)
{
	if (!Merge20HighRiskConfirmFileCycleNeedsConfirm(state.risk, fromState, toState))
		return true;
	const String* countArgs[] = { &relPath };
	const String countsText = strutils::format_strings(_("High-risk file: %1"), countArgs, 1);
	std::vector<String> detailLines;
	if (state.classified && !state.summary.empty())
		detailLines.push_back(state.summary);
	const String doneText = _("Done");
	const String* nextArgs[] = { &doneText };
	detailLines.push_back(strutils::format_strings(_("Next state: %1"), nextArgs, 1));
	Merge20HighRiskConfirmDlg dlg(pParent, countsText, detailLines);
	return dlg.DoModal() == IDOK && dlg.GetResult() == Merge20HighRiskConfirmDlg::Result::Execute;
}
