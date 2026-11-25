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
 * rule / judgment counts). One source can be deleted here: only that
 * source's contribution is subtracted (L0 `RemoveSource`), the other
 * sources stay. The ON/OFF toggle switches the same key the compare
 * options page binds (`OPT_MERGE20_LEARNING_ENABLED`); switching it
 * recomputes the candidates of all open merge docs.
 *
 * The dialog only displays and manages learning data (spec section
 * 4(c)): it never applies a candidate to a merge result. The
 * export / import / merged-export / clear buttons are placed here
 * but stay disabled in L4; their wiring is L5's scope (design
 * section 5). The source list has no check boxes yet either: the
 * check boxes select sources for the merged export, so they come
 * with L5; L4 deletes the single selected source.
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
	afx_msg void OnSelChangeSources();
	DECLARE_MESSAGE_MAP()

private:
	/** @brief Re-read the store and rebuild every control. */
	void RefreshAll();

	CListBox m_sourcesList;
	CListBox m_rulesList;
	/** @brief Source ids in the order of the rows of m_sourcesList. */
	std::vector<String> m_sourceIds;
};
