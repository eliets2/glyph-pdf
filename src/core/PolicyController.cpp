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

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#endif

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

#ifdef Q_OS_WIN
// W1-05 structural close — disclosed test seam: when set (tests exercise
// the ENFORCEMENT WIRING under fixtures this standard-user process wrote),
// the ownership gate below is skipped. Production installs never set it,
// and it is no wider than the existing GLYPHPDF_POLICY_PATH seam: whoever
// controls the process environment already controls the policy path.
bool assumeTrustedSeamActive()
{
    return !qEnvironmentVariable("GLYPHPDF_POLICY_ASSUME_TRUSTED").isEmpty();
}

// The gate: a policy file may drive machine-wide overrides only when its
// Windows owner is an administrator-tier account (BUILTIN\Administrators or
// LOCAL SYSTEM — what an elevated installer/script produces). A file owned
// by any other account is the planted-squatter posture (any standard user
// can pre-create the %PROGRAMDATA% location on a default install) and is
// refused. Fail-closed: an owner we cannot determine is NOT admin-tier.
bool fileOwnerIsAdminTier(const QString& path)
{
    PSID ownerSid = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    const DWORD rc = ::GetNamedSecurityInfoW(
        reinterpret_cast<const wchar_t*>(path.utf16()), SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION, &ownerSid, nullptr, nullptr, nullptr,
        &sd);
    if (rc != ERROR_SUCCESS) {
        if (sd)
            ::LocalFree(sd);
        return false;
    }
    bool trusted = false;
    LPWSTR sidString = nullptr;
    if (::ConvertSidToStringSidW(ownerSid, &sidString) && sidString) {
        // S-1-5-32-544 = BUILTIN\Administrators, S-1-5-18 = LOCAL SYSTEM.
        trusted = ::lstrcmpiW(sidString, L"S-1-5-32-544") == 0
                  || ::lstrcmpiW(sidString, L"S-1-5-18") == 0;
        ::LocalFree(sidString);
    }
    ::LocalFree(sd);
    return trusted;
}
#endif // Q_OS_WIN

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
#ifdef Q_OS_WIN
    // W1-05 structural close: the file parses — now verify it may SPEAK for
    // the machine. A non-admin-owned file is the squatter's plant; ignore it
    // and disclose (State::UntrustedOwner, statusLine()). The disclosure-only
    // W1-05 fix made the trust model honest; this gate makes the trust
    // DECISION safe at the ONE load boundary every consumer shares.
    if (!assumeTrustedSeamActive() && !fileOwnerIsAdminTier(path)) {
        m_state = State::UntrustedOwner;
        return false;
    }
#endif
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
        case State::UntrustedOwner: {
            // W1-05 structural close: present but not admin-owned — ignored
            // AND disclosed (the gate lives in load()). The trust-model note
            // travels with the refusal: every policy-status render explains
            // the admin-ownership rule and the platform difference.
            QString line = tr("Machine policy at %1 was not written by an "
                              "administrator-tier account and was IGNORED — "
                              "your own preferences remain in force.")
                               .arg(m_path);
            line += QLatin1Char(' ') + trustModelNote();
            return line;
        }
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
            // W1-05/F1: disclose the trust model right where the overrides
            // render — the file is enforced without origin/ownership checks,
            // so the disclosure travels with every claim of enforcement.
            line += QLatin1Char(' ') + trustModelNote();
            return line;
        }
    }
    return {};
}

QString PolicyController::policyPath() const { return m_path; }

QStringList PolicyController::knownKeys() { return knownKeysImpl(); }

QString PolicyController::trustModelNote()
{
    // W1-05 structural close: the one honest sentence about the machine-policy
    // trust model. On Windows load() VERIFIES the file's owner is an
    // administrator-tier account (Administrators/SYSTEM) — anything else is
    // ignored and disclosed (State::UntrustedOwner). On platforms without an
    // ownership check the file remains machine-trusted, and that residual is
    // said plainly. Disclosed wherever policy overrides render and in the
    // support bundle.
    return tr("Machine policy is enforced only when the policy file is "
              "owned by an administrator-tier account (Windows: "
              "Administrators or SYSTEM); files written by anyone else are "
              "ignored. Where no ownership check applies, the policy file "
              "is machine-trusted: keep this machine's user accounts "
              "trustworthy.");
}

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
