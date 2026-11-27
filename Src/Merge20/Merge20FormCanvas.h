// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file  Merge20FormCanvas.h
 *
 * @brief FORM/XML comparison (Wave 5 F3): the visual-preview
 * canvas (design merge20-form-compare-design.md sections 3.4,
 * 6 (6), 7.2 and 10 F3).
 *
 * `CMerge20FormCanvas` is a `CWnd`-derived control that draws one
 * FORM file's layout with GDI, at native 1:1 scale: the white
 * background, the 100-unit grid (500-unit major lines), the
 * shapes (unchanged = grey, old = red dashed, new = green) and
 * the element text inside the shapes (design section 6 (6)).
 * It owns the zoom (100% / 25% / fit / slider 10-200%), the
 * scrollbars, the drag scroll (a drag over 4 px scrolls, a
 * plain click selects a shape) and the shape selection the F4
 * spotlight builds on. F4 adds the spotlight itself: focusing
 * a diff record dims the surroundings, re-draws the old (red
 * dashed) and new (green) shapes of the focused element with
 * a move arrow and a callout box, and scrolls the shape into
 * the centre of the viewport (design sections 2.7 and 10 F4).
 *
 * The geometry that needs no window (zoom scale, XML <-> screen
 * transform, hit test, truncation, pt -> px) lives in the pure
 * module Merge20FormCanvasGeom so GoogleTest can pin it; this
 * control only does the GDI drawing and the mouse handling.
 * The control never writes anything: FORM comparison is
 * view-only (design section 1).
 */
#pragma once

#include <string>
#include "Merge20/Merge20FormCanvasGeom.h"
#include "Merge20/Merge20FormSpotlight.h"
#include "Merge20/Merge20FormXml.h"

/**
 * @brief Notification code sent to the parent with WM_COMMAND
 * when the user clicks a shape (the canvas sends it through
 * `GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(),
 * MERGE20_FORM_CANVAS_SELCHANGE))`, like a button notification;
 * the parent reads the key with GetSelectedShapeKey()).
 */
#define MERGE20_FORM_CANVAS_SELCHANGE 1

/**
 * @brief FORM visual-preview canvas (GDI, CWnd-derived).
 */
class CMerge20FormCanvas : public CWnd
{
	DECLARE_DYNAMIC(CMerge20FormCanvas)
public:
	CMerge20FormCanvas();
	virtual ~CMerge20FormCanvas();

	/** @brief Create the canvas as a child control (scrollbars included). */
	BOOL Create(CWnd* pParentWnd, UINT nID);

