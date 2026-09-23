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
#include <QSysInfo>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#include <wincrypt.h>
#endif

#include "core/EncryptedFileSecretStore.h"
#include "core/CredentialManager.h"
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

// Craft a 0x03 blob (SEP13:5 AES-256-GCM WITH the service name as GCM AAD)
// under the given raw key.
QByteArray craftAadBlob(const QByteArray& rawKey, const QByteArray& identity,
                        const QByteArray& plaintext)
{
    const QByteArray key = QCryptographicHash::hash(rawKey, QCryptographicHash::Sha256);
    unsigned char nonce[12] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 1, 2};
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    QByteArray cipher(plaintext.size(), Qt::Uninitialized);
    int cipherLen = 0;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) return {};
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) != 1) return {};
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           nonce) != 1) return {};
    int aadLen = 0;
    if (EVP_EncryptUpdate(ctx, nullptr, &aadLen,
                          reinterpret_cast<const unsigned char*>(identity.constData()),
                          identity.size()) != 1) return {};
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
    blob.append(static_cast<char>(0x03));
    blob.append(reinterpret_cast<const char*>(nonce), 12);
    blob.append(reinterpret_cast<const char*>(tag), 16);
    blob.append(cipher);
    return blob;
}

#ifdef Q_OS_WIN
// Craft a legacy 0x02 blob (EC04 — DPAPI with a constant description and NO
// optional entropy) — the migration fixture for the Windows default path.
QByteArray craftLegacyV2Blob(const QByteArray& plaintext)
{
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.constData()));
    in.cbData = static_cast<DWORD>(plaintext.size());
    DATA_BLOB out{};
    if (!CryptProtectData(&in, L"GlyphPDF.SecretStore.Secret.v2", nullptr,
                          nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return {};
    QByteArray blob;
    blob.append(static_cast<char>(0x02));
    blob.append(reinterpret_cast<const char*>(out.pbData),
                static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return blob;
}
#endif

// PGR-20: replicate the store's no-override seed derivation (resolveKey())
// exactly. Its inputs — home path, machine id, a constant — are identifiers,
// not secrets: this is precisely the material a local attacker would use to
// forge AES blobs on the Windows default path.
QByteArray windowsDefaultSeed()
{
    QByteArray seed;
    seed += QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toUtf8();
    seed += QSysInfo::machineUniqueId();
    seed += QByteArrayLiteral("glyphpdf-secret-store-v1");
    return seed;
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

    // ── PGR-25 — concurrent writers must not lose updates ───────────────────
    // Each write is a read-modify-write of the JSON store. Two processes (or
    // threads) storing DIFFERENT entries at the same time must both survive:
    // without a lock around the whole RMW the last commit wins and the other
    // entry silently vanishes — the money-race pattern from the backend
    // doctrine, applied to the secret store. Each write verifies itself by
    // re-reading (never-silent-fail), so a lost update surfaces either as a
    // failed storeSecret or as a missing final value.
    void concurrentWritersDifferentEntriesBothSurvive() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/race.json";

        static constexpr int kRounds = 40;
        // Round handshake: the driver releases round r (go >= r) only after
        // both writers finished round r-1 (done >= 2*(r-1)), so both writers
        // enter storeSecret within microseconds of each other every round.
        QAtomicInt go{0};
        QAtomicInt done{0};

        struct Writer : QThread {
            Writer(QString p, QByteArray k, const QString s,
                   const QAtomicInt* goRef, QAtomicInt* doneRef)
                : path(std::move(p)), key(std::move(k)), svc(std::move(s)),
                  go(goRef), done(doneRef) {}
            QString path;
            QByteArray key;
            QString svc;
            const QAtomicInt* go;
            QAtomicInt* done;
            bool ok = true;
            void run() override {
                EncryptedFileSecretStore store(path, key);
                for (int r = 1; r <= kRounds; ++r) {
                    while (go->loadAcquire() < r) {}
                    if (ok) {
                        ok = store.storeSecret(
                            svc, QStringLiteral("sk-ant-race-%1-%2").arg(svc).arg(r));
                    }
                    done->fetchAndAddRelaxed(1);  // keep the handshake alive
                }
            }
        };

        Writer a(path, testKey(), QStringLiteral("RaceSvcA"), &go, &done);
        Writer b(path, testKey(), QStringLiteral("RaceSvcB"), &go, &done);
        a.start();
        b.start();
        for (int r = 1; r <= kRounds; ++r) {
            while (done.loadAcquire() < 2 * (r - 1)) {}
            go.fetchAndAddRelaxed(1);
        }
        QVERIFY(a.wait(60000));
        QVERIFY(b.wait(60000));

        QVERIFY2(a.ok && b.ok,
                 "both concurrent writers must report success on every round "
                 "— a lost update makes storeSecret fail its own read-back "
                 "verification");

        // The store must retain BOTH entries, each with its final value.
        EncryptedFileSecretStore reader(path, testKey());
        QCOMPARE(reader.readSecret("RaceSvcA"),
                 QStringLiteral("sk-ant-race-RaceSvcA-%1").arg(kRounds));
        QCOMPARE(reader.readSecret("RaceSvcB"),
                 QStringLiteral("sk-ant-race-RaceSvcB-%1").arg(kRounds));
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
        const QByteArray blob = craftLegacyV2Blob(secret.toUtf8());
        QVERIFY2(!blob.isEmpty(), "fixture crafting: CryptProtectData must succeed");
        QVERIFY(writeServiceBlob(path, "LegacyV2Svc", blob));

        EncryptedFileSecretStore reader(path);
        QCOMPARE(reader.readSecret("LegacyV2Svc"), secret);
    }

    // ── PGR-20 — Windows default path: AES blobs there are forgeries ────────
    // No legitimate blob on the Windows default path (no key override) ever
    // used the identifier-derived AES key: pre-EC04 0x01 writes could never be
    // reread (EC04), and SEP13:5+ writes 0x04 only. The derivation's inputs —
    // home path, machine id, a constant — are public identifiers, so accepting
    // 0x01/0x03 there lets anyone who can write secrets.enc.json INJECT a
    // secret the store will happily serve. Both versions must be rejected.

    void forgedAesBlobsRejectedOnWindowsDefaultPath() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/forged.json";
        const QByteArray seed = windowsDefaultSeed();

        QVERIFY(writeServiceBlob(path, "ForgedV1Svc",
                                 craftLegacyV1Blob(seed,
                                         "sk-ant-forged-v1-0001")));
        QVERIFY(writeServiceBlob(path, "ForgedV3Svc",
                                 craftAadBlob(seed, "ForgedV3Svc",
                                              "sk-ant-forged-v3-0002")));

        EncryptedFileSecretStore reader(path);  // NO override — the forged path
        QVERIFY2(reader.readSecret("ForgedV1Svc").isEmpty(),
                 "a 0x01 blob keyed by the public identifier seed must be "
                 "rejected on the Windows default path (forgery guard)");
        QVERIFY2(reader.readSecret("ForgedV3Svc").isEmpty(),
                 "a 0x03 blob keyed by the public identifier seed must be "
                 "rejected on the Windows default path (forgery guard)");
        QVERIFY(!reader.hasSecret("ForgedV1Svc"));
        QVERIFY(!reader.hasSecret("ForgedV3Svc"));

        // The override-key path is untouched: the same crafted blobs read
        // under an explicit key remain valid (non-Windows stores too).
        EncryptedFileSecretStore overrideReader(path, seed);
        QCOMPARE(overrideReader.readSecret("ForgedV1Svc"),
                 QString("sk-ant-forged-v1-0001"));
        QCOMPARE(overrideReader.readSecret("ForgedV3Svc"),
                 QString("sk-ant-forged-v3-0002"));
    }

    // PGR-20 migration: 0x02 (DPAPI WITHOUT entry entropy) is the last
    // unbound legacy format, so a legacy blob can still be swapped between
    // entries during migration. The read path therefore re-wraps every 0x02
    // blob as a v3 blob (0x04 default / 0x03 override — entry-bound) on first
    // successful read: the unbound window closes after one read. (Rejecting
    // 0x02 outright would orphan pre-SEP13:5 stores; the format is fully
    // retired once every store has been read once — documented follow-up.)
    void dpapiV2LegacyBlobMigratesToV3OnFirstRead() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.path() + "/dpapi-v2-migrate.json";
        const QString secretA = QStringLiteral("sk-ant-migrate-a-0001");
        const QString secretB = QStringLiteral("sk-ant-migrate-b-0002");

        QVERIFY(writeServiceBlob(path, "MigSvcA", craftLegacyV2Blob(secretA.toUtf8())));
        QVERIFY(writeServiceBlob(path, "MigSvcB", craftLegacyV2Blob(secretB.toUtf8())));

        EncryptedFileSecretStore store(path);
        // First read returns the secret AND re-wraps the blob in place.
        QCOMPARE(store.readSecret("MigSvcA"), secretA);
        QCOMPARE(store.readSecret("MigSvcB"), secretB);
        QCOMPARE(int(static_cast<quint8>(readServiceBlob(path, "MigSvcA").at(0))),
                 0x04);
        QCOMPARE(int(static_cast<quint8>(readServiceBlob(path, "MigSvcB").at(0))),
                 0x04);

        // Second read still works — now through the entry-bound v3 format.
        QCOMPARE(store.readSecret("MigSvcA"), secretA);
        QCOMPARE(store.readSecret("MigSvcB"), secretB);

        // Post-migration the entries are bound: swapping them fails loudly.
        QVERIFY(swapServiceBlobs(path, "MigSvcA", "MigSvcB"));
        QVERIFY2(store.readSecret("MigSvcA").isEmpty(),
                 "a migrated blob moved to another entry must fail loudly");
        QVERIFY2(store.readSecret("MigSvcB").isEmpty(),
                 "the other migrated blob must fail loudly too");
        QVERIFY(swapServiceBlobs(path, "MigSvcA", "MigSvcB"));
        QCOMPARE(store.readSecret("MigSvcA"), secretA);
        QCOMPARE(store.readSecret("MigSvcB"), secretB);
    }
    // ── PGR-26 — API-key credentials must not roam beyond this machine ──────
    // CRED_PERSIST_ENTERPRISE roams the credential with roaming profiles to
    // every machine the user logs into — wider exposure than a desktop app's
    // API keys need. Pin the persistence scope through a REAL vault write
    // (synthetic credential, deleted afterwards): CredReadW reports the
    // persistence that was actually stored, so this is machine inspection,
    // not a style check.
    void credentialPersistIsLocalMachine() {
        CredentialManager mgr;
        const QString service = QStringLiteral("PersistProbeSvc");
        const QString secret  = QStringLiteral("sk-ant-persist-probe-fake-0001");
        if (!mgr.storeKey(service, secret))
            QSKIP("Credential Manager write unavailable on this machine");
        const std::wstring target =
            QStringLiteral("GlyphPDF.AI.PersistProbeSvc").toStdWString();
        PCREDENTIALW pcred = nullptr;
        QVERIFY2(CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &pcred) && pcred,
                 "the probed credential must be readable from the vault");
        const DWORD persist = pcred->Persist;
        CredFree(pcred);
        QVERIFY(mgr.deleteKey(service));  // clean the real user state
        QVERIFY2(persist == CRED_PERSIST_LOCAL_MACHINE,
                 "API-key credentials must persist per-machine "
                 "(CRED_PERSIST_LOCAL_MACHINE), not roam enterprise-wide");
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
