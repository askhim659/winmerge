// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20GrepDlg.cpp
 *
 * @brief Implementation of the Grep dialog (Merge20 Phase 4 D7).
 *
 * T-4-9 (design section 1.4): how the target text of unopened files
 * is read. There is no text cache for unopened files: CDiffContext
 * holds compare results (DIFFITEMs), not file text, and opening a
 * CMergeDoc per file would create views/documents and cannot run on
 * a background thread. The scan therefore reads the on-disk file
 * text directly on the worker thread, with the same encoding
 * detection the compare uses (codepage_detect::Guess, then
 * UniMemFile + ReadString: the CDiffTextBuffer::LoadFromFile
 * sequence without unpacker or prediffer). Consequence, recorded
 * in MERGE20_NOTES.md section 33: Grep searches on-disk text, not
 * unpacker/prediffer-processed text, and binary files are skipped.
 * The diff index is resolved at click time from the opened compare
 * document (CDirView::Merge20OpenFileAtLine), so the scan stays
 * I/O-only and fast.
 *
 * Background scan (design section 3.7): DirScan's Poco ThreadPool
 * precedent (DirScan.cpp:480), with a single worker on purpose:
 * hits must arrive in folder-list order (HTML parity) and the
 * 80-hit cutoff must be exact; parallel workers would reorder hits
 * and race the cutoff. The UI stays responsive because all file
 * I/O runs off the UI thread; hits appear live in the list.
 * The worker calls CoInitializeEx first: the MLang detector behind
 * codepage_detect::Guess is COM-based. If COM init fails, the scan
 * still runs (Guess falls back to the default codepage).
 */
#include "StdAfx.h"
#include "Merge20/Merge20GrepDlg.h"
#include "DirView.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"
#include "UnicodeString.h"
#include "codepage_detect.h"
#include "UniFile.h"
#include <Poco/ThreadPool.h>
#include <Poco/Runnable.h>
#include <objbase.h>
#include <algorithm>
#include <cstring>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20GrepDlg, CTrDialog)

BEGIN_MESSAGE_MAP(CMerge20GrepDlg, CTrDialog)
	//{{AFX_MSG_MAP(CMerge20GrepDlg)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MERGE20_GREP_BTN_SEARCH, IDC_MERGE20_GREP_BTN_CLOSE, OnButtonClicked)
	ON_NOTIFY(NM_CLICK, IDC_MERGE20_GREP_LIST, OnClickResult)
	ON_MESSAGE(WM_MERGE20_GREP_PROGRESS, OnGrepProgress)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

namespace
{
/** @brief OnGrepProgress wParam: more hits arrived (drain them). */
constexpr WPARAM GrepProgress = 0;
/** @brief OnGrepProgress wParam: the worker finished. */
constexpr WPARAM GrepFinished = 1;
/** @brief Bytes scanned for the binary (NUL) check. */
constexpr int64_t GrepBinaryProbeBytes = 65536;
/** @brief Stop-flag check interval while reading one file (lines). */
constexpr int GrepStopCheckLines = 1024;
/** @brief Search-box label width in pixels. */
constexpr int GrepLabelWidth = 90;
/** @brief Search/Stop/Close button width in pixels. */
constexpr int GrepButtonWidth = 84;
/** @brief Margins and row heights in pixels. */
constexpr int GrepMargin = 8;
constexpr int GrepRowGap = 6;
constexpr int GrepTopRowHeight = 24;
constexpr int GrepStatusHeight = 16;
constexpr int GrepCloseRowHeight = 24;
}

/**
 * @brief Scan worker (file-local, DirScan's DiffWorker precedent).
 * Owns its copies of the file list, the lower-cased query and the
 * encoding-guess option; talks to the dialog only through its
 * public worker-thread interface (AddPendingHit /
 * NotifyScanFinished / ShouldStop / GetHitCount /
 * IncrementFilesScanned). Never touches a CWnd or a CMergeDoc.
 */
class Merge20GrepWorker : public Poco::Runnable
{
public:
	Merge20GrepWorker(CMerge20GrepDlg* pDlg, std::vector<Merge20GrepFile> files, String queryLower, int nGuessEncodingType)
		: m_pDlg(pDlg)
		, m_files(std::move(files))
		, m_queryLower(std::move(queryLower))
		, m_nGuessEncodingType(nGuessEncodingType)
	{
	}
	void run() override;

private:
	void ScanFile(const Merge20GrepFile& file, int pane);

