/**
 * @file Merge20Measure_test.cpp
 *
 * @brief Merge20 (Phase 4 D0) unit tests for the folder-compare
 * timing hooks and for the Phase 4 option key names.
 *
 * The timing tests use fixed time points for the pure computation
 * (no sleeps, no flakiness) and the real clock only to check that
 * the Begin/ListFinalized flow stores a measurement.
 *
 * The option-key test checks the registry/INI names of the eleven
 * new OPT_MERGE20_* keys (OptionsDef.h). Their *defaults* live in
 * OptionsInit.cpp, which is not linked into the UnitTests runner
 * (its include closure pulls in the whole options initialisation),
 * so the defaults are verified by the CI build (the InitOption
 * calls are type-checked there) and on a real machine by reading
 * the values back from the INI file / registry after the first run
 * (see MERGE20_NOTES.md section 22).
 *
 * This file stays ASCII: the Japanese text is not needed here.
 */
#include "pch.h"
#include <gtest/gtest.h>
#include "Merge20/Merge20Measure.h"
#include "OptionsDef.h"

namespace
{

using Clock = std::chrono::steady_clock;

Clock::time_point At(long long ms)
{
	return Clock::time_point(Clock::duration(std::chrono::milliseconds(ms)));
}

} // namespace

TEST(Merge20Measure, ComputeResultWithScanPoint)
{
	const Merge20MeasureResult r = Merge20MeasureComputeResult(At(1000), At(2200), At(2234), 100, true);
	EXPECT_TRUE(r.valid);
	EXPECT_TRUE(r.hasScanPoint);
	EXPECT_EQ(r.totalMs, 1234);
	EXPECT_EQ(r.scanMs, 1200);
	EXPECT_EQ(r.listMs, 34);
	EXPECT_EQ(r.totalItems, 100);
}

TEST(Merge20Measure, ComputeResultWithoutScanPoint)
{
	const Merge20MeasureResult r = Merge20MeasureComputeResult(At(1000), At(1000), At(2234), 500, false);
	EXPECT_TRUE(r.valid);
	EXPECT_FALSE(r.hasScanPoint);
	EXPECT_EQ(r.totalMs, 1234);
	EXPECT_EQ(r.scanMs, 1234);
	EXPECT_EQ(r.listMs, 0);
	EXPECT_EQ(r.totalItems, 500);
}

TEST(Merge20Measure, ComputeResultZeroElapsed)
{
	const Merge20MeasureResult r = Merge20MeasureComputeResult(At(1000), At(1000), At(1000), 0, true);
	EXPECT_TRUE(r.valid);
	EXPECT_EQ(r.totalMs, 0);
	EXPECT_EQ(r.scanMs, 0);
	EXPECT_EQ(r.listMs, 0);
}

TEST(Merge20Measure, BeginListFinalizedFlow)
{
	Merge20MeasureReset();
	EXPECT_FALSE(Merge20MeasureHasResult());
	EXPECT_EQ(Merge20MeasureFormatLast(), "Merge20 measure: no measurement yet");

	Merge20MeasureBeginCompare();
	Merge20MeasureCompareCompleted();
	Merge20MeasureListFinalized(42);
	EXPECT_TRUE(Merge20MeasureHasResult());
	const Merge20MeasureResult r = Merge20MeasureGetLast();
	EXPECT_TRUE(r.valid);
	EXPECT_TRUE(r.hasScanPoint);
	EXPECT_GE(r.totalMs, 0);
	EXPECT_GE(r.scanMs, 0);
	EXPECT_GE(r.listMs, 0);
	EXPECT_EQ(r.totalItems, 42);
	EXPECT_NE(Merge20MeasureFormatLast().find("Merge20 measure: folder compare "), std::string::npos);
	EXPECT_NE(Merge20MeasureFormatLast().find("42 items"), std::string::npos);
}

TEST(Merge20Measure, ListFinalizedWithoutBeginIsIgnored)
{
	Merge20MeasureReset();
	Merge20MeasureCompareCompleted();
	Merge20MeasureListFinalized(7);
	EXPECT_FALSE(Merge20MeasureHasResult());
	EXPECT_FALSE(Merge20MeasureGetLast().valid);
}

TEST(Merge20Measure, BeginRestartsMeasurement)
{
	Merge20MeasureReset();
	Merge20MeasureBeginCompare();
	Merge20MeasureListFinalized(1);
	EXPECT_TRUE(Merge20MeasureHasResult());
	Merge20MeasureBeginCompare();
	EXPECT_FALSE(Merge20MeasureHasResult());
}

TEST(Merge20Measure, FormatWithoutScanPointOmitsSplit)
{
	Merge20MeasureReset();
	Merge20MeasureBeginCompare();
	Merge20MeasureListFinalized(3);
	EXPECT_TRUE(Merge20MeasureHasResult());
	EXPECT_FALSE(Merge20MeasureGetLast().hasScanPoint);
	EXPECT_EQ(Merge20MeasureFormatLast().find("scan "), std::string::npos);
	EXPECT_NE(Merge20MeasureFormatLast().find("3 items"), std::string::npos);
}

TEST(Merge20Phase4D0Options, KeyNames)
{
	// Registry/INI names (design section 4.1). The defaults are in
	// OptionsInit.cpp; see the file comment above for why they are
	// not asserted here.
	EXPECT_EQ(OPT_MERGE20_DED_MODE, _T("Merge20/DedMode"));
	EXPECT_EQ(OPT_MERGE20_DED_PINNED, _T("Merge20/DedPinned"));
	EXPECT_EQ(OPT_MERGE20_DED_ZOOM, _T("Merge20/DedZoom"));
	EXPECT_EQ(OPT_MERGE20_TODO_ONLY, _T("Merge20/TodoOnly"));
	EXPECT_EQ(OPT_MERGE20_SNIPER_ON, _T("Merge20/SniperOn"));
	EXPECT_EQ(OPT_MERGE20_SNIPER_FONT, _T("Merge20/SniperFont"));
	EXPECT_EQ(OPT_MERGE20_SNIPER_LAYOUT, _T("Merge20/SniperLayout"));
	EXPECT_EQ(OPT_MERGE20_SLIM, _T("Merge20/Slim"));
	EXPECT_EQ(OPT_MERGE20_DOCK_HIDDEN, _T("Merge20/DockHidden"));
	EXPECT_EQ(OPT_MERGE20_FOCUS_DOCK_HEIGHT, _T("Merge20/FocusDockHeight"));
	EXPECT_EQ(OPT_MERGE20_GREP_HISTORY, _T("Merge20/GrepHistory"));
}
