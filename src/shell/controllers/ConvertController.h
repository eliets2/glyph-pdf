// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "core/Capability.h"
#include "core/interfaces/IToolController.h"
#include "engines/ocr/OcrPreprocessor.h" // OcrPreprocessOptions (§4 #5 OCR-stage seam)

struct AppContext;

namespace gp {

class MainWindow;

class ConvertController : public QObject, public IToolController {
    Q_OBJECT
public:
    ConvertController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

    // §9.16 P0: surfaced on export/import completions — every conversion
    // runs on-device (in-process engines or a local LibreOffice/qpdf
    // subprocess), so the privacy claim is factual, not marketing.
    static QString localProcessingNotice();

    // ── §4 #5 / July P1 row 27: OCR exposure in the export dialogs ──────────
    // Bounded probe: does the document carry ANY extractable text in its
    // first `maxPages` pages? (fresh PdfiumBackend, trimmed non-empty runs).
    // Unknown = the document could not be opened for probing — the export
    // path itself will fail honestly downstream; the probe never invents an
    // answer.
    enum class TextProbe { HasText, Scanned, Unknown };
    static TextProbe probeDocumentText(const QString& pdfPath, int maxPages = 3);
    // The persisted master switch for the scanned-document OCR offer
    // (the prompt's "Don't ask again" checkbox writes false here).
    static QString scannedOfferPrefKey();
    // §4 #5 seam (findings-tests 2026-10-02): the PRODUCT read of that
    // master switch — the shipped default is ON (the offer exists). Both
    // consumers below read the key through THIS function, so the default
    // lives in product code; the pins assert GlyphPDF's choice, never
    // QSettings' default-argument behavior.
    static bool scannedOfferEnabledByPref();
    // The user's decision when the export target looks like a scan.
    enum class ScannedChoice { RunOcr, ExportAsIs, Cancel };
    // §4 #5 seam (findings-tests 2026-10-02; hoisted from the anonymous
    // namespace, body unchanged): the export pipeline's OCR stage —
    // whole-document OCR into a temporary searchable MRC PDF/A copy. Returns
    // the temp path, or EMPTY with a stage-specific, human-readable message
    // in *errorOut. The empty-result-plus-typed-message shape IS the row-5
    // honest-abort contract: every export worker consumes exactly this
    // (`source.isEmpty()` → typed failure, no conversion — never a silent
    // un-OCR'd fallback of the original document). Static pure seam (the
    // EditController::buildPageOcrResult idiom): fresh engines per call, no
    // shared state.
    static QString buildSearchableOcrCopy(const QString& inputPath,
                                          const QString& engineLang,
                                          const OcrPreprocessOptions& preprocess,
                                          QString* errorOut = nullptr);

private:
    void exportToWord();
    void exportToExcel();
    void exportToCsv();
    void exportToHtml();
    void exportToText();
    void exportToPowerPoint();
    void exportToImage();
    void openCompressDialog();
    void mergePdfs();
    void linearizeDocument();
    void exportAsPdfA();

    // ── U08 pre-execution capability disclosure ─────────────────────────────
    // gateExport: false → the registry reported the format unavailable; the
    // whyNot + alternative are surfaced and NO file dialog / worker runs.
    // A null registry (tests, early boot) keeps the previous behavior.
    bool gateExport(gp::CapId id);
    // Pre-dialog format disclosure: with the in-house OOXML writers the real-
    // OOXML path is unconditional, so the notice names the writer that WILL
    // run (the §9.16 honest badge, moved before the file dialog).
    QString exportFormatNotice(gp::CapId id) const;

    // ── §4 #5: scanned-document OCR offer ───────────────────────────────────
    // Offer OCR before the save dialog when the document probes as scanned.
    // Returns false only when the user cancelled. *ocrFirst reports the OCR
    // decision; the offer is skipped entirely when the pref disables it or
    // the document demonstrably has text (or could not be probed — the
    // export itself will fail honestly downstream).
    bool gateScannedExportChoice(const QString& inputPath, bool* ocrFirst);
    // The modal three-way prompt (Run OCR / Export as-is / Cancel) with the
    // "Don't ask again" checkbox. Returns the decision.
    ScannedChoice promptScannedOcrChoice();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
