// SPDX-License-Identifier: Apache-2.0
#include "EditController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "engines/OcrEngine.h"
#include "engines/ocr/RapidOcrEngine.h"
#include "engines/ocr/OcrPipeline.h"
#include "core/interfaces/IOcrEngine.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "ui/EditAnnotationCommand.h"
#include "commands/MoveImageCommand.h"
#include "commands/ResizeImageCommand.h"
#include "commands/RotateImageCommand.h"
#include "commands/ReplaceImageCommand.h"
#include "commands/DeleteImageCommand.h"
#include "commands/EditTextInlineCommand.h"
#include "ui/AnnotationLayer.h"
#include "ui/FindBar.h"
#include "ui/EditToolBar.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QThread>
#include <QPointer>
#include <QMetaObject>
#include <QCoreApplication>
#include <QFile>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
#include <QPdfSearchModel>
#include <QPdfBookmarkModel>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUndoStack>
#include <QGuiApplication>
#include <QClipboard>
#include "shell/StatusBar.h"

namespace {
// Wave 1A §9.4: OCRMode's language combo persists the 2-letter UI code shown in
// "EN · English" (see OCRMode.cpp's kOcrLanguageKey), but IOcrEngine::initialize()
// (both Tesseract and RapidOCR) expects Tesseract's 3-letter ISO 639-2/B codes
// ("eng", "deu", ...) per OcrEngine::allowedLanguages(). This mapping is the missing
// link that let EditController::runOcr() hardcode "eng" regardless of the user's
// selection. Unknown/unset codes fall back to "eng".
QString ocrUiLanguageToTesseractCode(const QString &uiCode) {
    static const QHash<QString, QString> map = {
        {"EN", "eng"},     {"DE", "deu"},     {"FR", "fra"},
        {"ES", "spa"},     {"IT", "ita"},     {"PT", "por"},
        {"RU", "rus"},     {"ZH", "chi_sim"}, {"JA", "jpn"},
        {"KO", "kor"},     {"AR", "ara"},     {"NL", "nld"},
    };
    return map.value(uiCode.trimmed().toUpper(), QStringLiteral("eng"));
}
}

namespace gp {

EditController::EditController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> EditController::handledTools() const {
    return {
        ToolId::Search, ToolId::Ocr,
        ToolId::EditText, ToolId::Hand, ToolId::Select,
        ToolId::SelectObject, ToolId::EditObject,
        ToolId::Cut, ToolId::Copy, ToolId::Delete,
        ToolId::Highlight, ToolId::Underline, ToolId::Strikeout, ToolId::Squiggly,
        ToolId::Pencil, ToolId::Freehand,
        ToolId::TextBox, ToolId::AddText,
        ToolId::Note, ToolId::Comment,
        ToolId::Stamp, ToolId::Callout, ToolId::Erase,
        ToolId::MarkRedact, ToolId::Signature,
        ToolId::Rectangle, ToolId::Oval,
        ToolId::Line, ToolId::Arrow,
        ToolId::Image, ToolId::EditImage
    };
}

void EditController::activate(ToolId id) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    static const QHash<ToolId, ToolMode> toolModes = {
        { ToolId::Hand,          ToolMode::HandTool },
        { ToolId::Select,        ToolMode::SelectText },
        { ToolId::SelectObject,  ToolMode::EditObject },
        { ToolId::EditObject,    ToolMode::EditObject },
        { ToolId::Highlight,     ToolMode::Highlight },
        { ToolId::Underline,     ToolMode::Underline },
        { ToolId::Squiggly,      ToolMode::Squiggly },
        { ToolId::Strikeout,     ToolMode::Strikeout },
        { ToolId::Pencil,        ToolMode::DrawFreehand },
        { ToolId::Freehand,      ToolMode::DrawFreehand },
        { ToolId::TextBox,       ToolMode::AddTextBox },
        { ToolId::AddText,       ToolMode::AddTextBox },
        { ToolId::Note,          ToolMode::AddComment },
        { ToolId::Comment,       ToolMode::AddComment },
        { ToolId::Stamp,         ToolMode::Stamp },
        { ToolId::Callout,       ToolMode::Callout },
        { ToolId::MarkRedact,    ToolMode::Redact },
        { ToolId::Signature,     ToolMode::AddSignature },
        { ToolId::Rectangle,     ToolMode::DrawRectangle },
        { ToolId::Oval,          ToolMode::DrawEllipse },
        { ToolId::Line,          ToolMode::DrawLine },
        { ToolId::Arrow,         ToolMode::DrawArrow },
    };

