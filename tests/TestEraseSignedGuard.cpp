// SPDX-License-Identifier: Apache-2.0
// Audit 9.2 P0 regression test (ER-2): the eraser must NOT bypass the
// signed-document guard. PdfEditorEngine::deleteObjectAt routes through the
// backend's incremental PoDoFo SaveUpdate, which would leave excised bytes
// recoverable in revision 1 of a signed PDF. The engine must refuse to erase
// on a signed document (mirroring applyRedactions/applyMarkRedactions), so the
// caller routes the user to save an unsigned copy first.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>

#include "engines/PdfEditorEngine.h"
#include "engines/SignatureManager.h"

#ifdef SOURCE_DIR
static const QString kFixtureDir = QStringLiteral(SOURCE_DIR "/tests/fixtures/signing");
#else
static const QString kFixtureDir = QStringLiteral("tests/fixtures/signing");
#endif
static const QString kP12Path  = kFixtureDir + "/test_signer.p12";
static const QString kInputPdf = kFixtureDir + "/test_input.pdf";
static const QString kCaPath   = kFixtureDir + "/test_ca.pem";
static const QString kP12Pass  = QStringLiteral("test");

#define REQUIRE_FIXTURES() \
    do { \
        if (!QFileInfo::exists(kP12Path) || !QFileInfo::exists(kInputPdf) || !QFileInfo::exists(kCaPath)) { \
            QSKIP("Signing fixtures missing — skipping signed-erase guard test. " \
                  "Run cmake -P tests/fixtures/signing/generate_fixtures.cmake to create them."); \
        } \
    } while(0)

class TestEraseSignedGuard : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Failed to create temp directory");
    }

    void eraseOnSignedDocumentIsRefused() {
        REQUIRE_FIXTURES();

        // Sign a copy of the input PDF so we have a document with a real signature.
        QString signedPdf = m_tmpDir.filePath("signed_for_erase.pdf");
        SignatureManager mgr;
        bool signedOk = (mgr.signDocument(kInputPdf, signedPdf, kP12Path, kP12Pass,
                                          "EraseGuardTest", "") == SignOutcome::Success);
        QVERIFY2(signedOk, "signDocument should succeed with valid P12");
        QVERIFY2(QFileInfo::exists(signedPdf), "Signed PDF must exist");

        // Snapshot the signed bytes before any erase attempt.
        QFile before(signedPdf);
        QVERIFY(before.open(QIODevice::ReadOnly));
        QByteArray beforeBytes = before.readAll();
        before.close();
        QVERIFY2(!beforeBytes.isEmpty(), "Signed PDF must not be empty");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(signedPdf));
        QVERIFY2(engine.hasPdfSignatures(), "Engine must detect the signature");

        // Erasing on a signed document must be REFUSED (ER-2 guard), not silently
        // written via incremental save.
        bool ok = engine.deleteObjectAt(0, QPointF(100, 100));
        QVERIFY2(!ok, "deleteObjectAt must refuse to erase on a signed document");

        // The file must be byte-for-byte unchanged — no incremental revision was
        // appended, so no excised content can be recovered from revision history.
        QFile after(signedPdf);
        QVERIFY(after.open(QIODevice::ReadOnly));
        QByteArray afterBytes = after.readAll();
        after.close();
        QCOMPARE(afterBytes, beforeBytes);
    }
};

QTEST_MAIN(TestEraseSignedGuard)
#include "TestEraseSignedGuard.moc"