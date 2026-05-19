#include "FormBuilderMode.h"
#include "ui/PdfViewerWidget.h"
#include "util/GpTheme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

FormBuilderMode::FormBuilderMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0,0,0,0); col->setSpacing(0);

    auto* tb = new QFrame; tb->setProperty("role","modeToolbar"); tb->setFixedHeight(Theme::ToolbarH);
    auto* tr = new QHBoxLayout(tb); tr->setContentsMargins(10,0,10,0); tr->setSpacing(4);
    auto mono = [](const QString& s){ auto* l = new QLabel(s); l->setProperty("mono",true); return l; };
    tr->addWidget(mono("FORM"));

    const QStringList tools = { "TEXT FIELD","CHECKBOX","RADIO","DROPDOWN","LIST BOX","DATE","NUMERIC","SIGNATURE","BUTTON" };
    bool first = true;
    for (const QString& t : tools) {
        auto* b = new QToolButton; b->setText(t);
        b->setProperty("variant","ghost"); b->setCheckable(true); b->setAutoExclusive(true);
        if (first) { b->setChecked(true); first = false; }
        tr->addWidget(b);
    }
    tr->addStretch(1);
    auto* autoDet = new QToolButton; autoDet->setText("Auto-detect Fields"); autoDet->setProperty("variant","ghost"); tr->addWidget(autoDet);
    auto* tabO = new QToolButton; tabO->setText("Tab Order"); tabO->setProperty("variant","pill"); tabO->setCheckable(true); tr->addWidget(tabO);
    auto* preview = new QToolButton; preview->setText("Preview Form"); preview->setProperty("variant","ghost"); tr->addWidget(preview);
    auto* exit = new QToolButton; exit->setText("Exit Form"); exit->setProperty("variant","ghost"); tr->addWidget(exit);
    col->addWidget(tb);

    auto* canvas = new PdfViewerWidget;
    col->addWidget(canvas, 1);
}

} // namespace gp