    switch (id) {
    case ToolId::Search:
        _mainWindow->toggleFindBar();
        break;
    case ToolId::Ocr:
        runOcr();
        break;
    case ToolId::EditText:
        editPdfText();
        break;
    case ToolId::Cut:
        cutSelectedObject();
        break;
    case ToolId::Copy:
        copySelectedObject();
        break;
    case ToolId::Delete:
        deleteSelectedObject();
        break;
    case ToolId::Image:
    case ToolId::EditImage:
        enterImageEditMode();
        break;
    case ToolId::Erase:
        // §9.2 Wave 1B: real erase. A click hit-tests page content at the
        // clicked point and excises it via the already-correct
        // PoDoFoBackend::deleteObjectAt (same pipeline applyRedactions uses).
        if (_ctx && _ctx->pdfEditor) {
            _ctx->pdfEditor->loadDocumentForEditing(viewer->filePath());
        }
        viewer->setToolMode(ToolMode::Erase);
        connect(viewer->annotationLayer(), &AnnotationLayer::eraseRequested,
                this, &EditController::onEraseRequested, Qt::UniqueConnection);
        _mainWindow->statusBar()->showMessage(
            tr("Eraser Mode. Click on page content to erase it."), 5000);
        break;
    default:
        if (toolModes.contains(id)) {
            viewer->setToolMode(toolModes.value(id));
            _mainWindow->statusBar()->showMessage(tr("Tool active: %1").arg(toolIdToString(id)), 2500);
        }
        break;
    }
}

// ── Search ──────────────────────────────────────────────────────────────────

void EditController::onSearchRequested(const QString &text, bool forward, bool matchCase,
                                       bool wholeWords, bool useRegex, int scope) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) return;

    // For document text scope, use QPdfSearchModel (fast PDFium-backed search)
    if (scope == FindBar::ScopeDocumentText || scope == FindBar::ScopeAll) {
        viewer->searchDocument(text, forward, matchCase, wholeWords);

        auto* sm = viewer->searchModel();
        if (sm) {
            _totalMatches = sm->rowCount(QModelIndex());
            if (_totalMatches > 0) {
                _currentMatchIndex = forward
                    ? qMin(_currentMatchIndex + 1, _totalMatches - 1)
                    : qMax(_currentMatchIndex - 1, 0);
                if (_currentMatchIndex < 0) _currentMatchIndex = 0;

                // Navigate to the match
                QModelIndex idx = sm->index(_currentMatchIndex, 0);
                int page = sm->data(idx, static_cast<int>(QPdfSearchModel::Role::Page)).toInt();
                viewer->goToPage(page);
            } else {
                _currentMatchIndex = -1;
            }
        }
    }

    // For comments scope, search annotation text
    if (scope == FindBar::ScopeComments || scope == FindBar::ScopeAll) {
        QRegularExpression rx;
        if (useRegex) {
            QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
            if (!matchCase) opts |= QRegularExpression::CaseInsensitiveOption;
            rx.setPattern(text);
            rx.setPatternOptions(opts);
        }

        const auto annots = viewer->annotations();
        for (const auto &a : annots) {
            bool found = false;
            if (useRegex && rx.isValid()) {
                found = rx.match(a.text).hasMatch();
            } else {
                auto cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
                found = a.text.contains(text, cs);
            }
            if (found) {
                viewer->goToPage(a.pageIndex);
                _mainWindow->statusBar()->showMessage(
                    tr("Found in comment on page %1").arg(a.pageIndex + 1), 3000);
                break;
            }
        }
    }

    // For bookmarks scope, search outline titles
    if (scope == FindBar::ScopeBookmarks || scope == FindBar::ScopeAll) {
        auto* bm = viewer->bookmarkModel();
        if (bm) {
            QRegularExpression rx;
            if (useRegex) {
                QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
                if (!matchCase) opts |= QRegularExpression::CaseInsensitiveOption;
                rx.setPattern(text);
                rx.setPatternOptions(opts);
            }

            std::function<bool(const QModelIndex&)> searchBookmarks = [&](const QModelIndex &parent) -> bool {
                int rows = bm->rowCount(parent);
                for (int i = 0; i < rows; ++i) {
                    QModelIndex idx = bm->index(i, 0, parent);
                    QString title = idx.data(Qt::DisplayRole).toString();
                    bool found = false;
                    if (useRegex && rx.isValid()) {
                        found = rx.match(title).hasMatch();
                    } else {
                        auto cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
                        found = title.contains(text, cs);
                    }
                    if (found) {
                        int page = idx.data(static_cast<int>(QPdfBookmarkModel::Role::Page)).toInt();
                        if (page >= 0) viewer->goToPage(page);
                        _mainWindow->statusBar()->showMessage(
                            tr("Found bookmark: %1 (page %2)").arg(title).arg(page + 1), 3000);
                        return true;
                    }
                    if (searchBookmarks(idx)) return true;
                }
                return false;
            };
            searchBookmarks(QModelIndex());
        }
    }

    // Update match counter in FindBar
    if (scope == FindBar::ScopeDocumentText) {
        auto* findBar = _mainWindow->findChild<FindBar*>("findBar");
        if (findBar) {
            findBar->setMatchCount(_currentMatchIndex + 1, _totalMatches);
        }
    }
}

