// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20ImpactSearch.h
 *
 * @brief Background impact search (Merge20 best-workflow B5, PR1).
 *
 * The engine (Merge20ImpactCollector) is pure; this class is the
 * thin MFC shell that runs it off the UI thread, following the D7
 * Grep worker exactly (Merge20GrepDlg.cpp): one Poco ThreadPool
 * worker on purpose, so hits arrive in folder-list order and the
 * 80-hit cutoff is exact; on-disk file text read with the same
 * encoding detection the compare uses (codepage_detect::Guess,
 * then UniMemFile + ReadString, the CDiffTextBuffer::LoadFromFile
 * sequence without unpacker or prediffer); binary files skipped.
 * The same consequences as D7 apply (NOTES section 33): the
 * search reads on-disk text, not unpacker/prediffer-processed
 * text, and the diff index is resolved at click time from the
 * opened compare document (PR2 reuses the D7 jump route).
 *
 * The search is started for one judged diff: the caller passes
 * the folder file list (CDirView::Merge20BuildGrepFiles, the D7
 * list), the changed symbols (Merge20ImpactSymbolsFromClassify
 * of the diff's S4 classification) and the judged file's
 * relative path (self-file exclusion). The worker never touches
 * a CWnd or a CMergeDoc, and it never blocks the UI thread: the
 * user can keep judging diffs while it runs (acceptance (e)).
 *
 * Thread contract (same shape as the D7 worker interface): the
 * sink callbacks run on the worker thread. The sink must not
 * touch a window from them; it forwards hits to the UI thread
 * (e.g. PostMessage, as CMerge20GrepDlg::AddPendingHit does) and
 * only touches windows there. The sink must outlive the search,
 * or call Stop() before it is destroyed; the destructor stops
 * and joins the worker.
 *
 * This class owns no display: PR2 (FocusDock / Sniper) implements
 * the sink, shows the hits and wires the jump. FORM comparison
 * never starts a search (FORM is visualisation-only).
 */
#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include "Merge20/Merge20Grep.h"
#include "Merge20/Merge20Impact.h"

class Merge20ImpactSearchWorker;
namespace Poco { class ThreadPool; }

/**
 * @brief Sink for impact-search results (implemented by PR2).
 * All methods run on the worker thread: forward to the UI thread
 * before touching any window.
 */
struct IMerge20ImpactSearchSink
{
	virtual ~IMerge20ImpactSearchSink() = default;
	/** @brief One hit found (worker thread; hits arrive in scan order). */
	virtual void OnMerge20ImpactHit(const Merge20ImpactHit& hit) = 0;
	/** @brief The scan finished (all files, stopped, or hit the limit). Always called exactly once per Start. */
	virtual void OnMerge20ImpactFinished(const Merge20ImpactResult& result) = 0;
};

class CMerge20ImpactSearch
{
public:
	CMerge20ImpactSearch();
	~CMerge20ImpactSearch();

	/**
	 * @brief Start a background search for one judged diff.
	 * A running search is stopped (joined) first, so the new run
	 * starts clean. With no symbols to search for, no worker is
	 * started: the sink is told at once that the search finished
	 * with an empty result (symbols empty means the diff changed
	 * no symbol S4 could name).
	 * @param [in] files Folder file list (CDirView::Merge20BuildGrepFiles).
	 * @param [in] symbols Changed symbols (Merge20ImpactSymbolsFromClassify).
	 * @param [in] currentRelPath Relative path of the judged file (self-file exclusion).
	 * @param [in] guessEncodingType OPT_CP_DETECT value, read on the UI thread.
	 * @param [in] pSink Result sink (must outlive the search; may be null, in which case hits are counted but not delivered).
	 */
	void Start(std::vector<Merge20GrepFile> files, std::vector<String> symbols,
		String currentRelPath, int guessEncodingType, IMerge20ImpactSearchSink* pSink);
	/** @brief Stop a running search and wait for the worker to end (idempotent). */
	void Stop();
	/** @brief True while a scan is running. */
	bool IsRunning() const { return m_bRunning.load(); }

	// Worker-thread interface (called from Merge20ImpactSearchWorker only).
	/** @brief Stop requested, or the 80-hit limit reached. */
	bool ShouldStop() const;
	/**
	 * @brief Feed one file line to the collector and forward any
	 * new hits to the sink. Returns false when the search is full
	 * or stopping (the worker stops scanning that file).
	 */
	bool ScanLine(const String& relPath, int pane, int line, const String& text);
	/** @brief Files fully scanned so far (for the progress status). */
	void IncrementFilesScanned() { ++m_nFilesScanned; }
	/** @brief The worker finished (all files, stopped, or hit the limit). */
	void NotifyScanFinished();
	/** @brief Hits found so far. */
	int GetHitCount() const { return m_nHitCount.load(); }
	/** @brief Files fully scanned so far. */
	int GetFilesScanned() const { return m_nFilesScanned.load(); }
	/** @brief Files in the search scope. */
	int GetFilesTotal() const { return m_nFilesTotal; }

private:
	std::vector<Merge20GrepFile> m_files; /**< Files to scan (caller's copies). */
	Merge20ImpactCollector m_collector; /**< Pure collector (worker thread only). */
	IMerge20ImpactSearchSink* m_pSink; /**< Result sink, or nullptr. */
	int m_nGuessEncodingType; /**< OPT_CP_DETECT value read on the UI thread. */
	int m_nFilesTotal; /**< Files to scan (for the progress status). */
	std::atomic<int> m_nFilesScanned; /**< Files fully scanned so far. */
	std::atomic<int> m_nHitCount; /**< Hits found so far (atomic: the UI thread reads it while the worker writes). */
	std::atomic<bool> m_bStop; /**< Stop requested (Stop, or destruction). */
	std::atomic<bool> m_bRunning; /**< True while the worker runs. */
	std::unique_ptr<Poco::ThreadPool> m_pPool; /**< Single-worker pool (D7 precedent). */
	std::shared_ptr<Merge20ImpactSearchWorker> m_pWorker; /**< Running scan worker, or nullptr. */
};
