/**
 * @file  Merge20Measure.h
 *
 * @brief Folder-compare timing hooks (Merge20 Phase 4 D0, design
 * sections 3.11 and 5.2/D0, spec sections 7 and 9 Phase 4).
 *
 * The foundation for the D10 speed work: three connection points
 * record the compare start, the scan completion and the moment the
 * folder list is rebuilt on the UI thread, so that the elapsed time
 * from compare start to a finalized list can be measured on
 * 100/500/1000-file folder pairs (design section 8.3).
 *
 * Connection points (all in the folder-compare path):
 * - Begin: CDirDoc::Rescan(), next to `m_compareStart = clock();`
 * - Scan completed: CDirDoc::DiffThreadCallback(), in the
 *   EVENT_COMPARE_COMPLETED branch, next to the `m_elapsed`
 *   computation (the compare worker thread finished).
 * - List finalized: CDirView::OnUpdateUIMessage(), in the
 *   EVENT_COMPARE_COMPLETED branch, right after Redisplay()
 *   (the list was rebuilt and the focus/scroll position restored).
 *   The existing `m_elapsed` (DirDoc.cpp) only covers the scan;
 *   the list-finalized point is what the user actually waits for.
 *
 * The last measurement is kept in memory for D10 (which will show
 * it in the UI) and is also written to WinMerge's log
 * (RootLogger::Info) by the list-finalized hook site in DirView.cpp,
 * so it can be checked on a real machine without a debugger
 * (View the log after a folder compare).
 *
 * This module is deliberately free of MFC and of the translation
 * macro `_()`: the same code is compiled into WinMergeU and into
 * the GoogleTest runner. The formatted line is ASCII only.
 */
#pragma once

#include <atomic>
#include <chrono>
#include <string>

/** @brief Result of one folder-compare measurement. */
struct Merge20MeasureResult
{
	bool valid = false; /**< False when no complete measurement exists yet (Begin was never called, or ListFinalized ran before Begin). */
	bool hasScanPoint = false; /**< False when the scan-completed point was not recorded (then scanMs holds the whole time and listMs is 0). */
	long long totalMs = 0; /**< Compare start -> list finalized (milliseconds). */
	long long scanMs = 0; /**< Compare start -> scan completed (milliseconds). */
	long long listMs = 0; /**< Scan completed -> list finalized (milliseconds). */
	int totalItems = 0; /**< CompareStats total items at list-finalized time. */
};

/**
 * @brief Record the compare start (call from CDirDoc::Rescan()).
 *
 * Clears any previous measurement; a new Begin always starts a
 * fresh measurement even if the previous compare never finalized.
 */
void Merge20MeasureBeginCompare();

/**
 * @brief Record the scan-completed point (call from
 * CDirDoc::DiffThreadCallback() on EVENT_COMPARE_COMPLETED).
 *
 * Ignored when no compare was started.
 */
void Merge20MeasureCompareCompleted();

/**
 * @brief Record the list-finalized point and store the measurement
 * (call from CDirView::OnUpdateUIMessage() after Redisplay()).
 * @param [in] totalItems CompareStats total items at this point.
 *
 * Ignored when no compare was started.
 */
void Merge20MeasureListFinalized(int totalItems);

/**
 * @brief Clear the stored measurement (used by the unit tests).
 */
void Merge20MeasureReset();

/**
 * @brief Whether a complete measurement is stored.
 * @return True after ListFinalized ran for a started compare.
 */
bool Merge20MeasureHasResult();

/**
 * @brief The last stored measurement.
 * @return The result, or an invalid result when none exists yet.
 */
Merge20MeasureResult Merge20MeasureGetLast();

/**
 * @brief Format the last measurement as one ASCII log line.
 * @return e.g. "Merge20 measure: folder compare 1234 ms (scan 1200 ms, list 34 ms), 100 items",
 * or "Merge20 measure: no measurement yet" when none exists.
 */
std::string Merge20MeasureFormatLast();

/**
 * @brief Pure computation of a measurement from three time points.
 * @param [in] begin Compare start.
 * @param [in] scanCompleted Scan-completed point (ignored when hasScanPoint is false).
 * @param [in] listFinalized List-finalized point.
 * @param [in] totalItems CompareStats total items.
 * @param [in] hasScanPoint Whether scanCompleted was recorded.
 * @return The measurement (always valid).
 */
Merge20MeasureResult Merge20MeasureComputeResult(std::chrono::steady_clock::time_point begin,
	std::chrono::steady_clock::time_point scanCompleted, std::chrono::steady_clock::time_point listFinalized,
	int totalItems, bool hasScanPoint);
