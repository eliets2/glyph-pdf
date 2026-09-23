// SPDX-License-Identifier: Apache-2.0
#include "core/EncryptedFileSecretStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QDebug>

#include <openssl/evp.h>
#include <openssl/rand.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>   // DPAPI: CryptProtectData / CryptUnprotectData
#endif

namespace {

constexpr int    kKeyLen   = 32;   // AES-256
constexpr int    kNonceLen = 12;   // GCM standard nonce
constexpr int    kTagLen   = 16;   // GCM tag

// On-disk blob version bytes (first byte of every stored blob).
//
//  0x01 — AES-256-GCM under SHA-256(keyMaterial override), or — legacy, kept
//         for reading pre-EC04 stores and for non-Windows default-path writes —
//         under SHA-256 of the per-user seed described in resolveKey().
//         LEGACY (readable, never written) since SEP13:5.
//         PGR-20: on the WINDOWS DEFAULT PATH (no override) this version is
//         REJECTED — no legitimate Windows default-path blob ever used the
//         identifier-derived key (pre-EC04 writes were never rereadable, EC04),
//         and the seed inputs are public identifiers, so any such blob is a
//         forgery (secret injection).
//  0x02 — Windows DPAPI (CryptProtectData) wrapped secret. EC04: the DEFAULT
//         Windows path stored this format, because DPAPI protection is
//         intentionally non-deterministic: deriving an AES key by re-protecting
//         on every read (the pre-fix resolveKey()) produced a DIFFERENT key at
//         decrypt time than at encrypt time, so the store could never reread
//         its own ciphertext. Key naming/ownership: blobs are bound to the
//         Windows user account (+machine) by DPAPI itself — no app-managed key
//         material exists or is persisted; the description string
//         "GlyphPDF.SecretStore.Secret.v2" labels the blobs.
//         LEGACY (readable, never written) since SEP13:5.
//         PGR-20 migration: this is the LAST format with no entry binding, so
//         a legacy blob is swappable between entries while it exists. Every
//         successful read therefore RE-WRAPS the entry as v3 (0x04 default /
//         0x03 override — entry-bound), closing the unbound window after one
//         read. Follow-up (documented): hard-reject 0x02 once the migration
//         window has passed — after one read per store, 0x02 acceptance only
//         ever serves a forged/planted legacy blob.
//  0x03 — SEP13:5 v3 generation: AES-256-GCM with the SERVICE NAME as the GCM
//         AAD. Ciphertext+tag authenticate the entry identity, so a blob moved
//         to another JSON entry fails authentication instead of decrypting to
//         the wrong secret. PGR-20: REJECTED on the Windows default path (no
//         override) for the same reason as 0x01 — never legitimately produced
//         there, so acceptance would be a forgery hole.
//  0x04 — SEP13:5 v3 generation, Windows default path: DPAPI wrapped with the
//         SERVICE NAME as the optional entropy (description string
//         "GlyphPDF.SecretStore.Secret.v3"). The blob is bound to the entry
//         identity it was written under — cross-entry blob swaps fail
//         unprotection loudly (the v2 format's constant description + missing
//         entropy let a local actor with store write-access swap blobs
//         between entries undetected).
constexpr quint8 kVersionAes   = 0x01;  // legacy — never written; rejected on
                                        // the Windows default path (PGR-20)
constexpr quint8 kVersionDpapi = 0x02;  // legacy — never written; re-wrapped
                                        // to v3 on first read (PGR-20)
constexpr quint8 kVersionAesAad   = 0x03;
constexpr quint8 kVersionDpapiV3  = 0x04;
const wchar_t kDpapiV3Description[] = L"GlyphPDF.SecretStore.Secret.v3";

// Explicit on-disk marker so the file is self-describing / labelled.
const QString kMarker = QStringLiteral("glyphpdf-encrypted-secret-store");

// PGR-25: every mutation of the store is a read-modify-write of the shared
// JSON. All writers serialize behind a lock file beside the store, with a
// bounded wait — a lock that waits forever would hang the UI, so a timeout is
// a LOUD failure, never a silent lost update.
constexpr int kStoreLockTimeoutMs = 30000;

QString storeLockPath(const QString& filePath)
{
    return filePath + QStringLiteral(".lock");
}

QString defaultStorePath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.glyphpdf");
    return base + QStringLiteral("/secrets.enc.json");
}

} // namespace

