// SPDX-License-Identifier: Apache-2.0
// R24(b) — the redacted support bundle. See SupportBundle.h for the honesty
// contract. Redaction is BY CONSTRUCTION: sections are built only from
// allowlisted, non-sensitive inputs; a final scrub pass is defense in depth
// for any path-shaped value that a future section might accidentally echo.
#include "core/SupportBundle.h"

#include "core/Capability.h"
#include "core/PolicyController.h"
#include "core/UpdateChecker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSettings>
#include <QSysInfo>

namespace gp {
namespace {

// SupportBundle is not a QObject; translate through the application context.
inline QString bundleTr(const char* sourceText)
{
    return QCoreApplication::translate("SupportBundle", sourceText);
}

// The settings the bundle MAY carry: feature toggles and non-identifying
// selections only. Anything URL-shaped is deliberately absent.
QStringList bundleSettingKeys()
{
    static const QStringList s_keys = {
        QStringLiteral("update/checkOnStartup"),
        QStringLiteral("update/channel"),
        QStringLiteral("recent/autoPrune"),
        QStringLiteral("autosave/intervalSeconds"),
        QStringLiteral("ocr/engine"),
        QStringLiteral("ocr/allowNetworkDownload"),
        QStringLiteral("signing/padesLevel"),
        QStringLiteral("ai/ollamaModel"),
        QStringLiteral("ui/language"),
        QStringLiteral("ui/theme"),
    };
    return s_keys;
}

QString capIdName(CapId id)
{
    switch (id) {
        case CapId::OfficeImport:             return QStringLiteral("OfficeImport");
        case CapId::WordExport:               return QStringLiteral("WordExport");
        case CapId::ExcelExport:              return QStringLiteral("ExcelExport");
        case CapId::PptExport:                return QStringLiteral("PptExport");
        case CapId::CsvExport:                return QStringLiteral("CsvExport");
        case CapId::HtmlExport:               return QStringLiteral("HtmlExport");
        case CapId::TextExport:               return QStringLiteral("TextExport");
        case CapId::ImageExport:              return QStringLiteral("ImageExport");
        case CapId::PdfAExport:               return QStringLiteral("PdfAExport");
        case CapId::Linearize:                return QStringLiteral("Linearize");
        case CapId::OcrTesseract:             return QStringLiteral("OcrTesseract");
        case CapId::OcrRapidModels:           return QStringLiteral("OcrRapidModels");
        case CapId::OcrEnsemble:              return QStringLiteral("OcrEnsemble");
        case CapId::OcrLanguageData:          return QStringLiteral("OcrLanguageData");
        case CapId::CompressSubsetFonts:      return QStringLiteral("CompressSubsetFonts");
        case CapId::CompressRemoveUnused:     return QStringLiteral("CompressRemoveUnused");
        case CapId::MrcCompression:           return QStringLiteral("MrcCompression");
        case CapId::DigitalSignature:         return QStringLiteral("DigitalSignature");
        case CapId::VisibleSignatureGraphic:  return QStringLiteral("VisibleSignatureGraphic");
        case CapId::PdfAValidation:           return QStringLiteral("PdfAValidation");
        case CapId::FormJavaScript:           return QStringLiteral("FormJavaScript");
        case CapId::XfaForms:                 return QStringLiteral("XfaForms");
        case CapId::COUNT:                    break;
    }
    return QStringLiteral("Unknown(%1)").arg(int(id));
}

QString availabilityName(Availability a)
{
    switch (a) {
        case Availability::Available:          return QStringLiteral("available");
        case Availability::Degraded:           return QStringLiteral("degraded");
        case Availability::UnavailableBuild:   return QStringLiteral("unavailable-build");
        case Availability::UnavailableRuntime: return QStringLiteral("unavailable-runtime");
    }
    return QStringLiteral("unknown");
}

// ── Defense in depth: the final scrub pass ──────────────────────────────────
QJsonValue scrubValue(const QJsonValue& v);

QJsonObject scrubObject(const QJsonObject& obj)
{
    QJsonObject out;
    for (auto it = obj.begin(); it != obj.end(); ++it)
        out.insert(it.key(), scrubValue(it.value()));
    return out;
}

QJsonArray scrubArray(const QJsonArray& arr)
{
    QJsonArray out;
    for (const QJsonValue& v : arr)
        out.append(scrubValue(v));
    return out;
}

QJsonValue scrubValue(const QJsonValue& v)
{
    if (v.isString())
        return SupportBundle::redactPathString(v.toString());
    if (v.isObject())
        return scrubObject(v.toObject());
    if (v.isArray())
        return scrubArray(v.toArray());
    return v;
}

} // namespace

QString SupportBundle::redactPathString(const QString& value)
{
    // Username-bearing path segments on any platform spelling:
    //   C:/Users/<name>/…  C:\Users\<name>\…  /home/<name>/…  /Users/<name>/…
    // Raw literal: [/\\] = slash or backslash; the username run stops at a
    // separator, a quote or end of line.
    static const QRegularExpression re(
        QStringLiteral(R"rx(((?:[A-Za-z]:)?[/\\](?:Users|home)[/\\])(?:[^"/\\\r\n]+))rx"),
        QRegularExpression::CaseInsensitiveOption);
    QString out = value;
    out.replace(re, QStringLiteral("\\1<redacted>"));
    return out;
}

