// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20BulkClassifyDlg.cpp
 *
 * @brief Implementation of the bulk classification dialog (Merge20 Phase 4 D10).
 *
 * The worker reads on-disk file text directly (the D7 Grep worker's
 * finding, design section 3.7: opening a CMergeDoc per file would
 * create views/documents and cannot run on a background thread).
 * Consequences, same as Grep: the bulk classification reads on-disk
 * text, not unpacker/prediffer-processed text, and binary files are
 * skipped. A file opened later is reclassified from its merge doc
 * by the S4 Rescan hook, so the opened result always wins over the
 * bulk one. The differ itself is the HTML `diffFast` port
 * (Merge20BulkDiff), so the bulk classification of a file matches
 * the HTML tool's classification of the same two files (same
 * threshold, same positional fallback, same ignore options).
 *
 * Background run (design section 3.7): DirScan's Poco ThreadPool
 * precedent (DirScan.cpp:480), with a single worker on purpose:
 * results arrive in folder-list order, and the differ never runs
 * concurrently with itself. The UI stays responsive because all
 * file I/O and diffing runs off the UI thread; the progress bar and
 * the status text advance live. The worker calls CoInitializeEx
 * first: the MLang detector behind codepage_detect::Guess is
 * COM-based. If COM init fails, the run still proceeds (Guess
 * falls back to the default codepage).
 */
#include "StdAfx.h"
#include "Merge20/Merge20BulkClassifyDlg.h"
#include "Merge20/Merge20BulkDiff.h"
#include "Merge20/Merge20Classify.h"
#include "Merge20/Merge20ClassifyDoc.h"
#include "DirView.h"
#include "DirDoc.h"
#include "DiffContext.h"
#include "PathContext.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"
#include "UnicodeString.h"
#include "Logger.h"
#include "codepage_detect.h"
#include "UniFile.h"
#include <Poco/ThreadPool.h>
#include <Poco/Runnable.h>
#include <objbase.h>
#include <algorithm>
#include <chrono>
#include <cstring>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20BulkClassifyDlg, CTrDialog)

BEGIN_MESSAGE_MAP(CMerge20BulkClassifyDlg, CTrDialog)
	//{{AFX_MSG_MAP(CMerge20BulkClassifyDlg)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_CONTROL_RANGE(BN_CLICKED, IDC_MERGE20_BULK_STOP, IDC_MERGE20_BULK_CLOSE, OnButtonClicked)
	ON_MESSAGE(WM_MERGE20_BULK_PROGRESS, OnBulkProgress)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

namespace
{
/** @brief OnBulkProgress wParam: more results arrived (drain them). */
constexpr WPARAM BulkProgress = 0;
/** @brief OnBulkProgress wParam: the worker finished. */
constexpr WPARAM BulkFinished = 1;
/** @brief Bytes scanned for the binary (NUL) check. */
constexpr int64_t BulkBinaryProbeBytes = 65536;
/** @brief Stop-flag check interval while reading one file (lines). */
constexpr int BulkStopCheckLines = 1024;
/** @brief Search/Stop/Close button width in pixels. */
constexpr int BulkButtonWidth = 84;
/** @brief Margins and row heights in pixels. */
constexpr int BulkMargin = 8;
constexpr int BulkRowGap = 6;
constexpr int BulkRowHeight = 24;
constexpr int BulkStatusHeight = 16;
}

/**
 * @brief Bulk worker (file-local, DirScan's DiffWorker precedent).
 * Owns its copies of the file list, the differ options, the risk
 * keywords and the encoding-guess option; talks to the dialog only
 * through its public worker-thread interface (AddPendingResult /
 * NotifyScanFinished / ShouldStop / IncrementFilesDone). Never
 * touches a CWnd, a CDiffContext or a CMergeDoc.
 */