EncryptedFileSecretStore::EncryptedFileSecretStore(QString filePath, QByteArray keyMaterial)
    : m_filePath(filePath.isEmpty() ? defaultStorePath() : std::move(filePath))
    , m_keyOverride(std::move(keyMaterial))
{
}

QByteArray EncryptedFileSecretStore::resolveKey() const
{
    if (!m_keyOverride.isEmpty()) {
        // Test/host-injected material — hash to a fixed-length key.
        return QCryptographicHash::hash(m_keyOverride, QCryptographicHash::Sha256);
    }

    // Legacy derivation kept ONLY to read pre-EC04 (0x01) stores written
    // without an override (non-Windows). EC04: on Windows the default path no
    // longer writes through this derivation at all — a fresh DPAPI blob is not
    // a deterministic key-derivation function, so hashing it per call made
    // every write unreadable. SEP13:5: the no-override non-Windows path now
    // writes 0x03 (same key derivation, plus the entry identity as AAD); the
    // derivation itself is unchanged so legacy stores stay readable.
    // The seed components below are account/machine IDENTIFIERS, not
    // confidential entropy: on non-Windows the derivation is best-effort
    // obfuscation only, documented as such in the header.
    // PGR-20: precisely BECAUSE the seed is public, decrypt() on the Windows
    // default path (no override) rejects the AES versions this derivation
    // would unlock — there, a blob under this key can only be a forgery.
    QByteArray seed;
    seed += QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toUtf8();
    seed += QSysInfo::machineUniqueId();
    seed += QByteArrayLiteral("glyphpdf-secret-store-v1");

    return QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
}

QByteArray EncryptedFileSecretStore::encrypt(const QString& service,
                                             const QByteArray& plaintext) const
{
    // SEP13:5: the entry identity travels WITH the protection — the service
    // name authenticates the blob (GCM AAD / DPAPI entropy), so a ciphertext
    // is only recoverable under the entry it was written for.
    const QByteArray identity = service.toUtf8();
#ifdef _WIN32
    // EC04: default path — protect the secret DIRECTLY with the platform
    // primitive. DPAPI wraps the plaintext under the user's account key; the
    // unwrapped secret is recoverable by the same user (any process, any
    // instance, any later session) and by nobody else. No derived AES key, no
    // persisted master key. SEP13:5 v3: the service name rides as the
    // optional entropy, binding the blob to its JSON entry.
    if (m_keyOverride.isEmpty()) {
        DATA_BLOB entropy{};
        entropy.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(identity.constData()));
        entropy.cbData = static_cast<DWORD>(identity.size());
        DATA_BLOB in{};
        in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.constData()));
        in.cbData = static_cast<DWORD>(plaintext.size());
        DATA_BLOB out{};
        if (!CryptProtectData(&in, kDpapiV3Description, &entropy,
                              nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
            const DWORD err = GetLastError();
            qWarning() << "EncryptedFileSecretStore: CryptProtectData failed"
                       << "(Win32 error" << err << "); secret NOT stored";
            return {};  // explicit failure — the caller never claims success
        }
        QByteArray blob;
        blob.append(static_cast<char>(kVersionDpapiV3));
        blob.append(reinterpret_cast<const char*>(out.pbData),
                    static_cast<int>(out.cbData));
        LocalFree(out.pbData);
        return blob;
    }