	CMerge20GrepDlg* m_pDlg;
	std::vector<Merge20GrepFile> m_files;
	String m_queryLower;
	int m_nGuessEncodingType;
};

void Merge20GrepWorker::run()
{
	// COM for this thread (MLang detector behind Guess; see the
	// file header). A failed init only degrades the guess.
	const HRESULT hrCo = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	for (const Merge20GrepFile& file : m_files)
	{
		if (m_pDlg->ShouldStop())
			break;
		for (int pane = 0; pane < static_cast<int>(file.paths.size()); ++pane)
		{
			if (file.paths[static_cast<size_t>(pane)].empty())
				continue;
			if (m_pDlg->ShouldStop())
				break;
			ScanFile(file, pane);
			if (m_pDlg->ShouldStop())
				break;
		}
		m_pDlg->IncrementFilesScanned();
	}
	if (SUCCEEDED(hrCo))
		::CoUninitialize();
	m_pDlg->NotifyScanFinished();
}

void Merge20GrepWorker::ScanFile(const Merge20GrepFile& file, int pane)
{
	const String& path = file.paths[static_cast<size_t>(pane)];
	// Same encoding detection as the compare (MergeDoc.cpp:2952).
	const FileTextEncoding encoding = codepage_detect::Guess(path, m_nGuessEncodingType);
	UniMemFile uf;
	if (!uf.OpenReadOnly(path))
		return;
	// Binary files are skipped: a NUL byte in the first 64KB of a
	// single-byte file means binary. UTF-16 legitimately contains
	// NUL bytes, so the check does not apply to it (UTF-8 text
	// with a NUL is treated as binary, as in the compare).
	if (encoding.m_unicoding == ucr::NONE)
	{
		const unsigned char* base = uf.GetBase();
		const int64_t size = uf.GetFileSize();
		const int64_t check = std::min<int64_t>(size, GrepBinaryProbeBytes);
		if (base != nullptr && check > 0 && std::memchr(base, '\0', static_cast<size_t>(check)) != nullptr)
			return;
	}
	uf.SetUnicoding(encoding.m_unicoding);
	uf.SetCodepage(encoding.m_codepage);
	uf.SetBom(encoding.m_bom);
	if (encoding.m_bom)
		uf.ReadBom();
	// CDiffTextBuffer::LoadFromFile loop idiom: ReadString returns
	// false on the last line but still delivers it.
	String line, eol;
	bool done = false;
	int nLine = 0;
	do
	{
		bool lossy = false;
		done = !uf.ReadString(line, eol, &lossy);
		const int pos = Merge20GrepFindInLine(line, m_queryLower);
		if (pos >= 0)
		{
			Merge20GrepHit hit;
			hit.relPath = file.relPath;
			hit.pane = pane;
			hit.line = nLine;
			hit.excerpt = Merge20GrepMakeExcerpt(line, static_cast<size_t>(pos), m_queryLower.size());
			m_pDlg->AddPendingHit(hit);
			if (m_pDlg->ShouldStop())
				break;
		}
		++nLine;
		if ((nLine & (GrepStopCheckLines - 1)) == 0 && m_pDlg->ShouldStop())
			break;
	} while (!done);
}

CMerge20GrepDlg::CMerge20GrepDlg()
	: CTrDialog(IDD_MERGE20_GREP)
	, m_pDirView(nullptr)
	, m_nGuessEncodingType(0)
	, m_nFilesTotal(0)
	, m_nPanes(2)
	, m_bStoppedByUser(false)
	, m_nHitCount(0)
	, m_nFilesScanned(0)
	, m_bStop(false)
	, m_bRunning(false)
{
}

CMerge20GrepDlg::~CMerge20GrepDlg()
{
	// Belt and braces: OnDestroy already stopped the scan, but the
	// dialog object may outlive its window (member of the view).
	StopScan();
}

BOOL CMerge20GrepDlg::Create(CWnd* pParentWnd)
{
	return CDialog::Create(IDD_MERGE20_GREP, pParentWnd);
}

