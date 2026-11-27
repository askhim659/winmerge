// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCompareDoc.h
 *
 * @brief FORM/XML comparison (Wave 5 F2): the FORM compare
 * document (design merge20-form-compare-design.md sections 3.2,
 * 4.3, 7.1 and 10 F2).
 *
 * CFormCompareDoc holds the two folder paths, the scan + diff
 * results (changed files only; identical files are counted, never
 * listed, design section 2.1), the KPI numbers and the list state
 * (filter tab, search text, sort order). It derives from CDocument
 * directly, not from CMergeDoc (design section 3.2): the merge
 * engine, the result pane, learning and session saving must never
 * apply to this view-only mode.
 *
 * The folder scan is recursive and takes `.xml` files only
 * (case-insensitive extension). Each pair is diffed with the F0
 * parser and the F1 matching (Merge20DiffFormFiles); the
 * large-change flag is applied over the whole result
 * (Merge20ApplyLargeChangeFlag) before the KPI is computed.
 * Per-file extras the list module does not own (the F1 groups and
 * the ignored-noise count) are kept in vectors aligned with the
 * results, for the F3/F5 card views.
 */
#pragma once

#include <vector>
#include "UnicodeString.h"
#include "Merge20/Merge20FormXml.h"
#include "Merge20/Merge20FormDiff.h"
#include "Merge20/Merge20FormList.h"

/**
 * @brief FORM compare document (view-only, design section 3.2).
 */
class CFormCompareDoc : public CDocument
{
	DECLARE_DYNCREATE(CFormCompareDoc)
protected:
	CFormCompareDoc();
public:
	virtual ~CFormCompareDoc();

	/** @brief Folders to compare (old = locally collected, new = package). */
	void SetFolders(const String& leftFolder, const String& rightFolder);
	/** @brief Old folder (locally collected). Empty before SetFolders. */
	const String& GetLeftFolder() const { return m_strLeftFolder; }
	/** @brief New folder (package). Empty before SetFolders. */
	const String& GetRightFolder() const { return m_strRightFolder; }
	/** @brief True when both folders are set. */
	bool HasFolders() const { return !m_strLeftFolder.empty() && !m_strRightFolder.empty(); }

	/**
	 * @brief Scan both folders (.xml only, recursive) and diff
	 * every file pair (F0 parser + F1 matching). Runs on the UI
	 * thread; a progress bar is follow-up work (see the F2 PR
	 * description), so a very large folder blocks the UI while
	 * it runs.
	 * @return False when a folder cannot be read at all (the
	 * document then stays unanalyzed).
	 */
	bool RunAnalysis();

	/** @brief True once RunAnalysis has completed. */
	bool IsAnalyzed() const { return m_bAnalyzed; }
	/** @brief Changed files only, in file-name order. */
	const std::vector<Merge20FormFileResult>& GetResults() const { return m_results; }
	/** @brief F1 groups per result, aligned with GetResults(). */
	const std::vector<std::vector<Merge20FormDiffGroup>>& GetGroups() const { return m_groups; }
	/** @brief Ignored-noise count (design section 5.2) of one result. */
	int GetIgnoredNoiseCount(size_t index) const;
	/** @brief The seven KPI numbers (HTML `updateKpiBadges`). */
	Merge20FormKpi GetKpi() const { return m_kpi; }
	/** @brief All .xml files scanned (both folders' union). */
	int GetTotalFiles() const { return m_totalFiles; }
	/** @brief Identical files (counted, never listed). */
	int GetMatchedFiles() const { return m_kpi.matched; }

	// List state (filter / search / sort). Each setter refreshes
	// the visible list and notifies the views.
	/** @brief Selected filter tab. */
	Merge20FormFilter GetFilter() const { return m_filter; }
	/** @brief Select a filter tab. */
	void SetFilter(Merge20FormFilter filter);
	/** @brief Search text as typed. */
	const String& GetSearchText() const { return m_strSearchText; }
	/** @brief Set the search text (lowered once, stored with it). */
	void SetSearchText(const String& text);
	/** @brief Selected sort order. */
	Merge20FormSort GetSortOrder() const { return m_sortOrder; }
	/** @brief Select a sort order. */
	void SetSortOrder(Merge20FormSort sort);
	/** @brief Visible result indices (into GetResults()), in display order. */
	const std::vector<size_t>& GetVisibleIndices() const { return m_visibleIndices; }

	/** @brief Recompute the visible list from the current state. */
	void RefreshVisible();

	/** @brief Re-apply _() texts after a GUI language change. */
	void UpdateResources();

	/**
	 * @brief Re-resolve every result's form name from the
	 * name-master file (F6, design section 8). Called after the
	 * master is edited and saved; reloads the file (the default
	 * list when the file is missing or unparsable) and refreshes
	 * the visible list and the views.
	 */
	void RefreshFormNames();

protected:
	virtual BOOL OnNewDocument() override;
	virtual void DeleteContents() override;

private:
	void RefreshKpi();

	String m_strLeftFolder; /**< Old folder (locally collected). */
	String m_strRightFolder; /**< New folder (package). */
	bool m_bAnalyzed = false; /**< True once RunAnalysis completed. */
	int m_totalFiles = 0; /**< All .xml files scanned (both folders' union). */
	std::vector<Merge20FormFileResult> m_results; /**< Changed files only, file-name order. */
	std::vector<std::vector<Merge20FormDiffGroup>> m_groups; /**< F1 groups, aligned with m_results. */
	std::vector<int> m_ignoredNoise; /**< Ignored-noise counts, aligned with m_results. */
	Merge20FormKpi m_kpi; /**< The seven KPI numbers. */
	Merge20FormFilter m_filter = Merge20FormFilter::All; /**< Selected filter tab. */
	String m_strSearchText; /**< Search text as typed. */
	std::wstring m_strSearchLower; /**< Lowered search text. */
	Merge20FormSort m_sortOrder = Merge20FormSort::FileName; /**< Selected sort order. */
	std::vector<size_t> m_visibleIndices; /**< Visible result indices, display order. */

	DECLARE_MESSAGE_MAP()
};

/**
 * @brief Open a FORM compare window for two folders (view-only).
 * Creates the document through the application's FORM compare
 * template, hands it the folders and runs the analysis.
 * @param [in] leftFolder Old folder (locally collected).
 * @param [in] rightFolder New folder (package).
 * @return The document, or nullptr when the window could not be created.
 */
CFormCompareDoc* Merge20FormCompareOpenFolders(const String& leftFolder, const String& rightFolder);