#endif

    const QByteArray key = resolveKey();
    if (key.size() != kKeyLen) return {};

    unsigned char nonce[kNonceLen];
    if (RAND_bytes(nonce, kNonceLen) != 1) return {};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    auto guard = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>(
        ctx, EVP_CIPHER_CTX_free);

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) return {};
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr) != 1) return {};
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           nonce) != 1) return {};

    // SEP13:5: authenticate the entry identity — the AAD is not part of the
    // ciphertext but IS covered by the tag.
    int aadLen = 0;
    if (EVP_EncryptUpdate(ctx, nullptr, &aadLen,
                          reinterpret_cast<const unsigned char*>(identity.constData()),
                          identity.size()) != 1) return {};

    QByteArray cipher(plaintext.size(), Qt::Uninitialized);
    int cipherLen = 0;
    if (EVP_EncryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(cipher.data()), &cipherLen,
                          reinterpret_cast<const unsigned char*>(plaintext.constData()),
                          static_cast<int>(plaintext.size())) != 1) return {};
    int finalLen = 0;
    if (EVP_EncryptFinal_ex(ctx,
                            reinterpret_cast<unsigned char*>(cipher.data()) + cipherLen,
                            &finalLen) != 1) return {};
    cipher.resize(cipherLen + finalLen);

    unsigned char tag[kTagLen];
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, kTagLen, tag) != 1) return {};

    QByteArray blob;
    blob.append(static_cast<char>(kVersionAesAad));
    blob.append(reinterpret_cast<const char*>(nonce), kNonceLen);
    blob.append(reinterpret_cast<const char*>(tag), kTagLen);
    blob.append(cipher);
    return blob;
}

QByteArray EncryptedFileSecretStore::decrypt(const QString& service,
                                             const QByteArray& blob) const
{
    if (blob.size() < 1 + kNonceLen + kTagLen) return {};

    const unsigned char* p = reinterpret_cast<const unsigned char*>(blob.constData());
    const quint8 version = static_cast<quint8>(blob.at(0));
    // SEP13:5: the entry identity must match the one the blob was protected
    // under — the service name IS part of the authentication material.
    const QByteArray identity = service.toUtf8();

#ifdef _WIN32
    // SEP13:5 v3 DPAPI blob: unprotection REQUIRES the entry's service name
    // as the optional entropy. A blob swapped between JSON entries (or a
    // corrupted one) fails here — explicitly, never to garbage.
    if (version == kVersionDpapiV3) {
        DATA_BLOB entropy{};
        entropy.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(identity.constData()));
        entropy.cbData = static_cast<DWORD>(identity.size());
        DATA_BLOB in{};
        in.pbData = const_cast<BYTE*>(p + 1);
        in.cbData = static_cast<DWORD>(blob.size() - 1);
        DATA_BLOB out{};
        if (!CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr,
                                CRYPTPROTECT_UI_FORBIDDEN, &out)) {
            const DWORD err = GetLastError();
            qWarning() << "EncryptedFileSecretStore: CryptUnprotectData failed"
                       << "(Win32 error" << err
                       << "); the blob does not belong to this entry, belongs "
                          "to a different user/machine, or is corrupted";
            return {};
        }
        QByteArray plain(reinterpret_cast<const char*>(out.pbData),
                         static_cast<int>(out.cbData));
        LocalFree(out.pbData);
        return plain;
    }
    // EC04 legacy v2 blob — readable for migration (no entry binding; new
    // writes no longer use this format).
    if (version == kVersionDpapi) {
        DATA_BLOB in{};
        in.pbData = const_cast<BYTE*>(p + 1);
        in.cbData = static_cast<DWORD>(blob.size() - 1);
        DATA_BLOB out{};
        if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                                CRYPTPROTECT_UI_FORBIDDEN, &out)) {
            const DWORD err = GetLastError();
            qWarning() << "EncryptedFileSecretStore: CryptUnprotectData failed"
                       << "(Win32 error" << err
                       << "); the stored secret belongs to a different user, "
                          "machine, or is corrupted";
            return {};
        }
        QByteArray plain(reinterpret_cast<const char*>(out.pbData),
                         static_cast<int>(out.cbData));
        LocalFree(out.pbData);
        return plain;
    }
