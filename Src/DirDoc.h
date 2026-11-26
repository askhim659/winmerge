/////////////////////////////////////////////////////////////////////////////
//    WinMerge:  an interactive diff/merge utility
//    Copyright (C) 1997  Dean P. Grimm
//    SPDX-License-Identifier: GPL-2.0-or-later
/////////////////////////////////////////////////////////////////////////////
/** 
 * @file  DirDoc.h
 *
 * @brief Declaration file for CDirDoc
 *
 */
#pragma once

#include <memory>
#include "DiffThread.h"
#include "PluginManager.h"
#include "FileFilterHelper.h"
#include "DirCmpReport.h"
#include "IMDITab.h"
#include "IDirDoc.h"
#include "Merge20/Merge20Types.h"

class CDirView;
struct IMergeDoc;
typedef CTypedPtrList<CPtrList, IMergeDoc *> MergeDocPtrList;
class DirDocFilterGlobal;
class DirDocFilterByExtension;
class CTempPathContext;
struct FileActionItem;
struct FileLocation;
class DirCompProgressBar;

/////////////////////////////////////////////////////////////////////////////
// CDirDoc document

/**
 * @brief Class for folder compare data.
 * This class is "document" class for folder compare. It has compare context,
 * which in turn has a list of differences and other compare result data.
 * This class also has compare statistics which are updated during compare.
 * GUI calls this class to operate with results.
 */
class CDirDoc : public CDocument, public IMDITab, public IDirDoc
{
protected:
	CDirDoc();           // protected constructor used by dynamic creation
	DECLARE_DYNCREATE(CDirDoc)

// Attributes
public:
	CTempPathContext *m_pTempPathContext;
	int m_nDirs;
	static int m_nDirsTemp;

// Operations
public:
	bool CloseMergeDocs();
	CDirView * GetMainView() const;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDirDoc)
	public:
	virtual void Serialize(CArchive& ar);   // overridden for document i/o
	virtual void SetTitle(LPCTSTR lpszTitle);
	CString GetTooltipString() const;
	protected:
	virtual BOOL OnNewDocument();
	virtual void OnCloseDocument();
	virtual BOOL SaveModified();
	//}}AFX_VIRTUAL

// Implementation
public:
	void InitCompare(const PathContext & paths, bool bRecursive, CTempPathContext *);
	void DiffThreadCallback(int& state);
	void Rescan();
	String GetDescription(int nIndex) const { return m_strDesc[nIndex]; };
	bool GetReadOnly(int nIndex) const;
	const bool *GetReadOnly(void) const;
	void SetReadOnly(int nIndex, bool bReadOnly);
	const String& GetReportFile() const { return m_sReportFile; }
	void SetReportFile(const String& sReportFile) { m_sReportFile = sReportFile; }
	const std::vector<String>& GetHiddenItems() const { return m_pCtxt->m_vCurrentlyHiddenItems; }
	void SetHiddenItems(const std::vector<String>& hiddenItems) { m_pCtxt->m_vCurrentlyHiddenItems = hiddenItems; }
	bool GetGeneratingReport() const { return m_bGeneratingReport; }
	void SetGeneratingReport(bool bGeneratingReport) { m_bGeneratingReport = bGeneratingReport; }
	void SetReport(DirCmpReport* pReport) { m_pReport.reset(pReport);  }
	bool HasDirView() const { return m_pDirView != nullptr; }
	void RefreshOptions();
	void CompareReady();
	clock_t GetElapsedTime() const { return m_elapsed; }
	void UpdateChangedItem(const PathContext & paths,
		UINT nDiffs, UINT nTrivialDiffs, bool bIdentical);
	void UpdateResources();
	void InitStatusStrings();
	void ReloadItemStatus(DIFFITEM *diffPos, int idx);
	// Merge20 (S1): review state of one item. The state lives in the
	// CDiffContext sidecar map (see CDiffContext::GetMerge20ReviewState);
	// these forwarders are the API the S2 key handling will use.
	ReviewState GetMerge20ReviewState(const DIFFITEM& di) const;
	void SetMerge20ReviewState(const DIFFITEM& di, ReviewState state);
	// Merge20 (S2): output checkbox flag of one item, and the review
	// setter with the output-flag linkage of spec section 4(b).
	bool GetMerge20IncludeInOutput(const DIFFITEM& di) const;
	void SetMerge20IncludeInOutput(const DIFFITEM& di, bool include);
	void ApplyMerge20ReviewState(const DIFFITEM& di, ReviewState state);
	// Merge20 (Phase 4 D2, IDirDoc): review state of the file a merge
	// doc shows, found by the merge doc's paths (same lookup, incl. the
	// swapped-paths fallback, as SetMerge20Classification). The
	// dedicated-mode bar's review buttons use these.
	ReviewState GetMerge20ReviewState(const PathContext& paths) const override;
	void ApplyMerge20ReviewState(const PathContext& paths, ReviewState state) override;
	// Merge20 (S4, IDirDoc): store the classification computed by a
	// merge doc after a rescan (see IDirDoc::SetMerge20Classification).
	void SetMerge20Classification(const PathContext& paths, const Merge20FileState& state) override;
	// Merge20 (S8): audit memo of one item (see CDiffContext::GetMerge20Memo).
	String GetMerge20Memo(const DIFFITEM& di) const;
	void SetMerge20Memo(const DIFFITEM& di, const String& memo);
	// Merge20 (S8): refresh the diff judgments of all still-open merge
	// docs into the sidecar map (called before a session save).
	void RefreshMerge20DiffJudgments();
	// Merge20 (S8): true once the session-restore offer was shown for
	// this document (the offer is made once, after the first compare).
	bool Merge20SessionAsked() const { return m_bMerge20SessionAsked; }
	void SetMerge20SessionAsked() { m_bMerge20SessionAsked = true; }
	void Redisplay();
	virtual ~CDirDoc();
	void SetDirView( CDirView *newView ); // TODO Perry
	void AddMergeDoc(IMergeDoc * pMergeDoc);
	void MergeDocClosing(IMergeDoc * pMergeDoc);
	CDiffThread m_diffThread;
	void UpdateHeaderPath(int nIndex);
	void AbortCurrentScan();
	void PauseCurrentScan();
	void ContinueCurrentScan();
	bool IsCurrentScanAbortable() const;
	void SetDescriptions(const String strDesc[]);
	void ApplyDisplayRoot(int nIndex, String &);

	bool HasDiffs() const { return m_pCtxt != nullptr; }
	const CDiffContext & GetDiffContext() const { return *m_pCtxt; }
	CDiffContext& GetDiffContext() { return *m_pCtxt.get(); }
	void SetMarkedRescan() {m_bMarkedRescan = true; }
	const CompareStats * GetCompareStats() const { return m_pCompareStats.get(); };
	bool IsArchiveFolders() const;
	PluginManager& GetPluginManager() { return m_pluginman; };
	void Swap(int idx1, int idx2);
	bool MoveableToNextDiff();
	bool MoveableToPrevDiff();
	void MoveToNextDiff(IMergeDoc *pMergeDoc);
	void MoveToPrevDiff(IMergeDoc *pMergeDoc);
	void MoveToFirstFile(IMergeDoc* pMergeDoc);
	void MoveToLastFile(IMergeDoc* pMergeDoc);
	void MoveToNextFile(IMergeDoc* pMergeDoc);
	void MoveToPrevFile(IMergeDoc* pMergeDoc);
	bool IsFirstFile();
	bool IsLastFile();

	bool CompareFilesIfFilesAreLarge(int nFiles, const FileLocation ifileloc[]);

