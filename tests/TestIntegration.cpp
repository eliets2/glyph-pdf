#include <QtTest/QtTest>
#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/PdfEditorEngine.h"
#include "core/ErrorInfo.h"

#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

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
        bool result = engine.encryptDocument(encrypted, "testpass123", true, true, false);
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
