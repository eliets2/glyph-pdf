// SPDX-License-Identifier: Apache-2.0
// TestBatchOcrSkipText — N3 (pdf24 §1.3 skip-already-text pattern).
//
// Spec: "-skipFilesWithText, -skipPagesWithText … force OCR". The batch OCR
// worker must be able to leave already-textual material alone and say so
// truthfully:
//   1. skip-files: a mixed corpus (text PDF + image-only PDF) produces output
//      ONLY for the image-only file; the text file lands in the skipped
//      bucket (never success, never failure).
//   2. skip-pages: a mixed document (text page + image-only page) keeps the
//      text page ORIGINAL (page kept as-is: identical extracted text through
//      PDFium on both input and output — the N09 page-copy writer preserves
//      the page content instead of re-encoding it) and OCRed the other page.
//   3. force-OCR overrides both skips (OCR runs on everything).
//   4. idempotency: a second run over the first run's outputs is a no-op
//      (everything is skipped — the outputs now carry the invisible OCR text
//      layer).
//
// The skip decision itself is REAL: PDFium text extraction over real PDF
// files (the same seam the Bates tests read). Only the OCR words come from a
// stub engine; the MRC writer is the REAL PdfEditorEngine pipeline
// (TestMrcPipeline proves it works headlessly in this environment).
//
// Run: QT_QPA_PLATFORM=offscreen ctest -R TestBatchOcrSkipText --output-on-failure

#include <QtTest/QtTest>
#include <QApplication>
#include <QAtomicInt>
#include <QCheckBox>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QTemporaryDir>
#include <cmath>

#include "core/AppContext.h"
#include "core/PolicyController.h"
#include "core/interfaces/IOcrEngine.h"
#include "modes/BatchMode.h"
#include "engines/PdfEditorEngine.h"
#include "engines/pdfium/PdfiumBackend.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <podofo/podofo.h> // Q4: artifact checks on the SAVED file

using gp::PolicyController;

// ── OCR stub: returns one confident word per image (no external engine) ──────

class StubOcr final : public IOcrEngine {
public:
    // Q1: counts how many images actually reach the OCR engine. The
    // skip-pages contract means kept (already-textual) pages must NEVER be
    // rendered+OCRed, so the count is load-bearing evidence.
    QAtomicInt calls{0};
    bool initialize(const QString &, const QString &) override { return true; }
    QList<OcrResult> processImage(const QImage &) override {
        calls.fetchAndAddRelaxed(1);
        OcrResult r;
        r.text        = QStringLiteral("ocrlayer");
        r.boundingBox = QRectF(10, 10, 80, 20);
        r.confidence  = 95;
        return { r };
    }
    QString getRawText(const QImage &) override { return QStringLiteral("ocrlayer"); }
    bool isMockImplementation() const override { return true; }
};

// ── Fixtures (hand-written PDFs; byte-accurate xref, Bates-test layout) ──────

namespace {

constexpr const char* kSkipFilesText = "Skip files that already contain text";
constexpr const char* kSkipPagesText = "Skip pages that already contain text (keep original page)";
constexpr const char* kForceText     = "Force OCR (override skip options)";

// Write a minimal one-or-more-page PDF; each page gets the SAME content
// stream (which may be empty = image-only semantics for the text probe).
// Returns an empty string on I/O failure.
QString writePdf(const QString& dir, const QString& name,
                 const QStringList& pageContents, const QByteArray& mediaBox = "[0 0 612 792]")
{
    const QString path = dir + "/" + name;
    QByteArray pdf;
    pdf += "%PDF-1.4\n";
    QList<int> offsets;
    const auto mark = [&offsets, &pdf]() { offsets.append(pdf.size()); };

    const int pageCount = pageContents.size();
    const int firstPageObj = 3;
    const int firstContentObj = firstPageObj + pageCount;
    const int fontObj = firstContentObj + pageCount;

    mark();
    pdf += "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n";
    QByteArray kids;
    for (int i = 0; i < pageCount; ++i)
        kids += QByteArray::number(firstPageObj + i) + " 0 R ";
    mark();
    pdf += "2 0 obj<</Type/Pages/Kids[" + kids + "]/Count "
           + QByteArray::number(pageCount) + ">>endobj\n";
    for (int i = 0; i < pageCount; ++i) {
        mark();
        pdf += QByteArray::number(firstPageObj + i)
               + " 0 obj<</Type/Page/Parent 2 0 R/MediaBox" + mediaBox
               + "/Contents " + QByteArray::number(firstContentObj + i)
               + " 0 R/Resources<</Font<</F1 " + QByteArray::number(fontObj)
               + " 0 R>>>>>>endobj\n";
    }
    for (int i = 0; i < pageCount; ++i) {
        const QByteArray content = pageContents.at(i).toUtf8();
        mark();
        pdf += QByteArray::number(firstContentObj + i)
               + " 0 obj<</Length " + QByteArray::number(content.size())
               + ">>stream\n" + content + "endstream endobj\n";
    }
    mark();
    pdf += QByteArray::number(fontObj)
           + " 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica/Encoding/WinAnsiEncoding>>endobj\n";

    const int totalObjects = fontObj;
    const int xrefStart = pdf.size();
    pdf += "xref\n0 " + QByteArray::number(totalObjects + 1) + "\n";
    pdf += "0000000000 65535 f \n";
    for (int off : offsets)
        pdf += QByteArray::number(static_cast<qulonglong>(off))
                   .rightJustified(10, '0') + " 00000 n \n";
    pdf += "trailer<</Size " + QByteArray::number(totalObjects + 1)
           + "/Root 1 0 R>>\nstartxref\n"
           + QByteArray::number(xrefStart) + "\n%%EOF\n";

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(pdf) != pdf.size()) return {};
    return path;
}

