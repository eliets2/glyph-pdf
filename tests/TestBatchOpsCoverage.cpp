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
//   6. N03 (P1): every selectable PDF/A level must also write its correct PDF
//      base version — PDF/A-1 ← PDF 1.4, PDF/A-2 ← PDF 1.7, PDF/A-3 ← PDF 1.7
//      (PDF 2.0 is the PDF/A-4 family) — asserted on the saved artifact via
//      PdfMetadata::GetPdfVersion, not only the self-declared PDF/A level.
//   7. E-1: PDF/A conformance validation of the EXPORTED artifacts.
//
//      WHAT IS VALIDATED, ALWAYS (no external tool): the full structural
//      contract of every exported artifact for EVERY selectable level
//      (1B/2B/2U/3B/3U) via PoDoFo readback — XMP pdfaid:part + conformance
//      exactly matching the requested level (decodable AND raw-byte pinned),
//      PDF base version, /OutputIntents[0] with /S == GTS_PDFA1 and
//      OutputConditionIdentifier "sRGB IEC61966-2.1", and no encryption
//      introduced (/Encrypt absent).
//
//      WHAT IS VALIDATED, WHEN A veraPDF CLI IS PRESENT (optional, never a
//      hard dependency): every exported artifact is run through the real
//      veraPDF validator at its flavour. Asserted: (a) veraPDF parses the
//      artifact with no taskException — well-formedness beyond PoDoFo's own
//      reader; (b) NO identification/metadata rule (clause 6.6.x under
//      ISO 19005-2/3, 6.7.x under ISO 19005-1) fails — veraPDF independently
//      confirms the pdfaid identification matches the flavour; (c) a
//      negative control validates the 2B artifact at the 1b flavour and
//      REQUIRES identification failures, proving (b) has teeth; (d) all
//      remaining violations are logged honestly on every run, and any rule
//      OUTSIDE the observed writer-gap classes fails the run.
//      DISCOVERED AND PINNED HONESTLY: the artifacts do NOT reach FULL PDF/A
//      conformance today. The observed writer gaps (PoDoFo-side, outside
//      this lane's ownership): DeviceGray is used without an output-intent
//      profile (clause 6.2.4.3 under ISO 19005-2/3, 6.2.3.3 under
//      ISO 19005-1) because exportPdfA writes /OutputIntents WITHOUT a
//      DestOutputProfile ICC stream; 1b additionally flags an incomplete
//      CIDSet in the embedded font subset's FontDescriptor (clause 6.3.5).
//      When the CLI is absent the conformance pass QSKIPs with that message
//      and only the structural contract above is claimed.
//
//      NOTE ON VeraPdfValidator::validate(): its parseJson() reads the
//      pre-1.26 veraPDF JSON schema (validationResult.result / an array of
//      failedChecks), which no released veraPDF emits — against a real CLI
//      (1.26–1.30) it reports isValid=false with an empty violations list for
//      EVERY document. parseJson/VeraPdfValidator are outside this lane's
//      file ownership, so these tests speak to the CLI directly and parse
//      the real schema (see VeraPdfRawVerdict below). The schema mismatch is
//      reported as a finding for the owner of src/engines/VeraPdfValidator.
//
// Run: QT_QPA_PLATFORM=offscreen ctest -R TestBatchOpsCoverage --output-on-failure
#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPdfDocument>
#include <QProcess>
#include <QPdfSelection>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <podofo/podofo.h>

#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/VeraPdfValidator.h"
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

// ── PDF/A level matrix — every level the batch panel offers ──────────────────
// One row per Export-PDF/A combo item: the PoDoFo level/version the artifact
// must read back as, the veraPDF --flavour that matches it, and the XMP
// pdfaid:part / pdfaid:conformance values the artifact must carry.
struct PdfALevelSpec {
    const char* comboText;
    PoDoFo::PdfALevel level;
    PoDoFo::PdfVersion version;
    const char* flavour;   // veraPDF --flavour flag
    const char* aidPart;   // <pdfaid:part>
    const char* aidConf;   // <pdfaid:conformance>
    const char* fixture;
    const char* output;    // <base>_pdfa.pdf next to the fixture
};