#else
    // A DPAPI blob can only be produced (and read) on Windows; elsewhere it is
    // an explicit failure, not silence.
    if (version == kVersionDpapiV3 || version == kVersionDpapi) {
        qWarning() << "EncryptedFileSecretStore: DPAPI-wrapped secret found on "
                      "a non-Windows platform; cannot decrypt";
        return {};
    }
#endif

    // PGR-20: on the default Windows path (no key override) the AES versions
    // are unreachable by construction — pre-EC04 0x01 writes could never be
    // reread, and SEP13:5+ writes 0x04 only. Their key inputs (home path,
    // machine id, a constant) are public identifiers, so a 0x01/0x03 blob
    // found in a Windows default-path store can only be an INJECTED forgery:
    // reject it loudly instead of serving the attacker's secret. The
    // override-key path (hosts/tests) and the non-Windows default path (which
    // writes 0x03) still read these formats.
#ifdef _WIN32
    if (m_keyOverride.isEmpty()
        && (version == kVersionAes || version == kVersionAesAad)) {
        qWarning() << "EncryptedFileSecretStore: AES blob found on the Windows "
                      "default path — this format cannot be produced "
                      "legitimately there; rejecting a likely forged store "
                      "entry";
        return {};
    }
#endif

    if (version != kVersionAesAad && version != kVersionAes) return {};

    const QByteArray key = resolveKey();
    if (key.size() != kKeyLen) return {};

    const unsigned char* nonce = p + 1;
    const unsigned char* tag = p + 1 + kNonceLen;
    const unsigned char* cipher = p + 1 + kNonceLen + kTagLen;
    const int cipherLen = blob.size() - 1 - kNonceLen - kTagLen;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    auto guard = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>(
        ctx, EVP_CIPHER_CTX_free);

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) return {};
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, kNonceLen, nullptr) != 1) return {};
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           nonce) != 1) return {};

    // SEP13:5: v3 blobs authenticate the entry identity through the GCM AAD.
    // Legacy 0x01 blobs carry no AAD and are read without it (migration).
    if (version == kVersionAesAad) {
        int aadLen = 0;
        if (EVP_DecryptUpdate(ctx, nullptr, &aadLen,
                              reinterpret_cast<const unsigned char*>(identity.constData()),
                              identity.size()) != 1) return {};
    }

    QByteArray plain(cipherLen, Qt::Uninitialized);
    int plainLen = 0;
    if (EVP_DecryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(plain.data()), &plainLen,
                          cipher, cipherLen) != 1) return {};

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, kTagLen,
                            const_cast<unsigned char*>(tag)) != 1) return {};

    int finalLen = 0;
    if (EVP_DecryptFinal_ex(ctx,
                            reinterpret_cast<unsigned char*>(plain.data()) + plainLen,
                            &finalLen) != 1) {
        qWarning() << "EncryptedFileSecretStore: authentication failed for an "
                      "entry blob (corrupted, wrong key, or a blob moved "
                      "between entries); refusing to return data";
        return {};  // authentication failure — never garbage
    }
    plain.resize(plainLen + finalLen);
    return plain;
}

