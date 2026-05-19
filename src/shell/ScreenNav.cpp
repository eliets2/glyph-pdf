#include "ScreenNav.h"
#include "util/GpTheme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

namespace gp {

ScreenNav::ScreenNav(QWidget* parent) : QFrame(parent) {
    setObjectName("screenNav");
    setFixedHeight(Theme::ScreenNavH);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    auto* label = new QLabel("SCREENS");
    label->setProperty("role", "screenNavLabel");
    row->addWidget(label);

    const QVector<QPair<QPair<QString,QString>, QString>> items = {
        {{"",          "Standard"},      "00"},
        {{"ocr",       "OCR Verify"},    "01"},
        {{"redact",    "Redaction"},     "02"},
        {{"signature", "Signatures"},    "03"},
        {{"compare",   "Compare"},       "04"},
        {{"pages",     "Pages"},         "05"},
        {{"batch",     "Batch"},         "06"},
        {{"ai",        "AI Chat"},       "07"},
        {{"form",      "Form Builder"},  "08"},
        {{"compress",  "Compress"},      "09"},
        {{"pdfa",      "PDF/A"},         "10"},
        {{"watermark", "Watermark"},     "11"},
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