QString textPdf(const QString& dir, const QString& name) {
    // Visible text: the file carries a real text layer.
    return writePdf(dir, name,
                    { "BT /F1 12 Tf 72 700 Td (Already Textual) Tj ET" });
}

QString imageOnlyPdf(const QString& dir, const QString& name) {
    // No text operators at all — what a scanned page looks like to the probe.
    return writePdf(dir, name, { "" });
}

QString mixedPdf(const QString& dir, const QString& name) {
    // Page 1: text layer. Page 2: image-only.
    return writePdf(dir, name,
                    { "BT /F1 12 Tf 72 700 Td (PageOne Words) Tj ET", "" });
}

// Q4 fixture: page 0 carries real text PLUS annotations (a Link and a form
// Widget) and a catalog /AcroForm referencing the Widget; page 1 carries a
// REAL 1x1 DeviceRGB uncompressed image XObject drawn full-page and no text
// operators. Byte-accurate xref, same hand-written idiom as writePdf.
// Object map: 1 catalog, 2 pages, 3/4 pages, 5/6 contents, 7 font,
// 8 Widget, 9 Link, 10 image XObject.
QString richMixedPdf(const QString& dir, const QString& name) {
    const QString path = dir + "/" + name;
    QByteArray pdf;
    pdf += "%PDF-1.4\n";
    QList<int> offsets;
    const auto mark = [&offsets, &pdf]() { offsets.append(pdf.size()); };
    const auto obj = [&pdf](int n, const QByteArray& body) {
        pdf += QByteArray::number(n) + " 0 obj" + body + "endobj\n";
    };

    mark(); obj(1, "<</Type/Catalog/Pages 2 0 R/AcroForm<</Fields[8 0 R]>>>>\n");
    mark(); obj(2, "<</Type/Pages/Kids[3 0 R 4 0 R]/Count 2>>\n");
    mark(); obj(3, "<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]"
                   "/Contents 5 0 R/Resources<</Font<</F1 7 0 R>>>>"
                   "/Annots[9 0 R 8 0 R]>>\n");
    mark(); obj(4, "<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]"
                   "/Contents 6 0 R/Resources<</XObject<</Im0 10 0 R>>>>>>\n");
    const QByteArray c0 = "BT /F1 12 Tf 72 700 Td (PageOne Words) Tj ET";
    mark(); obj(5, "<</Length " + QByteArray::number(c0.size())
                   + ">>stream\n" + c0 + "endstream\n");
    const QByteArray c1 = "q 612 0 0 792 0 0 cm /Im0 Do Q";
    mark(); obj(6, "<</Length " + QByteArray::number(c1.size())
                   + ">>stream\n" + c1 + "endstream\n");
    mark(); obj(7, "<</Type/Font/Subtype/Type1/BaseFont/Helvetica"
                   "/Encoding/WinAnsiEncoding>>\n");
    mark(); obj(8, "<</Type/Annot/Subtype/Widget/FT/Tx/T (SigLine)"
                   "/Rect[40 700 200 720]/V()>>\n");
    mark(); obj(9, "<</Type/Annot/Subtype/Link/Rect[60 60 200 80]/Border[0 0 0]"
                   "/A<</S/URI/URI(https://example.org/)>>>>\n");
    const QByteArray px = QByteArray::fromHex("FF0000"); // one DeviceRGB pixel
    mark(); obj(10, "<</Type/XObject/Subtype/Image/Width 1/Height 1"
                    "/ColorSpace/DeviceRGB/BitsPerComponent 8/Length "
                    + QByteArray::number(px.size()) + ">>stream\n" + px + "endstream\n");

    constexpr int totalObjects = 10;
    const int xrefStart = pdf.size();
    pdf += "xref\n0 " + QByteArray::number(totalObjects + 1) + "\n";
    pdf += "0000000000 65535 f \n";
    for (int off : offsets)
        pdf += QByteArray::number(static_cast<qulonglong>(off))
                   .rightJustified(10, '0') + " 00000 n \n";
    pdf += "trailer<</Size " + QByteArray::number(totalObjects + 1)
           + "/Root 1 0 R>>\nstartxref\n"
           + QByteArray::number(xrefStart) + "\n%%EOF\n";

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(pdf) != pdf.size()) return {};
    return path;
}