	/**
	 * @brief Show one file result (nullptr clears the canvas).
	 * The document owns the result; the canvas only borrows the
	 * pointer until the next SetResult call.
	 */
	void SetResult(const Merge20FormFileResult* pResult);
	/** @brief Select a zoom mode (100% / 25% / fit / slider). */
	void SetZoomMode(Merge20FormZoomMode mode);
	/** @brief Set the slider percent (10-200, clamped) and switch to Slider mode. */
	void SetSliderPercent(double percent);
	/** @brief Selected zoom mode. */
	Merge20FormZoomMode GetZoomMode() const { return m_zoomMode; }
	/** @brief Slider percent (10-200). */
	double GetSliderPercent() const { return m_sliderPercent; }
	/** @brief Effective scale (1.0 = native 1:1). */
	double GetScale() const { return m_scale; }
	/** @brief Key of the clicked shape, empty when nothing is selected. */
	const std::wstring& GetSelectedShapeKey() const { return m_selectedShapeKey; }
	/** @brief Select a shape by key (F4 spotlight drives this). */
	void SetSelectedShapeKey(const std::wstring& key);
	/**
	 * @brief Focus a diff record for the spotlight (F4): the
	 * canvas dims the surroundings and emphasises the old/new
	 * shapes of the record's element. The record is borrowed
	 * from the document's result; nullptr clears the spotlight.
	 * The record must belong to the current result (SetResult
	 * clears the spotlight).
	 */
	void SetSpotlight(const Merge20FormDiffRecord* pRecord);
	/** @brief Clear the spotlight (back to the whole view). */
	void ClearSpotlight();
	/** @brief True when a record is focused for the spotlight. */
	bool HasSpotlight() const { return m_pSpotlightRecord != nullptr; }
	/** @brief Key of the spotlighted shape, empty when none. */
	const std::wstring& GetSpotlightShapeKey() const { return m_spotlightShapeKey; }
	/**
	 * @brief Scroll so the shape's centre is in the centre of
	 * the viewport (HTML `scrollToShapeKey`). Does nothing when
	 * the key has no shape in the current result.
	 */
	void ScrollToShapeKey(const std::wstring& key);
	/**
	 * @brief Scroll to the changed area (HTML
	 * `scrollToChangedArea`): the spotlighted shape when there
	 * is one, otherwise the first changed shape.
	 */
	void ScrollToChangedArea();
	/** @brief True when any drawn text uses the fallback font (the note is shown). */
	bool IsFontApproximate() const { return m_bFontApproximate; }

protected:
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	DECLARE_MESSAGE_MAP()

private:
	void UpdateScale();
	void UpdateScrollBars();
	void ClampScroll();
	void ScrollBy(int dx, int dy);
	void ScrollTo(int x, int y);
	CPoint XmlToScreen(double xmlX, double xmlY) const;
	void DrawGrid(CDC& dc) const;
	void DrawShapes(CDC& dc);
	void DrawOneShape(CDC& dc, const Merge20FormShape& shape, bool isNew, bool changed);
	void DrawShapeText(CDC& dc, const Merge20FormShape& shape, COLORREF color);
	/** @brief Create the font for one shape (shape fontName when installed, else default). */
	bool CreateShapeFont(CDC& dc, const Merge20FormShape& shape, CFont& font, int pixelHeight);
	void DrawSelectedOutline(CDC& dc);
	/** @brief Spotlight pass (F4): dim veil, white window, re-drawn old/new shapes, move arrow, callout. */
	void DrawSpotlight(CDC& dc);
	/** @brief One focused shape in spotlight style (old = red dashed, new = green) with ruler guides and a badge. `badgeBelow` puts the new badge under the shape (HTML: when the old and new badges would overlap). */
	void DrawSpotlightShape(CDC& dc, const Merge20FormShape& shape, bool isOld, bool badgeBelow);
	/** @brief Callout box with the focused element's changes (F4). `rcWindow` is the spotlight window in screen pixels. */
	void DrawSpotlightCallout(CDC& dc, const CRect& rcWindow);
	/** @brief True when `key` is one of the result's changed shape keys. */
	bool IsChangedKey(const std::wstring& key) const;

	const Merge20FormFileResult* m_pResult = nullptr; /**< Borrowed result (document-owned). */
	Merge20FormZoomMode m_zoomMode = Merge20FormZoomMode::Percent100; /**< Selected zoom mode. */
	double m_sliderPercent = 100.0; /**< Slider percent (10-200). */
	double m_scale = 1.0; /**< Effective scale (1.0 = native 1:1). */
	int m_scrollX = 0; /**< Horizontal scroll offset (screen pixels). */
	int m_scrollY = 0; /**< Vertical scroll offset (screen pixels). */
	std::wstring m_selectedShapeKey; /**< Key of the clicked shape. */
	const Merge20FormDiffRecord* m_pSpotlightRecord = nullptr; /**< Borrowed focused record (F4 spotlight). */
	std::wstring m_spotlightShapeKey; /**< Key of the spotlighted shape (the record's element). */
	bool m_bFontApproximate = false; /**< True when a drawn text used the fallback font. */
	bool m_bDragging = false; /**< True between button down and up. */
	bool m_bMoved = false; /**< True when the drag passed the 4 px threshold. */
	CPoint m_dragStart; /**< Mouse position at button down (client pixels). */
	CPoint m_dragStartScroll; /**< Scroll offset at button down. */
};
