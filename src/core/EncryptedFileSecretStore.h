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
// EC04 / SEP13:5 — at-rest formats (the first byte of every blob; entries
// live as base64 inside the labelled JSON store `secrets.enc.json`):
//
//   0x04  Windows default path (v3 generation): the secret is wrapped
//         DIRECTLY by DPAPI (CryptProtectData/CryptUnprotectData, advapi32)
//         with the SERVICE NAME as the optional entropy. The blob is bound
//         to the entry identity it was written under: swapping base64 blobs
//         between JSON entries fails unprotection loudly (SEP13:5 — the v2
//         format below used a constant description and no entropy, so a
//         local actor with write access to the store could swap blobs
//         between entries undetected). DPAPI binds the blob to the Windows
//         user account (+ machine); no app-managed key exists.
//   0x03  AES-256-GCM (v3 generation) under SHA-256(injected key material) —
//         used by tests and hosts that supply explicit key material — or
//         under the non-Windows derived key (see resolveKey()), with the
//         SERVICE NAME as the GCM AAD. Ciphertext+tag authenticate the entry
//         identity: a blob moved to another entry fails authentication
//         instead of decrypting to the wrong secret (SEP13:5).
//   0x02  (legacy, readable for migration) Windows DPAPI WITHOUT entry
//         entropy — EC04's format. Still readable; no longer written.
//         PGR-20 migration: the LAST format with no entry binding — every
//         successful read RE-WRAPS the entry as v3 (0x04/0x03, entry-bound),
//         closing the swappable-legacy window after one read. Follow-up:
//         hard-reject 0x02 once that window has passed.
//   0x01  (legacy, readable for migration) AES-256-GCM WITHOUT AAD — the
//         pre-SEP13:5 override-key and non-Windows format. Still readable;
//         no longer written. PGR-20: on the WINDOWS DEFAULT PATH (no key
//         override) 0x01 and 0x03 are REJECTED — no legitimate blob there
//         ever used the identifier-derived key (pre-EC04 writes were never
//         rereadable), so acceptance would only serve forged entries.
//
// Migration: Windows default-path 0x01 data is NOT recoverable — it was never
// readable (every such write failed its own verification and storeSecret
// returned false), so there was nothing to migrate (EC04) — and per PGR-20
// forged 0x01/0x03 blobs planted there are now rejected outright. 0x01 stores
// written WITH an override key and non-Windows 0x01 stores remain readable as
// before; Windows 0x02 stores read once and are transparently upgraded to the
// entry-bound v3 generation on that read. New writes use the v3 generation
// (0x03/0x04), which binds every blob to its entry identity. Non-Windows
// 0x01/0x03 keys derive from home-path/machine identifiers — those are
// identifiers, not confidential entropy, so the non-Windows default path is
// honest obfuscation only (no OS protection primitive exists there); the
// Windows default path carries the real per-user encryption guarantee, now
// additionally bound to the entry identity.
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

    // AES-256-GCM (v3, 0x03) or DPAPI (v3, 0x04) — the entry identity is
    // part of the protection: the service name is the GCM AAD / the DPAPI
    // optional entropy. Returns empty on failure (caller treats empty as
    // failure and never persists / never claims success).
    QByteArray encrypt(const QString& service, const QByteArray& plaintext) const;
    // Empty on auth failure, entry-identity mismatch (a blob read under a
    // service it was not written for), corruption, or a legacy blob whose
    // key material is unavailable.
    QByteArray decrypt(const QString& service, const QByteArray& blob) const;

    QString    m_filePath;
    QByteArray m_keyOverride;  // test-injected key material (optional)
};
