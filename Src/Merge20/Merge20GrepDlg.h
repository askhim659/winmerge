// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20GrepDlg.h
 *
 * @brief Cross-file search (Grep) dialog (Merge20 Phase 4 D7, design
 * sections 3.7, 4.3 and 5.2/D7, spec sections 3 (4/4) and 4(g)).
 *
 * A modeless dialog: it stays open while the user opens a file
 * compare from a hit and comes back to the result list afterwards
 * (design section 4.3; the HTML Grep modal closes on jump, the
 * fork keeps the list). It is owned by the folder view
 * (CDirView::m_wndGrep, the D6 frame-member pattern applied to the
 * folder view): created once, shown on demand and hidden on close,
 * never deleted on close, so no PostNcDestroy self-delete is needed.
 *
 * The scan runs on a background worker (DirScan's Poco ThreadPool
 * precedent, DirScan.cpp:480): one worker, so hits arrive in
 * folder-list order and the 80-hit cutoff is exact. The worker
 * reads on-disk file text (codepage_detect::Guess + UniMemFile,
 * the CDiffTextBuffer::LoadFromFile sequence without unpacker or
 * prediffer) and posts hits to this dialog with PostMessage; the
 * dialog drains them into the list control on the UI thread.
 * The diff index is resolved at click time from the opened compare
 * document (CDirView::Merge20OpenFileAtLine), not during the scan.
 */
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include "TrDialogs.h"
#include "Merge20/Merge20Grep.h"
#include "resource.h"

class CDirView;
class Merge20GrepWorker;
namespace Poco { class ThreadPool; }

/** @brief Worker-to-dialog progress message (MainFrm uses WM_USER + 1 for its own window only). */
#define WM_MERGE20_GREP_PROGRESS (WM_USER + 100)

class CMerge20GrepDlg : public CTrDialog
{
	DECLARE_DYNAMIC(CMerge20GrepDlg);
public:
	CMerge20GrepDlg();
	virtual ~CMerge20GrepDlg();

	BOOL Create(CWnd* pParentWnd);
	/**
	 * @brief Show the dialog for the given folder view (rebuilding
	 * the scan file list and the history combo), or bring it to
	 * the front when it is already open.
	 */
	void ShowGrep(CDirView* pDirView);
	/** @brief Hide the dialog and stop a running scan. */
	void CloseGrep();
	/** @brief True while the dialog is shown. */
	bool IsOpen() const { return m_hWnd != nullptr && !!IsWindowVisible(); }
	/** @brief Stop a running scan and wait for the worker to end (idempotent). */
	void StopScan();
	/** @brief True while a scan is running. */
	bool IsScanning() const { return m_bRunning.load(); }

	// Worker-thread interface (called from Merge20GrepWorker only).
	/** @brief Append one hit (worker thread): counts it and asks the UI thread to display it. */
	void AddPendingHit(const Merge20GrepHit& hit);
	/** @brief The worker finished (all files, stopped, or hit the limit). */
	void NotifyScanFinished();
	/** @brief Stop requested, or the 80-hit limit reached. */
	bool ShouldStop() const;
	/** @brief Hits appended so far. */
	int GetHitCount() const { return m_nHitCount.load(); }
	/** @brief Files fully scanned so far (for the progress status). */
	void IncrementFilesScanned() { m_nFilesScanned++; }
	int GetFilesScanned() const { return m_nFilesScanned.load(); }
	int GetFilesTotal() const { return m_nFilesTotal; }

protected:
	virtual BOOL OnInitDialog() override;
	virtual void OnOK() override;
	virtual void OnCancel() override;
	virtual void OnDestroy() override;
	virtual BOOL PreTranslateMessage(MSG* pMsg) override;

	//{{AFX_MSG(CMerge20GrepDlg)
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnButtonClicked(UINT nID);
	afx_msg void OnClickResult(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnGrepProgress(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void LayoutAll();
	void StartSearch();
	void UpdateStatus();
	void DrainPendingHits();
	void RefreshHistoryCombo();
	/** @brief Pane label for a hit (reuses the D6 "Old" / "New side" strings). */
	String PaneLabel(int pane, int nPanes) const;

	CComboBox m_cbQuery;
	CButton m_btnSearch;
	CButton m_btnStop;
	CButton m_btnClose;
	CStatic m_stLabel;
	CStatic m_stStatus;
	CListCtrl m_list;

	CDirView* m_pDirView; /**< Folder view that owns this dialog, or nullptr */
	std::vector<Merge20GrepFile> m_files; /**< Files to scan (built by ShowGrep) */
	std::vector<Merge20GrepHit> m_hits; /**< Hits shown in the list, in scan order */
	String m_queryLower; /**< Lower-cased query of the running scan */
	int m_nGuessEncodingType; /**< OPT_CP_DETECT value read on the UI thread */
	int m_nFilesTotal; /**< Files to scan (for the progress status) */
	int m_nPanes; /**< Compared panes (2 or 3; pane labels depend on it) */
	bool m_bStoppedByUser; /**< Stop button pressed (status text after a stop) */

	std::mutex m_mutex; /**< Guards m_pendingHits */
	std::vector<Merge20GrepHit> m_pendingHits; /**< Hits posted by the worker, not yet displayed */
	std::atomic<int> m_nHitCount; /**< Hits appended so far */
	std::atomic<int> m_nFilesScanned; /**< Files fully scanned so far */
	std::atomic<bool> m_bStop; /**< Stop requested (Stop button, close, or destroy) */
	std::atomic<bool> m_bRunning; /**< True while the worker runs */
	std::unique_ptr<Poco::ThreadPool> m_pPool; /**< Single-worker pool (DirScan precedent) */
	std::shared_ptr<Merge20GrepWorker> m_pWorker; /**< Running scan worker, or nullptr */
};
