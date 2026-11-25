// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20LearnDlg.h
 *
 * @brief Learning dialog for the Merge20 folder compare workflow.
 *
 * Merge20 (Phase 3 L4, design sections 3.3 and 5, spec section 4(d)):
 * the HTML tool's learning modal (`learnMo`) as a WinMerge dialog.
 * It shows the learning statistics (patterns / judgments / sources /
 * candidates currently shown), the learned rules (top 50, with the
 * dominant strategy and its share, computed at run time as an integer
 * ratio, like the L0 matcher), and the sources (one per import, with
 * rule / judgment counts). Checked sources can be deleted together:
 * only each checked source's contribution is subtracted (L0
 * `RemoveSource`), the other sources stay. The ON/OFF toggle switches
 * the same key the compare options page binds
 * (`OPT_MERGE20_LEARNING_ENABLED`); switching it recomputes the
 * candidates of all open merge docs.
 *
 * Merge20 (Phase 3 L5): the export / import / merged-export / clear
 * buttons are wired here (design section 5). The source list is a
 * check-box list (CCheckListBox): the checked sources are the ones
 * the merged export merges and the multi-source delete deletes
 * (the HTML modal checks every source by default, and so does this
 * dialog). The dialog only displays and manages learning data
 * (spec section 4(c)): it never applies a candidate to a merge result.
 */
#pragma once

#include <vector>
#include "TrDialogs.h"
#include "UnicodeString.h"
#include "resource.h"

class Merge20LearnDlg : public CTrDialog
{
	DECLARE_DYNAMIC(Merge20LearnDlg)

public:
	explicit Merge20LearnDlg(CWnd* pParent);

	enum { IDD = IDD_MERGE20_LEARN };

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	afx_msg void OnToggle();
	afx_msg void OnDeleteSource();
	afx_msg void OnExport();
	afx_msg void OnImport();
	afx_msg void OnMergeSources();
	afx_msg void OnClear();
	afx_msg void OnSelChangeSources();
	afx_msg void OnCheckChangeSources();
	DECLARE_MESSAGE_MAP()

private:
	/** @brief Re-read the store and rebuild every control. */
	void RefreshAll();
	/** @brief Source ids of the checked rows of m_sourcesList. */
	std::vector<String> GetCheckedSourceIds();
	/** @brief Enable the delete / merged-export buttons when a source is checked. */
	void UpdateSourceButtons();
	/** @brief Sharing-notice confirmation before writing an export file. */
	bool AskExportConfirm(const String& filePath);

	CCheckListBox m_sourcesList;
	CListBox m_rulesList;
	/** @brief Source ids in the order of the rows of m_sourcesList. */
	std::vector<String> m_sourceIds;
};