static const QList<PdfALevelSpec>& pdfaLevelSpecs() {
    static const QList<PdfALevelSpec> specs = {
        { "PDF/A-1B", PoDoFo::PdfALevel::L1B, PoDoFo::PdfVersion::V1_4, "1b", "1", "B",
          "pdfa_1b_e1.pdf",         "pdfa_1b_e1_pdfa.pdf" },
        { "PDF/A-2B", PoDoFo::PdfALevel::L2B, PoDoFo::PdfVersion::V1_7, "2b", "2", "B",
          "pdfa_2b_e1.pdf",         "pdfa_2b_e1_pdfa.pdf" },
        { "PDF/A-2U", PoDoFo::PdfALevel::L2U, PoDoFo::PdfVersion::V1_7, "2u", "2", "U",
          "pdfa_2u_e1.pdf",         "pdfa_2u_e1_pdfa.pdf" },
        { "PDF/A-3B", PoDoFo::PdfALevel::L3B, PoDoFo::PdfVersion::V1_7, "3b", "3", "B",
          "pdfa_3b_e1.pdf",         "pdfa_3b_e1_pdfa.pdf" },
        { "PDF/A-3U", PoDoFo::PdfALevel::L3U, PoDoFo::PdfVersion::V1_7, "3u", "3", "U",
          "pdfa_3u_e1.pdf",         "pdfa_3u_e1_pdfa.pdf" },
    };
    return specs;
}

// ── veraPDF raw-CLI verdict (E-1 optional integration) ───────────────────────
// VeraPdfValidator::validate() is the in-app path, but its parseJson() reads a
// schema no released veraPDF emits (see the header note). These tests run the
// CLI directly — SAME discovery (VeraPdfValidator::locateCli: bundle /
// GLYPHPDF_VERAPDF / PATH), SAME .bat handling — and parse the REAL 1.26–1.30
// JSON schema:
//   { "report": { "jobs": [ {
//       "taskException"?: { ... }                    ← parse/IO failure
//       "validationResult": [ { "details": {
//           "passedRules": N, "failedRules": M,
//           "ruleSummaries": [ { "clause": "6.6.4", "testNumber": 1,
//               "status": "failed",
//               "checks": [ { "errorMessage": "..." } ] } ] } } ] } ] } }
// valid == no failed rule anywhere; a taskException means veraPDF could not
// parse the file at all (the artifact is not even well-formed).
struct VeraPdfRawVerdict {
    bool ran = false;          // CLI started and finished
    bool jsonOk = false;       // stdout parsed as veraPDF JSON with a job
    bool definite = false;     // no taskException — a real verdict exists
    bool valid = false;        // failedRules == 0 across all entries
    QStringList failedClauses; // "<clause>-<testNumber>" of every failed rule
    QStringList messages;      // per-check error messages (honest logging)
    QString error;             // process/parse failure description
};

