// SPDX-License-Identifier: Apache-2.0
// WP-R08 (P1) regression suite — print range job ownership and error outcomes.
//
// WHOLE-PRODUCT-AND-PLAN-REVIEW-2026-09-10 PP03: the print path ignored the
// dialog's selected page range (always pages 0..N-1) and every page was
// rendered from the LIVE view-owned QPdfDocument by raw QThread workers, so a
// document switch/close during printing was a lifetime risk and painter/spool
// failures were silently ignored.
//
// The repair boundary is gp::PrintJob (src/ui/PrintJob.{h,cpp}): the job owns
// an immutable snapshot (its OWN QPdfDocument opened on the selected path),
// prints an explicit selected-page sequence, is cancellable at every page
// boundary, reports render/painter/spool failures, and emits exactly one
// terminal finished().
//
// These tests drive the REAL paint path through a QPrinter in PdfFormat
// (print-to-file): the printed output PDF is reopened and each output page is
// raster-compared against the source pages, proving which source pages were
// printed and in which order. The selection-mapping function is pinned by
// unit assertions. A physical printer / native print dialog is NOT available
// headlessly (stated residual): dialog-to-sequence plumbing is exercised at
// the pageSequence boundary, output correctness through the actual paint path.
#include <QtTest/QtTest>
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QPainter>
#include <QPrinter>
#include <QTimer>
#include <QWidget>
#include <QtPdf/QPdfDocument>
#include <QtPdf/QPdfDocumentRenderOptions>
#include <QtGui/qpdfwriter.h>

#include "ui/PrintJob.h"

using gp::PrintJob;

// ── fixtures ─────────────────────────────────────────────────────────────────

// A page whose raster is dominated by a huge digit over a distinct background
// tint, so different pages differ strongly and the same page differs weakly.
static QString makeFixturePdf(const QString& path, int pages)
{
    QPdfWriter writer(path);
    writer.setResolution(150);
    QPainter p(&writer);
    const QColor tints[3] = { QColor(255, 255, 255), QColor(235, 240, 255), QColor(255, 240, 225) };
    for (int i = 0; i < pages; ++i) {
        if (i > 0)
            writer.newPage();
        p.fillRect(p.viewport(), tints[i % 3]);
        QFont f = p.font();
        f.setPointSize(400);
        f.setBold(true);
        p.setFont(f);
        p.setPen(Qt::black);
        p.drawText(p.viewport(), Qt::AlignCenter, QString::number(i + 1));
    }
    p.end();
    return path;
}

static QImage renderPdfPage(const QString& path, int page, const QSize& size)
{
    QPdfDocument doc;
    doc.load(path);
    if (doc.error() != QPdfDocument::Error::None)
        return {};
    return doc.render(page, size, QPdfDocumentRenderOptions());
}

static double meanAbsDiff(const QImage& aIn, const QImage& bIn)
{
    const QSize canonical(64, 90);
    const QImage a = aIn.convertToFormat(QImage::Format_Grayscale8).scaled(canonical);
    const QImage b = bIn.convertToFormat(QImage::Format_Grayscale8).scaled(canonical);
    if (a.size() != b.size() || a.isNull())
        return 255.0;
    qint64 acc = 0;
    for (int y = 0; y < canonical.height(); ++y) {
        const uchar* la = a.constScanLine(y);
        const uchar* lb = b.constScanLine(y);
        for (int x = 0; x < canonical.width(); ++x)
            acc += qAbs(int(la[x]) - int(lb[x]));
    }
    return double(acc) / (canonical.width() * canonical.height());
}

// Scale/translation-invariant comparison: crop to the ink bounding box (the
// printed page carries printer margins, the source page is full-bleed) and
// rescale. Returns the normalized image, or the original downscaled when
// there is no ink.
static QImage normalizedInk(const QImage& grayIn)
{
    const QImage gray = grayIn.convertToFormat(QImage::Format_Grayscale8)
                            .scaled(QSize(255, 360));
    int minX = gray.width(), minY = gray.height(), maxX = -1, maxY = -1;
    for (int y = 0; y < gray.height(); ++y) {
        const uchar* line = gray.constScanLine(y);
        for (int x = 0; x < gray.width(); ++x) {
            if (line[x] < 200) {   // ink (text) on a light page
                minX = qMin(minX, x); maxX = qMax(maxX, x);
                minY = qMin(minY, y); maxY = qMax(maxY, y);
            }
        }
    }
    if (maxX < 0)
        return gray.scaled(QSize(64, 90));
    return gray.copy(QRect(minX, minY, maxX - minX + 1, maxY - minY + 1))
               .scaled(QSize(64, 90));
}

