// SPDX-License-Identifier: Apache-2.0
#include "SignaturesPanel.h"
#include "util/GpTheme.h"
#include "util/Badge.h"
#include "core/interfaces/ISignatureManager.h"
#include "ui/PdfViewerWidget.h"   // §9.7 P0: SignatureBadgeSpec / SignatureBadgeState (badge seam payload)

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace gp {

namespace {

// ── §9.7 P0: SignatureInfo → on-page badge state (binding mapping) ──────────
//   no validation data at all            → Unknown (gray)
//   integrityIntact == false             → ModifiedAfterSigning (red)
//   integrity intact + valid + trusted   → ValidTrusted (green)
//   integrity intact, untrusted chain    → UntrustedChain (amber)
SignatureBadgeState gpBadgeStateFor(const SignatureInfo &s)
{
    const bool noData = s.trustStatus.isEmpty() && s.signerName.isEmpty()
                        && s.fieldName.isEmpty() && !s.date.isValid();
    if (noData)
        return SignatureBadgeState::Unknown;
    if (!s.integrityIntact)
        return SignatureBadgeState::ModifiedAfterSigning;
    // Trusted chain per the engine's trustStatus vocabulary: "Valid" /
    // "ValidWithDSS" (see SignatureManager::validateSignatures). Anything else
    // — UntrustedChain, CertExpired, WeakKey, InvalidEKU, ... — is a chain the
    // viewer must NOT paint green.
    const bool trusted =
        s.trustStatus.compare(QStringLiteral("Valid"), Qt::CaseInsensitive) == 0
        || s.trustStatus.compare(QStringLiteral("ValidWithDSS"), Qt::CaseInsensitive) == 0;
    if (s.isValid && trusted)
        return SignatureBadgeState::ValidTrusted;
    return SignatureBadgeState::UntrustedChain;
}

// Tooltip = signer name + status detail (§9.7 P0 design).
QString gpBadgeTooltipFor(const SignatureInfo &s)
{
    const QString name = s.signerName.isEmpty() ? QStringLiteral("(unnamed signer)") : s.signerName;
    const bool noData = s.trustStatus.isEmpty() && s.signerName.isEmpty()
                        && s.fieldName.isEmpty() && !s.date.isValid();
    if (noData)
        return QStringLiteral("%1 — not validated").arg(name);
    if (!s.integrityIntact)
        return QStringLiteral("%1 — MODIFIED AFTER SIGNING (integrity check failed)").arg(name);
    const bool trusted =
        s.trustStatus.compare(QStringLiteral("Valid"), Qt::CaseInsensitive) == 0
        || s.trustStatus.compare(QStringLiteral("ValidWithDSS"), Qt::CaseInsensitive) == 0;
    if (s.isValid && trusted)
        return QStringLiteral("%1 — VALID (trust: %2)").arg(name, s.trustStatus);
    return QStringLiteral("%1 — signature intact, chain NOT trusted (%2)").arg(name, s.trustStatus);
}

} // namespace

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
        "<span style='color:#a8abb0; font-size:8pt; letter-spacing:0.4px; font-family:JetBrains Mono'>"
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

    m_placeBtn = new QPushButton(tr("Place Signature →"));
    m_placeBtn->setStyleSheet(
        "QPushButton{background:#ff8c42;color:#1a1b1e;border:1px solid #ff8c42;"
        "font-weight:700;letter-spacing:0.6px;padding:10px 14px;}"
        "QPushButton:hover{background:#ff9d5c;}"
        "QPushButton:disabled{background:#5a5a5a;border-color:#5a5a5a;color:#888;}");
    // O2: disabled until setDocument() receives a real, non-empty file path.
    m_placeBtn->setEnabled(false);
    m_placeBtn->setToolTip(tr("Open a document first"));
    connect(m_placeBtn, &QPushButton::clicked, this, &SignaturesPanel::placeSignatureRequested);
    col->addWidget(m_placeBtn);

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

    // O2: enable/disable "Place Signature" based on whether a document is loaded.
    if (m_placeBtn) {
        const bool hasDoc = !filePath.isEmpty();
        m_placeBtn->setEnabled(hasDoc);
        m_placeBtn->setToolTip(hasDoc ? QString() : tr("Open a document first"));
    }

    // §9.7 P0: badges are pushed on EVERY setDocument path — an empty list
    // clears any stale on-page badges when the document has no signatures.
    QList<SignatureBadgeSpec> badges;

    if (filePath.isEmpty()) { showNoSignatures(tr("NO DOCUMENT")); emit signatureBadgesChanged(badges); return; }
    if (!signing)           { showNoSignatures(tr("UNAVAILABLE")); emit signatureBadgesChanged(badges); return; }

    const QList<SignatureInfo> sigs = signing->validateSignatures(filePath);
    if (sigs.isEmpty()) { showNoSignatures(tr("UNSIGNED")); emit signatureBadgesChanged(badges); return; }

    // §9.7 P0: one badge per signature, mapped from the validation flow's
    // outcome (gpBadgeStateFor). SignatureInfo carries NO page / field rect —
    // the on-page anchor (pageIndex + fieldRect) must be resolved by the
    // consumer that owns the field geometry; the panel contributes the
    // validated state and the signer/status tooltip. Unanchored specs are
    // stored by the viewer but only painted once anchored.
    badges.reserve(sigs.size());
    // §9.7 badge anchoring: the engine resolves each signature field's on-page
    // anchor (widget /Rect, top-left convention); match by fieldName.
    const auto anchors = signing->signatureFieldAnchors(filePath);
    for (const SignatureInfo& s : sigs) {
        SignatureBadgeSpec b;
        b.state = gpBadgeStateFor(s);
        b.tooltip = gpBadgeTooltipFor(s);
        for (const auto& a : anchors) {
            if (a.fieldName == s.fieldName) {
                b.pageIndex = a.pageIndex;
                b.fieldRect = a.rect;
                break;
            }
        }
        badges.append(b);
    }
    emit signatureBadgesChanged(badges);

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
            tr("<span style='color:#a8abb0; font-size:8pt; letter-spacing:0.4px; "
               "font-family:JetBrains Mono'>%1 signatures present.<br/>Showing the most "
               "recent below.</span>").arg(sigs.size()));
    }
}

} // namespace gp
