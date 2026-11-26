// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Split.h
 *
 * @brief Per-row split / regroup of a diff group (Merge20 Phase 4 D5,
 * design sections 3.8, 4.2 and 5.2/D5, spec sections 3 (3/4) and 5.2).
 *
 * The HTML tool's `splitHunk` (17_v17.15.html, line 1666) gives every
 * row of a diff group its own hunk id and carries the group's
 * judgment to each row; `regroupHunk` (line 1678) puts the rows back
 * and keeps the first row's judgment. The fork does the same by
 * splitting the DIFFRANGE itself (T-4-10 decision): the result pane
 * builds one segment per DIFFRANGE, so a split diff list gives
 * per-row segments, per-row judgments (ResultChooseSources),
 * per-row session judgments and per-row learning candidates for
 * free, with no shadow structure.
 *
 * The sub-ranges are derived from the parent range alone, using the
 * ghost layout PrimeTextBuffers() builds (MergeDoc.cpp): inside one
 * diff, every pane's real lines occupy the first lines of the
 * apparent range [dbegin, dend] and the ghost lines fill the tail,
 * so apparent line dbegin + k is real line begin[p] + k of every
 * pane that still has a line there, and a ghost line of the panes
 * that ran out. A sub-range therefore has dbegin == dend ==
 * dbegin + k, blank[p] == -1 for a pane with a line at row k and
 * blank[p] == dbegin + k for a pane without one, and the empty side
 * of a one-sided row follows the diffutils convention (end ==
 * begin - 1, see DiffWrapper.cpp:1503). The rows tile the parent's
 * apparent range and every pane's real range contiguously, so the
 * compare views (whose line flags PrimeTextBuffers set for the
 * whole parent block) need no change.
 *
 * This module is deliberately free of MFC and of the translation
 * macro `_()`: the same code is compiled into WinMergeU and into
 * the GoogleTest runner. It includes DiffList.h directly (like
 * Merge20Classify.h): that header is MFC-free.
 */
#pragma once

#include <vector>
#include "DiffList.h"
#include "Merge20/Merge20Types.h"

/**
 * @brief Apparent (ghost-inclusive) row count of a diff range: the
 * number of per-row hunks a split produces (the longest side's real
 * line count, which is also dend - dbegin + 1 once the buffers are
 * primed).
 * @param [in] range Diff range to measure.
 * @param [in] nBuffers Compared panes (2 or 3); panes at or above
 *             this index are the unused third pane of a 2-way diff
 *             (begin == end == -1) and are not counted.
 * @return Row count (>= 0).
 */
int Merge20SplitRowCount(const DIFFRANGE& range, int nBuffers);

/**
 * @brief Whether a diff range can be split into per-row hunks.
 * @param [in] range Diff range to check.
 * @param [in] nBuffers Compared panes (2 or 3).
 * @return True when the range is a significant diff of at least two
 *         apparent rows whose apparent span agrees with its real
 *         row count. Ignored (OP_TRIVIAL) and empty (OP_NONE)
 *         ranges, single-row ranges and ranges whose buffers were
 *         never primed (dbegin/dend unset) are not splittable.
 */
bool Merge20SplitIsSplittable(const DIFFRANGE& range, int nBuffers);

/**
 * @brief Split one diff range into per-row ranges.
 * @param [in] parent Diff range to split.
 * @param [in] nBuffers Compared panes (2 or 3).
 * @param [out] rowsOut Per-row ranges, in order. Each row covers
 *             one apparent line; a row's op follows which panes
 *             have a line there (all panes -> the parent's op; one
 *             pane -> that pane's OP_*ONLY; two of three panes ->
 *             OP_DIFF, which keeps the conflict semantics because
 *             the op taxonomy has no two-of-three value).
 * @return True when the range was splittable and rowsOut holds the
 *         rows; false (rowsOut empty) otherwise.
 */
bool Merge20SplitRange(const DIFFRANGE& parent, int nBuffers,
	std::vector<DIFFRANGE>& rowsOut);

/**
 * @brief Build the split-group record for a split range (design
 * section 4.2 layer 3: the original range plus the row count, so a
 * regroup can restore it and a session can re-apply the split).
 * @param [in] parent Original diff range.
 * @param [in] rows Per-row hunks it was split into (>= 2).
 * @return Group record for the document sidecar and the session file.
 */
Merge20SplitGroup Merge20SplitGroupFromRange(const DIFFRANGE& parent, int rows);

/**
 * @brief Convert a stored split group back to its original diff range.
 * @param [in] group Split-group record.
 * @return Original DIFFRANGE (op, real ranges, apparent range, blanks).
 */
DIFFRANGE Merge20SplitGroupToRange(const Merge20SplitGroup& group);

/**
 * @brief Whether a diff range is the first row of a split group.
 * The first row keeps the group's real-line starts and apparent
 * start, so it identifies the group without an index (indices
 * shift when another group is split or regrouped).
 * @param [in] group Split-group record.
 * @param [in] range Candidate first row.
 * @return True when range is the group's first row.
 */
bool Merge20SplitGroupMatchesFirstRow(const Merge20SplitGroup& group,
	const DIFFRANGE& range);