QString pdfiumTextOf(const QString& path, int page) {
    PdfiumBackend reader;
    if (!reader.loadDocument(path)) return QStringLiteral("<load failed>");
    QString text;
    for (const auto& run : reader.extractPageTextRuns(page)) text += run.text;
    return text;
}

// Q4 helper: MediaBox [x y w h] equality within a point tolerance (the MRC
// re-encode may round page geometry slightly).
bool mediaBoxNearly(PoDoFo::PdfPage& page, double x, double y, double w, double h,
                    double tol = 0.01) {
    auto* mb = page.GetDictionary().FindKey("MediaBox");
    if (!mb || !mb->IsArray()) return false;
    const auto& arr = mb->GetArray();
    if (arr.GetSize() != 4) return false;
    const double want[4] = { x, y, w, h };
    for (int i = 0; i < 4; ++i) {
        const auto* o = arr.FindAt(i);
        if (!o) return false;
        if (std::abs(static_cast<double>(o->GetNumber()) - want[i]) > tol)
            return false;
    }
    return true;
}

} // namespace

class TestBatchOcrSkipText : public QObject {
    Q_OBJECT

private:
    struct Harness {
        QTemporaryDir tmp;
        AppContext ctx;
        gp::BatchMode bm;
        // Same instance installed into ctx.ocr — tests read the call counter.
        std::shared_ptr<StubOcr> ocr;
    };

    // Real MRC/assembly engine + stub OCR words; skip decisions run REAL
    // PDFium probes over the real files.
    static std::unique_ptr<Harness> makeHarness() {
        auto h = std::make_unique<Harness>();
        if (!h->tmp.isValid()) return h;
        h->ctx.pdfEditor = std::make_shared<PdfEditorEngine>();
        h->ocr           = std::make_shared<StubOcr>();
        h->ctx.ocr       = h->ocr;
        h->bm.setAppContext(&h->ctx);
        return h;
    }

    static QCheckBox* findCheckBox(QWidget* host, const char* text) {
        const auto boxes = host->findChildren<QCheckBox*>();
        for (QCheckBox* b : boxes)
            if (b->text().contains(QLatin1String(text)))
                return b;
        return nullptr;
    }

    static void pumpUntilDone(gp::BatchMode& bm, int expectedAccounted = -1) {
        int waited = 0;
        while (bm.isBatchRunning() && waited < 30000) {
            QTest::qWait(50);
            waited += 50;
        }
        QVERIFY2(!bm.isBatchRunning(), "Batch did not complete within 30 seconds");
        // Q2 (test-harness fix): the per-result callbacks are
        // Qt::QueuedConnection, so they can lag the future's completion by an
        // event-loop pass — isRunning() flips false before the queued
        // resultReadyAt handlers update the counters (observed as
        // "BATCH COMPLETE — 1 of 1 succeeded" in the log while
        // successCount()==0). Drain until the accounting catches up, bounded.
        waited = 0;
        while (expectedAccounted >= 0
               && bm.successCount() + bm.failCount() + bm.skipCount() < expectedAccounted
               && waited < 5000) {
            QTest::qWait(25);
            waited += 25;
        }
    }

private slots:

    // Q3: isolate QSettings so the test neither reads the user's real
    // preference nor clobbers it (TestBatchOcrLanguage idiom). Must run
    // BEFORE the first harness — the BatchMode ctor reads QSettings.
    void initTestCase() {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestBatchOcrSkipText"));
    }
    void cleanupTestCase() {
        QSettings().remove(QStringLiteral("ocr/skipFilesWithText"));
        QSettings().remove(QStringLiteral("ocr/skipPagesWithText"));
        QSettings().remove(QStringLiteral("ocr/forceOcr"));
    }
    // Per-test wipe: every slot starts from a clean settings store, and the
    // write-on-change toggles of one slot never leak into the next.
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

    // ── 1. skip-files: mixed corpus → output only for the image-only file ────
    void skipFileProducesOutputsOnlyForImageOnlyFiles() {
        auto h = makeHarness();
        QVERIFY(h->tmp.isValid());
        const QString texty   = textPdf(h->tmp.path(), "texty.pdf");
        const QString scanned = imageOnlyPdf(h->tmp.path(), "scanned.pdf");
        QVERIFY(!texty.isEmpty() && !scanned.isEmpty());

        QCheckBox* skipFiles = findCheckBox(&h->bm, kSkipFilesText);
        QVERIFY2(skipFiles, "the skip-files checkbox must exist in the OCR panel");
        skipFiles->setChecked(true);

        h->bm.addFilesForTest({ texty, scanned });
        h->bm.setOperationForTest(5); // OpOCR (m_opCombo order)
        h->bm.onRunBatch();
        pumpUntilDone(h->bm, 2);

        // Truthful accounting: the text file SKIPPED (its own bucket), the
        // image-only file OCR'd, nothing failed.
        QCOMPARE(h->bm.skipCount(), 1);
        QCOMPARE(h->bm.successCount(), 1);
        QCOMPARE(h->bm.failCount(), 0);
        QCOMPARE(h->bm.remainingCount(), 0);

        // Output exists ONLY for the image-only input.
        const QString scannedOut = h->tmp.filePath("scanned_ocr.pdf");
        const QString textyOut   = h->tmp.filePath("texty_ocr.pdf");
        QVERIFY2(QFileInfo::exists(scannedOut), "image-only file must be OCRed");
        QVERIFY2(!QFileInfo::exists(textyOut),
                 "an already-textual file must not be re-OCRed under skip-files");

        // The OCR'd output carries the invisible text layer (real MRC writer).
        QVERIFY2(pdfiumTextOf(scannedOut, 0).contains(QStringLiteral("ocrlayer")),
                 "the OCRed output must be searchable");
    }

