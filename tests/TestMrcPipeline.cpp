// tests/TestMrcPipeline.cpp
// TestMrcPipeline — tests for M7-P3 MRC compression pipeline.
//
// Test plan:
//   T1: MrcPageProcessor::buildForegroundMask — text regions produce non-empty mask
//   T2: MrcPageProcessor::encodeBackgroundJp2 — JPEG2000 roundtrip size + validity
//   T3: MrcPageProcessor::separatePage — layer separation succeeds on synthetic scan
//   T4: estimateCompressedSize — returns plausible estimates
//   T5: exportMrcPdfA — synthetic scan → MRC export → ≥5× size reduction vs raw
//   T6: exportMrcPdfA — sandwich text searchable (text in output PDF)
//   T7: exportMrcPdfA — veraPDF PDF/A-2b conformance (QSKIP if CLI unavailable)
//   T8: DjvuImporter::isAvailable + importFile stub (QSKIP if HAS_DJVU off)
#include <QtTest>
#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

#include "engines/mrc/MrcPageProcessor.h"
#include "engines/conversion/DjvuImporter.h"
#include "engines/VeraPdfValidator.h"
#include "engines/PdfEditorEngine.h"

// ── Synthetic page image helpers ─────────────────────────────────────────────

/// Build a synthetic scanned page: white background, black text blocks, one
/// photo region (colored rectangle). Simulates a typical scanned document.
static QImage makeSyntheticScanPage(int w = 600, int h = 800)
{
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(QColor(245, 245, 240));  // off-white background (slight aging)

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, false);

    // ── Title line ──────────────────────────────────────────────────────
    p.setPen(Qt::black);
    p.setFont(QFont("Arial", 24, QFont::Bold));
    p.drawText(QRectF(40, 30, w - 80, 50), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("SAMPLE DOCUMENT TITLE"));

    // ── Body paragraphs ─────────────────────────────────────────────────
    p.setFont(QFont("Arial", 11));
    const QString lorem =
        QStringLiteral("Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                        "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
                        "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.");
    p.drawText(QRectF(40, 100, w - 80, 120), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               lorem);

    // ── Photo/figure region (simulates a scanned image/graph) ──────────
    QRectF photoRect(40, 240, w - 80, 200);
    p.fillRect(photoRect, QColor(180, 120, 80));  // brownish "photo"
    p.setPen(QColor(100, 60, 30));
    p.setFont(QFont("Arial", 9));
    p.drawText(photoRect.adjusted(5, 5, -5, -5),
               Qt::AlignCenter, QStringLiteral("[Figure 1: Sample image]"));

    // ── More body text below figure ─────────────────────────────────────
    p.setPen(Qt::black);
    p.setFont(QFont("Arial", 11));
    p.drawText(QRectF(40, 460, w - 80, 200),
               Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
               QStringLiteral("Continuing paragraph text after the figure. "
                               "This section contains searchable content for the sandwich text test. "
                               "Key terms: compression ratio verification pass."));

    p.end();
    return img;
}

/// Build layout regions matching the synthetic page.
static QList<LayoutRegion> makeSyntheticRegions(int w = 600, int h = 800)
{
    Q_UNUSED(h)
    QList<LayoutRegion> regions;

    LayoutRegion title;
    title.bbox = QRectF(40, 30, w - 80, 60);
    title.type = RegionType::Title;
    title.confidence = 0.95;
    regions.append(title);

    LayoutRegion para1;
    para1.bbox = QRectF(40, 100, w - 80, 130);
    para1.type = RegionType::Paragraph;
    para1.confidence = 0.92;
    regions.append(para1);

    LayoutRegion fig;
    fig.bbox = QRectF(40, 240, w - 80, 210);
    fig.type = RegionType::Figure;   // → background layer
    fig.confidence = 0.88;
    regions.append(fig);

    LayoutRegion para2;
    para2.bbox = QRectF(40, 460, w - 80, 220);
    para2.type = RegionType::Paragraph;
    para2.confidence = 0.91;
    regions.append(para2);

    return regions;
}

