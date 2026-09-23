// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QImage>
#include <QWidget>

class QPaintEvent;

namespace gp {

/// U03: the zoom pane — a magnified crop of the selected word's source pixels.
/// Custom paintEvent + QPainter::drawImage(target, image, sourceRect) with
/// SmoothPixmapTransform: each repaint blits only the small crop region (a
/// QGraphicsView would add scene/scrollbar machinery for one word box).
///
/// The ×N magnification is COMPUTED from the pane fit math and shown in the
/// pane header — replacing the old static "ZOOM · 4×" claims.
class OcrWordMagnifier : public QWidget {
    Q_OBJECT
public:
    explicit OcrWordMagnifier(QWidget* parent = nullptr);

    void setPageImage(const QImage& image);

    /// Select the word to magnify. wordBoxPx is in pageImage pixel space
    /// (OcrReviewedWord::boundingBox) — no second DPR multiply. An empty or
    /// invalid rect clears the crop.
    void setCropRect(const QRectF& wordBoxPx);
    void clearSelection();

    QImage pageImage() const { return m_image; }

    /// The source rect currently drawn (padded around the word box, clamped
    /// to the image bounds); empty when nothing is selected.
    QRect currentSourceRect() const { return m_sourceRect; }

    /// Actual magnification of the crop for the CURRENT widget size
    /// (letterboxed fit inside the margins). 0.0 when nothing is selected.
    qreal currentMagnification() const;

    /// Pure seam: the padded, clamped source rect for a word box within an
    /// image of the given size (~25% padding per side, clamped to bounds).
    static QRect sourceRectFor(const QRectF& wordBox, const QSize& imageSize,
                               qreal padding = 0.25);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    /// Letterboxed fit scale for a source rect inside a pane (8px margins).
    static qreal magnificationFor(const QRectF& sourceRect, const QSizeF& pane);

    QImage m_image;
    QRectF m_wordBox;
    QRect  m_sourceRect;   // padded/clamped source pixels currently drawn
};

} // namespace gp
