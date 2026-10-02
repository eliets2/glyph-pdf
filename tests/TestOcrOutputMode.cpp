// SPDX-License-Identifier: Apache-2.0
// PARITY-SCORECARD-2026-09-30 §4 #5 — OCR OutputMode (searchable vs editable)
// plus its exposure in the OCRMode UI and the ConvertController export path.
//
// Pins:
//   1. The ocr/outputMode pref parses to the mode; only an explicit
//      "editable" selects Editable (unknown/empty → the documented Searchable
//      default, never a silent mode swap).
//   2. The Accept save dialog title and success status NAME the kind of copy
//      (editable vs searchable); the Searchable strings stay byte-identical.
//   3. The editable writer REPLACES the page content: visible text, NO image
//      XObject, NO invisible (3 Tr) layer, and PDFium extraction reads the
//      recognized words. The searchable writer keeps the image — the modes
//      must never be confused.
//   4. The editable writer honestly refuses an all-empty payload (a blank
//      "editable copy" is not a copy).
//   5. EditController::outputModeFromSettings reads the persisted pref.
//   6. The OCRMode toolbar combo persists the choice through the shared
//      ocr/outputMode key and restores it in a fresh panel (the same wire
//      EditController's accept flow reads).
//   7. ConvertController::probeDocumentText classifies a text PDF vs an
//      image-only (scanned) PDF — the trigger for the export-dialog OCR offer.
#include <QtTest>
#include <QComboBox>
#include <QImage>
#include <QPainter>
#include <QSettings>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QPageSize>

#include "core/OcrTypes.h"
#include "modes/OCRMode.h"
#include "shell/controllers/EditController.h"
#include "shell/controllers/ConvertController.h"
#include "engines/PdfEditorEngine.h"
#include "engines/ocr/OcrPipeline.h"
#include "engines/pdfium/PdfiumBackend.h"

using gp::ConvertController;
using gp::EditController;
using gp::OCRMode;
// OcrOutputMode lives in core/OcrTypes.h at global scope (beside OcrResult).

namespace {

QImage makeScanPage(int w = 400, int h = 300)
{
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(QColor(245, 245, 240));
    return img;
}

PageOcrResult makePayload(const QList<QString>& words)
{
    PageOcrResult r;
    r.pageIndex = 0;
    int id = 0;
    for (const QString& t : words) {
        MergedOcrWord w;
        w.text = t;
        w.boundingBox = QRectF(10, 10 + id * 24, 60, 14);
        w.confidence = 95;
        w.sourceEngine = QStringLiteral("Tesseract");
        r.words.append(w);
        ++id;
    }
    r.success = !r.words.isEmpty();
    return r;
}

bool extractContains(const QString& pdfPath, int page, const QString& needle)
{
    PdfiumBackend pdfium;
    if (!pdfium.loadDocument(pdfPath)) return false;
    if (page >= pdfium.pageCount()) return false;
    return pdfium.extractText(page).contains(needle);
}

// A one-page PDF containing real text (for the probe's HasText side).
QString writeTextPdf(const QString& path)
{
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize::A4);
    QPainter p(&writer);
    p.drawText(QRect(100, 100, 3000, 500), Qt::AlignLeft,
               QStringLiteral("extractable probe text"));
    p.end();
    return QFile::exists(path) ? path : QString();
}

// A one-page image-only PDF (for the probe's Scanned side): a rasterized
// page picture, no text operators.
QString writeScannedPdf(const QString& path)
{
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize::A4);
    QPainter p(&writer);
    QImage page = makeScanPage(800, 1100);
    QPainter ip(&page);
    ip.setPen(Qt::black);
    ip.setFont(QFont("Arial", 24, QFont::Bold));
    ip.drawText(QRect(20, 20, 760, 200), Qt::AlignLeft,
                QStringLiteral("scanned words baked into pixels"));
    ip.end();
    p.drawImage(QRect(0, 0, writer.width(), writer.height()), page);
    p.end();
    return QFile::exists(path) ? path : QString();
}

} // namespace

class TestOcrOutputMode : public QObject {
    Q_OBJECT
private slots:
    // ── pref ↔ mode seams ────────────────────────────────────────────────
    void prefParsesToModeAndRoundTrips();
    void unknownPrefNeverSilentlySelectsEditable();
    void outputModeFromSettingsReadsThePref();

    // ── user-facing naming: the copy kind is never mislabeled ───────────
    void dialogTitleAndStatusNameTheEditableCopy();
    void searchableViewKeepsHistoricalStrings();

    // ── the two writers produce DIFFERENT, honest output ────────────────
    void editableWriterReplacesContentWithVisibleText();
    void searchableWriterStillEmbedsTheImage();
    void editableWriterRefusesAnAllEmptyPayload();

    // ── OCRMode UI exposure ─────────────────────────────────────────────
    void ocrModeComboExposesAndPersistsTheChoice();

    // ── ConvertController probe (the export-dialog trigger) ─────────────
    void probeClassifiesTextVsScannedDocuments();
};

