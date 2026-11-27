// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCompareDoc.cpp
 *
 * @brief Implementation of the FORM compare document (Wave 5 F2).
 * The folder scan and the file reading follow the Merge20 Grep
 * precedent (Merge20GrepDlg.cpp): `codepage_detect::Guess` with
 * the compare's OPT_CP_DETECT setting, then `UniMemFile` +
 * `ReadString` (the CDiffTextBuffer::LoadFromFile loop idiom).
 * The folder walk is recursive (the HTML tool's webkitdirectory
 * reads subfolders too).
 */
#include "pch.h"
#include "Merge20/Merge20FormCompareDoc.h"
#include "Merge20/Merge20FormCompareView.h"
#include "Merge.h"
#include "MergeApp.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"
#include "codepage_detect.h"
#include "UniFile.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <map>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{

	// Lower-case copy (towlower loop, Grep precedent).
	std::wstring FormToLower(const std::wstring& text)
	{
		std::wstring out(text);
		for (wchar_t& ch : out)
			ch = std::towlower(ch);
		return out;
	}

	// True when the file has a .xml extension (case-insensitive).
	bool IsXmlFile(const std::filesystem::path& path)
	{
		return FormToLower(path.extension().wstring()) == L".xml";
	}

	// Read one XML file into text with WinMerge's charset
	// detection (design section 3.3). Returns false when the file
	// cannot be opened; the caller then diffs an empty text, which
	// the F0 parser reports as an ERROR record.
	bool ReadXmlFileText(const String& filePath, String& outText)
	{
		outText.clear();
		const FileTextEncoding encoding = codepage_detect::Guess(filePath, GetOptionsMgr()->GetInt(OPT_CP_DETECT));
		UniMemFile uf;
		if (!uf.OpenReadOnly(filePath))
			return false;
		uf.SetUnicoding(encoding.m_unicoding);
		uf.SetCodepage(encoding.m_codepage);
		uf.SetBom(encoding.m_bom);
		if (encoding.m_bom)
			uf.ReadBom();
		// CDiffTextBuffer::LoadFromFile loop idiom: ReadString
		// returns false on the last line but still delivers it.
		String line, eol;
		bool done = false;
		do
		{
			bool lossy = false;
			done = !uf.ReadString(line, eol, &lossy);
			outText += line;
			outText += eol;
		} while (!done);
		return true;
	}

	// One side of the folder union: the relative path as found
	// (old side preferred for display) and the full path per side.
	struct FormScanEntry
	{
		String displayRelPath; /**< Relative path as found (old side preferred). */
		String oldFullPath; /**< Empty when the file is new-only. */
		String newFullPath; /**< Empty when the file is old-only. */
	};

	// Collect the .xml files of one folder, keyed by the lowered
	// relative path (Windows file names compare case-insensitively,
	// so `A.xml` and `a.xml` are the same file). Unreadable
	// subfolders are skipped (error_code overload, no exceptions).
	void ScanFolderXmlFiles(const String& folder,
		std::map<std::wstring, FormScanEntry>& entries, bool isOldSide)
	{
		std::error_code ec;
		const std::filesystem::path root(folder);
		std::filesystem::recursive_directory_iterator it(root, ec);
		if (ec)
			return;
		const std::filesystem::recursive_directory_iterator end;
		for (; it != end; it.increment(ec))
		{
			if (ec)
				break;
			const std::filesystem::path& path = it->path();
			std::error_code ec2;
			if (!it->is_regular_file(ec2) || ec2)
				continue;
			if (!IsXmlFile(path))
				continue;
			const std::filesystem::path rel = std::filesystem::relative(path, root, ec2);
			if (ec2)
				continue;
			const std::wstring relText = rel.wstring();
			const std::wstring key = FormToLower(relText);
			FormScanEntry& entry = entries[key];
			if (entry.displayRelPath.empty())
				entry.displayRelPath = relText;
			if (isOldSide)
				entry.oldFullPath = path.wstring();
			else
				entry.newFullPath = path.wstring();
		}
	}

} // namespace

IMPLEMENT_DYNCREATE(CFormCompareDoc, CDocument)

BEGIN_MESSAGE_MAP(CFormCompareDoc, CDocument)
END_MESSAGE_MAP()

CFormCompareDoc::CFormCompareDoc()
{
}

CFormCompareDoc::~CFormCompareDoc()
{
}

BOOL CFormCompareDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;
	return TRUE;
}

void CFormCompareDoc::DeleteContents()
{
	m_bAnalyzed = false;
	m_totalFiles = 0;
	m_results.clear();
	m_groups.clear();
	m_ignoredNoise.clear();
	m_kpi = Merge20FormKpi();
	m_visibleIndices.clear();
	CDocument::DeleteContents();
}

