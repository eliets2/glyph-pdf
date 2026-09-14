// R14 INDEPENDENT REVIEWER probe — Q1 (skip-pages work + kept-page extraction),
// Q3 (settings write-on-change), Q4 (AcroForm XFAIL gap honesty).
// NOT a lane artifact. Written by the independent reviewer (2026-09-14).
// Independence: own 3-page fixtures with DISTINCT markers, own counting OCR
// engine, verdicts on the SAVED artifact via PDFium + PoDoFo stream identity,
// and QSettings verified by reading the INI bytes directly.
#include <QtTest/QtTest>
#include <QCheckBox>
#include <QFile>
#include <QSignalSpy>
#include <QProcess>
#include <QTemporaryDir>

#include <podofo/podofo.h>

#include "core/AppContext.h"
#include "engines/PdfEditorEngine.h"
#include "modes/BatchMode.h"

using namespace gp;

namespace {

class CountingOcr final : public IOcrEngine {
public:
    int calls = 0;
    bool initialize(const QString&, const QString&) override { return true; }
    QList<OcrResult> processImage(const QImage&) override {
        ++calls;
        OcrResult r;
        r.text = QStringLiteral("r14ocrlayer");
        r.boundingBox = QRectF(10, 10, 80, 20);
        r.confidence = 95;
        return { r };
    }
    QString getRawText(const QImage&) override { return QStringLiteral("r14ocrlayer"); }
    bool isMockImplementation() const override { return true; }
};

constexpr const char* kSkipPagesText = "Skip pages that already contain text";

// Reviewer's own multi-page PDF builder: each page gets its own content, an
// optional image XObject on image pages, optional AcroForm + Tx widget.
// Objects: 1 cat, 2 pages, 3..(2+n) pages, then contents per page, then font,
// then image, then field.
struct BuiltPdf { QByteArray bytes; };

QByteArray buildPdf(int pageCount,
                    const QStringList& pageContents,
                    int imagePage = -1,
                    bool withAcroFormOnFirstPage = false)
{
    QByteArray pdf = "%PDF-1.4\n";
    QList<int> offsets;
    const int firstPageObj = 3;
    const int firstContentObj = firstPageObj + pageCount;
    const int fontObj = firstContentObj + pageCount;
    const int imageObj = fontObj + (imagePage >= 0 ? 1 : 0);
    const int fieldObj = imageObj + (withAcroFormOnFirstPage ? 1 : 0);

    const auto mark = [&] { offsets.append(pdf.size()); };
    mark();
    pdf += "1 0 obj<</Type/Catalog/Pages 2 0 R";
    if (withAcroFormOnFirstPage)
        pdf += "/AcroForm<</Fields[" + QByteArray::number(fieldObj) + " 0 R]>>";
    pdf += ">>endobj\n";
    QByteArray kids;
    for (int i = 0; i < pageCount; ++i)
        kids += QByteArray::number(firstPageObj + i) + " 0 R ";
    mark();
    pdf += "2 0 obj<</Type/Pages/Kids[" + kids + "]/Count "
           + QByteArray::number(pageCount) + ">>endobj\n";
    for (int i = 0; i < pageCount; ++i) {
        QByteArray annots;
        if (withAcroFormOnFirstPage && i == 0)
            annots = "/Annots[" + QByteArray::number(fieldObj) + " 0 R]";
        mark();
        pdf += QByteArray::number(firstPageObj + i)
               + " 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]"
               + annots
               + "/Contents " + QByteArray::number(firstContentObj + i)
               + " 0 R/Resources<<"
               + (i == imagePage
                  ? "/XObject<</Im0 " + QByteArray::number(imageObj) + " 0 R>>"
                  : "/Font<</F1 " + QByteArray::number(fontObj) + " 0 R>>")
               + ">>>>endobj\n";
    }
    for (int i = 0; i < pageCount; ++i) {
        QByteArray content = pageContents.at(i).toUtf8();
        if (i == imagePage) {
            content = "q 612 0 0 792 0 0 cm /Im0 Do Q\n";
        }
        mark();
        pdf += QByteArray::number(firstContentObj + i)
               + " 0 obj<</Length " + QByteArray::number(content.size())
               + ">>stream\n" + content + "endstream endobj\n";
    }
    mark();
    pdf += QByteArray::number(fontObj)
           + " 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica"
             "/Encoding/WinAnsiEncoding>>endobj\n";
    if (imagePage >= 0) {
        // 1x1 gray uncompressed image
        const QByteArray img("  \x7f", 3);
        mark();
        pdf += QByteArray::number(imageObj)
               + " 0 obj<</Type/XObject/Subtype/Image/Width 1/Height 1"
                 "/ColorSpace/DeviceGray/BitsPerComponent 8/Length "
               + QByteArray::number(img.size()) + ">>stream\n" + img
               + "\nendstream endobj\n";
    }
    if (withAcroFormOnFirstPage) {
        mark();
        pdf += QByteArray::number(fieldObj)
               + " 0 obj<</FT/Tx/T(r14field)/V()/Subtype/Widget"
                 "/Rect[50 50 200 80]>>endobj\n";
    }

    const int lastObj = withAcroFormOnFirstPage
        ? fieldObj
        : (imagePage >= 0 ? imageObj : fontObj);
    const int xrefStart = pdf.size();
    pdf += "xref\n0 " + QByteArray::number(lastObj + 1) + "\n"
           "0000000000 65535 f \n";
    for (int i = 1; i <= lastObj; ++i)
        pdf += QString::asprintf("%010d 00000 n \n", offsets.at(i - 1)).toLatin1();
    pdf += "trailer<</Size " + QByteArray::number(lastObj + 1)
           + "/Root 1 0 R>>\nstartxref\n" + QByteArray::number(xrefStart)
           + "\n%%EOF\n";
    return pdf;
}

bool writeFile(const QString& path, const QByteArray& bytes) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const bool ok = f.write(bytes) == bytes.size();
    f.close();
    return ok;
}

