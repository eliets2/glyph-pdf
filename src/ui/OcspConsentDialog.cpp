// SPDX-License-Identifier: Apache-2.0
// R24 wiring closure — the OCSP consent surface (see OcspConsentDialog.h for
// the design contract). The engine's OCSP code is untouched: this is the
// UI/controller gate that decides whether a dispatch may reach it at all.
#include "ui/OcspConsentDialog.h"

#include <QDir>
#include <QHash>
#include <QLabel>
#include <QSettings>
#include <QPushButton>
#include <QVBoxLayout>

namespace gp {

namespace {
// QDialog carries no clickedButton() — the Allow-for-this-document branch
// is the dialog RESULT code (must not collide with QDialog::Accepted == 1
// or Rejected == 0).
constexpr int kAllowedDocumentResult = 2;

// Session-scoped per-document remember store (Allow for this document).
// Keyed by the cleaned absolute document path; cleared by resetForTesting()
// and naturally by process exit (remembering is a SESSION decision, not a
// standing consent — the send-for-signing plan D1a).
QHash<QString, OcspConsentDecision>& rememberStore()
{
    static QHash<QString, OcspConsentDecision> s_store;
    return s_store;
}

bool globalNeverNetwork()
{
    // Fail-closed: only the exact "ask" value permits the dialog path; the
    // unset default IS "ask", "never" and every unknown value refuse.
    QSettings s;
    const QString mode = s.value(QLatin1String(OcspNetworkPolicyKey),
                                 QStringLiteral("ask")).toString();
    return mode != QLatin1String("ask");
}
} // namespace

OcspConsentDialog::OcspConsentDialog(QWidget* parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("ocspConsentDialog"));
    setWindowTitle(tr("Network consent — certificate revocation check"));
    setModal(true);

    auto* col = new QVBoxLayout(this);

    m_disclosure = new QLabel(
        tr("Signing at PAdES B-LT or B-LTA embeds long-term-validation data. "
           "To build it, GlyphPDF contacts the OCSP responder URL embedded in "
           "the signing certificate (AIA extension) — a network request. "
           "The request is HTTPS-only and carries certificate identifiers "
           "only; the document content is never uploaded. Denying refuses "
           "the B-LT/B-LTA signing attempt — choose level B-T or B-B to sign "
           "without any OCSP network access."), this);
    m_disclosure->setObjectName(QStringLiteral("ocspConsentDisclosure"));
    m_disclosure->setWordWrap(true);
    col->addWidget(m_disclosure);

    m_allowOnce = new QPushButton(tr("Allow once"), this);
    m_allowOnce->setObjectName(QStringLiteral("ocspAllowOnceBtn"));
    m_allowOnce->setDefault(true);
    col->addWidget(m_allowOnce);

    m_allowDocument = new QPushButton(tr("Allow for this document"), this);
    m_allowDocument->setObjectName(QStringLiteral("ocspAllowDocumentBtn"));
    col->addWidget(m_allowDocument);

    m_deny = new QPushButton(tr("Deny (no network — signing not attempted)"), this);
    m_deny->setObjectName(QStringLiteral("ocspDenyBtn"));
    col->addWidget(m_deny);

    // QDialog carries no clickedButton() — the branch is the RESULT code:
    // Allow once = Accepted, Allow for this document = AcceptedDocument,
    // Deny = Rejected.
    connect(m_allowOnce, &QPushButton::clicked, this, [this]() { accept(); });
    connect(m_allowDocument, &QPushButton::clicked, this, [this]() {
        done(kAllowedDocumentResult);
    });
    connect(m_deny, &QPushButton::clicked, this, &QDialog::reject);
}

OcspConsentDecision OcspConsent::obtain(QWidget* parent,
                                        const QString& documentPath)
{
    // 1. The global never-network switch: refuse WITHOUT any dialog.
    if (globalNeverNetwork())
        return OcspConsentDecision::Denied;

    // 2. A remembered per-document decision (Allowed only — a Deny is never
    //    remembered: the next attempt asks again, so a misclick cannot
    //    silently lock the user out for the session).
    const QString key = QDir::cleanPath(documentPath);
    const auto remembered = rememberStore().constFind(key);
    if (remembered != rememberStore().constEnd())
        return remembered.value();

    // 3. First OCSP-needing use for this document: ask.
    OcspConsentDialog dlg(parent);
    dlg.exec();
    if (dlg.result() == kAllowedDocumentResult) {
        rememberStore().insert(key, OcspConsentDecision::AllowedDocument);
        return OcspConsentDecision::AllowedDocument;
    }
    if (dlg.result() == QDialog::Accepted)
        return OcspConsentDecision::AllowedOnce;
    return OcspConsentDecision::Denied;
}

bool OcspConsent::egressAllowed(OcspConsentDecision decision)
{
    return decision == OcspConsentDecision::AllowedOnce
           || decision == OcspConsentDecision::AllowedDocument;
}

QString OcspConsent::refusalReason(OcspConsentDecision decision)
{
    if (decision == OcspConsentDecision::Denied) {
        QSettings s;
        const QString mode = s.value(QLatin1String(OcspNetworkPolicyKey),
                                     QStringLiteral("ask")).toString();
        if (mode != QLatin1String("ask"))
            return QObject::tr(
                "Signing at PAdES B-LT/B-LTA requires contacting the "
                "certificate's OCSP responder (a network request), which is "
                "disabled by the network consent setting "
                "(signing/ocspNetworkPolicy = %1). No signature was "
                "attempted. Choose level B-T or B-B to sign without OCSP "
                "network access, or change the setting under Preferences → "
                "Security.").arg(mode);
        return QObject::tr(
            "Signing at PAdES B-LT/B-LTA requires contacting the "
            "certificate's OCSP responder (a network request), and consent "
            "was declined for this attempt. No signature was attempted. "
            "Choose level B-T or B-B to sign without OCSP network access, or "
            "allow the check when asked.");
    }
    return {};
}

void OcspConsent::resetForTesting()
{
    rememberStore().clear();
}

} // namespace gp
