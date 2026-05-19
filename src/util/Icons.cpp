#include "Icons.h"
#include "GpTheme.h"

#include <QByteArray>
#include <QHash>
#include <QPainter>
#include <QFile>

#if defined(GLYPH_HAS_SVG)
#  include <QSvgRenderer>
#endif

namespace gp {

QString Icons::svg(const QString& name) {
    QFile file(":/resources/icons/" + name + ".svg");
    if (file.open(QFile::ReadOnly)) {
        return QString::fromUtf8(file.readAll());
    }
    return QString();
}

QPixmap Icons::pixmap(const QString& name, int sizePx, const QColor& tint) {
    QString markup = svg(name);
    if (markup.isEmpty()) {
        QPixmap p(sizePx, sizePx);
        p.fill(Qt::transparent);
        QPainter pn(&p);
        pn.setBrush(Theme::fg2());
        pn.setPen(Qt::NoPen);
        pn.drawEllipse(QRect(2, 2, sizePx - 4, sizePx - 4));
        return p;
    }
#if defined(GLYPH_HAS_SVG)
    const QColor c = tint.isValid() ? tint : Theme::fg1();
    markup.replace(QStringLiteral("currentColor"), c.name());
    QSvgRenderer renderer(markup.toUtf8());
    QPixmap p(sizePx, sizePx);
    p.fill(Qt::transparent);
    QPainter pn(&p);
    renderer.render(&pn);
    return p;
#else
    Q_UNUSED(tint);
    // Without QtSvg, fall back to a tinted square placeholder so the layout
    // still has visual mass. Link Qt6::Svg to get real glyphs.
    QPixmap p(sizePx, sizePx);
    p.fill(Qt::transparent);
    QPainter pn(&p);
    pn.setBrush(tint.isValid() ? tint : Theme::fg1());
    pn.setPen(Qt::NoPen);
    pn.drawRect(QRect(2, 2, sizePx - 4, sizePx - 4));
    return p;
#endif
}

QIcon Icons::get(const QString& name, const QColor& tint) {
    QRgb tintRgb = tint.isValid() ? tint.rgba() : 0;
    QPair<QString, QRgb> key(name, tintRgb);
    static QHash<QPair<QString, QRgb>, QIcon> cache;
    auto it = cache.find(key);
    if (it != cache.end()) {
        return *it;
    }

    QIcon icon;
    for (int sz : {16, 24, 32}) {
        icon.addPixmap(pixmap(name, sz, tint));
    }
    cache.insert(key, icon);
    return icon;
}

} // namespace gp