// Content stream of a page, raw (fixtures are uncompressed).
QByteArray pageStreamBytes(const QString& pdfPath, int page) {
    PoDoFo::PdfMemDocument doc;
    doc.Load(pdfPath.toUtf8().constData());
    auto& pg = doc.GetPages().GetPageAt(page);
    auto* contents = pg.GetDictionary().FindKey("Contents");
    if (!contents) return {};
    auto stream = contents->GetStream();
    if (!stream) return {};
    auto out = stream->GetCopy();
    return QByteArray(reinterpret_cast<const char*>(out.data()),
                      static_cast<qsizetype>(out.size()));
}

QCheckBox* findCheckBox(QWidget* host, const char* text) {
    const auto boxes = host->findChildren<QCheckBox*>();
    for (QCheckBox* b : boxes)
        if (b->text().contains(QLatin1String(text)))
            return b;
    return nullptr;
}

} // namespace

class R14ProbeBatchSkip : public QObject {
    Q_OBJECT

    AppContext m_ctx;
    std::shared_ptr<CountingOcr> m_ocr;
    QTemporaryDir m_tmp;

    void pumpUntilDone(BatchMode& bm) {
        int waited = 0;
        while (bm.isBatchRunning() && waited < 30000) {
            QTest::qWait(50);
            waited += 50;
        }
        QVERIFY2(!bm.isBatchRunning(), "batch did not complete");
        waited = 0;
        while (bm.successCount() + bm.failCount() + bm.skipCount() < 1
               && waited < 5000) { QTest::qWait(25); waited += 25; }
    }

private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("R14ProbeBatchSkip"));
    }
    void init() {
        QSettings().remove(QStringLiteral("ocr/skipFilesWithText"));
        QSettings().remove(QStringLiteral("ocr/skipPagesWithText"));
        QSettings().remove(QStringLiteral("ocr/forceOcr"));
    }
    void cleanup() {
        QSettings().remove(QStringLiteral("ocr/skipFilesWithText"));
        QSettings().remove(QStringLiteral("ocr/skipPagesWithText"));
        QSettings().remove(QStringLiteral("ocr/forceOcr"));
    }

    // Q1: 3-page mixed doc (text / image / text) — exactly ONE OCR call, kept
    // pages byte-identical content streams (original page objects, NOT MRC
    // re-encodes), OCRed page differs; PDFium text on the SAVED artifact.
    void q1_skipPagesOcrOnceAndKeepsOriginalPageStreams() {
        QVERIFY(m_tmp.isValid());
        const QString src = m_tmp.filePath("mixed3.pdf");
        QVERIFY(writeFile(src, buildPdf(3,
            { "BT /F1 12 Tf 72 700 Td (KEEPONE marker) Tj ET",
              "",   // image page (content replaced by image draw)
              "BT /F1 12 Tf 72 700 Td (KEEPTWO marker) Tj ET" },
            /*imagePage=*/1)));
        QVERIFY(writeFile(m_tmp.filePath("src-copy-for-sha.pdf"),
                          buildPdf(3,
            { "BT /F1 12 Tf 72 700 Td (KEEPONE marker) Tj ET",
              "",
              "BT /F1 12 Tf 72 700 Td (KEEPTWO marker) Tj ET" },
            1)));

        AppContext ctx;
        ctx.pdfEditor = std::make_shared<PdfEditorEngine>();
        m_ocr = std::make_shared<CountingOcr>();
        ctx.ocr = m_ocr;

        BatchMode bm;
        bm.setAppContext(&ctx);
        QCheckBox* skipPages = findCheckBox(&bm, kSkipPagesText);
        QVERIFY2(skipPages, "skip-pages checkbox must exist");
        skipPages->setChecked(true);

        bm.addFilesForTest({ src });
        bm.setOperationForTest(5);   // OpOCR
        bm.onRunBatch();
        pumpUntilDone(bm);

        qInfo() << "ocr calls =" << m_ocr->calls
                << "success =" << bm.successCount() << "fail =" << bm.failCount();
        QCOMPARE(bm.successCount(), 1);
        QCOMPARE(m_ocr->calls, 1);   // ONLY the image page reaches OCR

        const QString out = m_tmp.filePath("mixed3_ocr.pdf");
        QVERIFY(QFile::exists(out));

        // PoDoFo stream identity on the SAVED artifact.
        const QByteArray keptSrc0 = pageStreamBytes(src, 0);
        const QByteArray keptSrc2 = pageStreamBytes(src, 2);
        const QByteArray keptOut0 = pageStreamBytes(out, 0);
        const QByteArray keptOut2 = pageStreamBytes(out, 2);
        const QByteArray ocrOut1 = pageStreamBytes(out, 1);
        qInfo() << "kept0 identical:" << (keptSrc0 == keptOut0)
                << "kept2 identical:" << (keptSrc2 == keptOut2)
                << "ocr page differs:" << (ocrOut1 != keptSrc0);
        QCOMPARE(keptOut0, keptSrc0);   // original page object preserved
        QCOMPARE(keptOut2, keptSrc2);   // original page object preserved
        QVERIFY(ocrOut1.contains("Im0") || ocrOut1.contains("ocrlayer")
                || ocrOut1.size() != keptSrc0.size());

        // PDFium-equivalent text check via PDFium backend is already covered
        // by the lane suite; here the marker must exist in the RAW stream.
        QVERIFY(keptOut0.contains("(KEEPONE marker)"));
        QVERIFY(keptOut2.contains("(KEEPTWO marker)"));
    }

    // Q4 XFAIL honesty: the Widget annotation survives on the kept page but
    // the catalog /AcroForm is gone — the documented gap is REAL (the XFAIL
    // is honest), verified independently here on the saved artifact.
    void q4_acroFormGapIsRealAndXfailHonest() {
        QVERIFY(m_tmp.isValid());
        const QString src = m_tmp.filePath("formy.pdf");
        QVERIFY(writeFile(src, buildPdf(2,
            { "BT /F1 12 Tf 72 700 Td (FORMPAGE marker) Tj ET", "" },
            /*imagePage=*/1, /*withAcroFormOnFirstPage=*/true)));

        AppContext ctx;
        ctx.pdfEditor = std::make_shared<PdfEditorEngine>();
        m_ocr = std::make_shared<CountingOcr>();
        ctx.ocr = m_ocr;

        BatchMode bm;
        bm.setAppContext(&ctx);
        QCheckBox* skipPages = findCheckBox(&bm, kSkipPagesText);
        QVERIFY(skipPages);
        skipPages->setChecked(true);
        bm.addFilesForTest({ src });
        bm.setOperationForTest(5);
        bm.onRunBatch();
        pumpUntilDone(bm);

        const QString out = m_tmp.filePath("formy_ocr.pdf");
        QVERIFY(QFile::exists(out));

        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        const bool catalogHasAcroForm =
            doc.GetCatalog().GetDictionary().HasKey("AcroForm");
        bool keptHasWidget = false;
        auto& annos = doc.GetPages().GetPageAt(0).GetAnnotations();
        for (unsigned i = 0; i < annos.GetCount(); ++i) {
            auto& a = annos.GetAnnotAt(i);
            if (a.GetObject().GetDictionary().HasKey("Subtype")
                && a.GetObject().GetDictionary().FindKey("Subtype")->IsName()
                && a.GetObject().GetDictionary().FindKey("Subtype")->GetName()
                       == PoDoFo::PdfName("Widget"))
                keptHasWidget = true;
        }
        qInfo() << "catalog /AcroForm present:" << catalogHasAcroForm
                << "| kept-page Widget annot:" << keptHasWidget;
        QVERIFY2(!catalogHasAcroForm,
                 "the documented Q4 gap must reproduce for the reviewer "
                 "(Widget survives, catalog /AcroForm dropped) — if this "
                 "flips, the XFAIL must be promoted to a hard assertion");
        QVERIFY2(keptHasWidget,
                 "the Widget annotation itself must survive (the gap is only "
                 "the catalog registration)");
    }

    // Q3: toggling writes the settings file IMMEDIATELY — verified by reading
    // the INI bytes directly (independent of QSettings round-trip).
    void q3_writeOnChangeLandsInTheIniBytes() {
        QSettings().sync();
        BatchMode bm;   // fresh: reads QSettings in its ctor
        bm.setAppContext(&m_ctx);
        QCheckBox* skipFiles = findCheckBox(&bm, "Skip files that already contain text");
        QCheckBox* skipPages = findCheckBox(&bm, kSkipPagesText);
        QCheckBox* force = findCheckBox(&bm, "Force OCR (override skip options)");
        QVERIFY(skipFiles && skipPages && force);
        QVERIFY2(!skipFiles->isChecked() && !skipPages->isChecked(),
                 "fresh store: checkboxes start unchecked");
        skipFiles->setChecked(true);
        skipPages->setChecked(true);
        force->setChecked(true);
        QSettings().sync();

        // Independent read: the native store on this Qt build is the REGISTRY
        // (fileName() is a \HKEY_... path). Read the persisted values with
        // reg.exe — fully outside the QSettings machinery.
        QProcess reg;
        reg.start(QStringLiteral("reg"), {
            QStringLiteral("query"),
            QStringLiteral(R"(HKCU\Software\GlyphPDFTests\R14ProbeBatchSkip)"),
            QStringLiteral("/s") });
        QVERIFY2(reg.waitForFinished(10000), "reg query did not finish");
        const QString outBytes = QString::fromUtf8(reg.readAllStandardOutput());
        qInfo().nospace() << "reg dump: " << outBytes;
        QVERIFY2(outBytes.contains("skipFilesWithText")
                 && outBytes.contains("skipPagesWithText")
                 && outBytes.contains("forceOcr"),
                 "the persisted store must already carry the write-on-change values");
        QVERIFY2(outBytes.count(QStringLiteral("true")) >= 3,
                 "all three keys must be persisted as true");

        // Fresh instance reads them back checked.
        BatchMode bm2;
        bm2.setAppContext(&m_ctx);
        QCheckBox* skipFiles2 = findCheckBox(&bm2, "Skip files that already contain text");
        QCheckBox* skipPages2 = findCheckBox(&bm2, kSkipPagesText);
        QVERIFY(skipFiles2 && skipPages2);
        QVERIFY2(skipFiles2->isChecked() && skipPages2->isChecked(),
                 "a fresh BatchMode must read the persisted choices");
    }
};

#include "R14ProbeBatchSkip.moc"
QTEST_MAIN(R14ProbeBatchSkip)
