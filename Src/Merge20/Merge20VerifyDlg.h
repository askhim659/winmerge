// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20VerifyDlg.h
 *
 * @brief Problem-list dialog for the output-time final
 * verification (Merge20 best-workflow B7, pillar 4).
 *
 * Shown by the verification gate when the engine found a
 * problem: the output is stopped, and this dialog lists the
 * problems (check, file, location, detail) so the person can
 * jump to the offending file/diff, fix it, and try the output
 * again -- or explicitly proceed anyway. The dialog only
 * displays the rows it is given; it never verifies anything
 * itself and never writes anything.
 *
 * The rows are built by the gate (Merge20VerifyGate) from the
 * engine's structured issues; this dialog composes the display
 * text from those fields with `_()`, so every display string
 * goes through Translations/WinMerge/Japanese.po (the S4
 * precedent: the engine returns data, the caller formats it).
 * The syntax note ("simple check only") is shown whenever a
 * syntax issue is present, so the simple check is never
 * mistaken for a syntax guarantee.
 *
 * Buttons: [Jump to location] (default, the non-destructive
 * choice, same lineage as the B2/B9 dialogs), [Output anyway]
 * (explicit override: the gate then lets the save continue),
 * [Close] (IDCANCEL, including Esc and the close button:
 * output stays stopped, nothing changes).
 */
#pragma once

#include <vector>
#include "TrDialogs.h"
#include "resource.h"
#include "Merge20/Merge20Verify.h"

/**
 * @brief One row of the problem list: which file the issue
 * belongs to, and the engine's structured issue.
 */
struct Merge20VerifyIssueRow
{
	int fileIndex = 0;  /**< File index for the caller (S7: folder-list index; one-file gate: 0) */
	String fileName;    /**< Display name of the file (file name only, not the full path) */
	Merge20VerifyIssue issue; /**< The engine's structured issue */
};

/** @brief The B7 problem-list dialog. */
class Merge20VerifyDlg : public CTrDialog
{
	DECLARE_DYNAMIC(Merge20VerifyDlg)

public:
	/** @brief What the user chose in the dialog. */
	enum class Result
	{
		Close,   /**< Close (or Esc): output stays stopped, nothing changes. */
		Jump,    /**< Jump to the selected row's file/diff. */
		Proceed  /**< Output anyway (explicit override). */
	};

	/**
	 * @param [in] pParent Parent window.
	 * @param [in] rows Problem rows (non-empty: the gate only
	 *             shows this dialog when there is a problem).
	 */
	explicit Merge20VerifyDlg(CWnd* pParent, const std::vector<Merge20VerifyIssueRow>& rows);

	enum { IDD = IDD_MERGE20_VERIFY };

	Result GetResult() const { return m_result; }
	/** @brief Index of the selected row for a Jump, or -1. */
	int GetJumpRow() const { return m_jumpRow; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	afx_msg void OnJump();
	afx_msg void OnProceed();
	afx_msg void OnDblclkList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnItemchangedList(NMHDR* pNMHDR, LRESULT* pResult);
	DECLARE_MESSAGE_MAP()

private:
	/** @brief Compose one row's detail text from its issue. */
	String FormatDetail(const Merge20VerifyIssue& issue) const;
	/** @brief Compose one row's location text from its issue. */
	String FormatLocation(const Merge20VerifyIssue& issue) const;
	/** @brief Enable Jump only when a row is selected. */
	void UpdateJumpButton();

	std::vector<Merge20VerifyIssueRow> m_rows;
	CListCtrl m_list;
	Result m_result = Result::Close;
	int m_jumpRow = -1;
};
