#pragma once
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace gp {

// Tiny SVG-icon factory. We ship monochrome geometric glyphs at 16x16. The
// caller passes a name (matching the HTML icon set) and an optional color;
// colored variants are rendered by patching the SVG's currentColor.
class Icons {
public:
    static QIcon get(const QString& name, const QColor& tint = QColor());
    static QPixmap pixmap(const QString& name, int sizePx, const QColor& tint = QColor());

private:
    // Returns the raw SVG markup for a named icon, or an empty string if unknown.
    static QString svg(const QString& name);
};

} // namespace gp
