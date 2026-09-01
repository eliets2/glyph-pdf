// SPDX-License-Identifier: Apache-2.0
// §9.13 F2 regression test (audit 2026-07-01): Compress "Strip metadata" (and
// every other optimize phase) on a SIGNED document must be refused. The
// optimize path writes via writeUpdate(), which routes signed documents to an
// incremental SaveUpdate — the "stripped" metadata, attachments and XMP stay
// physically recoverable in revision 1 while the dialog reports success (and
// a size that actually GROWS). Same guarantee class as the ER-2 redaction
// guard, so the engine must refuse the same way.
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
static const QString kP12Pass  = QStringLiteral("test");

#define REQUIRE_FIXTURES() \
    do { \
        if (!QFileInfo::exists(kP12Path) || !QFileInfo::exists(kInputPdf)) { \
            QSKIP("Signing fixtures missing — skipping signed-optimize guard test. " \
                  "Run cmake -P tests/fixtures/signing/generate_fixtures.cmake to create them."); \
        } \
    } while(0)

class TestOptimizeSignedGuard : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Failed to create temp directory");
    }

    void optimizeOnSignedDocumentIsRefused() {
        REQUIRE_FIXTURES();

        QString signedPdf = m_tmpDir.filePath("signed_for_optimize.pdf");
        SignatureManager mgr;
        bool signedOk = (mgr.signDocument(kInputPdf, signedPdf, kP12Path, kP12Pass,
                                          "OptimizeGuardTest", "") == SignOutcome::Success);
        QVERIFY2(signedOk, "signDocument should succeed with valid P12");
        QVERIFY2(QFileInfo::exists(signedPdf), "Signed PDF must exist");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(signedPdf));
        QVERIFY2(engine.hasPdfSignatures(), "Engine must detect the signature");

        OptimizeOptions opts;
        opts.downsampleImages = true;
        opts.stripMetadata = true;   // the promise broken by revision retention
        opts.deduplicateImages = true;
        QString out = m_tmpDir.filePath("signed_optimized.pdf");
        QVERIFY2(!engine.optimizeDocument(out, opts),
                 "optimizeDocument must refuse on a signed document — incremental "
                 "save keeps the stripped metadata recoverable in revision 1");
        QVERIFY2(!QFileInfo::exists(out),
                 "a refused optimize must not have written an output file");
    }

    void unsignedDocumentStillOptimizes() {
        REQUIRE_FIXTURES();

        // Control: the guard must be signature-specific, not a blanket refusal.
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(kInputPdf));
        QVERIFY2(!engine.hasPdfSignatures(), "control fixture must be unsigned");

        OptimizeOptions opts;
        opts.downsampleImages = false;
        opts.stripMetadata = true;
        opts.deduplicateImages = false;
        QString out = m_tmpDir.filePath("unsigned_optimized.pdf");
        QVERIFY2(engine.optimizeDocument(out, opts),
                 "an unsigned document must still optimize");
        QVERIFY2(QFileInfo::exists(out), "unsigned optimize must write its output");
    }
};

QTEST_MAIN(TestOptimizeSignedGuard)
#include "TestOptimizeSignedGuard.moc"
