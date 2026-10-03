// SPDX-License-Identifier: Apache-2.0
#include "ConvertController.h"
#include "core/AppContext.h"
#include "core/OcrTypes.h"
#include "core/PolicyController.h"
#include "core/TempFileManager.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "core/interfaces/IConversionEngine.h"
#include "core/interfaces/IPdfEditorEngine.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QProgressDialog>
#include <QThread>
#include <QPointer>
#include <QMetaObject>
#include <QCoreApplication>
#include <atomic>
#include <QFileInfo>
#include <QCheckBox>
#include <QPushButton>
#include <QSettings>
#include <QPdfDocument>
#include "shell/StatusBar.h"
#include "engines/ConversionManager.h"
#include "engines/OcrEngine.h"
#include "engines/PdfEditorEngine.h"
#include "engines/ocr/OcrPipeline.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "modes/CompressDialog.h"

namespace gp {

QString ConvertController::localProcessingNotice()
{
    return QObject::tr("Processed 100% locally — no internet, no upload.");
}

// ── §4 #5 / July P1 row 27: OCR exposure in the text-format export dialogs ──

QString ConvertController::scannedOfferPrefKey()
{
    return QStringLiteral("ocr/scannedExportOffer");
}

ConvertController::TextProbe ConvertController::probeDocumentText(const QString& pdfPath,
                                                                  int maxPages)
{
    // Fresh backend per probe — the same ownership pattern the conversion
    // workers use. Bounded to the first pages: the probe exists to catch the
    // "scanned document" case, not to audit the whole file.
    PdfiumBackend backend;
    if (!backend.loadDocument(pdfPath))
        return TextProbe::Unknown;
    const int pages = qMin(qMax(1, maxPages), backend.pageCount());
    for (int p = 0; p < pages; ++p) {
        const auto runs = backend.extractPageTextRuns(p);
        for (const auto& run : runs) {
            if (!run.text.trimmed().isEmpty())
                return TextProbe::HasText;
        }
    }
    return TextProbe::Scanned;
}

// The PRODUCT read of the scanned-export master switch (§4 #5): the shipped
// default is ON — the offer exists out of the box. Both consumers
// (promptScannedOcrChoice's early-out and gateScannedExportChoice's gate)
// read the key through THIS function, so the default lives in product code
// and the pins assert GlyphPDF's choice, not QSettings' default-argument
// behavior. (findings-tests 2026-10-02: extracted from the two inline
// `settings.value(scannedOfferPrefKey(), true)` reads, byte-identical.)
bool ConvertController::scannedOfferEnabledByPref()
{
    return QSettings().value(scannedOfferPrefKey(), true).toBool();
}

ConvertController::ScannedChoice ConvertController::promptScannedOcrChoice()
{
    QSettings settings;
    if (!scannedOfferEnabledByPref())
        return ScannedChoice::ExportAsIs;   // master switch off — behave as before

    QMessageBox box(_mainWindow);
    box.setWindowTitle(tr("Scanned document detected"));
    box.setIcon(QMessageBox::Question);
    box.setText(tr("The first pages of this document contain no extractable text — it looks like a scan."));
    box.setInformativeText(tr(
        "Exporting without OCR produces output with little or no text.\n\n"
        "Run OCR first? GlyphPDF recognizes the scanned pages into a temporary "
        "searchable copy (100% locally) and exports the recognized text."));
    QAbstractButton* runOcr = box.addButton(tr("Run OCR, then export"), QMessageBox::YesRole);
    QAbstractButton* asIs   = box.addButton(tr("Export without OCR"), QMessageBox::NoRole);
    box.addButton(QMessageBox::Cancel);
    auto* dontAsk = new QCheckBox(tr("Don't ask again for scanned documents"), &box);
    box.setCheckBox(dontAsk);
    box.exec();

    if (dontAsk->isChecked())
        settings.setValue(scannedOfferPrefKey(), false);
    if (box.clickedButton() == runOcr) return ScannedChoice::RunOcr;
    if (box.clickedButton() == asIs)   return ScannedChoice::ExportAsIs;
    return ScannedChoice::Cancel;
}

bool ConvertController::gateScannedExportChoice(const QString& inputPath, bool* ocrFirst)
{
    *ocrFirst = false;
    {
        QSettings settings;
        if (!scannedOfferEnabledByPref())
            return true;
    }
    // Only a positive "scanned" probe triggers the offer: a document with
    // text exports exactly as before, and an unprobeable document fails
    // honestly in the export itself rather than through a guessed answer.
    if (probeDocumentText(inputPath) != TextProbe::Scanned)
        return true;
    switch (promptScannedOcrChoice()) {
    case ScannedChoice::RunOcr:     *ocrFirst = true; return true;
    case ScannedChoice::ExportAsIs: return true;
    case ScannedChoice::Cancel:     return false;
    }
    return false;
}

// §4 #5: whole-document OCR → temporary SEARCHABLE copy (MRC PDF/A, the same
// production writer the batch OCR op and the interactive Accept use). The
// text-format converter then extracts from that copy, so the exported
// Word/Excel/CSV/Text content is the RECOGNIZED text. Returns the temp path,
// or empty with a stage-specific message in *errorOut. Runs entirely inside
// the export worker: a fresh OCR engine and a fresh QPdfDocument per call —
// no shared state with the app-wide engine Batch Mode serializes.
//
// (findings-tests 2026-10-02): hoisted from the anonymous namespace to a
// public static seam, body UNCHANGED — the row-5 honest-abort contract ("OCR
// stage fails → the export aborts, never a silent un-OCR'd fallback") is
// exactly this function's empty-result-plus-typed-message shape, and the
// export workers consume nothing else (the source.isEmpty() early-failure
// before convertTo). Static member (the EditController::buildPageOcrResult
// pure-seam idiom) so the pins can read the contract without driving the
// five modal export dialogs.
QString ConvertController::buildSearchableOcrCopy(const QString& inputPath,
                                                  const QString& engineLang,
                                                  const OcrPreprocessOptions& preprocess,
                                                  QString* errorOut)
{
    auto engine = std::make_shared<OcrEngine>();
    if (!engine->initialize(engineLang)) {
        // emergence E-2 wording discipline: name the machine policy when it
        // manages the OCR download (same message contract as EditController).
        auto& policy = gp::PolicyController::instance();
        policy.ensureLoaded();
        const QString downloadKey = QStringLiteral("ocr/allowNetworkDownload");
        *errorOut = policy.isManaged(downloadKey)
            ? QObject::tr("OCR failed: Tesseract language data for '%1' is unavailable, "
                          "and the download that would provide it is managed by machine "
                          "policy (ocr/allowNetworkDownload — see the Network Touchpoints "
                          "page for the effective value).").arg(engineLang)
            : QObject::tr("OCR failed: Tesseract language data for '%1' is unavailable.").arg(engineLang);
        return {};
    }
    OcrPipeline pipeline(engine);
    pipeline.setStrategy(OcrStrategy::PrimaryOnly);
    pipeline.setPreprocessing(preprocess);

    QPdfDocument pdf;
    pdf.load(inputPath);
    if (pdf.status() != QPdfDocument::Status::Ready || pdf.pageCount() <= 0) {
        *errorOut = QObject::tr("OCR failed: could not open the document for rendering.");
        return {};
    }

    const double dpi = 150.0;
    QList<QImage> images;
    QList<PageOcrResult> results;
    images.reserve(pdf.pageCount());
    results.reserve(pdf.pageCount());
    for (int p = 0; p < pdf.pageCount(); ++p) {
        const QSizeF pts = pdf.pagePointSize(p);
        const QSize px(qMax(1, int(pts.width()  * dpi / 72.0)),
                       qMax(1, int(pts.height() * dpi / 72.0)));
        const QImage img = pdf.render(p, px);
        if (img.isNull()) {
            *errorOut = QObject::tr("OCR failed: could not render page %1.").arg(p + 1);
            return {};
        }
        PageOcrResult pr;
        pr.pageIndex = p;
        pr.words     = pipeline.run(img);
        pr.success   = true;
        images.append(img);
        results.append(pr);
    }

    bool anyWords = false;
    for (const auto& r : results) {
        if (!r.words.isEmpty()) { anyWords = true; break; }
    }
    if (!anyWords) {
        *errorOut = QObject::tr("OCR recognized no text in this document — there is nothing to export.");
        return {};
    }

    PdfEditorEngine writer;
    const QString tmp = TempFileManager::instance().createTempFile(QStringLiteral(".pdf"));
    if (tmp.isEmpty()) {
        *errorOut = QObject::tr("OCR failed: could not create the temporary searchable copy.");
        return {};
    }
    if (!writer.exportMrcPdfA(tmp, images, results)) {
        *errorOut = QObject::tr("OCR failed: could not write the temporary searchable copy.");
        QFile::remove(tmp);
        return {};
    }
    return tmp;
}

// ── U08 pre-execution capability disclosure ──────────────────────────────────

bool ConvertController::gateExport(gp::CapId id)
{
    // No registry (tests, early boot) → previous unconditional behavior.
    if (!_ctx || !_ctx->capabilities) return true;
    const gp::Capability c = _ctx->capabilities->query(id);
    if (c.status == gp::Availability::Available) return true;

    // Unavailable*: the format is never offered — explain why not and name a
    // supported alternative BEFORE any file dialog or worker thread exists.
    const QString explanation = gp::CapabilityRegistry::combineWhyNot(c);
    _mainWindow->statusBar()->showMessage(explanation, 8000);
    QMessageBox::information(_mainWindow, tr("Not Available"), explanation);
    return false;
}

QString ConvertController::exportFormatNotice(gp::CapId id) const
{
    if (!_ctx || !_ctx->capabilities) return {};
    const gp::Capability c = _ctx->capabilities->query(id);
    if (c.status == gp::Availability::Available) return c.detail;
    return gp::CapabilityRegistry::combineWhyNot(c);
}

ConvertController::ConvertController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> ConvertController::handledTools() const {
    return {
        ToolId::Combine, ToolId::ToWord, ToolId::ToExcel, ToolId::ToCsv,
        ToolId::ToHtml, ToolId::ToText, ToolId::Compress,
        ToolId::ToPPT, ToolId::ToImage, ToolId::Linearize, ToolId::PdfA
    };
}

void ConvertController::activate(ToolId id) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer && id != ToolId::Combine) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    switch (id) {
    case ToolId::Combine:
        mergePdfs();
        break;
    case ToolId::ToWord:
        exportToWord();
        break;
    case ToolId::ToExcel:
        exportToExcel();
        break;
    case ToolId::ToCsv:
        exportToCsv();
        break;
    case ToolId::ToHtml:
        exportToHtml();
        break;
    case ToolId::ToText:
        exportToText();
        break;
    case ToolId::ToPPT:
        exportToPowerPoint();
        break;
    case ToolId::ToImage:
        exportToImage();
        break;
    case ToolId::Compress:
        openCompressDialog();
        break;
    case ToolId::Linearize:
        linearizeDocument();
        break;
    case ToolId::PdfA:
        exportAsPdfA();
        break;
    default:
        break;
    }
}