    // ── 2. skip-pages: text page kept original, image-only page OCRed ────────
    void skipPageKeepsTextPageOriginal() {
        auto h = makeHarness();
        QVERIFY(h->tmp.isValid());
        const QString mixed = mixedPdf(h->tmp.path(), "mixed.pdf");
        QVERIFY(!mixed.isEmpty());
        const QString originalPage0Text = pdfiumTextOf(mixed, 0);
        QVERIFY(originalPage0Text.contains(QStringLiteral("PageOne Words")));

        QCheckBox* skipPages = findCheckBox(&h->bm, kSkipPagesText);
        QVERIFY2(skipPages, "the skip-pages checkbox must exist in the OCR panel");
        skipPages->setChecked(true);

        h->bm.addFilesForTest({ mixed });
        h->bm.setOperationForTest(5);
        h->bm.onRunBatch();
        pumpUntilDone(h->bm, 1);

        QCOMPARE(h->bm.skipCount(), 0);   // the FILE was processed…
        QCOMPARE(h->bm.successCount(), 1);
        QCOMPARE(h->bm.failCount(), 0);

        const QString out = h->tmp.filePath("mixed_ocr.pdf");
        QVERIFY2(QFileInfo::exists(out), "the mixed document must produce an output");
        QCOMPARE(pdfiumTextOf(out, 0), originalPage0Text);
        QVERIFY2(pdfiumTextOf(out, 1).contains(QStringLiteral("ocrlayer")),
                 "the image-only page must be OCRed");
    }

    // ── 2b. Q1: skip-pages must not render/OCR the pages it keeps ────────────
    // The skip decision runs BEFORE any render/OCR work: a kept text page
    // must never be rasterized nor pushed through the OCR engine. The
    // original defect rendered + OCRed EVERY page up front and then silently
    // discarded the text page's result — wasted work and a doubled OCR call
    // count on every mixed document.
    void skipPagesOcrsOnlyPagesWithoutText() {
        auto h = makeHarness();
        QVERIFY(h->tmp.isValid());
        const QString mixed = mixedPdf(h->tmp.path(), "mixed.pdf");
        QVERIFY(!mixed.isEmpty());

        QCheckBox* skipPages = findCheckBox(&h->bm, kSkipPagesText);
        QVERIFY2(skipPages, "the skip-pages checkbox must exist in the OCR panel");
        skipPages->setChecked(true);

        h->bm.addFilesForTest({ mixed });
        h->bm.setOperationForTest(5);
        h->bm.onRunBatch();
        pumpUntilDone(h->bm, 1);

        QCOMPARE(h->bm.skipCount(), 0);
        QCOMPARE(h->bm.successCount(), 1);
        QCOMPARE(h->bm.failCount(), 0);
        // The load-bearing assertion: exactly ONE image (the image-only page)
        // may reach the OCR engine. The kept text page must not be rendered
        // or OCRed at all.
        QCOMPARE(h->ocr->calls.loadRelaxed(), 1);

        // The skip-pages contract still holds on the output.
        const QString out = h->tmp.filePath("mixed_ocr.pdf");
        QVERIFY2(QFileInfo::exists(out), "the mixed document must produce an output");
        QVERIFY2(pdfiumTextOf(out, 0).contains(QStringLiteral("PageOne Words")),
                 "the text page must be kept original");
        QVERIFY2(pdfiumTextOf(out, 1).contains(QStringLiteral("ocrlayer")),
                 "the image-only page must be OCRed");
    }

    // ── 3. force-OCR overrides the skips ─────────────────────────────────────
    void forceOcrOverridesSkips() {
        auto h = makeHarness();
        QVERIFY(h->tmp.isValid());
        const QString texty = textPdf(h->tmp.path(), "texty.pdf");
        QVERIFY(!texty.isEmpty());

        QCheckBox* skipFiles = findCheckBox(&h->bm, kSkipFilesText);
        QCheckBox* force     = findCheckBox(&h->bm, kForceText);
        QVERIFY(skipFiles && force);
        skipFiles->setChecked(true);
        force->setChecked(true);

        h->bm.addFilesForTest({ texty });
        h->bm.setOperationForTest(5);
        h->bm.onRunBatch();
        pumpUntilDone(h->bm, 1);

        QCOMPARE(h->bm.skipCount(), 0);
        QCOMPARE(h->bm.successCount(), 1);
        QCOMPARE(h->bm.failCount(), 0);
        QVERIFY2(QFileInfo::exists(h->tmp.filePath("texty_ocr.pdf")),
                 "force-OCR must OCR even an already-textual file");
    }

