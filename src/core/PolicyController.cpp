// SPDX-License-Identifier: Apache-2.0
// R24(a) — machine-level policy: a bounded-allowlist policy file that wins
// over user preferences at load time, with every override disclosed in the
// UI (Preferences "Managed by policy" rows + status line) and in the support
// bundle. See PolicyController.h for the schema and the honesty contract.
#include "core/PolicyController.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>

namespace gp {
namespace {

constexpr int kSchemaVersion = 1;

// The bounded allowlist (4-8 keys): machine-admin stories only. A key joins
// this list together with its enforcement wording; keys NEVER join silently
// — see the honesty contract in PolicyController.h.
QStringList knownKeysImpl()
{
    static const QStringList s_keys = {
        QStringLiteral("signing/tsaUrl"),
        QStringLiteral("signing/padesLevel"),
        QStringLiteral("update/checkOnStartup"),
        QStringLiteral("update/channel"),
        QStringLiteral("ai/ollamaEndpoint"),
        QStringLiteral("ocr/allowNetworkDownload"),
    };
    return s_keys;
}

bool acceptedValueType(const QJsonValue& v)
{
    return v.isString() || v.isBool() || v.isDouble();
}

} // namespace

PolicyController::PolicyController(QObject* parent) : QObject(parent) {}

PolicyController& PolicyController::instance()
{
    static PolicyController s_instance;
    return s_instance;
}

QString PolicyController::defaultPolicyPath()
{
    // Test seam first: an explicit env override is disclosed via the same
    // status line as any other path (the resolved path is never hidden).
    const QString env = qEnvironmentVariable("GLYPHPDF_POLICY_PATH");
    if (!env.isEmpty())
        return env;
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/GlyphPDF/policy.json");
}

bool PolicyController::load(const QString& path)
{
    // An explicit load always counts as "loaded" — a later ensureLoaded()
    // must never silently swap the consulted file underneath the caller.
    m_loadedOnce = true;

    m_path = path;
    m_managed.clear();
    m_unrecognized.clear();

    QFile f(path);
    if (!f.exists()) {
        m_state = State::NoPolicy;
        return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        // Unreadable is as honest as tampered: ignored AND disclosed.
        m_state = State::Invalid;
        return false;
    }
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &parseError);
    f.close();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        m_state = State::Invalid;
        return false;
    }
    const QJsonObject root = doc.object();
    if (!root.contains(QStringLiteral("schemaVersion"))
        || root.value(QStringLiteral("schemaVersion")).toInt() != kSchemaVersion) {
        m_state = State::Invalid;
        return false;
    }
    if (!root.value(QStringLiteral("settings")).isObject()) {
        m_state = State::Invalid;
        return false;
    }
    const QJsonObject settings =
        root.value(QStringLiteral("settings")).toObject();
    const QStringList known = knownKeysImpl();
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        if (!known.contains(it.key()) || !acceptedValueType(it.value())) {
            // Ignored AND disclosed — the policy surface stays bounded.
            m_unrecognized.append(it.key());
            continue;
        }
        m_managed.insert(it.key(), it.value().toVariant());
    }
    m_state = State::Loaded;
    return true;
}

void PolicyController::ensureLoaded()
{
    if (m_loadedOnce)
        return;
    load(defaultPolicyPath());
}

PolicyController::State PolicyController::state() const { return m_state; }

QString PolicyController::statusLine() const
{
    switch (m_state) {
        case State::NoPolicy:
            return tr("No machine policy found (checked %1). All settings "
                      "follow your preferences.")
                .arg(m_path.isEmpty() ? defaultPolicyPath() : m_path);
        case State::Invalid:
            return tr("Machine policy at %1 is invalid and was IGNORED — "
                      "your own preferences remain in force.")
                .arg(m_path);
        case State::Loaded: {
            QStringList parts;
            for (auto it = m_managed.cbegin(); it != m_managed.cend(); ++it)
                parts.append(QStringLiteral("%1 (%2)")
                                 .arg(it.key(),
                                      isEnforcedKey(it.key())
                                          ? tr("enforced")
                                          : tr("enforcement pending")));
            QString line = tr("Machine policy loaded from %1 — %2 key(s) "
                              "override your preferences: %3.")
                               .arg(m_path)
                               .arg(m_managed.size())
                               .arg(parts.isEmpty()
                                        ? QStringLiteral("none")
                                        : parts.join(QStringLiteral(", ")));
            if (!m_unrecognized.isEmpty())
                line += tr(" Ignored (outside the audited allowlist): %1.")
                            .arg(m_unrecognized.join(QStringLiteral(", ")));
            return line;
        }
    }
    return {};
}

