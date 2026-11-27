// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormMasterDlg.h
 *
 * @brief FORM/XML comparison (Wave 5 F6): the name-master edit
 * dialog (design merge20-form-compare-design.md sections 2.10,
 * 8 and 10 F6).
 *
 * A modal dialog (the HTML tool's `masterModal`): a text area
 * with one `ID<TAB>form name` row per line, a count badge, a
 * "Save and apply immediately" button and a "Restore default
 * list" button. The dialog has no controls in its template (the
 * D8 master-dialog precedent): the controls are created in code
 * in OnInitDialog and laid out at runtime.
 *
 * Saving writes `Merge20FormMaster.txt` (tab-separated text,
 * UTF-8 with a BOM) to the learning-data directory scheme
 * (design section 8.2): `<progpath>\Merge20FormMaster\` when
 * the options manager is INI-based (portable), otherwise
 * `%APPDATA%\WinMerge\Merge20FormMaster\`. The dialog edits a
 * working copy; the live form names are re-resolved only after
 * a successful save (the view calls
 * CFormCompareDoc::RefreshFormNames when DoModal returns IDOK).
 *
 * The file functions below are also used by CFormCompareDoc
 * (RunAnalysis resolves the form names from the same file), so
 * they are free functions, not dialog members.
 */
#pragma once

#include "TrDialogs.h"
#include "Merge20/Merge20FormMaster.h"
#include "resource.h"

/**
 * @brief Full path of the name-master file
 * (`Merge20FormMaster.txt`, design section 8.2).
 * @return The file path (directory scheme like the learning data).
 */
String Merge20FormMasterGetFilePath();

/**
 * @brief Read the name-master file as text (UTF-8, BOM stripped).
 * @param [out] outText File text.
 * @return False when the file is missing, unreadable or not valid
 * UTF-8 (first run: a missing file is normal; the caller then
 * uses the default list).
 */
bool Merge20FormMasterLoadText(String& outText);

/**
 * @brief Write name-master text to the file (UTF-8 with a BOM),
 * creating the directory when needed. The write goes through a
 * temporary file plus rename (the S8 session-save way).
 * @param [in] text Text to write (`ID<TAB>name` lines).
 * @param [out] errDetail Error detail (English) on failure.
 * @return False when the file cannot be written.
 */
bool Merge20FormMasterSaveText(const String& text, String& errDetail);

/**
 * @brief The default master as text (design appendix A).
 * @return The default `ID<TAB>name` text.
 */
String Merge20FormMasterGetDefaultText();

/**
 * @brief The name-master edit dialog (design section 8.2).
 * DoModal returns IDOK only when the text was validated and
 * saved; IDCANCEL (or closing the window) discards the edits.
 * Several windows editing the master at the same time are not
 * warned about (first-version limitation, design section 7.3):
 * the last save wins.
 */
class CMerge20FormMasterDlg : public CTrDialog
{
	DECLARE_DYNAMIC(CMerge20FormMasterDlg);
public:
	enum { IDD = IDD_MERGE20_FORM_MASTER };
	CMerge20FormMasterDlg(CWnd* pParent = nullptr);
	virtual ~CMerge20FormMasterDlg() = default;

	/** @brief True when the dialog closed after a successful save. */
	bool WasSaved() const { return m_bSaved; }

protected:
	virtual BOOL OnInitDialog() override;
	virtual void OnOK() override;

	//{{AFX_MSG(CMerge20FormMasterDlg)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnTextChanged();
	afx_msg void OnResetToDefault();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void LayoutAll();
	/** @brief Recompute the count badge from the edit text. */
	void UpdateCount();

	CStatic m_stHint; /**< Format hint (one `ID<TAB>name` row per line). */
	CStatic m_stCount; /**< Count badge (`Registered: N kinds (M rows)`). */
	CEdit m_edText; /**< The master text (multiline edit). */
	CButton m_btnSave; /**< Save and apply immediately (IDOK). */
	CButton m_btnReset; /**< Restore the default list. */
	CButton m_btnCancel; /**< Close without saving (IDCANCEL). */
	bool m_bSaved = false; /**< True after a successful save. */
};