    // ── 4. idempotency: the second run is a no-op ────────────────────────────
    void secondRunOverOutputsIsNoOp() {
        auto h = makeHarness();
        QVERIFY(h->tmp.isValid());
        const QString scanned = imageOnlyPdf(h->tmp.path(), "scanned.pdf");
        QVERIFY(!scanned.isEmpty());

        QCheckBox* skipFiles = findCheckBox(&h->bm, kSkipFilesText);
        QVERIFY(skipFiles);
        skipFiles->setChecked(true);

        // Run 1: the image-only file is OCRed.
        h->bm.addFilesForTest({ scanned });
        h->bm.setOperationForTest(5);
        h->bm.onRunBatch();
        pumpUntilDone(h->bm, 1);
        QCOMPARE(h->bm.skipCount(), 0);
        QCOMPARE(h->bm.successCount(), 1);
        const QString out = h->tmp.filePath("scanned_ocr.pdf");
        QVERIFY(QFileInfo::exists(out));

        // Run 2 over the OUTPUT: it now carries the invisible text layer, so
        // the same skip-files run must leave it alone — a no-op.
        // Q2: run 2 uses a FRESH harness holding ONLY run 1's output.
        // addFilePaths has append semantics (BatchMode.cpp), so reusing h
        // would resubmit BOTH scanned.pdf and its output; the multi-file
        // AR-8 overwrite pre-check then pops a modal QMessageBox::warning
        // that blocks THIS thread inside onRunBatch() under offscreen — an
        // unbounded hang (the old test never reached pumpUntilDone). A fresh
        // harness makes run 2 a clean single-file run whose own output
        // (scanned_ocr_ocr.pdf) does not exist, so no dialog can appear.
        // h stays in scope, keeping its QTemporaryDir (and `out`) alive.
        auto h2 = makeHarness();
        QVERIFY(h2->tmp.isValid());
        QCheckBox* skipFiles2 = findCheckBox(&h2->bm, kSkipFilesText);
        QVERIFY2(skipFiles2, "the skip-files checkbox must exist in the OCR panel");
        skipFiles2->setChecked(true);
        h2->bm.addFilesForTest({ out });
        h2->bm.setOperationForTest(5);
        h2->bm.onRunBatch();
        pumpUntilDone(h2->bm, 1);
        QCOMPARE(h2->bm.skipCount(), 1);
        QCOMPARE(h2->bm.successCount(), 0);
        QCOMPARE(h2->bm.failCount(), 0);
    }

    // ── 6. Q3: skip options persist across an app restart (write-on-change) ──
    // The OCR panel initializes the three skip checkboxes FROM QSettings, so
    // persistence is clearly the advertised behavior — but nothing ever wrote
    // the values back: the user's choice silently evaporated on restart.
    void skipOptionsPersistAcrossRestart() {
        // init() wiped the three keys: a fresh harness starts all-unchecked.
        {
            auto h = makeHarness();
            QVERIFY(h->tmp.isValid());
            QCheckBox* skipFiles = findCheckBox(&h->bm, kSkipFilesText);
            QCheckBox* skipPages = findCheckBox(&h->bm, kSkipPagesText);
            QCheckBox* force     = findCheckBox(&h->bm, kForceText);
            QVERIFY(skipFiles && skipPages && force);
            QVERIFY(!skipFiles->isChecked());
            QVERIFY(!skipPages->isChecked());
            QVERIFY(!force->isChecked());
            // Toggling must write through to QSettings immediately.
            skipFiles->setChecked(true);
            skipPages->setChecked(true);
            force->setChecked(true);
            QSettings().sync();
        }
        // A FRESH harness — the next app start — restores the choice.
        auto h2 = makeHarness();
        QVERIFY(h2->tmp.isValid());
        QCheckBox* skipFiles2 = findCheckBox(&h2->bm, kSkipFilesText);
        QCheckBox* skipPages2 = findCheckBox(&h2->bm, kSkipPagesText);
        QCheckBox* force2     = findCheckBox(&h2->bm, kForceText);
        QVERIFY(skipFiles2 && skipPages2 && force2);
        QVERIFY2(skipFiles2->isChecked(), "skip-files choice must persist across restart");
        QVERIFY2(skipPages2->isChecked(), "skip-pages choice must persist across restart");
        QVERIFY2(force2->isChecked(), "force-OCR choice must persist across restart");
    }

