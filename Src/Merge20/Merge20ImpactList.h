// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20ImpactList.h
 *
 * @brief Declaration of CMerge20ImpactList class
 *
 * Merge20 (best-workflow B5, PR2: display + jump wiring): the
 * "uses in other files" list, shared by the focus dock (D4) and
 * the sniper overlay (D6). A plain CWnd holding a title label,
 * the estimate note, a report list and a status label. The owner
 * (dock or sniper) calls StartForDiff whenever the shown diff
 * changes; the list then starts a background impact search for
 * that diff (PR1 engine) and shows the hits as they arrive.
 *
 * The list owns its CMerge20ImpactSearch and implements the sink:
 * the sink callbacks run on the worker thread, so they only queue
 * the hits under a mutex and PostMessage to the UI thread (the
 * D7 Grep dialog precedent). The UI thread drains the queue into
 * the list control. A row click jumps through the D7 route
 * (CDirView::Merge20OpenFileAtLine, the Merge20ImpactHit
 * overload): the diff index is resolved at click time from the
 * opened compare document.
 *
 * The search never blocks the UI thread: judging a diff while a
 * search runs only starts a new search (the old worker is joined
 * inside Start), and the judgement buttons stay live throughout
 * (acceptance (e)). The search reads on-disk text (PR1), never a
 * CWnd or a CMergeDoc, and it never runs for FORM comparison:
 * this control only exists in the file-compare frame (the focus
 * dock and the sniper overlay), never in the FORM frame.
 */
#pragma once

#include <atomic>
#include <mutex>
#include <vector>
#include "Merge20/Merge20Impact.h"
#include "Merge20/Merge20ImpactSearch.h"
#include "resource.h"

class CMergeDoc;
class CDirView;

/** @brief Worker-to-list progress message (Grep uses WM_USER + 100). */
#define WM_MERGE20_IMPACT_PROGRESS (WM_USER + 101)

class CMerge20ImpactList : public CWnd, public IMerge20ImpactSearchSink
{
	DECLARE_DYNAMIC(CMerge20ImpactList);
public:
	CMerge20ImpactList();
	virtual ~CMerge20ImpactList();
	virtual BOOL Create(CWnd* pParentWnd);

	/**
	 * @brief Start the impact search for the given diff of the
	 * given doc (or show the placeholder status when no search
	 * can run: no merge session, no diff selected, no folder
	 * compare, or no symbol S4 could name for this diff). A
	 * running search is stopped first; calling this again for the
	 * next diff restarts the search for that diff.
	 */
	void StartForDiff(CMergeDoc* pDoc, int nDiff);
	/** @brief Stop a running search and wait for the worker to end (idempotent). */
	void StopSearch();
	/** @brief Re-apply the translated texts (title, note, column headers, status). */
	void UpdateResources();

	// IMerge20ImpactSearchSink (worker thread: queue + PostMessage only).
	void OnMerge20ImpactHit(const Merge20ImpactHit& hit) override;
	void OnMerge20ImpactFinished(const Merge20ImpactResult& result) override;

	//{{AFX_MSG(CMerge20ImpactList)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
	afx_msg void OnClickResult(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg LRESULT OnImpactProgress(WPARAM wParam, LPARAM lParam);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	/** @brief Placeholder status shown when no search runs. */
	enum class Placeholder
	{
		None,      /**< A search is running or finished. */
		NoSession, /**< No merge session (or no document). */
		NoDiff,    /**< No difference selected. */
		NoFolder,  /**< No folder comparison (or its view is gone). */
		NoSymbols, /**< The diff changed no symbol S4 could name. */
	};

	void LayoutAll();
	void UpdateStatus();
	void DrainPendingHits();
	void SetColumns();
	/** @brief Pane label for a hit (reuses the D7 "Old" / "New side" strings). */
	String PaneLabel(int pane) const;

	CStatic m_stTitle;  /**< "Uses in other files" title */
	CStatic m_stNote;   /**< Text-search estimate note (always shown) */
	CStatic m_stStatus; /**< Search progress / result count / placeholder */
	CListCtrl m_list;   /**< Hits: Symbol / File / Pane / Line / Text */

	CDirView* m_pDirView; /**< Folder view for the jump, or nullptr */
	std::vector<Merge20ImpactHit> m_hits; /**< Hits shown in the list, in scan order */
	int m_nPanes; /**< Compared panes (2 or 3; pane labels depend on it) */
	Placeholder m_placeholder; /**< Placeholder status, or None while searching/finished */

	CMerge20ImpactSearch m_search; /**< Background search (PR1 engine) */
	std::mutex m_mutex; /**< Guards m_pendingHits and the finished result */
	std::vector<Merge20ImpactHit> m_pendingHits; /**< Hits posted by the worker, not yet displayed */
	Merge20ImpactResult m_finishedResult; /**< Last finished result (UI thread reads it) */
	std::atomic<bool> m_bHasFinished; /**< True once the current search finished */
	std::atomic<int> m_nHitCount; /**< Hits found so far (worker writes, UI reads) */
};
