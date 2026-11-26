// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20MasterDlg.h
 *
 * @brief Master management dialog (Merge20 Phase 4 D8, design
 * sections 4.4 and 5.2/D8, spec sections 3 (4/4) and 6).
 *
 * One modal dialog for all three masters (the D8 sharing
 * decision: the risk-keyword editor shares this dialog instead
 * of getting its own window). The master is switched with the
 * combo at the top; the list, the editor and the buttons below
 * follow the selected master. The dialog edits a working copy
 * and writes it to the live stores only on Save:
 * exclude-word rules to the prediffer's option keys
 * (`Plugins\Merge20Exclude.sct/`), risk keywords to
 * OPT_MERGE20_RISK_KEYWORDS, and the whole masters to
 * `Merge20Masters.json` (the shareable mirror). Export and
 * import (JSON, L5 precedent: a save dialog plus a
 * sharing-notice confirmation) work on the working copy.
 *
 * The dialog is modal (the learning dialog precedent): after
 * it closes with Save, the folder view re-compares all files
 * when the exclude rules changed (see ExcludeChanged()).
 * Highlight display in the compare views is not part of D8,
 * so highlight changes need no re-compare; risk-keyword
 * changes take effect the next time a file is classified
 * (the S4 hook in CMergeDoc::Rescan).
 */
#pragma once

#include <vector>
#include "TrDialogs.h"
#include "Merge20/Merge20Master.h"
#include "resource.h"

class CMerge20MasterDlg : public CTrDialog
{
	DECLARE_DYNAMIC(CMerge20MasterDlg);
public:
	CMerge20MasterDlg();
	virtual ~CMerge20MasterDlg() = default;

	/** @brief True when the exclude rules (or IgnoreL2) differ from the state at dialog open. */
	bool ExcludeChanged() const;

protected:
	virtual BOOL OnInitDialog() override;
	virtual void OnOK() override;

	//{{AFX_MSG(CMerge20MasterDlg)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMasterChanged();
	afx_msg void OnSelChangeList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnButtonClicked(UINT nID);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	/** @brief Which master the combo selects. */
	enum MasterKind
	{
		MASTER_EXCLUDE = 0,
		MASTER_HIGHLIGHT = 1,
		MASTER_RISK = 2
	};

	void LayoutAll();
	void RefreshMaster();
	void RefreshList();
	void RefreshEditor();
	void RefreshEditorFromSelection();
	/** @brief Editor field values for the current master. */
	bool ReadEditor(String& pattern, String& name, bool& useRegExp, bool& ignoreCase, bool& enabled) const;
	/** @brief Validate the editor pattern (non-empty, and a valid regex when regex is on). */
	bool ValidateEditorPattern(const String& pattern, bool useRegExp) const;
	/** @brief True when the pattern already exists in the current master (excluding one row). */
	bool PatternExists(const String& pattern, int skipRow) const;
	void AddRule();
	void UpdateRule();
	void DeleteRule();
	void MoveRule(int delta);
	void CheckRegex();
	void ExportMasters();
	void ImportMasters();
	/** @brief Sharing-notice confirmation before writing an export file (L5 precedent). */
	bool AskExportConfirm(const String& filePath);
	/** @brief Write the working risk-keyword list back into m_masters.riskKeywords. */
	void SyncRiskToMasters();
	/** @brief Hint text of the current master. */
	String HintText() const;

	CComboBox m_cbMaster;
	CListCtrl m_list;
	CStatic m_stLabelMaster;
	CStatic m_stLabelPattern;
	CStatic m_stLabelName;
	CStatic m_stHint;
	CEdit m_edPattern;
	CEdit m_edName;
	CButton m_chkRegex;
	CButton m_chkIgnoreCase;
	CButton m_chkEnabled;
	CButton m_chkIgnoreL2;
	CButton m_btnAdd;
	CButton m_btnUpdate;
	CButton m_btnDelete;
	CButton m_btnUp;
	CButton m_btnDown;
	CButton m_btnCheck;
	CButton m_btnExport;
	CButton m_btnImport;
	CButton m_btnSave;
	CButton m_btnCancel;

	Merge20Masters m_masters; /**< Working copy (written to the live stores on Save) */
	std::vector<Merge20ExcludeRule> m_originalExcludeRules; /**< Exclude rules at dialog open (change detection) */
	bool m_originalIgnoreL2 = true; /**< IgnoreL2 at dialog open (change detection) */
	std::vector<String> m_riskKeywords; /**< Working risk-keyword list (parsed from m_masters.riskKeywords) */
	int m_nMaster = MASTER_EXCLUDE; /**< Selected master */
};
