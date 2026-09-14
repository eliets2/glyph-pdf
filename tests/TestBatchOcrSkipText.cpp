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
#include <QSettings>
#include <QTemporaryDir>

#include "core/AppContext.h"
#include "core/interfaces/IOcrEngine.h"
#include "modes/BatchMode.h"
#include "engines/PdfEditorEngine.h"
#include "engines/pdfium/PdfiumBackend.h"

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

QString pdfiumTextOf(const QString& path, int page) {
    PdfiumBackend reader;
    if (!reader.loadDocument(path)) return QStringLiteral("<load failed>");
    QString text;
    for (const auto& run : reader.extractPageTextRuns(page)) text += run.text;
    return text;
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
};

QTEST_MAIN(TestBatchOcrSkipText)
#include "TestBatchOcrSkipText.moc"
