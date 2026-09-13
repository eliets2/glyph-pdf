// SPDX-License-Identifier: Apache-2.0
//
// AR-10 D4 — proves the encrypted-file secret store is REAL and NEVER
// silent-fails: a stored secret durably round-trips, an unwritable target is
// reported as a failure (not a silent drop), the plaintext is never on disk,
// and tampered / wrong-key blobs do not decrypt to garbage.

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

#include "core/EncryptedFileSecretStore.h"
#include "core/ISecretStore.h"

#if defined(HAS_LIBSECRET)
#include "core/LibSecretStore.h"
#endif

// ---------------------------------------------------------------------------
// SEP13:5 helpers — direct manipulation of the JSON store's blob entries so
// the cross-entry substitution attack (swap two entries' base64 blobs) can be
// exercised exactly as an attacker with file write-access would.
// ---------------------------------------------------------------------------
namespace {

QJsonObject readStoreRoot(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

bool writeServiceBlob(const QString& path, const QString& service,
                      const QByteArray& blob)
{
    QJsonObject root = readStoreRoot(path);
    if (root.isEmpty()) {
        // Absent store: seed the same layout the real writer produces.
        root.insert(QStringLiteral("_marker"),
                    QStringLiteral("glyphpdf-encrypted-secret-store"));
        root.insert(QStringLiteral("secrets"), QJsonObject{});
    }
    QJsonObject entries = root.value(QStringLiteral("secrets")).toObject();
    entries.insert(service, QString::fromLatin1(blob.toBase64()));
    root.insert(QStringLiteral("secrets"), entries);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(QJsonDocument(root).toJson(QJsonDocument::Compact)) > 0;
}

QByteArray readServiceBlob(const QString& path, const QString& service)
{
    const QJsonObject root = readStoreRoot(path);
    return QByteArray::fromBase64(root.value(QStringLiteral("secrets"))
                                     .toObject().value(service).toString().toLatin1());
}

bool swapServiceBlobs(const QString& path, const QString& a, const QString& b)
{
    const QByteArray blobA = readServiceBlob(path, a);
    const QByteArray blobB = readServiceBlob(path, b);
    if (blobA.isEmpty() || blobB.isEmpty()) return false;
    return writeServiceBlob(path, a, blobB) && writeServiceBlob(path, b, blobA);
}

// Craft a legacy 0x01 blob (pre-SEP13:5 AES-GCM WITHOUT AAD) under the given
// raw key — the migration-readability fixture.
QByteArray craftLegacyV1Blob(const QByteArray& rawKey, const QByteArray& plaintext)
{
    const QByteArray key = QCryptographicHash::hash(rawKey, QCryptographicHash::Sha256);
    unsigned char nonce[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    QByteArray cipher(plaintext.size(), Qt::Uninitialized);
    int len = 0, cipherLen = 0;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) return {};
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1) return {};
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           nonce) != 1) return {};
    if (EVP_EncryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(cipher.data()), &cipherLen,
                          reinterpret_cast<const unsigned char*>(plaintext.constData()),
                          plaintext.size()) != 1) return {};
    int finalLen = 0;
    if (EVP_EncryptFinal_ex(ctx,
                            reinterpret_cast<unsigned char*>(cipher.data()) + cipherLen,
                            &finalLen) != 1) return {};
    cipher.resize(cipherLen + finalLen);
    unsigned char tag[16];
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) return {};
    EVP_CIPHER_CTX_free(ctx);
    QByteArray blob;
    blob.append(static_cast<char>(0x01));
    blob.append(reinterpret_cast<const char*>(nonce), 12);
    blob.append(reinterpret_cast<const char*>(tag), 16);
    blob.append(cipher);
    return blob;
}

} // namespace

// ── EC04 child mode ──────────────────────────────────────────────────────────
// The DPAPI round-trip must survive a PROCESS boundary, not merely a new store
// instance. The test binary re-execs itself with `--ec04-child <storePath>`:
// the child writes a synthetic credential through the REAL default key path
// (no override) and exits; the parent then reads it back with its own fresh
// store instance. Synthetic credentials only — never a real API key.
#ifdef Q_OS_WIN
static const char* kChildService = "ChildProcessSvc";
static const char* kChildSecret  = "sk-ant-child-process-fake-key-0001";

