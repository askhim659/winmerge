// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20VerifyGate.h
 *
 * @brief MFC adapter between merge documents and the B7
 * verification engine (Merge20 best-workflow B7, gap analysis
 * section 4.2 B7).
 *
 * Two layers, both in this file's .cpp:
 * - Build the engine input from a CMergeDoc (pane lines by real
 *   line number, saved-form result lines, one record per diff
 *   with its saved line range), and the representability
 *   predicate from the result buffer's encoding.
 * - The public gate API for S7 and for the merge-result save:
 *   `Merge20VerifyBeforeOutput` runs the engine for one merge
 *   doc and shows the problem-list dialog when there is a
 *   problem (true = proceed, false = stop). S7 (multi-file
 *   output) uses the lower-level pieces instead: check each doc
 *   with `Merge20VerifyCheckMergeDoc`, collect the rows with
 *   `Merge20VerifyCollectRows`, show one dialog for all files
 *   with `Merge20VerifyShowIssuesAndDecide`, and navigate with
 *   `Merge20VerifyJumpToIssue` -- so S7 calls the same engine
 *   and the same dialog, only the call site changes.
 *
 * Stock save is NOT gated here, deliberately: DoSave writes
 * the compared input panes, not the merge output, and gating
 * it would block legitimate editing of OLD/NEW and risk
 * interfering with the stock save/close paths (backup,
 * read-only handling, SaveModified). The merge output funnel
 * is SaveMergeResult, and that is the only place this gate
 * hooks in (see the call in MergeDocResultPane.cpp).
 */
#pragma once

#include <vector>
#include "Merge20/Merge20Verify.h"
#include "Merge20/Merge20VerifyDlg.h"

class CMergeDoc;
class CWnd;

/**
 * @brief Load the four per-check option keys
 * (Merge20/VerifyRediff etc., all default ON).
 */
Merge20VerifyOptions Merge20VerifyLoadOptions();

/**
 * @brief Build the engine input for one merge doc.
 * @param [out] input Engine input (panes, saved-form result
 *             lines, diff records).
 * @param [out] displayName File name for the dialog (file name
 *             only, not the full path).
 * @param [out] unpairedSurrogateLines 1-based result lines
 *             whose UTF-16 source has an unpaired surrogate
 *             (ucr::toUTF8 mangles those: a lone high surrogate
 *             is combined with whatever follows it, so the
 *             engine's UTF-8 validation cannot see them; the
 *             gate reports them itself in CheckMergeDoc).
 * @return False when there is no merge session to verify
 *         (nothing built yet): the caller treats that as clean.
 */
bool Merge20VerifyBuildInput(const CMergeDoc& doc, Merge20VerifyFileInput& input,
	String& displayName, std::vector<int>& unpairedSurrogateLines);

/**
 * @brief Run the engine for one merge doc (no UI).
 * @return The engine result plus any unpaired-surrogate issues
 *         found while building the input. `passed` is true when
 *         there is nothing to verify (no merge session) or no
 *         enabled check found a problem.
 */
Merge20VerifyResult Merge20VerifyCheckMergeDoc(const CMergeDoc& doc);

/**
 * @brief One file's check result, for S7 aggregation.
 */
struct Merge20VerifyFileResult
{
	int fileIndex = 0;  /**< Caller-side file index (S7: folder-list index) */
	String fileName;    /**< Display name of the file (always set: the NEW pane's file name when no merge result was built) */
	bool verified = true; /**< False when no merge result was built: the engine never ran for this file */
	Merge20VerifyResult result; /**< Engine result for the file */
};

/**
 * @brief Run the engine for one merge doc and keep its display
 * name with the result (one build, no re-read).
 */
Merge20VerifyFileResult Merge20VerifyCheckMergeDocFile(const CMergeDoc& doc, int fileIndex = 0);

/**
 * @brief Flatten per-file results into dialog rows.
 */
std::vector<Merge20VerifyIssueRow> Merge20VerifyCollectRows(
	const std::vector<Merge20VerifyFileResult>& files);

/**
 * @brief What the problem dialog decided.
 */
enum class Merge20VerifyGateDecision
{
	Close,   /**< Close: output stays stopped, nothing changes. */
	Proceed, /**< Output anyway (explicit override). */
	Jump,    /**< Jump to the selected row's file/diff. */
};

/**
 * @brief Show the problem-list dialog for the given rows.
 * @param [out] jumpFileIndex File index of the Jump target.
 * @param [out] jumpDiff Diff index of the Jump target (-1 when
 *             the row has no diff; jumpResultLine then holds the
 *             result line instead).
 * @param [out] jumpResultLine 1-based result line of the Jump
 *             target (-1 when not applicable).
 * @return The dialog's decision.
 */
Merge20VerifyGateDecision Merge20VerifyShowIssuesAndDecide(CWnd* pParent,
	const std::vector<Merge20VerifyIssueRow>& rows,
	int& jumpFileIndex, int& jumpDiff, int& jumpResultLine);

/**
 * @brief Translated name of one check ("Re-diff" etc.), shared
 * by the problem-list dialog and the B8 report.
 */
String Merge20VerifyCheckDisplayName(Merge20VerifyCheck check);

/**
 * @brief Translated location of one issue ("Diff %1, result line %2"
 * etc.), shared by the problem-list dialog and the B8 report.
 */
String Merge20VerifyFormatIssueLocation(const Merge20VerifyIssue& issue);

/**
 * @brief Translated detail of one issue (kind + counts + the
 * offending line, verbatim), shared by the problem-list dialog
 * and the B8 report.
 */
String Merge20VerifyFormatIssueDetail(const Merge20VerifyIssue& issue);

/**
 * @brief Navigate a merge doc to one issue: the diff's segment
 * in the result pane (and the diff in the compare panes) when
 * the issue has a diff, else the result line in the result
 * pane. No-op when the view is not available.
 */
void Merge20VerifyJumpToIssue(CMergeDoc& doc, const Merge20VerifyIssue& issue);

/**
 * @brief The one-file output gate (S7 calls the lower-level
 * pieces; SaveMergeResult calls this).
 *
 * Runs the engine for the doc; when clean, returns true
 * (output proceeds, no dialog). When problems exist, shows the
 * problem-list dialog: Close stops the output (false), Proceed
 * lets it continue (true, explicit override), Jump navigates
 * to the problem and stops the output (false). In a
 * non-interactive run there is no dialog: problems are logged
 * and the output is stopped (false).
 * @param [out] pLastResult When non-null, receives the check result
 *             of this run (B8: SaveMergeResult saves the report
 *             from it, without running the verification twice).
 * @return True when the output may proceed, false when it must stop.
 */
bool Merge20VerifyBeforeOutput(CMergeDoc& doc, Merge20VerifyFileResult* pLastResult = nullptr);
