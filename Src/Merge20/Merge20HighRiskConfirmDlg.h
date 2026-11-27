// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20HighRiskConfirmDlg.h
 *
 * @brief High-risk judgment confirmation dialog (Merge20
 * best-workflow B14, pillar 8: "keyboard speed, with care for
 * dangerous operations").
 *
 * Merge20 (B14, gap analysis section 4.2 B14): when a destructive
 * judgment (UseOld / UseNew / Reject) is about to be applied to a
 * HIGH-risk diff, or when a Space cycle step would declare a
 * HIGH-risk file done, show the risk reason (the S4 per-diff or
 * per-file summary) and, when a learning candidate exists for the
 * diff, the candidate text (L3 wording), and offer two choices:
 * execute, or cancel (change nothing). The wording and feel
 * follow the B9 summary dialog and the B2 close-warning dialog:
 * the same "counts + target + cancel changes nothing" shape (gap
 * analysis section 3.2). S5 itself is still unimplemented
 * (U-0-2 waiting), so B9/B2 are the working prototypes of that
 * shape.
 *
 * The dialog only displays the counts and detail lines it is
 * given; the callers build them (CMergeDoc for diffs,
 * Merge20ConfirmHighRiskFileReview below for files). The dialog
 * never writes anything itself. Cancel is the default button
 * (including the close button and Esc), so an accidental Enter
 * key cannot commit a high-risk judgment.
 */
#pragma once

#include <vector>
#include "TrDialogs.h"
#include "resource.h"
#include "Merge20Types.h"

/** @brief The high-risk confirmation dialog (B14). */
class Merge20HighRiskConfirmDlg : public CTrDialog
{
	DECLARE_DYNAMIC(Merge20HighRiskConfirmDlg)

public:
	/** @brief What the user chose in the dialog. */
	enum class Result
	{
		Cancel,  /**< Cancel (or close): abort, change nothing. */
		Execute  /**< Execute the judgment. */
	};

	/**
	 * @param [in] pParent Parent window.
	 * @param [in] countsText Counts line, already translated by the
	 *             caller ("High-risk diffs: N" / "High-risk file:
	 *             <path>").
	 * @param [in] detailLines One line per confirmed item: the risk
	 *             reason (S4 summary) and, for a diff with a
	 *             learning candidate, the candidate text. Already
	 *             translated by the caller.
	 */
	Merge20HighRiskConfirmDlg(CWnd* pParent, const String& countsText, const std::vector<String>& detailLines);

	enum { IDD = IDD_MERGE20_HIGHRISK_CONFIRM };

	Result GetResult() const { return m_result; }

protected:
	virtual void DoDataExchange(CDataExchange* pDX) override;
	virtual BOOL OnInitDialog() override;

	afx_msg void OnExecute();
	DECLARE_MESSAGE_MAP()

private:
	String m_countsText;
	std::vector<String> m_detailLines;
	Result m_result = Result::Cancel;
};

/**
 * @brief Show the high-risk confirmation for one Space-cycle step
 * on a file (B14 acceptance (f)).
 * @param [in] pParent Parent window.
 * @param [in] relPath Relative path of the file (shown in the
 *             counts line).
 * @param [in] state S4 classification of the file (risk and
 *             summary; NONE / unclassified files never prompt).
 * @param [in] fromState The file's review state now.
 * @param [in] toState The state Space would move it to.
 * @return True when the step may proceed: no confirmation was
 *         needed, or the person confirmed. False when the person
 *         cancelled: the caller must change nothing.
 */
bool Merge20ConfirmHighRiskFileReview(CWnd* pParent, const String& relPath,
	const Merge20FileState& state, ReviewState fromState, ReviewState toState);