static int ec04ChildMain(const QString& storePath)
{
    EncryptedFileSecretStore store(storePath);  // default path: real DPAPI
    if (!store.storeSecret(QString::fromLatin1(kChildService),
                           QString::fromLatin1(kChildSecret)))
        return 3;  // storeSecret must report success (EC04: pre-fix it cannot)
    if (store.readSecret(QString::fromLatin1(kChildService))
            != QString::fromLatin1(kChildSecret))
        return 4;  // and the child itself must be able to reread it
    return 0;
}
#endif

class TestSecretStore : public QObject {
    Q_OBJECT

    // Deterministic key material so the crypto path is exercised without
    // touching machine/user state.
    static QByteArray testKey() { return QByteArrayLiteral("test-key-material-AR10-D4"); }

private slots:

    // Round-trip: a stored secret comes back intact (it is NOT dropped).
    void testStoreAndReadRoundTrip() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        EncryptedFileSecretStore store(dir.path() + "/s.json", testKey());

        QVERIFY(store.storeSecret("Anthropic", "sk-ant-secret-value-123456"));
        QCOMPARE(store.readSecret("Anthropic"), QString("sk-ant-secret-value-123456"));
        QVERIFY(store.hasSecret("Anthropic"));
        QCOMPARE(store.backend(), ISecretStore::Backend::EncryptedFile);
    }

    // The backing file must NOT contain the plaintext secret.
    void testPlaintextNotOnDisk() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/s.json";
        EncryptedFileSecretStore store(path, testKey());

        const QString secret = "sk-ant-PLAINTEXT-MARKER-zzz999";
        QVERIFY(store.storeSecret("OpenAI", secret));

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray raw = f.readAll();
        f.close();
        QVERIFY2(!raw.contains(secret.toUtf8()),
                 "Plaintext secret must not appear in the encrypted store file");
    }

    // NO SILENT FAILURE: an unwritable target reports false; it never returns
    // true while dropping the secret, and the secret is not readable.
    void testNoSilentFailureOnUnwritablePath() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // A path whose parent is a *file*, so mkpath + open must fail.
        const QString blocker = dir.path() + "/blocker";
        { QFile b(blocker); QVERIFY(b.open(QIODevice::WriteOnly)); b.write("x"); b.close(); }
        const QString badPath = blocker + "/cannot/secrets.json";

        EncryptedFileSecretStore store(badPath, testKey());
        const bool stored = store.storeSecret("Anthropic", "sk-ant-should-not-persist");
        QVERIFY2(!stored, "storeSecret must report failure when it cannot persist");
        // And it must not be silently readable either.
        QVERIFY2(store.readSecret("Anthropic").isEmpty(),
                 "A failed store must leave nothing readable");
    }

    // Empty inputs are rejected rather than silently 'succeeding'.
    void testEmptyInputsRejected() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        EncryptedFileSecretStore store(dir.path() + "/s.json", testKey());
        QVERIFY(!store.storeSecret("", "x"));
        QVERIFY(!store.storeSecret("svc", ""));
    }

    // A different key cannot read another store's secret (no cross-key leak,
    // and authentication failure is reported as empty, not garbage).
    void testWrongKeyCannotDecrypt() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/s.json";
        {
            EncryptedFileSecretStore good(path, testKey());
            QVERIFY(good.storeSecret("svc", "sk-ant-original-secret"));
        }
        EncryptedFileSecretStore wrong(path, QByteArrayLiteral("a-different-wrong-key"));
        QVERIFY2(wrong.readSecret("svc").isEmpty(),
                 "A wrong key must not decrypt; auth failure returns empty");
    }

    // Tampering with the ciphertext is detected (GCM auth) → empty, not garbage.
    void testTamperDetected() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/s.json";
        EncryptedFileSecretStore store(path, testKey());
        QVERIFY(store.storeSecret("svc", "sk-ant-tamper-target"));

        // Flip a byte inside the encrypted base64 ciphertext for "svc".
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QByteArray data = f.readAll();
        f.close();
        // The value lives at {..."secrets":{"svc":"<base64>"}}. Locate the
        // base64 value that follows the "svc" key and corrupt a byte well past
        // the version/nonce header so it lands in the ciphertext or GCM tag.
        const int keyPos = data.indexOf("\"svc\":\"");
        QVERIFY(keyPos > 0);
        const int valStart = keyPos + int(qstrlen("\"svc\":\""));
        const int valEnd = data.indexOf('"', valStart);
        QVERIFY(valEnd > valStart);
        // Corrupt a base64 char near the end of the value (tag region).
        const int p = valEnd - 2;
        QVERIFY(p > valStart);
        data[p] = (data.at(p) == 'A') ? 'B' : 'A';
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(data);
        f.close();

        EncryptedFileSecretStore reopened(path, testKey());
        QVERIFY2(reopened.readSecret("svc").isEmpty(),
                 "Tampered ciphertext must fail authentication and return empty");
    }

    // Delete removes the secret durably.
    void testDeleteRemovesSecret() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        EncryptedFileSecretStore store(dir.path() + "/s.json", testKey());
        QVERIFY(store.storeSecret("svc", "sk-ant-to-delete-000"));
        QVERIFY(store.hasSecret("svc"));
        QVERIFY(store.deleteSecret("svc"));
        QVERIFY(!store.hasSecret("svc"));
        QVERIFY(store.readSecret("svc").isEmpty());
        // Deleting an absent secret is a no-op success, never a silent error.
        QVERIFY(store.deleteSecret("svc"));
    }

    // Multiple services coexist independently.
    void testMultipleServices() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        EncryptedFileSecretStore store(dir.path() + "/s.json", testKey());
        QVERIFY(store.storeSecret("Anthropic", "sk-ant-aaa"));
        QVERIFY(store.storeSecret("OpenAI", "sk-ooo"));
        QCOMPARE(store.readSecret("Anthropic"), QString("sk-ant-aaa"));
        QCOMPARE(store.readSecret("OpenAI"), QString("sk-ooo"));
        QVERIFY(store.deleteSecret("Anthropic"));
        QVERIFY(!store.hasSecret("Anthropic"));
        QCOMPARE(store.readSecret("OpenAI"), QString("sk-ooo"));  // unaffected
    }

    // ── EC04: the DEFAULT key path (no injected override) on Windows ─────────
    // Pre-fix, resolveKey() called CryptProtectData on EVERY invocation and
    // hashed the fresh (non-deterministic) DPAPI blob as the AES key, so the
    // store could never reread its own ciphertext: storeSecret failed its own
    // verification and returned false. These tests pin the repaired contract
    // on the real default path with the real DPAPI (synthetic secrets only).