// Diff of the ink-normalized pages.
static double inkDiff(const QImage& a, const QImage& b)
{
    return meanAbsDiff(normalizedInk(a), normalizedInk(b));
}

class TestPrintJob : public QObject {
    Q_OBJECT

    QTemporaryDir m_work;

    // Runs a job to its terminal outcome with a hard watchdog (never hangs).
    struct RunResult { int printed = -1; bool canceled = false; QString error; int finishedCount = 0; };

    RunResult runJob(PrintJob* job)
    {
        RunResult r;
        QEventLoop loop;
        QTimer watchdog;
        watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
        connect(job, &PrintJob::finished, this, [&](int printed, bool canceled, const QString& error) {
            r.printed = printed;
            r.canceled = canceled;
            r.error = error;
            ++r.finishedCount;
            loop.quit();
        });
        watchdog.start(90000);
        loop.exec();
        return r;
    }

private slots:
    void initTestCase() {
        QVERIFY(m_work.isValid());
    }

    // ── selection mapping (the PP03 core: the dialog's choices must reach
    // the printed sequence) ────────────────────────────────────────────────
    void pageSequenceHonorsSelection() {
        using R = QPrinter::PrintRange;
        using O = QPrinter::PageOrder;
        // All pages, first-to-last.
        QCOMPARE(PrintJob::pageSequence(R::AllPages, 0, 0, 5, O::FirstPageFirst),
                 (QVector<int>{0, 1, 2, 3, 4}));
        // Explicit range is honored (1-based inclusive → 0-based sequence).
        QCOMPARE(PrintJob::pageSequence(R::PageRange, 2, 4, 5, O::FirstPageFirst),
                 (QVector<int>{1, 2, 3}));
        // Single-page range.
        QCOMPARE(PrintJob::pageSequence(R::PageRange, 3, 3, 5, O::FirstPageFirst),
                 (QVector<int>{2}));
        // Range is clamped to the document; beyond-document parts dropped.
        QCOMPARE(PrintJob::pageSequence(R::PageRange, 4, 99, 5, O::FirstPageFirst),
                 (QVector<int>{3, 4}));
        // Inverted range prints NOTHING (never silently widened to all pages).
        QVERIFY(PrintJob::pageSequence(R::PageRange, 4, 2, 5, O::FirstPageFirst).isEmpty());
        // Current page.
        QCOMPARE(PrintJob::pageSequence(R::CurrentPage, 0, 0, 5, O::FirstPageFirst, 3),
                 (QVector<int>{2}));
        // Unknown current page → empty (reported, not "all pages").
        QVERIFY(PrintJob::pageSequence(R::CurrentPage, 0, 0, 5, O::FirstPageFirst, 0).isEmpty());
        // Reverse order is honored.
        QCOMPARE(PrintJob::pageSequence(R::AllPages, 0, 0, 4, O::LastPageFirst),
                 (QVector<int>{3, 2, 1, 0}));
        QCOMPARE(PrintJob::pageSequence(R::PageRange, 1, 3, 5, O::LastPageFirst),
                 (QVector<int>{2, 1, 0}));
        // Unset range fields (0/0 with PageRange) degenerate to the full document.
        QCOMPARE(PrintJob::pageSequence(R::PageRange, 0, 0, 3, O::FirstPageFirst),
                 (QVector<int>{0, 1, 2}));
    }

