// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormSpotPreview.h
 *
 * @brief FORM/XML comparison (Wave 5 F4): the small old/new
 * side-by-side preview in the spotlight inspector banner
 * (design merge20-form-compare-design.md sections 2.7, 6 (4)
 * and 10 F4, test 3).
 *
 * `CMerge20FormSpotPreview` is a small `CWnd`-derived control
 * that draws one shape only, fitted to its client area: the
 * old shape in red dashed (with a light red fill) or the new
 * shape in green (with a light green fill), with the element
 * text inside when it fits. It is deliberately not a
 * `CMerge20FormCanvas`: it has no scrollbars, no zoom and no
 * grid, because the banner only shows the one focused shape
 * before and after, side by side.
 *
 * The control never writes anything: FORM comparison is
 * view-only (design section 1). It borrows the shape pointer
 * until the next SetShape call; the view owns the result.
 */
#pragma once

#include "Merge20/Merge20FormXml.h"

/**
 * @brief One-shape old/new preview for the spotlight banner.
 */
class CMerge20FormSpotPreview : public CWnd
{
	DECLARE_DYNAMIC(CMerge20FormSpotPreview)
public:
	CMerge20FormSpotPreview();
	virtual ~CMerge20FormSpotPreview();

	/** @brief Create the preview as a child control. */
	BOOL Create(CWnd* pParentWnd, UINT nID);

	/**
	 * @brief Show one shape (nullptr clears the preview).
	 * @param [in] pShape Borrowed shape, or nullptr for "no shape".
	 * @param [in] bOld True for the old side (red dashed), false
	 * for the new side (green solid).
	 */
	void SetShape(const Merge20FormShape* pShape, bool bOld);
	/** @brief True when the preview shows a shape. */
	bool HasShape() const { return m_pShape != nullptr; }

protected:
	afx_msg void OnPaint();
	DECLARE_MESSAGE_MAP()

private:
	const Merge20FormShape* m_pShape = nullptr; /**< Borrowed shape (view-owned). */
	bool m_bOld = true; /**< True for the old side, false for the new side. */
};
