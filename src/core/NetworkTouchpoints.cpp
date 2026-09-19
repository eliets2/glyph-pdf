// SPDX-License-Identifier: Apache-2.0
// R24(c) — the ONE network audit surface. Enumerates every network touchpoint
// of the EXISTING code paths with its derived enabled/disabled state and a
// pointer to the consent setting that governs it. Pure: reads only the passed
// settings; no network is touched. See NetworkTouchpoints.h for the honesty
// contract.
#include "core/NetworkTouchpoints.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>

#include "core/PolicyController.h"  // R24 wiring: effective-value disclosure

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
    const QString tsaUrl =
        s.value(QStringLiteral("signing/tsaUrl")).toString().trimmed();
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
                          "enforced (HTTP URLs are refused)."),
    });

    // 3. SignatureManager fetchOcspResponse — during signature validation
    //    the responder URL from the certificate's AIA extension is contacted
    //    automatically. NO consent switch exists today — disclosed here
    //    honestly rather than papered over.
    out.append(NetworkTouchpoint{
        QStringLiteral("ocsp"),
        QObject::tr("Certificate revocation check (OCSP)"),
        QObject::tr("automatic"),
        true,
        QString(),
        QObject::tr("Fires automatically during signature validation: the "
                    "responder URL embedded in the certificate (AIA "
                    "extension) is contacted. No Preferences consent switch "
                    "exists yet — this is disclosed here rather than "
                    "silently assumed away."),
    });

    // 4. UpdateChecker — manifest GET (HTTPS enforced). The startup leg is
    //    governed by update/checkOnStartup (default OFF); Check Now in
    //    Preferences is manual on demand. R24 wiring: the machine policy
    //    overrides the stored preference at the decision point
    //    (MainWindow::initUpdateChecker), so the state follows the policy.
    auto& policy = PolicyController::instance();
    policy.ensureLoaded();
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
