// SPDX-License-Identifier: Apache-2.0
// SWEEP-W3-PERF document-operations measurement driver.
//
// Purpose: MEASURE the real engine/operation paths for the W3 performance
// baseline (docs/audit/PERF-BASELINE-2026-09-20.md). One scenario per process
// invocation so the process-lifetime peak working set is attributable to that
// scenario alone (PeakWorkingSetSize is monotonic — disclosed in every run).
//
// Scenarios (argv):
//   open-engine <pdf> <iters>     QPdfDocument load + first-page render (2x)
//                                 + full-pagination render — the engine floor
//                                 under QPdfView (the render_path_profile basis).
//   open-app    <pdfCopyA> <pdfCopyB> <iters>
//                                 REAL MainWindow::openDocument round-robin over
//                                 two byte-identical copies (alternating paths
//                                 forces a real load every iteration); timed
//                                 call->pageCount>0 (the async contract the GUI
//                                 tests wait on).
//   redact      <pdf> <iters>     PdfEditorEngine load + applyRedactions(3 small
//                                 rects on page 0) + saveDocument — full op.
//   ocrskip     <pdf> <iters>     THE Q1/N3 skip-decision probe exactly as
//                                 BatchMode runs it: PdfiumBackend::loadDocument
//                                 + per-page extractPageTextRuns. A text page is
//                                 KEPT — near-zero work beyond the probe.
//   compare     <a> <b> <iters>   DiffEngine::compare (150 DPI default).
//   convert     <pdf> <word|excel> <iters>
//                                 ConversionManager::convertTo native OOXML
//                                 export (docx/xlsx).
//   sign        <pdf> <p12> <iters>
//                                 SignatureManager::signDocument, local P12
//                                 (tests/fixtures/signing/test_signer.p12,
//                                 password "test"), NO TSA url set.
//   save        <pdf> <iters>     Engine load + saveDocument (save-as round trip).
//   batch       <corpusDir> <outDir>
//                                 50 files through the REAL BatchMode preset
//                                 pipeline (compress q50 preset), onRunBatch ->
//                                 batchFinished. Single sample per invocation.
//   mem-open    <pdf>             Single open + full pagination; peak WS printed.
//   mem-compare <a> <b>           Single compare; peak WS printed.
//
// Output: ONE JSON line per invocation:
//   {"scenario":..,"samples_ms":[..],"peak_ws_bytes":..,"extra":{..}}
//
// LIMITS (stated honestly):
//   * QT_QPA_PLATFORM=offscreen (set by the spawning suite before the process
//     starts) — CPU-side costs only, no compositor.
//   * Timing anchor for async app-level operations is the same condition the
//     GUI tests use (pageCount/finish signals) polled with QElapsedTimer.
//   * Samples are machine-dependent raw numbers; the suite reports
//     median/p95/min/max plus the concurrent-load context captured around
//     every invocation.
#include <QApplication>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPdfDocument>
#include <QLineEdit>
#include <QSlider>
#include <QTimer>
#include <algorithm>
#include <cstdio>
#include <memory>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#endif

#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "engines/DiffEngine.h"
#include "engines/PdfEditorEngine.h"
#include "engines/ConversionManager.h"
#include "engines/SignatureManager.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "modes/BatchMode.h"
#include "ui/PdfViewerWidget.h" // pdfViewer()->pageCount() — GpMainWindow.h only forward-declares
#include "GpMainWindow.h"

using gp::BatchMode;

namespace {

qint64 peakWorkingSetBytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    return GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))
               ? static_cast<qint64>(pmc.PeakWorkingSetSize) : -1;
#else
    return -1;
#endif
}