void EditController::onReplaceRequested(const QString &searchText, const QString &replaceText,
                                        bool matchCase, bool wholeWords, bool useRegex) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;

    _ctx->pdfEditor->loadDocumentForEditing(viewer->filePath());

    // Replace current match using PoDoFo content stream text substitution
    auto* sm = viewer->searchModel();
    if (!sm || _currentMatchIndex < 0 || _currentMatchIndex >= sm->rowCount(QModelIndex()))
        return;

    QModelIndex idx = sm->index(_currentMatchIndex, 0);
    int page = sm->data(idx, static_cast<int>(QPdfSearchModel::Role::Page)).toInt();
    QPointF loc = sm->data(idx, static_cast<int>(QPdfSearchModel::Role::Location)).toPointF();

    QRectF rect(loc.x(), loc.y() - 15, 200, 20);
    if (_ctx->undoStack) {
        _ctx->document->setPath(viewer->filePath());
        _ctx->undoStack->push(new EditTextInlineCommand(
            _ctx->pdfEditor.get(), _ctx->document.get(), page, rect, replaceText,
            _fontFamily, _fontSize, _fontColor, _fontBold, _fontItalic, _fontAlignment));
    }

    _mainWindow->statusBar()->showMessage(
        tr("Replaced match %1 on page %2").arg(_currentMatchIndex + 1).arg(page + 1), 3000);

    // Re-search to update counts
    onSearchRequested(searchText, true, matchCase, wholeWords, useRegex, FindBar::ScopeDocumentText);
}

void EditController::onReplaceAllRequested(const QString &searchText, const QString &replaceText,
                                           bool matchCase, bool wholeWords, bool useRegex) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;

    _ctx->pdfEditor->loadDocumentForEditing(viewer->filePath());

    auto* sm = viewer->searchModel();
    if (!sm) return;

    int count = sm->rowCount(QModelIndex());
    if (count == 0) {
        _mainWindow->statusBar()->showMessage(tr("No matches to replace."), 3000);
        return;
    }

    // Iterate all matches from last to first (reverse order to preserve positions)
    for (int i = count - 1; i >= 0; --i) {
        QModelIndex idx = sm->index(i, 0);
        int page = sm->data(idx, static_cast<int>(QPdfSearchModel::Role::Page)).toInt();
        QPointF loc = sm->data(idx, static_cast<int>(QPdfSearchModel::Role::Location)).toPointF();

        QRectF rect(loc.x(), loc.y() - 15, 200, 20);
        _ctx->pdfEditor->editTextInline(page, rect, replaceText,
                                        _fontFamily, _fontSize, _fontColor,
                                        _fontBold, _fontItalic, _fontAlignment);
    }

    // R2-1 D2: route through incremental update when document is signed, so
    // existing /ByteRange signatures are not invalidated by a full rewrite.
    // D3 (R2-2): check the save return value — a silent discard here means
    // the user sees "Replaced N occurrences" while the file was never written.
    {
        const bool isSigned = _ctx->pdfEditor->hasPdfSignatures();
        const bool saveOk = isSigned
            ? _ctx->pdfEditor->writeUpdate(viewer->filePath())
            : _ctx->pdfEditor->saveDocument(viewer->filePath());
        if (!saveOk) {
            QMessageBox::critical(
                _mainWindow,
                tr("Save Failed"),
                tr("The replacements were applied in memory, but the file could not "
                   "be saved. Check that the disk is not full and the file is not "
                   "write-protected."));
            _mainWindow->statusBar()->showMessage(tr("Replace All: save failed."), 5000);
            return;
        }
    }

    if (_ctx->document) {
        _ctx->document->setPath(viewer->filePath());
        _ctx->document->markReload();
    }

    _mainWindow->statusBar()->showMessage(
        tr("Replaced %1 occurrences.").arg(count), 5000);

    // Reload to reflect changes
    viewer->loadDocument(viewer->filePath());
}

