// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include <QRectF>
#include <QList>
#include <memory>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"
#include "engines/ocr/OcrPipeline.h" // PageOcrResult / MergedOcrWord (§9.4 Accept seam)

struct AppContext;
class EditToolBar;
class IOcrEngine;

namespace gp {

class MainWindow;

class EditController : public QObject, public IToolController {
    Q_OBJECT
public:
    EditController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

    // Search / replace slots wired from FindBar
    void onSearchRequested(const QString &text, bool forward, bool matchCase,
                           bool wholeWords, bool useRegex, int scope);
    void onReplaceRequested(const QString &searchText, const QString &replaceText,
                            bool matchCase, bool wholeWords, bool useRegex);
    void onReplaceAllRequested(const QString &searchText, const QString &replaceText,
                               bool matchCase, bool wholeWords, bool useRegex);
    void onRedactAllRequested(const QString &text, bool matchCase, bool wholeWords);

public slots:
    // Run OCR on the viewer's current page (engine chosen per Preferences). Public so
    // the OCR Verify screen's Run button can drive the same real pipeline as the ribbon.
    void runOcr();

    // §9.4 P0 test seam: assemble the per-page OCR payload for exportMrcPdfA.
    static PageOcrResult buildPageOcrResult(int pageIndex, const QList<MergedOcrWord>& words);

signals:
    // Emitted on the GUI thread when an OCR run finishes, carrying the recognised
    // words so the OCR Verify screen can display them for review.
    void ocrResultsReady(const QList<MergedOcrWord>& words);

private slots:
    void onImageSelected(const QString &name, const QRectF &placement);
    void onImageMoved(const QString &name, double dx, double dy);
    void onImageResized(const QString &name, double newW, double newH);
    void onTextEditRequested(int pageIndex, QPointF pos);
    void onTextFormatChanged(const QString &fontFamily, int fontSize, const QColor &color, bool bold, bool italic, int alignment);
    void onEraseRequested(int pageIndex, QPointF pos);

private:
    void editPdfText();
    void enterImageEditMode();
    bool copySelectionToClipboard();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
    bool _ocrRunning = false;

    // §9.4 P0: inputs of the most recent interactive OCR run, cached so that
    // Accept can persist a searchable MRC PDF/A copy (the PRD headline claim).
    QImage m_lastOcrPageImage;
    QList<MergedOcrWord> m_lastOcrWords;
    int m_lastOcrPage = -1;
    QString m_lastOcrSourcePath;
public slots:
    // Persist the accepted OCR results as a searchable MRC PDF/A copy.
    void onOcrAcceptRequested();
private:

    // P4: cache the initialized OCR engine pair across runs. Constructing a fresh
    // OcrEngine/RapidOcrEngine per call rebuilt 3 ONNX sessions (and the Tesseract
    // API) from disk every time, defeating each engine's own init guard. We reuse
    // one Tesseract + one RapidOCR instance, reinitializing only when the language
    // changes. Access is serialized by _ocrRunning (one OCR run at a time), so no
    // additional locking is required.
    std::shared_ptr<IOcrEngine> _ocrTesseract;   // primary (Tesseract 5)
    std::shared_ptr<IOcrEngine> _ocrRapid;        // RapidOCR / PP-OCRv5
    QString _ocrTesseractLang;                    // language the cached Tesseract was init'd with
    QString _ocrRapidLang;                        // language the cached RapidOCR was init'd with

    QString _selectedImageName;
    int _imageEditPage = -1;
    EditToolBar* _textToolBar = nullptr;

    // Text formatting state
    QString _fontFamily = "Helvetica";
    int _fontSize = 12;
    QColor _fontColor = Qt::black;
    bool _fontBold = false;
    bool _fontItalic = false;
    int _fontAlignment = 0;

    // Search state for match navigation
    int _currentMatchIndex = -1;
    int _totalMatches = 0;
};

} // namespace gp
