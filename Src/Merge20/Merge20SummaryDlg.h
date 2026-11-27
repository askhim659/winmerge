// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20SummaryDlg.h
 *
 * @brief End-of-session summary dialog (Merge20 best-workflow B9,
 * pillar 5: "confirm the session is complete before finishing").
 *
 * Merge20 (B9, gap analysis section 4.2 B9): at the end of a
 * session, show one line per file (file name, review state, action
 * category, risk, output on/off, one-line summary) under the
 * H-DASH dashboard counts (total, with differences, changed,
 * old-only, new-only, identical, needs manual work, plus
 * unreviewed and output targets). When unreviewed files remain, a
 * warning line is shown. The three buttons are: show unreviewed
 * only (D3 TODO-only patrol), confirm and export (S7 output; while
 * S7 does not exist, the S6 CSV export), and back (close the dialog
 * and change nothing).
 *
 * The dialog only displays the rows and counts it is given; the
 * caller (CDirView::OnMerge20Summary) builds them from the compare
 * context. The dialog never writes anything itself.
 */
#pragma once

#include "TrDialogs.h"
#include "resource.h"
#include <vector>
#include "Merge20/Merge20Summary.h"

/**
 * @brief One row of the end-of-session summary list: the same
 * display strings the folder compare list and the S6 CSV show, so
 * the dialog, the list and the CSV never disagree.
 */
struct Merge20SummaryRow
{
	String fileName; /**< File name (same text as the folder list). */
	String review;   /**< Review state, short form. */
	String action;   /**< Action category. */
	String risk;     /**< Risk level. */
	String output;   /**< Output checkbox glyph (checked / unchecked). */
	String summary;  /**< Rule-based summary, one line. */
};

class Merge20SummaryDlg : public CTrDialog
{
	DECLARE_DYNAMIC(Merge20SummaryDlg)

public:
	/** @brief What the user chose in the dialog. */
	enum class Result
	{
		Back,     /**< Back (or Cancel / close): change nothing. */
		TodoOnly, /**< Show unreviewed only (D3 TODO-only patrol). */
		Confirm   /**< Confirm and export (S7 output; S6 CSV for now). */
	};

	/**
	 * @param [in] pParent Parent window.
	 * @param [in] rows One row per file, in comparison order.
	 * @param [in] counts Summary counts for the rows (H-DASH).
	 */
	explicit Merge20SummaryDlg(CWnd* pParent, const std::vector<Merge20SummaryRow>& rows, const Merge20SummaryCounts& counts);

	enum { IDD = IDD_MERGE20_SUMMARY };

	Result GetResult() const { return m_result; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	afx_msg void OnTodoOnly();
	afx_msg void OnConfirm();
	DECLARE_MESSAGE_MAP()

private:
	const std::vector<Merge20SummaryRow>& m_rows;
	Merge20SummaryCounts m_counts;
	Result m_result = Result::Back;
	CListCtrl m_list;
};
