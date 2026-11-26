// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20BulkClassifyDlg.h
 *
 * @brief Bulk classification dialog (Merge20 Phase 4 D10, design
 * sections 3.11 and 5.2/D10, spec sections 3 (2/4) and 7).
 *
 * S4 stage 2: classify the files that were never opened, in the
 * background, instead of leaving them "Not calculated". A modeless
 * dialog, owned by the folder view (CDirView::m_wndBulkClassify,
 * the D7 Grep-dialog pattern): created once, shown on demand and
 * hidden on close, never deleted on close, so no PostNcDestroy
 * self-delete is needed.
 *
 * The scan runs on a background worker (DirScan's Poco ThreadPool
 * precedent, DirScan.cpp:480, and the D7 Grep worker): one worker,
 * so results arrive in folder-list order and the single
 * CDiffTextBuffer-free differ never runs concurrently with itself.
 * The worker reads on-disk file text (codepage_detect::Guess +
 * UniMemFile, the CDiffTextBuffer::LoadFromFile sequence without
 * unpacker or prediffer, like the Grep worker) and posts results
 * to this dialog with PostMessage; the dialog applies them on the
 * UI thread through CDirDoc::SetMerge20Classification, which
 * repaints the affected row. One-sided files are classified from
 * their line count alone (the folder doc refines the action and
 * summary for them, like it does for opened files).
 */
#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>
#include "TrDialogs.h"
#include "Merge20/Merge20BulkDiff.h"
#include "Merge20Types.h"
#include "resource.h"

class CDirView;
class Merge20BulkClassifyWorker;
namespace Poco { class ThreadPool; }

/** @brief Worker-to-dialog progress message (Grep uses WM_USER + 100). */
#define WM_MERGE20_BULK_PROGRESS (WM_USER + 101)

/** @brief One file to classify: relative path, expected full paths per pane, and which sides exist. */
struct Merge20BulkClassifyFile
{
	String relPath;
	std::vector<String> paths; /**< Expected full paths (both panes; the missing side's path is the would-be path, for the folder-doc lookup). */
	bool exists[2] = { false, false }; /**< Which sides exist on disk. */
};

/** @brief One classified file posted by the worker (or a skip). */
struct Merge20BulkClassifyResult
{
	String relPath;
	std::vector<String> paths; /**< Expected full paths (for CDirDoc::SetMerge20Classification). */
	Merge20FileState state; /**< Classification (ignored when skipped is true). */
	bool skipped = false; /**< True when the file could not be classified (binary, unreadable). */
};

class CMerge20BulkClassifyDlg : public CTrDialog
{
	DECLARE_DYNAMIC(CMerge20BulkClassifyDlg);
public:
	CMerge20BulkClassifyDlg();
	virtual ~CMerge20BulkClassifyDlg();

	BOOL Create(CWnd* pParentWnd);
	/**
	 * @brief Show the dialog for the given folder view and start a
	 * bulk run over its unclassified files (or bring it to the
	 * front when a run is already going).
	 */
	void ShowBulkClassify(CDirView* pDirView);
	/** @brief Hide the dialog and stop a running bulk run. */
	void CloseBulkClassify();
	/** @brief True while the dialog is shown. */
	bool IsOpen() const { return m_hWnd != nullptr && !!IsWindowVisible(); }
	/** @brief Stop a running bulk run and wait for the worker to end (idempotent). */
	void StopScan();
	/** @brief True while a bulk run is running. */
	bool IsRunning() const { return m_bRunning.load(); }

	// Worker-thread interface (called from Merge20BulkClassifyWorker only).
	/** @brief Append one result (worker thread): counts it and asks the UI thread to apply it. */
	void AddPendingResult(const Merge20BulkClassifyResult& result);
	/** @brief The worker finished (all files, or stopped). */
	void NotifyScanFinished();
	/** @brief Stop requested. */
	bool ShouldStop() const;
	/** @brief Files fully processed so far (for the progress status). */
	void IncrementFilesDone() { m_nFilesDone++; }
	int GetFilesDone() const { return m_nFilesDone.load(); }
	int GetFilesTotal() const { return m_nFilesTotal; }

protected:
	virtual BOOL OnInitDialog() override;
	virtual void OnOK() override;
	virtual void OnCancel() override;
	virtual BOOL PreTranslateMessage(MSG* pMsg) override;

	//{{AFX_MSG(CMerge20BulkClassifyDlg)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	afx_msg void OnButtonClicked(UINT nID);
	afx_msg LRESULT OnBulkProgress(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void LayoutAll();
	void StartRun();
	void UpdateStatus();
	void DrainPendingResults();

	CProgressCtrl m_progress;
	CButton m_btnStop;
	CButton m_btnClose;
	CStatic m_stStatus;
	CStatic m_stFile;

	CDirView* m_pDirView; /**< Folder view that owns this dialog, or nullptr */
	std::vector<Merge20BulkClassifyFile> m_files; /**< Files to classify (built by ShowBulkClassify) */
	std::vector<String> m_riskKeywords; /**< Parsed OPT_MERGE20_RISK_KEYWORDS, read on the UI thread */
	Merge20BulkDiffOptions m_diffOptions; /**< Compare options for the differ, read on the UI thread */
	int m_nGuessEncodingType; /**< OPT_CP_DETECT value read on the UI thread */
	int m_nFilesTotal; /**< Files to classify (for the progress status) */
	bool m_bStoppedByUser; /**< Stop button pressed (status text after a stop) */
	std::chrono::steady_clock::time_point m_start; /**< Run start (for the elapsed time) */

	std::mutex m_mutex; /**< Guards m_pendingResults */
	std::vector<Merge20BulkClassifyResult> m_pendingResults; /**< Results posted by the worker, not yet applied */
	std::atomic<int> m_nClassified; /**< Files classified so far */
	std::atomic<int> m_nSimple; /**< Files classified with the simple fallback so far */
	std::atomic<int> m_nSkipped; /**< Files skipped so far */
	std::atomic<int> m_nFilesDone; /**< Files fully processed so far */
	std::atomic<bool> m_bStop; /**< Stop requested (Stop button, close, or destroy) */
	std::atomic<bool> m_bRunning; /**< True while the worker runs */
	std::unique_ptr<Poco::ThreadPool> m_pPool; /**< Single-worker pool (DirScan precedent) */
	std::shared_ptr<Merge20BulkClassifyWorker> m_pWorker; /**< Running bulk worker, or nullptr */
};