    // ── end-to-end through the ACTUAL paint path (print-to-PDF) ────────────
    void printedPdfContainsExactlyTheSelectedPages() {
        const QString src = makeFixturePdf(m_work.filePath(QStringLiteral("src3.pdf")), 3);
        const QString out = m_work.filePath(QStringLiteral("out-range.pdf"));
        QFile::remove(out);

        QPrinter* printer = new QPrinter(QPrinter::HighResolution);
        printer->setOutputFormat(QPrinter::PdfFormat);
        printer->setOutputFileName(out);

        auto* job = PrintJob::start({src}, printer, {1}, nullptr);  // ONLY page 2
        const RunResult r = runJob(job);

        QVERIFY(r.error.isEmpty());               // failures are never silent — and none here
        QCOMPARE(r.finishedCount, 1);             // exactly one terminal outcome
        QVERIFY(!r.canceled);
        QCOMPARE(r.printed, 1);
        QVERIFY(QFileInfo::exists(out));

        // The output PDF has exactly ONE page and it is source page 2.
        // Comparison is ink-normalized: the printed page carries printer
        // margins the source page does not have, so absolute placement is not
        // comparable — the printed page must match its source page closely
        // and every OTHER source page clearly worse (argmin + separation).
        const QSize canonical(255, 360);
        QPdfDocument outDoc;
        outDoc.load(out);
        QCOMPARE(outDoc.error(), QPdfDocument::Error::None);
        QCOMPARE(outDoc.pageCount(), 1);
        const QImage printedPage = outDoc.render(0, canonical, QPdfDocumentRenderOptions());
        const double samePage = inkDiff(printedPage, renderPdfPage(src, 1, canonical));
        const double otherPage = inkDiff(printedPage, renderPdfPage(src, 0, canonical));
        QVERIFY2(samePage < 35.0 && otherPage > samePage * 1.5 + 2.0,
                 qPrintable(QStringLiteral("printed page must be source page 2: same %1, other %2")
                                .arg(samePage).arg(otherPage)));
    }

    void printedPdfHonorsRangeAndOrder() {
        const QString src = makeFixturePdf(m_work.filePath(QStringLiteral("src4.pdf")), 4);
        const QString out = m_work.filePath(QStringLiteral("out-order.pdf"));
        QFile::remove(out);

        QPrinter* printer = new QPrinter(QPrinter::HighResolution);
        printer->setOutputFormat(QPrinter::PdfFormat);
        printer->setOutputFileName(out);

        // Pages 3 then 1 (0-based {2,0}) — a subset in an explicit order.
        auto* job = PrintJob::start({src}, printer, {2, 0}, nullptr);
        const RunResult r = runJob(job);

        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.printed, 2);
        QVERIFY(QFileInfo::exists(out));

