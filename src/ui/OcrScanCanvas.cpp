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
// Widget-space movement before a press becomes a REGION drag — below this a
// release is still the plain word-selection click.
const qreal kRegionDragThreshold = 4.0;
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
    // A region bbox is only meaningful in the pixel space of the image it was
    // dragged on — a new page image invalidates it.
    m_regionRect = QRectF();
    m_rubberBand = QRectF();
    m_dragging = false;
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

void OcrScanCanvas::clearSelectedRegion()
{
    m_regionRect = QRectF();
    update();
}

QRectF OcrScanCanvas::imageRegionFor(const QRectF& widgetRect, const QImage& image,
                                     const QRectF& pane)
{
    const QRectF imgRect = imageRectFor(QSizeF(image.size()), pane);
    if (imgRect.isEmpty() || image.isNull())
        return QRectF();
    // Normalize FIRST (right-to-left / bottom-to-top drags arrive with a
    // negative extent — checking isEmpty() before normalized() would throw
    // those legitimate drags away).
    const QRectF drag = widgetRect.normalized();
    if (drag.isEmpty())
        return QRectF();
    // Widget → pageImage pixel space: the EXACT inverse of paintEvent's
    // (widgetPos − imgRect.topLeft()) × scale mapping and of wordIdAt()'s
    // click mapping — all three derive from the same imageRectFor source of
    // truth, so a dragged region always lands where it was drawn.
    const qreal scale = imgRect.width() / image.width();
    QRectF region((drag.left() - imgRect.left()) / scale,
                  (drag.top() - imgRect.top()) / scale,
                  drag.width() / scale,
                  drag.height() / scale);
    region = region.normalized();
    // Clamp to the image: a drag that runs off the page is cut to its real
    // extent; a drag that misses the image entirely stays EMPTY (the garbage
    // selection is refused downstream, never widened into a whole page).
    region = region.intersected(QRectF(image.rect()));
    return region;
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
    // Row 12: remember the press — real movement from here turns the gesture
    // into a region drag; a plain release stays a word-selection click.
    m_pressPos = event->position();
    m_dragging = false;
    m_rubberBand = QRectF();
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

void OcrScanCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        event->ignore();
        return;
    }
    if (!m_dragging) {
        // Only real movement makes the gesture a region drag (click != drag).
        if ((event->position() - m_pressPos).manhattanLength()
                < kRegionDragThreshold) {
            event->accept();
            return;
        }
        m_dragging = true;
    }
    m_rubberBand = QRectF(m_pressPos, event->position()).normalized();
    update();
    event->accept();
}

void OcrScanCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    if (!m_dragging) {
        // Plain click: no region semantics (word selection already happened
        // on press). The committed region survives plain clicks.
        m_rubberBand = QRectF();
        event->accept();
        return;
    }
    m_dragging = false;
    m_rubberBand = QRectF();
    // Commit the drag as a pageImage-space region (row 12). An EMPTY result
    // (drag missed the image) CLEARS the selection and says so through the
    // signal — a stale bbox must never scope the next re-OCR run.
    const QRectF imageRegion =
        imageRegionFor(QRectF(m_pressPos, event->position()).normalized(),
                       m_image, QRectF(rect()));
    m_regionRect = imageRegion;
    emit regionSelected(imageRegion);
    update();
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
    // D04: painting maps image-space → widget space as
    //     widgetPos = imgRect.topLeft() + scale × imagePos
    // — the exact inverse of wordIdAt()'s (widgetPos − imgRect.topLeft()) /
    // scale mapping, so the drawn overlay always lands under the click
    // target. The old code SUBTRACTED the letterboxed origin before scaling,
    // which pushed every overlay off the widget as soon as the pane's aspect
    // ratio differed from the page image's (100×100 image in a 200×100 pane:
    // box (10,10) painted at (−40,10) instead of (60,10)). Pen widths stay in
    // device pixels, keeping the selection border an intentional 2px ring.
    const qreal scale = imgRect.width() / m_image.width();
    QPen hitPen(QColor(kSelectionColor), 2);
    for (const auto& rec : m_words) {
        const QRectF box(imgRect.x() + rec.boundingBox.x() * scale,
                         imgRect.y() + rec.boundingBox.y() * scale,
                         rec.boundingBox.width() * scale,
                         rec.boundingBox.height() * scale);

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

    // ── Row 12: region re-OCR selection ─────────────────────────────────────
    // The committed region (image space → widget space through the SAME
    // scale/imgRect mapping the word boxes use) and the live rubber band
    // (widget space). A dashed outline + light fill distinguishes the
    // re-OCR scope from a selected word's solid ring.
    const auto drawRegionRect = [&p, &imgRect, scale](const QRectF& imageRect) {
        if (imageRect.isEmpty())
            return;
        const QRectF w(imgRect.x() + imageRect.x() * scale,
                       imgRect.y() + imageRect.y() * scale,
                       imageRect.width() * scale,
                       imageRect.height() * scale);
        QColor fill(kSelectionColor);
        fill.setAlpha(36);
        p.fillRect(w, fill);
        p.setPen(QPen(QColor(kSelectionColor), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(w);
    };
    drawRegionRect(m_regionRect);
    if (!m_rubberBand.isEmpty()) {
        p.setPen(QPen(QColor(kSelectionColor), 1, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(m_rubberBand);
    }
}

} // namespace gp
