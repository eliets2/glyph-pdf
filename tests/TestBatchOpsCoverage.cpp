// SPDX-License-Identifier: Apache-2.0
// §9.12 P1 — automated test coverage for the BATCH OPERATIONS (July-parity gap).
//
// Gap-only coverage, complementing what already exists:
//   * TestBatchMode            — convert (mock engine), compress (real engine,
//                                output produced), DPI/preset seams, cancel.
//   * TestBatchOcrLanguage     — batch OCR language selection seam.
//   * TestBatchOcrConfidence   — low-confidence review-note seam.
//   * TestWatermarkFont        — watermark ENGINE font handling (not batch).
//   * TestVeraPdf/TestMrcPipeline — veraPDF CLI + MRC PDF/A (not batch export).
//   * TestMergeSuccess         — PdfViewerWidget::mergeDocuments seam (not
//                                BatchMode::runMerge, no page-count/order).
//   * TestPatternRedact        — engine applyPatternRedactionsMulti API (not
//                                driven through the BatchMode redact panel).
//
// The gaps closed here, each driving the REAL BatchMode worker over REAL
// fixtures (offscreen, engines on disk — no mocks for the document ops):
//   1. Watermark batch — OpWatermark over a 2-page fixture; the watermark text
//      must be extractable from EVERY page of the output.
//   2. Export-PDF/A batch — OpExportPdfA must produce an output that opens in
//      PoDoFo and carries the PDF/A identification (OutputIntents/GTS_PDFA1,
//      XMP pdfaid, PdfALevel readback).
//   3. Merge batch — BatchMode::runMerge output page count == sum of inputs,
//      page ORDER follows list order (distinct per-input tokens, in order).
//   4. Redact batch — OpRedact with ONLY a named-PII preset checkbox checked;
//      the matched content must be excised from the output while surrounding
//      text survives.
//   5. FINDING (characterization): the Export PDF/A panel offers PDF/A-2U /
//      PDF/A-3U items whose data (4 / 5) falls through PoDoFoBackend::
//      exportPdfA's switch (only 2 and 3 are mapped) to the PDF/A-1B default —
//      the combo silently promises a level it does not deliver.
//
// Run: QT_QPA_PLATFORM=offscreen ctest -R TestBatchOpsCoverage --output-on-failure
#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLineEdit>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <podofo/podofo.h>

#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "modes/BatchMode.h"
#include "engines/PatternRedactor.h"
#include "mocks/MockPdfEditorEngine.h"

// ── Fixture: PoDoFo painter pages, each carrying text ─────────────────────────
// Same idiom as TestPatternRedact's createPdfWithText (standard-14 Helvetica,
// drawn with PdfPainter) so PDFium-based extraction (QPdfDocument /
// PatternRedactor::findMatches) decodes it.

// Multi-RUN fixture: each page draws each entry as its OWN text-showing
// operator (separate DrawText → separate Tj) at the given x offsets.
// Required for redaction granularity: PoDoFoBackend::applyRedactions excises
// the WHOLE intersecting Tj/TJ operator (Edact-Ray glyph-advance defense —
// partial strings are never emitted), so a single-Tj line containing both the
// PII and innocent text is fully excised by design. Distinct runs let the test
// assert surgical excision: the PII run goes, the neighbouring runs survive.
using PageRuns = QList<QList<QPair<QString, double>>>;

static QString createMultiRunTextPdf(const QString& dir, const QString& name,
                                     const PageRuns& pages) {
    const QString path = dir + "/" + name;
    try {
        PoDoFo::PdfMemDocument doc;
        for (const auto& runs : pages) {
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            for (const auto& run : runs)
                painter.DrawText(run.first.toUtf8().constData(), run.second, 700);
            painter.FinishDrawing();
        }
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "createMultiRunTextPdf failed:" << e.what();
        return {};
    }
    return path;
}

// Convenience: one text object per page (own Tj per page, centered layout).
static QString createMultiPageTextPdf(const QString& dir, const QString& name,
                                      const QStringList& pageTexts) {
    PageRuns pages;
    for (const QString& t : pageTexts)
        pages.append({ { t, 50.0 } });
    return createMultiRunTextPdf(dir, name, pages);
}

