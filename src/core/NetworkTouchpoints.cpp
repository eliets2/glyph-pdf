// SPDX-License-Identifier: Apache-2.0
// R24(c) — the ONE network audit surface. Enumerates every network touchpoint
// of the EXISTING code paths with its derived enabled/disabled state and a
// pointer to the consent setting that governs it. Pure: reads only the passed
// settings; no network is touched. See NetworkTouchpoints.h for the honesty
// contract.
#include "core/NetworkTouchpoints.h"
#include "core/PolicyController.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>

#include "core/PolicyController.h"  // R24 wiring: effective-value disclosure
// R24 wiring closure: the OCSP consent key's single definition lives with
// the consent surface (both files are pdfws_ui; the literal must not drift).
#include "ui/OcspConsentDialog.h"

namespace gp {

QList<NetworkTouchpoint> NetworkTouchpoints::enumerate(QSettings& s)
{
    QList<NetworkTouchpoint> out;

    // R24 wiring closure: the states below are derived from the EFFECTIVE
    // values (machine policy over the passed user settings) — under a policy
    // this page must not show the user's raw preference as the state.
    // 1. OllamaProvider — local AI chat. POSTs to the user-configured
    //    endpoint (default http://localhost:11434) only when AI Chat or
    //    Preferences → Test connection is used. No consent switch exists:
    //    the feature is user-invoked by construction.
    out.append(NetworkTouchpoint{
        QStringLiteral("ollama"),
        QObject::tr("AI chat (local Ollama server)"),
        QObject::tr("on demand"),
        true,
        QString(),
        QObject::tr("Fires only when you use AI Chat or Preferences → "
                    "Test connection. Talks to the LOCAL endpoint configured "
                    "in Preferences → AI (default http://localhost:11434); "
                    "document content goes to that local server only."),
    });

    // 2. SignatureManager TSA timestamping — httpPost(tsaUrl) during B-T /
    //    B-LT / B-LTA signing and document timestamps. Governed by
    //    signing/tsaUrl (empty = disabled; the controller refuses levels
    //    above B-B before any network attempt). HTTPS enforced — HTTP URLs
    //    are refused.
    //    SWEEP-W1 F4: `enabled` must mean "will fire under the CURRENT
    //    settings" — and enforcement resolves signing/tsaUrl through the
    //    machine policy (SecurityController::readSigningConfig ->
    //    PolicyController::effectiveValue). The row therefore reads the
    //    EFFECTIVE value, never the raw user setting alone, so the page and
    //    the support bundle cannot show "Disabled" while the next sign will
    //    fetch the policy TSA.
    PolicyController::instance().ensureLoaded();
    const QString tsaUrl = PolicyController::instance()
        .effectiveValue(QStringLiteral("signing/tsaUrl"),
                        s.value(QStringLiteral("signing/tsaUrl")).toString())
        .toString()
        .trimmed();
    out.append(NetworkTouchpoint{
        QStringLiteral("tsa"),
        QObject::tr("RFC 3161 timestamping (TSA)"),
        QObject::tr("on demand"),
        !tsaUrl.isEmpty(),
        QStringLiteral("signing/tsaUrl"),
        tsaUrl.isEmpty()
            ? QObject::tr("Disabled: no TSA URL configured. Signing levels "
                          "above B-B are refused up front — no timestamp "
                          "request is attempted.")
            : QObject::tr("Enabled: a timestamp token is fetched from the "
                          "configured TSA whenever signing requests a level "
                          "above B-B or a document timestamp. HTTPS is "
                          "enforced (HTTP URLs are refused). A machine "
                          "policy that manages signing/tsaUrl overrides the "
                          "user setting here (see the policy trust-model "
                          "disclosure in Preferences)."),
    });

    // 3. SignatureManager fetchOcspResponse — during a B-LT/B-LTA signing
    //    dispatch the responder URL from the certificate's AIA extension is
    //    contacted to build the DSS. R24 wiring closure: this USED to fire
    //    with no consent switch at all — it is now gated by
    //    signing/ocspNetworkPolicy (SecurityController refuses the B-LT/B-LTA
    //    dispatch before any network attempt when consent is "never" or not
    //    granted per document).
    auto& policy = PolicyController::instance();
    policy.ensureLoaded();
    const QString ocspPolicy =
        policy
            .effectiveValue(QLatin1String(OcspNetworkPolicyKey),
                            s.value(QLatin1String(OcspNetworkPolicyKey),
                                    QStringLiteral("ask")))
            .toString();
    const bool ocspEgressPossible = ocspPolicy == QLatin1String("ask");
    out.append(NetworkTouchpoint{
        QStringLiteral("ocsp"),
        QObject::tr("Certificate revocation check (OCSP)"),
        ocspEgressPossible ? QObject::tr("on consent, per document")
                           : QObject::tr("never"),
        ocspEgressPossible,
        QLatin1String(OcspNetworkPolicyKey),
        ocspEgressPossible
            ? QObject::tr("Consent-gated: before signing at PAdES B-LT/B-LTA "
                          "(the levels that build long-term-validation data) "
                          "a consent dialog asks once per document — allow "
                          "once, allow for the document, or deny. The "
                          "responder URL comes from the certificate's AIA "
                          "extension; the request is HTTPS-only and carries "
                          "certificate identifiers, never document content.")
            : QObject::tr("Disabled: the network consent setting "
                          "signing/ocspNetworkPolicy is not \"ask\", so "
                          "signing never contacts an OCSP responder (B-LT/"
                          "B-LTA signing is refused up front with that "
                          "reason; B-B/B-T need no OCSP)."),
    });

    // 4. UpdateChecker — manifest GET (HTTPS enforced). The startup leg is
    //    governed by update/checkOnStartup (default OFF); Check Now in
    //    Preferences is manual on demand. R24 wiring: the machine policy
    //    overrides the stored preference at the decision point
    //    (MainWindow::initUpdateChecker), so the state follows the policy.
    const bool updateOnStartup =
        policy
            .effectiveValue(QStringLiteral("update/checkOnStartup"),
                            s.value(QStringLiteral("update/checkOnStartup"), false))
            .toBool();
    out.append(NetworkTouchpoint{
        QStringLiteral("update-check"),
        QObject::tr("Update check"),
        QObject::tr("startup + on demand"),
        updateOnStartup,
        QStringLiteral("update/checkOnStartup"),
        updateOnStartup
            ? QObject::tr("Enabled at startup: the manifest is fetched on "
                          "launch (HTTPS enforced). The manual Check Now in "
                          "Preferences always runs on demand.")
            : QObject::tr("Disabled at startup (default OFF). The manual "
                          "Check Now in Preferences runs on demand."),
    });

    // 5. OcrEngine downloadTrainedData — only when a Tesseract language pack
    //    is missing AND ocr/allowNetworkDownload is ON (default OFF).
    //    Bundled / AppData packs are used first, so OCR normally needs no
    //    network at all. R24 wiring: the machine policy overrides the stored
    //    preference at the engine's model-load gate.
    const bool ocrDownload =
        policy
            .effectiveValue(QStringLiteral("ocr/allowNetworkDownload"),
                            s.value(QStringLiteral("ocr/allowNetworkDownload"), false))
            .toBool();
    out.append(NetworkTouchpoint{
        QStringLiteral("ocr-traineddata"),
        QObject::tr("OCR language-pack download"),
        QObject::tr("on demand"),
        ocrDownload,
        QStringLiteral("ocr/allowNetworkDownload"),
        ocrDownload
            ? QObject::tr("Enabled: a missing Tesseract language pack is "
                          "downloaded from the tessdata_best repository when "
                          "that language is first used.")
            : QObject::tr("Disabled (default OFF): missing language packs are "
                          "never downloaded; OCR uses the bundled or "
                          "installed packs only."),
    });

    return out;
}

QJsonArray NetworkTouchpoints::onOffJson(const QList<NetworkTouchpoint>& list)
{
    QJsonArray arr;
    for (const auto& tp : list) {
        arr.append(QJsonObject{
            {QStringLiteral("id"), tp.id},
            {QStringLiteral("enabled"), tp.enabled},
            {QStringLiteral("invocation"), tp.invocation},
        });
    }
    return arr;
}

} // namespace gp