#ifdef Q_OS_WIN
    // The default path must round-trip: store, read repeatedly, and read from
    // a NEW store instance — all without any injected key material.
    void dpapiDefaultPathRoundTrips() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/dpapi-store.json";
        const QString secret = QStringLiteral("sk-ant-dpapi-roundtrip-fake-0001");

        EncryptedFileSecretStore writer(path);  // NO override — the real path
        QVERIFY2(writer.storeSecret("DpapiSvc", secret),
                 "storeSecret must succeed on the default (DPAPI) key path");

        // Multiple reads from the same instance.
        QCOMPARE(writer.readSecret("DpapiSvc"), secret);
        QCOMPARE(writer.readSecret("DpapiSvc"), secret);

        // A brand-new instance (fresh DPAPI unwrap) must read the same secret.
        EncryptedFileSecretStore reader(path);
        QCOMPARE(reader.readSecret("DpapiSvc"), secret);
        QVERIFY(reader.hasSecret("DpapiSvc"));
        QCOMPARE(reader.backend(), ISecretStore::Backend::EncryptedFile);

        // The stored blob carries the v3-generation DPAPI version byte — the
        // format pin that documents on-disk key naming for support/forensics.
        // SEP13:5: new default-path writes are 0x04 (DPAPI with the service
        // name as optional entropy — the blob is bound to its JSON entry);
        // the 0x02 format stays readable for migration
        // (dpapiV2LegacyBlobStillReadable).
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        QVERIFY(doc.isObject());
        const QString b64 = doc.object().value("secrets").toObject()
                                .value("DpapiSvc").toString();
        QVERIFY(!b64.isEmpty());
        const QByteArray blob = QByteArray::fromBase64(b64.toLatin1());
        QVERIFY(!blob.isEmpty());
        QCOMPARE(int(static_cast<quint8>(blob.at(0))), 0x04);  // v3 DPAPI+entropy
    }

    // The DPAPI round-trip survives a process boundary: a child process writes
    // through its own default-path store; this process reads it back.
    void dpapiRoundTripSurvivesNewProcess() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/dpapi-child-store.json";

        const QString exe = QCoreApplication::applicationFilePath();
        const int rc = QProcess::execute(exe,
                                         {QStringLiteral("--ec04-child"), path});
        QVERIFY2(rc == 0,
                 "child process must store AND reread its synthetic secret "
                 "through the default DPAPI path (pre-fix rc=3)");

        // The parent is a different process than the writer: fresh DPAPI
        // unwrap must yield the exact secret.
        EncryptedFileSecretStore reader(path);
        QCOMPARE(reader.readSecret(QString::fromLatin1(kChildService)),
                 QString::fromLatin1(kChildSecret));

        // And the plaintext never touched the disk unencrypted.
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray raw = f.readAll();
        f.close();
        QVERIFY2(!raw.contains(kChildSecret),
                 "plaintext child secret must not appear in the store file");
    }

    // Corruption must fail EXPLICITLY: CryptUnprotectData rejects a damaged
    // blob and the store reports empty — never garbage, never a crash.
    void corruptDpapiBlobFailsExplicitly() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/dpapi-corrupt.json";
        const QString secret = QStringLiteral("sk-ant-dpapi-corrupt-fake-0001");

        EncryptedFileSecretStore writer(path);
        QVERIFY(writer.storeSecret("DpapiCorruptSvc", secret));

        // Corrupt bytes deep inside the DPAPI blob (past the version byte).
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QByteArray data = f.readAll();
        f.close();
        const int keyPos = data.indexOf("\"DpapiCorruptSvc\":\"");
        QVERIFY(keyPos > 0);
        const int valStart = keyPos + int(qstrlen("\"DpapiCorruptSvc\":\""));
        const int valEnd = data.indexOf('"', valStart);
        QVERIFY(valEnd > valStart + 20);
        const int p = valEnd - 8;  // middle of the DPAPI blob
        data[p] = (data.at(p) == 'A') ? 'B' : 'A';
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(data);
        f.close();

        EncryptedFileSecretStore reader(path);
        QVERIFY2(reader.readSecret("DpapiCorruptSvc").isEmpty(),
                 "a corrupted DPAPI blob must fail unprotection explicitly "
                 "(empty result), never decrypt to garbage");
        QVERIFY(!reader.hasSecret("DpapiCorruptSvc"));
    }