QJsonObject SupportBundle::buildFromSettings(QSettings& user,
                                             const SupportBundleInput& in)
{
    QJsonObject bundle;
    bundle.insert(QStringLiteral("schemaVersion"), 1);
    bundle.insert(QStringLiteral("type"),
                  QStringLiteral("glyphpdf-support-bundle"));
    bundle.insert(QStringLiteral("generatedAtUtc"),
                  QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    // Build config / platform family — no serial numbers, no hostnames.
    bundle.insert(QStringLiteral("appVersion"), UpdateChecker::currentVersion());
    bundle.insert(QStringLiteral("qtVersion"), QString::fromLatin1(qVersion()));
    QJsonObject platform;
    platform.insert(QStringLiteral("productType"),
                    QSysInfo::productType());
    platform.insert(QStringLiteral("productVersion"),
                    QSysInfo::productVersion());
    platform.insert(QStringLiteral("buildCpuArchitecture"),
                    QSysInfo::buildCpuArchitecture());
    bundle.insert(QStringLiteral("platform"), platform);

    // ── Settings: the feature-toggle allowlist ───────────────────────────
    QJsonObject settingsSection;
    const QStringList keys = bundleSettingKeys();
    for (const QString& k : keys) {
        if (user.contains(k))
            settingsSection.insert(k, QJsonValue::fromVariant(user.value(k)));
    }
    // URL-valued settings are excluded AND the exclusion is disclosed.
    QJsonObject excluded;
    const QString urlExclusion = bundleTr("URL values are excluded by the bundle "
                                    "redaction policy (never included).");
    excluded.insert(QStringLiteral("signing/tsaUrl"),
                    QJsonObject{{QStringLiteral("reason"), urlExclusion}});
    excluded.insert(QStringLiteral("ai/ollamaEndpoint"),
                    QJsonObject{{QStringLiteral("reason"), urlExclusion}});
    settingsSection.insert(QStringLiteral("excludedByRedaction"), excluded);
    bundle.insert(QStringLiteral("settings"), settingsSection);

    // ── Recents / documents: COUNTS only ─────────────────────────────────
    const QStringList recents =
        user.value(QStringLiteral("recentFiles")).toStringList();
    QJsonObject recentsSection;
    recentsSection.insert(QStringLiteral("count"), recents.size());
    recentsSection.insert(QStringLiteral("disclosure"),
                          bundleTr("Names and paths are never included."));
    bundle.insert(QStringLiteral("recents"), recentsSection);
    QJsonObject docsSection;
    docsSection.insert(QStringLiteral("openCount"), in.openDocumentCount);
    bundle.insert(QStringLiteral("documents"), docsSection);

    // ── Machine policy: STATE and key NAMES, never policy values ────────
    auto& policy = PolicyController::instance();
    policy.ensureLoaded();
    QJsonObject policySection;
    switch (policy.state()) {
        case PolicyController::State::NoPolicy:
            policySection.insert(QStringLiteral("state"),
                                 QStringLiteral("none"));
            break;
        case PolicyController::State::Invalid:
            policySection.insert(QStringLiteral("state"),
                                 QStringLiteral("invalid-ignored"));
            break;
        case PolicyController::State::Loaded:
            policySection.insert(QStringLiteral("state"),
                                 QStringLiteral("loaded"));
            break;
    }
    policySection.insert(QStringLiteral("disclosure"), policy.statusLine());
    QJsonArray managed;
    for (const QString& k : policy.managedKeys()) {
        managed.append(QJsonObject{
            {QStringLiteral("key"), k},
            {QStringLiteral("enforcement"),
             PolicyController::enforcementNote(k)},
        });
    }
    policySection.insert(QStringLiteral("managedKeys"), managed);
    policySection.insert(QStringLiteral("disclosureNote"),
                         bundleTr("Policy values are never included in the bundle."));
    bundle.insert(QStringLiteral("policy"), policySection);

    // ── Capabilities: id + status + whyNot/alternative; detail EXCLUDED ──
    if (in.capabilities) {
        QJsonArray caps;
        for (int i = 0; i < int(CapId::COUNT); ++i) {
            const CapId id = static_cast<CapId>(i);
            const Capability c = in.capabilities->query(id);
            QJsonObject entry;
            entry.insert(QStringLiteral("id"), capIdName(id));
            entry.insert(QStringLiteral("status"), availabilityName(c.status));
            if (c.status != Availability::Available) {
                entry.insert(QStringLiteral("whyNot"), c.whyNot);
                entry.insert(QStringLiteral("alternative"), c.alternative);
            }
            // c.detail deliberately omitted: it can carry absolute paths.
            caps.append(entry);
        }
        bundle.insert(QStringLiteral("capabilities"), caps);
    } else {
        bundle.insert(QStringLiteral("capabilities"),
                      bundleTr("Capability registry unavailable in this context."));
    }

    // ── Network: on/off states only (no history, no destinations) ────────
    QJsonObject network;
    network.insert(QStringLiteral("aiChatOllamaLocal"), true); // on demand, local
    network.insert(QStringLiteral("tsaTimestamping"),
                   !user.value(QStringLiteral("signing/tsaUrl"))
                        .toString()
                        .trimmed()
                        .isEmpty());
    network.insert(QStringLiteral("ocspDuringValidation"), true);
    network.insert(QStringLiteral("updateCheckOnStartup"),
                   user.value(QStringLiteral("update/checkOnStartup"), false).toBool());
    network.insert(QStringLiteral("updateCheckManual"), true);
    network.insert(QStringLiteral("ocrTraineddataDownload"),
                   user.value(QStringLiteral("ocr/allowNetworkDownload"), false).toBool());
    network.insert(QStringLiteral("disclosure"),
                   bundleTr("On/off states only — the bundle never carries network "
                      "history or destinations."));
    bundle.insert(QStringLiteral("network"), network);

    bundle.insert(QStringLiteral("privacyNote"),
                  bundleTr("This bundle contains no PDF content, no document "
                     "metadata, no file paths and no network history."));

    // Defense in depth: scrub every string in the finished object.
    return scrubObject(bundle);
}

QByteArray SupportBundle::serialize(const QJsonObject& bundle)
{
    return QJsonDocument(bundle).toJson(QJsonDocument::Indented) + '\n';
}

} // namespace gp
