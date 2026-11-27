// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCsvExportDlg.h
 *
 * @brief FORM/XML comparison (Wave 5 F7): range-selection dialog
 * for the FORM compare CSV export (design
 * merge20-form-compare-design.md sections 9 and 10 F7).
 *
 * The S6 `Merge20CsvExportDlg` way, with FORM ranges: the
 * dialog offers "filtered results only" (the files the current
 * filter tab / search / sort shows, design section 9) or "all
 * changed files" (every changed file), each labelled with its
 * file count before anything runs (as in the S3 batch-check
 * dialog). The "filtered only" radio is disabled when the
 * filter shows nothing. Cancel exports nothing.
 */
#pragma once

#include "TrDialogs.h"
#include "resource.h"

class CMerge20FormCsvExportDlg : public CTrDialog
{
	DECLARE_DYNAMIC(CMerge20FormCsvExportDlg)

public:
	/** @brief Export range chosen by the user (design section 9). */
	enum class Range
	{
		FilteredOnly, /**< Files shown by the current filter/search. */
		AllChanged    /**< All changed files. */
	};

	/**
	 * @param [in] pParent Parent window.
	 * @param [in] filteredOnlyCount Files shown by the current filter/search.
	 * @param [in] allChangedCount All changed files.
	 */
	explicit CMerge20FormCsvExportDlg(CWnd* pParent, int filteredOnlyCount, int allChangedCount);

	enum { IDD = IDD_MERGE20_FORM_CSV_EXPORT };

	Range GetRange() const { return m_range; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	afx_msg void OnRangeClicked(UINT nID);
	DECLARE_MESSAGE_MAP()

private:
	int m_filteredOnlyCount;
	int m_allChangedCount;
	Range m_range = Range::AllChanged;
};
