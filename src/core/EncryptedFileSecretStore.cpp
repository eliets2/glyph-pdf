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
//  0x02 — Windows DPAPI (CryptProtectData) wrapped secret. EC04: the DEFAULT
//         Windows path stores this format, because DPAPI protection is
//         intentionally non-deterministic: deriving an AES key by re-protecting
//         on every read (the pre-fix resolveKey()) produced a DIFFERENT key at
//         decrypt time than at encrypt time, so the store could never reread
//         its own ciphertext. Key naming/ownership: blobs are bound to the
//         Windows user account (+machine) by DPAPI itself — no app-managed key
//         material exists or is persisted; the description string
//         "GlyphPDF.SecretStore.Secret.v2" labels the blobs.
constexpr quint8 kVersionAes   = 0x01;
constexpr quint8 kVersionDpapi = 0x02;

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
    // every write unreadable. New Windows writes are DPAPI-wrapped (0x02).
    // The seed components below are account/machine IDENTIFIERS, not
    // confidential entropy: on non-Windows the derivation is best-effort
    // obfuscation only, documented as such in the header.
    QByteArray seed;
    seed += QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toUtf8();
    seed += QSysInfo::machineUniqueId();
    seed += QByteArrayLiteral("glyphpdf-secret-store-v1");

    return QCryptographicHash::hash(seed, QCryptographicHash::Sha256);
}

QByteArray EncryptedFileSecretStore::encrypt(const QByteArray& plaintext) const
{
#ifdef _WIN32
    // EC04: default path — protect the secret DIRECTLY with the platform
    // primitive. DPAPI wraps the plaintext under the user's account key; the
    // unwrapped secret is recoverable by the same user (any process, any
    // instance, any later session) and by nobody else. No derived AES key, no
    // persisted master key.
    if (m_keyOverride.isEmpty()) {
        DATA_BLOB in{};
        in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.constData()));
        in.cbData = static_cast<DWORD>(plaintext.size());
        DATA_BLOB out{};
        if (!CryptProtectData(&in, L"GlyphPDF.SecretStore.Secret.v2", nullptr,
                              nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
            const DWORD err = GetLastError();
            qWarning() << "EncryptedFileSecretStore: CryptProtectData failed"
                       << "(Win32 error" << err << "); secret NOT stored";
            return {};  // explicit failure — the caller never claims success
        }
        QByteArray blob;
        blob.append(static_cast<char>(kVersionDpapi));
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
    blob.append(static_cast<char>(kVersionAes));
    blob.append(reinterpret_cast<const char*>(nonce), kNonceLen);
    blob.append(reinterpret_cast<const char*>(tag), kTagLen);
    blob.append(cipher);
    return blob;
}

QByteArray EncryptedFileSecretStore::decrypt(const QByteArray& blob) const
{
    if (blob.size() < 1 + kNonceLen + kTagLen) return {};

    const unsigned char* p = reinterpret_cast<const unsigned char*>(blob.constData());

#ifdef _WIN32
    // EC04: DPAPI-wrapped secret — unwrap through CryptUnprotectData. A failed
    // unprotection (different user/machine, corrupted blob) is an EXPLICIT
    // failure: empty result, never garbage, never a crash.
    if (static_cast<quint8>(blob.at(0)) == kVersionDpapi) {
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
    if (static_cast<quint8>(blob.at(0)) == kVersionDpapi) {
        qWarning() << "EncryptedFileSecretStore: DPAPI-wrapped secret found on "
                      "a non-Windows platform; cannot decrypt";
        return {};
    }
#endif

    if (static_cast<quint8>(blob.at(0)) != kVersionAes) return {};

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
        return {};  // authentication failure
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

    const QByteArray blob = encrypt(secret.toUtf8());
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
    const QByteArray plain = decrypt(blob);
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