void EditController::onRedactAllRequested(const QString &text, bool matchCase, bool wholeWords) {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer && _ctx && _ctx->pdfEditor) {
        QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
        if (!matchCase) opts |= QRegularExpression::CaseInsensitiveOption;
        
        QString pattern = QRegularExpression::escape(text);
        if (wholeWords) {
            pattern = QStringLiteral("\\b") + pattern + QStringLiteral("\\b");
        }
        
        QRegularExpression rx(pattern, opts);
        if (_ctx->pdfEditor->applyPatternRedactions(rx, QList<int>())) {
            _mainWindow->statusBar()->showMessage(tr("Applied redactions to all search results for '%1'").arg(text), 5000);
            viewer->loadDocument(viewer->filePath());
        }
    }
}

// ── OCR ─────────────────────────────────────────────────────────────────────

void EditController::runOcr() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || _ocrRunning) return;

    const QString filePath = viewer->filePath();
    const int page = viewer->currentPage();
    if (filePath.isEmpty() || page < 0) return;

    // Read the pref at call-time so changes take effect without restart (D2 guardrail 3).
    // Default is "auto": prefer the ROVER ensemble when the PP-OCRv5 models are
    // installed, else degrade to Tesseract. (Legacy installs may still hold the old
    // "tesseract" default; that is honoured as an explicit single-engine choice.)
    QString engineKey = QSettings().value(QStringLiteral("ocr/engine"),
                                          QStringLiteral("auto")).toString();
    const bool autoSelect = (engineKey.isEmpty() || engineKey == QStringLiteral("auto"));

    // Resolve ONNX model availability once (needed both for auto-resolution and the
    // explicit-selection availability check below).
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                            + QStringLiteral("/models/ppocrv5");
    const QString nextToExe = QCoreApplication::applicationDirPath()
                              + QStringLiteral("/models/ppocrv5");
    const QString detModel = QStringLiteral("/PP-OCRv5_mobile_det_infer.onnx");
    const bool onnxAvailable =
#ifdef HAS_RAPIDOCR
        QFile::exists(appData + detModel) || QFile::exists(nextToExe + detModel);
#else
        false;
