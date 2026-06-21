// SPDX-License-Identifier: Apache-2.0
//
// AR-10 D4 — proves the encrypted-file secret store is REAL and NEVER
// silent-fails: a stored secret durably round-trips, an unwritable target is
// reported as a failure (not a silent drop), the plaintext is never on disk,
// and tampered / wrong-key blobs do not decrypt to garbage.

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "core/EncryptedFileSecretStore.h"
#include "core/ISecretStore.h"

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
};

QTEST_GUILESS_MAIN(TestSecretStore)
#include "TestSecretStore.moc"
