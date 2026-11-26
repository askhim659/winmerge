// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FocusResultView.h
 *
 * @brief Read-only view for the third segment of the focus dock
 * (Merge20 Phase 4 D4, design sections 3.5 and 5.2/D4, spec
 * sections 3 (3/4) and 5.2).
 *
 * The view shows only the current diff's lines of the merge result
 * buffer, copied into its own CGhostTextBuffer by
 * CMerge20FocusDock::ShowDiff (design section 3.5, option (b)).
 * Copying the lines instead of borrowing the document's
 * CMergeResultView keeps the document's single result-view pointer
 * (CMergeDoc::m_pMergeResultView, used as the undo-group source and
 * for invalidation in ResultChooseSources) untouched, and lets this
 * view show exactly the segment: the main result view shows the
 * whole result buffer and can only scroll to the segment.
 *
 * The view is read-only (QueryEditable is false and its buffer is
 * read-only): the merge result is edited in the main result pane,
 * and every judgement or rebuild refreshes this view through
 * ShowDiff. It carries no syntax highlighting and no segment-state
 * tint in D4 (the header label shows the state instead); the
 * character-level highlight lives in the old/new detail panes,
 * which are plain CMergeEditViews with the existing inline-diff
 * display.
 */
#pragma once

#include "GhostTextView.h"
#include "GhostTextBuffer.h"
#include "UnicodeString.h"

class CMerge20FocusDock;

/**
 * @brief Read-only view showing one result segment.
 */
class CMerge20FocusResultView : public CGhostTextView
{
public:
	CMerge20FocusResultView();
	virtual ~CMerge20FocusResultView();
	DECLARE_DYNCREATE(CMerge20FocusResultView)

	virtual CCrystalTextBuffer* LocateTextBuffer() override;
	virtual bool QueryEditable() override;
	virtual void OnInitialUpdate() override;
	int GetTopLine() const { return m_nTopLine; }

	/**
	 * @brief Replace the shown text with the given segment lines.
	 * @param [in] sText The segment's lines (with EOLs), or a
	 *             placeholder line when there is no segment to show.
	 *             The view is scrolled back to the top.
	 */
	void SetSegmentText(const String& sText);

	/**
	 * @brief Scroll, then tell the dock so the detail panes follow
	 * (design section 3.3: explicit sync with a reentrancy flag in
	 * the dock; the flag makes the panes' callback a no-op).
	 */
	virtual void ScrollToSubLine(int nNewTopLine, bool bNoSmoothScroll = false, bool bTrackScrollBar = true) override;

protected:
	//{{AFX_MSG(CMerge20FocusResultView)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	CGhostTextBuffer m_buf; /**< Own buffer: the current segment's lines only */
};