void emitJson(const QString &scenario, const QList<qint64> &samples,
              const QJsonObject &extra)
{
    QJsonObject o;
    o.insert(QStringLiteral("scenario"), scenario);
    QJsonArray arr;
    for (qint64 s : samples) arr.append(static_cast<double>(s));
    o.insert(QStringLiteral("samples_ms"), arr);
    o.insert(QStringLiteral("peak_ws_bytes"), static_cast<double>(peakWorkingSetBytes()));
    o.insert(QStringLiteral("extra"), extra);
    std::fprintf(stdout, "%s\n",
                 QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
    std::fflush(stdout);
}

// Render every page of `pdf` at `scale` x page points; returns total ms.
qint64 paginate(QPdfDocument &pdf, double scale)
{
    QElapsedTimer t; t.start();
    const int pages = pdf.pageCount();
    for (int i = 0; i < pages; ++i) {
        const QSizeF pts = pdf.pagePointSize(i);
        const QSize px(qMax(1, int(pts.width() * scale)),
                       qMax(1, int(pts.height() * scale)));
        QImage img = pdf.render(i, px);
        Q_UNUSED(img);
    }
    return t.elapsed();
}

int runOpenEngine(const QString &pdf, int iters)
{
    QList<qint64> first, full;
    QJsonObject extra;
    for (int it = 0; it < iters; ++it) {
        QPdfDocument pdfDoc;
        QElapsedTimer t; t.start();
        pdfDoc.load(pdf);
        if (pdfDoc.status() != QPdfDocument::Status::Ready) {
            std::fprintf(stderr, "load failed: %s\n", qPrintable(pdf));
            return 1;
        }
        // open-to-first-page: load + render page 0 at 2x page points (the
        // convention render_path_profile/the corrected hot-path doc report).
        {
            const QSizeF pts = pdfDoc.pagePointSize(0);
            const QSize px(qMax(1, int(pts.width() * 2.0)),
                           qMax(1, int(pts.height() * 2.0)));
            QImage img = pdfDoc.render(0, px);
            Q_UNUSED(img);
        }
        first.append(t.elapsed());
        full.append(paginate(pdfDoc, 2.0));
    }
    extra.insert(QStringLiteral("iters"), iters);
    extra.insert(QStringLiteral("metric"), QStringLiteral("first_ms=load+page0@2x; full_ms=all-pages@2x"));
    emitJson(QStringLiteral("open-engine-first"), first, extra);
    emitJson(QStringLiteral("open-engine-full-pagination"), full, extra);
    return 0;
}

int runOpenApp(const QString &pdfA, const QString &pdfB, int iters)
{
    auto ctx = Bootstrapper::createContext();
    gp::MainWindow win(std::move(ctx));
    win.resize(1280, 800);
    win.show();
    for (int i = 0; i < 50; ++i)
        QApplication::processEvents(QEventLoop::AllEvents, 20);

    QList<qint64> samples;
    for (int it = 0; it < iters; ++it) {
        const QString &path = (it % 2 == 0) ? pdfA : pdfB;
        QElapsedTimer t; t.start();
        win.openDocument(path);
        // The GUI tests' readiness contract: pdfViewer()->pageCount() > 0.
        while (win.pdfViewer()->pageCount() <= 0 && t.elapsed() < 120000)
            QApplication::processEvents(QEventLoop::AllEvents, 1);
        samples.append(t.elapsed());
    }
    QJsonObject extra;
    extra.insert(QStringLiteral("iters"), iters);
    extra.insert(QStringLiteral("route"), QStringLiteral("MainWindow::openDocument -> pageCount>0"));
    emitJson(QStringLiteral("open-app"), samples, extra);
    return 0;
}

int runRedact(const QString &pdf, int iters, const QString &outDir)
{
    QList<qint64> samples;
    int failures = 0;
    const QList<QRectF> marks = {
        QRectF(72, 72, 200, 30), QRectF(72, 120, 300, 30), QRectF(72, 168, 250, 30)
    };
    for (int it = 0; it < iters; ++it) {
        const QString out = outDir + QStringLiteral("/redact-out-%1.pdf").arg(it);
        QElapsedTimer t; t.start();
        PdfEditorEngine engine;
        bool ok = engine.loadDocumentForEditing(pdf);
        if (ok) ok = engine.applyRedactions(0, marks);
        if (ok) ok = engine.saveDocument(out);
        const qint64 ms = t.elapsed();
        if (!ok) ++failures;
        samples.append(ms);
        QFile::remove(out);
    }
    QJsonObject extra;
    extra.insert(QStringLiteral("iters"), iters);
    extra.insert(QStringLiteral("failures"), failures);
    extra.insert(QStringLiteral("marks_per_page"), 3);
    emitJson(QStringLiteral("redact-apply"), samples, extra);
    return failures == 0 ? 0 : 1;
}

int runOcrSkip(const QString &pdf, int iters)
{
    QList<qint64> samples;
    int pagesProbed = 0, textPages = 0;
    for (int it = 0; it < iters; ++it) {
        QElapsedTimer t; t.start();
        PdfiumBackend probe;
        if (!probe.loadDocument(pdf)) {
            std::fprintf(stderr, "pdfium probe load failed\n");
            return 1;
        }
        // EXACTLY the BatchMode decision loop: first non-empty run marks the
        // page as having text (near-zero work — page is KEPT, never rendered
        // nor OCRed).
        for (int p = 0; p < probe.pageCount(); ++p) {
            bool has = false;
            const auto runs = probe.extractPageTextRuns(p);
            for (const auto &run : runs) {
                if (!run.text.trimmed().isEmpty()) { has = true; break; }
            }
            if (it == 0) { ++pagesProbed; if (has) ++textPages; }
        }
        samples.append(t.elapsed());
    }
    QJsonObject extra;
    extra.insert(QStringLiteral("iters"), iters);
    extra.insert(QStringLiteral("pages_probed"), pagesProbed);
    extra.insert(QStringLiteral("text_pages"), textPages);
    extra.insert(QStringLiteral("decision"), QStringLiteral("skipped (text layer present)"));
    emitJson(QStringLiteral("ocr-skip-decision"), samples, extra);
    return 0;
}

int runCompare(const QString &a, const QString &b, int iters)
{
    DiffEngine engine;
    QList<qint64> samples;
    QJsonObject extra;
    for (int it = 0; it < iters; ++it) {
        QElapsedTimer t; t.start();
        const DiffResult r = engine.compare(a, b);
        const qint64 ms = t.elapsed();
        samples.append(ms);
        if (it == 0) {
            extra.insert(QStringLiteral("result_pages"), r.pages.size());
            extra.insert(QStringLiteral("page_count1"), r.pageCount1);
            extra.insert(QStringLiteral("page_count2"), r.pageCount2);
            extra.insert(QStringLiteral("cancelled"), r.cancelled);
        }
    }
    extra.insert(QStringLiteral("iters"), iters);
    emitJson(QStringLiteral("compare"), samples, extra);
    return 0;
}

int runConvert(const QString &pdf, const QString &fmt, int iters, const QString &outDir)
{
    ConversionManager conv;
    const auto format = (fmt == QStringLiteral("excel"))
                            ? ConversionManager::TargetFormat::Excel
                            : ConversionManager::TargetFormat::Word;
    const QString ext = (fmt == QStringLiteral("excel")) ? QStringLiteral("xlsx")
                                                         : QStringLiteral("docx");
    QList<qint64> samples;
    int failures = 0;
    for (int it = 0; it < iters; ++it) {
        const QString out = outDir + QStringLiteral("/conv-out-%1.%2").arg(it).arg(ext);
        QElapsedTimer t; t.start();
        const bool ok = conv.convertTo(pdf, out, format);
        const qint64 ms = t.elapsed();
        samples.append(ms);
        if (!ok || !QFileInfo::exists(out)) ++failures;
        QFile::remove(out);
    }
    QJsonObject extra;
    extra.insert(QStringLiteral("format"), ext);
    extra.insert(QStringLiteral("iters"), iters);
    extra.insert(QStringLiteral("failures"), failures);
    emitJson(QStringLiteral("convert-export"), samples, extra);
    return failures == 0 ? 0 : 1;
}

int runSign(const QString &pdf, const QString &p12, int iters, const QString &outDir)
{
    SignatureManager mgr;    // no setTsaUrl -> NO timestamp authority (local only)
    QList<qint64> samples;
    int failures = 0;
    for (int it = 0; it < iters; ++it) {
        const QString out = outDir + QStringLiteral("/sign-out-%1.pdf").arg(it);
        QElapsedTimer t; t.start();
        const SignOutcome r = mgr.signDocument(pdf, out, p12,
                                               QStringLiteral("test"),
                                               QStringLiteral("PerfBaseline"),
                                               QString());
        const qint64 ms = t.elapsed();
        samples.append(ms);
        if (r != SignOutcome::Success) ++failures;
        QFile::remove(out);
    }
    QJsonObject extra;
    extra.insert(QStringLiteral("iters"), iters);
    extra.insert(QStringLiteral("failures"), failures);
    extra.insert(QStringLiteral("tsa"), QStringLiteral("none (local P12)"));
    emitJson(QStringLiteral("sign-local-p12"), samples, extra);
    return failures == 0 ? 0 : 1;
}

int runSave(const QString &pdf, int iters, const QString &outDir)
{
    QList<qint64> samples;
    int failures = 0;
    for (int it = 0; it < iters; ++it) {
        const QString out = outDir + QStringLiteral("/save-out-%1.pdf").arg(it);
        QElapsedTimer t; t.start();
        PdfEditorEngine engine;
        bool ok = engine.loadDocumentForEditing(pdf);
        if (ok) ok = engine.saveDocument(out);
        const qint64 ms = t.elapsed();
        samples.append(ms);
        if (!ok) ++failures;
        QFile::remove(out);
    }
    QJsonObject extra;
    extra.insert(QStringLiteral("iters"), iters);
    extra.insert(QStringLiteral("failures"), failures);
    emitJson(QStringLiteral("save-roundtrip"), samples, extra);
    return failures == 0 ? 0 : 1;
}

int runBatch(const QString &corpusDir, const QString &outDir)
{
    QDir().mkpath(outDir);
    QStringList files;
    const QFileInfoList entries =
        QDir(corpusDir).entryInfoList(QStringList() << QStringLiteral("*.pdf"),
                                      QDir::Files, QDir::Name);
    for (const QFileInfo &fi : entries) files << fi.absoluteFilePath();
    if (files.size() != 50) {
        std::fprintf(stderr, "batch corpus must hold 50 files, found %lld\n",
                     static_cast<long long>(files.size()));
        return 1;
    }

    // Isolated preset store (the BatchMode settings-isolation seam).
    const QString storeDir = outDir + QStringLiteral("/preset-store");
    QDir().mkpath(storeDir);
    BatchMode::setPresetStoreDirForTest(storeDir);

    auto ctx = Bootstrapper::createContext();

    BatchMode bm;
    bm.setAppContext(&ctx);
    bm.setOperationForTest(1 /* OpCompress — the TestBatchPresets op index */);
    const auto sliders = bm.findChildren<QSlider *>();
    if (!sliders.isEmpty()) sliders.first()->setValue(50);

    QString err;
    if (!bm.saveConfiguredOpAsPresetForTest(QStringLiteral("Perf Baseline"), &err)) {
        std::fprintf(stderr, "preset save failed: %s\n", qPrintable(err));
        return 1;
    }
    if (!bm.selectPresetForTest(QStringLiteral("perf-baseline"))) {
        std::fprintf(stderr, "preset select failed\n");
        return 1;
    }
    auto *outEdit = bm.findChild<QLineEdit *>(QStringLiteral("batchPresetOutDir"));
    if (!outEdit) {
        std::fprintf(stderr, "batchPresetOutDir line edit not found\n");
        return 1;
    }
    outEdit->setText(outDir);

    bool finished = false;
    int success = -1, failed = -1, skipped = -1;
    QObject::connect(&bm, &BatchMode::batchFinished, &bm,
                     [&] { finished = true; }, Qt::DirectConnection);

    QElapsedTimer t; t.start();
    bm.addFilesForTest(files);
    bm.onRunBatch();
    while (!finished && t.elapsed() < 600000)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    const qint64 ms = t.elapsed();
    success = bm.successCount();
    failed = bm.failCount();
    skipped = bm.skipCount();

    QJsonObject extra;
    extra.insert(QStringLiteral("files"), files.size());
    extra.insert(QStringLiteral("success"), success);
    extra.insert(QStringLiteral("failed"), failed);
    extra.insert(QStringLiteral("skipped"), skipped);
    extra.insert(QStringLiteral("finished"), finished);
    extra.insert(QStringLiteral("per_file_mean_ms"),
                 success > 0 ? static_cast<double>(ms) / success : -1.0);
    emitJson(QStringLiteral("batch-50-preset-compress"), QList<qint64>{ ms }, extra);
    return (finished && failed == 0) ? 0 : 1;
}

int runMemOpen(const QString &pdf)
{
    QPdfDocument pdfDoc;
    QElapsedTimer t; t.start();
    pdfDoc.load(pdf);
    const qint64 loadMs = t.elapsed();
    const qint64 pagMs = paginate(pdfDoc, 2.0);
    QJsonObject extra;
    extra.insert(QStringLiteral("load_ms"), static_cast<double>(loadMs));
    extra.insert(QStringLiteral("pagination_ms"), static_cast<double>(pagMs));
    extra.insert(QStringLiteral("pages"), pdfDoc.pageCount());
    emitJson(QStringLiteral("mem-open-large"), QList<qint64>{ loadMs + pagMs }, extra);
    return 0;
}

// Per-scale cold page render (the "scrolling/zoom" target row: record render
// latency at each supported scale). Scales are the clamped zoom domain
// [0.1, 16.0] sampled at 0.5/1/2/4/8/16; the shared 64 Mpx render guard
// bounds the largest (production behaviour, unchanged by this lane).
int runPageScale(const QString &pdf)
{
    QPdfDocument pdfDoc;
    pdfDoc.load(pdf);
    if (pdfDoc.status() != QPdfDocument::Status::Ready) {
        std::fprintf(stderr, "load failed: %s\n", qPrintable(pdf));
        return 1;
    }
    QJsonArray perScale;
    const QList<double> scales = { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
    for (double s : scales) {
        const QSizeF pts = pdfDoc.pagePointSize(0);
        const QSize px(qMax(1, int(pts.width() * s)), qMax(1, int(pts.height() * s)));
        QElapsedTimer t; t.start();
        QImage img = pdfDoc.render(0, px);
        const qint64 ms = t.elapsed();
        QJsonObject o;
        o.insert(QStringLiteral("scale"), s);
        o.insert(QStringLiteral("px_w"), px.width());
        o.insert(QStringLiteral("px_h"), px.height());
        o.insert(QStringLiteral("render_ms"), static_cast<double>(ms));
        o.insert(QStringLiteral("null_image"), img.isNull());
        perScale.append(o);
    }
    QJsonObject extra;
    extra.insert(QStringLiteral("page"), 0);
    extra.insert(QStringLiteral("perScale"), perScale);
    emitJson(QStringLiteral("page-render-per-scale"), QList<qint64>{}, extra);
    return 0;
}

int runMemCompare(const QString &a, const QString &b)
{
    DiffEngine engine;
    QElapsedTimer t; t.start();
    const DiffResult r = engine.compare(a, b);
    const qint64 ms = t.elapsed();
    QJsonObject extra;
    extra.insert(QStringLiteral("elapsed_ms"), static_cast<double>(ms));
    extra.insert(QStringLiteral("result_pages"), r.pages.size());
    emitJson(QStringLiteral("mem-compare-50"), QList<qint64>{ ms }, extra);
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("GlyphPDF"));
    QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFPerf"));
    if (argc < 3) {
        std::fprintf(stderr,
                     "usage: perf_docops <scenario> <args...>\n"
                     "  scenarios: open-engine open-app redact ocrskip compare\n"
                     "             convert sign save batch mem-open mem-compare\n");
        return 2;
    }
    const QString scenario = argv[1];
    // Transient op outputs go to GLYPHPDF_PERF_OUTDIR when set — the suite
    // points this at a roomy disk (D: runs at 100% capacity; see the baseline
    // doc's environment notes). Fallback: beside the driver exe.
    const QString outDir = [] {
        const QString fromEnv = qEnvironmentVariable("GLYPHPDF_PERF_OUTDIR");
        return fromEnv.isEmpty()
                   ? QCoreApplication::applicationDirPath() + QStringLiteral("/perf-out")
                   : fromEnv;
    }();
    QDir().mkpath(outDir);

    if (scenario == QStringLiteral("open-engine") && argc >= 3)
        return runOpenEngine(argv[2], argc >= 4 ? atoi(argv[3]) : 10);
    if (scenario == QStringLiteral("open-app") && argc >= 4)
        return runOpenApp(argv[2], argv[3], argc >= 5 ? atoi(argv[4]) : 10);
    if (scenario == QStringLiteral("redact") && argc >= 3)
        return runRedact(argv[2], argc >= 4 ? atoi(argv[3]) : 10, outDir);
    if (scenario == QStringLiteral("ocrskip") && argc >= 3)
        return runOcrSkip(argv[2], argc >= 4 ? atoi(argv[3]) : 10);
    if (scenario == QStringLiteral("compare") && argc >= 4)
        return runCompare(argv[2], argv[3], argc >= 5 ? atoi(argv[4]) : 5);
    if (scenario == QStringLiteral("convert") && argc >= 4)
        return runConvert(argv[2], argv[3], argc >= 5 ? atoi(argv[4]) : 10, outDir);
    if (scenario == QStringLiteral("sign") && argc >= 4)
        return runSign(argv[2], argv[3], argc >= 5 ? atoi(argv[4]) : 10, outDir);
    if (scenario == QStringLiteral("save") && argc >= 3)
        return runSave(argv[2], argc >= 4 ? atoi(argv[3]) : 10, outDir);
    if (scenario == QStringLiteral("batch") && argc >= 4)
        return runBatch(argv[2], argv[3]);
    if (scenario == QStringLiteral("mem-open") && argc >= 3)
        return runMemOpen(argv[2]);
    if (scenario == QStringLiteral("pagescale") && argc >= 3)
        return runPageScale(argv[2]);
    if (scenario == QStringLiteral("mem-compare") && argc >= 4)
        return runMemCompare(argv[2], argv[3]);

    std::fprintf(stderr, "unknown scenario or wrong arg count: %s\n", qPrintable(scenario));
    return 2;
}