        const QSize canonical(255, 360);
        QPdfDocument outDoc;
        outDoc.load(out);
        QCOMPARE(outDoc.pageCount(), 2);
        const QImage out0 = outDoc.render(0, canonical, QPdfDocumentRenderOptions());
        const QImage out1 = outDoc.render(1, canonical, QPdfDocumentRenderOptions());
        // Order: output page 1 is source page 3, output page 2 is source
        // page 1 — each printed page's BEST match must be its intended source
        // page, with clear separation from the others (all values reported).
        const double o0s1 = inkDiff(out0, renderPdfPage(src, 0, canonical));
        const double o0s2 = inkDiff(out0, renderPdfPage(src, 1, canonical));
        const double o0s3 = inkDiff(out0, renderPdfPage(src, 2, canonical));
        const double o1s1 = inkDiff(out1, renderPdfPage(src, 0, canonical));
        const double o1s2 = inkDiff(out1, renderPdfPage(src, 1, canonical));
        const double o1s3 = inkDiff(out1, renderPdfPage(src, 2, canonical));
        QVERIFY2(o0s3 < 35.0 && o0s3 < o0s1 * 0.7 && o0s3 < o0s2 * 0.7 &&
                 o1s1 < 35.0 && o1s1 < o1s2 * 0.7 && o1s1 < o1s3 * 0.7,
                 qPrintable(QStringLiteral("order must be src3 then src1: "
                                          "out0{1:%1, 2:%2, 3:%3} out1{1:%4, 2:%5, 3:%6}")
                                .arg(o0s1).arg(o0s2).arg(o0s3).arg(o1s1).arg(o1s2).arg(o1s3)));
    }

    // ── job ownership: the snapshot outlives the dialog parent / view ──────
    void jobSurvivesDialogParentDestructionMidPrint() {
        const QString src = makeFixturePdf(m_work.filePath(QStringLiteral("src2.pdf")), 2);
        const QString out = m_work.filePath(QStringLiteral("out-orphan.pdf"));
        QFile::remove(out);

        QPrinter* printer = new QPrinter(QPrinter::HighResolution);
        printer->setOutputFormat(QPrinter::PdfFormat);
        printer->setOutputFileName(out);

        // A stand-in for the viewer: destroyed mid-print (immediately after
        // start, before the job even begins). The OLD design dereferenced the
        // view's document from workers — the job owns its snapshot instead.
        auto* dialogParent = new QWidget;
        dialogParent->show();
        auto* job = PrintJob::start({src}, printer, {0, 1}, dialogParent);
        delete dialogParent;   // close/switch during printing

        const RunResult r = runJob(job);
        QVERIFY(r.error.isEmpty());
        QCOMPARE(r.printed, 2);
        QCOMPARE(r.finishedCount, 1);
        QVERIFY(QFileInfo::exists(out));
        QPdfDocument outDoc;
        outDoc.load(out);
        QCOMPARE(outDoc.pageCount(), 2);          // both pages made it out
    }

    // ── cancellation ────────────────────────────────────────────────────────
    void cancelBeforeStartProducesNoOutputAndReportsTruthfully() {
        const QString src = makeFixturePdf(m_work.filePath(QStringLiteral("src1.pdf")), 1);
        const QString out = m_work.filePath(QStringLiteral("out-cancel.pdf"));
        QFile::remove(out);

        QPrinter* printer = new QPrinter(QPrinter::HighResolution);
        printer->setOutputFormat(QPrinter::PdfFormat);
        printer->setOutputFileName(out);

        auto* job = PrintJob::start({src}, printer, {0}, nullptr);
        job->cancel();   // deterministic: before the queued begin() runs
        const RunResult r = runJob(job);

        QVERIFY(r.canceled);
        QCOMPARE(r.printed, 0);
        QVERIFY(r.error.isEmpty());               // a cancel is not a failure
        QCOMPARE(r.finishedCount, 1);
        QVERIFY(!QFileInfo::exists(out));          // NO partial output
    }

    // ── error outcomes ──────────────────────────────────────────────────────
    void unopenablePrinterSurfacesAsVisibleError() {
        const QString src = makeFixturePdf(m_work.filePath(QStringLiteral("srcf.pdf")), 1);
        // An output path that cannot exist — the print target cannot open.
        const QString out = m_work.filePath(QStringLiteral("no-such-dir")) +
                            QStringLiteral("/out.pdf");
        QFile::remove(out);

        QPrinter* printer = new QPrinter(QPrinter::HighResolution);
        printer->setOutputFormat(QPrinter::PdfFormat);
        printer->setOutputFileName(out);

        auto* job = PrintJob::start({src}, printer, {0}, nullptr);
        const RunResult r = runJob(job);

        QVERIFY(!r.error.isEmpty());               // VISIBLE failure, not silence
        QCOMPARE(r.printed, 0);
        QCOMPARE(r.finishedCount, 1);
        QVERIFY(!QFileInfo::exists(out));          // and no partial artifact
    }

    void unopenableDocumentSurfacesAsVisibleError() {
        const QString missing = m_work.filePath(QStringLiteral("missing.pdf"));
        QFile::remove(missing);
        const QString out = m_work.filePath(QStringLiteral("out-miss.pdf"));
        QFile::remove(out);

        QPrinter* printer = new QPrinter(QPrinter::HighResolution);
        printer->setOutputFormat(QPrinter::PdfFormat);
        printer->setOutputFileName(out);

        auto* job = PrintJob::start({missing}, printer, {0}, nullptr);
        const RunResult r = runJob(job);

        QVERIFY(!r.error.isEmpty());
        QCOMPARE(r.printed, 0);
        QCOMPARE(r.finishedCount, 1);
        QVERIFY(!QFileInfo::exists(out));
    }
};

QTEST_MAIN(TestPrintJob)
#include "TestPrintJob.moc"