#endif

    if (autoSelect) {
        // ROVER-by-default: dual-engine ensemble when models are present, else Tesseract.
        engineKey = onnxAvailable ? QStringLiteral("ensemble") : QStringLiteral("tesseract");
    }

    const bool wantRapid    = (engineKey == QStringLiteral("rapidocr"));
    const bool wantEnsemble = (engineKey == QStringLiteral("ensemble"));

    // Honest availability check for an EXPLICIT RapidOCR/Ensemble selection: fail
    // loudly rather than silently downgrade (audit §7 Pattern 5). The auto path
    // already guaranteed availability above, so it is exempt.
    if (!autoSelect && (wantRapid || wantEnsemble) && !onnxAvailable) {
        _mainWindow->statusBar()->showMessage(
            tr("OCR failed: PP-OCRv5 ONNX models not found. "
               "Change the OCR engine in Preferences → Engines, or install the models."), 7000);
        return;
    }

    _ocrRunning = true;
    const QString engineLabel = wantEnsemble ? tr("Ensemble (Tesseract + RapidOCR)")
                              : wantRapid    ? tr("RapidOCR / PP-OCRv5")
                              :                tr("Tesseract 5");
    _mainWindow->statusBar()->showMessage(tr("Processing OCR (%1)...").arg(engineLabel));

    QPointer<EditController> self(this);
    QPointer<PdfViewerWidget> viewerPtr(viewer);

    // P12: render the page on the GUI thread using the viewer's already-loaded
    // QPdfDocument (and its render cache) instead of doing a second
    // QPdfDocument::load(filePath) + render inside the worker. QPdfDocument is not
    // thread-safe, so rendering must happen on its owning (GUI) thread anyway; the
    // worker then only runs the (heavy, parallelizable) OCR over the QImage.
    // renderPage(page, 2.0) reproduces the previous worker's scale exactly
    // (pageSize * 2.0).
    const QImage renderedPage = viewer->renderPage(page, 2.0);

    QThread *worker = QThread::create([self, viewerPtr, filePath, page, renderedPage,
                                       wantRapid, wantEnsemble]() {
        QString error;
        QList<OcrResult> resultsArr;
        QList<MergedOcrWord> mergedWords;   // also surfaced to the OCR Verify screen

        const QImage pageImg = renderedPage;
        {
            if (pageImg.isNull()) {
                error = QStringLiteral("OCR failed: could not render page.");
            } else if (!self) {
                error = QStringLiteral("OCR failed: editor was closed.");
            } else {
                // P4: reuse cached engine instances instead of rebuilding them
                // (and their ONNX/Tesseract sessions) on every OCR run. Engines are
                // (re)initialized only when first used or when the language changes.
                // Serialized by _ocrRunning, so accessing self's cached members from
                // this worker thread is race-free.
                //
                // Wave 1A §9.4: honour the OCRMode language combo (persisted under
                // "ocr/language" as a 2-letter UI code) instead of hardcoding English.
                const QString lang = ocrUiLanguageToTesseractCode(
                    QSettings().value(QStringLiteral("ocr/language"), QStringLiteral("EN")).toString());
                std::shared_ptr<IOcrEngine> primary;
                std::shared_ptr<IOcrEngine> secondary;

                if (wantRapid || wantEnsemble) {
                    if (!self->_ocrRapid) {
                        self->_ocrRapid = std::make_shared<RapidOcrEngine>();
                        self->_ocrRapidLang.clear();
                    }
                    // initialize() is a no-op past the first successful call for the
                    // same language thanks to RapidOcrEngine's own init guard.
                    if (!self->_ocrRapid->initialize(lang)) {
                        // initialize() already logged the reason; surface it to the user.
                        self->_ocrRapidLang.clear();
                        error = QStringLiteral(
                            "OCR failed: RapidOCR engine could not be initialised. "
                            "Check that the PP-OCRv5 ONNX models are installed correctly.");
                    } else {
                        self->_ocrRapidLang = lang;
                        if (wantRapid) {
                            // RapidOCR-only: use as primary, no secondary
                            primary   = self->_ocrRapid;
                            secondary = nullptr;
                        } else {
                            // Ensemble: Tesseract primary, RapidOCR secondary (ROVER merge)
                            secondary = self->_ocrRapid;
                        }
                    }
                }

                if (error.isEmpty() && !wantRapid) {
                    // Tesseract primary (cached). RapidOCR-only path skips this.
                    if (!self->_ocrTesseract) {
                        self->_ocrTesseract = std::make_shared<OcrEngine>();
                        self->_ocrTesseractLang.clear();
                    }
                    if (!self->_ocrTesseract->initialize(lang)) {
                        self->_ocrTesseractLang.clear();
                        // Wave 1A §9.4: name the actually-selected language instead of
                        // always blaming "English" regardless of what the user picked.
                        error = QStringLiteral("OCR failed: Tesseract language data for '%1' is unavailable.").arg(lang);
                    } else {
                        self->_ocrTesseractLang = lang;
                        primary = self->_ocrTesseract;
                    }
                }

                if (error.isEmpty()) {
                    const OcrStrategy strategy = wantEnsemble
                        ? OcrStrategy::RoverVote
                        : OcrStrategy::PrimaryOnly;
                    OcrPipeline pipeline(primary, secondary);
                    pipeline.setStrategy(strategy);
                    mergedWords = pipeline.run(pageImg);

                    // Convert MergedOcrWord → OcrResult for the viewer layer
                    resultsArr.reserve(mergedWords.size());
                    for (const auto& w : mergedWords) {
                        OcrResult r;
                        r.text        = w.text;
                        r.boundingBox = w.boundingBox;
                        r.confidence  = w.confidence;
                        resultsArr.append(r);
                    }
                }
            }
        }

        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, viewerPtr, filePath, page, resultsArr, mergedWords, error]() {
            if (!self) return;

            self->_ocrRunning = false;

            if (!error.isEmpty()) {
                self->_mainWindow->statusBar()->showMessage(error, 7000);
                return;
            }

            if (!viewerPtr || viewerPtr->filePath() != filePath || viewerPtr->currentPage() != page) {
                self->_mainWindow->statusBar()->showMessage(tr("OCR complete, but the page changed before results could be applied."), 5000);
                return;
            }

            viewerPtr->setOcrResults(resultsArr);
            viewerPtr->setToolMode(ToolMode::SelectText);
            // Feed the OCR Verify screen (if open) so it shows real recognised words
            // for review instead of an empty/decorative panel.
            emit self->ocrResultsReady(mergedWords);
            self->_mainWindow->statusBar()->showMessage(tr("OCR Complete. %1 text blocks detected.").arg(resultsArr.size()), 5000);
        }, Qt::QueuedConnection);
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

// ── Text editing ────────────────────────────────────────────────────────────

void EditController::editPdfText() {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer) {
        viewer->setToolMode(ToolMode::EditText);
        if (_ctx && _ctx->pdfEditor) {
            _ctx->pdfEditor->loadDocumentForEditing(viewer->filePath());
        }
        _mainWindow->statusBar()->showMessage(tr("Direct Text Editing Mode. Click a text block to modify its contents."), 5000);

        if (!_textToolBar) {
            _textToolBar = new EditToolBar(tr("Text Edit"), _mainWindow);
            _mainWindow->addToolBar(Qt::TopToolBarArea, _textToolBar);
            connect(_textToolBar, &EditToolBar::textFormatChanged, this, &EditController::onTextFormatChanged);
            connect(_textToolBar, &EditToolBar::opacityChanged, this, &EditController::onOpacityChanged, Qt::UniqueConnection);
            connect(viewer, &PdfViewerWidget::textEditRequested, this, &EditController::onTextEditRequested, Qt::UniqueConnection);
        }
        _textToolBar->setActiveMode(ToolMode::EditText);
        _textToolBar->show();
    }
}

