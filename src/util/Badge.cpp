#include "Badge.h"
#include "GpTheme.h"

#include <QStyle>

namespace gp {

Badge::Badge(const QString& text, Kind kind, QWidget* parent)
    : QLabel(text, parent) {
    setKind(kind);
    setAlignment(Qt::AlignCenter);
    setFixedHeight(18);
    setContentsMargins(6, 0, 6, 0);
}

void Badge::setKind(Kind k) {
    const char* v = "badge-ok";
    switch (k) {
    case Ok:   v = "badge-ok";   break;
    case Warn: v = "badge-warn"; break;
    case Err:  v = "badge-err";  break;
    case Info: v = "badge-info"; break;
    case Beta: v = "badge-beta"; break;
    }
    setProperty("variant", v);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

} // namespace gp
