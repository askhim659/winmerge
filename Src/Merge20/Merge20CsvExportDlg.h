// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20CsvExportDlg.h
 *
 * @brief Range-selection dialog for the Merge20 diff-list CSV export.
 *
 * Merge20 (S6, design section 3.5, spec section 4(f)): the HTML tool
 * offers the choice with two buttons in its CSV modal ("selected only"
 * / "all diffs"). This dialog is the WinMerge equivalent: two radio
 * buttons, each labelled with the file count of its range before
 * anything runs (as in the S3 batch-check dialog). The "selected
 * only" radio is disabled when no file has its output checkbox on.
 * Cancel exports nothing.
 */
#pragma once

#include "TrDialogs.h"
#include "resource.h"

class Merge20CsvExportDlg : public CTrDialog
{
	DECLARE_DYNAMIC(Merge20CsvExportDlg)

public:
	/** @brief Export range chosen by the user (see Merge20CsvExportRange). */
	enum class Range
	{
		SelectedOnly, /**< Files with the output checkbox on. */
		AllDiffs      /**< Files with differences. */
	};

	/**
	 * @param [in] pParent Parent window.
	 * @param [in] selectedOnlyCount Files with the output checkbox on.
	 * @param [in] allDiffsCount Files with differences.
	 */
	explicit Merge20CsvExportDlg(CWnd* pParent, int selectedOnlyCount, int allDiffsCount);

	enum { IDD = IDD_MERGE20_CSV_EXPORT };

	Range GetRange() const { return m_range; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	afx_msg void OnRangeClicked(UINT nID);
	DECLARE_MESSAGE_MAP()

private:
	int m_selectedOnlyCount;
	int m_allDiffsCount;
	Range m_range = Range::AllDiffs;
};