void EditController::onTextFormatChanged(const QString &fontFamily, int fontSize, const QColor &color, bool bold, bool italic, int alignment) {
    _fontFamily = fontFamily;
    _fontSize = fontSize;
    _fontColor = color;
    _fontBold = bold;
    _fontItalic = italic;
    _fontAlignment = alignment;
}

void EditController::onTextEditRequested(int pageIndex, QPointF pos) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor || !_ctx->document) return;

    bool ok;
    QString newText = QInputDialog::getMultiLineText(_mainWindow, tr("Edit Text Inline"),
                                                     tr("Enter new text:"), "", &ok);
    if (ok && !newText.isEmpty()) {
        QRectF rect(pos.x(), pos.y(), 200, 50);
        _ctx->document->setPath(viewer->filePath());
        _ctx->undoStack->push(new EditTextInlineCommand(_ctx->pdfEditor.get(), _ctx->document.get(), pageIndex, rect, newText,
                                                        _fontFamily, _fontSize, _fontColor, _fontBold, _fontItalic, _fontAlignment,
                                                        _opacity));
    }
}

// §9.2 Wave 1B: real eraser. deleteObjectAt() excises whatever content sits
// under the click (via applyRedactions on a small hit-test rect) and writes
// the result straight to disk (writeUpdate), so this handler just needs to
// invoke it and refresh the view -- there is no separate save step and, like
// the pre-existing deleteObjectAt() itself, no undo command for this path.
void EditController::onEraseRequested(int pageIndex, QPointF pos) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;

    if (_ctx->pdfEditor->deleteObjectAt(pageIndex, pos)) {
        if (_ctx->document) _ctx->document->markReload();
        viewer->loadDocument(viewer->filePath());
        _mainWindow->statusBar()->showMessage(tr("Erased content at the clicked location."), 3000);
    } else {
        _mainWindow->statusBar()->showMessage(tr("Nothing to erase at that location."), 3000);
    }
}

// §9.2 Wave 2B item 3: opacity control shared by the text-edit and image-edit
// toolbars. In EditText mode this just records _opacity for the next inline
// edit/replace (applied via EditTextInlineCommand -> editTextInline). In
// EditImage mode with a selection, it applies immediately via
// setImageOpacity() -- there is no undo command for this path, matching the
// eraser's precedent of a minimal, non-undo-tracked direct engine call.
void EditController::onOpacityChanged(double opacity) {
    _opacity = opacity;

    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || viewer->toolMode() != ToolMode::EditImage) return;
    if (_selectedImageName.isEmpty() || _imageEditPage < 0 || !_ctx || !_ctx->pdfEditor) return;

    if (_ctx->pdfEditor->setImageOpacity(_imageEditPage, _selectedImageName, opacity)) {
        if (_ctx->document) _ctx->document->markReload();
        _mainWindow->statusBar()->showMessage(
            tr("Image opacity set to %1%.").arg(qRound(opacity * 100)), 3000);
    } else {
        _mainWindow->statusBar()->showMessage(tr("Could not set image opacity."), 3000);
    }
}

// ── Image editing ───────────────────────────────────────────────────────────

