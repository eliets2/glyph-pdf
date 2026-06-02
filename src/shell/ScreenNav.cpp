// SPDX-License-Identifier: Apache-2.0
#include "ScreenNav.h"
#include "util/GpTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

namespace gp {

ScreenNav::ScreenNav(QWidget* parent) : QFrame(parent) {
    setObjectName("screenNav");
    setFixedHeight(Theme::ScreenNavH);
    setAccessibleName(tr("Screen navigation"));
    setAccessibleDescription(tr("Switch between specialized screens like OCR, Redaction, Signatures, and more"));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    auto* label = new QLabel(tr("SCREENS"));
    label->setProperty("role", "screenNavLabel");
    row->addWidget(label);

    const QVector<QPair<QPair<QString,QString>, QString>> items = {
        {{"",          tr("Standard")},      "00"},
        {{"ocr",       tr("OCR Verify")},    "01"},
        {{"redact",    tr("Redaction")},      "02"},
        {{"signature", tr("Signatures")},     "03"},
        {{"compare",   tr("Compare")},        "04"},
        {{"pages",     tr("Pages")},          "05"},
        {{"batch",     tr("Batch")},          "06"},
        {{"ai",        tr("AI Chat")},        "07"},
        {{"form",      tr("Form Builder")},   "08"},
        {{"compress",  tr("Compress")},       "09"},
        {{"pdfa",      tr("PDF/A")},          "10"},
        {{"watermark", tr("Watermark")},      "11"},
    };
    for (const auto& itm : items) {
        const QString id  = itm.first.first;
        const QString lbl = itm.first.second;
        const QString num = itm.second;
        auto* b = new QToolButton;
        b->setText(QString("%1   %2").arg(num, lbl.toUpper()));
        b->setProperty("variant", "screenNav");
        b->setCheckable(true);
        b->setAutoExclusive(true);
        b->setFocusPolicy(Qt::TabFocus);
        b->setAccessibleName(tr("Screen: %1").arg(lbl));
        if (id.isEmpty()) b->setChecked(true);
        connect(b, &QToolButton::clicked, this, [this, id]() {
            _active = id;
            emit screenSelected(id);
        });
        _items.insert(id, b);
        row->addWidget(b);
    }
    row->addStretch(1);
}

void ScreenNav::setActive(const QString& id) {
    if (auto* b = _items.value(id)) b->setChecked(true);
    _active = id;
}

} // namespace gp
