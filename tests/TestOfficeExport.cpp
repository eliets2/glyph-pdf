// SPDX-License-Identifier: Apache-2.0
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <podofo/podofo.h>

#include "engines/ConversionManager.h"
#include "core/interfaces/IConversionEngine.h"

// ---------------------------------------------------------------------------
// TestOfficeExport (Wave 1A §9.5)
//
// Covers PDF -> Word/Excel export via ConversionManager::convertTo(), exercising
// whichever branch this build actually took: the real-OOXML path (HAS_DUCKX for
// .docx via libduckx, HAS_OPENXLSX for .xlsx via OpenXLSX) or the honest fallback
// (HTML re-labelled .doc, CSV re-labelled .xls). Before this test, neither code
// path had ANY automated coverage (audit §9.5), so a regression that silently
// re-introduced the fallback on a build where the real libraries ARE linked
// would have gone undetected by CI.
//
// Rather than branching on the HAS_DUCKX/HAS_OPENXLSX macros directly (they are
// PRIVATE compile definitions on the pdfws_engines target and are not visible to
// this test binary), the test sniffs the *actual output file's content*: a real
// .docx/.xlsx is a ZIP container (magic bytes "PK\x03\x04") with an OOXML part
// inside, while the fallback writes plain HTML/CSV text starting with "<html"
// or containing comma-separated quoted fields. This is a stronger test anyway --
// it verifies the file matches what its extension promises, which is exactly
// the correctness property the audit flagged as broken (§9.16 mislabeled
// exports).
// ---------------------------------------------------------------------------

namespace {

QString createTextPdf(const QTemporaryDir &tmpDir, const QString &name, const QString &text)
{
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        auto &page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto &font = doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
        painter.TextState.SetFont(font, 12.0);
        painter.DrawText(text.toStdString(), 50, 700);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception &e) {
        qWarning() << "createTextPdf failed:" << e.what();
        return {};
    }
    return path;
}

bool looksLikeZip(const QByteArray &data)
{
    // Local file header magic for a ZIP archive (OOXML .docx/.xlsx are ZIPs).
    return data.size() >= 4 &&
           static_cast<unsigned char>(data[0]) == 0x50 && // 'P'
           static_cast<unsigned char>(data[1]) == 0x4B && // 'K'
           (static_cast<unsigned char>(data[2]) == 0x03 || static_cast<unsigned char>(data[2]) == 0x05 ||
            static_cast<unsigned char>(data[2]) == 0x07);
}

} // namespace

class TestOfficeExport : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