    // ── 7. Q4: the preservation contract, checked on the SAVED file ──────────
    // The honest guarantee is EXTRACTED-TEXT EQUALITY plus object-level page
    // preservation — NOT byte identity: kept pages are the original page
    // objects carried over by the N09 page-copy writer (annotations included);
    // OCRed pages are re-encoded MRC fragments. So the fixture page 0 carries
    // a Link and a form Widget plus a catalog /AcroForm, and page 1 carries a
    // REAL image XObject; everything below is verified on the saved OUTPUT.
    void skipPagesPreservesPageObjectsOnSavedFile() {
        auto h = makeHarness();
        QVERIFY(h->tmp.isValid());
        const QString rich = richMixedPdf(h->tmp.path(), "rich.pdf");
        QVERIFY(!rich.isEmpty());
        const QString originalPage0Text = pdfiumTextOf(rich, 0);
        QVERIFY(originalPage0Text.contains(QStringLiteral("PageOne Words")));

        QCheckBox* skipPages = findCheckBox(&h->bm, kSkipPagesText);
        QVERIFY2(skipPages, "the skip-pages checkbox must exist in the OCR panel");
        skipPages->setChecked(true);
        h->bm.addFilesForTest({ rich });
        h->bm.setOperationForTest(5);
        h->bm.onRunBatch();
        pumpUntilDone(h->bm, 1);
        QCOMPARE(h->bm.skipCount(), 0);
        QCOMPARE(h->bm.successCount(), 1);
        QCOMPARE(h->bm.failCount(), 0);

        const QString out = h->tmp.filePath("rich_ocr.pdf");
        QVERIFY2(QFileInfo::exists(out), "the mixed document must produce an output");

        // Extracted-text equality on the kept page; invisible OCR layer on
        // the OCRed page. (Byte identity of the FILE is explicitly NOT the
        // contract — the OCRed page is a different, re-encoded object.)
        QCOMPARE(pdfiumTextOf(out, 0), originalPage0Text);
        QVERIFY2(pdfiumTextOf(out, 1).contains(QStringLiteral("ocrlayer")),
                 "the image-only page must be OCRed");

        // Artifact checks on the SAVED file via PoDoFo.
        PoDoFo::PdfMemDocument doc;
        bool loadedOut = false;
        try {
            doc.Load(out.toUtf8().constData());
            loadedOut = true;
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QStringLiteral("the saved output must load: %1")
                                 .arg(QString::fromUtf8(e.what()))));
        } catch (...) {
            QFAIL("the saved output must load (non-standard exception)");
        }
        QVERIFY(loadedOut);
        QCOMPARE(doc.GetPages().GetCount(), 2u);

        // Kept page: original page OBJECTS — annotations survive, including
        // the form Widget annotation.
        auto& pg0 = doc.GetPages().GetPageAt(0);
        auto& annots0 = pg0.GetAnnotations();
        QCOMPARE(annots0.GetCount(), 2u);
        QSet<QString> subtypes;
        for (unsigned i = 0; i < annots0.GetCount(); ++i) {
            auto& a = annots0.GetAnnotAt(i);
            if (auto* st = a.GetDictionary().FindKey("Subtype"); st && st->IsName())
                subtypes.insert(QString::fromStdString(
                    std::string(st->GetName().GetString())));
        }
        QVERIFY2(subtypes.contains(QStringLiteral("Widget")),
                 "the kept page must preserve its form Widget annotation");
        QVERIFY2(subtypes.contains(QStringLiteral("Link")),
                 "the kept page must preserve its Link annotation");
        QVERIFY2(pg0.GetDictionary().HasKey("Resources"),
                 "the kept page must keep its /Resources");
        QVERIFY2(mediaBoxNearly(pg0, 0, 0, 612, 792),
                 "the kept page must keep its MediaBox");

        // OCRed page: re-encoded MRC, but page geometry is preserved and the
        // page still carries /Resources (the MRC image + text layer).
        auto& pg1 = doc.GetPages().GetPageAt(1);
        QVERIFY2(pg1.GetDictionary().HasKey("Resources"),
                 "the OCRed page must carry /Resources");
        QVERIFY2(mediaBoxNearly(pg1, 0, 0, 612, 792, 0.5),
                 "the OCRed page must keep its MediaBox geometry");

        // KNOWN PRODUCTION GAP (surfaced, never silently weakened): the
        // page-copy path preserves the Widget ANNOTATIONS but does not
        // re-create the catalog-level /AcroForm (verified down to the
        // single-page extract itself), so the output is no longer a
        // registered fillable form. Tracked in the Q-lane ledger; if the
        // writer gains AcroForm reconstruction this XFAIL flips to XPASS and
        // this assertion should be promoted to a hard requirement.
        QEXPECT_FAIL("", "Q4 production finding: catalog /AcroForm is dropped "
                         "by extractPageAsBytes + writeDocumentFromPages",
                     Continue);
        QVERIFY2(doc.GetCatalog().GetDictionary().HasKey("AcroForm"),
                 "the catalog /AcroForm should survive the page-copy assembly");
    }

    // ── 5. defaults unchanged: no flags → full MRC path, nothing skipped ─────
    void noFlagsKeepsLegacyWholeDocumentBehavior() {
        auto h = makeHarness();
        QVERIFY(h->tmp.isValid());
        const QString texty = textPdf(h->tmp.path(), "texty.pdf");
        QVERIFY(!texty.isEmpty());

        h->bm.addFilesForTest({ texty });
        h->bm.setOperationForTest(5);
        h->bm.onRunBatch();
        pumpUntilDone(h->bm, 1);

        QCOMPARE(h->bm.skipCount(), 0);
        QCOMPARE(h->bm.successCount(), 1);
        QCOMPARE(h->bm.failCount(), 0);
        QVERIFY(QFileInfo::exists(h->tmp.filePath("texty_ocr.pdf")));
    }

    // ── 8. emergence E-2 (SWEEP-W3-EMERGENCE §1b): a refused OCR download ────
    // fails the file HONESTLY. Pre-fix the worker discarded initialize()'s
    // return: the pipeline ran on the uninitialized engine (the real engine's
    // processImage then re-initialized with the DEFAULT "eng" — a wrong-
    // language text layer — or produced zero-word output), the file was
    // accounted SUCCESSFUL and an image-only "_ocr.pdf" was written, while
    // the policy whyNot stayed console-only. Post-fix: the file FAILS with a
    // whyNot naming the deciding half of ocr/allowNetworkDownload, the engine
    // is never fed an image, and no output file exists.
    //
    // The stub reproduces the engine gate's contract: initialize(lang) fails
    // console-only when the language data is missing and the effective policy
    // refuses the download; processImage is the wrong-language trap the fix
    // must make unreachable.
