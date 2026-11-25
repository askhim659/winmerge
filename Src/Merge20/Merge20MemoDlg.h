// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20MemoDlg.h
 *
 * @brief Audit-memo editor dialog for one file (S8).
 *
 * Merge20 (S8, design section 3.5): the memo column added by S6 was
 * read-only ("editing the memo is not implemented yet"). This dialog
 * is the editor: opened from the folder list context menu, it shows
 * the file's relative path and a multiline edit for the memo. The
 * memo is saved in the session file and included in the diff-list CSV.
 */
#pragma once

#include "TrDialogs.h"
#include "UnicodeString.h"
#include "resource.h"

class Merge20MemoDlg : public CTrDialog
{
	DECLARE_DYNAMIC(Merge20MemoDlg)

public:
	/**
	 * @param [in] pParent Parent window.
	 * @param [in] relPath Relative path shown in the dialog title area.
	 * @param [in] memo Current memo text (empty for a new memo).
	 */
	explicit Merge20MemoDlg(CWnd* pParent, const String& relPath, const String& memo);

	enum { IDD = IDD_MERGE20_MEMO };

	String GetMemo() const { return m_memo; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;
	virtual void OnOK() override;

	DECLARE_MESSAGE_MAP()

private:
	String m_relPath;
	String m_memo;
};
