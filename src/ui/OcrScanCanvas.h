// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QImage>
#include <QList>
#include <QWidget>

#include "modes/OcrReviewSession.h"

class QMouseEvent;
class QPaintEvent;

namespace gp {

/// U03: the scan pane's REAL source view — the page image OCR ran on, with
/// each OcrReviewedWord's box drawn at its position and click-to-select.
///
/// Word boxes are in pageImage PIXEL space (the 2.0× render EditController
/// delivered). The canvas maps widget positions through the fit-to-pane
/// transform only; devicePixelRatio is deliberately NOT applied a second
/// time (the boxes are already in device pixels of the session image).
class OcrScanCanvas : public QWidget {
    Q_OBJECT
public:
    explicit OcrScanCanvas(QWidget* parent = nullptr);

    void setPageImage(const QImage& image);
    void setWords(const QList<OcrReviewedWord>& words);
    void setSelectedWord(int stableId);
    int  selectedWord() const { return m_selectedId; }
    QImage pageImage() const { return m_image; }

    /// Pure seam: the letterboxed, centered rect an image occupies inside a
    /// pane rect. Empty when either side is empty. Shared by painting and
    /// hit-testing so the two can never drift apart.
    static QRectF imageRectFor(const QSizeF& imageSize, const QRectF& pane);

    /// ── PARITY-SCORECARD-2026-09-30 §4 row 12: region re-OCR selection ─────
    /// Pure seam: map a WIDGET-space drag rect onto the page image the canvas
    /// currently letterboxes into `pane` — the inverse of the mapping
    /// wordIdAt()/paintEvent() use, built on the same imageRectFor source of
    /// truth. Returns the clamped, normalized region in pageImage PIXEL space
    /// (the LayoutRegion::bbox coordinate system); an empty result means the
    /// drag did not intersect the image (a garbage selection must not be
    /// silently widened into a whole-page region).
    static QRectF imageRegionFor(const QRectF& widgetRect, const QImage& image,
                                 const QRectF& pane);

    /// The region the user last drag-selected, in pageImage pixel space
    /// (empty = no selection). Reset whenever a new page image is loaded —
    /// a bbox from the previous image's pixel space is stale garbage.
    QRectF selectedRegion() const { return m_regionRect; }
    void clearSelectedRegion();

    QSize minimumSizeHint() const override;

signals:
    /// Emitted with the word's stableId when the user clicks inside its box.
    /// OCRMode funnels this into the same selectWord() the word links use.
    void wordClicked(int stableId);

    /// Emitted when a drag-select finishes: the clamped region in pageImage
    /// pixel space, or an empty rect when the drag missed the image (which
    /// also CLEARS the selection — the honest "no region" state).
    void regionSelected(QRectF imageRect);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRectF drawRect() const;             // current fit-to-pane image rect
    int wordIdAt(const QPointF& pos) const;

    QImage m_image;
    QList<OcrReviewedWord> m_words;
    int m_selectedId = -1;
    // Region re-OCR: the committed selection (image space) and the live
    // rubber band while a drag is in progress (widget space; empty = none).
    QRectF m_regionRect;
    QRectF m_rubberBand;
    QPointF m_pressPos;
    bool m_dragging = false;
};

} // namespace gp
