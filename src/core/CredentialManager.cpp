// SPDX-License-Identifier: Apache-2.0
#include "CredentialManager.h"
#include "core/EncryptedFileSecretStore.h"
#include <QDebug>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#endif

// AR-10 D4 — secret storage is now real on every platform and never silent-fails.
//
// Primary backend (Windows): the OS Credential Manager (DPAPI-backed vault).
// Fallback (non-Windows, or a Windows write failure): an explicitly-labelled
// AES-256-GCM EncryptedFileSecretStore that actually persists the key. The old
// behaviour — `return false` off-Windows, silently dropping the user's API key
// — is gone: a key the user enters is either durably stored or the failure is
// reported, never both-silent-and-lost.

namespace {
QString targetFor(const QString& service) {
    return QStringLiteral("GlyphPDF.AI.") + service;
}

#ifdef _WIN32
// Attempt the OS Credential Manager. Returns true only on a confirmed write.
bool credStore(const QString& service, const QString& secret) {
    const auto target = targetFor(service);
    const auto targetW = target.toStdWString();
    const auto secretBytes = secret.toUtf8();
    CREDENTIALW cred = {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<LPWSTR>(targetW.c_str());
    cred.CredentialBlobSize = static_cast<DWORD>(secretBytes.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secretBytes.constData()));
    cred.Persist = CRED_PERSIST_ENTERPRISE;
    if (!CredWriteW(&cred, 0)) {
        qWarning() << "CredentialManager: Credential Manager write failed for" << service
                   << "error" << GetLastError() << "- falling back to encrypted file";
        return false;
    }
    return true;
}

QString credRead(const QString& service) {
    const auto targetW = targetFor(service).toStdWString();
    PCREDENTIALW pcred = nullptr;
    if (!CredReadW(targetW.c_str(), CRED_TYPE_GENERIC, 0, &pcred) || !pcred) {
        return {};
    }
    QByteArray bytes(reinterpret_cast<const char*>(pcred->CredentialBlob),
                     static_cast<int>(pcred->CredentialBlobSize));
    CredFree(pcred);
    return QString::fromUtf8(bytes);
}

bool credDelete(const QString& service) {
    const auto targetW = targetFor(service).toStdWString();
    return CredDeleteW(targetW.c_str(), CRED_TYPE_GENERIC, 0);
}
#endif // _WIN32

// Process-wide fallback store (encrypted file). Lazily constructed.
EncryptedFileSecretStore& fallbackStore() {
    static EncryptedFileSecretStore store;
    return store;
}
} // namespace

CredentialManager::CredentialManager(QObject* parent) : QObject(parent) {}

bool CredentialManager::storeKey(const QString& service, const QString& secret) {
    if (service.isEmpty() || secret.isEmpty()) return false;
#ifdef _WIN32
    if (credStore(service, secret))
        return true;
    // Credential Manager rejected the write — fall back rather than drop the key.
#endif
    const bool ok = fallbackStore().storeSecret(service, secret);
    if (!ok)
        qWarning() << "CredentialManager: could not store key for" << service
                   << "in any backend";
    return ok;
}

QString CredentialManager::readKey(const QString& service) const {
    if (service.isEmpty()) return {};
#ifdef _WIN32
    const QString fromVault = credRead(service);
    if (!fromVault.isEmpty())
        return fromVault;
#endif
    return fallbackStore().readSecret(service);
}

bool CredentialManager::deleteKey(const QString& service) {
    if (service.isEmpty()) return false;
    bool any = false;
#ifdef _WIN32
    if (credDelete(service)) any = true;
#endif
    // Always also clear the fallback so a key cannot linger in one backend.
    if (fallbackStore().deleteSecret(service)) any = true;
    return any;
}

bool CredentialManager::hasKey(const QString& service) const {
    return !readKey(service).isEmpty();
}

bool CredentialManager::isPlausibleKey(const QString& service, const QString& key) {
    if (key.length() < 20) return false;
    if (service == QStringLiteral("Anthropic")) {
        return key.startsWith(QStringLiteral("sk-ant-"));
    }
    if (service == QStringLiteral("OpenAI")) {
        return key.startsWith(QStringLiteral("sk-"));
    }
    return true; // Permissive for future providers
}
