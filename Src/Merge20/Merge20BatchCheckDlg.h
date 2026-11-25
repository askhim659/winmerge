// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20BatchCheckDlg.h
 *
 * @brief Batch-check dialog for the Merge20 folder compare workflow.
 *
 * Merge20 (S3, design section 3.2, spec section 4(b)): the HTML tool's
 * batch-check popup (v17.5) lets the user tick several review states and
 * then check the output boxes of every matching file at once, showing the
 * matching file count before running. This dialog is the WinMerge
 * equivalent: five check boxes (one per review state), a live target
 * count, and an OK button whose label carries that count
 * ("Check selected (N items)"). "Check all displayed" and "Clear all
 * checks" are separate buttons in the same dialog, as in the HTML popup.
 */
#pragma once

#include "TrDialogs.h"
#include "Merge20Types.h"
#include "resource.h"

class Merge20BatchCheckDlg : public CTrDialog
{
	DECLARE_DYNAMIC(Merge20BatchCheckDlg)

public:
	/** @brief What the user asked for when the dialog closed with IDOK. */
	enum class Action
	{
		None,              /**< Dialog was cancelled. */
		Selected,          /**< Check the files with the selected review states (OK button). */
		CheckAllDisplayed, /**< Check every displayed file. */
		ClearAll           /**< Uncheck every file in the comparison. */
	};

	/**
	 * @param [in] pParent Parent window.
	 * @param [in] countsByState Number of displayed files per review state,
	 *             indexed by static_cast<int>(ReviewState).
	 */
	explicit Merge20BatchCheckDlg(CWnd* pParent, const int countsByState[5]);

	enum { IDD = IDD_MERGE20_BATCH_CHECK };

	Action GetAction() const { return m_action; }
	bool IsStateSelected(ReviewState state) const { return m_selected[static_cast<int>(state)]; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;
	virtual void OnOK() override;

	afx_msg void OnReviewStateClicked(UINT nID);
	afx_msg void OnCheckAllDisplayed();
	afx_msg void OnClearAll();
	DECLARE_MESSAGE_MAP()

private:
	void UpdateCount();

	int m_countsByState[5];
	bool m_selected[5] = {};
	Action m_action = Action::None;
};
