// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20AuditDlg.h
 *
 * @brief Audit-log viewer dialog (Merge20 best-workflow B11,
 * pillar 6: "trace who changed what, and when, afterwards").
 *
 * Merge20 (B11, gap analysis section 4.2 B11): show the folder
 * pair's audit log as a chronological list (oldest first), one
 * row per judgment: date/time, file, operation (with the diff
 * number when about one diff), old value, new value, operator
 * id, remarks. The dialog only displays the rows it is given;
 * the caller (CDirView::OnMerge20AuditView) loads them from the
 * log file. The note line states that this log is separate from
 * the learning data (learning stores patterns for suggestions,
 * not this history). [Export as CSV...] exports the same rows
 * with the S6 CSV rules (BOM-prefixed UTF-8); [Close] changes
 * nothing. The dialog never writes the log itself.
 */
#pragma once

#include "TrDialogs.h"
#include "resource.h"
#include <vector>
#include "Merge20/Merge20Audit.h"

/**
 * @brief One row of the audit-log list: display strings, already
 * mapped by the caller (Merge20AuditBuildDisplayRow), so the
 * dialog, the list and the CSV never disagree. Reuses
 * Merge20AuditCsvRow (the CSV row) as the display row.
 */
using Merge20AuditDlgRow = Merge20AuditCsvRow;

class Merge20AuditDlg : public CTrDialog
{
	DECLARE_DYNAMIC(Merge20AuditDlg)

public:
	/** @brief What the user chose in the dialog. */
	enum class Result
	{
		Close,  /**< Close (or Cancel / close button): change nothing. */
		Export, /**< Export as CSV... (the caller runs the save dialog). */
	};

	/**
	 * @param [in] pParent Parent window.
	 * @param [in] rows One row per log entry, in file order (oldest first).
	 * @param [in] skippedLines Lines skipped while loading the log (0 when clean).
	 */
	explicit Merge20AuditDlg(CWnd* pParent, const std::vector<Merge20AuditDlgRow>& rows, int skippedLines);

	enum { IDD = IDD_MERGE20_AUDIT };

	Result GetResult() const { return m_result; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	afx_msg void OnExport();
	DECLARE_MESSAGE_MAP()

private:
	const std::vector<Merge20AuditDlgRow>& m_rows;
	int m_skippedLines;
	Result m_result = Result::Close;
	CListCtrl m_list;
};