bool EncryptedFileSecretStore::storeSecret(const QString& service, const QString& secret)
{
    if (service.isEmpty() || secret.isEmpty()) return false;

    // Encrypt OUTSIDE the lock: key derivation is pure computation and must
    // not extend the critical section.
    const QByteArray blob = encrypt(service, secret.toUtf8());
    if (blob.isEmpty()) {
        qWarning() << "EncryptedFileSecretStore: encryption failed; secret NOT stored for" << service;
        return false;  // never claim success
    }

    // Ensure the directory exists BEFORE locking — QLockFile needs somewhere
    // to put its lock file.
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    // PGR-25: this is a read-modify-write of the shared JSON. Without the
    // lock, two instances storing different entries at the same time lose
    // updates silently (last commit wins, the other entry vanishes). Hold the
    // store lock across the WHOLE read-modify-write; on timeout fail loudly
    // rather than race.
    {
        QLockFile lock(storeLockPath(m_filePath));
        if (!lock.tryLock(kStoreLockTimeoutMs)) {
            qWarning() << "EncryptedFileSecretStore: store is locked by "
                          "another GlyphPDF instance; timed out after"
                       << kStoreLockTimeoutMs << "ms; secret NOT stored for"
                       << service;
            return false;
        }

        // Load existing object (tolerate absent/empty/corrupt — we overwrite).
        QJsonObject root;
        QFile in(m_filePath);
        if (in.exists() && in.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(in.readAll());
            in.close();
            if (doc.isObject()) root = doc.object();
        }
        root.insert(QStringLiteral("_marker"), kMarker);

        QJsonObject entries = root.value(QStringLiteral("secrets")).toObject();
        entries.insert(service, QString::fromLatin1(blob.toBase64()));
        root.insert(QStringLiteral("secrets"), entries);

        // Atomic write — QSaveFile commits all-or-nothing; never leaves a partial.
        QSaveFile out(m_filePath);
        if (!out.open(QIODevice::WriteOnly)) {
            qWarning() << "EncryptedFileSecretStore: cannot open store for write:" << m_filePath;
            return false;
        }
        const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
        if (out.write(json) != json.size()) {
            out.cancelWriting();
            qWarning() << "EncryptedFileSecretStore: short write; secret NOT stored for" << service;
            return false;
        }
        if (!out.commit()) {
            qWarning() << "EncryptedFileSecretStore: commit failed; secret NOT stored for" << service;
            return false;
        }
    }  // lock released — verification below must not hold the lock

    // Verify the secret is actually re-readable before reporting success, so we
    // can NEVER silently lose a key while returning true.
    return readSecret(service) == secret;
}