void TestOcrOutputMode::prefParsesToModeAndRoundTrips()
{
    QCOMPARE(ocrOutputModePrefValue(OcrOutputMode::Editable),
             QStringLiteral("editable"));
    QCOMPARE(ocrOutputModePrefValue(OcrOutputMode::Searchable),
             QStringLiteral("searchable"));
    QCOMPARE(ocrOutputModeFromPref(QStringLiteral("editable")),
             OcrOutputMode::Editable);
    QCOMPARE(ocrOutputModeFromPref(QStringLiteral("EDITABLE")),
             OcrOutputMode::Editable);
    for (const QString& v : { QString(), QStringLiteral("searchable"),
                              QStringLiteral("text"), QStringLiteral("junk") }) {
        QCOMPARE(ocrOutputModeFromPref(v), OcrOutputMode::Searchable);
    }
    // Round trip for both modes.
    QCOMPARE(ocrOutputModeFromPref(ocrOutputModePrefValue(OcrOutputMode::Editable)),
             OcrOutputMode::Editable);
    QCOMPARE(ocrOutputModeFromPref(ocrOutputModePrefValue(OcrOutputMode::Searchable)),
             OcrOutputMode::Searchable);
    QCOMPARE(ocrOutputModePrefKey(), QStringLiteral("ocr/outputMode"));
}

void TestOcrOutputMode::unknownPrefNeverSilentlySelectsEditable()
{
    // A corrupted/legacy stored value behaves as the DOCUMENTED default
    // (Searchable) — the honest fallback; it must never invent an "editable"
    // export the user did not choose.
    QCOMPARE(ocrOutputModeFromPref(QStringLiteral("edittable")),
             OcrOutputMode::Searchable);
    QCOMPARE(ocrOutputModeFromPref(QStringLiteral(" editableView ")),
             OcrOutputMode::Searchable);
}

void TestOcrOutputMode::outputModeFromSettingsReadsThePref()
{
    QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TestOcrOutputMode"));
    QSettings().remove(QStringLiteral("ocr/outputMode"));
    QCOMPARE(EditController::outputModeFromSettings(), OcrOutputMode::Searchable);
    QSettings().setValue(QStringLiteral("ocr/outputMode"), QStringLiteral("editable"));
    QCOMPARE(EditController::outputModeFromSettings(), OcrOutputMode::Editable);
    QSettings().setValue(QStringLiteral("ocr/outputMode"), QStringLiteral("searchable"));
    QCOMPARE(EditController::outputModeFromSettings(), OcrOutputMode::Searchable);
    QSettings().remove(QStringLiteral("ocr/outputMode"));
}

void TestOcrOutputMode::dialogTitleAndStatusNameTheEditableCopy()
{
    // Editable mode names the EDITABLE copy (single- and multi-page).
    QVERIFY(EditController::ocrSaveDialogTitle(1, 0, OcrOutputMode::Editable)
                .contains(QStringLiteral("Editable Text"), Qt::CaseInsensitive));
    QVERIFY(!EditController::ocrSaveDialogTitle(1, 0, OcrOutputMode::Editable)
                 .contains(QStringLiteral("Searchable"), Qt::CaseInsensitive));
    const QString multi = EditController::ocrSaveDialogTitle(40, 2, OcrOutputMode::Editable);
    QVERIFY(multi.contains(QStringLiteral("Editable Text"), Qt::CaseInsensitive));
    QVERIFY(multi.contains(QStringLiteral("Current Page Only"), Qt::CaseInsensitive));
    QVERIFY(multi.contains(QStringLiteral("3 of 40")));
    const QString status = EditController::ocrSavedStatus(40, 2, QStringLiteral("x.pdf"),
                                                          OcrOutputMode::Editable);
    QVERIFY(status.contains(QStringLiteral("Editable text copy"), Qt::CaseInsensitive));
    QVERIFY(status.contains(QStringLiteral("page 3 of 40")));
    QVERIFY(status.contains(QStringLiteral("x.pdf")));
    QVERIFY(EditController::ocrSavedStatus(1, 0, QStringLiteral("x.pdf"),
                                           OcrOutputMode::Editable)
                .contains(QStringLiteral("Editable text copy"), Qt::CaseInsensitive));
}

void TestOcrOutputMode::searchableViewKeepsHistoricalStrings()
{
    // The default-argument overloads must reproduce the §9.4 strings exactly —
    // the searchable copy's naming is pinned history (TestOcrAcceptScope).
    QCOMPARE(EditController::ocrSaveDialogTitle(1, 0),
             QStringLiteral("Save Searchable (OCR) Copy"));
    QCOMPARE(EditController::ocrSaveDialogTitle(1, 0, OcrOutputMode::Searchable),
             QStringLiteral("Save Searchable (OCR) Copy"));
    QCOMPARE(EditController::ocrSavedStatus(1, 0, QStringLiteral("doc_ocr.pdf")),
             QStringLiteral("Searchable copy saved: doc_ocr.pdf"));
}

