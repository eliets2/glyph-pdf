#include "SignaturesPanel.h"
#include "util/GpTheme.h"
#include "util/Badge.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace gp {

SignaturesPanel::SignaturesPanel(QWidget* parent) : QFrame(parent) {
    setObjectName("rightSidebar");
    setFixedWidth(Theme::RightPaneW);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* head = new QFrame; head->setProperty("role","modeToolbar"); head->setFixedHeight(32);
    auto* hr = new QHBoxLayout(head); hr->setContentsMargins(10,0,10,0);
    auto* t = new QLabel(tr("SIGN DOCUMENT")); t->setStyleSheet("font-weight:600;letter-spacing:1.2px;");
    hr->addWidget(t); hr->addStretch(1);
    outer->addWidget(head);

    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto* body = new QWidget;
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(12, 12, 12, 12);
    col->setSpacing(14);

    // DIGITAL ID section
    auto* idCard = new QFrame;
    idCard->setStyleSheet("border:1px solid #393b40; background:#1a1b1e; padding:10px;");
    auto* idLay = new QFormLayout(idCard);
    idLay->setLabelAlignment(Qt::AlignRight);
    auto monoVal = [](const QString& v) { auto* l = new QLabel(v); l->setProperty("mono", true); return l; };
    auto monoKey = [](const QString& k) { auto* l = new QLabel(k); l->setProperty("mono", true); l->setStyleSheet("color:#71747a;"); return l; };
    idLay->addRow(monoKey(tr("SUBJECT")),    monoVal("Elie Matta"));
    idLay->addRow(monoKey(tr("ISSUER")),     monoVal("GlobalSign CA"));
    idLay->addRow(monoKey(tr("EXPIRES")),    monoVal("2027-03-14"));
    idLay->addRow(monoKey(tr("ALGORITHM")),  monoVal("RSA-2048"));
    idLay->addRow(monoKey(tr("FINGERPRINT")),monoVal("A8:F2:31:8E:…"));
    auto* badgeRow = new QHBoxLayout;
    badgeRow->addWidget(new Badge("✓ VALID", Badge::Ok));
    badgeRow->addStretch(1);
    auto* chainLab = new QLabel(tr("CHAIN OK · TRUSTED"));
    chainLab->setProperty("mono", true);
    badgeRow->addWidget(chainLab);
    idLay->addRow(badgeRow);
    col->addWidget(idCard);

    // APPEARANCE
    auto* appCard = new QFrame;
    appCard->setStyleSheet("border:1px solid #393b40; background:#1a1b1e;");
    auto* appLay = new QVBoxLayout(appCard);
    appLay->setContentsMargins(12, 12, 12, 12);
    appLay->setSpacing(10);

    auto* preview = new QFrame;
    preview->setFixedHeight(120);
    preview->setStyleSheet("background:#1a1b1e; border:1px solid #4a4d52;");
    auto* prevLay = new QHBoxLayout(preview);
    prevLay->setContentsMargins(12, 8, 12, 8);
    auto* glyph = new QLabel("Em");
    glyph->setStyleSheet("font-family:Manrope; font-size:32px; font-style:italic; color:#ff8c42;");
    prevLay->addWidget(glyph);
    prevLay->addSpacing(12);
    auto* prevText = new QLabel(
        "<b style='color:#dfe1e5'>Elie Matta</b><br/>"
        "<span style='color:#a8abb0; font-size:9px; letter-spacing:0.4px; font-family:JetBrains Mono'>"
        "SIGNED BY: ELIE MATTA<br/>2026-05-14 09:42:17 +00:00<br/>BERLIN, DE</span>");
    prevText->setTextFormat(Qt::RichText);
    prevLay->addWidget(prevText, 1);
    appLay->addWidget(preview);

    auto* layoutRow = new QHBoxLayout;
    layoutRow->addWidget(new QRadioButton(tr("Name + Details")));
    layoutRow->addWidget(new QRadioButton(tr("Name Only")));
    appLay->addLayout(layoutRow);

    auto* form = new QFormLayout;
    form->addRow(tr("Reason"),   new QLineEdit(tr("Approved for distribution")));
    form->addRow(tr("Location"), new QLineEdit(tr("Berlin, DE")));
    form->addRow(tr("Contact"),  new QLineEdit("em@glyph.example"));
    appLay->addLayout(form);
    col->addWidget(appCard);

    auto* place = new QPushButton(tr("Place Signature →"));
    place->setStyleSheet(
        "QPushButton{background:#ff8c42;color:#1a1b1e;border:1px solid #ff8c42;"
        "font-weight:700;letter-spacing:0.6px;padding:10px 14px;}"
        "QPushButton:hover{background:#ff9d5c;}");
    col->addWidget(place);

    col->addStretch(1);
    scroll->setWidget(body);
    outer->addWidget(scroll, 1);
}

} // namespace gp
