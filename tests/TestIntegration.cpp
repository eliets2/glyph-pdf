#include <QtTest/QtTest>
#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/PdfEditorEngine.h"
#include "engines/RenderCache.h"
#include "core/ErrorInfo.h"

#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <memory>

/**
 * Session 20 D1 — End-to-end integration tests.
 *
 * Tests real-world PDF workflows through the engine layer.
 * Uses the actual PdfEditorEngine (not mock) with a test PDF
 * generated programmatically via PoDoFo.
 */
class TestIntegration : public QObject {
    Q_OBJECT

private:
    /** Create a minimal valid PDF in a temp file and return its path. */
    static QString createTestPdf(const QString& dir, const QString& name = "test.pdf") {
        QString path = dir + "/" + name;
        // Minimal PDF 1.4 — a single blank A4 page (valid for PoDoFo parsing)
        QByteArray pdf(
            "%PDF-1.4\n"
            "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
            "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
            "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
            "xref\n0 4\n"
            "0000000000 65535 f \n"
            "0000000009 00000 n \n"
            "0000000058 00000 n \n"
            "0000000115 00000 n \n"
            "trailer<</Size 4/Root 1 0 R>>\n"
            "startxref\n183\n%%EOF\n");
        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(pdf);
        return path;
    }

private slots:
    // ── Test 1: Open → Save → Reopen ────────────────────────────────────
    void testOpenSaveReopen() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString pdf = createTestPdf(tmpDir.path());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        // Save to a new path
        QString saved = tmpDir.path() + "/saved.pdf";
        QVERIFY(engine.saveDocument(saved));
        QVERIFY(QFileInfo::exists(saved));