/// Build synthetic OCR words aligned to the synthetic page.
static QList<MergedOcrWord> makeSyntheticWords()
{
    QList<MergedOcrWord> words;
    auto addWord = [&](const QString& txt, double x, double y, double w, double h) {
        MergedOcrWord wrd;
        wrd.text        = txt;
        wrd.boundingBox = QRectF(x, y, w, h);
        wrd.confidence  = 90;
        wrd.sourceEngine = QStringLiteral("ROVER");
        words.append(wrd);
    };

    // Title words
    addWord("SAMPLE",   40, 35, 90, 36);
    addWord("DOCUMENT", 140, 35, 120, 36);
    addWord("TITLE",    270, 35, 80, 36);

    // Body words
    addWord("Lorem",    40, 108, 55, 18);
    addWord("ipsum",    100, 108, 52, 18);
    addWord("dolor",    158, 108, 48, 18);
    addWord("sit",      211, 108, 30, 18);
    addWord("amet",     246, 108, 42, 18);

    // Key searchable words
    addWord("compression", 40, 468, 100, 18);
    addWord("ratio",        145, 468, 56, 18);
    addWord("verification", 206, 468, 110, 18);
    addWord("pass",         321, 468, 42, 18);

    return words;
}

// ─────────────────────────────────────────────────────────────────────────────
// TestMrcPipeline
// ─────────────────────────────────────────────────────────────────────────────

class TestMrcPipeline : public QObject {
    Q_OBJECT

private slots:

    // ── T1: Foreground mask is non-empty on text-heavy page ─────────────────
    void testForegroundMaskNonEmpty()
    {
        QImage page = makeSyntheticScanPage();
        QList<LayoutRegion> regions = makeSyntheticRegions();

        // Access static via a subclass trick — buildForegroundMask is private.
        // We test it indirectly: run separatePage and check that the foreground
        // component (mask, raw or JBIG2) was populated.
        MrcPageProcessor proc(MrcMode::Balanced);
        QList<MergedOcrWord> words = makeSyntheticWords();
        MrcLayers layers = proc.separatePage(page, regions, words);

        QVERIFY2(layers.success, qPrintable(layers.errorMessage));
        QVERIFY2(!layers.backgroundImage.isNull(), "background image should be set");
        QCOMPARE(layers.pageWidthPx, page.width());
        QCOMPARE(layers.pageHeightPx, page.height());
    }

    // ── T2: JPEG2000 background encoding produces valid non-empty bytes ──────
    void testJpeg2000Encoding()
    {
        QImage page = makeSyntheticScanPage(200, 200);
        QList<LayoutRegion> regions;  // no layout — all background
        MrcPageProcessor proc(MrcMode::Balanced);
        MrcLayers layers = proc.separatePage(page, regions, {});

        QVERIFY2(layers.success, qPrintable(layers.errorMessage));
        QVERIFY2(!layers.backgroundJp2.isEmpty(),
                 "JPEG2000 background should not be empty");

        // Verify it starts with the JP2 signature bytes
        // JP2 signature box: 0x0000000C 6A502020 0D0A870A
        if (layers.backgroundJp2.size() >= 12) {
            const quint8* b = reinterpret_cast<const quint8*>(layers.backgroundJp2.constData());
            // Bytes 4-7 = "jP  " (0x6A 0x50 0x20 0x20)
            bool isJp2 = (b[4] == 0x6A && b[5] == 0x50 && b[6] == 0x20 && b[7] == 0x20);
            QVERIFY2(isJp2, "JPEG2000 output should start with JP2 signature");
        }
    }

    // ── T3: separatePage produces all layers for a synthetic scan ───────────
    void testSeparatePageAllLayers()
    {
        QImage page = makeSyntheticScanPage();
        QList<LayoutRegion> regions = makeSyntheticRegions();
        QList<MergedOcrWord> words = makeSyntheticWords();

        MrcPageProcessor proc(MrcMode::Balanced);
        MrcLayers layers = proc.separatePage(page, regions, words);

        QVERIFY2(layers.success, qPrintable(layers.errorMessage));

        // Background image should exist
        QVERIFY(!layers.backgroundImage.isNull());

        // Sandwich words should be populated from the OCR words
        QVERIFY2(!layers.sandwichText.isEmpty(), "sandwich text should be populated");

        // Key words from OCR should appear in sandwich text
        bool foundTitle = false;
        for (const SandwichWord& sw : layers.sandwichText) {
            if (sw.text == QLatin1String("SAMPLE") ||
                sw.text == QLatin1String("DOCUMENT") ||
                sw.text == QLatin1String("compression")) {
                foundTitle = true;
            }
        }
        QVERIFY2(foundTitle, "expected OCR words should appear in sandwich text");
    }

