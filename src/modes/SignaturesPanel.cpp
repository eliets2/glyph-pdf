#include "SignaturesPanel.h"
#include "util/GpTheme.h"
#include "util/Badge.h"
#include "core/interfaces/ISignatureManager.h"

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

    // DIGITAL ID section — values are populated by setDocument() from the real
    // ISignatureManager::validateSignatures output. They start in an honest
    // "no document loaded" state instead of fabricated identity data.
    auto* idCard = new QFrame;
    idCard->setStyleSheet("border:1px solid #393b40; background:#1a1b1e; padding:10px;");
    auto* idLay = new QFormLayout(idCard);
    idLay->setLabelAlignment(Qt::AlignRight);
    auto monoVal = [](const QString& v) { auto* l = new QLabel(v); l->setProperty("mono", true); return l; };
    auto monoKey = [](const QString& k) { auto* l = new QLabel(k); l->setProperty("mono", true); l->setStyleSheet("color:#71747a;"); return l; };
    m_subjectVal     = monoVal(QStringLiteral("—"));
    m_issuerVal      = monoVal(QStringLiteral("—"));
    m_expiresVal     = monoVal(QStringLiteral("—"));
    m_algorithmVal   = monoVal(QStringLiteral("—"));
    m_fingerprintVal = monoVal(QStringLiteral("—"));
    idLay->addRow(monoKey(tr("SUBJECT")),     m_subjectVal);
    idLay->addRow(monoKey(tr("ISSUER")),      m_issuerVal);
    idLay->addRow(monoKey(tr("SIGNED")),      m_expiresVal);
    idLay->addRow(monoKey(tr("PAdES")),       m_algorithmVal);
    idLay->addRow(monoKey(tr("FIELD")),       m_fingerprintVal);
    auto* badgeRow = new QHBoxLayout;
    m_statusBadge = new Badge(tr("NO DOCUMENT"), Badge::Info);
    badgeRow->addWidget(m_statusBadge);
    badgeRow->addStretch(1);
    m_chainLabel = new QLabel(tr("—"));
    m_chainLabel->setProperty("mono", true);
    badgeRow->addWidget(m_chainLabel);
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
    auto* glyph = new QLabel("✍");
    glyph->setStyleSheet("font-family:Manrope; font-size:32px; font-style:italic; color:#ff8c42;");
    prevLay->addWidget(glyph);
    prevLay->addSpacing(12);
    m_previewText = new QLabel(
        "<span style='color:#a8abb0; font-size:9px; letter-spacing:0.4px; font-family:JetBrains Mono'>"
        "Signature appearance preview.<br/>Loads signer identity from the<br/>selected certificate when you sign.</span>");
    m_previewText->setTextFormat(Qt::RichText);
    prevLay->addWidget(m_previewText, 1);
    appLay->addWidget(preview);

    auto* layoutRow = new QHBoxLayout;
    layoutRow->addWidget(new QRadioButton(tr("Name + Details")));
    layoutRow->addWidget(new QRadioButton(tr("Name Only")));
    appLay->addLayout(layoutRow);

    auto* form = new QFormLayout;
    form->addRow(tr("Reason"),   new QLineEdit(tr("Approved for distribution")));
    form->addRow(tr("Location"), new QLineEdit);
    form->addRow(tr("Contact"),  new QLineEdit);
    appLay->addLayout(form);
    col->addWidget(appCard);

    auto* place = new QPushButton(tr("Place Signature →"));
    place->setStyleSheet(
        "QPushButton{background:#ff8c42;color:#1a1b1e;border:1px solid #ff8c42;"
        "font-weight:700;letter-spacing:0.6px;padding:10px 14px;}"
        "QPushButton:hover{background:#ff9d5c;}");
    connect(place, &QPushButton::clicked, this, &SignaturesPanel::placeSignatureRequested);
    col->addWidget(place);

    col->addStretch(1);
    scroll->setWidget(body);
    outer->addWidget(scroll, 1);
}

void SignaturesPanel::showNoSignatures(const QString& reason) {
    m_subjectVal->setText(QStringLiteral("—"));
    m_issuerVal->setText(QStringLiteral("—"));
    m_expiresVal->setText(QStringLiteral("—"));
    m_algorithmVal->setText(QStringLiteral("—"));
    m_fingerprintVal->setText(QStringLiteral("—"));
    m_statusBadge->setText(reason);
    m_statusBadge->setKind(Badge::Info);
    m_chainLabel->setText(QStringLiteral("—"));
}

void SignaturesPanel::setDocument(const QString& filePath, ISignatureManager* signing) {
    m_currentPath = filePath;

    if (filePath.isEmpty()) { showNoSignatures(tr("NO DOCUMENT")); return; }
    if (!signing)           { showNoSignatures(tr("UNAVAILABLE")); return; }

    const QList<SignatureInfo> sigs = signing->validateSignatures(filePath);
    if (sigs.isEmpty()) { showNoSignatures(tr("UNSIGNED")); return; }

    // Show the most recent signature (last in document order is typically the
    // newest incremental update). All values come straight from the validator.
    const SignatureInfo& s = sigs.last();

    m_subjectVal->setText(s.signerName.isEmpty() ? tr("(unnamed)") : s.signerName);
    m_issuerVal->setText(s.trustStoreUsed.isEmpty() ? tr("(no trust store)") : s.trustStoreUsed);
    m_expiresVal->setText(s.date.isValid()
        ? s.date.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : tr("(no date)"));

    // PAdES conformance summary derived from the real DSS/timestamp flags.
    QString pades = QStringLiteral("B-B");
    if (s.hasDocTimestamp)      pades = QStringLiteral("B-LTA");
    else if (s.hasDss)          pades = QStringLiteral("B-LT");
    m_algorithmVal->setText(pades);
    m_fingerprintVal->setText(s.fieldName.isEmpty() ? tr("(unnamed field)") : s.fieldName);

    // Status badge + chain reflect real integrity / trust outcome.
    if (s.isValid && s.integrityIntact) {
        m_statusBadge->setText(tr("✓ VALID"));
        m_statusBadge->setKind(Badge::Ok);
    } else if (s.integrityIntact) {
        m_statusBadge->setText(tr("⚠ UNTRUSTED"));
        m_statusBadge->setKind(Badge::Warn);
    } else {
        m_statusBadge->setText(tr("✕ INVALID"));
        m_statusBadge->setKind(Badge::Err);
    }
    m_chainLabel->setText(s.trustStatus.isEmpty() ? tr("—") : s.trustStatus.toUpper());

    if (sigs.size() > 1) {
        m_previewText->setText(
            tr("<span style='color:#a8abb0; font-size:9px; letter-spacing:0.4px; "
               "font-family:JetBrains Mono'>%1 signatures present.<br/>Showing the most "
               "recent below.</span>").arg(sigs.size()));
    }
}

} // namespace gp
