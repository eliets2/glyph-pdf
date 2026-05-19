#include "RedactMode.h"
#include "util/GpTheme.h"
#include "ui/PdfViewerWidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

RedactMode::RedactMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // toolbar
    auto* tb = new QFrame;
    tb->setProperty("role","modeToolbar");
    tb->setFixedHeight(Theme::ToolbarH);
    auto* row = new QHBoxLayout(tb);
    row->setContentsMargins(10,0,10,0); row->setSpacing(6);
    auto monoLab = [](const QString& s){ auto* l = new QLabel(s); l->setProperty("mono",true); return l; };
    row->addWidget(monoLab("REDACT"));

    auto pill = [&](const QString& t, bool active=false) {
        auto* b = new QToolButton; b->setText(t);
        b->setProperty("variant", "ghost");
        b->setCheckable(true);
        b->setChecked(active);
        return b;
    };
    row->addWidget(pill("Mark Region", true));
    row->addWidget(pill("Mark by Pattern ▾"));
    row->addWidget(pill("Mark All Occurrences"));
    row->addStretch(1);
    auto* apply = new QToolButton; apply->setText("Apply All Redactions"); apply->setProperty("variant","danger"); row->addWidget(apply);
    auto* exit  = new QToolButton; exit->setText("Cancel"); exit->setProperty("variant","ghost"); row->addWidget(exit);
    col->addWidget(tb);

    // canvas with redaction overlay — reuse a PdfViewerWidget
    auto* canvas = new PdfViewerWidget;
    col->addWidget(canvas, 1);

    // info strip
    auto* info = new QFrame;
    info->setProperty("role","infoStrip");
    info->setFixedHeight(Theme::InfoStripH);
    auto* irow = new QHBoxLayout(info);
    irow->setContentsMargins(12,0,12,0); irow->setSpacing(14);
    auto i = [](const QString& s){ auto* l = new QLabel(s); l->setProperty("role","infoStrip"); return l; };
    irow->addWidget(i("4 REGIONS MARKED"));
    irow->addWidget(i("2 TEXT · 1 IMAGE · 1 PATTERN"));
    irow->addWidget(i("ESTIMATED 847 CHARACTERS REMOVED"));
    irow->addStretch(1);
    col->addWidget(info);
}

} // namespace gp
