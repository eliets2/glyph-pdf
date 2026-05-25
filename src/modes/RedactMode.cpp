#include "RedactMode.h"
#include "util/GpTheme.h"
#include "ui/PdfViewerWidget.h"

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

RedactMode::RedactMode(QWidget* parent) : QWidget(parent) {
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);

    // ── v1.0.0 preview banner ─────────────────────────────────────────
    auto* previewBanner = new QLabel(tr("Preview — not wired in v1.0.0\n\nUse the ribbon toolbar for production-ready operations.\nThis mode page is scheduled for v1.1."), this);
    previewBanner->setObjectName("modePreviewBanner");
    previewBanner->setAlignment(Qt::AlignCenter);
    previewBanner->setStyleSheet("QLabel#modePreviewBanner { background: rgba(255, 200, 100, 0.92); color: #5c3000; font-weight: bold; padding: 24px; border: 2px solid #c87000; border-radius: 8px; }");
    previewBanner->setWordWrap(true);
    col->insertWidget(0, previewBanner);

    // toolbar
    auto* tb = new QFrame;
    tb->setProperty("role","modeToolbar");
    tb->setFixedHeight(Theme::ToolbarH);
    auto* row = new QHBoxLayout(tb);
    row->setContentsMargins(10,0,10,0); row->setSpacing(6);
    auto monoLab = [](const QString& s){ auto* l = new QLabel(s); l->setProperty("mono",true); return l; };
    row->addWidget(monoLab(tr("REDACT")));

    auto pill = [&](const QString& t, bool active=false) {
        auto* b = new QToolButton; b->setText(t);
        b->setProperty("variant", "ghost");
        b->setCheckable(true);
        b->setChecked(active);
        return b;
    };
    row->addWidget(pill(tr("Mark Region"), true));
    row->addWidget(pill(tr("Mark by Pattern ▾")));
    row->addWidget(pill(tr("Mark All Occurrences")));
    row->addStretch(1);
    auto* apply = new QToolButton; apply->setText(tr("Apply All Redactions")); apply->setProperty("variant","danger"); row->addWidget(apply);
    auto* exit  = new QToolButton; exit->setText(tr("Cancel")); exit->setProperty("variant","ghost"); row->addWidget(exit);
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
    irow->addWidget(i(tr("— REGIONS MARKED")));
    irow->addWidget(i(tr("— TEXT · — IMAGE · — PATTERN")));
    irow->addWidget(i(tr("ESTIMATED — CHARACTERS REMOVED")));
    irow->addStretch(1);
    col->addWidget(info);

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
