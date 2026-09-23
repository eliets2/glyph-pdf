// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

class QSettings;

namespace gp {

class CapabilityRegistry;

// R24(b): a REDACTED support bundle — the one file a user can attach to a
// bug report without leaking document or machine content. Built by
// construction from non-sensitive inputs only, with a final scrub pass as
// defense in depth:
//   * app version / build config / platform family — yes.
//   * capability disclosure (id + status + whyNot/alternative) — yes; probe
//     `detail` is EXCLUDED (it can carry absolute paths).
//   * machine policy STATE (status line + managed key NAMES + enforcement
//     wording) — yes; policy VALUES are excluded (an internal TSA hostname
//     is the admin's business, not the bundle's).
//   * feature toggles from an explicit settings allowlist — yes; URL-valued
//     settings (signing/tsaUrl, ai/ollamaEndpoint) are excluded and the
//     exclusion itself is disclosed in the bundle.
//   * recents / documents — COUNTS only, never names or paths.
//   * network features — on/off states only, no history, no destinations.
//   * NO PDF content, NO document metadata, NO network history, NO raw
//     paths — any path-shaped string that sneaks into a value has its
//     username segment redacted (C:/Users/<name>/ → C:/Users/<redacted>/).
struct SupportBundleInput {
    // Optional: the capability registry to disclose. Null → the bundle says
    // so honestly instead of inventing an empty list.
    gp::CapabilityRegistry* capabilities = nullptr;
    // Count of open documents (counts only — never identities).
    int openDocumentCount = 0;
};

class SupportBundle {
public:
    // Builds the bundle reading ONLY the passed settings (tests pass a
    // hermetic store; production passes the application QSettings). Pure:
    // no network calls, no engine work.
    static QJsonObject buildFromSettings(QSettings& user,
                                         const SupportBundleInput& in = {});

    // Indented UTF-8 JSON + trailing newline — the exact bytes to write.
    static QByteArray serialize(const QJsonObject& bundle);

    // Defense-in-depth helper: strips the username segment from path-shaped
    // values (Users|home on any separator); non-path strings pass through.
    static QString redactPathString(const QString& value);
};

} // namespace gp