class Merge20BulkClassifyWorker : public Poco::Runnable
{
public:
	Merge20BulkClassifyWorker(CMerge20BulkClassifyDlg* pDlg, std::vector<Merge20BulkClassifyFile> files,
		Merge20BulkDiffOptions diffOptions, std::vector<String> riskKeywords, int nGuessEncodingType)
		: m_pDlg(pDlg)
		, m_files(std::move(files))
		, m_diffOptions(diffOptions)
		, m_riskKeywords(std::move(riskKeywords))
		, m_nGuessEncodingType(nGuessEncodingType)
	{
	}
	void run() override;

private:
	void ClassifyFile(const Merge20BulkClassifyFile& file);
	bool ReadFileLines(const String& path, std::vector<String>& lines);

	CMerge20BulkClassifyDlg* m_pDlg;
	std::vector<Merge20BulkClassifyFile> m_files;
	Merge20BulkDiffOptions m_diffOptions;
	std::vector<String> m_riskKeywords;
	int m_nGuessEncodingType;
};

void Merge20BulkClassifyWorker::run()
{
	// COM for this thread (MLang detector behind Guess; see the
	// file header). A failed init only degrades the guess.
	const HRESULT hrCo = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	for (const Merge20BulkClassifyFile& file : m_files)
	{
		if (m_pDlg->ShouldStop())
			break;
		ClassifyFile(file);
		m_pDlg->IncrementFilesDone();
	}
	if (SUCCEEDED(hrCo))
		::CoUninitialize();
	m_pDlg->NotifyScanFinished();
}

void Merge20BulkClassifyWorker::ClassifyFile(const Merge20BulkClassifyFile& file)
{
	Merge20BulkClassifyResult result;
	result.relPath = file.relPath;
	result.paths = file.paths;
	const bool oldExists = file.exists[0];
	const bool newExists = file.exists[1];
	if (!oldExists && !newExists)
	{
		result.skipped = true;
		m_pDlg->AddPendingResult(result);
		return;
	}
	if (!oldExists || !newExists)
	{
		// One-sided file: only the line count is needed. The folder
		// doc refines the action and summary when the result is
		// applied (CDirDoc::SetMerge20Classification), like it does
		// for opened files.
		std::vector<String> lines;
		if (!ReadFileLines(oldExists ? file.paths[0] : file.paths[1], lines))
		{
			result.skipped = true;
			m_pDlg->AddPendingResult(result);
			return;
		}
		Merge20FileState state;
		state.classified = true;
		if (oldExists)
		{
			state.action = ActionCategory::COPY_OLD_FILE;
			state.nMissing = static_cast<int>(lines.size());
		}
		else
		{
			state.action = ActionCategory::SKIP_NEW_FILE;
			state.nAdded = static_cast<int>(lines.size());
		}
		result.state = state;
		m_pDlg->AddPendingResult(result);
		return;
	}
	std::vector<String> oldLines, newLines;
	if (!ReadFileLines(file.paths[0], oldLines) || !ReadFileLines(file.paths[1], newLines))
	{
		result.skipped = true;
		m_pDlg->AddPendingResult(result);
		return;
	}
	const Merge20BulkDiffResult diff = Merge20BulkDiff(oldLines, newLines, m_diffOptions);
	Merge20FileState state = Merge20ClassifyFromRanges(diff.ranges, oldLines, newLines, m_riskKeywords);
	state.simpleCompare = diff.simpleUsed;
	result.state = state;
	m_pDlg->AddPendingResult(result);
}

bool Merge20BulkClassifyWorker::ReadFileLines(const String& path, std::vector<String>& lines)
{
	// Same encoding detection as the compare (MergeDoc.cpp:2952).
	const FileTextEncoding encoding = codepage_detect::Guess(path, m_nGuessEncodingType);
	UniMemFile uf;
	if (!uf.OpenReadOnly(path))
		return false;
	// Binary files are skipped: a NUL byte in the first 64KB of a
	// single-byte file means binary. UTF-16 legitimately contains
	// NUL bytes, so the check does not apply to it (UTF-8 text
	// with a NUL is treated as binary, as in the compare).
	if (encoding.m_unicoding == ucr::NONE)
	{
		const unsigned char* base = uf.GetBase();
		const int64_t size = uf.GetFileSize();
		const int64_t check = std::min<int64_t>(size, BulkBinaryProbeBytes);
		if (base != nullptr && check > 0 && std::memchr(base, '\0', static_cast<size_t>(check)) != nullptr)
			return false;
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
		lines.push_back(line);
		++nLine;
		if ((nLine & (BulkStopCheckLines - 1)) == 0 && m_pDlg->ShouldStop())
			return false;
	} while (!done);
	return true;
}

