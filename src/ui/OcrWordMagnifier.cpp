// SPDX-License-Identifier: Apache-2.0
#include "OcrWordMagnifier.h"

#include <QPainter>

namespace gp {

namespace {
// Margin around the letterboxed crop inside the pane.
constexpr qreal kMargin = 8.0;
// The pane's paper background (matches the old zoom-pane swatch).
const char* kBackground = "#e8e6df";
// Selection ring color (same blue the scan-pane selection uses).
const char* kRingColor = "#2563eb";
} // namespace

OcrWordMagnifier::OcrWordMagnifier(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(QStringLiteral("background:%1;").arg(kBackground));
}

void OcrWordMagnifier::setPageImage(const QImage& image)
{
    m_image = image;
    // Re-derive the crop against the new image bounds (the word box stays in
    // image pixel space; the clamping may change).
    if (!m_wordBox.isEmpty())
        m_sourceRect = sourceRectFor(m_wordBox, m_image.size());
    update();
}

void OcrWordMagnifier::setCropRect(const QRectF& wordBoxPx)
{
    m_wordBox = wordBoxPx;
    m_sourceRect = sourceRectFor(wordBoxPx, m_image.size());
    update();
}

void OcrWordMagnifier::clearSelection()
{
    m_wordBox = QRectF();
    m_sourceRect = QRect();
    update();
}

QRect OcrWordMagnifier::sourceRectFor(const QRectF& wordBox, const QSize& imageSize,
                                      qreal padding)
{
    if (wordBox.isEmpty() || wordBox.width() <= 0 || wordBox.height() <= 0
        || imageSize.isEmpty())
        return QRect();

    // Expand ~25% of the box's own extent on each side, then clamp to the
    // image bounds. Boxes are already in pageImage pixel space — no DPR math.
    const QRectF padded = wordBox.adjusted(-wordBox.width() * padding,
                                           -wordBox.height() * padding,
                                            wordBox.width() * padding,
                                            wordBox.height() * padding);
    const QRectF clamped = padded.intersected(
        QRectF(QPointF(0, 0), QSizeF(imageSize)));
    if (clamped.isEmpty())
        return QRect();
    return clamped.toAlignedRect();
}

qreal OcrWordMagnifier::magnificationFor(const QRectF& sourceRect, const QSizeF& pane)
{
    if (sourceRect.isEmpty() || sourceRect.width() <= 0 || sourceRect.height() <= 0)
        return 0.0;
    const QRectF target = QRectF(QPointF(0, 0), pane)
                              .adjusted(kMargin, kMargin, -kMargin, -kMargin);
    if (target.isEmpty())
        return 0.0;
    return qMin(target.width() / sourceRect.width(),
                target.height() / sourceRect.height());
}

qreal OcrWordMagnifier::currentMagnification() const
{
    if (m_sourceRect.isEmpty())
        return 0.0;
    return magnificationFor(QRectF(m_sourceRect), QSizeF(size()));
}

void OcrWordMagnifier::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(kBackground));

    if (m_image.isNull() || m_sourceRect.isEmpty()) {
        p.setPen(QColor(QStringLiteral("#777777")));
        p.drawText(rect(), Qt::AlignCenter, tr("No word selected"));
        return;
    }

    // Letterboxed, centered draw of the padded crop — SmoothPixmapTransform
    // smooths when upscaling; each repaint blits only this small source region.
    const QRectF target = QRectF(rect()).adjusted(kMargin, kMargin, -kMargin, -kMargin);
    const qreal scale = magnificationFor(QRectF(m_sourceRect), QSizeF(size()));
    if (scale <= 0.0)
        return;
    const qreal drawW = m_sourceRect.width() * scale;
    const qreal drawH = m_sourceRect.height() * scale;
    const QPointF topLeft(target.x() + (target.width() - drawW) / 2.0,
                          target.y() + (target.height() - drawH) / 2.0);
    const QRectF drawRect(topLeft, QSizeF(drawW, drawH));

    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(drawRect, m_image, QRectF(m_sourceRect));

    // The selected word's box, mapped into the drawn crop.
    if (!m_wordBox.isEmpty()) {
        const QRectF ring((m_wordBox.topLeft()
                               - QPointF(m_sourceRect.topLeft()))
                                  * scale + topLeft,
                          QSizeF(m_wordBox.width() * scale, m_wordBox.height() * scale));
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(QColor(kRingColor), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(ring);
    }
}

} // namespace gp
