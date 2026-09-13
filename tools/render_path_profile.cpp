// SPDX-License-Identifier: Apache-2.0
// R12 (PERF-04) render-path measurement harness.
//
// Purpose: produce MEASURED numbers for the real rendering paths —
//   (1) single-page view (QPdfView) navigation: QPdfDocument::render, the
//       allocator underneath QPdfView, cold vs warm per page,
//   (2) the custom two-page spread path (renderPage at zoom*2),
//   (3) the thumbnail path (ThumbnailSidebar's RenderCache + renderer seam),
//   (4) guard behaviour on the named extremes (large MediaBox, extreme zoom,
//       NaN scale, infinite scale, rapid zoom reversal).
// and record cold first-page time + repeated-page cache behaviour on the
// real view classes (offscreen platform).
//
// LIMITS (stated honestly):
//   * Runs with QT_QPA_PLATFORM=offscreen — no real GPU/compositor, no user
//     input timing; this measures CPU render + allocation behaviour, not
//     paint/compositor latency.
//   * QPdfView's internal page cache is Qt-owned and cannot be instrumented
//     from outside; the harness measures QPdfDocument::render directly (the
//     allocator underneath QPdfView) for cold/warm page costs, and the
//     widget-level paths we own end-to-end.
//   * ThumbnailSidebar renders lazily per visible widget; the harness measures
//     the exact seam it uses (RenderCache::getOrRender over a renderer that
//     calls PdfViewerWidget::renderPage at 75 DPI) across ALL pages.
//   * Numbers are machine-dependent; raw samples and medians only — no
//     derived percentages.
//
// Usage: render_path_profile <output.json>
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>
#include <QTemporaryDir>
#include <QMarginsF>
#include <QPageSize>
#include <algorithm>
#include <cstdio>
#include <limits>
#include <windows.h>
#include <psapi.h>

#include "core/interfaces/IPdfRenderer.h"
#include "engines/RenderCache.h"
#include "ui/PdfViewerWidget.h"

namespace {

constexpr int kProfilePages = 12;
constexpr double kTwoPageZoom = 1.0;         // viewer zoom; spread renders at zoom*2
constexpr double kThumbDpi = 75.0;

qint64 peakWorkingSetBytes() {
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    return GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))
               ? static_cast<qint64>(pmc.PeakWorkingSetSize)
               : -1;
}

QString makeDocPdf(const QTemporaryDir &dir, const QString &name, int pages,
                   QSizeF pageSizePt = {612.0, 792.0}) {
    const QString path = dir.filePath(name);
    QPdfWriter w(path);
    w.setPageSize(QPageSize(pageSizePt, QPageSize::Point));
    w.setPageMargins(QMarginsF(0, 0, 0, 0));
    w.setResolution(72);
    QPainter p(&w);
    for (int i = 0; i < pages; ++i) {
        if (i > 0) w.newPage();
        p.drawText(72, 200,
                   QStringLiteral("Render-path profile page %1 of %2").arg(i + 1).arg(pages));
    }
    p.end();
    return path;
}

QJsonArray sampleArray(const QVector<double> &ms) {
    QJsonArray a;
    for (double v : ms) a.append(v);
    return a;
}

QJsonObject summarize(const QVector<double> &ms) {
    QJsonObject o;
    if (ms.isEmpty()) { o.insert(QStringLiteral("n"), 0); return o; }
    QVector<double> sorted = ms;
    std::sort(sorted.begin(), sorted.end());
    o.insert(QStringLiteral("n"), ms.size());
    o.insert(QStringLiteral("medianMs"), sorted[sorted.size() / 2]);
    o.insert(QStringLiteral("minMs"), sorted.first());
    o.insert(QStringLiteral("maxMs"), sorted.last());
    o.insert(QStringLiteral("samples"), sampleArray(ms));
    return o;
}

// The exact seam ThumbnailSidebar uses: RenderCache over a renderer that
// forwards to PdfViewerWidget::renderPage (points→pixels scale = dpi/72).
class ViewerRenderer : public IPdfRenderer {
public:
    explicit ViewerRenderer(PdfViewerWidget *viewer) : m_viewer(viewer) {}
    QImage renderPage(int pageIndex, int dpi) override {
        return m_viewer->renderPage(pageIndex, static_cast<qreal>(dpi) / 72.0);
    }
    QImage renderTile(int pageIndex, const QRectF &, int dpi) override {
        return renderPage(pageIndex, dpi);
    }
    QSizeF pageSize(int pageIndex) const override {
        return m_viewer->document() ? m_viewer->document()->pagePointSize(pageIndex)
                                    : QSizeF();
    }
    QString extractText(int) override { return QString(); }
private:
    PdfViewerWidget *m_viewer;
};