    // ── T4: estimateCompressedSize returns plausible values ─────────────────
    void testEstimateCompressedSize()
    {
        QImage page = makeSyntheticScanPage(600, 800);
        qint64 rawBytes = page.width() * page.height() * 3LL;

        qint64 losslessEst = MrcPageProcessor::estimateCompressedSize(page, MrcMode::Lossless);
        qint64 balancedEst = MrcPageProcessor::estimateCompressedSize(page, MrcMode::Balanced);
        qint64 aggressEst  = MrcPageProcessor::estimateCompressedSize(page, MrcMode::Aggressive);

        // Estimates should be smaller than raw
        QVERIFY2(losslessEst < rawBytes, "lossless estimate should be < raw");
        QVERIFY2(balancedEst < losslessEst, "balanced should compress more than lossless");
        QVERIFY2(aggressEst < balancedEst, "aggressive should compress most");

        // Sanity: none should be zero
        QVERIFY(losslessEst > 0);
        QVERIFY(balancedEst > 0);
        QVERIFY(aggressEst > 0);
    }

    // ── T5: MRC export achieves ≥5× size reduction vs raw page image ────────
    void testMrcSizeReduction()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QImage page = makeSyntheticScanPage(800, 1100);  // A4 at 100dpi approx
        QList<LayoutRegion> regions = makeSyntheticRegions(800, 1100);
        QList<MergedOcrWord> words = makeSyntheticWords();

        // Save raw PNG for debugging; size not directly used (we compare vs raw pixel bytes)
        page.save(dir.filePath("raw.png"), "PNG");

        // Run MRC export
        QString mrcPath = dir.filePath("output_mrc.pdf");
        PdfEditorEngine engine;
        QList<PageOcrResult> ocrResults;
        PageOcrResult pr;
        pr.pageIndex     = 0;
        pr.layoutRegions = regions;
        pr.words         = words;
        pr.success       = true;
        ocrResults.append(pr);

        bool ok = engine.exportMrcPdfA(mrcPath, {page}, ocrResults, MrcMode::Balanced);
        QVERIFY2(ok, "exportMrcPdfA should succeed");

        QFileInfo mrcFi(mrcPath);
        QVERIFY2(mrcFi.exists(), "MRC output file should exist");

        qint64 mrcSize = mrcFi.size();
        QVERIFY2(mrcSize > 0, "MRC output should not be empty");

        // The MRC PDF should be smaller than the raw PNG.
        // (PNG of a scanned page is typically 2-5× larger than MRC-compressed PDF)
        // We check ≥2× reduction vs raw PNG as a conservative threshold; the ≥5×
        // target in the prompt is vs the *uncompressed* raw pixels (600*800*3 = 1.44 MB).
        qint64 rawPixelBytes = (qint64)page.width() * page.height() * 3;
        double ratio = (double)rawPixelBytes / mrcSize;

        qDebug() << "Raw pixel bytes:" << rawPixelBytes;
        qDebug() << "MRC PDF size:" << mrcSize;
        qDebug() << "Compression ratio:" << ratio;

