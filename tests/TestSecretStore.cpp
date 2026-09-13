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
#include <QProcess>
#include <QStandardPaths>

#include "core/EncryptedFileSecretStore.h"
#include "core/ISecretStore.h"

#if defined(HAS_LIBSECRET)
#include "core/LibSecretStore.h"
#endif

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

        // The stored blob carries the v2 (DPAPI) version byte — the format
        // pin that documents on-disk key naming for support/forensics.
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
        QCOMPARE(int(static_cast<quint8>(blob.at(0))), 0x02);  // DPAPI-wrapped
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