#endif // Q_OS_WIN

    // ── SEP13:5 — AEAD bound to entry identity ──────────────────────────────
    // Pre-fix the ciphertexts were not bound to the JSON `service` key they
    // live under (no GCM AAD; constant DPAPI description, no optional
    // entropy), so a local actor with WRITE ACCESS to secrets.enc.json — no
    // key/DPAPI needed — could swap two entries' base64 blobs and each still
    // decrypted + authenticated: readSecret("api_key_prod") silently returned
    // the swapped secret. The v3 generation feeds the service name into the
    // authentication material, so every swapped read fails LOUDLY (empty).

    // The AES path (override key; portable — this is also the non-Windows
    // default format): swapped blobs must fail authentication on BOTH entries.
    void blobSwapBetweenEntriesFailsLoudly() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/s.json";
        EncryptedFileSecretStore store(path, testKey());
        QVERIFY(store.storeSecret("api_key_prod", "sk-ant-prod-value-0001"));
        QVERIFY(store.storeSecret("api_key_test", "sk-ant-test-value-0002"));
        QCOMPARE(store.readSecret("api_key_prod"), QString("sk-ant-prod-value-0001"));
        QCOMPARE(store.readSecret("api_key_test"), QString("sk-ant-test-value-0002"));

        // The substitution attack: swap the two base64 blobs in the JSON.
        QVERIFY2(swapServiceBlobs(path, "api_key_prod", "api_key_test"),
                 "the fixture must be able to rewrite the store's blobs");
        QVERIFY2(store.readSecret("api_key_prod").isEmpty(),
                 "a blob moved to another entry must fail authentication "
                 "(GCM AAD binds the ciphertext to the entry identity)");
        QVERIFY2(store.readSecret("api_key_test").isEmpty(),
                 "the other swapped blob must fail authentication too");
        QVERIFY(!store.hasSecret("api_key_prod"));

        // Swapping BACK restores both reads — the blobs were intact all
        // along; only the entry binding was violated. This pins that the
        // failure is identity enforcement, not corruption.
        QVERIFY(swapServiceBlobs(path, "api_key_prod", "api_key_test"));
        QCOMPARE(store.readSecret("api_key_prod"), QString("sk-ant-prod-value-0001"));
        QCOMPARE(store.readSecret("api_key_test"), QString("sk-ant-test-value-0002"));
    }

