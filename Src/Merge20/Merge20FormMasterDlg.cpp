// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormMasterDlg.cpp
 *
 * @brief Implementation of the FORM name-master edit dialog
 * (Wave 5 F6, design sections 2.10, 8 and 10 F6).
 */
#include "StdAfx.h"
#include "Merge20/Merge20FormMasterDlg.h"
#include "Merge20/Merge20Session.h"
#include "OptionsMgr.h"
#include "IniOptionsMgr.h"
#include "Environment.h"
#include "paths.h"
#include "MergeApp.h"
#include <filesystem>
#include <fstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNAMIC(CMerge20FormMasterDlg, CTrDialog)

namespace
{

	// File name of the name master (fixed: the data is not bound
	// to a folder pair, like the learning data).
	const String kFormMasterFileName = _T("Merge20FormMaster.txt");

	// Directory of the name master (design section 8.2): the
	// learning-data scheme (Merge20LearnGetDirectory).
	String FormMasterGetDirectory()
	{
		if (dynamic_cast<CIniOptionsMgr*>(GetOptionsMgr()) != nullptr)
			return paths::ConcatPath(env::GetProgPath(), _T("Merge20FormMaster"));
		return paths::ConcatPath(env::GetAppDataPath(), _T("WinMerge\\Merge20FormMaster"));
	}

	// Layout constants (pixels; the dialog is resizable).
	const int kMasterMargin = 8;
	const int kMasterRowGap = 6;
	const int kMasterHintHeight = 28;
	const int kMasterCountHeight = 16;
	const int kMasterButtonHeight = 26;
	const int kMasterButtonWidth = 170;
	const int kMasterCancelWidth = 90;

	// Read a whole file as bytes. False when it cannot be opened
	// or read.
	bool ReadFileBytes(const String& filePath, std::string& bytes)
	{
		std::ifstream in(std::filesystem::path(filePath), std::ios::binary);
		if (!in)
			return false;
		bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		return !in.bad();
	}

	// Write text to a file with a temporary file plus rename (the
	// S8 session-save way); falls back to a direct write when the
	// rename fails (e.g. a locked target).
	bool WriteTextFileAtomic(const String& filePath, const std::string& utf8, String& errDetail)
	{
		errDetail.clear();
		std::filesystem::path target(filePath);
		String tmpName = filePath;
		tmpName += L".tmp";
		std::filesystem::path tmp(tmpName);
		{
			std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
			if (!out)
			{
				errDetail = _T("cannot create temporary file");
				return false;
			}
			out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
			out.flush();
			if (!out)
			{
				errDetail = _T("failed to write temporary file");
				return false;
			}
		}
		std::error_code ec;
		std::filesystem::rename(tmp, target, ec);
		if (ec)
		{
			std::ofstream out(target, std::ios::binary | std::ios::trunc);
			if (!out)
			{
				errDetail = _T("cannot write master file");
				return false;
			}
			out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
			out.flush();
			if (!out)
			{
				errDetail = _T("failed to write master file");
				return false;
			}
			std::filesystem::remove(tmp, ec);
		}
		return true;
	}

} // namespace

String Merge20FormMasterGetFilePath()
{
	return paths::ConcatPath(FormMasterGetDirectory(), kFormMasterFileName);
}

bool Merge20FormMasterLoadText(String& outText)
{
	outText.clear();
	std::string bytes;
	if (!ReadFileBytes(Merge20FormMasterGetFilePath(), bytes))
		return false;
	// Strip a UTF-8 BOM when present.
	if (bytes.size() >= 3 &&
		static_cast<unsigned char>(bytes[0]) == 0xEF &&
		static_cast<unsigned char>(bytes[1]) == 0xBB &&
		static_cast<unsigned char>(bytes[2]) == 0xBF)
		bytes.erase(0, 3);
	return Merge20Utf8Decode(bytes, outText);
}

