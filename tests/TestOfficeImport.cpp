#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QImage>
#include <QPainter>
#include <QProcess>

#include "engines/ConversionManager.h"
#include "core/interfaces/IConversionEngine.h"
#include <podofo/podofo.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QString sofficePath()
{
#ifdef HAS_LIBREOFFICE
    return QString::fromLatin1(LIBREOFFICE_SOFFICE_PATH);
#else
    // Check common Windows install locations at runtime
    QStringList candidates = {
        "C:/Program Files/LibreOffice/program/soffice.exe",
        "C:/Program Files (x86)/LibreOffice/program/soffice.exe",
    };
    for (const QString& p : candidates)
        if (QFileInfo::exists(p)) return p;
    return QString();
#endif
}

// Create a minimal 3-page synthetic PNG image for testing.
static bool writeTestPng(const QString& path, QColor color, int w = 100, int h = 80)
{
    QImage img(w, h, QImage::Format_RGB888);
    img.fill(color);
    QPainter p(&img);
    p.setPen(Qt::black);
    p.drawText(10, 40, path.section('/', -1));
    p.end();
    return img.save(path, "PNG");
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestOfficeImport : public QObject {
    Q_OBJECT

private slots:

    // D3 — convertImagesToPdf: 3 PNG inputs → 3-page PDF
    void testImagesToPdf_threePages()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        QStringList pngPaths;
        for (int i = 0; i < 3; ++i) {
            const QString path = tmp.filePath(QString("img%1.png").arg(i));
            QVERIFY(writeTestPng(path, QColor(50 + i * 60, 100, 200)));
            pngPaths << path;
        }

        const QString outPdf = tmp.filePath("out.pdf");
        ConversionManager mgr;
        QVERIFY(mgr.convertImagesToPdf(pngPaths, outPdf));
        QVERIFY(QFileInfo::exists(outPdf));
        QVERIFY(QFileInfo(outPdf).size() > 0);

        // Verify page count via PoDoFo
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(outPdf.toUtf8().constData());
            QCOMPARE(static_cast<int>(doc.GetPages().GetCount()), 3);

            // Each page should have at least one XObject in its resources
            for (unsigned pg = 0; pg < doc.GetPages().GetCount(); ++pg) {
                const PoDoFo::PdfPage& page = doc.GetPages().GetPageAt(pg);
                const PoDoFo::PdfDictionary& dict = page.GetDictionary();
                auto* res = dict.FindKey("Resources");
                bool hasXObj = false;
                if (res && res->GetDictionary().HasKey("XObject"))
                    hasXObj = true;
                QVERIFY2(hasXObj,
                    qPrintable(QString("Page %1 has no XObject resource").arg(pg)));
            }
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("PoDoFo load failed: %1").arg(e.what())));
        }
    }

    // D3 — empty input list should return false gracefully
    void testImagesToPdf_emptyList()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        ConversionManager mgr;
        QVERIFY(!mgr.convertImagesToPdf({}, tmp.filePath("out.pdf")));
    }

    // D2 — OfficeToPdf via convertTo: QSKIP when soffice unavailable
    void testOfficeToPdf_realConversion()
    {
        const QString soffice = sofficePath();
        if (soffice.isEmpty() || !QFileInfo::exists(soffice)) {
            QSKIP("LibreOffice (soffice) not available at runtime — skipping real-conversion test");
        }

        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Write a trivial RTF document (no binary fixtures needed)
        const QString rtfPath = tmp.filePath("test.rtf");
        QFile f(rtfPath);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("{\\rtf1\\ansi\\deff0 {\\fonttbl {\\f0 Times New Roman;}} "
                "{\\pard Hello World\\par}}");
        f.close();

        const QString outPdf = tmp.filePath("test.pdf");
        ConversionManager mgr;
        const bool ok = mgr.convertTo(rtfPath, outPdf, IConversionEngine::TargetFormat::OfficeToPdf);
        QVERIFY2(ok, "OfficeToPdf conversion should succeed with a valid RTF input");
        QVERIFY(QFileInfo::exists(outPdf));
        QVERIFY(QFileInfo(outPdf).size() > 0);

        // Verify output is parseable as PDF
        try {
            PoDoFo::PdfMemDocument doc;
            doc.Load(outPdf.toUtf8().constData());
            QVERIFY(doc.GetPages().GetCount() >= 1);
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QString("Output PDF not valid: %1").arg(e.what())));
        }
    }

    // D2 — OfficeToPdf with non-existent file returns false gracefully
    void testOfficeToPdf_missingFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        ConversionManager mgr;
        // File does not exist — should return false without crashing
        const bool ok = mgr.convertTo("/nonexistent/path/test.docx",
                                      tmp.filePath("out.pdf"),
                                      IConversionEngine::TargetFormat::OfficeToPdf);
        QVERIFY2(!ok, "convertTo(OfficeToPdf) should return false for missing input");
    }

    // D2 — OfficeToPdf: no-op when LibreOffice not configured at build time
    void testOfficeToPdf_noLibreOffice()
    {
#ifdef HAS_LIBREOFFICE
        QSKIP("HAS_LIBREOFFICE defined — skipping no-op test");
#else
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        // Create a dummy .docx file
        const QString docxPath = tmp.filePath("test.docx");
        QFile f(docxPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("PK");   // minimal ZIP header
        f.close();

        ConversionManager mgr;
        const bool ok = mgr.convertTo(docxPath, tmp.filePath("out.pdf"),
                                      IConversionEngine::TargetFormat::OfficeToPdf);
        QVERIFY2(!ok, "OfficeToPdf should return false when HAS_LIBREOFFICE is not defined");
#endif
    }
};

#include "TestOfficeImport.moc"
QTEST_MAIN(TestOfficeImport)
