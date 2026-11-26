// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FocusResultView.cpp
 *
 * @brief Implementation of the focus dock's read-only result view
 * (Merge20 Phase 4 D4).
 */
#include "StdAfx.h"
#include "Merge20/Merge20FocusResultView.h"
#include "Merge20/Merge20FocusDock.h"
#include "Merge.h"
#include "OptionsMgr.h"
#include "OptionsDef.h"
#include "DarkModeLib.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CMerge20FocusResultView, CGhostTextView)

BEGIN_MESSAGE_MAP(CMerge20FocusResultView, CGhostTextView)
	//{{AFX_MSG_MAP(CMerge20FocusResultView)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CMerge20FocusResultView::CMerge20FocusResultView()
{
	// The buffer starts empty; SetSegmentText fills it. InitNew
	// gives it one empty line so LocateTextBuffer never hands the
	// view an uninitialized buffer before the first ShowDiff.
	m_buf.InitNew();
}

CMerge20FocusResultView::~CMerge20FocusResultView()
{
}

CCrystalTextBuffer* CMerge20FocusResultView::LocateTextBuffer()
{
	return &m_buf;
}

bool CMerge20FocusResultView::QueryEditable()
{
	// Read-only by design: the result is edited in the main result
	// pane. Returning false here stops typing even if the buffer's
	// read-only flag were ever cleared by mistake.
	return false;
}

void CMerge20FocusResultView::OnInitialUpdate()
{
	// Same font and colors as the main result view
	// (CMergeResultView::OnInitialUpdate), but attached to this
	// view's own buffer, which already exists. Sent explicitly with
	// WM_INITIALUPDATE after creation (the frame's InitialUpdateFrame
	// only reaches the main splitter view).
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

void CMerge20FocusResultView::SetSegmentText(const String& sText)
{
	// The buffer is read-only between refills, so drop the flag
	// while replacing the content, then put it back.
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

void CMerge20FocusResultView::ScrollToSubLine(int nNewTopLine, bool bNoSmoothScroll, bool bTrackScrollBar)
{
	CCrystalTextView::ScrollToSubLine(nNewTopLine, bNoSmoothScroll, bTrackScrollBar);
	if (auto* pDock = dynamic_cast<CMerge20FocusDock*>(GetParent()))
		pDock->SyncDetailFromFocus(GetTopLine());
}