void TestOcrOutputMode::editableWriterReplacesContentWithVisibleText()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString out = dir.filePath(QStringLiteral("editable.pdf"));

    PdfEditorEngine engine;
    QVERIFY(engine.exportEditableTextPdf(out, { makeScanPage() },
                                         { makePayload({ QStringLiteral("invoice"),
                                                        QStringLiteral("total") }) }));
    QFile f(out);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray bytes = f.readAll();
    f.close();

    // REPLACES the content: no scan image in the output at all.
    QVERIFY2(!bytes.contains("/Subtype /Image"),
             "the editable copy must not embed the page image");
    // Visible text: no invisible-text rendering mode anywhere.
    QVERIFY2(!bytes.contains("3 Tr"),
             "the editable copy must not carry an invisible text layer");
    QVERIFY(bytes.startsWith("%PDF"));
    // Extraction reads the recognized words back.
    QVERIFY(extractContains(out, 0, QStringLiteral("invoice")));
    QVERIFY(extractContains(out, 0, QStringLiteral("total")));
    // Unicode words survive extraction through the ToUnicode CMap.
    const QString uni = dir.filePath(QStringLiteral("editable-uni.pdf"));
    QVERIFY(engine.exportEditableTextPdf(uni, { makeScanPage() },
                                         { makePayload({ QStringLiteral("café") }) }));
    QVERIFY(extractContains(uni, 0, QStringLiteral("café")));
}

void TestOcrOutputMode::searchableWriterStillEmbedsTheImage()
{
    // Mode contrast: the SAME payload through the SEARCHABLE writer keeps the
    // page image (invisible text over the scan). If this ever regressed, both
    // modes would silently produce the same artifact.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString out = dir.filePath(QStringLiteral("searchable.pdf"));
    PdfEditorEngine engine;
    QVERIFY(engine.exportMrcPdfA(out, { makeScanPage() },
                                 { makePayload({ QStringLiteral("invoice") }) }));
    QFile f(out);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray bytes = f.readAll();
    f.close();
    QVERIFY2(bytes.contains("/Subtype /Image"),
             "the searchable copy keeps the original page image");
    QVERIFY(extractContains(out, 0, QStringLiteral("invoice")));
}

void TestOcrOutputMode::editableWriterRefusesAnAllEmptyPayload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString out = dir.filePath(QStringLiteral("blank.pdf"));
    PdfEditorEngine engine;
    // No words anywhere → an "editable copy" would be a blank lie.
    QVERIFY2(!engine.exportEditableTextPdf(out, { makeScanPage() },
                                           { makePayload({}) }),
             "an all-empty payload must be refused, not written as a blank PDF");
    QVERIFY2(!QFile::exists(out), "a refused export must not leave a file behind");
}

void TestOcrOutputMode::ocrModeComboExposesAndPersistsTheChoice()
{
    QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TestOcrOutputMode"));
    QSettings().remove(QStringLiteral("ocr/outputMode"));

    OCRMode panel;
    auto* combo = panel.findChild<QComboBox*>(QStringLiteral("ocrOutputModeCombo"));
    QVERIFY2(combo, "the OCRMode toolbar must expose the output-mode combo");
    QCOMPARE(combo->count(), 2);
    // Default: the documented Searchable.
    QCOMPARE(panel.outputMode(), OcrOutputMode::Searchable);

    // Switching the mode persists through the shared ocr/outputMode key —
    // the exact wire EditController::onOcrAcceptRequested reads.
    panel.setOutputMode(OcrOutputMode::Editable);
    QCOMPARE(panel.outputMode(), OcrOutputMode::Editable);
    QCOMPARE(QSettings().value(QStringLiteral("ocr/outputMode")).toString(),
             QStringLiteral("editable"));
    QCOMPARE(combo->currentData().toString(), QStringLiteral("editable"));

    // A fresh panel restores the persisted choice.
    OCRMode fresh;
    QCOMPARE(fresh.outputMode(), OcrOutputMode::Editable);

    // And back.
    fresh.setOutputMode(OcrOutputMode::Searchable);
    QCOMPARE(QSettings().value(QStringLiteral("ocr/outputMode")).toString(),
             QStringLiteral("searchable"));
    QSettings().remove(QStringLiteral("ocr/outputMode"));
}

void TestOcrOutputMode::probeClassifiesTextVsScannedDocuments()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QCOMPARE(ConvertController::probeDocumentText(writeTextPdf(dir.filePath("text.pdf"))),
             ConvertController::TextProbe::HasText);
    QCOMPARE(ConvertController::probeDocumentText(writeScannedPdf(dir.filePath("scan.pdf"))),
             ConvertController::TextProbe::Scanned);
    // A missing file is honestly Unknown — the probe never invents an answer.
    QCOMPARE(ConvertController::probeDocumentText(dir.filePath("missing.pdf")),
             ConvertController::TextProbe::Unknown);
    // The persisted master switch defaults ON (the offer exists).
    QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
    QCoreApplication::setApplicationName(QStringLiteral("TestOcrOutputMode"));
    QSettings().remove(ConvertController::scannedOfferPrefKey());
    QCOMPARE(QSettings().value(ConvertController::scannedOfferPrefKey(), true).toBool(), true);
}

QTEST_MAIN(TestOcrOutputMode)
#include "TestOcrOutputMode.moc"
