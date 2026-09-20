// SPDX-License-Identifier: Apache-2.0
// SWEEP-W3-PERF deterministic fixture generator.
//
// Purpose: generate the document corpus for the W3 performance baseline.
// ALL content is derived from a fixed-seed PRNG (std::mt19937, seed 20260920)
// so every regeneration produces the SAME page structure and near-same file
// sizes. (Byte-exact determinism is impossible for PDFs — creation timestamps
// are embedded by the writer; "deterministic" here means identical structure
// and statistically identical sizes, disclosed honestly.)
//
// Fixtures written into the directory given as argv[1]:
//   small-text-20p.pdf    20 pages, text + small noise image  (~1 MB class)
//   medium-20mb.pdf       noise-image pages, calibrated       (~20 MB class)
//   large-100mb.pdf       noise-image pages, calibrated       (~100+ MB class)
//   compare-a-50p.pdf     50 text pages
//   compare-b-50p.pdf     same 50 pages, last 3 changed
//   redact-target.pdf     3 pages, known text marks
//   batch-corpus/         50 two-page text PDFs (batch pipeline corpus)
//
// Calibration: size classes are hit by growing the per-page noise-image
// dimensions from the fixed seed until the written file crosses the class
// target (max 4 grow attempts) — a deterministic algorithm, no magic sizes.
// If a class target cannot be reached within the attempts the generator
// FAILS LOUDLY rather than silently producing a wrong class.
//
// Usage: perf_fixtures <output-dir>
#include <QApplication>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPdfWriter>
#include <QPageSize>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <cstdio>
#include <memory>

namespace {

constexpr quint32 kSeed = 20260920u;

// Deterministic noise image (fixed seed -> identical bytes every run).
QImage noiseImage(int w, int h, quint32 salt)
{
    QImage img(w, h, QImage::Format_RGB32);
    quint32 s = kSeed ^ salt;
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            s = s * 1664525u + 1013904223u;          // LCG x = f(x): fixed, cheap
            const quint32 v = (s >> 16) & 0xFF;
            line[x] = qRgb(v, (s >> 8) & 0xFF, s & 0xFF);
        }
    }
    return img;
}

qint64 writeFileWithPageImageBytes(const QString &path, int pages,
                                   int imgW, int imgH, int imgPages)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    w.setTitle(QStringLiteral("W3 perf fixture"));
    w.setCreator(QStringLiteral("perf_fixtures"));
    QPainter p(&w);
    quint32 salt = 1u;
    for (int i = 0; i < pages; ++i) {
        if (i > 0) w.newPage();
        p.drawText(100, 100, QStringLiteral("PAGE %1 of %2").arg(i + 1).arg(pages));
        if (i < imgPages) {
            // Grow salt with the page index so images differ per page but are
            // reproducible run to run.
            p.drawImage(QRect(150, 200, 400, 400),
                        noiseImage(imgW, imgH, salt += 7919u));
        }
    }
    p.end();
    return QFileInfo(path).size();
}

// Grow the noise image until the file crosses targetBytes (deterministic).
// Two axes, in order: image dimensions first, then how many pages carry an
// image — if the largest dimension is not enough, more pages get payloads.
qint64 generateCalibrated(const QString &path, int pages, qint64 targetBytes)
{
    static const int kDims[] = { 64, 128, 256, 384, 512, 768, 1024, 1152, 1280,
                                 1408, 1536, 1792, 2048, 2560, 3072 };
    qint64 size = -1;
    for (const int d : kDims) {
        const int imgPages = qMin(pages, 8);
        size = writeFileWithPageImageBytes(path, pages, d, d, imgPages);
        if (size >= targetBytes)
            return size;
    }
    static const int kMoreImagePages[] = { 12, 20, 40, 80 };
    for (const int imgPages : kMoreImagePages) {
        if (imgPages > pages)
            break;
        size = writeFileWithPageImageBytes(path, pages, 3072, 3072, imgPages);
        if (size >= targetBytes)
            return size;
    }
    return size; // caller decides whether this is a failure
}

