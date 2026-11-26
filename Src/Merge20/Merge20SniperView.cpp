// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20SniperView.cpp
 *
 * @brief Implementation of the auto sniper overlay's read-only panel
 * (Merge20 Phase 4 D6).
 */
#include "StdAfx.h"
#include "Merge20/Merge20SniperView.h"
#include "Merge20/Merge20SniperDlg.h"
#include "Merge20/Merge20Sniper.h"
#include "Merge.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"
#include "DarkModeLib.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CMerge20SniperView, CGhostTextView)

BEGIN_MESSAGE_MAP(CMerge20SniperView, CGhostTextView)
	//{{AFX_MSG_MAP(CMerge20SniperView)
	ON_WM_KEYDOWN()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CMerge20SniperView::CMerge20SniperView()
	: m_bDragging(false)
	, m_ptDragStart(0, 0)
	, m_nDragTopLine(0)
{
	// The buffer starts empty; SetSegmentText fills it. InitNew
	// gives it one empty line so LocateTextBuffer never hands the
	// view an uninitialized buffer before the first ShowDiff
	// (same as CMerge20FocusResultView).
	m_buf.InitNew();
}

CMerge20SniperView::~CMerge20SniperView()
{
}

CCrystalTextBuffer* CMerge20SniperView::LocateTextBuffer()
{
	return &m_buf;
}

bool CMerge20SniperView::QueryEditable()
{
	// Read-only by design: the merge result is edited in the main
	// result pane. Returning false here stops typing even if the
	// buffer's read-only flag were ever cleared by mistake.
	return false;
}

void CMerge20SniperView::OnInitialUpdate()
{
	// Same font and colors as the main result view
	// (CMergeResultView::OnInitialUpdate), but attached to this
	// view's own buffer, which already exists. Sent explicitly with
	// WM_INITIALUPDATE after creation (same as
	// CMerge20FocusResultView). The dialog's ApplyFont then applies
	// the sniper font size (OPT_MERGE20_SNIPER_FONT) over this.
	CView::OnInitialUpdate();
	LOGFONT lf = theApp.m_lfDiff;
	lf.lfHeight = static_cast<LONG>(lf.lfHeight * GetOptionsMgr()->GetInt(OPT_VIEW_ZOOM) / 1000.0);
	SetFont(lf);
	SetColorContext(theApp.GetMainSyntaxColors());
	SetMarkersContext(theApp.GetMainMarkers());

	if (HWND hSelf = GetSafeHwnd())
		DarkMode::setDarkScrollBar(hSelf);
	AttachToBuffer();
}

void CMerge20SniperView::SetSegmentText(const String& sText)
{
	// The buffer is read-only between refills, so drop the flag
	// while replacing the content, then put it back (same as
	// CMerge20FocusResultView::SetSegmentText).
	m_buf.SetReadOnly(false);
	m_buf.ResetInit();
	if (!sText.empty())
	{
		int nEndLine = 0, nEndChar = 0;
		m_buf.InsertText(nullptr, 0, 0, sText.c_str(), sText.length(),
			nEndLine, nEndChar, CE_ACTION_UNKNOWN, false /* no history */);
	}
	m_buf.SetReadOnly(true);
	// Re-attaching resets the view to the top line and rebuilds the
	// scrollbars for the new content (the buffer was refilled with a
	// nullptr source, so the view got no update notification).
	ReAttachToBuffer();
	Invalidate();
}

void CMerge20SniperView::ScrollToSubLine(int nNewTopLine, bool bNoSmoothScroll, bool bTrackScrollBar)
{
	CCrystalTextView::ScrollToSubLine(nNewTopLine, bNoSmoothScroll, bTrackScrollBar);
	if (auto* pDlg = dynamic_cast<CMerge20SniperDlg*>(GetParent()))
		pDlg->SyncFromView(this, GetTopLine());
}

bool CMerge20SniperView::ForwardKeyToDialog(UINT nChar)
{
	if (auto* pDlg = dynamic_cast<CMerge20SniperDlg*>(GetParent()))
		return pDlg->HandleSniperKey(nChar);
	return false;
}

void CMerge20SniperView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// Sniper keys (spec section 4(g)) are handled by the dialog,
	// which owns the current diff; anything else keeps the
	// CrystalEdit behaviour (cursor keys the dialog did not take,
	// Home/End and the like).
	if (ForwardKeyToDialog(nChar))
		return;
	CGhostTextView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CMerge20SniperView::OnLButtonDown(UINT nFlags, CPoint point)
{
	// Record the drag start (HTML `initSniper` mousedown): the
	// gesture is decided while dragging, not on release.
	m_bDragging = true;
	m_ptDragStart = point;
	m_nDragTopLine = GetTopLine();
	SetCapture();
	CGhostTextView::OnLButtonDown(nFlags, point);
}

void CMerge20SniperView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (m_bDragging && (nFlags & MK_LBUTTON) != 0)
	{
		const int nDx = point.x - m_ptDragStart.x;
		const int nDy = point.y - m_ptDragStart.y;
		const Merge20SniperDrag drag = Merge20SniperDragAction(nDx, nDy);
		if (drag != Merge20SniperDrag::None)
		{
			// A horizontal swipe moves to the next/previous diff
			// at once (HTML `initSniper` mousemove: navigate and
			// stop tracking this gesture).
			m_bDragging = false;
			ReleaseCapture();
			if (auto* pDlg = dynamic_cast<CMerge20SniperDlg*>(GetParent()))
				pDlg->MoveDiff(drag == Merge20SniperDrag::Next ? +1 : -1);
			return;
		}
		// A mostly-vertical drag scrolls this panel (HTML
		// `initSniper`: scrollTop = top0 - dy once the drag passes
		// 8px). The ScrollToSubLine override syncs the other two
		// panels. The top line is recomputed from the drag start,
		// so the panel follows the pointer instead of drifting.
		const int nAbsDy = nDy < 0 ? -nDy : nDy;
		const int nAbsDx = nDx < 0 ? -nDx : nDx;
		const int nLineHeight = GetLineHeight();
		if (nAbsDy > 8 && nAbsDy >= nAbsDx && nLineHeight > 0)
		{
			const int nLines = (m_ptDragStart.y - point.y) / nLineHeight;
			ScrollToSubLine(m_nDragTopLine + nLines);
		}
	}
	CGhostTextView::OnMouseMove(nFlags, point);
}

void CMerge20SniperView::OnLButtonUp(UINT nFlags, CPoint point)
{
	m_bDragging = false;
	if (::GetCapture() == GetSafeHwnd())
		ReleaseCapture();
	CGhostTextView::OnLButtonUp(nFlags, point);
}
