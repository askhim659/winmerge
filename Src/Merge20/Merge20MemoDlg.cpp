// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20MemoDlg.cpp
 *
 * @brief Implementation of the Merge20 audit-memo editor dialog (S8).
 */
#include "StdAfx.h"
#include "Merge20/Merge20MemoDlg.h"

IMPLEMENT_DYNAMIC(Merge20MemoDlg, CTrDialog)

Merge20MemoDlg::Merge20MemoDlg(CWnd* pParent, const String& relPath, const String& memo)
	: CTrDialog(Merge20MemoDlg::IDD, pParent)
	, m_relPath(relPath)
	, m_memo(memo)
{
}

void Merge20MemoDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(Merge20MemoDlg, CTrDialog)
END_MESSAGE_MAP()

BOOL Merge20MemoDlg::OnInitDialog()
{
	__super::OnInitDialog();
	SetDlgItemText(IDC_MERGE20_MEMO_PATH, m_relPath.c_str());
	SetDlgItemText(IDC_MERGE20_MEMO_TEXT, m_memo.c_str());
	return TRUE;
}

void Merge20MemoDlg::OnOK()
{
	CString text;
	GetDlgItemText(IDC_MERGE20_MEMO_TEXT, text);
	m_memo = String(text.GetString());
	__super::OnOK();
}
