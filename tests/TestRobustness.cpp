#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"

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
};

QTEST_GUILESS_MAIN(TestRobustness)
#include "TestRobustness.moc"