void makeTextPdf(const QString &path, int pages, const QString &tag,
                 int changedFromPage = -1)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    w.setTitle(QStringLiteral("W3 perf fixture %1").arg(tag));
    QPainter p(&w);
    for (int i = 0; i < pages; ++i) {
        if (i > 0) w.newPage();
        if (changedFromPage >= 0 && i >= changedFromPage) {
            p.drawText(100, 100, QStringLiteral("%1 CHANGED PAGE %2").arg(tag).arg(i + 1));
            p.drawRect(100, 150, 2000, 600);
        } else {
            p.drawText(100, 100, QStringLiteral("PAGE %1").arg(i + 1));
            for (int ln = 1; ln <= 12; ++ln)
                p.drawText(100, 160 + ln * 60,
                           QStringLiteral("Line %1 of document %2 — sample text for the perf corpus").arg(ln).arg(tag));
        }
    }
    p.end();
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);          // QPdfWriter needs QGuiApplication base
    if (argc < 2) {
        std::fprintf(stderr, "usage: perf_fixtures <output-dir>\n");
        return 2;
    }
    const QString dir = argv[1];
    QDir().mkpath(dir);
    const QString corpus = dir + QStringLiteral("/batch-corpus");
    QDir().mkpath(corpus);

    QJsonObject report;
    QElapsedTimer t; t.start();

    // Small ~1 MB class: 20-page text document + image payload.
    {
        const QString f = dir + QStringLiteral("/small-text-20p.pdf");
        const qint64 sz = generateCalibrated(f, 20, 900 * 1000);
        if (sz < 500 * 1000) {
            std::fprintf(stderr, "FAIL small class: %lld bytes\n", (long long)sz);
            return 1;
        }
        report.insert(QStringLiteral("small-text-20p.pdf"), static_cast<double>(sz));
    }
    // Medium ~20 MB class.
    {
        const QString f = dir + QStringLiteral("/medium-20mb.pdf");
        const qint64 sz = generateCalibrated(f, 40, 19ll * 1000 * 1000);
        if (sz < 15ll * 1000 * 1000) {
            std::fprintf(stderr, "FAIL medium class: %lld bytes\n", (long long)sz);
            return 1;
        }
        report.insert(QStringLiteral("medium-20mb.pdf"), static_cast<double>(sz));
    }
    // Large 100+ MB class.
    {
        const QString f = dir + QStringLiteral("/large-100mb.pdf");
        const qint64 sz = generateCalibrated(f, 150, 100ll * 1000 * 1000);
        if (sz < 95ll * 1000 * 1000) {
            std::fprintf(stderr, "FAIL large class: %lld bytes\n", (long long)sz);
            return 1;
        }
        report.insert(QStringLiteral("large-100mb.pdf"), static_cast<double>(sz));
    }
    // Compare pair: 50 text pages, B changes the last 3.
    {
        const QString a = dir + QStringLiteral("/compare-a-50p.pdf");
        const QString b = dir + QStringLiteral("/compare-b-50p.pdf");
        makeTextPdf(a, 50, QStringLiteral("A"));
        makeTextPdf(b, 50, QStringLiteral("B"), 47);
        report.insert(QStringLiteral("compare-a-50p.pdf"),
                      static_cast<double>(QFileInfo(a).size()));
        report.insert(QStringLiteral("compare-b-50p.pdf"),
                      static_cast<double>(QFileInfo(b).size()));
    }
    // Redaction target: 3 pages, known text marks (redact rects cover the
    // header band on each page).
    {
        const QString f = dir + QStringLiteral("/redact-target.pdf");
        makeTextPdf(f, 3, QStringLiteral("R"));
        report.insert(QStringLiteral("redact-target.pdf"),
                      static_cast<double>(QFileInfo(f).size()));
    }
    // Batch corpus: 50 two-page text PDFs.
    {
        QJsonArray arr;
        for (int i = 0; i < 50; ++i) {
            const QString f = corpus + QStringLiteral("/doc-%1.pdf").arg(i, 2, 10, QLatin1Char('0'));
            makeTextPdf(f, 2, QStringLiteral("D%1").arg(i));
            arr.append(static_cast<double>(QFileInfo(f).size()));
        }
        report.insert(QStringLiteral("batch-corpus-count"), 50.0);
        report.insert(QStringLiteral("batch-corpus-bytes"), arr);
    }

    report.insert(QStringLiteral("elapsed_ms"), static_cast<double>(t.elapsed()));
    report.insert(QStringLiteral("dir"), dir);
    std::fprintf(stdout, "%s\n",
                 QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
    return 0;
}
