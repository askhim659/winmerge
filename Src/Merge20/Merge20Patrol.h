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
