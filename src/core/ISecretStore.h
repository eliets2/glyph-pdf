// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

// AR-10 D4 — platform secret storage abstraction.
//
// The audited CredentialManager no-op'd off Windows (returned false and
// silently DROPPED the API key), so a user on a non-Windows build, or on a
// Windows machine where the Credential Manager write failed, could type a key,
// see no error, and have it vanish. ISecretStore makes storage real on every
// platform and, critically, NEVER silent-fails: every store/delete returns a
// definite success/failure and the backend in use is queryable.
//
// Backends:
//   * Windows Credential Manager (primary on Windows) — see CredentialManager.
//   * EncryptedFileSecretStore — an explicitly-labelled, AES-256-GCM encrypted
//     file fallback used when no OS keystore is available. It is labelled (the
//     user can see secrets are in an app-managed encrypted file, not the OS
//     vault) and it actually persists — it never reports success without
//     writing the bytes, and never reports failure while leaving a partial.
class ISecretStore {
public:
    // Which concrete backend is providing storage. Surfaced so callers (and the
    // UI) can tell the user where their secret actually lives and so a fallback
    // is never silent.
    enum class Backend {
        WindowsCredentialManager,  // OS-managed vault (DPAPI-backed)
        EncryptedFile,             // app-managed AES-256-GCM encrypted file
    };

    virtual ~ISecretStore() = default;

    // Persist `secret` under `service`. Returns true ONLY if the secret was
    // durably stored; returns false (never silently swallows) otherwise.
    virtual bool storeSecret(const QString& service, const QString& secret) = 0;

    // Read a previously-stored secret. Empty string if absent.
    virtual QString readSecret(const QString& service) const = 0;

    // Remove a stored secret. Returns true if it is gone afterwards (including
    // the already-absent case); false only on a real removal failure.
    virtual bool deleteSecret(const QString& service) = 0;

    virtual bool hasSecret(const QString& service) const = 0;

    // The backend actually in use, so the caller can label the storage location
    // honestly and never present a silent fallback as the OS vault.
    virtual Backend backend() const = 0;
};