BOOL CMerge20GrepDlg::OnInitDialog()
{
	__super::OnInitDialog();

	SetWindowText(_("Cross-File Search").c_str());

	const DWORD dwLabelStyle = WS_CHILD | WS_VISIBLE | SS_LEFT;
	m_stLabel.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);
	m_stLabel.SetWindowText(_("Search Text").c_str());

	const DWORD dwComboStyle = WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL;
	if (!m_cbQuery.Create(dwComboStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_GREP_SEARCH))
		return FALSE;

	const DWORD dwBtnStyle = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
	m_btnSearch.Create(_T(""), dwBtnStyle | BS_DEFPUSHBUTTON, CRect(0, 0, 1, 1), this, IDC_MERGE20_GREP_BTN_SEARCH);
	m_btnSearch.SetWindowText(_("Search").c_str());
	m_btnStop.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_GREP_BTN_STOP);
	m_btnStop.SetWindowText(_("Stop").c_str());
	m_btnStop.EnableWindow(FALSE);
	m_btnClose.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_GREP_BTN_CLOSE);
	m_btnClose.SetWindowText(_("Close").c_str());

	m_stStatus.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);

	const DWORD dwListStyle = WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
	if (!m_list.Create(dwListStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_GREP_LIST))
		return FALSE;
	m_list.SetExtendedStyle(m_list.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_list.InsertColumn(0, _("File").c_str(), LVCFMT_LEFT, 280);
	m_list.InsertColumn(1, _("Pane").c_str(), LVCFMT_LEFT, 70);
	m_list.InsertColumn(2, _("Line").c_str(), LVCFMT_LEFT, 70);
	m_list.InsertColumn(3, _("Text").c_str(), LVCFMT_LEFT, 200);

	LayoutAll();
	return TRUE;
}

void CMerge20GrepDlg::ShowGrep(CDirView* pDirView)
{
	if (pDirView == nullptr)
		return;
	m_pDirView = pDirView;
	m_files = pDirView->Merge20BuildGrepFiles();
	m_nFilesTotal = static_cast<int>(m_files.size());
	m_nPanes = 2;
	for (const Merge20GrepFile& file : m_files)
		m_nPanes = (std::max)(m_nPanes, static_cast<int>(file.paths.size()));
	if (m_hWnd == nullptr && !Create(pDirView))
		return;
	RefreshHistoryCombo();
	ShowWindow(SW_SHOW);
	SetForegroundWindow();
	m_cbQuery.SetFocus();
}

void CMerge20GrepDlg::CloseGrep()
{
	// Modeless dialog: never destroy the window on Esc/Cancel,
	// only hide it (the view owns this dialog as a member).
	// A running scan stops: hiding means the user is done with it.
	StopScan();
	if (m_hWnd != nullptr)
	{
		ShowWindow(SW_HIDE);
		if (m_pDirView != nullptr)
			m_pDirView->SetFocus();
	}
}

void CMerge20GrepDlg::StopScan()
{
	// UI thread only: joinAll from the worker itself would deadlock.
	m_bStop = true;
	if (m_pPool != nullptr)
	{
		m_pPool->joinAll();
		m_pPool.reset();
	}
	m_pWorker.reset();
	m_bRunning = false;
}

void CMerge20GrepDlg::AddPendingHit(const Merge20GrepHit& hit)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_pendingHits.push_back(hit);
	}
	++m_nHitCount;
	if (m_hWnd != nullptr)
		PostMessage(WM_MERGE20_GREP_PROGRESS, GrepProgress, 0);
}

void CMerge20GrepDlg::NotifyScanFinished()
{
	if (m_hWnd != nullptr)
		PostMessage(WM_MERGE20_GREP_PROGRESS, GrepFinished, 0);
}

bool CMerge20GrepDlg::ShouldStop() const
{
	return m_bStop.load() || Merge20GrepReachedLimit(m_nHitCount.load());
}

void CMerge20GrepDlg::OnOK()
{
	// Enter in the search box starts a search (never closes).
	StartSearch();
}

void CMerge20GrepDlg::OnCancel()
{
	// Esc hides the dialog (never destroys it).
	CloseGrep();
}

void CMerge20GrepDlg::OnDestroy()
{
	// The worker posts to this HWND: join it before the window dies.
	StopScan();
	CTrDialog::OnDestroy();
}

BOOL CMerge20GrepDlg::PreTranslateMessage(MSG* pMsg)
{
	return CTrDialog::PreTranslateMessage(pMsg);
}

