// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/ISecretStore.h"

#include <QByteArray>
#include <QString>

// AR-10 D4 — explicitly-labelled encrypted-file fallback secret store.
//
// Used when no OS keystore is available (non-Windows, or a Windows machine
// whose Credential Manager rejected the write). The store is "labelled" in two
// senses: backend() returns Backend::EncryptedFile so the UI can tell the user
// the secret lives in an app-managed file, not the OS vault; and the on-disk
// file carries an explicit header marker.
//
// EC04 — at-rest formats (the first byte of every blob; entries live as
// base64 inside the labelled JSON store `secrets.enc.json`):
//
//   0x02  Windows default path: the secret is wrapped DIRECTLY by DPAPI
//         (CryptProtectData/CryptUnprotectData, advapi32). DPAPI binds the
//         blob to the Windows user account (+ machine); no app-managed key
//         exists. Pre-fix this path re-derived an AES key from a fresh DPAPI
//         blob on every call — DPAPI protection is non-deterministic, so the
//         store could never reread its own writes (storeSecret always failed
//         its verification).
//   0x01  AES-256-GCM under SHA-256(injected key material) — used by tests
//         and hosts that supply explicit key material; and (legacy, pre-EC04)
//         by the no-override path on every platform.
//
// Migration: Windows default-path 0x01 data is NOT recoverable — it was never
// readable (every such write failed its own verification and storeSecret
// returned false), so there is nothing to migrate; 0x02 replaces it. 0x01
// stores written WITH an override key, and non-Windows 0x01 stores, remain
// readable as before. Non-Windows 0x01 keys derive from home-path/machine
// identifiers — those are identifiers, not confidential entropy, so the
// non-Windows default path is honest obfuscation only (no OS protection
// primitive exists there); the Windows default path carries the real
// per-user encryption guarantee.
//
// It NEVER silent-fails: storeSecret returns false (and writes nothing) if it
// cannot durably persist; it never returns true without the ciphertext hitting
// disk and being re-readable. DPAPI protect/unprotect failures are explicit:
// a warning is logged and the operation fails — readSecret returns empty, it
// never decrypts to garbage.
class EncryptedFileSecretStore : public ISecretStore {
public:
    // `filePath` is the JSON store location. If empty, a per-user default under
    // QStandardPaths AppDataLocation is used. `keyMaterial`, if non-empty,
    // overrides the derived key (used by tests to exercise the crypto path
    // deterministically without touching machine/user state).
    explicit EncryptedFileSecretStore(QString filePath = {},
                                      QByteArray keyMaterial = {});

    bool storeSecret(const QString& service, const QString& secret) override;
    QString readSecret(const QString& service) const override;
    bool deleteSecret(const QString& service) override;
    bool hasSecret(const QString& service) const override;
    Backend backend() const override { return Backend::EncryptedFile; }

    // Path of the backing file actually in use (resolved default if none was
    // supplied). Exposed for diagnostics and tests.
    QString filePath() const { return m_filePath; }

private:
    // Legacy 0x01 key derivation: SHA-256 of the override material, or — for
    // stores written without one — SHA-256 of the per-user seed (see the EC04
    // note above; on Windows the default path no longer WRITES this format).
    QByteArray resolveKey() const;

    // AES-256-GCM. Returns empty on failure (caller treats empty as failure and
    // never persists / never claims success).
    QByteArray encrypt(const QByteArray& plaintext) const;
    QByteArray decrypt(const QByteArray& blob) const;  // empty on auth failure

    QString    m_filePath;
    QByteArray m_keyOverride;  // test-injected key material (optional)
};
