// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QString>

class QJsonArray;
class QSettings;

namespace gp {

// R24(c): ONE audit surface for the app's network behavior. Every network
// touchpoint of the existing code paths, enumerated at runtime with its
// enabled/disabled state and a pointer to the consent setting that governs
// it. Read-only disclosure — enumeration is a pure QSettings read and the
// Preferences "Network" page performs no network requests.
//
// Honesty contract:
//   * `enabled` = the touchpoint will fire under the CURRENT settings when
//     its invocation moment comes — for ungated on-demand features (local
//     Ollama chat, OCSP during validation) that is `true` by construction
//     and the disclosure text says exactly when it fires. It is never a
//     claim that the network is being used right now, nor a usage history.
//     SWEEP-W1 F4: "current settings" includes the machine-policy snapshot —
//     for keys ENFORCED through PolicyController (signing/tsaUrl) the state
//     is derived from the policy-EFFECTIVE value, matching what the next
//     dispatch will actually do; recognized-but-pending keys stay raw
//     QSettings reads, matching their "pending" enforcement notes.
//   * A touchpoint with NO consent switch (OCSP) carries an empty
//     `consentKey` and says so in `disclosure` — the gap is disclosed, not
//     papered over.
//   * No destination URLs and no history leave this surface into the
//     support bundle: onOffJson() carries id + enabled + invocation only.
struct NetworkTouchpoint {
    QString id;         // stable id: ollama | tsa | ocsp | update-check | ocr-traineddata
    QString label;      // human name for the Network page
    QString invocation; // "on demand" | "automatic" | "startup + on demand"
    bool enabled;       // derived from existing code paths + current settings
    QString consentKey; // governing settings key; empty = no switch exists
    QString disclosure; // honest one-paragraph note
};

class NetworkTouchpoints {
public:
    // Pure: reads ONLY the passed settings. The five touchpoints mirror the
    // existing code paths: OllamaProvider (local chat endpoint),
    // SignatureManager TSA httpPost (signing/tsaUrl), SignatureManager
    // fetchOcspResponse (cert AIA responder), UpdateChecker
    // (update/checkOnStartup + manual Check Now), OcrEngine downloadTrainedData
    // (ocr/allowNetworkDownload, default OFF).
    static QList<NetworkTouchpoint> enumerate(QSettings& s);

    // Support-bundle shape: id + enabled + invocation only.
    static QJsonArray onOffJson(const QList<NetworkTouchpoint>& list);
};

} // namespace gp