void EditController::enterImageEditMode() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;

    _ctx->pdfEditor->loadDocumentForEditing(viewer->filePath());
    viewer->setToolMode(ToolMode::EditImage);

    int page = viewer->currentPage();
    _imageEditPage = page;
    auto images = _ctx->pdfEditor->listImages(page);

    viewer->annotationLayer()->setImageOverlays(images);

    connect(viewer->annotationLayer(), &AnnotationLayer::imageSelected,
            this, &EditController::onImageSelected, Qt::UniqueConnection);
    connect(viewer->annotationLayer(), &AnnotationLayer::imageMoved,
            this, &EditController::onImageMoved, Qt::UniqueConnection);
    connect(viewer->annotationLayer(), &AnnotationLayer::imageResized,
            this, &EditController::onImageResized, Qt::UniqueConnection);
    connect(viewer->annotationLayer(), &AnnotationLayer::imageRotateRequested,
            this, &EditController::onImageRotateRequested, Qt::UniqueConnection);
    connect(viewer->annotationLayer(), &AnnotationLayer::imageDeleteRequested,
            this, &EditController::onImageDeleteRequested, Qt::UniqueConnection);
    connect(viewer->annotationLayer(), &AnnotationLayer::imageReplaceRequested,
            this, &EditController::onImageReplaceRequested, Qt::UniqueConnection);

    // §9.2 Wave 2B item 3: same EditToolBar instance as editPdfText(), just
    // showing the opacity control instead of the text-format group.
    if (!_textToolBar) {
        _textToolBar = new EditToolBar(tr("Text Edit"), _mainWindow);
        _mainWindow->addToolBar(Qt::TopToolBarArea, _textToolBar);
        connect(_textToolBar, &EditToolBar::textFormatChanged, this, &EditController::onTextFormatChanged);
        connect(_textToolBar, &EditToolBar::opacityChanged, this, &EditController::onOpacityChanged, Qt::UniqueConnection);
    }
    _textToolBar->setActiveMode(ToolMode::EditImage);
    _textToolBar->show();

    _mainWindow->statusBar()->showMessage(
        tr("Image Edit Mode. %1 images found. Click to select.").arg(images.size()), 5000);
}

void EditController::onImageSelected(const QString &name, const QRectF &placement) {
    _selectedImageName = name;
    _mainWindow->statusBar()->showMessage(
        tr("Selected: %1 (%2x%3 at %4,%5)")
            .arg(name)
            .arg(placement.width(), 0, 'f', 1)
            .arg(placement.height(), 0, 'f', 1)
            .arg(placement.x(), 0, 'f', 1)
            .arg(placement.y(), 0, 'f', 1),
        5000);
}

void EditController::onImageMoved(const QString &name, double dx, double dy) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor || _imageEditPage < 0) return;
    _ctx->document->setPath(viewer->filePath());
    _ctx->undoStack->push(new MoveImageCommand(
        _ctx->pdfEditor.get(), _ctx->document.get(), _imageEditPage, name, dx, dy));
}

void EditController::onImageResized(const QString &name, double newW, double newH) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor || _imageEditPage < 0) return;
    auto images = _ctx->pdfEditor->listImages(_imageEditPage);
    double oldW = newW, oldH = newH;
    for (const auto& img : images) {
        if (img.xobjectName == name) {
            oldW = img.placement.width();
            oldH = img.placement.height();
            break;
        }
    }
    _ctx->document->setPath(viewer->filePath());
    _ctx->undoStack->push(new ResizeImageCommand(
        _ctx->pdfEditor.get(), _ctx->document.get(), _imageEditPage, name, oldW, oldH, newW, newH));
}

// Wave 1A §9.2: drag-rotate handle + right-click "Rotate" submenu wiring.
// RotateImageCommand itself was already correct; it just had no UI caller.
void EditController::onImageRotateRequested(const QString &name, double degrees) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor || _imageEditPage < 0) return;
    _ctx->document->setPath(viewer->filePath());
    _ctx->undoStack->push(new RotateImageCommand(
        _ctx->pdfEditor.get(), _ctx->document.get(), _imageEditPage, name, degrees));
    // Rotation changes the image's on-page placement (via CTM), so refresh the
    // overlay list from the engine rather than trying to compute the new rect here.
    auto images = _ctx->pdfEditor->listImages(_imageEditPage);
    viewer->annotationLayer()->setImageOverlays(images);
    viewer->annotationLayer()->setSelectedImageName(name);
    _mainWindow->statusBar()->showMessage(tr("Rotated image %1 by %2°").arg(name).arg(degrees), 3000);
}