static VeraPdfRawVerdict runVeraPdfRaw(const QString& pdfPath, const QString& flavourFlag) {
    VeraPdfRawVerdict v;
    const QString cli = gp::VeraPdfValidator::locateCli();
    if (cli.isEmpty()) {
        v.error = QStringLiteral("veraPDF CLI not found");
        return v;
    }
    v.ran = true;
    QElapsedTimer stageTimer;
    stageTimer.start();
    qDebug().noquote() << "[E-1] veraPDF CLI:" << cli << "flavour" << flavourFlag
                       << "on" << pdfPath;

    QProcess proc;
    QStringList args;
    if (cli.endsWith(QStringLiteral(".bat"), Qt::CaseInsensitive) ||
        cli.endsWith(QStringLiteral(".cmd"), Qt::CaseInsensitive)) {
        args << QStringLiteral("/c") << QStringLiteral("call") << cli
             << QStringLiteral("--format") << QStringLiteral("json")
             << QStringLiteral("--flavour") << flavourFlag << pdfPath;
        proc.start(QStringLiteral("cmd.exe"), args);
    } else {
        args << QStringLiteral("--format") << QStringLiteral("json")
             << QStringLiteral("--flavour") << flavourFlag << pdfPath;
        proc.start(cli, args);
    }
    if (!proc.waitForStarted(15000)) {
        v.error = QStringLiteral("veraPDF CLI failed to start");
        return v;
    }
    if (!proc.waitForFinished(60000)) {
        proc.kill();
        v.error = QStringLiteral("veraPDF CLI timed out after 60 seconds");
        qDebug().noquote() << "[E-1] CLI stage: TIMED OUT after"
                           << stageTimer.elapsed() << "ms";
        return v;
    }
    qDebug().noquote() << "[E-1] CLI stage: finished, exit" << proc.exitCode()
                       << "in" << stageTimer.elapsed() << "ms";

    QJsonParseError parseErr;
    const QJsonDocument doc =
        QJsonDocument::fromJson(proc.readAllStandardOutput(), &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        v.error = QStringLiteral("veraPDF output is not JSON: ") + parseErr.errorString();
        return v;
    }
    const QJsonArray jobs =
        doc.object()[QStringLiteral("report")].toObject()[QStringLiteral("jobs")].toArray();
    if (jobs.isEmpty()) {
        v.error = QStringLiteral("veraPDF JSON has no jobs");
        return v;
    }
    v.jsonOk = true;

    const QJsonObject job = jobs.first().toObject();
    if (job.contains(QStringLiteral("taskException"))) {
        v.error = QStringLiteral("veraPDF could not parse the document (taskException)");
        return v; // definite == false: not even well-formed
    }
    v.definite = true;

    int failedRulesTotal = 0;
    const QJsonArray results = job[QStringLiteral("validationResult")].toArray();
    for (const QJsonValue& rv : results) {
        const QJsonObject details = rv.toObject()[QStringLiteral("details")].toObject();
        failedRulesTotal += details[QStringLiteral("failedRules")].toInt(0);
        const QJsonArray summaries = details[QStringLiteral("ruleSummaries")].toArray();
        for (const QJsonValue& sv : summaries) {
            const QJsonObject rule = sv.toObject();
            if (rule[QStringLiteral("status")].toString()
                    .compare(QLatin1String("failed"), Qt::CaseInsensitive) != 0)
                continue;
            v.failedClauses << QStringLiteral("%1-%2")
                    .arg(rule[QStringLiteral("clause")].toString(),
                         rule[QStringLiteral("testNumber")].toVariant().toString());
            for (const QJsonValue& cv : rule[QStringLiteral("checks")].toArray())
                v.messages << cv.toObject()[QStringLiteral("errorMessage")].toString();
        }
    }
    v.valid = (failedRulesTotal == 0);
    return v;
}

