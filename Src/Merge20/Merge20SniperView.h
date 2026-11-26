// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20SniperView.h
 *
 * @brief Read-only panel of the auto sniper overlay (Merge20 Phase 4
 * D6, design sections 3.3, 3.6 and 5.2/D6, spec sections 3 (4/4),
 * 4(g) and 5.4).
 *
 * One of the three panels (OLD / NEW / MERGED) of the sniper
 * overlay. It follows the D4 focus-result-view precedent
 * (CMerge20FocusResultView): a CGhostTextView with its own
 * CGhostTextBuffer, filled with only the current diff's lines, so
 * the document's single result view (CMergeDoc::m_pMergeResultView)
 * and the compare views (CMergeDoc::m_pView[group][buffer]) are
 * never touched. That is also the T-4-2 answer: reusing
 * CMergeEditView for the panels was rejected, because a
 * CMergeEditView is registered in the document's view array and
 * takes part in UpdateAllViews / SelectDiff, while these panels
 * only display text.
 *
 * The view notifies its parent dialog (CMerge20SniperDlg) of
 * scrolls so the other two panels follow (design section 3.3:
 * explicit sync with a reentrancy flag in the dialog), handles the
 * sniper keys, and turns drags into swipe navigation (horizontal,
 * 50px or more) or a synced scroll (vertical).
 */
#pragma once

#include "GhostTextView.h"
#include "GhostTextBuffer.h"
#include "UnicodeString.h"

class CMerge20SniperDlg;

/**
 * @brief Read-only view showing one diff's lines in the sniper overlay.
 */
class CMerge20SniperView : public CGhostTextView
{
public:
	CMerge20SniperView();
	virtual ~CMerge20SniperView();
	DECLARE_DYNCREATE(CMerge20SniperView)

	virtual CCrystalTextBuffer* LocateTextBuffer() override;
	virtual bool QueryEditable() override;
	virtual void OnInitialUpdate() override;
	int GetTopLine() const { return m_nTopLine; }
	/**
	 * @brief Panel line height in pixels (CCrystalTextView::
	 * GetLineHeight is protected, so the dialog reads it
	 * through this accessor for the pixel-to-line scroll
	 * conversion).
	 */
	int GetSniperLineHeight() { return GetLineHeight(); }

	/**
	 * @brief Replace the shown text with the given diff lines.
	 * @param [in] sText The diff's lines (with EOLs), or a
	 *             placeholder line when there is nothing to show.
	 *             The view is scrolled back to the top.
	 */
	void SetSegmentText(const String& sText);

	/**
	 * @brief Scroll, then tell the dialog so the other two panels
	 * follow (design section 3.3: explicit sync with a reentrancy
	 * flag in the dialog; the flag makes the other panels'
	 * callback a no-op).
	 */
	virtual void ScrollToSubLine(int nNewTopLine, bool bNoSmoothScroll = false, bool bTrackScrollBar = true) override;

protected:
	//{{AFX_MSG(CMerge20SniperView)
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	/**
	 * @brief Ask the parent dialog to handle a sniper key.
	 * @return True when the dialog handled the key.
	 */
	bool ForwardKeyToDialog(UINT nChar);

	CGhostTextBuffer m_buf; /**< Own buffer: the current diff's lines only */
	bool m_bDragging; /**< A drag gesture is in progress */
	CPoint m_ptDragStart; /**< Pointer position at drag start */
	int m_nDragTopLine; /**< This panel's top line at drag start */
};