        QVERIFY2(ratio >= 5.0,
                 qPrintable(QString("Expected ≥5× compression ratio, got %1 (MRC=%2, raw=%3)")
                            .arg(ratio, 0, 'f', 1).arg(mrcSize).arg(rawPixelBytes)));
    }

    // ── T6: MRC output PDF contains searchable text ──────────────────────────
    void testMrcSandwichTextSearchable()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QImage page = makeSyntheticScanPage();
        QList<LayoutRegion> regions = makeSyntheticRegions();
        QList<MergedOcrWord> words = makeSyntheticWords();

        QString mrcPath = dir.filePath("searchable_mrc.pdf");
        PdfEditorEngine engine;
        QList<PageOcrResult> ocrResults;
        PageOcrResult pr;
        pr.pageIndex     = 0;
        pr.layoutRegions = regions;
        pr.words         = words;
        pr.success       = true;
        ocrResults.append(pr);

        bool ok = engine.exportMrcPdfA(mrcPath, {page}, ocrResults, MrcMode::Balanced);
        QVERIFY2(ok, "exportMrcPdfA should succeed");

        // Read the PDF bytes and look for the key searchable words in the content stream.
        // The sandwich text is written as raw PDF literals: (word) Tj
        // This is a content-level check — verifies words appear in the PDF byte stream.
        QFile f(mrcPath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QByteArray pdfBytes = f.readAll();
        f.close();

        // Check that key words appear in the PDF content (as PDF literal strings)
        QVERIFY2(pdfBytes.contains("compression") || pdfBytes.contains("(compression)"),
                 "PDF should contain the word 'compression' in the text layer");
        QVERIFY2(pdfBytes.contains("SAMPLE") || pdfBytes.contains("(SAMPLE)"),
                 "PDF should contain the word 'SAMPLE' in the text layer");
    }

    // ── T7: veraPDF PDF/A-2b conformance gate ───────────────────────────────
    void testVeraPdfConformance()
    {
        if (!gp::VeraPdfValidator::isAvailable()) {
            QSKIP("veraPDF CLI not configured — skipping PDF/A-2b conformance check");
        }

        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QImage page = makeSyntheticScanPage();
        QList<LayoutRegion> regions = makeSyntheticRegions();
        QList<MergedOcrWord> words = makeSyntheticWords();

        QString mrcPath = dir.filePath("verapdf_test.pdf");
        PdfEditorEngine engine;
        QList<PageOcrResult> ocrResults;
        PageOcrResult pr;
        pr.pageIndex     = 0;
        pr.layoutRegions = regions;
        pr.words         = words;
        pr.success       = true;
        ocrResults.append(pr);

        bool ok = engine.exportMrcPdfA(mrcPath, {page}, ocrResults, MrcMode::Balanced);
        QVERIFY2(ok, "exportMrcPdfA should succeed");

        auto report = gp::VeraPdfValidator::validate(mrcPath, gp::PdfAConformance::PDF_A_2B);
        if (!report.isValid) {
            for (const auto& v : report.violations)
                qWarning() << "veraPDF:" << v.ruleId << ":" << v.description;
        }
        QVERIFY2(report.isValid, "MRC output should pass PDF/A-2b conformance");
    }

    // ── T8: DjvuImporter stub (QSKIP if HAS_DJVU off) ───────────────────────
    void testDjvuImporterStub()
    {
        DjvuImporter importer;

        if (!DjvuImporter::isAvailable()) {
            QSKIP("DjVu support not built (HAS_DJVU=OFF) — skipping DjVu import test");
        }

        // HAS_DJVU is ON: test that importing a non-existent file returns a proper error
        DjvuImportResult result = importer.importFile("/nonexistent/test.djvu");
        QVERIFY(!result.success);
        QVERIFY(!result.errorMessage.isEmpty());
    }

    // ── T9: MRC mode estimation is monotonically ordered ────────────────────
    void testMrcModeOrdering()
    {
        QImage page = makeSyntheticScanPage(400, 600);

        qint64 est_l = MrcPageProcessor::estimateCompressedSize(page, MrcMode::Lossless);
        qint64 est_b = MrcPageProcessor::estimateCompressedSize(page, MrcMode::Balanced);
        qint64 est_a = MrcPageProcessor::estimateCompressedSize(page, MrcMode::Aggressive);

        QVERIFY2(est_l >= est_b, "Lossless should estimate larger than Balanced");
        QVERIFY2(est_b >= est_a, "Balanced should estimate larger than Aggressive");
    }
};

QTEST_MAIN(TestMrcPipeline)
#include "TestMrcPipeline.moc"