void requireImage(const QImage &img, const char *what) {
    if (img.isNull()) {
        std::fprintf(stderr, "FAIL: %s produced a null image\n", what);
        std::exit(5);
    }
}

} // namespace

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    if (argc < 2) { std::fprintf(stderr, "usage: render_path_profile <out.json>\n"); return 2; }

    QTemporaryDir dir;
    if (!dir.isValid()) return 3;

    QJsonObject report;
    report.insert(QStringLiteral("method"), QStringLiteral(
        "offscreen QApplication; real PdfViewerWidget/QPdfView/RenderCache classes; "
        "raw per-operation samples + medians; no derived percentages"));

    // ── Fixtures ────────────────────────────────────────────────────────────
    const QString letterPdf = makeDocPdf(dir, QStringLiteral("letter.pdf"), kProfilePages);
    const QString hugePdf = makeDocPdf(dir, QStringLiteral("huge.pdf"), 1, {40000.0, 40000.0});

    // ── (1) QPdfView's allocator: QPdfDocument::render cold vs warm ────────
    {
        QPdfDocument doc;
        doc.load(letterPdf);
        QVector<double> cold, warm;
        for (int i = 0; i < kProfilePages; ++i) {
            const QSizeF ps = doc.pagePointSize(i);
            QSize px(int(ps.width() * 2.0), int(ps.height() * 2.0));
            QElapsedTimer t;
            t.start();
            QImage img = doc.render(i, px, QPdfDocumentRenderOptions());
            cold.append(double(t.nsecsElapsed()) / 1e6);
            requireImage(img, "QPdfDocument::render cold");
            t.restart();
            img = doc.render(i, px, QPdfDocumentRenderOptions());
            warm.append(double(t.nsecsElapsed()) / 1e6);
            requireImage(img, "QPdfDocument::render warm");
        }
        QJsonObject o;
        o.insert(QStringLiteral("cold2xRender"), summarize(cold));
        o.insert(QStringLiteral("warm2xRender"), summarize(warm));
        report.insert(QStringLiteral("singlePageViewAllocator"), o);
    }

    // ── (2) Real viewer: PdfViewerWidget::renderPage cold vs cached ────────
    {
        PdfViewerWidget viewer;
        viewer.loadDocument(letterPdf);
        viewer.resize(1200, 900);
        QVector<double> cold, warm;
        for (int i = 0; i < kProfilePages; ++i) {
            QElapsedTimer t;
            t.start();
            QImage img = viewer.renderPage(i, kTwoPageZoom * 2.0);
            cold.append(double(t.nsecsElapsed()) / 1e6);
            requireImage(img, "viewer renderPage cold");
            t.restart();
            img = viewer.renderPage(i, kTwoPageZoom * 2.0);
            warm.append(double(t.nsecsElapsed()) / 1e6);
            requireImage(img, "viewer renderPage cached");
        }
        // Spread navigation: the two-page path renders the pair at zoom*2 on
        // every page change — cold cache per spread, then a warm repeat.
        QVector<double> spreadCold, spreadWarm;
        for (int i = 0; i < kProfilePages; i += 2) {
            viewer.clearPageCache();
            QElapsedTimer t;
            t.start();
            QImage l = viewer.renderPage(i, kTwoPageZoom * 2.0);
            QImage r = (i + 1 < kProfilePages)
                           ? viewer.renderPage(i + 1, kTwoPageZoom * 2.0)
                           : QImage();
            spreadCold.append(double(t.nsecsElapsed()) / 1e6);
            requireImage(l, "spread left cold");
            t.restart();
            l = viewer.renderPage(i, kTwoPageZoom * 2.0);
            r = (i + 1 < kProfilePages) ? viewer.renderPage(i + 1, kTwoPageZoom * 2.0) : QImage();
            spreadWarm.append(double(t.nsecsElapsed()) / 1e6);
        }
        QJsonObject o;
        o.insert(QStringLiteral("pageColdAt2x"), summarize(cold));
        o.insert(QStringLiteral("pageCachedAt2x"), summarize(warm));
        o.insert(QStringLiteral("spreadCold"), summarize(spreadCold));
        o.insert(QStringLiteral("spreadWarm"), summarize(spreadWarm));
        o.insert(QStringLiteral("peakWorkingSetMiB"), double(peakWorkingSetBytes()) / 1048576.0);
        report.insert(QStringLiteral("twoPageSpread"), o);
    }

    // ── (3) Thumbnail seam: RenderCache over the viewer renderer ───────────
    {
        PdfViewerWidget viewer;
        viewer.loadDocument(letterPdf);
        viewer.resize(1200, 900);
        RenderCache cache;
        ViewerRenderer renderer(&viewer);
        QVector<double> cold, warm;
        for (int i = 0; i < kProfilePages; ++i) {
            QElapsedTimer t;
            t.start();
            QImage img = cache.getOrRender(i, kThumbDpi / 72.0, &renderer);
            cold.append(double(t.nsecsElapsed()) / 1e6);
            requireImage(img, "thumbnail cold");
        }
        for (int i = 0; i < kProfilePages; ++i) {
            QElapsedTimer t;
            t.start();
            QImage img = cache.getOrRender(i, kThumbDpi / 72.0, &renderer);
            warm.append(double(t.nsecsElapsed()) / 1e6);
            requireImage(img, "thumbnail warm");
        }
        QJsonObject o;
        o.insert(QStringLiteral("thumb75dpiCold"), summarize(cold));
        o.insert(QStringLiteral("thumb75dpiWarm"), summarize(warm));
        o.insert(QStringLiteral("cacheHits"), cache.cacheHits());
        o.insert(QStringLiteral("cacheMisses"), cache.cacheMisses());
        report.insert(QStringLiteral("thumbnailPath"), o);
    }

    // ── (4) Guard extremes (recorded behaviour, machine-honest) ────────────
    {
        QJsonObject o;
        PdfViewerWidget viewer;
        viewer.loadDocument(letterPdf);
        viewer.resize(1200, 900);

        // (a) extreme zoom on a normal Letter page, through the public seam
        QElapsedTimer t;
        t.start();
        QImage img = viewer.renderPage(0, 1e9);
        o.insert(QStringLiteral("extremeZoom1e9_ms"), double(t.nsecsElapsed()) / 1e6);
        o.insert(QStringLiteral("extremeZoom1e9_null"), img.isNull());
        o.insert(QStringLiteral("extremeZoom1e9_w"), img.width());
        o.insert(QStringLiteral("extremeZoom1e9_h"), img.height());

        // (b) NaN and infinite scales
        img = viewer.renderPage(0, std::numeric_limits<double>::quiet_NaN());
        o.insert(QStringLiteral("nanScale_null"), img.isNull());
        img = viewer.renderPage(0, std::numeric_limits<double>::infinity());
        o.insert(QStringLiteral("infScale_null"), img.isNull());

        // (c) large MediaBox document (40000x40000 pt) at snapshot scale 2 —
        // a bounded render or a useful refusal, never a gigapixel alloc.
        QPdfDocument hugeDoc;
        hugeDoc.load(hugePdf);
        const QSizeF ps = hugeDoc.pagePointSize(0);
        o.insert(QStringLiteral("hugeMediaBox_ptW"), ps.width());
        o.insert(QStringLiteral("hugeMediaBox_ptH"), ps.height());
        PdfViewerWidget hugeViewer;
        hugeViewer.loadDocument(hugePdf);
        hugeViewer.resize(1200, 900);
        t.restart();
        img = hugeViewer.renderPage(0, 2.0);
        o.insert(QStringLiteral("hugeMediaBox_ms"), double(t.nsecsElapsed()) / 1e6);
        o.insert(QStringLiteral("hugeMediaBox_null"), img.isNull());
        o.insert(QStringLiteral("hugeMediaBox_w"), img.width());
        o.insert(QStringLiteral("hugeMediaBox_h"), img.height());
        o.insert(QStringLiteral("hugeMediaBox_mpx"),
                 double(qint64(img.width()) * qint64(img.height())) / 1e6);

        // (d) rapid zoom reversal: alternate extremes, two-page-scale renders
        QVector<double> reversal;
        for (int i = 0; i < 20; ++i) {
            const double z = (i % 2 == 0) ? 0.1 : 32.0;
            viewer.clearPageCache();
            t.restart();
            QImage a = viewer.renderPage(0, z * 2.0);
            QImage b = viewer.renderPage(1, z * 2.0);
            reversal.append(double(t.nsecsElapsed()) / 1e6);
        }
        o.insert(QStringLiteral("rapidZoomReversal20x"), summarize(reversal));
        o.insert(QStringLiteral("peakWorkingSetMiB"), double(peakWorkingSetBytes()) / 1048576.0);
        report.insert(QStringLiteral("guardExtremes"), o);
    }

    QFile out(QString::fromLocal8Bit(argv[1]));
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 4;
    out.write(QJsonDocument(report).toJson(QJsonDocument::Indented));

    std::printf("%s\n", QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
    return 0;
}