bool Merge20FormMasterSaveText(const String& text, String& errDetail)
{
	errDetail.clear();
	const String filePath = Merge20FormMasterGetFilePath();
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(filePath).parent_path(), ec);
	if (ec)
	{
		errDetail = _T("cannot create master folder");
		return false;
	}
	const std::string utf8 = Merge20Utf8Encode(text);
	// A UTF-8 BOM, so the file opens directly in Notepad/Excel.
	std::string bytes("\xEF\xBB\xBF");
	bytes += utf8;
	return WriteTextFileAtomic(filePath, bytes, errDetail);
}

String Merge20FormMasterGetDefaultText()
{
	return Merge20FormMasterToText(Merge20FormMasterGetDefault());
}

CMerge20FormMasterDlg::CMerge20FormMasterDlg(CWnd* pParent)
	: CTrDialog(CMerge20FormMasterDlg::IDD, pParent)
{
}

BEGIN_MESSAGE_MAP(CMerge20FormMasterDlg, CTrDialog)
	ON_EN_CHANGE(IDC_MERGE20_FORM_MASTER_TEXT, OnTextChanged)
	ON_BN_CLICKED(IDC_MERGE20_FORM_MASTER_RESET, OnResetToDefault)
	ON_WM_SIZE()
END_MESSAGE_MAP()

BOOL CMerge20FormMasterDlg::OnInitDialog()
{
	__super::OnInitDialog();
	SetWindowText(_("Name master").c_str());

	String text;
	if (!Merge20FormMasterLoadText(text))
	{
		// A missing file is the first run (use the default list
		// silently); an existing but unreadable file is reported
		// and the default list is shown instead.
		std::error_code ec;
		if (std::filesystem::exists(std::filesystem::path(Merge20FormMasterGetFilePath()), ec))
			AfxMessageBox(_("Could not read the name master. Showing the default list."), MB_ICONWARNING);
		text = Merge20FormMasterGetDefaultText();
	}

	const DWORD dwLabelStyle = WS_CHILD | WS_VISIBLE | SS_LEFT;
	m_stHint.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this);
	m_stHint.SetWindowText(_("One \"ID<TAB>form name\" line per form. Saving applies the names immediately to the FORM compare window.").c_str());
	m_stCount.Create(_T(""), dwLabelStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_FORM_MASTER_COUNT);

	const DWORD dwEditStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_VSCROLL |
		ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_AUTOHSCROLL;
	if (!m_edText.Create(dwEditStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_FORM_MASTER_TEXT))
		return FALSE;
	m_edText.SetWindowText(text.c_str());
	// Tab stops of 8 dialog units would compress the names; use a
	// wide tab (about 24 average characters) so the ID column
	// stays readable.
	m_edText.SetTabStops(96);

	const DWORD dwBtnStyle = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
	m_btnReset.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDC_MERGE20_FORM_MASTER_RESET);
	m_btnReset.SetWindowText(_("Restore default list").c_str());
	m_btnSave.Create(_T(""), dwBtnStyle | BS_DEFPUSHBUTTON, CRect(0, 0, 1, 1), this, IDOK);
	m_btnSave.SetWindowText(_("Save and apply immediately").c_str());
	m_btnCancel.Create(_T(""), dwBtnStyle, CRect(0, 0, 1, 1), this, IDCANCEL);
	m_btnCancel.SetWindowText(_("Cancel").c_str());

	CFont* pFont = GetFont();
	if (pFont != nullptr)
	{
		m_stHint.SetFont(pFont);
		m_stCount.SetFont(pFont);
		m_edText.SetFont(pFont);
		m_btnReset.SetFont(pFont);
		m_btnSave.SetFont(pFont);
		m_btnCancel.SetFont(pFont);
	}
	UpdateCount();
	LayoutAll();
	return TRUE;
}

void CMerge20FormMasterDlg::OnSize(UINT nType, int cx, int cy)
{
	__super::OnSize(nType, cx, cy);
	LayoutAll();
}

