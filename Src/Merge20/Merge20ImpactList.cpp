// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20ImpactList.cpp
 *
 * @brief Implementation of the "uses in other files" list
 * (Merge20 best-workflow B5, PR2).
 *
 * The list is the display half of PR1's engine: the owner (focus
 * dock or sniper overlay) calls StartForDiff whenever the shown
 * diff changes, and the list starts CMerge20ImpactSearch for that
 * diff's changed symbols (Merge20ClassifyOneDiffResult of the
 * diff, i.e. the S4 functions; the symbols are exactly what S4
 * extracted, no accuracy work here). The file list comes from
 * CDirView::Merge20BuildGrepFiles (the D7 list, via the folder
 * document's main view, the P3 route); the judged file's relative
 * path comes from CDirDoc::Merge20FindRelPath (self-file
 * exclusion). The encoding guess is OPT_CP_DETECT, read on the UI
 * thread. A diff change restarts the search (Start joins the
 * previous worker first), so the list never shows another diff's
 * hits; destroying the list stops the worker before the window
 * dies, so no PostMessage can land on a dead HWND.
 *
 * The status line follows the D7 Grep dialog: "Searching... N
 * hits (M of K files)" while running, "Limit reached (80 items)"
 * at the cutoff, "N hits" when finished, and "No uses in other
 * files" when the search finished with no hits (acceptance (b)).
 * The estimate note is a permanent label above the list: every
 * result is a text-search estimate, never a precise reference
 * analysis (acceptance (d)). Placeholders reuse existing msgids
 * ("No merge session", "No difference selected", "No folder
 * comparison"); only the B5-specific strings are new.
 *
 * This file stays ASCII: the Japanese texts live in the .po file.
 */
#include "StdAfx.h"
#include "Merge20/Merge20ImpactList.h"
#include "Merge20/Merge20ImpactView.h"
#include "Merge20/Merge20ClassifyDoc.h"
#include "Merge20/Merge20Impact.h"
#include "DirDoc.h"
#include "DirView.h"
#include "MergeDoc.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"
#include "UnicodeString.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20ImpactList, CWnd)

BEGIN_MESSAGE_MAP(CMerge20ImpactList, CWnd)
	//{{AFX_MSG_MAP(CMerge20ImpactList)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_NOTIFY(NM_CLICK, IDC_MERGE20_IMPACT_LIST, OnClickResult)
	ON_MESSAGE(WM_MERGE20_IMPACT_PROGRESS, OnImpactProgress)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

namespace
{
	/** @brief OnImpactProgress wParam: more hits arrived (drain them). */
	constexpr WPARAM ImpactProgress = 0;
	/** @brief OnImpactProgress wParam: the worker finished. */
	constexpr WPARAM ImpactFinished = 1;
	/** @brief Title row height in pixels. */
	constexpr int ImpactTitleHeight = 20;
	/** @brief Estimate-note row height in pixels. */
	constexpr int ImpactNoteHeight = 16;
	/** @brief Status row height in pixels. */
	constexpr int ImpactStatusHeight = 16;
	/** @brief Margins and row gaps in pixels. */
	constexpr int ImpactMargin = 4;
	constexpr int ImpactRowGap = 2;
	/** @brief Column widths in pixels (Text takes the rest). */
	constexpr int ImpactSymbolWidth = 140;
	constexpr int ImpactFileWidth = 280;
	constexpr int ImpactPaneWidth = 70;
	constexpr int ImpactLineWidth = 60;
}

CMerge20ImpactList::CMerge20ImpactList()
	: m_pDirView(nullptr)
	, m_nPanes(2)
	, m_placeholder(Placeholder::NoSession)
	, m_bHasFinished(false)
	, m_nHitCount(0)
{
}

CMerge20ImpactList::~CMerge20ImpactList()
{
	// Belt and braces: OnDestroy already stopped the search, but
	// the list object may outlive its window (member of the dock
	// or the sniper dialog). The worker must be joined before the
	// sink (this) goes away.
	StopSearch();
}

BOOL CMerge20ImpactList::Create(CWnd* pParentWnd)
{
	CString wndclass = ::AfxRegisterWndClass(CS_DBLCLKS,
		::LoadCursor(nullptr, IDC_ARROW),
		::GetSysColorBrush(COLOR_BTNFACE), 0);
	DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
	if (!CWnd::Create(wndclass, _T("Merge20ImpactList"), dwStyle,
		CRect(0, 0, 0, 0), pParentWnd, 0))
		return FALSE;
	return TRUE;
}

int CMerge20ImpactList::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	CFont* pFont = CFont::FromHandle(static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT)));
	const DWORD dwLabelStyle = WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP | SS_ENDELLIPSIS | SS_CENTERIMAGE;
	if (!m_stTitle.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_IMPACT_TITLE))
		return -1;
	m_stTitle.SetFont(pFont);
	if (!m_stNote.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_IMPACT_NOTE))
		return -1;
	m_stNote.SetFont(pFont);
	if (!m_stStatus.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_IMPACT_STATUS))
		return -1;
	m_stStatus.SetFont(pFont);

	const DWORD dwListStyle = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
	if (!m_list.Create(dwListStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_IMPACT_LIST))
		return -1;
	m_list.SetFont(pFont);
	m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	UpdateResources();
	LayoutAll();
	return 0;
}

