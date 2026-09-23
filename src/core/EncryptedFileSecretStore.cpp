// SPDX-License-Identifier: Apache-2.0
#include "core/EncryptedFileSecretStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QDebug>

#include <openssl/crypto.h>  // OPENSSL_cleanse — secret scrubbing
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

// PGR-10 triage (key/plaintext zeroization): scrub a secret buffer before its
// storage is released. OPENSSL_cleanse is guaranteed not to be optimized away
// (a plain memset over a soon-dead buffer would be elided), so the derived
// AES key, the decrypted plaintext, and DPAPI's output blobs do not linger in
// freed heap memory after the store is done with them.
void scrubBuffer(void* p, size_t n)
{
    if (p && n) OPENSSL_cleanse(p, n);
}

void scrubBuffer(QByteArray& buffer)
{
    if (!buffer.isEmpty()) scrubBuffer(buffer.data(), size_t(buffer.size()));
}

// RAII: scrub on every scope exit — decrypt()/encrypt() have several early
// returns (allocation failure, GCM init failure, authentication failure) and
// the derived key must be scrubbed on all of them.
struct ScrubOnScopeExit {
    QByteArray* buffer;
    explicit ScrubOnScopeExit(QByteArray* b) : buffer(b) {}
    ~ScrubOnScopeExit() { if (buffer) scrubBuffer(*buffer); }
    ScrubOnScopeExit(const ScrubOnScopeExit&) = delete;
    ScrubOnScopeExit& operator=(const ScrubOnScopeExit&) = delete;
};


// On-disk blob version bytes (first byte of every stored blob).
//
//  0x01 — AES-256-GCM under SHA-256(keyMaterial override), or — legacy, kept
//         for reading pre-EC04 stores and for non-Windows default-path writes —
//         under SHA-256 of the per-user seed described in resolveKey().
//         LEGACY (readable, never written) since SEP13:5.
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
//  0x03 — SEP13:5 v3 generation: AES-256-GCM with the SERVICE NAME as the GCM
//         AAD. Ciphertext+tag authenticate the entry identity, so a blob moved
//         to another JSON entry fails authentication instead of decrypting to
//         the wrong secret.
//  0x04 — SEP13:5 v3 generation, Windows default path: DPAPI wrapped with the
//         SERVICE NAME as the optional entropy (description string
//         "GlyphPDF.SecretStore.Secret.v3"). The blob is bound to the entry
//         identity it was written under — cross-entry blob swaps fail
//         unprotection loudly (the v2 format's constant description + missing
//         entropy let a local actor with store write-access swap blobs
//         between entries undetected).
constexpr quint8 kVersionAes   = 0x01;  // legacy — readable, never written
constexpr quint8 kVersionDpapi = 0x02;  // legacy — readable, never written
constexpr quint8 kVersionAesAad   = 0x03;
constexpr quint8 kVersionDpapiV3  = 0x04;
const wchar_t kDpapiV3Description[] = L"GlyphPDF.SecretStore.Secret.v3";

// Explicit on-disk marker so the file is self-describing / labelled.
const QString kMarker = QStringLiteral("glyphpdf-encrypted-secret-store");

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
        // PGR-10 triage (key/plaintext zeroization): LocalFree releases the
        // DPAPI output blob without clearing it — scrub first.
        scrubBuffer(out.pbData, size_t(out.cbData));
        LocalFree(out.pbData);
        return blob;
    }
#endif

    QByteArray key = resolveKey();
    // PGR-10 triage (key/plaintext zeroization): scrub the derived AES key on
    // EVERY scope exit — both functions early-return on failure paths.
    ScrubOnScopeExit scrubKey(&key);
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
        // PGR-10 triage (key/plaintext zeroization): the decrypted plaintext
        // was copied out — clear DPAPI's heap copy before releasing it
        // (LocalFree does not zero; DPAPI guidance says to clear first).
        scrubBuffer(out.pbData, size_t(out.cbData));
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
        // PGR-10 triage (key/plaintext zeroization): the decrypted plaintext
        // was copied out — clear DPAPI's heap copy before releasing it
        // (LocalFree does not zero; DPAPI guidance says to clear first).
        scrubBuffer(out.pbData, size_t(out.cbData));
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

    if (version != kVersionAesAad && version != kVersionAes) return {};

    QByteArray key = resolveKey();
    // PGR-10 triage (key/plaintext zeroization): scrub the derived AES key on
    // EVERY scope exit — both functions early-return on failure paths.
    ScrubOnScopeExit scrubKey(&key);
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

    // Load existing object (tolerate absent/empty/corrupt — we overwrite).
    QJsonObject root;
    QFile in(m_filePath);
    if (in.exists() && in.open(QIODevice::ReadOnly)) {
        const auto doc = QJsonDocument::fromJson(in.readAll());
        in.close();
        if (doc.isObject()) root = doc.object();
    }
    root.insert(QStringLiteral("_marker"), kMarker);

    const QByteArray blob = encrypt(service, secret.toUtf8());
    if (blob.isEmpty()) {
        qWarning() << "EncryptedFileSecretStore: encryption failed; secret NOT stored for" << service;
        return false;  // never claim success
    }

    QJsonObject entries = root.value(QStringLiteral("secrets")).toObject();
    entries.insert(service, QString::fromLatin1(blob.toBase64()));
    root.insert(QStringLiteral("secrets"), entries);

    // Ensure the directory exists.
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

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
    QByteArray plain = decrypt(service, blob);
    if (plain.isEmpty()) return {};
    const QString secret = QString::fromUtf8(plain);
    // PGR-10 triage (key/plaintext zeroization): the intermediate plaintext
    // buffer is scrubbed before it goes out of scope; the returned QString is
    // the store's documented output and is owned by the caller.
    scrubBuffer(plain);
    return secret;
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
    entries.remove(service);
    root.insert(QStringLiteral("secrets"), entries);

    QSaveFile out(m_filePath);
    if (!out.open(QIODevice::WriteOnly)) return false;
    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    if (out.write(json) != json.size()) { out.cancelWriting(); return false; }
    if (!out.commit()) return false;
    return readSecret(service).isEmpty();
}

bool EncryptedFileSecretStore::hasSecret(const QString& service) const
{
    return !readSecret(service).isEmpty();
}
