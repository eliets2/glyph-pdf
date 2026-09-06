// SPDX-License-Identifier: Apache-2.0
#include "OcrScanCanvas.h"

#include <QMouseEvent>
#include <QPainter>

#include "modes/OcrConfidence.h"

namespace gp {

namespace {
// Pane background around the letterboxed page image (the scan pane's dark
// surround, matching the old scroll-area chrome).
const char* kSurround = "#2a2a2a";
// The page paper (drawn under the image while it loads) and empty-state text.
const char* kPaper = "#f4f1ea";
const char* kSelectionColor = "#2563eb";
const char* kRemovedColor = "#8a8a8a";
} // namespace

OcrScanCanvas::OcrScanCanvas(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(false);
    setContextMenuPolicy(Qt::CustomContextMenu);
}

void OcrScanCanvas::setPageImage(const QImage& image)
{
    m_image = image;
    update();
}

void OcrScanCanvas::setWords(const QList<OcrReviewedWord>& words)
{
    m_words = words;
    update();
}

void OcrScanCanvas::setSelectedWord(int stableId)
{
    m_selectedId = stableId;
    update();
}

QRectF OcrScanCanvas::imageRectFor(const QSizeF& imageSize, const QRectF& pane)
{
    if (imageSize.isEmpty() || imageSize.width() <= 0 || imageSize.height() <= 0
        || pane.isEmpty())
        return QRectF();
    // Fit-to-pane, preserving aspect ratio, centered (letterboxed).
    const qreal scale = qMin(pane.width() / imageSize.width(),
                             pane.height() / imageSize.height());
    const qreal w = imageSize.width() * scale;
    const qreal h = imageSize.height() * scale;
    return QRectF(pane.x() + (pane.width() - w) / 2.0,
                  pane.y() + (pane.height() - h) / 2.0, w, h);
}

QSize OcrScanCanvas::minimumSizeHint() const
{
    return QSize(120, 160);
}

QRectF OcrScanCanvas::drawRect() const
{
    // Recomputed from the CURRENT size for both painting and hit-testing —
    // one source of truth, so a stale cached transform can never make a click
    // select the wrong word after a resize.
    return imageRectFor(QSizeF(m_image.size()), QRectF(rect()));
}

int OcrScanCanvas::wordIdAt(const QPointF& pos) const
{
    const QRectF imgRect = drawRect();
    if (imgRect.isEmpty() || m_image.isNull())
        return -1;
    // Map the widget position back into pageImage pixel space. The boxes are
    // already in that space (the 2.0× session render) — no DPR multiply here.
    const qreal scale = imgRect.width() / m_image.width();
    const QPointF imagePos((pos.x() - imgRect.x()) / scale,
                           (pos.y() - imgRect.y()) / scale);
    for (const auto& rec : m_words) {
        if (rec.deleted)
            continue;   // removed words are not clickable targets
        if (rec.boundingBox.contains(imagePos))
            return rec.stableId;
    }
    return -1;
}

void OcrScanCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int id = wordIdAt(event->position());
    if (id >= 0) {
        // Immediate highlight feedback; OCRMode::selectWord (the funnel) will
        // set the same selection again through setSelectedWord.
        setSelectedWord(id);
        emit wordClicked(id);
    }
    // Clicks on empty page space change nothing (no signal, selection kept).
    event->accept();
}

void OcrScanCanvas::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(kSurround));

    if (m_image.isNull()) {
        p.setPen(QColor(kRemovedColor));
        p.drawText(rect(), Qt::AlignCenter,
                   tr("No source image yet — run OCR to review this page."));
        return;
    }

    const QRectF imgRect = drawRect();
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(imgRect, m_image, QRectF(m_image.rect()));
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);

    // Word boxes at their real positions, colored by THE one classifier.
    const qreal scale = imgRect.width() / m_image.width();
    QPen hitPen(QColor(kSelectionColor), 2);
    for (const auto& rec : m_words) {
        QRectF box = rec.boundingBox.adjusted(-imgRect.x(), -imgRect.y(),
                                              -imgRect.x(), -imgRect.y());
        box = QRectF(box.x() * scale, box.y() * scale,
                     box.width() * scale, box.height() * scale);

        const bool removed = rec.deleted || rec.reviewedText.trimmed().isEmpty();
        QColor fill = OcrConfidence::bandColor(
            OcrConfidence::bandFor(rec.confidence));
        fill.setAlpha(removed ? 30 : 60);
        p.fillRect(box, fill);

        p.setPen(QPen(removed ? QColor(kRemovedColor)
                              : OcrConfidence::bandColor(
                                    OcrConfidence::bandFor(rec.confidence)),
                      1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(box);

        if (removed) {
            // Strikethrough diagonal — meaning not carried by color alone.
            p.drawLine(box.topLeft(), box.bottomRight());
        }
        if (m_selectedId == rec.stableId) {
            p.setPen(hitPen);
            p.drawRect(box.adjusted(-2, -2, 2, 2));
        }
    }
}

} // namespace gp
