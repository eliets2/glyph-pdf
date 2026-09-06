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

    QSize minimumSizeHint() const override;

signals:
    /// Emitted with the word's stableId when the user clicks inside its box.
    /// OCRMode funnels this into the same selectWord() the word links use.
    void wordClicked(int stableId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QRectF drawRect() const;             // current fit-to-pane image rect
    int wordIdAt(const QPointF& pos) const;

    QImage m_image;
    QList<OcrReviewedWord> m_words;
    int m_selectedId = -1;
};

} // namespace gp
