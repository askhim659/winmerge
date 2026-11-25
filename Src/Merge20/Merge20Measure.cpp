/**
 * @file  Merge20Measure.cpp
 *
 * @brief Implementation of the folder-compare timing hooks (D0).
 *
 * The three time points are kept as steady_clock ticks in atomics:
 * Begin runs on the UI thread (Rescan), CompareCompleted runs on
 * the compare thread (DiffThreadCallback) and ListFinalized runs on
 * the UI thread (OnUpdateUIMessage). The ordering between the
 * points is enforced by the message flow (the callback posts
 * MSG_UI_UPDATE before the UI thread rebuilds the list), so no
 * lock is needed; the atomics only keep the cross-thread reads
 * well-defined.
 *
 * This file stays ASCII: the formatted line is a developer
 * diagnostic and is not translated.
 */
#include "pch.h"
#include <chrono>
#include "Merge20/Merge20Measure.h"

namespace
{

using Clock = std::chrono::steady_clock;

long long Ticks(Clock::time_point t)
{
	return static_cast<long long>(t.time_since_epoch().count());
}

Clock::time_point FromTicks(long long ticks)
{
	return Clock::time_point(Clock::duration(ticks));
}

long long MsBetween(Clock::time_point from, Clock::time_point to)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count();
}

std::atomic<long long> s_beginTicks{ 0 };
std::atomic<long long> s_scanTicks{ 0 };
std::atomic<bool> s_hasBegin{ false };
std::atomic<bool> s_hasScan{ false };
std::atomic<bool> s_valid{ false };
std::atomic<long long> s_totalMs{ 0 };
std::atomic<long long> s_scanMs{ 0 };
std::atomic<long long> s_listMs{ 0 };
std::atomic<int> s_totalItems{ 0 };

} // namespace

void Merge20MeasureBeginCompare()
{
	s_beginTicks.store(Ticks(Clock::now()));
	s_hasBegin.store(true);
	s_hasScan.store(false);
	s_valid.store(false);
}

void Merge20MeasureCompareCompleted()
{
	if (!s_hasBegin.load())
		return;
	s_scanTicks.store(Ticks(Clock::now()));
	s_hasScan.store(true);
}

void Merge20MeasureListFinalized(int totalItems)
{
	if (!s_hasBegin.load())
		return;
	const Merge20MeasureResult r = Merge20MeasureComputeResult(FromTicks(s_beginTicks.load()),
		FromTicks(s_scanTicks.load()), Clock::now(), totalItems, s_hasScan.load());
	s_totalMs.store(r.totalMs);
	s_scanMs.store(r.scanMs);
	s_listMs.store(r.listMs);
	s_totalItems.store(r.totalItems);
	s_valid.store(true);
}

void Merge20MeasureReset()
{
	s_hasBegin.store(false);
	s_hasScan.store(false);
	s_valid.store(false);
	s_totalMs.store(0);
	s_scanMs.store(0);
	s_listMs.store(0);
	s_totalItems.store(0);
}

bool Merge20MeasureHasResult()
{
	return s_valid.load();
}

Merge20MeasureResult Merge20MeasureGetLast()
{
	Merge20MeasureResult r;
	r.valid = s_valid.load();
	r.hasScanPoint = s_hasScan.load();
	r.totalMs = s_totalMs.load();
	r.scanMs = s_scanMs.load();
	r.listMs = s_listMs.load();
	r.totalItems = s_totalItems.load();
	return r;
}

std::string Merge20MeasureFormatLast()
{
	const Merge20MeasureResult r = Merge20MeasureGetLast();
	if (!r.valid)
		return "Merge20 measure: no measurement yet";
	std::string line = "Merge20 measure: folder compare " + std::to_string(r.totalMs) + " ms";
	if (r.hasScanPoint)
	{
		line += " (scan " + std::to_string(r.scanMs) + " ms, list " + std::to_string(r.listMs) + " ms)";
	}
	line += ", " + std::to_string(r.totalItems) + " items";
	return line;
}

Merge20MeasureResult Merge20MeasureComputeResult(Clock::time_point begin,
	Clock::time_point scanCompleted, Clock::time_point listFinalized,
	int totalItems, bool hasScanPoint)
{
	Merge20MeasureResult r;
	r.valid = true;
	r.hasScanPoint = hasScanPoint;
	r.totalMs = MsBetween(begin, listFinalized);
	if (hasScanPoint)
	{
		r.scanMs = MsBetween(begin, scanCompleted);
		r.listMs = MsBetween(scanCompleted, listFinalized);
	}
	else
	{
		// No split available: the whole time is attributed to the scan.
		r.scanMs = r.totalMs;
		r.listMs = 0;
	}
	r.totalItems = totalItems;
	return r;
}