private:
    struct RefusedOcr final : public IOcrEngine {
        QAtomicInt calls{0};
        QString requestedLang;
        bool initialize(const QString& lang, const QString&) override {
            requestedLang = lang;
            return false;   // the policy refused the language-data download
        }
        QList<OcrResult> processImage(const QImage&) override {
            calls.fetchAndAddRelaxed(1);
            OcrResult r;
            r.text        = QStringLiteral("ocrlayer");
            r.boundingBox = QRectF(10, 10, 80, 20);
            r.confidence  = 95;
            return { r };
        }
        QString getRawText(const QImage&) override { return QStringLiteral("ocrlayer"); }
        bool isMockImplementation() const override { return true; }
    };

    struct RefusedHarness {
        QTemporaryDir tmp;
        AppContext    ctx;
        gp::BatchMode bm;
        std::shared_ptr<RefusedOcr> ocr;
    };

    static std::unique_ptr<RefusedHarness> makeRefusedHarness() {
        auto h = std::make_unique<RefusedHarness>();
        if (!h->tmp.isValid()) return h;
        h->ctx.pdfEditor = std::make_shared<PdfEditorEngine>();
        h->ocr           = std::make_shared<RefusedOcr>();
        h->ctx.ocr       = h->ocr;
        h->bm.setAppContext(&h->ctx);
        return h;
    }

    // The failure entry's whyNot in the batch error log (both honest refusal
    // wordings carry this closing clause; skip reasons and other failures do
    // not).
    static QString refusedDetail(const gp::BatchMode& bm) {
        for (int i = 0; i < bm.errorLogCount(); ++i) {
            const QString d = bm.errorDetailForTest(i);
            if (d.contains(QStringLiteral("no output was written"))) return d;
        }
        return {};
    }