void CMerge20ImpactList::StartForDiff(CMergeDoc* pDoc, int nDiff)
{
	// The list control needs its window (the list rows and the
	// status live there): a ShowDiff before creation is a no-op,
	// the next ShowDiff starts the search.
	if (GetSafeHwnd() == nullptr)
		return;
	// A previous search (finished or still running) is joined
	// here so the new run starts clean and the list never shows
	// another diff's hits.
	StopSearch();
	m_list.DeleteAllItems();
	m_hits.clear();
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_pendingHits.clear();
		m_bHasFinished = false;
	}
	m_nHitCount = 0;
	m_pDirView = nullptr;
	m_placeholder = Placeholder::None;

	if (pDoc == nullptr || !pDoc->GetMergeResultBuildState())
	{
		m_placeholder = Placeholder::NoSession;
		UpdateStatus();
		return;
	}
	if (nDiff < 0)
	{
		m_placeholder = Placeholder::NoDiff;
		UpdateStatus();
		return;
	}
	// Folder compare, through the same route as the P3 panel:
	// the merge doc's folder document, then its main view for
	// the file list and the jump. A file compare opened on its
	// own has no folder document, so no search can run.
	CDirDoc* pDirDoc = dynamic_cast<CDirDoc*>(pDoc->GetDirDoc());
	if (pDirDoc == nullptr || pDirDoc->GetMainView() == nullptr)
	{
		m_placeholder = Placeholder::NoFolder;
		UpdateStatus();
		return;
	}
	m_pDirView = pDirDoc->GetMainView();
	m_nPanes = pDoc->m_nBuffers;
	String sRelPath;
	pDirDoc->Merge20FindRelPath(pDoc->m_filePaths, sRelPath);
	const std::vector<String> symbols = Merge20ImpactSymbolsFromClassify(
		Merge20ClassifyOneDiffResult(*pDoc, nDiff,
			Merge20ParseRiskKeywords(GetOptionsMgr()->GetString(OPT_MERGE20_RISK_KEYWORDS))));
	if (symbols.empty())
	{
		// The diff changed no symbol S4 could name: nothing to
		// search for, and no worker is started (PR1 rule).
		m_placeholder = Placeholder::NoSymbols;
		UpdateStatus();
		return;
	}
	// The name avoids C4459: a global named `files` exists, and
	// the D7/D10 builders already trigger the same warning for
	// their own local of that name.
	const std::vector<Merge20GrepFile> grepFiles = m_pDirView->Merge20BuildGrepFiles();
	m_search.Start(grepFiles, symbols, sRelPath,
		GetOptionsMgr()->GetInt(OPT_CP_DETECT), this);
	UpdateStatus();
}

void CMerge20ImpactList::StopSearch()
{
	// UI thread only: joining from the worker itself would deadlock.
	m_search.Stop();
}

void CMerge20ImpactList::UpdateResources()
{
	if (m_stTitle.GetSafeHwnd() == nullptr)
		return;
	m_stTitle.SetWindowText(_("Uses in other files").c_str());
	// The note msgid is the PR1 constant; the Japanese rendering
	// is the fixed text in Merge20Impact.h / Japanese.po.
	m_stNote.SetWindowText(_(Merge20ImpactEstimateNoteMsgId).c_str());
	SetColumns();
	UpdateStatus();
}

void CMerge20ImpactList::OnMerge20ImpactHit(const Merge20ImpactHit& hit)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_pendingHits.push_back(hit);
	}
	++m_nHitCount;
	if (m_hWnd != nullptr)
		PostMessage(WM_MERGE20_IMPACT_PROGRESS, ImpactProgress, 0);
}

void CMerge20ImpactList::OnMerge20ImpactFinished(const Merge20ImpactResult& result)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_finishedResult = result;
		m_bHasFinished = true;
	}
	if (m_hWnd != nullptr)
		PostMessage(WM_MERGE20_IMPACT_PROGRESS, ImpactFinished, 0);
}

void CMerge20ImpactList::OnDestroy()
{
	// The worker posts to this HWND: join it before the window dies.
	StopSearch();
	CWnd::OnDestroy();
}

void CMerge20ImpactList::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	LayoutAll();
}

void CMerge20ImpactList::OnClickResult(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	if (m_pDirView == nullptr)
		return;
	const NMITEMACTIVATE* pNM = reinterpret_cast<NMITEMACTIVATE*>(pNMHDR);
	if (pNM->iItem < 0 || pNM->iItem >= static_cast<int>(m_hits.size()))
		return;
	// The jump itself lives in the view (it owns the selection
	// and the compare opening); the list only knows the hit.
	// The Merge20ImpactHit overload resolves the diff at click
	// time from the opened compare document (D7 route).
	m_pDirView->Merge20OpenFileAtLine(m_hits[static_cast<size_t>(pNM->iItem)]);
}