void CMerge20FormMasterDlg::LayoutAll()
{
	if (m_hWnd == nullptr || m_edText.GetSafeHwnd() == nullptr)
		return;
	CRect rc;
	GetClientRect(&rc);
	const int cx = rc.Width();
	const int cy = rc.Height();
	if (cx < 300 || cy < 200)
		return;
	int y = kMasterMargin;
	m_stHint.MoveWindow(kMasterMargin, y, cx - 2 * kMasterMargin, kMasterHintHeight, TRUE);
	y += kMasterHintHeight + kMasterRowGap;
	m_stCount.MoveWindow(kMasterMargin, y, cx - 2 * kMasterMargin, kMasterCountHeight, TRUE);
	y += kMasterCountHeight + kMasterRowGap;
	const int yButtons = cy - kMasterMargin - kMasterButtonHeight;
	const int editBottom = yButtons - kMasterRowGap;
	m_edText.MoveWindow(kMasterMargin, y, cx - 2 * kMasterMargin,
		(std::max)(40, editBottom - y), TRUE);
	m_btnReset.MoveWindow(kMasterMargin, yButtons, kMasterButtonWidth, kMasterButtonHeight, TRUE);
	m_btnCancel.MoveWindow(cx - kMasterMargin - kMasterCancelWidth, yButtons,
		kMasterCancelWidth, kMasterButtonHeight, TRUE);
	m_btnSave.MoveWindow(cx - kMasterMargin - kMasterCancelWidth - kMasterRowGap - kMasterButtonWidth,
		yButtons, kMasterButtonWidth, kMasterButtonHeight, TRUE);
}

void CMerge20FormMasterDlg::UpdateCount()
{
	CString text;
	m_edText.GetWindowText(text);
	Merge20FormMaster master;
	int badLine = 0;
	const String content(text.GetString());
	if (Merge20FormMasterParseText(content, master, badLine))
	{
		const String kinds = strutils::to_str(Merge20FormMasterCountUniqueIds(master));
		const String rows = strutils::to_str(static_cast<int>(master.entries.size()));
		const String* args[] = { &kinds, &rows };
		m_stCount.SetWindowText(
			strutils::format_strings(_("Registered: %1 kinds (%2 rows)"), args, 2).c_str());
	}
	else
	{
		const String line = strutils::to_str(badLine);
		const String* args[] = { &line };
		m_stCount.SetWindowText(
			strutils::format_strings(_("Line %1 has no tab (ID and form name must be separated by a tab)."), args, 1).c_str());
	}
}

void CMerge20FormMasterDlg::OnTextChanged()
{
	UpdateCount();
}

void CMerge20FormMasterDlg::OnResetToDefault()
{
	const Merge20FormMaster def = Merge20FormMasterGetDefault();
	const String kinds = strutils::to_str(Merge20FormMasterCountUniqueIds(def));
	const String rows = strutils::to_str(static_cast<int>(def.entries.size()));
	const String* args[] = { &kinds, &rows };
	if (AfxMessageBox(
			strutils::format_strings(_("Restore the name master to the default list (%1 kinds, %2 rows)? Your edits will be lost."), args, 2).c_str(),
			MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;
	m_edText.SetWindowText(Merge20FormMasterGetDefaultText().c_str());
	UpdateCount();
}

void CMerge20FormMasterDlg::OnOK()
{
	CString text;
	m_edText.GetWindowText(text);
	const String content(text.GetString());
	Merge20FormMaster master;
	int badLine = 0;
	// A line without a tab warns and aborts the save (design
	// section 8.2); the dialog stays open.
	if (!Merge20FormMasterParseText(content, master, badLine))
	{
		const String line = strutils::to_str(badLine);
		const String* args[] = { &line };
		AfxMessageBox(
			strutils::format_strings(_("Line %1 has no tab (ID and form name must be separated by a tab). Saving was cancelled."), args, 1).c_str(),
			MB_ICONWARNING);
		return;
	}
	String errDetail;
	if (!Merge20FormMasterSaveText(Merge20FormMasterToText(master), errDetail))
	{
		AfxMessageBox(
			strutils::format_string1(_("Could not save the name master: %1"), errDetail).c_str(),
			MB_ICONSTOP);
		return;
	}
	m_bSaved = true;
	__super::OnOK();
}
