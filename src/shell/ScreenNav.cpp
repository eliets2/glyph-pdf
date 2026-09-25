// SPDX-License-Identifier: Apache-2.0
#include "ScreenNav.h"
#include "TaskNav.h"
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

    // U02: one table — items, order and titles come from TaskNav. The old
    // "01 OCR VERIFY" numbering is gone; the title IS the task name.
    for (const auto& spec : TaskNav::tasks()) {
        const QString id  = QString::fromLatin1(spec.id);
        const QString lbl = QString::fromLatin1(spec.title);
        auto* b = new QToolButton;
        b->setObjectName(QStringLiteral("screenNav_") + id);
        b->setText(lbl);
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
