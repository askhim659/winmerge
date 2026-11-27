// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20AuditDlg.cpp
 *
 * @brief Implementation of the audit-log viewer dialog (B11).
 *
 * The count line and the skipped-lines note are set in
 * OnInitDialog from the rows the caller loaded; the list is
 * filled from those rows (the same display strings the CSV
 * export writes, so the dialog and the CSV never disagree).
 * [Export as CSV...] is disabled when there is nothing to
 * export; Close (IDCANCEL, including the close button) leaves
 * the result at Close, so the caller changes nothing.
 */
#include "StdAfx.h"
#include "Merge20/Merge20AuditDlg.h"
#include "Merge20/Merge20AuditDoc.h"

IMPLEMENT_DYNAMIC(Merge20AuditDlg, CTrDialog)

Merge20AuditDlg::Merge20AuditDlg(CWnd* pParent, const std::vector<Merge20AuditDlgRow>& rows, int skippedLines)
	: CTrDialog(Merge20AuditDlg::IDD, pParent)
	, m_rows(rows)
	, m_skippedLines(skippedLines)
{
}

void Merge20AuditDlg::DoDataExchange(CDataExchange* pDX)
{
	CTrDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_MERGE20_AUDIT_LIST, m_list);
}

BEGIN_MESSAGE_MAP(Merge20AuditDlg, CTrDialog)
	ON_BN_CLICKED(IDC_MERGE20_AUDIT_EXPORT, OnExport)
END_MESSAGE_MAP()

BOOL Merge20AuditDlg::OnInitDialog()
{
	__super::OnInitDialog();

	// Count line (acceptance (c): the entries can be traced in
	// chronological order; the count says how many there are).
	const String count = strutils::to_str(static_cast<int>(m_rows.size()));
	String info = strutils::format_string1(_("Audit log entries: %1"), count);
	if (m_skippedLines > 0)
	{
		const String skipped = strutils::to_str(m_skippedLines);
		info += _T("\n");
		info += strutils::format_string1(_("%1 line(s) could not be read and were skipped."), skipped);
	}
	SetDlgItemText(IDC_MERGE20_AUDIT_INFO, info.c_str());

	// The audit log is not the learning data: say so in the
	// dialog itself (gap analysis section 4.2 B11 note).
	SetDlgItemText(IDC_MERGE20_AUDIT_NOTE,
		_("This audit log records who changed what and when. It is separate from the learning data (learning stores patterns for suggestions, not this history).").c_str());

	// One line per entry, oldest first (file order): the seven
	// columns are the CSV headers, so the dialog and the CSV
	// never disagree.
	const std::vector<String> headers = Merge20AuditCsvHeaders();
	m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	static const int widths[] = { 120, 150, 110, 60, 60, 70, 56 };
	for (size_t i = 0; i < headers.size(); ++i)
		m_list.InsertColumn(static_cast<int>(i), headers[i].c_str(), LVCFMT_LEFT, widths[i]);
	for (size_t i = 0; i < m_rows.size(); ++i)
	{
		const Merge20AuditDlgRow& row = m_rows[i];
		const int item = m_list.InsertItem(static_cast<int>(i), row.time.c_str());
		m_list.SetItemText(item, 1, row.file.c_str());
		m_list.SetItemText(item, 2, row.kind.c_str());
		m_list.SetItemText(item, 3, row.oldValue.c_str());
		m_list.SetItemText(item, 4, row.newValue.c_str());
		m_list.SetItemText(item, 5, row.operatorId.c_str());
		m_list.SetItemText(item, 6, row.note.c_str());
	}

	// Nothing to export when the log is empty.
	GetDlgItem(IDC_MERGE20_AUDIT_EXPORT)->EnableWindow(!m_rows.empty());
	return TRUE;
}

void Merge20AuditDlg::OnExport()
{
	m_result = Result::Export;
	EndDialog(IDOK);
}
