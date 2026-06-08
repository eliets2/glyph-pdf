// SPDX-License-Identifier: Apache-2.0
#include "CredentialManager.h"
#include <QDebug>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#endif

namespace {
QString targetFor(const QString& service) {
    return QStringLiteral("GlyphPDF.AI.") + service;
}
}

CredentialManager::CredentialManager(QObject* parent) : QObject(parent) {}

bool CredentialManager::storeKey(const QString& service, const QString& secret) {
    if (service.isEmpty() || secret.isEmpty()) return false;
#ifdef _WIN32
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
        qWarning() << "CredentialManager::storeKey CredWriteW failed for" << service << "error" << GetLastError();
        return false;
    }
    return true;
#else
    Q_UNUSED(service);
    Q_UNUSED(secret);
    qWarning() << "CredentialManager: storeKey not implemented on non-Windows platforms";
    return false;
#endif
}

QString CredentialManager::readKey(const QString& service) const {
    if (service.isEmpty()) return {};
#ifdef _WIN32
    const auto target = targetFor(service);
    const auto targetW = target.toStdWString();
    PCREDENTIALW pcred = nullptr;
    if (!CredReadW(targetW.c_str(), CRED_TYPE_GENERIC, 0, &pcred) || !pcred) {
        return {};
    }
    QByteArray bytes(reinterpret_cast<const char*>(pcred->CredentialBlob),
                     static_cast<int>(pcred->CredentialBlobSize));
    CredFree(pcred);
    return QString::fromUtf8(bytes);
#else
    Q_UNUSED(service);
    return {};
#endif
}

bool CredentialManager::deleteKey(const QString& service) {
    if (service.isEmpty()) return false;
#ifdef _WIN32
    const auto target = targetFor(service);
    return CredDeleteW(target.toStdWString().c_str(), CRED_TYPE_GENERIC, 0);
#else
    Q_UNUSED(service);
    return false;
#endif
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