// ── Verification helper: PDFium text extraction per page (QtPdf) ─────────────
// (No QVERIFY/QCOMPARE here — those macros `return;` and would break a
// QString-returning function; callers verify the load status themselves.)

static QString extractPageText(QPdfDocument& doc, int page) {
    return doc.getAllText(page).text();
}

// ── Test class ─────────────────────────────────────────────────────────────────

class TestBatchOpsCoverage : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    QString tmpPath(const QString& name) { return m_tmpDir.filePath(name); }

    // Minimal AppContext mirroring TestBatchMode::testCompressOpProducesOutput:
    // the Watermark/ExportPdfA/Redact workers construct a FRESH per-file
    // PdfEditorEngine internally, so the context only needs to be non-null.
    static AppContext makeCtx() {
        AppContext ctx;
        ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(
            new MockPdfEditorEngine, [](auto*){});
        return ctx;
    }

    // The watermark text edit has no objectName — identify it by its unique
    // placeholder ("CONFIDENTIAL"); every other BatchMode QLineEdit is an
    // output-dir / pattern / hot-folder field.
    static QLineEdit* watermarkTextEdit(gp::BatchMode& bm) {
        const auto edits = bm.findChildren<QLineEdit*>();
        for (QLineEdit* e : edits)
            if (e->placeholderText() == QStringLiteral("CONFIDENTIAL"))
                return e;
        return nullptr;
    }

    // The PDF/A conformance combo is the only one offering "PDF/A-1B".
    static QComboBox* pdfaLevelCombo(gp::BatchMode& bm) {
        const auto combos = bm.findChildren<QComboBox*>();
        for (QComboBox* c : combos)
            if (c->findText(QStringLiteral("PDF/A-1B")) >= 0)
                return c;
        return nullptr;
    }

    // Pump the event loop until the QtConcurrent batch completes.
    static void runAndWait(gp::BatchMode& bm) {
        bm.onRunBatch();
        int waited = 0;
        while (bm.isBatchRunning() && waited < 15000) {
            QTest::qWait(50);
            waited += 50;
        }
        QVERIFY2(!bm.isBatchRunning(), "Batch did not complete within 15 seconds");
    }

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
        // Isolate QSettings like every other batch test (never touch real prefs).
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestBatchOpsCoverage"));
    }

    // ── 1. Watermark batch — every page of the output carries the text ────────
    void watermarkBatchStampsEveryPage() {
        const QStringList tokens = {
            QStringLiteral("WMSRC-PAGE-ZERO"),
            QStringLiteral("WMSRC-PAGE-ONE"),
        };
        const QString src = createMultiPageTextPdf(
            m_tmpDir.path(), QStringLiteral("wm_src.pdf"), tokens);
        QVERIFY2(!src.isEmpty(), "fixture creation failed");

        AppContext ctx = makeCtx();
        gp::BatchMode bm;
        bm.setAppContext(&ctx);
        bm.addFilesForTest({src});
        bm.setOperationForTest(2); // OpWatermark

        QLineEdit* wmEdit = watermarkTextEdit(bm);
        QVERIFY2(wmEdit, "watermark text edit (placeholder CONFIDENTIAL) not found");
        wmEdit->setText(QStringLiteral("GLYPHBATCHWM"));

        runAndWait(bm);
        QCOMPARE(bm.successCount(), 1);
        QCOMPARE(bm.failCount(), 0);

        const QString out = tmpPath(QStringLiteral("wm_src_watermarked.pdf"));
        QVERIFY2(QFile::exists(out),
                 "Watermark batch must produce <base>_watermarked.pdf next to the source");

        QPdfDocument outDoc;
        QCOMPARE(outDoc.load(out), QPdfDocument::Error::None);
        QCOMPARE(outDoc.pageCount(), 2);

        for (int p = 0; p < outDoc.pageCount(); ++p) {
            const QString text = outDoc.getAllText(p).text();
            QVERIFY2(text.contains(QStringLiteral("GLYPHBATCHWM")),
                     qPrintable(QStringLiteral("watermark text must be extractable "
                                              "from output page %1 (got: %2)")
                                    .arg(p).arg(text.left(120))));
            QVERIFY2(text.contains(tokens.at(p)),
                     qPrintable(QStringLiteral("original page-%1 content must survive "
                                              "the watermark op (got: %2)")
                                    .arg(p).arg(text.left(120))));
        }
    }

    // ── 2. Export-PDF/A batch — output exists and is structurally identified ──
    void exportPdfABatchWritesStructuralIdentification() {
        const QString src = createMultiPageTextPdf(
            m_tmpDir.path(), QStringLiteral("pdfa_src.pdf"),
            { QStringLiteral("PDFA-SRC-9-12") });
        QVERIFY2(!src.isEmpty(), "fixture creation failed");

        AppContext ctx = makeCtx();
        gp::BatchMode bm;
        bm.setAppContext(&ctx);
        bm.addFilesForTest({src});
        bm.setOperationForTest(3); // OpExportPdfA

        // Select PDF/A-2B (combo item data == 2 → PoDoFoBackend L2B / PDF 1.7).
        QComboBox* level = pdfaLevelCombo(bm);
        QVERIFY2(level, "PDF/A conformance combo (PDF/A-1B item) not found");
        level->setCurrentIndex(level->findText(QStringLiteral("PDF/A-2B")));

        runAndWait(bm);
        QCOMPARE(bm.successCount(), 1);
        QCOMPARE(bm.failCount(), 0);

        const QString out = tmpPath(QStringLiteral("pdfa_src_pdfa.pdf"));
        QVERIFY2(QFile::exists(out),
                 "Export-PDF/A batch must produce <base>_pdfa.pdf next to the source");

        // Structural validation: the output opens in PoDoFo and the catalog
        // carries an OutputIntents entry whose /S names the GTS_PDFA1 scheme.
        QByteArray raw;
        {
            QFile f(out);
            QVERIFY(f.open(QIODevice::ReadOnly));
            raw = f.readAll();
        }
        QVERIFY2(!raw.isEmpty(), "PDF/A output must not be empty");

        try {
            PoDoFo::PdfMemDocument check;
            check.Load(out.toUtf8().constData());
            QCOMPARE(static_cast<int>(check.GetPages().GetCount()), 1);

            const PoDoFo::PdfObject* intents =
                check.GetCatalog().GetDictionary().FindKey(PoDoFo::PdfName("OutputIntents"));
            QVERIFY2(intents && intents->IsArray() && intents->GetArray().GetSize() >= 1,
                     "PDF/A output must declare /OutputIntents in the catalog");
            const PoDoFo::PdfObject* intent = intents->GetArray().FindAt(0);
            QVERIFY2(intent && intent->IsDictionary(),
                     "OutputIntents[0] must be a dictionary");
            const PoDoFo::PdfObject* s = intent->GetDictionary().GetKey(PoDoFo::PdfName("S"));
            QVERIFY2(s && s->IsName(),
                     "OutputIntent must carry a /S name");
            QCOMPARE(QString::fromLatin1(s->GetName().GetString().data(),
                                         int(s->GetName().GetString().size())),
                     QStringLiteral("GTS_PDFA1"));

            // XMP PDF/A identification written via PdfMetadata::SyncXMPMetadata.
            const PoDoFo::PdfALevel reported = check.GetMetadata().GetPdfALevel();
            QVERIFY2(reported != PoDoFo::PdfALevel::Unknown,
                     "the exported document must identify its PDF/A level in XMP "
                     "(pdfaid) — got Unknown on readback");
            QCOMPARE(static_cast<int>(reported), static_cast<int>(PoDoFo::PdfALevel::L2B));
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QStringLiteral("PDF/A output failed to open in PoDoFo: %1")
                                 .arg(e.what())));
        }
        QVERIFY2(raw.contains("pdfaid"),
                 "PDF/A output must carry the XMP pdfaid identification");
    }

    // ── 3. Merge batch — page count == sum of inputs, order == list order ─────
    void mergeBatchConcatenatesPagesInListOrder() {
        const QString a = createMultiPageTextPdf(
            m_tmpDir.path(), QStringLiteral("a.pdf"), { QStringLiteral("MERGE-AAA-FIRST") });
        const QString b = createMultiPageTextPdf(
            m_tmpDir.path(), QStringLiteral("b.pdf"), { QStringLiteral("MERGE-BBB-SECOND") });
        const QString c = createMultiPageTextPdf(
            m_tmpDir.path(), QStringLiteral("c.pdf"), { QStringLiteral("MERGE-CCC-THIRD") });
        QVERIFY2(!a.isEmpty() && !b.isEmpty() && !c.isEmpty(), "fixture creation failed");

        AppContext ctx = makeCtx(); // run-click guard only; merge uses gp::mergeDocuments
        gp::BatchMode bm;
        bm.setAppContext(&ctx);
        bm.addFilesForTest({a, b, c}); // list order defines the merged page order
        bm.setOperationForTest(4);     // OpMerge

        QSignalSpy finishedSpy(&bm, &gp::BatchMode::batchFinished);
        bm.onRunBatch(); // runMerge is synchronous — no worker, no pump needed
        QCOMPARE(finishedSpy.count(), 1);

        // Merged output is named after the FIRST file, in the first file's dir.
        const QString out = tmpPath(QStringLiteral("a_merged.pdf"));
        QVERIFY2(QFile::exists(out),
                 "merge batch must produce <firstBase>_merged.pdf");

        QPdfDocument outDoc;
        QCOMPARE(outDoc.load(out), QPdfDocument::Error::None);
        QCOMPARE(outDoc.pageCount(), 3); // == sum of inputs (1+1+1)

        const QStringList expectedOrder = {
            QStringLiteral("MERGE-AAA-FIRST"),
            QStringLiteral("MERGE-BBB-SECOND"),
            QStringLiteral("MERGE-CCC-THIRD"),
        };
        for (int p = 0; p < outDoc.pageCount(); ++p) {
            const QString text = outDoc.getAllText(p).text();
            QVERIFY2(text.contains(expectedOrder.at(p)),
                     qPrintable(QStringLiteral("merged page %1 must carry %2 (got: %3) "
                                              "— page ORDER must follow list order")
                                    .arg(p).arg(expectedOrder.at(p), text.left(120))));
            // Strict order: a page must not carry a LATER input's token.
            for (int later = p + 1; later < expectedOrder.size(); ++later) {
                QVERIFY2(!text.contains(expectedOrder.at(later)),
                         qPrintable(QStringLiteral("merged page %1 must not contain "
                                                  "later input's token %2")
                                        .arg(p).arg(expectedOrder.at(later))));
            }
        }
    }

    // ── 4. Redact batch — a checked preset checkbox alone excises its matches ─
    void redactBatchPresetCheckboxExcisesMatchedContent() {
        // Three well-separated text RUNS on one page: the email PII flanked by
        // innocent runs. Whole-run excision (Edact-Ray defense) must take the
        // email run only — the neighbours are non-intersecting Tj operators.
        const PageRuns runs = {
            {
                { QStringLiteral("Contact"),        50.0 },
                { QStringLiteral("admin@secret.org"), 200.0 },
                { QStringLiteral("done"),           420.0 },
            },
        };
        const QString src = createMultiRunTextPdf(
            m_tmpDir.path(), QStringLiteral("redact_src.pdf"), runs);
        QVERIFY2(!src.isEmpty(), "fixture creation failed");

        AppContext ctx = makeCtx();
        gp::BatchMode bm;
        bm.setAppContext(&ctx);
        bm.addFilesForTest({src});
        bm.setOperationForTest(6); // OpRedact

        // Drive the §9.12 P1 seam: ONLY the named "email" preset is checked —
        // the free-form pattern edit stays empty, so the preset is the sole
        // source of the redaction patterns.
        auto* emailPreset =
            bm.findChild<QCheckBox*>(QStringLiteral("batchRedactPreset_email"));
        QVERIFY2(emailPreset, "preset checkbox batchRedactPreset_email missing");
        emailPreset->setChecked(true);
        QCOMPARE(bm.checkedRedactPresetKeys(),
                 QStringList{ QStringLiteral("email") });

        runAndWait(bm);
        QCOMPARE(bm.successCount(), 1);
        QCOMPARE(bm.failCount(), 0);

        const QString out = tmpPath(QStringLiteral("redact_src_redacted.pdf"));
        QVERIFY2(QFile::exists(out),
                 "redact batch must produce <base>_redacted.pdf next to the source");

        // The matched PII must be EXCISED (not merely covered) from the output.
        QPdfDocument outDoc;
        QCOMPARE(outDoc.load(out), QPdfDocument::Error::None);
        const QString outText = extractPageText(outDoc, 0);
        QVERIFY2(!outText.contains(QStringLiteral("admin@secret.org")),
                 qPrintable(QStringLiteral("the preset-matched email must be excised "
                                          "from the batch-redacted output (got: %1)")
                                .arg(outText.left(160))));
        // The neighbouring, non-matching runs must survive — the op is surgical
        // at the text-run level (whole-Tj excision, per the Edact-Ray defense).
        QVERIFY2(outText.contains(QStringLiteral("Contact")),
                 qPrintable(QStringLiteral("non-matching run 'Contact' must survive "
                                          "the redaction (got: %1)").arg(outText.left(160))));
        QVERIFY2(outText.contains(QStringLiteral("done")),
                 qPrintable(QStringLiteral("non-matching run 'done' must survive "
                                          "the redaction (got: %1)").arg(outText.left(160))));

#ifdef HAS_PDFIUM
        // Belt and braces on the PDFium seam: the email pattern must find NO
        // matches in the output document.
        const QRegularExpression emailRx =
            PatternRedactor::namedPattern(QStringLiteral("email"));
        QVERIFY(emailRx.isValid());
        const QList<QRectF> leftovers = PatternRedactor::findMatches(out, 0, emailRx);
        QVERIFY2(leftovers.isEmpty(),
                 "PatternRedactor::findMatches must find no email in the output");
#endif
    }

    // ── 5. PDF/A "U" levels are honored ──────────────────────────────────────
    // The Export PDF/A panel offers PDF/A-2U (data 4) and PDF/A-3U (data 5);
    // exportPdfA now maps them for real (previously everything except 2/3
    // silently produced PDF/A-1B — see the §9.12 finding in the ledger).
    void pdfaTwoUComboLevelFallsThroughToL1B() {
        const QString src = createMultiPageTextPdf(
            m_tmpDir.path(), QStringLiteral("pdfa_u_src.pdf"),
            { QStringLiteral("PDFA-U-FINDING") });
        QVERIFY2(!src.isEmpty(), "fixture creation failed");

        AppContext ctx = makeCtx();
        gp::BatchMode bm;
        bm.setAppContext(&ctx);
        bm.addFilesForTest({src});
        bm.setOperationForTest(3); // OpExportPdfA

        QComboBox* level = pdfaLevelCombo(bm);
        QVERIFY2(level, "PDF/A conformance combo not found");
        level->setCurrentIndex(level->findText(QStringLiteral("PDF/A-2U")));

        runAndWait(bm);
        QCOMPARE(bm.successCount(), 1);

        const QString out = tmpPath(QStringLiteral("pdfa_u_src_pdfa.pdf"));
        QVERIFY2(QFile::exists(out), "PDF/A output missing");

        try {
            PoDoFo::PdfMemDocument check;
            check.Load(out.toUtf8().constData());
            const PoDoFo::PdfALevel reported = check.GetMetadata().GetPdfALevel();
            QCOMPARE(static_cast<int>(reported), static_cast<int>(PoDoFo::PdfALevel::L2U));
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QStringLiteral("PDF/A output failed to open in PoDoFo: %1")
                                 .arg(e.what())));
        }
    }
};

QTEST_MAIN(TestBatchOpsCoverage)
#include "TestBatchOpsCoverage.moc"
