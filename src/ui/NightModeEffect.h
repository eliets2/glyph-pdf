// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QGraphicsEffect>
#include <QImage>
#include <QPainter>
#include <QPixmap>

namespace gp {

// Night Mode's page filter: draws whatever the widget paints with its RGB
// channels inverted (alpha kept), so a white page with black text becomes a
// dark page with light text. Qt ships no inversion effect (only blur,
// colorize, drop-shadow and opacity), hence this small subclass.
class NightModeEffect : public QGraphicsEffect {
public:
    explicit NightModeEffect(QObject *parent = nullptr) : QGraphicsEffect(parent) {}

protected:
    void draw(QPainter *painter) override
    {
        QPoint offset;
        const QPixmap source = sourcePixmap(Qt::LogicalCoordinates, &offset,
                                            QGraphicsEffect::PadToEffectiveBoundingRect);
        if (source.isNull()) {
            drawSource(painter);
            return;
        }
        QImage inverted = source.toImage().convertToFormat(QImage::Format_ARGB32);
        inverted.invertPixels(QImage::InvertRgb);
        painter->drawImage(offset, inverted);
    }
};

} // namespace gp