LRESULT CMerge20ImpactList::OnImpactProgress(WPARAM wParam, LPARAM /*lParam*/)
{
	DrainPendingHits();
	if (wParam == ImpactFinished)
	{
		// The worker is done: the finished result is now the
		// status source (limit / hit count / no uses).
	}
	UpdateStatus();
	return 0;
}

void CMerge20ImpactList::LayoutAll()
{
	if (m_hWnd == nullptr || m_stTitle.GetSafeHwnd() == nullptr)
		return;
	CRect rc;
	GetClientRect(&rc);
	const int cx = rc.Width();
	const int cy = rc.Height();
	if (cx < 100 || cy < 60)
		return;

	int y = ImpactMargin;
	m_stTitle.MoveWindow(ImpactMargin, y, cx - 2 * ImpactMargin, ImpactTitleHeight, TRUE);
	y += ImpactTitleHeight + ImpactRowGap;
	m_stNote.MoveWindow(ImpactMargin, y, cx - 2 * ImpactMargin, ImpactNoteHeight, TRUE);
	y += ImpactNoteHeight + ImpactRowGap;
	const int yStatus = cy - ImpactMargin - ImpactStatusHeight;
	const int listBottom = yStatus - ImpactRowGap;
	m_list.MoveWindow(ImpactMargin, y, cx - 2 * ImpactMargin, (std::max)(20, listBottom - y), TRUE);
	m_stStatus.MoveWindow(ImpactMargin, yStatus, cx - 2 * ImpactMargin, ImpactStatusHeight, TRUE);

	// The excerpt column takes the remaining width.
	CRect rcList;
	m_list.GetClientRect(&rcList);
	const int fixed = ImpactSymbolWidth + ImpactFileWidth + ImpactPaneWidth + ImpactLineWidth;
	m_list.SetColumnWidth(4, (std::max)(80, rcList.Width() - fixed - 4));
}

void CMerge20ImpactList::SetColumns()
{
	if (m_list.GetSafeHwnd() == nullptr)
		return;
	// Rebuild the headers (language switch): the rows are kept,
	// only the header texts change.
	while (m_list.DeleteColumn(0))
		;
	m_list.InsertColumn(0, _("Symbol").c_str(), LVCFMT_LEFT, ImpactSymbolWidth);
	m_list.InsertColumn(1, _("File").c_str(), LVCFMT_LEFT, ImpactFileWidth);
	m_list.InsertColumn(2, _("Pane").c_str(), LVCFMT_LEFT, ImpactPaneWidth);
	m_list.InsertColumn(3, _("Line").c_str(), LVCFMT_LEFT, ImpactLineWidth);
	m_list.InsertColumn(4, _("Text").c_str(), LVCFMT_LEFT, 200);
}

void CMerge20ImpactList::DrainPendingHits()
{
	std::vector<Merge20ImpactHit> pending;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		pending.swap(m_pendingHits);
	}
	for (const Merge20ImpactHit& hit : pending)
	{
		const Merge20ImpactViewRow row = Merge20ImpactViewRowFromHit(hit);
		const int nRow = m_list.GetItemCount();
		m_hits.push_back(hit);
		m_list.InsertItem(nRow, row.symbol.c_str());
		m_list.SetItemText(nRow, 1, row.relPath.c_str());
		m_list.SetItemText(nRow, 2, PaneLabel(row.pane).c_str());
		m_list.SetItemText(nRow, 3, row.lineNumber.c_str());
		m_list.SetItemText(nRow, 4, row.excerpt.c_str());
	}
}

void CMerge20ImpactList::UpdateStatus()
{
	if (m_stStatus.GetSafeHwnd() == nullptr)
		return;
	String sText;
	if (m_search.IsRunning())
	{
		sText = strutils::format_string3(
			_("Searching... %1 hits (%2 of %3 files)"),
			strutils::to_str(m_nHitCount.load()),
			strutils::to_str(m_search.GetFilesScanned()),
			strutils::to_str(m_search.GetFilesTotal()));
	}
	else if (m_bHasFinished.load())
	{
		if (m_finishedResult.limitReached)
		{
			sText = _("Limit reached (80 items)");
		}
		else if (m_hits.empty())
		{
			// Acceptance (b): no use in other files is stated
			// explicitly, never left as an empty list.
			sText = _("No uses in other files");
		}
		else
		{
			sText = strutils::format_string1(
				_("%1 hits"), strutils::to_str(m_nHitCount.load()));
		}
	}
	else
	{
		switch (m_placeholder)
		{
		case Placeholder::NoSession:
			sText = _("No merge session");
			break;
		case Placeholder::NoDiff:
			sText = _("No difference selected");
			break;
		case Placeholder::NoFolder:
			sText = _("No folder comparison");
			break;
		case Placeholder::NoSymbols:
			sText = _("No symbol to search for in this diff");
			break;
		case Placeholder::None:
		default:
			break;
		}
	}
	m_stStatus.SetWindowText(sText.c_str());
}

String CMerge20ImpactList::PaneLabel(int pane) const
{
	if (pane == 0)
		return _("Old");
	if (pane == m_nPanes - 1)
		return _("New side");
	return _("Middle");
}
