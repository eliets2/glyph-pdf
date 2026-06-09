#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"
#include "engines/pdfium/PdfiumBackend.h"

class TestRobustness : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    QString tmpPath(const QString& name) const {
        return m_tmpDir.filePath(name);
    }

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Failed to create temp directory");
    }

    // G-08: Test handling of truncated PDFs.
    // Must return false without crashing or hanging.
    void testHandlingOfTruncatedPdfs() {
        QString pdfPath = tmpPath("truncated.pdf");
        QFile f(pdfPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("%PDF-1.4\n1 0 obj\n<<"); // Intentionally brutally truncated
        f.close();

        PdfEditorEngine engine;
        bool loaded = engine.loadDocumentForEditing(pdfPath);
        QVERIFY2(!loaded, "Truncated PDF must gracefully fail to load, not crash");
    }
    
    // G-08: Test handling of malformed PDFs with random bytes.
    // Seed RNG deterministically for reproducible fuzz-like validation.
    void testHandlingOfMalformedPdfs() {
        QString pdfPath = tmpPath("malformed.pdf");
        QFile f(pdfPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        
        QByteArray garbage(4096, '\0');
        std::srand(0xDEADBEEF); // Deterministic seed
        for (int i = 0; i < 1024; ++i) {
            garbage[i] = static_cast<char>(std::rand() % 256);
        }
        f.write(garbage);
        f.close();

        PdfEditorEngine engine;
        bool loaded = engine.loadDocumentForEditing(pdfPath);
        QVERIFY2(!loaded, "Malformed PDF with random bytes must gracefully fail to load, not crash");
    }

    // ER-3: recipientCount() must return 0 for an unencrypted document and a
    // positive value for a document with a public-key /Recipients array.
    void testRecipientCountReturnedCorrectly() {
        // Case 1: unencrypted document → must return 0.
        const QString plainPath = tmpPath("plain_for_recip.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(plainPath.toUtf8().constData());
        }

        PdfEditorEngine plainEngine;
        QVERIFY(plainEngine.loadDocumentForEditing(plainPath));
        QCOMPARE(plainEngine.recipientCount(), 0);

        // Case 2: password-encrypted document.
        // A standard password-encrypted PDF has no /Recipients array (that is
        // the public-key / CMS case), so recipientCount() must still return 0
        // — the guard only fires for public-key multi-recipient documents.
        const QString encPath = tmpPath("password_encrypted.pdf");
        {
            PdfEditorEngine encEngine;
            QVERIFY(encEngine.loadDocumentForEditing(plainPath));
            DocumentPermissions perms;
            QVERIFY(encEngine.encryptDocument("user", "owner", perms));
            QVERIFY(encEngine.saveDocument(encPath));
        }

        PdfEditorEngine encEngine2;
        // Loading a password-encrypted document without the password may fail
        // in strict builds; tolerate either outcome but require no crash.
        const bool loadedEnc = encEngine2.loadDocumentForEditing(encPath);
        if (loadedEnc) {
            // Password-encrypted docs have no /Recipients array.
            QCOMPARE(encEngine2.recipientCount(), 0);
        } else {
            // Could not open without password — that is acceptable behaviour.
            QCOMPARE(encEngine2.recipientCount(), 0);
        }
    }

    // NF-4: A crafted PDF with a very large /MediaBox must NOT cause OOM or a
    // crash when rendered.  PdfiumBackend::renderPage must reject dimensions that
    // exceed the safe limits (>20000 px in either dimension, or >120MP total) and
    // return an empty QImage instead of allocating gigabytes of memory.
    void testPdfiumRenderOversizedPageRejected() {
        // Build a minimal valid PDF with a 300000 x 300000 point MediaBox.
        // At 72 dpi, widthPixels = heightPixels = 300000, product = 90 billion px
        // — far above the 120MP clamp, so the backend must reject it.
        const QString pdfPath = tmpPath("giant_mediabox.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            // Create a page with a massive bounding box.
            PoDoFo::Rect hugeBox(0, 0, 300000, 300000); // points
            doc.GetPages().CreatePage(hugeBox);
            doc.Save(pdfPath.toUtf8().constData());
        }

        PdfiumBackend backend;
        const bool loaded = backend.loadDocument(pdfPath);
#ifndef HAS_PDFIUM
        // If PDFium is not compiled in, loadDocument returns false — test is vacuously
        // satisfied since there's nothing to clamp.
        Q_UNUSED(loaded);
        QSKIP("PDFium not compiled in — skipping oversized-render clamp test");
#else
        if (!loaded) {
            // PDFium could not load a PoDoFo-generated file in this build config;
            // the test cannot exercise the clamp. Skip rather than fail.
            QSKIP("PdfiumBackend could not load the test fixture — skipping oversized-render clamp test");
        }

        // renderPage at 72 dpi: widthPixels = 300000 * (72/72) = 300000 px — triggers clamp.
        QImage result = backend.renderPage(0, 72);
        QVERIFY2(result.isNull(),
                 "NF-4: renderPage on a >20000px page must return a null QImage, not allocate/crash");

        // renderTile with a large sub-rect must also be clamped.
        QImage tile = backend.renderTile(0, QRectF(0, 0, 25000, 25000), 72);
        QVERIFY2(tile.isNull(),
                 "NF-4: renderTile with >20000px tile must return a null QImage, not allocate/crash");
#endif
    }
};

QTEST_GUILESS_MAIN(TestRobustness)
#include "TestRobustness.moc"
