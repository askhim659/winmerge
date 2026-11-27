// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20CloseWarnDlg.h
 *
 * @brief Undecided-items warning dialog (Merge20 best-workflow B2
 * stage 1, pillar 1: "no diff left without an explicit conclusion").
 *
 * Merge20 (B2 stage 1, gap analysis section 4.2 B2): when a folder
 * compare document is about to close with undecided items left,
 * show the counts (unreviewed files, undecided diffs, of which
 * high risk) and offer three choices: show unreviewed only (D3
 * TODO-only patrol), proceed anyway (close the document), and
 * cancel (abort the close and change nothing). The wording and
 * feel follow the B9 summary dialog and the S5 count-confirm
 * dialog: the same "counts + target + cancel changes nothing"
 * shape (gap analysis section 3.2).
 *
 * The dialog only displays the counts it is given; the caller
 * (CDirDoc::SaveModified) builds them with
 * Merge20CloseWarnComputeCounts. The dialog never writes anything
 * itself.
 *
 * Stage 2 hook: the proceed button label is a constructor
 * argument, so the S7 unified-ZIP output gate and the S5
 * bulk-approve gate can reuse this dialog with their own label
 * ("Output anyway" / "Approve anyway") while the counts, the
 * warning and the other two buttons stay the same.
 */
#pragma once

#include "TrDialogs.h"
#include "resource.h"
#include "Merge20/Merge20CloseWarn.h"

/** @brief The folder-compare close-warning dialog (B2 stage 1). */
class Merge20CloseWarnDlg : public CTrDialog
{
	DECLARE_DYNAMIC(Merge20CloseWarnDlg)

public:
	/** @brief What the user chose in the dialog. */
	enum class Result
	{
		Cancel,   /**< Cancel (or close): abort, change nothing. */
		TodoOnly, /**< Show unreviewed only (D3 TODO-only patrol). */
		Proceed   /**< Proceed anyway (close the document). */
	};

	/**
	 * @param [in] pParent Parent window.
	 * @param [in] counts Close-warning counts (from
	 *             Merge20CloseWarnComputeCounts).
	 * @param [in] proceedLabel Text of the proceed button, already
	 *             translated by the caller (stage 1: "Close
	 *             anyway"; stage 2 callers pass their own label).
	 */
	explicit Merge20CloseWarnDlg(CWnd* pParent, const Merge20CloseWarnCounts& counts, const String& proceedLabel);

	enum { IDD = IDD_MERGE20_CLOSE_WARN };

	Result GetResult() const { return m_result; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	afx_msg void OnTodoOnly();
	afx_msg void OnProceed();
	DECLARE_MESSAGE_MAP()

private:
	Merge20CloseWarnCounts m_counts;
	String m_proceedLabel;
	Result m_result = Result::Cancel;
};
