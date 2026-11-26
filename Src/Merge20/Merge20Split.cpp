// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Split.cpp
 *
 * @brief Implementation of the per-row split / regroup of a diff
 * group (Merge20 Phase 4 D5). See Merge20Split.h for the derivation
 * of the sub-ranges from the parent's ghost layout.
 *
 * This file stays ASCII: it computes line ranges only and never
 * formats user-visible text.
 */
#include "pch.h"
#include "Merge20/Merge20Split.h"

namespace
{

	// Real line count of one pane of a range (0 for an empty side:
	// end == begin - 1, the diffutils convention).
	int RealLineCount(const DIFFRANGE& range, int pane)
	{
		const int count = range.end[pane] - range.begin[pane] + 1;
		return count > 0 ? count : 0;
	}

	// One-sided op for a row whose only line is in the given pane.
	OP_TYPE OnlyOp(int pane)
	{
		switch (pane)
		{
		case 0:
			return OP_1STONLY;
		case 1:
			return OP_2NDONLY;
		default:
			return OP_3RDONLY;
		}
	}

} // namespace

int Merge20SplitRowCount(const DIFFRANGE& range, int nBuffers)
{
	int rows = 0;
	for (int pane = 0; pane < nBuffers; ++pane)
	{
		const int count = RealLineCount(range, pane);
		if (count > rows)
			rows = count;
	}
	return rows;
}

bool Merge20SplitIsSplittable(const DIFFRANGE& range, int nBuffers)
{
	if (range.op == OP_NONE || range.op == OP_TRIVIAL)
		return false;
	if (nBuffers < 2 || nBuffers > 3)
		return false;
	const int rows = Merge20SplitRowCount(range, nBuffers);
	if (rows < 2)
		return false;
	// PrimeTextBuffers() sets dend - dbegin + 1 to the longest side's
	// real line count; a mismatch means the buffers were never primed
	// (dbegin/dend still 0), so no sub-range could be trusted.
	if (range.dend - range.dbegin + 1 != rows)
		return false;
	return true;
}

bool Merge20SplitRange(const DIFFRANGE& parent, int nBuffers,
	std::vector<DIFFRANGE>& rowsOut)
{
	rowsOut.clear();
	if (!Merge20SplitIsSplittable(parent, nBuffers))
		return false;
	const int rows = Merge20SplitRowCount(parent, nBuffers);
	int nlines[3] = { 0, 0, 0 };
	for (int pane = 0; pane < nBuffers; ++pane)
		nlines[pane] = RealLineCount(parent, pane);
	for (int k = 0; k < rows; ++k)
	{
		DIFFRANGE row;
		row.dbegin = parent.dbegin + k;
		row.dend = parent.dbegin + k;
		int panesWithLine = 0;
		int onlyPane = -1;
		for (int pane = 0; pane < 3; ++pane)
		{
			if (pane < nBuffers && k < nlines[pane])
			{
				row.begin[pane] = parent.begin[pane] + k;
				row.end[pane] = parent.begin[pane] + k;
				row.blank[pane] = -1;
				++panesWithLine;
				onlyPane = pane;
			}
			else if (pane < nBuffers)
			{
				// Empty side: the first line past this pane's real
				// lines, with end == begin - 1. This apparent line
				// is a ghost line for this pane.
				row.begin[pane] = parent.begin[pane] + nlines[pane];
				row.end[pane] = row.begin[pane] - 1;
				row.blank[pane] = parent.dbegin + k;
			}
			else
			{
				// Unused pane of a 2-way diff: keep the parent's
				// (begin == end == -1, blank == -1).
				row.begin[pane] = parent.begin[pane];
				row.end[pane] = parent.end[pane];
				row.blank[pane] = parent.blank[pane];
			}
		}
		if (panesWithLine == nBuffers)
			row.op = parent.op;
		else if (panesWithLine == 1)
			row.op = OnlyOp(onlyPane);
		else
			row.op = OP_DIFF;
		rowsOut.push_back(row);
	}
	return true;
}

Merge20SplitGroup Merge20SplitGroupFromRange(const DIFFRANGE& parent, int rows)
{
	Merge20SplitGroup group;
	group.op = static_cast<int>(parent.op);
	for (int pane = 0; pane < 3; ++pane)
	{
		group.begin[pane] = parent.begin[pane];
		group.end[pane] = parent.end[pane];
		group.blank[pane] = parent.blank[pane];
	}
	group.dbegin = parent.dbegin;
	group.dend = parent.dend;
	group.rows = rows;
	return group;
}

DIFFRANGE Merge20SplitGroupToRange(const Merge20SplitGroup& group)
{
	DIFFRANGE range;
	range.op = static_cast<OP_TYPE>(group.op);
	for (int pane = 0; pane < 3; ++pane)
	{
		range.begin[pane] = group.begin[pane];
		range.end[pane] = group.end[pane];
		range.blank[pane] = group.blank[pane];
	}
	range.dbegin = group.dbegin;
	range.dend = group.dend;
	return range;
}

bool Merge20SplitGroupMatchesFirstRow(const Merge20SplitGroup& group,
	const DIFFRANGE& range)
{
	if (group.rows < 2 || range.dbegin != group.dbegin)
		return false;
	for (int pane = 0; pane < 3; ++pane)
	{
		if (range.begin[pane] != group.begin[pane])
			return false;
	}
	return true;
}
