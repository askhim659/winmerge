// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20Patrol.h
 *
 * @brief Unreviewed-only patrol (Merge20 Phase 4 D3, design sections
 * 3.9 and 5.2/D3, spec sections 4(g) and 5.4).
 *
 * Pure module (no MFC, no `_()`): the index arithmetic for patrolling
 * only the unreviewed rows. The same code is compiled into WinMergeU
 * and into the GoogleTest runner.
 *
 * Port of the HTML tool's `navFile` TODO-only branch
 * (17_v17.15.html, lines 1420-1429): with `todoOnly` on, Prev/Next and
 * the Up/Down keys visit only the rows whose review state is TODO,
 * wrapping around both ends; with no TODO row the HTML tool shows a
 * toast ("未確認のファイルはありません"). The callers (CDirView for
 * files, CMergeDiffNavigation for diffs) build the `isTarget` vector
 * from their own rows and translate the result: -1 means "no target
 * at all" (the callers show "No unreviewed files" instead of the
 * toast), and a result equal to `current` means "the current row is
 * the only target" (the HTML tool returns early without moving).
 */
#pragma once

#include <vector>
#include "Merge20Types.h"

/**
 * @brief Find the next patrol target in the given direction.
 * @param [in] isTarget One entry per row in patrol order; true marks a
 *             patrol target (an unreviewed file, or an unresolved diff).
 * @param [in] current Index of the current row, or -1 when none is
 *             current. Out-of-range values are treated as -1.
 * @param [in] direction +1 for the next target, -1 for the previous
 *             one. Any other value returns -1.
 * @return Index of the next target in the given direction, wrapping
 *         around both ends; `current` when it is the only target;
 *         -1 when there is no target at all.
 */
int Merge20FindNextTarget(const std::vector<bool>& isTarget, int current, int direction);

/**
 * @brief Whether a patrol move to a different target exists.
 * @return True when at least one target other than `current` exists
 *         (false when there is no target, or `current` is the only
 *         one). Used to enable/disable Prev/Next.
 */
bool Merge20HasOtherTarget(const std::vector<bool>& isTarget, int current);

/**
 * @brief Number of patrol targets (the `m` of the dedicated bar's
 * file name + (n/m) display).
 */
int Merge20CountTargets(const std::vector<bool>& isTarget);

/**
 * @brief 1-based ordinal of `current` among the targets (the `n` of
 * the dedicated bar's (n/m) display).
 * @return The ordinal when `current` is a target, 0 when it is not
 *         (e.g. TODO-only is on but the current file is already
 *         reviewed) or when there is no current row.
 */
int Merge20OrdinalOfTarget(const std::vector<bool>& isTarget, int current);

/**
 * @brief Patrol rank of one row (file) or one diff in risk order
 * (Merge20 best-workflow B3, pillar 2: "review the dangerous diffs
 * first", gap analysis section 4.2 B3).
 *
 * Lower is visited first: High before Medium before None, and an
 * unclassified file (never opened, so S4 never computed a risk)
 * last, the same rule as the S4 risk-column sort (ColRiskSort in
 * DirViewColItems.cpp, where an unclassified file sorts after
 * High). Learning candidates never change the rank (gap analysis
 * section 3.1: the risk axis and the learning axis are
 * independent).
 */
enum class Merge20RiskRank
{
	HIGH = 0, /**< RiskLevel::HIGH. */
	MEDIUM = 1, /**< RiskLevel::MEDIUM. */
	NONE = 2, /**< RiskLevel::NONE. */
	UNCLASSIFIED = 3 /**< No S4 classification yet (file never opened). */
};

/**
 * @brief Rank of one S4 risk level (files and diffs share it).
 */
Merge20RiskRank Merge20RiskRankOfRisk(RiskLevel risk);

/**
 * @brief Rank of one file from its S4 classification: the risk rank
 * when classified, UNCLASSIFIED when never opened.
 */
Merge20RiskRank Merge20RiskRankOfFile(bool classified, RiskLevel risk);

/**
 * @brief Patrol order of the target rows/diffs in risk order.
 * @param [in] ranks One rank per row/diff, in list/diff order.
 * @param [in] isTarget One entry per row/diff, in the same order;
 *             true marks a patrol target (same meaning as in
 *             Merge20FindNextTarget).
 * @return Target indices in patrol order: by rank (High, Medium,
 *         None, unclassified), stable within one rank, so files and
 *         diffs keep their list/diff order among equal risks.
 *         Entries past the shorter of the two vectors count as
 *         unclassified, so a ragged input never crashes.
 */
std::vector<int> Merge20BuildRiskOrder(const std::vector<Merge20RiskRank>& ranks, const std::vector<bool>& isTarget);

/**
 * @brief Find the next patrol target in a precomputed order.
 * @param [in] order Target indices in patrol order (e.g. from
 *             Merge20BuildRiskOrder, or list order for the plain
 *             D3 patrol).
 * @param [in] current Index of the current row/diff, or -1 when none
 *             is current. A current row/diff that is not in the
 *             order (e.g. a reviewed file while TODO-only is on,
 *             or a directory row) steps from its list/diff
 *             position, like Merge20FindNextTarget scans from the
 *             current row: forward takes the first entry of the
 *             order past that position, backward the last entry
 *             before it. For a list-ordered order this is exactly
 *             Merge20FindNextTarget, so the plain D3 patrol is
 *             unchanged; for a risk-ordered order the risk
 *             priority still rules (the scan follows the patrol
 *             sequence, not the list).
 * @param [in] direction +1 for the next target, -1 for the previous
 *             one. Any other value returns -1.
 * @param [in] wrap True wraps around both ends (the D3 TODO-only
 *             patrol); false stops at the ends and returns -1 (the
 *             plain list-order patrol, which never wraps).
 * @return Index of the next target; `current` when it is the only
 *         target and wrapping is on; -1 when there is no target at
 *         all, or the end was reached without wrapping.
 */
int Merge20FindNextInOrder(const std::vector<int>& order, int current, int direction, bool wrap);

/**
 * @brief 1-based ordinal of `current` in a precomputed order (the `n`
 * of the dedicated bar's (n/m) display in risk order).
 * @return The ordinal when `current` is in the order, 0 when it is
 *         not or when there is no current row/diff.
 */
int Merge20OrdinalInOrder(const std::vector<int>& order, int current);