CMerge20BulkClassifyDlg::CMerge20BulkClassifyDlg()
	: CTrDialog(IDD_MERGE20_BULK_CLASSIFY)
	, m_pDirView(nullptr)
	, m_nGuessEncodingType(0)
	, m_nFilesTotal(0)
	, m_bStoppedByUser(false)
	, m_nClassified(0)
	, m_nSimple(0)
	, m_nSkipped(0)
	, m_nFilesDone(0)
	, m_bStop(false)
	, m_bRunning(false)
{
}

CMerge20BulkClassifyDlg::~CMerge20BulkClassifyDlg()
{
	// Belt and braces: OnDestroy already stopped the run, but the
	// dialog object may outlive its window (member of the view).
	StopScan();
}

BOOL CMerge20BulkClassifyDlg::Create(CWnd* pParentWnd)
{
	return CDialog::Create(IDD_MERGE20_BULK_CLASSIFY, pParentWnd);
}

BOOL CMerge20BulkClassifyDlg::OnInitDialog()
{
	__super::OnInitDialog();

	SetWindowText(_("Bulk Classify").c_str());

	const DWORD dwLabelStyle = WS_CHILD | WS_VISIBLE | SS_LEFT;
	m_stStatus.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);
	m_stFile.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);

	const DWORD dwProgressStyle = WS_CHILD | WS_VISIBLE | PBS_SMOOTH;
	m_progress.Create(dwProgressStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_BULK_PROGRESS);

	const DWORD dwBtnStyle = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
	m_btnStop.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_BULK_STOP);
	m_btnStop.SetWindowText(_("Stop").c_str());
	m_btnClose.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_BULK_CLOSE);
	m_btnClose.SetWindowText(_("Close").c_str());

	LayoutAll();
	return TRUE;
}

void CMerge20BulkClassifyDlg::ShowBulkClassify(CDirView* pDirView)
{
	if (pDirView == nullptr)
		return;
	if (IsRunning())
	{
		// Already running: bring to front, do not restart.
		if (m_hWnd != nullptr)
		{
			ShowWindow(SW_SHOW);
			SetForegroundWindow();
		}
		return;
	}
	m_pDirView = pDirView;
	if (pDirView->GetDiffContext().GetCompareDirs() != 2)
	{
		AfxMessageBox(_("Bulk classification supports 2-way folder compares only.").c_str());
		return;
	}
	m_files = pDirView->Merge20BuildBulkClassifyFiles();
	if (m_files.empty())
	{
		AfxMessageBox(_("No unclassified files.").c_str());
		return;
	}
	m_riskKeywords = Merge20ParseRiskKeywords(GetOptionsMgr()->GetString(OPT_MERGE20_RISK_KEYWORDS));
	m_diffOptions.ignoreWhitespace = GetOptionsMgr()->GetBool(OPT_CMP_IGNORE_WHITESPACE);
	m_diffOptions.ignoreCase = GetOptionsMgr()->GetBool(OPT_CMP_IGNORE_CASE);
	m_diffOptions.ignoreBlankLines = GetOptionsMgr()->GetBool(OPT_CMP_IGNORE_BLANKLINES);
	m_diffOptions.simpleThreshold = GetOptionsMgr()->GetInt(OPT_MERGE20_SIMPLE_THRESHOLD);
	m_nGuessEncodingType = GetOptionsMgr()->GetInt(OPT_CP_DETECT);
	if (m_hWnd == nullptr && !Create(pDirView))
		return;
	ShowWindow(SW_SHOW);
	SetForegroundWindow();
	StartRun();
}