// Wave 1A §9.2: right-click "Delete Image" wiring. DeleteImageCommand needs a
// pre-op page snapshot (like EditTextInlineCommand/DeletePageCommand) so undo can
// restore the deleted image; capture it here before invoking the command.
void EditController::onImageDeleteRequested(const QString &name) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor || _imageEditPage < 0) return;

    auto reply = QMessageBox::question(_mainWindow, tr("Delete Image"),
        tr("Delete image %1 from this page?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    _ctx->document->setPath(viewer->filePath());
    const QByteArray backup = _ctx->pdfEditor->extractPageAsBytes(viewer->filePath(), _imageEditPage);
    _ctx->undoStack->push(new DeleteImageCommand(
        _ctx->pdfEditor.get(), _ctx->document.get(), _imageEditPage, name, backup));

    auto images = _ctx->pdfEditor->listImages(_imageEditPage);
    viewer->annotationLayer()->setImageOverlays(images);
    viewer->annotationLayer()->setSelectedImageName(QString());
    _mainWindow->statusBar()->showMessage(tr("Deleted image %1").arg(name), 3000);
}

// Wave 1A §9.2: right-click "Replace Image…" wiring. Prompts for a new image file,
// same pre-op backup pattern as delete.
void EditController::onImageReplaceRequested(const QString &name) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor || _imageEditPage < 0) return;

    const QString newPath = QFileDialog::getOpenFileName(_mainWindow, tr("Replace Image"),
        QString(), tr("Images (*.png *.jpg *.jpeg *.bmp *.tiff)"));
    if (newPath.isEmpty()) return;

    _ctx->document->setPath(viewer->filePath());
    const QByteArray backup = _ctx->pdfEditor->extractPageAsBytes(viewer->filePath(), _imageEditPage);
    _ctx->undoStack->push(new ReplaceImageCommand(
        _ctx->pdfEditor.get(), _ctx->document.get(), _imageEditPage, name, newPath, backup));

    auto images = _ctx->pdfEditor->listImages(_imageEditPage);
    viewer->annotationLayer()->setImageOverlays(images);
    viewer->annotationLayer()->setSelectedImageName(name);
    _mainWindow->statusBar()->showMessage(tr("Replaced image %1").arg(name), 3000);
}

// ── Cut / Copy / Delete (§9.2 Wave 1B) ─────────────────────────────────────
//
// "The object EditController already selects" is one of two things depending
// on the active tool: an image selected in EditImage mode (_selectedImageName,
// placement looked up via listImages()) or an annotation/shape selected in
// EditObject mode (AnnotationLayer::selectedIndex()). Both selection rects
// share the same coordinate convention already used elsewhere in this file
// (onImageMoved/onImageResized forward image placement deltas directly to the
// PoDoFo engine with no separate widget->page transform), so renderPage()'s
// fixed 2.0 scale factor (the same one runOcr() uses) is used to rasterize the
// selection rect into clipboard pixels.

void EditController::copySelectedObject() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) return;

    QRectF rect;
    int page = -1;

    // Prefer an image selection (EditImage mode) if one is active.
    if (!_selectedImageName.isEmpty() && _imageEditPage >= 0 && _ctx && _ctx->pdfEditor) {
        const auto images = _ctx->pdfEditor->listImages(_imageEditPage);
        for (const auto& img : images) {
            if (img.xobjectName == _selectedImageName) {
                rect = img.placement;
                page = _imageEditPage;
                break;
            }
        }
    }

    // Otherwise fall back to the EditObject-mode annotation selection.
    if (page < 0) {
        auto* layer = viewer->annotationLayer();
        const int idx = layer ? layer->selectedIndex() : -1;
        if (idx >= 0) {
            const auto annos = layer->annotations();
            if (idx < annos.size()) {
                rect = annos.at(idx).rect;
                page = annos.at(idx).pageIndex >= 0 ? annos.at(idx).pageIndex : viewer->currentPage();
            }
        }
    }

    if (page < 0 || rect.isEmpty()) {
        _mainWindow->statusBar()->showMessage(tr("Nothing selected to copy."), 3000);
        return;
    }

    const QImage full = viewer->renderPage(page, 2.0);
    const QRect pixelRect(qRound(rect.x() * 2.0), qRound(rect.y() * 2.0),
                          qRound(rect.width() * 2.0), qRound(rect.height() * 2.0));
    const QImage snapshot = full.copy(pixelRect.intersected(full.rect()));
    if (snapshot.isNull() || full.isNull()) {
        _mainWindow->statusBar()->showMessage(tr("Could not create a snapshot of the selection."), 3000);
        return;
    }

    QGuiApplication::clipboard()->setImage(snapshot);
    _mainWindow->statusBar()->showMessage(tr("Copied selection as an image to the clipboard."), 3000);
}

void EditController::deleteSelectedObject() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) return;

    // Image selection: reuse the existing (Wave 1A) confirm + DeleteImageCommand
    // flow, identical to the right-click "Delete Image" menu entry.
    if (!_selectedImageName.isEmpty() && _imageEditPage >= 0) {
        onImageDeleteRequested(_selectedImageName);
        return;
    }

    auto* layer = viewer->annotationLayer();
    const int idx = layer ? layer->selectedIndex() : -1;
    if (idx < 0) {
        _mainWindow->statusBar()->showMessage(tr("Nothing selected to delete."), 3000);
        return;
    }

    viewer->deleteSelectedAnnotation();
    _mainWindow->statusBar()->showMessage(tr("Deleted selected object."), 3000);
}

void EditController::cutSelectedObject() {
    // Minimal Cut = Copy (raster snapshot) then Delete. No in-document paste.
    copySelectedObject();
    deleteSelectedObject();
}

} // namespace gp