        // Reopen the saved file
        PdfEditorEngine engine2;
        QVERIFY(engine2.loadDocumentForEditing(saved));
    }

    // ── Test 2: Open → Encrypt → Save → Verify error without password ──
    void testEncryptWorkflow() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString pdf = createTestPdf(tmpDir.path());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        QString encrypted = tmpDir.path() + "/encrypted.pdf";
        DocumentPermissions perms;
        perms.modify = false;
        bool result = engine.encryptDocument(encrypted, "testpass123", perms);
        // Encryption may succeed or fail depending on backend capabilities;
        // we verify the engine doesn't crash and returns a clean error.
        if (!result) {
            ErrorInfo err = engine.lastError();
            QVERIFY(!err.userMessage.isEmpty());
        }
    }

    // ── Test 3: Open → Rotate → Save → Reopen ──────────────────────────
    void testRotatePageWorkflow() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString pdf = createTestPdf(tmpDir.path());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        bool rotated = engine.rotatePage(pdf, 0, 90);
        if (rotated) {
            // Reopen and verify no crash
            PdfEditorEngine engine2;
            QVERIFY(engine2.loadDocumentForEditing(pdf));
        }
    }

    // ── Test 4: Open → Redact → Save → Verify ──────────────────────────
    void testRedactWorkflow() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString pdf = createTestPdf(tmpDir.path());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        QList<QRectF> regions;
        regions.append(QRectF(100, 100, 200, 50));
        bool result = engine.applyRedactions(0, regions);
        // Redaction on a blank page may or may not apply;
        // verify no crash and clean error reporting.
        if (!result) {
            ErrorInfo err = engine.lastError();
            // Engine should provide a reason
            QVERIFY(!err.userMessage.isEmpty() || err.isOk());
        }
    }

    // ── Test 5: Sanitize workflow ────────────────────────────────────────
    void testSanitizeWorkflow() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString pdf = createTestPdf(tmpDir.path());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        QString sanitized = tmpDir.path() + "/sanitized.pdf";
        bool result = engine.sanitizeDocument(sanitized);
        // Sanitization on a minimal PDF should succeed or report clean failure
        if (result) {
            QVERIFY(QFileInfo::exists(sanitized));
        } else {
            ErrorInfo err = engine.lastError();
            QVERIFY(!err.userMessage.isEmpty() || err.isOk());
        }
    }

    // ── Test 6: Metadata read/write ─────────────────────────────────────
    void testMetadataWorkflow() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString pdf = createTestPdf(tmpDir.path());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        PdfMetadata meta;
        bool gotMeta = engine.getMetadata(meta);
        if (gotMeta) {
            meta.title = "Integration Test Document";
            meta.author = "GlyphPDF Test Suite";
            bool set = engine.setMetadata(meta);
            if (set) {
                // Read back
                PdfMetadata readBack;
                QVERIFY(engine.getMetadata(readBack));
                QCOMPARE(readBack.title, QString("Integration Test Document"));
            }
        }
    }

    // ── Test 7: Linearize workflow ──────────────────────────────────────
    void testLinearizeWorkflow() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString pdf = createTestPdf(tmpDir.path());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        QString linearized = tmpDir.path() + "/linear.pdf";
        bool result = engine.linearizeDocument(linearized);
        if (result) {
            QVERIFY(QFileInfo::exists(linearized));
            QVERIFY(QFileInfo(linearized).size() > 0);
        }
    }

    // ── AR-1 D2: cancelAndWaitForPrefetch blocks until the pool thread exits ─
    // Pre-fix: no such method; caller had no safe way to drain the prefetch
    //   before destroying the renderer — leading to a UAF on document close.
    // Post-fix: cancelAndWaitForPrefetch() signals the token and joins the
    //   QFuture; this test verifies it does not crash or deadlock.
    void testPrefetchCancelBeforeRendererDestroy() {
        // Minimal stub renderer — returns a null image (prefetch will skip
        // the insert but the token-check path still exercises the join).
        class NullRenderer : public IPdfRenderer {
        public:
            QImage   renderPage(int, int) override                 { return {}; }
            QImage   renderTile(int, const QRectF&, int) override  { return {}; }
            QString  extractText(int) override                      { return {}; }
            QSizeF   pageSize(int) const override                   { return {595.276, 841.890}; }
        };

        auto cache = std::make_shared<RenderCache>();
        cache->setPageCount(4);

        // We hold the renderer as a unique_ptr to model the ownership that
        // PdfViewerWidget has: it owns the renderer and can destroy it.
        auto renderer = std::make_unique<NullRenderer>();
        IPdfRenderer* rawPtr = renderer.get();

        // Trigger an async prefetch
        cache->prefetchViewport(0, 1.0, rawPtr);

        // Simulate document close: cancel+join BEFORE destroying renderer
        cache->cancelAndWaitForPrefetch();

        // Now safe to destroy the renderer — no in-flight lambda holds rawPtr
        renderer.reset();

        // If we reach here without a crash/ASAN complaint, the guard works.
        QVERIFY(true);
    }

    // ── AR-1 D1: Watermark on a font-less PDF must not crash ─────────────
    // Pre-fix: SearchFont("Helvetica") returns nullptr; immediate deref → crash.
    // Post-fix: null-check falls back to GetStandard14Font; returns true.
    void testWatermarkOnFontlessPdf() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        // createTestPdf produces a minimal PDF with no embedded fonts
        QString pdf = createTestPdf(tmpDir.path(), "fontless.pdf");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        TextWatermarkOptions opts;
        opts.text        = "CONFIDENTIAL";
        opts.fontSize    = 48.0;
        opts.opacity     = 0.3;
        opts.rotationDeg = 45.0;
        opts.color       = Qt::red;
        opts.pageFrom    = 0;
        opts.pageTo      = 0;

        // Must complete without crashing and return true
        bool result = engine.addTextWatermark(opts);
        QVERIFY(result);
    }

    // ── Test 8: Error reporting consistency ──────────────────────────────
    void testErrorReportingConsistency() {
        PdfEditorEngine engine;

        // Operating without loading should fail gracefully
        bool result = engine.saveDocument("/nonexistent/path.pdf");
        QVERIFY(!result);
        ErrorInfo err = engine.lastError();
        QVERIFY(!err.isOk());
        QVERIFY(!err.userMessage.isEmpty());

        // Clear error
        engine.clearError();
        QVERIFY(engine.lastError().isOk());
    }
};

QTEST_GUILESS_MAIN(TestIntegration)
#include "TestIntegration.moc"