QString EncryptedFileSecretStore::readSecret(const QString& service) const
{
    if (service.isEmpty()) return {};
    QFile in(m_filePath);
    if (!in.exists() || !in.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(in.readAll());
    in.close();
    if (!doc.isObject()) return {};
    const QJsonObject entries = doc.object().value(QStringLiteral("secrets")).toObject();
    const QString b64 = entries.value(service).toString();
    if (b64.isEmpty()) return {};
    const QByteArray blob = QByteArray::fromBase64(b64.toLatin1());

#ifdef _WIN32
    // PGR-20 migration: 0x02 (DPAPI without entry entropy) is the LAST legacy
    // format with no entry binding. On the first successful read, re-wrap the
    // entry as v3 (0x04 default path / 0x03 override key — whichever encrypt()
    // produces here, both bound to the service name), closing the unbound
    // migration window. Best-effort: the secret is already recovered, so a
    // failed re-wrap keeps the legacy blob for a later attempt and never fails
    // the read.
    if (!blob.isEmpty() && static_cast<quint8>(blob.at(0)) == kVersionDpapi) {
        const QByteArray legacyPlain = decrypt(service, blob);
        if (legacyPlain.isEmpty()) return {};
        const QString plain = QString::fromUtf8(legacyPlain);
        const QByteArray wrapped = encrypt(service, legacyPlain);
        if (!wrapped.isEmpty()
            && static_cast<quint8>(wrapped.at(0)) != kVersionDpapi) {
            // PGR-25: the re-wrap rewrites the shared JSON — do it under the
            // same lock as every other write, and from a FRESH root read (the
            // root read above may predate another writer's commit).
            QLockFile lock(storeLockPath(m_filePath));
            if (lock.tryLock(kStoreLockTimeoutMs)) {
                QJsonObject freshRoot;
                QFile cur(m_filePath);
                if (cur.open(QIODevice::ReadOnly)) {
                    const auto freshDoc = QJsonDocument::fromJson(cur.readAll());
                    cur.close();
                    if (freshDoc.isObject()) freshRoot = freshDoc.object();
                }
                QJsonObject freshEntries =
                    freshRoot.value(QStringLiteral("secrets")).toObject();
                freshEntries.insert(service,
                                    QString::fromLatin1(wrapped.toBase64()));
                freshRoot.insert(QStringLiteral("secrets"), freshEntries);
                const QByteArray json =
                    QJsonDocument(freshRoot).toJson(QJsonDocument::Compact);
                QSaveFile out(m_filePath);
                if (out.open(QIODevice::WriteOnly)
                    && out.write(json) == json.size() && out.commit()) {
                    qDebug() << "EncryptedFileSecretStore: migrated legacy "
                                "0x02 entry" << service
                             << "to the v3 (entry-bound) format";
                } else {
                    qWarning() << "EncryptedFileSecretStore: could not "
                                  "migrate legacy 0x02 entry" << service
                               << "to the v3 format (store not rewritten); "
                                  "will retry on the next read";
                }
            } else {
                qWarning() << "EncryptedFileSecretStore: store is locked by "
                              "another GlyphPDF instance; deferring the "
                              "legacy 0x02 migration of" << service
                           << "to a later read";
            }
        }
        return plain;
    }
#endif

    const QByteArray plain = decrypt(service, blob);
    if (plain.isEmpty()) return {};
    return QString::fromUtf8(plain);
}

bool EncryptedFileSecretStore::deleteSecret(const QString& service)
{
    if (service.isEmpty()) return false;
    QFile in(m_filePath);
    if (!in.exists()) return true;  // already absent
    if (!in.open(QIODevice::ReadOnly)) return false;
    auto doc = QJsonDocument::fromJson(in.readAll());
    in.close();
    if (!doc.isObject()) return true;  // nothing parseable to delete
    QJsonObject root = doc.object();
    QJsonObject entries = root.value(QStringLiteral("secrets")).toObject();
    if (!entries.contains(service)) return true;  // already absent

    // PGR-25: removal is a read-modify-write of the shared JSON — same lock,
    // same bounded wait, same loud failure on timeout. The authoritative root
    // is re-read under the lock so a concurrent writer's entry survives.
    {
        QLockFile lock(storeLockPath(m_filePath));
        if (!lock.tryLock(kStoreLockTimeoutMs)) {
            qWarning() << "EncryptedFileSecretStore: store is locked by "
                          "another GlyphPDF instance; timed out after"
                       << kStoreLockTimeoutMs << "ms; entry NOT removed:"
                       << service;
            return false;
        }
        QJsonObject freshRoot;
        QFile cur(m_filePath);
        if (cur.open(QIODevice::ReadOnly)) {
            const auto freshDoc = QJsonDocument::fromJson(cur.readAll());
            cur.close();
            if (freshDoc.isObject()) freshRoot = freshDoc.object();
        }
        if (freshRoot.isEmpty()) freshRoot = root;  // vanished mid-flight: treat as absent
        QJsonObject freshEntries =
            freshRoot.value(QStringLiteral("secrets")).toObject();
        if (freshEntries.contains(service)) {
            freshEntries.remove(service);
            freshRoot.insert(QStringLiteral("secrets"), freshEntries);

            QSaveFile out(m_filePath);
            if (!out.open(QIODevice::WriteOnly)) return false;
            const QByteArray json =
                QJsonDocument(freshRoot).toJson(QJsonDocument::Compact);
            if (out.write(json) != json.size()) { out.cancelWriting(); return false; }
            if (!out.commit()) return false;
        }
    }  // lock released — verification below must not hold the lock
    return readSecret(service).isEmpty();
}

bool EncryptedFileSecretStore::hasSecret(const QString& service) const
{
    return !readSecret(service).isEmpty();
}
