#include "FormBuilderMode.h"
#include "ui/PdfViewerWidget.h"
#include "util/GpTheme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace gp {

FormBuilderMode::FormBuilderMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0,0,0,0); col->setSpacing(0);

    // ── v1.0.0 preview banner ─────────────────────────────────────────
    auto* previewBanner = new QLabel(FormBuilderMode::tr("Preview — not wired in v1.0.0\n\nUse the ribbon toolbar for production-ready operations.\nThis mode page is scheduled for v1.1."), this);
    previewBanner->setObjectName("modePreviewBanner");
    previewBanner->setAlignment(Qt::AlignCenter);
    previewBanner->setStyleSheet("QLabel#modePreviewBanner { background: rgba(255, 200, 100, 0.92); color: #5c3000; font-weight: bold; padding: 24px; border: 2px solid #c87000; border-radius: 8px; }");
    previewBanner->setWordWrap(true);
    col->insertWidget(0, previewBanner);

    auto* tb = new QFrame; tb->setProperty("role","modeToolbar"); tb->setFixedHeight(Theme::ToolbarH);
    auto* tr = new QHBoxLayout(tb); tr->setContentsMargins(10,0,10,0); tr->setSpacing(4);
    auto mono = [](const QString& s){ auto* l = new QLabel(s); l->setProperty("mono",true); return l; };
    tr->addWidget(mono(FormBuilderMode::tr("FORM")));

    const QStringList tools = { FormBuilderMode::tr("TEXT FIELD"), FormBuilderMode::tr("CHECKBOX"), FormBuilderMode::tr("RADIO"), FormBuilderMode::tr("DROPDOWN"), FormBuilderMode::tr("LIST BOX"), FormBuilderMode::tr("DATE"), FormBuilderMode::tr("NUMERIC"), FormBuilderMode::tr("SIGNATURE"), FormBuilderMode::tr("BUTTON") };
    bool first = true;
    for (const QString& t : tools) {
        auto* b = new QToolButton; b->setText(t);
        b->setProperty("variant","ghost"); b->setCheckable(true); b->setAutoExclusive(true);
        if (first) { b->setChecked(true); first = false; }
        tr->addWidget(b);
    }
    tr->addStretch(1);
    auto* autoDet = new QToolButton; autoDet->setText(FormBuilderMode::tr("Auto-detect Fields")); autoDet->setProperty("variant","ghost"); tr->addWidget(autoDet);
    auto* tabO = new QToolButton; tabO->setText(FormBuilderMode::tr("Tab Order")); tabO->setProperty("variant","pill"); tabO->setCheckable(true); tr->addWidget(tabO);
    auto* preview = new QToolButton; preview->setText(FormBuilderMode::tr("Preview Form")); preview->setProperty("variant","ghost"); tr->addWidget(preview);
    auto* exit = new QToolButton; exit->setText(FormBuilderMode::tr("Exit Form")); exit->setProperty("variant","ghost"); tr->addWidget(exit);
    col->addWidget(tb);

    auto* canvas = new PdfViewerWidget;
    col->addWidget(canvas, 1);

    // ── Disable all interactive child widgets (preview-only mode) ────
    const auto allChildren = findChildren<QWidget*>();
    for (auto* w : allChildren) {
        if (w == previewBanner) continue;
        if (qobject_cast<QPushButton*>(w) || qobject_cast<QLineEdit*>(w) ||
            qobject_cast<QComboBox*>(w) || qobject_cast<QCheckBox*>(w) ||
            qobject_cast<QRadioButton*>(w) || qobject_cast<QToolButton*>(w)) {
            w->setEnabled(false);
        }
    }
}

} // namespace gp