void CMerge20BulkClassifyDlg::CloseBulkClassify()
{
	// Modeless dialog: never destroy the window on Esc/Cancel,
	// only hide it (the view owns this dialog as a member).
	// A running bulk run stops: hiding means the user is done with it.
	StopScan();
	if (m_hWnd != nullptr)
	{
		ShowWindow(SW_HIDE);
		if (m_pDirView != nullptr)
			m_pDirView->SetFocus();
	}
}

void CMerge20BulkClassifyDlg::StopScan()
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

void CMerge20BulkClassifyDlg::AddPendingResult(const Merge20BulkClassifyResult& result)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_pendingResults.push_back(result);
	}
	if (m_hWnd != nullptr)
		PostMessage(WM_MERGE20_BULK_PROGRESS, BulkProgress, 0);
}

void CMerge20BulkClassifyDlg::NotifyScanFinished()
{
	if (m_hWnd != nullptr)
		PostMessage(WM_MERGE20_BULK_PROGRESS, BulkFinished, 0);
}

bool CMerge20BulkClassifyDlg::ShouldStop() const
{
	return m_bStop.load();
}

void CMerge20BulkClassifyDlg::OnOK()
{
	// Enter does nothing (never closes or restarts a run).
}

void CMerge20BulkClassifyDlg::OnCancel()
{
	// Esc hides the dialog (never destroys it).
	CloseBulkClassify();
}

void CMerge20BulkClassifyDlg::OnDestroy()
{
	// The worker posts to this HWND: join it before the window dies.
	StopScan();
	CTrDialog::OnDestroy();
}

BOOL CMerge20BulkClassifyDlg::PreTranslateMessage(MSG* pMsg)
{
	return CTrDialog::PreTranslateMessage(pMsg);
}

void CMerge20BulkClassifyDlg::OnSize(UINT nType, int cx, int cy)
{
	CTrDialog::OnSize(nType, cx, cy);
	LayoutAll();
}

void CMerge20BulkClassifyDlg::LayoutAll()
{
	if (m_hWnd == nullptr || m_progress.GetSafeHwnd() == nullptr)
		return;
	CRect rc;
	GetClientRect(&rc);
	const int cx = rc.Width();
	const int cy = rc.Height();
	if (cx < 300 || cy < 120)
		return;

	const int yStatus = BulkMargin;
	const int yProgress = yStatus + BulkStatusHeight + BulkRowGap;
	const int yFile = yProgress + BulkRowHeight + BulkRowGap;
	const int yButtons = cy - BulkMargin - BulkRowHeight;

	m_stStatus.MoveWindow(BulkMargin, yStatus, cx - 2 * BulkMargin, BulkStatusHeight, TRUE);
	m_progress.MoveWindow(BulkMargin, yProgress, cx - 2 * BulkMargin, BulkRowHeight - 4, TRUE);
	m_stFile.MoveWindow(BulkMargin, yFile, cx - 2 * BulkMargin, BulkStatusHeight, TRUE);
	m_btnClose.MoveWindow(cx - BulkMargin - BulkButtonWidth, yButtons, BulkButtonWidth, BulkRowHeight, TRUE);
	m_btnStop.MoveWindow(cx - BulkMargin - 2 * BulkButtonWidth - BulkRowGap, yButtons, BulkButtonWidth, BulkRowHeight, TRUE);
}

void CMerge20BulkClassifyDlg::OnButtonClicked(UINT nID)
{
	switch (nID)
	{
	case IDC_MERGE20_BULK_STOP:
		m_bStoppedByUser = true;
		StopScan();
		m_btnStop.EnableWindow(FALSE);
		UpdateStatus();
		break;
	case IDC_MERGE20_BULK_CLOSE:
		CloseBulkClassify();
		break;
	}
}