void CMerge20GrepDlg::OnSize(UINT nType, int cx, int cy)
{
	CTrDialog::OnSize(nType, cx, cy);
	LayoutAll();
}

void CMerge20GrepDlg::LayoutAll()
{
	if (m_hWnd == nullptr || m_list.GetSafeHwnd() == nullptr)
		return;
	CRect rc;
	GetClientRect(&rc);
	const int cx = rc.Width();
	const int cy = rc.Height();
	if (cx < 300 || cy < 150)
		return;

	const int yTop = GrepMargin;
	const int yStatus = yTop + GrepTopRowHeight + GrepRowGap;
	const int yList = yStatus + GrepStatusHeight + GrepRowGap;
	const int yClose = cy - GrepMargin - GrepCloseRowHeight;
	const int listBottom = yClose - GrepRowGap;

	m_stLabel.MoveWindow(GrepMargin, yTop + 4, GrepLabelWidth, GrepStatusHeight, TRUE);
	const int xCombo = GrepMargin + GrepLabelWidth + GrepRowGap;
	const int xStop = cx - GrepMargin - GrepButtonWidth;
	const int xSearch = xStop - GrepRowGap - GrepButtonWidth;
	m_cbQuery.MoveWindow(xCombo, yTop, (std::max)(60, xSearch - GrepRowGap - xCombo), 160, TRUE);
	m_btnSearch.MoveWindow(xSearch, yTop, GrepButtonWidth, GrepTopRowHeight, TRUE);
	m_btnStop.MoveWindow(xStop, yTop, GrepButtonWidth, GrepTopRowHeight, TRUE);
	m_stStatus.MoveWindow(GrepMargin, yStatus, cx - 2 * GrepMargin, GrepStatusHeight, TRUE);
	m_list.MoveWindow(GrepMargin, yList, cx - 2 * GrepMargin, (std::max)(40, listBottom - yList), TRUE);
	m_btnClose.MoveWindow(cx - GrepMargin - GrepButtonWidth, yClose, GrepButtonWidth, GrepCloseRowHeight, TRUE);

	// The excerpt column takes the remaining width.
	CRect rcList;
	m_list.GetClientRect(&rcList);
	const int fixed = 280 + 70 + 70;
	m_list.SetColumnWidth(3, (std::max)(80, rcList.Width() - fixed - 4));
}

void CMerge20GrepDlg::OnButtonClicked(UINT nID)
{
	switch (nID)
	{
	case IDC_MERGE20_GREP_BTN_SEARCH:
		StartSearch();
		break;
	case IDC_MERGE20_GREP_BTN_STOP:
		m_bStoppedByUser = true;
		StopScan();
		m_btnSearch.EnableWindow(TRUE);
		m_btnStop.EnableWindow(FALSE);
		m_cbQuery.EnableWindow(TRUE);
		UpdateStatus();
		break;
	case IDC_MERGE20_GREP_BTN_CLOSE:
		CloseGrep();
		break;
	}
}

void CMerge20GrepDlg::OnClickResult(NMHDR* pNMHDR, LRESULT* pResult)
{
	*pResult = 0;
	if (m_pDirView == nullptr)
		return;
	const NMITEMACTIVATE* pNM = reinterpret_cast<NMITEMACTIVATE*>(pNMHDR);
	if (pNM->iItem < 0 || pNM->iItem >= static_cast<int>(m_hits.size()))
		return;
	// The jump itself lives in the view (it owns the selection and
	// the compare opening); the dialog only knows the hit.
	m_pDirView->Merge20OpenFileAtLine(m_hits[static_cast<size_t>(pNM->iItem)]);
}

LRESULT CMerge20GrepDlg::OnGrepProgress(WPARAM wParam, LPARAM /*lParam*/)
{
	DrainPendingHits();
	if (wParam == GrepFinished)
	{
		m_bRunning = false;
		m_btnSearch.EnableWindow(TRUE);
		m_btnStop.EnableWindow(FALSE);
		m_cbQuery.EnableWindow(TRUE);
	}
	UpdateStatus();
	return 0;
}

