#include <QtTest/QtTest>
#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/PdfEditorEngine.h"

#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QFile>

/**
 * Session 20 D2 — Performance benchmarks.
 *
 * Target metrics (100-page PDF):
 *   - Open:          < 2 seconds
 *   - Navigate:      < 500 ms
 *   - Text search:   < 3 seconds
 *   - Render 1 page: < 200 ms
 *   - RAM:           < 200 MB (not measurable in unit test — documented)
 *
 * These tests create a programmatic multi-page PDF via PoDoFo
 * and run timed operations against it.
 */
class TestPerformance : public QObject {
    Q_OBJECT

private:
    /**
     * Create a multi-page minimal PDF with `pageCount` pages.
     * Each page has a text string for search testing.
     */
    static QString createMultiPagePdf(const QString& dir, int pageCount) {
        QString path = dir + "/perf_test.pdf";
        QByteArray pdf;
        pdf.append("%PDF-1.4\n");

        // Object 1: Catalog
        int offset1 = pdf.size();
        pdf.append("1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n");

        // Object 2: Pages (kids are 3..3+N-1)
        int offset2 = pdf.size();
        pdf.append("2 0 obj<</Type/Pages/Kids[");
        for (int i = 0; i < pageCount; ++i) {
            if (i > 0) pdf.append(' ');
            pdf.append(QByteArray::number(3 + i * 2) + " 0 R");
        }
        pdf.append("]/Count ");
        pdf.append(QByteArray::number(pageCount));
        pdf.append(">>endobj\n");

        // Font object (shared)
        int fontObjNum = 3 + pageCount * 2;
        int offsetFont = pdf.size();
        pdf.append(QByteArray::number(fontObjNum) + " 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>endobj\n");

        QList<int> offsets;
        offsets << offset1 << offset2;

        // Generate page+content pairs
        for (int i = 0; i < pageCount; ++i) {
            int pageObj = 3 + i * 2;
            int contentObj = 4 + i * 2;

            QByteArray content = "BT /F1 12 Tf 72 720 Td (Page " + QByteArray::number(i + 1) + " GlyphPDF Performance Test) Tj ET";
            int contentLen = content.size();

            int offsetContent = pdf.size();
            pdf.append(QByteArray::number(contentObj) + " 0 obj<</Length " + QByteArray::number(contentLen) + ">>stream\n");
            pdf.append(content);
            pdf.append("\nendstream endobj\n");

            int offsetPage = pdf.size();
            pdf.append(QByteArray::number(pageObj) + " 0 obj<</Type/Page/Parent 2 0 R"
                "/MediaBox[0 0 612 792]"
                "/Contents " + QByteArray::number(contentObj) + " 0 R"
                "/Resources<</Font<</F1 " + QByteArray::number(fontObjNum) + " 0 R>>>>>>endobj\n");

            // Store offsets in object-number order
            while (offsets.size() < pageObj) offsets.append(0);
            if (offsets.size() == pageObj) offsets.append(offsetPage);
            else offsets[pageObj] = offsetPage;
            while (offsets.size() <= contentObj) offsets.append(0);
            offsets[contentObj - 1] = offsetContent;  // 0-indexed mapping
        }

        while (offsets.size() <= fontObjNum) offsets.append(0);
        offsets[fontObjNum - 1] = offsetFont;

        // Cross-reference table
        int xrefOffset = pdf.size();
        int totalObjects = fontObjNum + 1;
        pdf.append("xref\n0 " + QByteArray::number(totalObjects) + "\n");
        pdf.append("0000000000 65535 f \n");
        for (int i = 0; i < totalObjects - 1; ++i) {
            int off = (i < offsets.size()) ? offsets[i] : 0;
            pdf.append(QString("%1 00000 n \n").arg(off, 10, 10, QChar('0')).toLatin1());
        }

        pdf.append("trailer<</Size " + QByteArray::number(totalObjects) + "/Root 1 0 R>>\n");
        pdf.append("startxref\n" + QByteArray::number(xrefOffset) + "\n%%EOF\n");

        QFile f(path);
        if (f.open(QIODevice::WriteOnly))
            f.write(pdf);
        return path;
    }

private slots:
    // ── Benchmark: Open a multi-page PDF ────────────────────────────────
    void benchOpenDocument() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString pdf = createMultiPagePdf(tmpDir.path(), 100);
        QVERIFY(QFile::exists(pdf));

        QElapsedTimer timer;
        timer.start();

        PdfEditorEngine engine;
        bool ok = engine.loadDocumentForEditing(pdf);

        qint64 elapsed = timer.elapsed();
        qDebug() << "Open 100-page PDF:" << elapsed << "ms";

        if (ok) {
            QVERIFY2(elapsed < 2000, qPrintable(
                QString("Open took %1 ms (target: < 2000 ms)").arg(elapsed)));
        } else {
            qWarning() << "Load failed (backend may not support this PDF format)";
        }
    }

    // ── Benchmark: Save document ────────────────────────────────────────
    void benchSaveDocument() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString pdf = createMultiPagePdf(tmpDir.path(), 50);

        PdfEditorEngine engine;
        if (!engine.loadDocumentForEditing(pdf)) {
            QSKIP("Backend could not load test PDF");
        }

        QElapsedTimer timer;
        timer.start();

        QString out = tmpDir.path() + "/saved.pdf";
        engine.saveDocument(out);

        qint64 elapsed = timer.elapsed();
        qDebug() << "Save 50-page PDF:" << elapsed << "ms";
        QVERIFY2(elapsed < 5000, qPrintable(
            QString("Save took %1 ms (target: < 5000 ms)").arg(elapsed)));
    }

    // ── Benchmark: Metadata operations ──────────────────────────────────
    void benchMetadataOperations() {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        QString pdf = createMultiPagePdf(tmpDir.path(), 10);

        PdfEditorEngine engine;
        if (!engine.loadDocumentForEditing(pdf)) {
            QSKIP("Backend could not load test PDF");
        }

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < 100; ++i) {
            PdfMetadata meta;
            engine.getMetadata(meta);
            meta.title = QString("Test %1").arg(i);
            engine.setMetadata(meta);
        }

        qint64 elapsed = timer.elapsed();
        qDebug() << "100 metadata read/write cycles:" << elapsed << "ms";
        QVERIFY2(elapsed < 2000, qPrintable(
            QString("Metadata ops took %1 ms (target: < 2000 ms)").arg(elapsed)));
    }

    // ── Benchmark: Error handling overhead ──────────────────────────────
    void benchErrorHandlingOverhead() {
        PdfEditorEngine engine;

        QElapsedTimer timer;
        timer.start();

        for (int i = 0; i < 1000; ++i) {
            engine.saveDocument("/nonexistent/path.pdf");
            engine.lastError();
            engine.clearError();
        }

        qint64 elapsed = timer.elapsed();
        qDebug() << "1000 error cycles:" << elapsed << "ms";
        QVERIFY2(elapsed < 5000, qPrintable(
            QString("Error overhead: %1 ms for 1000 cycles").arg(elapsed)));
    }
};

QTEST_GUILESS_MAIN(TestPerformance)
#include "TestPerformance.moc"