LRESULT CMerge20BulkClassifyDlg::OnBulkProgress(WPARAM wParam, LPARAM /*lParam*/)
{
	DrainPendingResults();
	if (wParam == BulkFinished)
	{
		m_bRunning = false;
		m_btnStop.EnableWindow(FALSE);
		// One log line for the run (D0's measure line precedent):
		// the D10 measurement record for the bulk computation.
		const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - m_start).count();
		std::string line = "Merge20 bulk classify: " + std::to_string(m_nClassified.load()) +
			" files classified (" + std::to_string(m_nSimple.load()) + " simple), " +
			std::to_string(m_nSkipped.load()) + " skipped, " + std::to_string(ms) + " ms";
		if (m_bStoppedByUser)
			line += ", stopped";
		RootLogger::Info(line);
	}
	UpdateStatus();
	return 0;
}

void CMerge20BulkClassifyDlg::DrainPendingResults()
{
	std::vector<Merge20BulkClassifyResult> pending;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		pending.swap(m_pendingResults);
	}
	// Merge20 (Panel P1, design section 12.5): suspend the per-file
	// panel notification while draining, so a bulk run posts one
	// refresh instead of one per classified file (each
	// SetMerge20Classification notifies on its own). Resume notifies
	// once when a change happened while suspended.
	const CDiffContext* pCtxt = nullptr;
	if (m_pDirView != nullptr)
	{
		if (CDirDoc* pDoc = m_pDirView->GetDocument())
			pCtxt = &pDoc->GetDiffContext();
	}
	if (pCtxt != nullptr)
		pCtxt->SuspendMerge20FileListNotify();
	for (const Merge20BulkClassifyResult& result : pending)
	{
		if (result.skipped)
		{
			++m_nSkipped;
			continue;
		}
		++m_nClassified;
		if (result.state.simpleCompare)
			++m_nSimple;
		if (m_pDirView != nullptr)
		{
			if (CDirDoc* pDoc = m_pDirView->GetDocument())
				pDoc->SetMerge20Classification(PathContext(result.paths), result.state);
		}
		m_stFile.SetWindowText(result.relPath.c_str());
	}
	if (pCtxt != nullptr)
		pCtxt->ResumeMerge20FileListNotify();
}

void CMerge20BulkClassifyDlg::StartRun()
{
	StopScan();
	m_bStop = false;
	m_bStoppedByUser = false;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_pendingResults.clear();
	}
	m_nClassified = 0;
	m_nSimple = 0;
	m_nSkipped = 0;
	m_nFilesDone = 0;
	m_nFilesTotal = static_cast<int>(m_files.size());
	m_start = std::chrono::steady_clock::now();

	m_pWorker = std::make_shared<Merge20BulkClassifyWorker>(this, m_files, m_diffOptions, m_riskKeywords, m_nGuessEncodingType);
	m_pPool = std::make_unique<Poco::ThreadPool>(1, 1);
	m_bRunning = true;
	m_btnStop.EnableWindow(TRUE);
	m_btnClose.EnableWindow(TRUE);
	m_progress.SetRange32(0, (std::max)(1, m_nFilesTotal));
	m_progress.SetPos(0);
	m_stFile.SetWindowText(_T(""));
	UpdateStatus();
	m_pPool->start(*m_pWorker);
}

void CMerge20BulkClassifyDlg::UpdateStatus()
{
	if (m_progress.GetSafeHwnd() != nullptr)
		m_progress.SetPos(GetFilesDone());
	if (m_bRunning.load())
	{
		m_stStatus.SetWindowText(strutils::format_string3(
			_("Classifying... %1 of %2 files (%3 classified)"),
			strutils::to_str(GetFilesDone()),
			strutils::to_str(GetFilesTotal()),
			strutils::to_str(m_nClassified.load())).c_str());
		return;
	}
	if (m_bStoppedByUser)
	{
		m_stStatus.SetWindowText(_("Classification stopped").c_str());
		return;
	}
	const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - m_start).count();
	m_stStatus.SetWindowText((strutils::format_string3(
		_("Done: %1 files classified (%2 simple), %3 skipped"),
		strutils::to_str(m_nClassified.load()),
		strutils::to_str(m_nSimple.load()),
		strutils::to_str(m_nSkipped.load())) +
		_T(" (") + strutils::to_str(static_cast<int>(ms)) + _T(" ms)")).c_str());
}