// Metadata/identification clause classes: 6.6.x under ISO 19005-2/3, 6.7.x
// under ISO 19005-1. A failure in either class means the pdfaid identification
// (or the XMP metadata it lives in) does not match the requested flavour.
static QStringList identificationRuleClauses(const QStringList& clauses) {
    QStringList hits;
    for (const QString& clause : clauses)
        if (clause.startsWith(QLatin1String("6.6")) ||
            clause.startsWith(QLatin1String("6.7")))
            hits << clause;
    return hits;
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

    // E-1/N03: drive the batch Export PDF/A worker at a given conformance combo
    // level and assert the SAVED artifact's FULL STRUCTURAL CONTRACT on
    // readback — PDF/A level (XMP pdfaid, decoded by PoDoFo AND pinned as raw
    // pdfaid:part/conformance bytes), PDF base version, /OutputIntents[0]
    // /S == GTS_PDFA1 with the sRGB OutputConditionIdentifier, and no /Encrypt.
    void runPdfAExportAndCheckContract(const PdfALevelSpec& spec) {
        // Clear any artifacts from a previous slot reusing these names: the
        // batch run pre-checks output paths and asks to overwrite via a MODAL
        // (BatchMode::confirmOverwrite, AR-8 D4) — a modal on a hidden widget
        // in an offscreen test would block until the watchdog kills the run.
        const QString fixturePath = m_tmpDir.filePath(QString::fromLatin1(spec.fixture));
        const QString expectedOut = m_tmpDir.filePath(QString::fromLatin1(spec.output));
        QFile::remove(fixturePath);
        QFile::remove(expectedOut);

        const QString src = createMultiPageTextPdf(
            m_tmpDir.path(), QString::fromLatin1(spec.fixture),
            { QStringLiteral("PDFA-E1-") + spec.comboText });
        QVERIFY2(!src.isEmpty(), "fixture creation failed");

        AppContext ctx = makeCtx();
        gp::BatchMode bm;
        bm.setAppContext(&ctx);
        bm.addFilesForTest({src});
        bm.setOperationForTest(3); // OpExportPdfA

        QComboBox* level = pdfaLevelCombo(bm);
        QVERIFY2(level, "PDF/A conformance combo not found");
        QVERIFY2(level->findText(QString::fromLatin1(spec.comboText)) >= 0,
                 qPrintable(QStringLiteral("combo item %1 missing").arg(spec.comboText)));
        level->setCurrentIndex(level->findText(QString::fromLatin1(spec.comboText)));

        runAndWait(bm);
        QCOMPARE(bm.successCount(), 1);
        QCOMPARE(bm.failCount(), 0);

        const QString out = expectedOut;
        QVERIFY2(QFile::exists(out), "PDF/A output missing");

        QByteArray raw;
        {
            QFile f(out);
            QVERIFY(f.open(QIODevice::ReadOnly));
            raw = f.readAll();
        }
        QVERIFY2(!raw.isEmpty(), "PDF/A output must not be empty");

        // The PDF/A op must never introduce encryption into the artifact.
        QVERIFY2(!raw.contains("/Encrypt"),
                 "PDF/A output must not carry /Encrypt");

        // XMP pdfaid identification, pinned as raw bytes (the serialized
        // part/conformance pair), not only via PoDoFo's decoder.
        const QByteArray aidPart =
            QByteArray("<pdfaid:part>") + spec.aidPart + "</pdfaid:part>";
        const QByteArray aidConf =
            QByteArray("<pdfaid:conformance>") + spec.aidConf + "</pdfaid:conformance>";
        QVERIFY2(raw.contains(aidPart),
                 qPrintable(QStringLiteral("XMP must pin pdfaid:part == %1 (got no '%2')")
                                .arg(spec.aidPart, aidPart.constData())));
        QVERIFY2(raw.contains(aidConf),
                 qPrintable(QStringLiteral("XMP must pin pdfaid:conformance == %1 (got no '%2')")
                                .arg(spec.aidConf, aidConf.constData())));

        try {
            PoDoFo::PdfMemDocument check;
            check.Load(out.toUtf8().constData());

            const PoDoFo::PdfALevel reportedLevel = check.GetMetadata().GetPdfALevel();
            QCOMPARE(static_cast<int>(reportedLevel), static_cast<int>(spec.level));
            const PoDoFo::PdfVersion reportedVersion = check.GetMetadata().GetPdfVersion();
            QCOMPARE(static_cast<int>(reportedVersion),
                     static_cast<int>(spec.version));

            // OutputIntent contract: /S names the GTS_PDFA1 scheme and the
            // sRGB condition identifies the intended color characterization.
            const PoDoFo::PdfObject* intents =
                check.GetCatalog().GetDictionary().FindKey(PoDoFo::PdfName("OutputIntents"));
            QVERIFY2(intents && intents->IsArray() && intents->GetArray().GetSize() >= 1,
                     "PDF/A output must declare /OutputIntents in the catalog");
            const PoDoFo::PdfObject* intent = intents->GetArray().FindAt(0);
            QVERIFY2(intent && intent->IsDictionary(),
                     "OutputIntents[0] must be a dictionary");
            const PoDoFo::PdfObject* s = intent->GetDictionary().GetKey(PoDoFo::PdfName("S"));
            QVERIFY2(s && s->IsName(), "OutputIntent must carry a /S name");
            QCOMPARE(QString::fromLatin1(s->GetName().GetString().data(),
                                         int(s->GetName().GetString().size())),
                     QStringLiteral("GTS_PDFA1"));
            const PoDoFo::PdfObject* oci =
                intent->GetDictionary().GetKey(PoDoFo::PdfName("OutputConditionIdentifier"));
            QVERIFY2(oci && oci->IsString(),
                     "OutputIntent must carry an OutputConditionIdentifier string");
            QCOMPARE(QString::fromLatin1(oci->GetString().GetString().data(),
                                         int(oci->GetString().GetString().size())),
                     QStringLiteral("sRGB IEC61966-2.1"));
        } catch (const std::exception& e) {
            QFAIL(qPrintable(QStringLiteral("PDF/A output failed to open in PoDoFo: %1")
                                 .arg(e.what())));
        }
    }

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
        // Isolate QSettings like every other batch test (never touch real prefs).
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestBatchOpsCoverage"));
        // E-1: honor both spellings of the optional veraPDF CLI override —
        // GLYPHPDF_VERAPDF_CLI (test-lane spelling) aliases the app's
        // GLYPHPDF_VERAPDF consumed by VeraPdfValidator::locateCli().
        const QByteArray cliAlias = qgetenv("GLYPHPDF_VERAPDF_CLI");
        if (!cliAlias.isEmpty() && qEnvironmentVariableIsEmpty("GLYPHPDF_VERAPDF"))
            qputenv("GLYPHPDF_VERAPDF", cliAlias);
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

            // N03: the PDF version of the saved artifact is pinned too —
            // PDF/A-2 is ISO 19005-2, based on PDF 1.7 (ISO 32000-1).
            const PoDoFo::PdfVersion reportedVersion = check.GetMetadata().GetPdfVersion();
            QCOMPARE(static_cast<int>(reportedVersion),
                     static_cast<int>(PoDoFo::PdfVersion::V1_7));
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

    // ── 5/6. Every selectable PDF/A level writes its full contract (E-1/N03) ──
    // Matrix over ALL five combo levels. Per exported artifact (real batch
    // worker, saved bytes): XMP pdfaid:part/conformance exactly matching the
    // requested level (decoded by PoDoFo AND pinned as raw bytes), the correct
    // PDF base version (N03: PDF/A-1 ← 1.4, PDF/A-2/3 ← 1.7; PDF 2.0 is the
    // PDF/A-4 family — the mapping table previously wrote V2_0 under a PDF/A-3
    // identity), /OutputIntents[0] with /S == GTS_PDFA1 and the sRGB
    // OutputConditionIdentifier, and no /Encrypt.
    void everyPdfALevelWritesFullStructuralContract() {
        for (const PdfALevelSpec& spec : pdfaLevelSpecs())
            runPdfAExportAndCheckContract(spec);
    }

    // ── 7. E-1: veraPDF conformance validation of every exported artifact ────
    // OPTIONAL integration — runs only when a veraPDF CLI is discoverable
    // (bundle / GLYPHPDF_VERAPDF / GLYPHPDF_VERAPDF_CLI / PATH); otherwise it
    // QSKIPs so the structural contract above remains the always-on claim.
    //
    // Asserted per exported artifact at its matching flavour:
    //   * the CLI runs and returns parseable JSON with a real verdict — the
    //     artifact is well-formed enough that veraPDF raises NO taskException;
    //   * NO identification/metadata rule (clause 6.6.x under ISO 19005-2/3,
    //     6.7.x under ISO 19005-1) fails — veraPDF independently confirms the
    //     pdfaid identification matches the flavour;
    //   * the remaining violations are logged honestly on every run. They are
    //     EXPECTED today (writer gaps outside this lane's ownership, see the
    //     ledger discovery note in the file header): DeviceGray without an
    //     output-intent profile (6.2.4.3 under 19005-2/3, 6.2.3.3 under
    //     19005-1) and an incomplete CIDSet on the 1b embedded subset (6.3.5).
    //     FULL conformance is claimed ONLY as far as "no rule outside the
    //     documented writer-gap classes fails". If a run reports a clause
    //     outside that set the test FAILS — the writer's behaviour changed
    //     and the ledger must be re-examined.
    void veraPdfValidatesEveryPdfALevelArtifact() {
        if (!gp::VeraPdfValidator::isAvailable())
            QSKIP("veraPDF CLI not found (bundle / GLYPHPDF_VERAPDF / GLYPHPDF_VERAPDF_CLI "
                  "/ PATH) — full-conformance validation of the exported artifacts is "
                  "UNVERIFIED in this run; the structural contract is still asserted by "
                  "everyPdfALevelWritesFullStructuralContract");

        for (const PdfALevelSpec& spec : pdfaLevelSpecs()) {
            runPdfAExportAndCheckContract(spec);
            const QString out = tmpPath(QString::fromLatin1(spec.output));
            const VeraPdfRawVerdict v =
                runVeraPdfRaw(out, QString::fromLatin1(spec.flavour));

            QVERIFY2(v.ran, qPrintable(QStringLiteral("PDF/A-%1: CLI did not run: %2")
                                           .arg(spec.flavour, v.error)));
            QVERIFY2(v.jsonOk, qPrintable(QStringLiteral("PDF/A-%1: %2")
                                              .arg(spec.flavour, v.error)));
            QVERIFY2(v.definite,
                     qPrintable(QStringLiteral("PDF/A-%1: exported artifact is not even "
                                               "well-formed to veraPDF: %2")
                                    .arg(spec.flavour, v.error)));

            // Honest ledger FIRST: every failed rule is logged on every run,
            // so full-conformance progress is visible in green runs too.
            qInfo().nospace().noquote()
                << "[E-1] PDF/A-" << spec.flavour << " artifact: "
                << (v.valid ? QStringLiteral("FULLY CONFORMANT")
                            : QStringLiteral("not fully conformant — failed rules: ")
                                  + v.failedClauses.join(QStringLiteral(", ")));
            for (const QString& msg : v.messages)
                qInfo().noquote() << "[E-1]   -" << msg;

            // Identification contract — no metadata/identification rule may fail.
            const QStringList identificationFailures =
                identificationRuleClauses(v.failedClauses);
            QVERIFY2(identificationFailures.isEmpty(),
                     qPrintable(QStringLiteral("PDF/A-%1: identification/metadata rules "
                                               "FAILED: %2")
                                    .arg(spec.flavour,
                                         identificationFailures.join(QStringLiteral(", ")))));

            // Honest ledger: every violation outside the OBSERVED writer-gap
            // classes is a new fact and must fail the run (tripwire: if the
            // writer changes, this fails and the E-1 ledger must be re-read).
            // Observed today (see the [E-1] log lines): DeviceGray used
            // without an output-intent profile (6.2.4.3 under ISO 19005-2/3,
            // 6.2.3.3 under ISO 19005-1) — exportPdfA writes /OutputIntents
            // without a DestOutputProfile ICC stream — and, 1b only, an
            // incomplete CIDSet in the FontDescriptor of the embedded subset
            // (6.3.5). Repair lives in the writer, outside this lane.
            QStringList unexpected;
            for (const QString& clause : v.failedClauses)
                if (!identificationRuleClauses({clause}).isEmpty() ||
                    !(clause.startsWith(QLatin1String("6.2.3.3")) ||
                      clause.startsWith(QLatin1String("6.2.4.3")) ||
                      clause.startsWith(QLatin1String("6.3.5"))))
                    unexpected << clause;
            QVERIFY2(unexpected.isEmpty(),
                     qPrintable(QStringLiteral("PDF/A-%1: violations outside the documented "
                                               "writer-gap classes {6.2.3.3, 6.2.4.3, 6.3.5, "
                                               "6.6.x/6.7.x} appeared: %2 — the writer "
                                               "changed; re-examine the E-1 ledger")
                                    .arg(spec.flavour, unexpected.join(QStringLiteral(", ")))));
        }

        // Negative control for the harness itself: validate the 2B artifact at
        // the 1b flavour — veraPDF must FLAG the identification mismatch (the
        // artifact declares pdfaid part 2, the profile expects part 1). This
        // proves the "no identification failures" assertions above have teeth:
        // the validator really reads the pdfaid the export writes.
        {
            runPdfAExportAndCheckContract(pdfaLevelSpecs().at(1)); // PDF/A-2B row
            const QString out2b = tmpPath(QString::fromLatin1(pdfaLevelSpecs().at(1).output));
            const VeraPdfRawVerdict mismatch =
                runVeraPdfRaw(out2b, QStringLiteral("1b"));
            QVERIFY2(mismatch.definite,
                     "mismatch control: 2b artifact must still parse at flavour 1b");
            const QStringList idFailures =
                identificationRuleClauses(mismatch.failedClauses);
            QVERIFY2(!idFailures.isEmpty(),
                     qPrintable(QStringLiteral("mismatch control: validating the PDF/A-2B "
                                               "artifact at flavour 1b must produce "
                                               "identification-rule failures, got failed "
                                               "rules: %1")
                                    .arg(mismatch.failedClauses.join(QStringLiteral(", ")))));
            qInfo().nospace().noquote()
                << "[E-1] mismatch control (2b artifact at flavour 1b) flagged: "
                << idFailures.join(QStringLiteral(", "));
        }
    }
};

QTEST_MAIN(TestBatchOpsCoverage)
#include "TestBatchOpsCoverage.moc"