void CMerge20GrepDlg::DrainPendingHits()
{
	std::vector<Merge20GrepHit> pending;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		pending.swap(m_pendingHits);
	}
	for (const Merge20GrepHit& hit : pending)
	{
		const int nRow = m_list.GetItemCount();
		m_hits.push_back(hit);
		m_list.InsertItem(nRow, hit.relPath.c_str());
		m_list.SetItemText(nRow, 1, PaneLabel(hit.pane, m_nPanes).c_str());
		m_list.SetItemText(nRow, 2, strutils::to_str(hit.line + 1).c_str());
		m_list.SetItemText(nRow, 3, hit.excerpt.c_str());
	}
}

void CMerge20GrepDlg::UpdateStatus()
{
	if (m_bRunning.load())
	{
		m_stStatus.SetWindowText(strutils::format_string3(
			_("Searching... %1 hits (%2 of %3 files)"),
			strutils::to_str(GetHitCount()),
			strutils::to_str(GetFilesScanned()),
			strutils::to_str(GetFilesTotal())).c_str());
		return;
	}
	if (Merge20GrepReachedLimit(GetHitCount()))
	{
		m_stStatus.SetWindowText(_("Limit reached (80 items)").c_str());
		return;
	}
	if (m_bStoppedByUser)
	{
		m_stStatus.SetWindowText(_("Search stopped").c_str());
		return;
	}
	if (m_hits.empty())
	{
		m_stStatus.SetWindowText(_("No matches").c_str());
		return;
	}
	m_stStatus.SetWindowText(strutils::format_string1(
		_("%1 hits"), strutils::to_str(GetHitCount())).c_str());
}

String CMerge20GrepDlg::PaneLabel(int pane, int nPanes) const
{
	if (pane == 0)
		return _("Old");
	if (pane == nPanes - 1)
		return _("New side");
	return _("Middle");
}

void CMerge20GrepDlg::RefreshHistoryCombo()
{
	m_cbQuery.ResetContent();
	const std::vector<String> history = Merge20GrepParseHistory(
		GetOptionsMgr()->GetString(OPT_MERGE20_GREP_HISTORY));
	for (const String& query : history)
		m_cbQuery.AddString(query.c_str());
	if (!history.empty())
		m_cbQuery.SetCurSel(0);
}

void CMerge20GrepDlg::StartSearch()
{
	if (m_bRunning.load() || m_pDirView == nullptr)
		return;

	CString sQuery;
	m_cbQuery.GetWindowText(sQuery);
	String query = static_cast<LPCTSTR>(sQuery);
	const size_t first = query.find_first_not_of(_T(" \t\r\n"));
	if (first == String::npos)
	{
		m_stStatus.SetWindowText(_("Enter a search string.").c_str());
		return;
	}
	const size_t last = query.find_last_not_of(_T(" \t\r\n"));
	query = query.substr(first, last - first + 1);

	// History first (design section 4.1): latest 10, most recent
	// first, saved to OPT_MERGE20_GREP_HISTORY on every search.
	const std::vector<String> history = Merge20GrepPushHistory(
		Merge20GrepParseHistory(GetOptionsMgr()->GetString(OPT_MERGE20_GREP_HISTORY)), query);
	GetOptionsMgr()->SaveOption(OPT_MERGE20_GREP_HISTORY, Merge20GrepFormatHistory(history));
	RefreshHistoryCombo();
	m_cbQuery.SetWindowText(query.c_str());

	// A previous scan (finished or stopped) is joined here so the
	// new pool starts clean.
	StopScan();
	m_bStop = false;
	m_bStoppedByUser = false;
	m_list.DeleteAllItems();
	m_hits.clear();
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_pendingHits.clear();
	}
	m_nHitCount = 0;
	m_nFilesScanned = 0;
	m_nFilesTotal = static_cast<int>(m_files.size());
	m_queryLower = strutils::makelower(query);
	m_nGuessEncodingType = GetOptionsMgr()->GetInt(OPT_CP_DETECT);

	m_pWorker = std::make_shared<Merge20GrepWorker>(this, m_files, m_queryLower, m_nGuessEncodingType);
	m_pPool = std::make_unique<Poco::ThreadPool>(1, 1);
	m_bRunning = true;
	m_btnSearch.EnableWindow(FALSE);
	m_btnStop.EnableWindow(TRUE);
	m_cbQuery.EnableWindow(FALSE);
	UpdateStatus();
	m_pPool->start(*m_pWorker);
}