protected:
	void InitDiffContext(CDiffContext *pCtxt);
	void LoadLineFilterList(CDiffContext *pCtxt);
	void LoadSubstitutionFiltersList(CDiffContext* pCtxt);
	void CheckFilter();
	DirCompProgressBar* GetCompProgressBar();

	// Generated message map functions
	//{{AFX_MSG(CDirDoc)
		// NOTE - the ClassWizard will add and remove member functions here.
	afx_msg void OnBnClickedComparisonStop();
	afx_msg void OnBnClickedComparisonPause();
	afx_msg void OnBnClickedComparisonContinue();
	afx_msg void OnCbnSelChangeCPUCores();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	// Implementation data
private:
	std::unique_ptr<CDiffContext> m_pCtxt; /**< Pointer to diff-data */
	CDirView *m_pDirView; /**< Pointer to GUI */
	std::unique_ptr<CompareStats> m_pCompareStats; /**< Compare statistics */
	MergeDocPtrList m_MergeDocs; /**< List of file compares opened from this compare */
	bool m_bRO[3]; /**< Is left/middle/right side read-only */
	String m_strDesc[3]; /**< Left/middle/right side desription text */
	String m_sReportFile;
	PluginManager m_pluginman;
	FileFilterHelper m_imgfileFilter;
	bool m_bMarkedRescan; /**< If `true` next rescan scans only marked items */
	bool m_bGeneratingReport;
	bool m_bMerge20SessionAsked = false; /**< Merge20 (S8): session-restore offer already shown for this document */
	std::unique_ptr<DirCmpReport> m_pReport;
	FileFilterHelper m_fileHelper; /**< File filter helper */
	clock_t m_compareStart; /**< Starting process time of the compare */
	clock_t m_elapsed; /**< Elapsed time of the compare */
	std::shared_ptr<TempFile> m_tempFile; /**< Temporary file for clipboard copy */
};

/**
 * @brief Set left/middle/right side readonly-status
 * @param nIndex Select side to set 
 * @param bReadOnly New status of selected side
 */
inline void CDirDoc::SetReadOnly(int nIndex, bool bReadOnly)
{
	m_bRO[nIndex] = bReadOnly;
}

/**
 * @brief Return left/middle/right side readonly-status
 * @param nIndex Select side to ask
 */
inline bool CDirDoc::GetReadOnly(int nIndex) const
{
	return m_bRO[nIndex];
}

inline const bool *CDirDoc::GetReadOnly(void) const
{
	return m_bRO;
}

