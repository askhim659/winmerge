// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20ImpactSearch.cpp
 *
 * @brief Implementation of the background impact search (Merge20
 * best-workflow B5, PR1).
 *
 * The worker mirrors Merge20GrepWorker (Merge20GrepDlg.cpp) line
 * for line in its file-reading idiom: codepage_detect::Guess +
 * UniMemFile + ReadString, binary skip on a NUL byte in the first
 * 64KB of a single-byte file, CoInitializeEx for the MLang
 * detector, stop-flag checks every 1024 lines. The only B5
 * differences are the per-line step (feed the line to the
 * Merge20ImpactCollector instead of one query match) and the
 * self-file skip (the judged file's lines never reach the
 * collector, so they are not even matched).
 *
 * This file stays ASCII: no Japanese literal is needed here (the
 * estimate-note wording is a msgid constant in Merge20Impact.h;
 * the Japanese rendering lives in the .po file).
 */
#include "StdAfx.h"
#include "Merge20/Merge20ImpactSearch.h"
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

namespace
{
/** @brief Bytes scanned for the binary (NUL) check (D7 value). */
constexpr int64_t ImpactBinaryProbeBytes = 65536;
/** @brief Stop-flag check interval while reading one file (lines, D7 value). */
constexpr int ImpactStopCheckLines = 1024;
}

/**
 * @brief Scan worker (file-local, Merge20GrepWorker precedent).
 * Owns its copies of the file list and the encoding-guess option;
 * talks to the search only through its public worker-thread
 * interface (ScanLine / ShouldStop / IncrementFilesScanned /
 * NotifyScanFinished). Never touches a CWnd or a CMergeDoc.
 */
class Merge20ImpactSearchWorker : public Poco::Runnable
{
public:
	Merge20ImpactSearchWorker(CMerge20ImpactSearch* pSearch, std::vector<Merge20GrepFile> files, int nGuessEncodingType)
		: m_pSearch(pSearch)
		, m_files(std::move(files))
		, m_nGuessEncodingType(nGuessEncodingType)
	{
	}
	void run() override;

private:
	void ScanFile(const Merge20GrepFile& file, int pane);

	CMerge20ImpactSearch* m_pSearch;
	std::vector<Merge20GrepFile> m_files;
	int m_nGuessEncodingType;
};

void Merge20ImpactSearchWorker::run()
{
	// COM for this thread (MLang detector behind Guess; see the
	// file header). A failed init only degrades the guess.
	const HRESULT hrCo = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	for (const Merge20GrepFile& file : m_files)
	{
		if (m_pSearch->ShouldStop())
			break;
		for (int pane = 0; pane < static_cast<int>(file.paths.size()); ++pane)
		{
			if (file.paths[static_cast<size_t>(pane)].empty())
				continue;
			if (m_pSearch->ShouldStop())
				break;
			ScanFile(file, pane);
			if (m_pSearch->ShouldStop())
				break;
		}
		m_pSearch->IncrementFilesScanned();
	}
	if (SUCCEEDED(hrCo))
		::CoUninitialize();
	m_pSearch->NotifyScanFinished();
}

void Merge20ImpactSearchWorker::ScanFile(const Merge20GrepFile& file, int pane)
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
		const int64_t check = std::min<int64_t>(size, ImpactBinaryProbeBytes);
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
		// A false return means the collector is full or a stop was
		// requested: stop scanning this file at once.
		if (!m_pSearch->ScanLine(file.relPath, pane, nLine, line))
			break;
		++nLine;
		if ((nLine & (ImpactStopCheckLines - 1)) == 0 && m_pSearch->ShouldStop())
			break;
	} while (!done);
}

CMerge20ImpactSearch::CMerge20ImpactSearch()
	: m_pSink(nullptr)
	, m_nGuessEncodingType(0)
	, m_nFilesTotal(0)
	, m_nFilesScanned(0)
	, m_nHitCount(0)
	, m_bStop(false)
	, m_bRunning(false)
{
}

CMerge20ImpactSearch::~CMerge20ImpactSearch()
{
	// Belt and braces: Stop() already joined the worker, but the
	// search object may outlive its owner (member of the dock).
	Stop();
}

void CMerge20ImpactSearch::Start(std::vector<Merge20GrepFile> files, std::vector<String> symbols,
	String currentRelPath, int guessEncodingType, IMerge20ImpactSearchSink* pSink)
{
	// A previous scan (finished or stopped) is joined here so the
	// new pool starts clean.
	Stop();
	m_bStop = false;
	m_files = std::move(files);
	m_nFilesTotal = static_cast<int>(m_files.size());
	m_nFilesScanned = 0;
	m_nHitCount = 0;
	m_nGuessEncodingType = guessEncodingType;
	m_pSink = pSink;
	m_collector = Merge20ImpactCollector(std::move(symbols), std::move(currentRelPath));
	if (m_collector.GetSymbols().empty())
	{
		// No symbol S4 could name: nothing to search for. Report
		// the empty result at once, without starting a worker.
		if (m_pSink != nullptr)
			m_pSink->OnMerge20ImpactFinished(m_collector.TakeResult(0, m_nFilesTotal));
		return;
	}
	m_pWorker = std::make_shared<Merge20ImpactSearchWorker>(this, m_files, m_nGuessEncodingType);
	m_pPool = std::make_unique<Poco::ThreadPool>(1, 1);
	m_bRunning = true;
	m_pPool->start(*m_pWorker);
}

void CMerge20ImpactSearch::Stop()
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

bool CMerge20ImpactSearch::ShouldStop() const
{
	return m_bStop.load() || m_collector.IsFull();
}

bool CMerge20ImpactSearch::ScanLine(const String& relPath, int pane, int line, const String& text)
{
	if (ShouldStop())
		return false;
	const std::vector<Merge20ImpactHit> added = m_collector.AddLine(relPath, pane, line, text);
	m_nHitCount += static_cast<int>(added.size());
	if (m_pSink != nullptr)
	{
		for (const Merge20ImpactHit& hit : added)
			m_pSink->OnMerge20ImpactHit(hit);
	}
	return !ShouldStop();
}

void CMerge20ImpactSearch::NotifyScanFinished()
{
	m_bRunning = false;
	if (m_pSink != nullptr)
		m_pSink->OnMerge20ImpactFinished(m_collector.TakeResult(m_nFilesScanned.load(), m_nFilesTotal));
}