#ifdef Q_OS_WIN
    // The REAL default path (DPAPI, no override): the v3 entropy binding must
    // reject a cross-entry blob swap on both entries.
    void dpapiBlobSwapBetweenEntriesFailsLoudly() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/dpapi-swap.json";
        EncryptedFileSecretStore store(path);   // default path — real DPAPI
        QVERIFY(store.storeSecret("api_key_prod", "sk-ant-dpapi-prod-0001"));
        QVERIFY(store.storeSecret("api_key_test", "sk-ant-dpapi-test-0002"));
        QCOMPARE(store.readSecret("api_key_prod"), QString("sk-ant-dpapi-prod-0001"));
        QCOMPARE(store.readSecret("api_key_test"), QString("sk-ant-dpapi-test-0002"));

        QVERIFY2(swapServiceBlobs(path, "api_key_prod", "api_key_test"),
                 "the fixture must be able to rewrite the store's blobs");
        QVERIFY2(store.readSecret("api_key_prod").isEmpty(),
                 "a DPAPI blob unwrapped with another entry's entropy must "
                 "fail loudly (pre-fix it silently returned the swapped "
                 "secret — the exact cross-entry substitution defect)");
        QVERIFY2(store.readSecret("api_key_test").isEmpty(),
                 "the other swapped blob must fail unprotection too");

        QVERIFY(swapServiceBlobs(path, "api_key_prod", "api_key_test"));
        QCOMPARE(store.readSecret("api_key_prod"), QString("sk-ant-dpapi-prod-0001"));
        QCOMPARE(store.readSecret("api_key_test"), QString("sk-ant-dpapi-test-0002"));
    }

    // Migration: legacy 0x02 blobs (EC04 — DPAPI WITHOUT entry entropy) must
    // stay readable; the v3 upgrade never orphans an existing store.
    void dpapiV2LegacyBlobStillReadable() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/dpapi-v2.json";

        // Craft a v2 blob exactly the way the EC04 store wrote it: constant
        // description, NO optional entropy.
        const QString secret = QStringLiteral("sk-ant-legacy-v2-0001");
        const QByteArray plain = secret.toUtf8();
        DATA_BLOB in{};
        in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plain.constData()));
        in.cbData = static_cast<DWORD>(plain.size());
        DATA_BLOB out{};
        QVERIFY2(CryptProtectData(&in, L"GlyphPDF.SecretStore.Secret.v2", nullptr,
                                  nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out),
                 "fixture crafting: CryptProtectData must succeed");
        QByteArray blob;
        blob.append(static_cast<char>(0x02));
        blob.append(reinterpret_cast<const char*>(out.pbData),
                    static_cast<int>(out.cbData));
        LocalFree(out.pbData);
        QVERIFY(writeServiceBlob(path, "LegacyV2Svc", blob));

        EncryptedFileSecretStore reader(path);
        QCOMPARE(reader.readSecret("LegacyV2Svc"), secret);
    }
