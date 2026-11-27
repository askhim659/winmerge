// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20CloseWarnDlg.cpp
 *
 * @brief Implementation of the undecided-items warning dialog (B2
 * stage 1).
 *
 * The counts line and the warning are set in OnInitDialog from
 * the counts the caller computed (the B9 pattern: the same
 * format_strings with %1/%2/%3, so the dialog, the counts and the
 * Japanese translation stay in step). The "show unreviewed only"
 * button is the default (the non-destructive choice); "proceed
 * anyway" carries the caller-supplied label; Cancel (IDCANCEL,
 * including the close button and Esc) leaves the result at
 * Cancel, so the caller changes nothing.
 */
#include "StdAfx.h"
#include "Merge20CloseWarnDlg.h"

IMPLEMENT_DYNAMIC(Merge20CloseWarnDlg, CTrDialog)

Merge20CloseWarnDlg::Merge20CloseWarnDlg(CWnd* pParent, const Merge20CloseWarnCounts& counts, const String& proceedLabel)
	: CTrDialog(Merge20CloseWarnDlg::IDD, pParent)
	, m_counts(counts)
	, m_proceedLabel(proceedLabel)
{
}

void Merge20CloseWarnDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(Merge20CloseWarnDlg, CTrDialog)
	ON_BN_CLICKED(IDC_MERGE20_CLOSE_WARN_TODO_ONLY, OnTodoOnly)
	ON_BN_CLICKED(IDC_MERGE20_CLOSE_WARN_PROCEED, OnProceed)
END_MESSAGE_MAP()

BOOL Merge20CloseWarnDlg::OnInitDialog()
{
	__super::OnInitDialog();

	// Counts (acceptance (b)): unreviewed files, undecided diffs,
	// of which high risk (see Merge20CloseWarn.h for the counting
	// rules).
	const String undecidedFiles = strutils::to_str(m_counts.undecidedFiles);
	const String undecidedDiffs = strutils::to_str(m_counts.undecidedDiffs);
	const String highRisk = strutils::to_str(m_counts.highRisk);
	const String* countArgs[] = { &undecidedFiles, &undecidedDiffs, &highRisk };
	SetDlgItemText(IDC_MERGE20_CLOSE_WARN_COUNTS,
		strutils::format_strings(_("Unreviewed files: %1 | Undecided diffs: %2 | Of which high risk: %3"), countArgs, 3).c_str());
	SetDlgItemText(IDC_MERGE20_CLOSE_WARN_WARNING,
		strutils::format_strings(_("Warning: %1 unreviewed file(s) and %2 undecided diff(s) remain, of which %3 high risk. Review them before closing."), countArgs, 3).c_str());

	// The proceed button carries the caller-supplied label (stage
	// 1: "Close anyway"); the other two buttons keep their
	// translated template captions ("Show unreviewed only" is the
	// same msgid as the B9 dialog, "Cancel" is stock).
	SetDlgItemText(IDC_MERGE20_CLOSE_WARN_PROCEED, m_proceedLabel.c_str());
	return TRUE;
}

void Merge20CloseWarnDlg::OnTodoOnly()
{
	m_result = Result::TodoOnly;
	EndDialog(IDOK);
}

void Merge20CloseWarnDlg::OnProceed()
{
	m_result = Result::Proceed;
	EndDialog(IDOK);
}