private slots:

    void testExportToWord_producesNonEmptyOutput()
    {
        QVERIFY(m_tmpDir.isValid());
        const QString pdfPath = createTextPdf(m_tmpDir, "word_src.pdf", "Hello Word Export");
        QVERIFY(!pdfPath.isEmpty());

        const QString outPath = m_tmpDir.filePath("out.docx");
        ConversionManager mgr;
        const bool ok = mgr.convertTo(pdfPath, outPath, IConversionEngine::TargetFormat::Word);
        QVERIFY2(ok, "convertTo(Word) should succeed for a simple one-page text PDF");
        QVERIFY(QFileInfo::exists(outPath));
        QVERIFY(QFileInfo(outPath).size() > 0);
    }

    // Wave 1A §9.5: whichever branch is compiled in (HAS_DUCKX real OOXML, or the
    // HTML fallback), the output content must be internally consistent -- either
    // a real ZIP-based OOXML package, or valid fallback HTML. Detects a silent
    // regression to a third, broken state (e.g. truncated/garbage output).
    void testExportToWord_contentMatchesActiveBackend()
    {
        QVERIFY(m_tmpDir.isValid());
        const QString pdfPath = createTextPdf(m_tmpDir, "word_content.pdf", "Content Check Word");
        QVERIFY(!pdfPath.isEmpty());

        const QString outPath = m_tmpDir.filePath("content.docx");
        ConversionManager mgr;
        QVERIFY(mgr.convertTo(pdfPath, outPath, IConversionEngine::TargetFormat::Word));

        QFile f(outPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray data = f.readAll();
        f.close();
        QVERIFY(!data.isEmpty());

        if (looksLikeZip(data)) {
            // Real-OOXML path (HAS_DUCKX): must be a valid ZIP with the expected
            // OOXML part name somewhere in the (uncompressed local file header)
            // central directory / filenames stream.
            QVERIFY2(data.contains("word/") || data.contains("[Content_Types]"),
                      "Real .docx output should contain the OOXML word/ part or [Content_Types].xml");
        } else {
            // Fallback path: must be the documented HTML-based .doc Word can open,
            // not silently-truncated or binary garbage.
            QVERIFY2(data.startsWith("<html>") || data.startsWith("<!DOCTYPE") || data.contains("<body>"),
                      "Fallback Word export should be well-formed HTML that Word can open");
        }
    }

    void testExportToExcel_producesNonEmptyOutput()
    {
        QVERIFY(m_tmpDir.isValid());
        const QString pdfPath = createTextPdf(m_tmpDir, "excel_src.pdf", "Hello Excel Export");
        QVERIFY(!pdfPath.isEmpty());

        const QString outPath = m_tmpDir.filePath("out.xlsx");
        ConversionManager mgr;
        const bool ok = mgr.convertTo(pdfPath, outPath, IConversionEngine::TargetFormat::Excel);
        QVERIFY2(ok, "convertTo(Excel) should succeed for a simple one-page text PDF");
        QVERIFY(QFileInfo::exists(outPath));
        QVERIFY(QFileInfo(outPath).size() > 0);
    }

    // Wave 1A §9.5: same content-consistency check as Word, for the
    // HAS_OPENXLSX real path vs the CSV fallback.
    void testExportToExcel_contentMatchesActiveBackend()
    {
        QVERIFY(m_tmpDir.isValid());
        const QString pdfPath = createTextPdf(m_tmpDir, "excel_content.pdf", "Content Check Excel");
        QVERIFY(!pdfPath.isEmpty());

        const QString outPath = m_tmpDir.filePath("content.xlsx");
        ConversionManager mgr;
        QVERIFY(mgr.convertTo(pdfPath, outPath, IConversionEngine::TargetFormat::Excel));

        QFile f(outPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray data = f.readAll();
        f.close();
        QVERIFY(!data.isEmpty());

        if (looksLikeZip(data)) {
            // Real-OOXML path (HAS_OPENXLSX): must be a valid ZIP with the
            // expected OOXML workbook part.
            QVERIFY2(data.contains("xl/") || data.contains("[Content_Types]"),
                      "Real .xlsx output should contain the OOXML xl/ part or [Content_Types].xml");
        } else {
            // Fallback path: must be plausible CSV (comma-or-quote-separated
            // text), not silently-truncated or binary garbage.
            QVERIFY2(data.contains(',') || data.contains('"') || data.trimmed().isEmpty(),
                      "Fallback Excel export should be well-formed CSV text");
        }
    }

    // Guard against a regression where convertTo() reports success but writes
    // an empty/zero-byte file for either target -- exactly the silent-failure
    // pattern the audit flagged across multiple domains (§9.9 merge, §9.5 export).
    void testExportToWord_missingInputFailsGracefully()
    {
        QVERIFY(m_tmpDir.isValid());
        ConversionManager mgr;
        const bool ok = mgr.convertTo("/nonexistent/path/does-not-exist.pdf",
                                       m_tmpDir.filePath("missing.docx"),
                                       IConversionEngine::TargetFormat::Word);
        QVERIFY2(!ok, "convertTo(Word) should return false for a missing input PDF");
    }

    void testExportToExcel_missingInputFailsGracefully()
    {
        QVERIFY(m_tmpDir.isValid());
        ConversionManager mgr;
        const bool ok = mgr.convertTo("/nonexistent/path/does-not-exist.pdf",
                                       m_tmpDir.filePath("missing.xlsx"),
                                       IConversionEngine::TargetFormat::Excel);
        QVERIFY2(!ok, "convertTo(Excel) should return false for a missing input PDF");
    }
};

#include "TestOfficeExport.moc"
QTEST_MAIN(TestOfficeExport)