QString PolicyController::policyPath() const { return m_path; }

QStringList PolicyController::knownKeys() { return knownKeysImpl(); }

bool PolicyController::isEnforcedKey(const QString& settingsKey)
{
    // Enforcement wired in THIS build (R24 wiring closure — each key is
    // pinned at its OBSERVABLE enforcement point by TestPolicyWiring):
    //   * signing pair → SecurityController::readSigningConfig (the ONE
    //     production reader of the signing settings) applies the policy
    //     before every sign/certify/timestamp dispatch;
    //   * update pair → MainWindow::startupUpdateCheckEnabled /
    //     startupUpdateChannel (consulted by initUpdateChecker);
    //   * ai/ollamaEndpoint → OllamaProvider::resolveEndpoint (the ONE
    //     endpoint gate feeding isReady/chat);
    //   * ocr/allowNetworkDownload → the OcrEngine model-load gate.
    return knownKeysImpl().contains(settingsKey);
}

QString PolicyController::enforcementNote(const QString& settingsKey)
{
    if (settingsKey == QLatin1String("signing/tsaUrl"))
        return tr("Enforced app-wide: every sign/certify/timestamp dispatch "
                  "uses this TSA URL.");
    if (settingsKey == QLatin1String("signing/padesLevel"))
        return tr("Enforced app-wide: every sign/certify dispatch uses this "
                  "PAdES level.");
    if (settingsKey == QLatin1String("update/checkOnStartup"))
        return tr("Enforced app-wide: the startup update check runs only "
                  "when this key allows it (MainWindow::initUpdateChecker "
                  "consults the effective value).");
    if (settingsKey == QLatin1String("update/channel"))
        return tr("Enforced app-wide: the startup update check uses this "
                  "channel's manifest (MainWindow::initUpdateChecker).");
    if (settingsKey == QLatin1String("ai/ollamaEndpoint"))
        return tr("Enforced app-wide: every AI chat request and probe "
                  "resolves its endpoint through this key "
                  "(OllamaProvider::resolveEndpoint); an empty policy value "
                  "disables AI chat.");
    if (settingsKey == QLatin1String("ocr/allowNetworkDownload"))
        return tr("Enforced app-wide: OCR language-pack downloads are "
                  "attempted only when this key allows it (the OcrEngine "
                  "model-load gate).");
    return {};
}

bool PolicyController::isManaged(const QString& settingsKey) const
{
    return m_managed.contains(settingsKey);
}

QVariant PolicyController::policyValue(const QString& settingsKey) const
{
    return m_managed.value(settingsKey);
}

QVariant PolicyController::effectiveValue(const QString& settingsKey,
                                          const QVariant& userValue) const
{
    return m_managed.contains(settingsKey) ? m_managed.value(settingsKey)
                                           : userValue;
}

QStringList PolicyController::managedKeys() const
{
    QStringList keys;
    keys.reserve(m_managed.size());
    for (auto it = m_managed.cbegin(); it != m_managed.cend(); ++it)
        keys.append(it.key());
    keys.sort();
    return keys;
}

QStringList PolicyController::unrecognizedKeys() const
{
    return m_unrecognized;
}

void PolicyController::resetForTesting()
{
    m_state = State::NoPolicy;
    m_path.clear();
    m_managed.clear();
    m_unrecognized.clear();
    m_loadedOnce = false;
}

} // namespace gp
