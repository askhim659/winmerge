// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCompareFrame.h
 *
 * @brief FORM/XML comparison (Wave 5 F2): the FORM compare frame
 * (design merge20-form-compare-design.md sections 3.2 and 7.1).
 *
 * The frame is the MDI child window of the FORM compare view. It
 * derives from CMergeFrameCommon like the folder and file compare
 * frames (tabbed MDI child, shared frame behaviour), but carries
 * no bars in F2: the view owns the KPI bar, the filter tabs, the
 * search, the sort box, the navigator and the card list. Dedicated
 * bars (dedicated mode, file-list panel) do not apply to this
 * view-only window.
 */
#pragma once

#include "MergeFrameCommon.h"

/**
 * @brief MDI child frame of the FORM compare window.
 */
class CFormCompareFrame : public CMergeFrameCommon
{
	DECLARE_DYNCREATE(CFormCompareFrame)
protected:
	CFormCompareFrame(); // protected constructor used by dynamic creation
	virtual ~CFormCompareFrame();

	DECLARE_MESSAGE_MAP()
};