private slots:
    void policyBlockedDownloadFailsFileHonestlyAndWritesNothing() {
        // The USER opted IN; the machine policy refuses — the whyNot must
        // name the POLICY (the E-5-style disclosure), not the dead-end
        // setting the user already enabled. The language is German: the
        // wrong-language trap (silent "eng" fallback) is exactly what the
        // honest refusal must prevent.
        QSettings().setValue(QStringLiteral("ocr/language"), QStringLiteral("DE"));
        QSettings().setValue(QStringLiteral("ocr/allowNetworkDownload"), true);
        PolicyController::instance().resetForTesting();
        // W1-05 structural close: a wiring pin (policy key -> OCR download
        // gate) with a standard-user-written fixture — run under the
        // disclosed assume-trusted seam; the gate itself is pinned in
        // TestPolicyWiring::plantedPolicyFromUserWritablePathIsNotEnforced.
        qputenv("GLYPHPDF_POLICY_ASSUME_TRUSTED", "1");

        auto h = makeRefusedHarness();
        QVERIFY(h->tmp.isValid());
        const QString policyPath = h->tmp.filePath(QStringLiteral("emfix-e2-policy.json"));
        QJsonObject settings;
        settings.insert(QStringLiteral("ocr/allowNetworkDownload"), false);
        {
            QFile pf(policyPath);
            QVERIFY(pf.open(QIODevice::WriteOnly | QIODevice::Truncate));
            pf.write(QJsonDocument(
                QJsonObject{ { QStringLiteral("schemaVersion"), 1 },
                             { QStringLiteral("settings"), settings } }).toJson());
        }
        QVERIFY2(PolicyController::instance().load(policyPath),
                 "the managed-policy fixture must load");

        const QString scanned = imageOnlyPdf(h->tmp.path(), "scanned.pdf");
        QVERIFY(!scanned.isEmpty());
        h->bm.addFilesForTest({ scanned });
        h->bm.setOperationForTest(5);
        h->bm.onRunBatch();
        pumpUntilDone(h->bm, 1);

        // Honest accounting: FAILED, never silently successful.
        QCOMPARE(h->bm.failCount(), 1);
        QCOMPARE(h->bm.successCount(), 0);
        QCOMPARE(h->bm.skipCount(), 0);
        // The engine gate refused BEFORE any OCR work — no wrong-language
        // fallback ever ran.
        QCOMPARE(h->ocr->calls.loadRelaxed(), 0);
        QCOMPARE(h->ocr->requestedLang, QStringLiteral("deu"));
        // No "_ocr.pdf" garbage output exists.
        QVERIFY2(!QFileInfo::exists(h->tmp.filePath("scanned_ocr.pdf")),
                 "a policy-refused download must not produce an _ocr.pdf output");
        // The whyNot names the policy and the language, and states plainly
        // that nothing was written.
        const QString detail = refusedDetail(h->bm);
        QVERIFY2(!detail.isEmpty(),
                 "the failure whyNot must be recorded in the batch error log");
        QVERIFY2(detail.contains(QStringLiteral("machine policy")),
                 qPrintable(QStringLiteral("the whyNot must name the machine "
                                          "policy, got: %1").arg(detail)));
        QVERIFY2(detail.contains(QStringLiteral("deu")),
                 qPrintable(detail));
        QVERIFY2(detail.contains(QStringLiteral("no output was written")),
                 qPrintable(detail));

        PolicyController::instance().resetForTesting();
        qunsetenv("GLYPHPDF_POLICY_ASSUME_TRUSTED");
        QSettings().remove(QStringLiteral("ocr/allowNetworkDownload"));
        QSettings().remove(QStringLiteral("ocr/language"));
    }

    void unmanagedDisabledDownloadNamesTheUserSetting() {
        // No policy loaded; the USER's own setting refuses → the whyNot names
        // the setting (which Preferences can change), never the policy.
        PolicyController::instance().resetForTesting();
        QSettings().setValue(QStringLiteral("ocr/allowNetworkDownload"), false);

        auto h = makeRefusedHarness();
        QVERIFY(h->tmp.isValid());
        const QString scanned = imageOnlyPdf(h->tmp.path(), "scanned.pdf");
        QVERIFY(!scanned.isEmpty());
        h->bm.addFilesForTest({ scanned });
        h->bm.setOperationForTest(5);
        h->bm.onRunBatch();
        pumpUntilDone(h->bm, 1);

        QCOMPARE(h->bm.failCount(), 1);
        QCOMPARE(h->bm.successCount(), 0);
        QCOMPARE(h->ocr->calls.loadRelaxed(), 0);
        QVERIFY2(!QFileInfo::exists(h->tmp.filePath("scanned_ocr.pdf")),
                 "no output for a refused download");
        const QString detail = refusedDetail(h->bm);
        QVERIFY2(!detail.isEmpty(), "the whyNot must be recorded");
        QVERIFY2(detail.contains(QStringLiteral("OCR download setting")),
                 qPrintable(QStringLiteral("unmanaged refusal must name the user "
                                          "setting, got: %1").arg(detail)));
        QVERIFY2(!detail.contains(QStringLiteral("machine policy")),
                 qPrintable(detail));

        PolicyController::instance().resetForTesting();
        QSettings().remove(QStringLiteral("ocr/allowNetworkDownload"));
    }

    void initializationFailureWithoutPolicyStillFailsTheFile() {
        // Download ALLOWED (user pref on, no policy) but the engine still
        // fails to initialize (e.g. no Tesseract at all): the file must FAIL
        // honestly — the generic honest wording, still never silent success.
        PolicyController::instance().resetForTesting();
        QSettings().setValue(QStringLiteral("ocr/allowNetworkDownload"), true);

        auto h = makeRefusedHarness();
        QVERIFY(h->tmp.isValid());
        const QString scanned = imageOnlyPdf(h->tmp.path(), "scanned.pdf");
        QVERIFY(!scanned.isEmpty());
        h->bm.addFilesForTest({ scanned });
        h->bm.setOperationForTest(5);
        h->bm.onRunBatch();
        pumpUntilDone(h->bm, 1);

        QCOMPARE(h->bm.failCount(), 1);
        QCOMPARE(h->bm.successCount(), 0);
        QCOMPARE(h->ocr->calls.loadRelaxed(), 0);
        QVERIFY2(!QFileInfo::exists(h->tmp.filePath("scanned_ocr.pdf")),
                 "no output for a failed initialization");
        const QString detail = refusedDetail(h->bm);
        QVERIFY2(detail.contains(QStringLiteral("initialization failed")),
                 qPrintable(QStringLiteral("generic honest wording expected, "
                                          "got: %1").arg(detail)));

        PolicyController::instance().resetForTesting();
        QSettings().remove(QStringLiteral("ocr/allowNetworkDownload"));
    }
};

QTEST_MAIN(TestBatchOcrSkipText)
#include "TestBatchOcrSkipText.moc"
