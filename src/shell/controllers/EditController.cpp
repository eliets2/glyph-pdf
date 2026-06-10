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
#include "ui/RedactCommand.h"
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
#include "shell/StatusBar.h"

namespace gp {

EditController::EditController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> EditController::handledTools() const {
    return {
        ToolId::Search, ToolId::Ocr,
        ToolId::EditText, ToolId::Hand, ToolId::Select,
        ToolId::SelectObject, ToolId::EditObject,
        ToolId::Highlight, ToolId::Underline, ToolId::Strikeout, ToolId::Squiggly,
        ToolId::Pencil, ToolId::Freehand,
        ToolId::TextBox, ToolId::AddText,
        ToolId::Note, ToolId::Comment,
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
    case ToolId::Image:
    case ToolId::EditImage:
        enterImageEditMode();
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
    if (viewer && _ctx && _ctx->document && _ctx->undoStack) {
        _ctx->document->setPath(viewer->filePath());
        _ctx->undoStack->push(new RedactCommand(viewer, _ctx->document.get(), text, matchCase, wholeWords));
        _mainWindow->statusBar()->showMessage(tr("Applied redactions to all search results for '%1'").arg(text), 5000);
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

    QThread *worker = QThread::create([self, viewerPtr, filePath, page,
                                       wantRapid, wantEnsemble]() {
        QString error;
        QList<OcrResult> resultsArr;

        QPdfDocument document;
        const auto loadError = document.load(filePath);
        if (loadError != QPdfDocument::Error::None) {
            error = QStringLiteral("OCR failed: could not load document.");
        } else if (page < 0 || page >= document.pageCount()) {
            error = QStringLiteral("OCR failed: page is no longer available.");
        } else {
            const QSizeF pageSize = document.pagePointSize(page);
            const QSize imageSize(pageSize.width() * 2.0, pageSize.height() * 2.0);
            QImage pageImg = document.render(page, imageSize, QPdfDocumentRenderOptions());

            if (pageImg.isNull()) {
                error = QStringLiteral("OCR failed: could not render page.");
            } else {
                // Build primary + optional secondary engine from preference.
                std::shared_ptr<IOcrEngine> primary = std::make_shared<OcrEngine>();
                std::shared_ptr<IOcrEngine> secondary;

                if (wantRapid || wantEnsemble) {
                    auto rapid = std::make_shared<RapidOcrEngine>();
                    if (!rapid->initialize(QStringLiteral("eng"))) {
                        // initialize() already logged the reason; surface it to the user.
                        error = QStringLiteral(
                            "OCR failed: RapidOCR engine could not be initialised. "
                            "Check that the PP-OCRv5 ONNX models are installed correctly.");
                    } else if (wantRapid) {
                        // RapidOCR-only: use as primary, no secondary
                        secondary = nullptr;
                        primary   = rapid;  // replace primary
                    } else {
                        // Ensemble: Tesseract primary, RapidOCR secondary (ROVER merge)
                        secondary = rapid;
                    }
                }

                if (error.isEmpty()) {
                    // Initialise Tesseract primary (unless replaced by rapid above)
                    if (!wantRapid) {
                        if (!primary->initialize(QStringLiteral("eng"))) {
                            error = QStringLiteral("OCR failed: Tesseract English language data is unavailable.");
                        }
                    }
                }

                if (error.isEmpty()) {
                    const OcrStrategy strategy = wantEnsemble
                        ? OcrStrategy::RoverVote
                        : OcrStrategy::PrimaryOnly;
                    OcrPipeline pipeline(primary, secondary);
                    pipeline.setStrategy(strategy);
                    const auto mergedWords = pipeline.run(pageImg);

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

        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, viewerPtr, filePath, page, resultsArr, error]() {
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
            connect(viewer, &PdfViewerWidget::textEditRequested, this, &EditController::onTextEditRequested, Qt::UniqueConnection);
        }
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
                                                        _fontFamily, _fontSize, _fontColor, _fontBold, _fontItalic, _fontAlignment));
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

} // namespace gp