void ConvertController::exportToWord() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    // U08: gate + format disclosure BEFORE the file dialog — the user learns
    // which writer will run (§9.16 honest badge) before picking a
    // destination. The post-write Fallback warning below stays as the
    // last-resort net until the degraded path is proven unreachable.
    if (!gateExport(gp::CapId::WordExport)) return;
    const QString formatNotice = exportFormatNotice(gp::CapId::WordExport);
    if (!formatNotice.isEmpty())
        _mainWindow->statusBar()->showMessage(formatNotice);
    // §4 #5: offer OCR when the document probes as scanned (before the file
    // dialog — the choice shapes what the export will contain).
    const QString inputPath = viewer->filePath();
    bool ocrFirst = false;
    if (!gateScannedExportChoice(inputPath, &ocrFirst)) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to Word"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".docx",
        tr("Word Documents (*.docx)"));
    if (outputPath.isEmpty()) {
        _mainWindow->statusBar()->clearMessage();
        return;
    }

    _mainWindow->statusBar()->showMessage(ocrFirst
        ? tr("Running OCR (recognizing scanned text), then converting to Word...")
        : tr("Converting to Word..."));

    auto* progress = new QProgressDialog(ocrFirst
        ? tr("Running OCR (recognizing scanned text), then converting to Word...")
        : tr("Converting to Word..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    auto* convMgr = dynamic_cast<ConversionManager*>(conv);
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);
    auto fallback = std::make_shared<std::atomic<bool>>(false);
    auto workerError = std::make_shared<QString>();
    // §4 #5: read the OCR prefs on the GUI thread (QSettings is not
    // thread-safe); the worker only uses the resolved copies.
    const QString ocrLang = ocrEngineLanguageCode(QSettings().value(
        QStringLiteral("ocr/language"), QStringLiteral("EN")).toString());
    OcrPreprocessOptions preprocess;
    preprocess.deskew   = QSettings().value(QStringLiteral("ocr/preprocessDeskew"), false).toBool();
    preprocess.binarize = QSettings().value(QStringLiteral("ocr/preprocessBinarize"), false).toBool();
    preprocess.denoise  = QSettings().value(QStringLiteral("ocr/preprocessDenoise"), false).toBool();
    preprocess.orientDetect = QSettings().value(QStringLiteral("ocr/orientDetect"), false).toBool();

    QThread* worker = QThread::create([conv, convMgr, inputPath, outputPath, result, fallback,
                                       ocrFirst, workerError, ocrLang, preprocess]() {
        QString stageError;
        QString source = inputPath;
        if (ocrFirst) {
            source = buildSearchableOcrCopy(inputPath, ocrLang, preprocess, &stageError);
            if (source.isEmpty()) {
                *workerError = stageError;
                result->store(false);
                return;
            }
        }
        bool ok = conv->convertTo(source, outputPath, IConversionEngine::TargetFormat::Word);
        if (ocrFirst)
            QFile::remove(source);
        result->store(ok);
        if (!ok && ocrFirst && stageError.isEmpty())
            *workerError = QObject::tr("The conversion failed after OCR.");
        // §9.16 P0 / §9.5 P0: detect whether a mislabeled fallback produced
        // this file. Only ExportEngine::Fallback warns — NativeOoxml (duckx)
        // and InHouseOoxml (built-in WordprocessingML writer) are both real
        // OOXML, so the HTML-as-docx warning must not fire for them.
        if (convMgr && ok)
            fallback->store(convMgr->lastWordExportEngine() == ConversionManager::ExportEngine::Fallback);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result, fallback, workerError, ocrFirst]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(
                ocrFirst
                    ? tr("OCR + export complete: %1 · %2").arg(outputPath, localProcessingNotice())
                    : tr("Export complete: %1 · %2").arg(outputPath, localProcessingNotice()),
                5000);
            if (fallback->load()) {
                // Honest disclosure: the .docx is actually HTML bytes.
                QMessageBox::warning(self->_mainWindow, tr("Export Format Notice"),
                    tr("This build lacks the native Word (OOXML) writer, so the exported file\n%1\n"
                       "contains HTML content under a .docx extension. Word may show a repair prompt.\n\n"
                       "For best results, choose HTML export instead.").arg(outputPath));
            }
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"),
                    ocrFirst
                        ? tr("OCR + export to Word complete. Open file?")
                        : tr("Export to Word complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else if (ocrFirst && !workerError->isEmpty()) {
            // §4 #5 honest failure: the OCR stage failed — the export is NOT
            // silently retried without OCR.
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), *workerError);
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to convert document to Word."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportToExcel() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    // U08: gate + pre-dialog writer disclosure, mirroring exportToWord.
    if (!gateExport(gp::CapId::ExcelExport)) return;
    const QString formatNotice = exportFormatNotice(gp::CapId::ExcelExport);
    if (!formatNotice.isEmpty())
        _mainWindow->statusBar()->showMessage(formatNotice);
    // §4 #5: scanned-document OCR offer (see exportToWord).
    const QString inputPath = viewer->filePath();
    bool ocrFirst = false;
    if (!gateScannedExportChoice(inputPath, &ocrFirst)) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to Excel"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".xlsx",
        tr("Excel Workbooks (*.xlsx)"));
    if (outputPath.isEmpty()) {
        _mainWindow->statusBar()->clearMessage();
        return;
    }

    _mainWindow->statusBar()->showMessage(ocrFirst
        ? tr("Running OCR (recognizing scanned text), then converting to Excel...")
        : tr("Converting to Excel..."));

    auto* progress = new QProgressDialog(ocrFirst
        ? tr("Running OCR (recognizing scanned text), then converting to Excel...")
        : tr("Converting to Excel..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    auto* convMgr = dynamic_cast<ConversionManager*>(conv);
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);
    auto fallback = std::make_shared<std::atomic<bool>>(false);
    auto workerError = std::make_shared<QString>();
    // §4 #5: OCR prefs captured on the GUI thread (see exportToWord).
    const QString ocrLang = ocrEngineLanguageCode(QSettings().value(
        QStringLiteral("ocr/language"), QStringLiteral("EN")).toString());
    OcrPreprocessOptions preprocess;
    preprocess.deskew   = QSettings().value(QStringLiteral("ocr/preprocessDeskew"), false).toBool();
    preprocess.binarize = QSettings().value(QStringLiteral("ocr/preprocessBinarize"), false).toBool();
    preprocess.denoise  = QSettings().value(QStringLiteral("ocr/preprocessDenoise"), false).toBool();
    preprocess.orientDetect = QSettings().value(QStringLiteral("ocr/orientDetect"), false).toBool();

    QThread* worker = QThread::create([conv, convMgr, inputPath, outputPath, result, fallback,
                                       ocrFirst, workerError, ocrLang, preprocess]() {
        QString stageError;
        QString source = inputPath;
        if (ocrFirst) {
            source = buildSearchableOcrCopy(inputPath, ocrLang, preprocess, &stageError);
            if (source.isEmpty()) {
                *workerError = stageError;
                result->store(false);
                return;
            }
        }
        bool ok = conv->convertTo(source, outputPath, IConversionEngine::TargetFormat::Excel);
        if (ocrFirst)
            QFile::remove(source);
        result->store(ok);
        if (!ok && ocrFirst && stageError.isEmpty())
            *workerError = QObject::tr("The conversion failed after OCR.");
        // §9.16 P0 / §9.5 P0: detect whether a mislabeled fallback produced
        // this file. Only ExportEngine::Fallback warns — NativeOoxml
        // (OpenXLSX) and InHouseOoxml (built-in SpreadsheetML writer) are both
        // real OOXML, so the CSV-as-xlsx warning must not fire for them.
        if (convMgr && ok)
            fallback->store(convMgr->lastExcelExportEngine() == ConversionManager::ExportEngine::Fallback);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result, fallback, workerError, ocrFirst]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(
                ocrFirst
                    ? tr("OCR + export complete: %1 · %2").arg(outputPath, localProcessingNotice())
                    : tr("Export complete: %1 · %2").arg(outputPath, localProcessingNotice()),
                5000);
            if (fallback->load()) {
                QMessageBox::warning(self->_mainWindow, tr("Export Format Notice"),
                    tr("This build lacks the native Excel (OOXML) writer, so the exported file\n%1\n"
                       "contains CSV content under a .xlsx extension. Excel may show a repair prompt.\n\n"
                       "For best results, choose CSV export instead.").arg(outputPath));
            }
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"),
                    ocrFirst
                        ? tr("OCR + export to Excel complete. Open file?")
                        : tr("Export to Excel complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else if (ocrFirst && !workerError->isEmpty()) {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), *workerError);
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to convert document to Excel."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportToCsv() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    if (!gateExport(gp::CapId::CsvExport)) return;
    // §4 #5: scanned-document OCR offer (see exportToWord).
    const QString inputPath = viewer->filePath();
    bool ocrFirst = false;
    if (!gateScannedExportChoice(inputPath, &ocrFirst)) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to CSV"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".csv",
        tr("CSV Files (*.csv)"));
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(ocrFirst
        ? tr("Running OCR (recognizing scanned text), then extracting to CSV...")
        : tr("Extracting to CSV..."));

    auto* progress = new QProgressDialog(ocrFirst
        ? tr("Running OCR (recognizing scanned text), then extracting to CSV...")
        : tr("Extracting to CSV..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);
    auto workerError = std::make_shared<QString>();
    // §4 #5: OCR prefs captured on the GUI thread (see exportToWord).
    const QString ocrLang = ocrEngineLanguageCode(QSettings().value(
        QStringLiteral("ocr/language"), QStringLiteral("EN")).toString());
    OcrPreprocessOptions preprocess;
    preprocess.deskew   = QSettings().value(QStringLiteral("ocr/preprocessDeskew"), false).toBool();
    preprocess.binarize = QSettings().value(QStringLiteral("ocr/preprocessBinarize"), false).toBool();
    preprocess.denoise  = QSettings().value(QStringLiteral("ocr/preprocessDenoise"), false).toBool();
    preprocess.orientDetect = QSettings().value(QStringLiteral("ocr/orientDetect"), false).toBool();

    QThread* worker = QThread::create([conv, inputPath, outputPath, result, ocrFirst, workerError, ocrLang, preprocess]() {
        QString stageError;
        QString source = inputPath;
        if (ocrFirst) {
            source = buildSearchableOcrCopy(inputPath, ocrLang, preprocess, &stageError);
            if (source.isEmpty()) {
                *workerError = stageError;
                result->store(false);
                return;
            }
        }
        bool ok = conv->convertTo(source, outputPath, IConversionEngine::TargetFormat::Csv);
        if (ocrFirst)
            QFile::remove(source);
        result->store(ok);
        if (!ok && ocrFirst && stageError.isEmpty())
            *workerError = QObject::tr("The conversion failed after OCR.");
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result, workerError, ocrFirst]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(
                ocrFirst
                    ? tr("OCR + export complete: %1 · %2").arg(outputPath, localProcessingNotice())
                    : tr("Export complete: %1 · %2").arg(outputPath, localProcessingNotice()),
                5000);
        } else if (ocrFirst && !workerError->isEmpty()) {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), *workerError);
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to extract data to CSV."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::mergePdfs() {
    QStringList files = QFileDialog::getOpenFileNames(_mainWindow, tr("Select PDFs to Merge"), "", tr("PDF Files (*.pdf)"));
    if (files.isEmpty()) return;
    QString outputFile = QFileDialog::getSaveFileName(_mainWindow, tr("Save Merged PDF"), "", tr("PDF Files (*.pdf)"));
    if (outputFile.isEmpty()) return;

    auto* progress = new QProgressDialog(tr("Merging documents..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);

    QPointer<ConvertController> self(this);
    auto ok = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([files, outputFile, ok]() {
        ok->store(PdfViewerWidget::mergeDocuments(files, outputFile));
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, files, outputFile, ok]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        if (!ok->load()) {
            QMessageBox::critical(self->_mainWindow, QObject::tr("Merge Failed"),
                QObject::tr("Merging %1 files failed. The output file was not written (or is incomplete).\n\n"
                            "Check that the input files are valid PDFs and the output location is writable.")
                    .arg(files.size()));
            self->_mainWindow->statusBar()->showMessage(QObject::tr("Merge failed."), 5000);
            return;
        }
        self->_mainWindow->statusBar()->showMessage(
            QObject::tr("Successfully merged %1 files to %2").arg(files.size()).arg(outputFile), 5000);
        if (QMessageBox::question(self->_mainWindow, QObject::tr("Open Merged PDF"),
                QObject::tr("Merge complete. Would you like to open the output file?")) == QMessageBox::Yes) {
            self->_mainWindow->openDocument(outputFile);
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::linearizeDocument() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;
    if (!gateExport(gp::CapId::Linearize)) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Save Linearized (Web-Optimized) PDF"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + "_optimized.pdf",
        tr("PDF Files (*.pdf)"));
    if (outputPath.isEmpty()) return;

    auto* progress = new QProgressDialog(tr("Linearizing document (Fast Web View)..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);

    const QString inputPath = viewer->filePath();
    IPdfEditorEngine* engine = _ctx->pdfEditor.get();
    QPointer<ConvertController> self(this);

    // Fix L: delete any pre-existing file at the target path so QFileInfo::exists
    // is a real success signal, not a leftover-file false positive.
    if (QFileInfo::exists(outputPath) && !QFile::remove(outputPath)) {
        progress->close();
        progress->deleteLater();
        QMessageBox::critical(_mainWindow, tr("Error"),
            tr("Could not overwrite existing file at: %1").arg(outputPath));
        return;
    }

    QThread* worker = QThread::create([engine, inputPath, outputPath]() {
        if (engine->currentFile() != inputPath)
            engine->loadDocumentForEditing(inputPath);
        engine->linearizeDocument(outputPath);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, engine, inputPath]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        if (QFileInfo::exists(outputPath)) {
            self->_mainWindow->statusBar()->showMessage(QObject::tr("Optimization complete: %1").arg(outputPath), 5000);
            if (QMessageBox::question(self->_mainWindow, QObject::tr("Linearization Success"),
                    QObject::tr("Linearization complete. Open file?")) == QMessageBox::Yes) {
                self->_mainWindow->openDocument(outputPath);
            }
        } else {
            QMessageBox::critical(self->_mainWindow, QObject::tr("Error"), QObject::tr("Failed to linearize document."));
            self->_mainWindow->statusBar()->showMessage(QObject::tr("Linearization failed."));
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportAsPdfA() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;
    if (!gateExport(gp::CapId::PdfAExport)) return;
    QStringList levels;
    levels << tr("PDF/A-1b (ISO 19005-1)") << tr("PDF/A-2b (ISO 19005-2)") << tr("PDF/A-3b (ISO 19005-3)");
    bool ok;
    QString selected = QInputDialog::getItem(_mainWindow, tr("PDF/A Conformance Level"),
        tr("Select archival conformance level:"), levels, 0, false, &ok);
    if (!ok) return;

    int level = 1;
    if (selected.contains("2b")) level = 2;
    else if (selected.contains("3b")) level = 3;

    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export as PDF/A"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + "_pdfa.pdf",
        tr("PDF Files (*.pdf)"));
    if (outputPath.isEmpty()) return;

    auto* progress = new QProgressDialog(tr("Exporting as PDF/A..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);

    const QString inputPath = viewer->filePath();
    IPdfEditorEngine* engine = _ctx->pdfEditor.get();
    QPointer<ConvertController> self(this);

    // Fix L: delete any pre-existing file at the target path so QFileInfo::exists
    // is a real success signal, not a leftover-file false positive.
    if (QFileInfo::exists(outputPath) && !QFile::remove(outputPath)) {
        progress->close();
        progress->deleteLater();
        QMessageBox::critical(_mainWindow, tr("Error"),
            tr("Could not overwrite existing file at: %1").arg(outputPath));
        return;
    }

    QThread* worker = QThread::create([engine, inputPath, outputPath, level]() {
        engine->loadDocumentForEditing(inputPath);
        engine->exportPdfA(outputPath, level);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        if (QFileInfo::exists(outputPath)) {
            self->_mainWindow->statusBar()->showMessage(QObject::tr("PDF/A export complete: %1").arg(outputPath), 5000);
            if (QMessageBox::question(self->_mainWindow, QObject::tr("Export Success"),
                    QObject::tr("PDF/A export complete. Open file?")) == QMessageBox::Yes) {
                self->_mainWindow->openDocument(outputPath);
            }
        } else {
            QMessageBox::critical(self->_mainWindow, QObject::tr("Error"), QObject::tr("Failed to export as PDF/A."));
            self->_mainWindow->statusBar()->showMessage(QObject::tr("PDF/A export failed."));
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}


void ConvertController::exportToHtml() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    if (!gateExport(gp::CapId::HtmlExport)) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to HTML"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".html",
        tr("HTML Files (*.html)"));
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(tr("Converting to HTML..."));

    auto* progress = new QProgressDialog(tr("Converting to HTML..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, inputPath, outputPath, result]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::Html);
        result->store(ok);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1 · %2").arg(outputPath, localProcessingNotice()), 5000);
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"), tr("Export to HTML complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to convert document to HTML."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportToText() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    if (!gateExport(gp::CapId::TextExport)) return;
    // §4 #5: scanned-document OCR offer (see exportToWord).
    const QString inputPath = viewer->filePath();
    bool ocrFirst = false;
    if (!gateScannedExportChoice(inputPath, &ocrFirst)) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to Text"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".txt",
        tr("Text Files (*.txt)"));
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(ocrFirst
        ? tr("Running OCR (recognizing scanned text), then converting to Text...")
        : tr("Converting to Text..."));

    auto* progress = new QProgressDialog(ocrFirst
        ? tr("Running OCR (recognizing scanned text), then converting to Text...")
        : tr("Converting to Text..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);
    auto workerError = std::make_shared<QString>();
    // §4 #5: OCR prefs captured on the GUI thread (see exportToWord).
    const QString ocrLang = ocrEngineLanguageCode(QSettings().value(
        QStringLiteral("ocr/language"), QStringLiteral("EN")).toString());
    OcrPreprocessOptions preprocess;
    preprocess.deskew   = QSettings().value(QStringLiteral("ocr/preprocessDeskew"), false).toBool();
    preprocess.binarize = QSettings().value(QStringLiteral("ocr/preprocessBinarize"), false).toBool();
    preprocess.denoise  = QSettings().value(QStringLiteral("ocr/preprocessDenoise"), false).toBool();
    preprocess.orientDetect = QSettings().value(QStringLiteral("ocr/orientDetect"), false).toBool();

    QThread* worker = QThread::create([conv, inputPath, outputPath, result, ocrFirst, workerError, ocrLang, preprocess]() {
        QString stageError;
        QString source = inputPath;
        if (ocrFirst) {
            source = buildSearchableOcrCopy(inputPath, ocrLang, preprocess, &stageError);
            if (source.isEmpty()) {
                *workerError = stageError;
                result->store(false);
                return;
            }
        }
        bool ok = conv->convertTo(source, outputPath, IConversionEngine::TargetFormat::Text);
        if (ocrFirst)
            QFile::remove(source);
        result->store(ok);
        if (!ok && ocrFirst && stageError.isEmpty())
            *workerError = QObject::tr("The conversion failed after OCR.");
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result, workerError, ocrFirst]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(
                ocrFirst
                    ? tr("OCR + export complete: %1 · %2").arg(outputPath, localProcessingNotice())
                    : tr("Export complete: %1 · %2").arg(outputPath, localProcessingNotice()),
                5000);
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"),
                    ocrFirst
                        ? tr("OCR + export to Text complete. Open file?")
                        : tr("Export to Text complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else if (ocrFirst && !workerError->isEmpty()) {
            // §4 #5 honest failure: the OCR stage failed — the export is NOT
            // silently retried without OCR.
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), *workerError);
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to convert document to Text."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportToPowerPoint() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    if (!gateExport(gp::CapId::PptExport)) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to PowerPoint"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".pptx",
        tr("PowerPoint Presentations (*.pptx)"));
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(tr("Converting to PowerPoint..."));

    auto* progress = new QProgressDialog(tr("Converting to PowerPoint..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, inputPath, outputPath, result]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::PowerPoint);
        result->store(ok);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1 · %2").arg(outputPath, localProcessingNotice()), 5000);
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"), tr("Export to PowerPoint complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to convert document to PowerPoint."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportToImage() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    if (!gateExport(gp::CapId::ImageExport)) return;

    // U08: capability-derived filter (fileFilterFor) replaces the hand-built
    // string — only formats this build can actually produce reach the dialog.
    // With ImageExport available in every build the output equals the previous
    // hand-built filter, byte for byte.
    QString imageFilter = tr("PNG Images (*.png);;JPEG Images (*.jpg);;TIFF Images (*.tif)");
    if (_ctx && _ctx->capabilities) {
        const QList<QPair<QString, gp::CapId>> clauses = {
            { tr("PNG Images (*.png)"),  gp::CapId::ImageExport },
            { tr("JPEG Images (*.jpg)"), gp::CapId::ImageExport },
            { tr("TIFF Images (*.tif)"), gp::CapId::ImageExport },
        };
        const QString built = _ctx->capabilities->fileFilterFor(clauses);
        if (!built.isEmpty()) imageFilter = built;
    }

    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to Image"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".png",
        imageFilter);
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(tr("Exporting to image..."));

    auto* progress = new QProgressDialog(tr("Exporting to image..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, inputPath, outputPath, result]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::Image);
        result->store(ok);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1 · %2").arg(outputPath, localProcessingNotice()), 5000);
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"), tr("Export to image complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to export document to image."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::openCompressDialog() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;
    
    CompressDialog dialog(_ctx, _mainWindow);
    dialog.exec();
}

} // namespace gp
