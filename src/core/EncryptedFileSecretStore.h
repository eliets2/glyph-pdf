// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/ISecretStore.h"

#include <QByteArray>
#include <QString>

// AR-10 D4 — explicitly-labelled encrypted-file fallback secret store.
//
// Used when no OS keystore is available (non-Windows, or a Windows machine
// whose Credential Manager rejected the write). Secrets are encrypted with
// AES-256-GCM (authenticated) under a key derived from a per-user salt and,
// on Windows, wrapped with DPAPI (CryptProtectData) so the key material at
// rest is bound to the user account. The store is "labelled" in two senses:
//   * backend() returns Backend::EncryptedFile so the UI can tell the user the
//     secret lives in an app-managed file, not the OS vault; and
//   * the on-disk file carries an explicit header marker.
//
// It NEVER silent-fails: storeSecret returns false (and writes nothing) if it
// cannot durably persist; it never returns true without the ciphertext hitting
// disk and being re-readable.
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
    // Derive the 32-byte AES key. On Windows the derived material is additionally
    // DPAPI-protected at rest in the key cache; here we resolve it into memory.
    QByteArray resolveKey() const;

    // AES-256-GCM. Returns empty on failure (caller treats empty as failure and
    // never persists / never claims success).
    QByteArray encrypt(const QByteArray& plaintext) const;
    QByteArray decrypt(const QByteArray& blob) const;  // empty on auth failure

    QString    m_filePath;
    QByteArray m_keyOverride;  // test-injected key material (optional)
};