#endif // Q_OS_WIN

    // Migration: legacy 0x01 blobs (pre-SEP13:5 AES-GCM WITHOUT AAD) must
    // stay readable under the override-key path.
    void aesV1LegacyBlobStillReadable() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/s.json";
        const QString secret = QStringLiteral("sk-ant-legacy-v1-0002");

        // Seed the store with one CURRENT entry so the JSON shape is real,
        // then add the crafted legacy blob as a second entry.
        EncryptedFileSecretStore writer(path, testKey());
        QVERIFY(writer.storeSecret("CurrentSvc", "sk-ant-current-0003"));
        QVERIFY(writeServiceBlob(path, "LegacyV1Svc",
                                 craftLegacyV1Blob(testKey(), secret.toUtf8())));

        EncryptedFileSecretStore reader(path, testKey());
        QCOMPARE(reader.readSecret("LegacyV1Svc"), secret);
        QCOMPARE(reader.readSecret("CurrentSvc"), QString("sk-ant-current-0003"));
    }

    // ── L07 (NATIVE-LINUX-READINESS-2026-09-10): the Secret Service backend ──
    // On HAS_LIBSECRET builds the Secret Service is the Linux PRIMARY store
    // and CredentialManager never writes new secrets through the
    // identifier-derived encrypted-file path. These tests pin the negative
    // contract in an environment with NO keyring daemon (containers/CI): the
    // store reports DEFINITE failures — loud, not silent, and with NO
    // file-store side effects. The positive round-trip needs a live Secret
    // Service and stays a desktop/runtime gate (recorded as residual).

#if defined(HAS_LIBSECRET)
    // Without a keyring, storeSecret fails loudly and stores nothing.
    void libsecretAbsentStoreFailsLoudly() {
        LibSecretStore store;
        QCOMPARE(store.backend(), ISecretStore::Backend::SecretService);
        QVERIFY2(!store.storeSecret("Anthropic", "sk-ant-libsecret-absent-0001"),
                 "storeSecret must report failure when no Secret Service is "
                 "available (never a silent drop, never a fallback)");
        QVERIFY2(store.readSecret("Anthropic").isEmpty(),
                 "a failed store must leave nothing readable");
    }

    // The loud failure must NOT side-effect into the encrypted-file fallback:
    // no default-path secrets file may appear from exercising this backend.
    void libsecretAbsentWritesNoFileFallback() {
        const QString defaultPath = []() {
            const QString base = QStandardPaths::writableLocation(
                QStandardPaths::AppDataLocation);
            return (base.isEmpty() ? QDir::homePath() + QStringLiteral("/.glyphpdf")
                                   : base) + QStringLiteral("/secrets.enc.json");
        }();
        if (QFile::exists(defaultPath)) {
            QFileInfo info(defaultPath);
            const QDateTime before = info.lastModified();
            LibSecretStore store;
            store.storeSecret("OpenAI", "sk-probe-fallback-side-effect-0001");
            info.refresh();
            QVERIFY2(info.lastModified() == before,
                     "a failed Secret Service write must not touch the "
                     "encrypted-file fallback store");
        } else {
            LibSecretStore store;
            store.storeSecret("OpenAI", "sk-probe-fallback-side-effect-0001");
            QVERIFY2(!QFile::exists(defaultPath),
                     "a failed Secret Service write must not create the "
                     "encrypted-file fallback store");
        }
    }

    // Reads of an absent service are empty (absence is not an error), and
    // deleteSecret without a keyring reports the definite outcome (a real
    // failure here is acceptable; a claimed success without a keyring is not).
    void libsecretAbsentReadAndDeleteHonesty() {
        LibSecretStore store;
        QVERIFY(store.readSecret("NeverStoredSvc").isEmpty());
        QVERIFY(!store.hasSecret("NeverStoredSvc"));
        // clear_sync without a daemon errors → false is honest. Just pin that
        // the call COMPLETES either way (no crash/hang).
        const bool removed = store.deleteSecret("NeverStoredSvc");
        if (removed) {
            QVERIFY(!store.hasSecret("NeverStoredSvc"));
        }
    }
#endif // HAS_LIBSECRET
};

// EC04: custom main mirroring QTEST_GUILESS_MAIN plus the --ec04-child
// re-exec branch used by dpapiRoundTripSurvivesNewProcess.
int main(int argc, char** argv)
{
#ifdef Q_OS_WIN
    for (int i = 1; i + 1 < argc; ++i) {
        if (QByteArray(argv[i]) == QByteArrayLiteral("--ec04-child"))
            return ec04ChildMain(QString::fromLocal8Bit(argv[i + 1]));
    }
#else
    Q_UNUSED(argc);
    Q_UNUSED(argv);
#endif
    QCoreApplication app(argc, argv);
    TestSecretStore tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "TestSecretStore.moc"