void CFormCompareDoc::SetFolders(const String& leftFolder, const String& rightFolder)
{
	m_strLeftFolder = leftFolder;
	m_strRightFolder = rightFolder;
}

bool CFormCompareDoc::RunAnalysis()
{
	DeleteContents();
	if (!HasFolders())
		return false;
	std::map<std::wstring, FormScanEntry> entries;
	ScanFolderXmlFiles(m_strLeftFolder, entries, true);
	ScanFolderXmlFiles(m_strRightFolder, entries, false);
	if (entries.empty())
	{
		// No .xml file at all: only a success when both folders
		// at least exist (an empty comparison). A missing folder
		// is a failure, not an empty result.
		std::error_code ecLeft, ecRight;
		if (!std::filesystem::is_directory(std::filesystem::path(m_strLeftFolder), ecLeft) ||
			!std::filesystem::is_directory(std::filesystem::path(m_strRightFolder), ecRight))
			return false;
	}
	const int totalFiles = static_cast<int>(entries.size());
	m_totalFiles = totalFiles;
	for (const auto& pair : entries)
	{
		const FormScanEntry& entry = pair.second;
		String oldText, newText;
		const std::wstring* pOld = nullptr;
		const std::wstring* pNew = nullptr;
		if (!entry.oldFullPath.empty())
		{
			// An unreadable file diffs as an empty text: the F0
			// parser reports it as an ERROR record.
			ReadXmlFileText(entry.oldFullPath, oldText);
			pOld = &oldText;
		}
		if (!entry.newFullPath.empty())
		{
			ReadXmlFileText(entry.newFullPath, newText);
			pNew = &newText;
		}
		Merge20FormDiffOutcome outcome = Merge20DiffFormFiles(pOld, pNew, entry.displayRelPath);
		// Identical files are counted, never listed (design
		// section 2.1): only a changed file reaches the list.
		if (outcome.result.changeCount <= 0)
			continue;
		m_results.push_back(outcome.result);
		m_groups.push_back(outcome.groups);
		m_ignoredNoise.push_back(outcome.ignoredNoiseCount);
	}
	// The large-change flag (design section 4.4) runs over the
	// whole folder result before the KPI is computed.
	Merge20ApplyLargeChangeFlag(m_results);
	RefreshKpi();
	m_bAnalyzed = true;
	RefreshVisible();
	UpdateAllViews(nullptr);
	return true;
}

int CFormCompareDoc::GetIgnoredNoiseCount(size_t index) const
{
	if (index >= m_ignoredNoise.size())
		return 0;
	return m_ignoredNoise[index];
}

void CFormCompareDoc::SetFilter(Merge20FormFilter filter)
{
	if (m_filter == filter)
		return;
	m_filter = filter;
	RefreshVisible();
	UpdateAllViews(nullptr);
}

void CFormCompareDoc::SetSearchText(const String& text)
{
	if (m_strSearchText == text)
		return;
	m_strSearchText = text;
	m_strSearchLower = Merge20FormListLowerQuery(text);
	RefreshVisible();
	UpdateAllViews(nullptr);
}

void CFormCompareDoc::SetSortOrder(Merge20FormSort sort)
{
	if (m_sortOrder == sort)
		return;
	m_sortOrder = sort;
	RefreshVisible();
	UpdateAllViews(nullptr);
}

void CFormCompareDoc::RefreshVisible()
{
	m_visibleIndices = Merge20FormListApplyFilterSort(m_results, m_filter, m_strSearchLower, m_sortOrder);
}

void CFormCompareDoc::UpdateResources()
{
	POSITION pos = GetFirstViewPosition();
	while (pos != nullptr)
	{
		CFormCompareView* pView = static_cast<CFormCompareView*>(GetNextView(pos));
		if (pView != nullptr)
			pView->UpdateResources();
	}
	SetTitle(_("FORM Compare (view only)"));
}

void CFormCompareDoc::RefreshKpi()
{
	m_kpi = Merge20FormListComputeKpi(m_totalFiles, m_results);
}

CFormCompareDoc* Merge20FormCompareOpenFolders(const String& leftFolder, const String& rightFolder)
{
	CMultiDocTemplate* pTemplate = theApp.GetFormCompareTemplate();
	if (pTemplate == nullptr)
		return nullptr;
	CFormCompareDoc* pDoc = static_cast<CFormCompareDoc*>(pTemplate->OpenDocumentFile(nullptr));
	if (pDoc == nullptr)
		return nullptr;
	pDoc->SetFolders(leftFolder, rightFolder);
	pDoc->SetTitle(_("FORM Compare (view only)"));
	pDoc->RunAnalysis();
	return pDoc;
}
