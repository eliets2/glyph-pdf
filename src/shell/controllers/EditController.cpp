// SPDX-License-Identifier: Apache-2.0
#include "EditController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "engines/OcrEngine.h"
#include "engines/ocr/RapidOcrEngine.h"
#include "engines/ocr/OcrPipeline.h"
#include "core/OcrTypes.h"
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
#include "ui/SignaturePicker.h" // §9.7 P0: Draw/Type/Upload signature picker

#include <QFileDialog>
#include <QInputDialog>
#include <QMenu>
#include <QCursor>
#include <QApplication>
#include <QClipboard>
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
        ToolId::Stamp, ToolId::Callout, ToolId::Erase,
        ToolId::MarkRedact, ToolId::Signature,
        ToolId::Rectangle, ToolId::Oval,
        ToolId::Line, ToolId::Arrow,
        ToolId::Image, ToolId::EditImage,
        ToolId::Cut, ToolId::Copy, ToolId::DeleteSelection
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
    case ToolId::Image:
    case ToolId::EditImage:
        enterImageEditMode();
        break;
    case ToolId::Erase:
        // §9.2 P0: real erase via the deleteObjectAt pipeline.
        viewer->setToolMode(ToolMode::Erase);
        connect(viewer->annotationLayer(), &AnnotationLayer::eraseRequested,
                this, &EditController::onEraseRequested, Qt::UniqueConnection);
        _mainWindow->statusBar()->showMessage(
            tr("Eraser active — click an object to delete it."), 4000);
        break;
    case ToolId::Copy:
        copySelectionToClipboard();
        break;
    case ToolId::Cut:
        if (copySelectionToClipboard())
            viewer->deleteSelectedAnnotation();
        break;
    case ToolId::DeleteSelection:
        viewer->deleteSelectedAnnotation();
        _mainWindow->statusBar()->showMessage(tr("Deleted selected object."), 3000);
        break;
    case ToolId::Signature: {
        // §9.7 P0: a Draw/Type/Upload picker replaces the silent draw-only
        // default. Draw keeps the existing freehand flow; Type/Upload render
        // or decode the signature image here, then arm the matching placement
        // mode — cancel leaves everything untouched.
        SignaturePickerDialog picker(_mainWindow);
        if (picker.exec() != QDialog::Accepted)
            break;
        switch (picker.acceptedKind()) {
        case SignatureContent::Kind::Draw:
            viewer->setToolMode(ToolMode::AddSignature);
            _mainWindow->statusBar()->showMessage(
                tr("Signature: draw on the page with the mouse."), 5000);
            break;
        case SignatureContent::Kind::Typed:
        case SignatureContent::Kind::Upload: {
            const bool typed = picker.acceptedKind() == SignatureContent::Kind::Typed;
            // Order matters: arm the placement mode FIRST, then set the image
            // (AnnotationLayer::setMode discards a pending image for any
            // non-signature tool).
            viewer->setToolMode(typed ? ToolMode::AddSignatureTyped
                                      : ToolMode::AddSignatureUpload);
            viewer->setPendingSignatureImage(picker.acceptedImage());
            _mainWindow->statusBar()->showMessage(
                tr("Signature ready — click or drag on the page to place it."), 5000);
            break;
        }
        }
        break;
    }
    default:
        if (toolModes.contains(id)) {
            viewer->setToolMode(toolModes.value(id));
            _mainWindow->statusBar()->showMessage(tr("Tool active: %1").arg(toolIdToString(id)), 2500);
        }
        break;
    }
}

// ── Search ──────────────────────────────────────────────────────────────────

// §9.15: single source of truth for the page-text matcher used by both the
// document-text search path and the redact-all path. Previously the two
// callers built their own QRegularExpression and had already drifted (the
// redact path ignored useRegex entirely).
EditController::PageTextPattern
EditController::pageTextPattern(const QString &text, bool matchCase,
                                bool wholeWords, bool useRegex) {
    PageTextPattern out;
    out.active = useRegex || wholeWords || matchCase;
    if (!out.active) return out;

    QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
    if (!matchCase) opts |= QRegularExpression::CaseInsensitiveOption;

    if (useRegex) {
        // Whole-words wraps the user's pattern in \b guards; the raw pattern is
        // used as-is otherwise. Invalid patterns are surfaced by the caller.
        out.rx.setPattern(wholeWords ? QStringLiteral("\\b(?:%1)\\b").arg(text) : text);
    } else {
        QString pattern = QRegularExpression::escape(text);
        if (wholeWords) pattern = QStringLiteral("\\b%1\\b").arg(pattern);
        out.rx.setPattern(pattern);
    }
    out.rx.setPatternOptions(opts);
    return out;
}

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

        // §9.15 P0: Match Case / Whole Words / Regex were silently ignored by
        // the document path. QPdfSearchModel only does case-insensitive
        // substring search, so when any option is set, scan page text and
        // navigate to matching pages, with an honest status note.
        if (useRegex || wholeWords || matchCase) {
            const PageTextPattern pt = pageTextPattern(text, matchCase, wholeWords, useRegex);
            if (!pt.rx.isValid()) {
                _mainWindow->statusBar()->showMessage(tr("Invalid regular expression."), 4000);
                return;
            }
            const QRegularExpression &rx = pt.rx;
            int firstPage = -1;
            int hitPages = 0;
            const int pages = viewer->pageCount();
            for (int p = 0; p < pages; ++p) {
                const QString pageText = viewer->document()->getAllText(p).text();
                if (rx.match(pageText).hasMatch()) {
                    ++hitPages;
                    if (firstPage < 0) firstPage = p;
                }
            }
            if (firstPage >= 0)
                viewer->goToPage(firstPage);
            _mainWindow->statusBar()->showMessage(
                tr("%1 page(s) match. Page-level navigation for Match Case / Whole Words / Regex; inline highlight unavailable.")
                    .arg(hitPages),
                5000);
            return;
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
        // §9.15: reuse the shared page-text matcher. wholeWords-only is the
        // historical behavior for this path (FindBar never sends useRegex here).
        const PageTextPattern pt = pageTextPattern(text, matchCase, wholeWords, /*useRegex*/ false);
        if (!pt.rx.isValid()) return;
        if (_ctx->pdfEditor->applyPatternRedactions(pt.rx, QList<int>())) {
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

    // Audit 9.4 P0: honor the user's OCR language selection instead of a
    // hard-coded "eng". Read + map on the GUI thread (QSettings is not
    // thread-safe); the worker only uses the resolved engine code.
    const QString ocrLang = ocrEngineLanguageCode(QSettings().value(
        QStringLiteral("ocr/language"), QStringLiteral("EN")).toString());
    // §9.4 P0: Auto-Rotate (page-level orientation detection) preference —
    // read on the GUI thread like ocr/language (QSettings is not thread-safe);
    // the worker only uses the copied value.
    const bool orientDetect = QSettings().value(
        QStringLiteral("ocr/orientDetect"), false).toBool();
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
                                       wantRapid, wantEnsemble, ocrLang, orientDetect]() {
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
                const QString lang = ocrLang;
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
                    // §9.4 P0: honor Auto-Rotate for scans whose rotation is
                    // baked into the content; word boxes still map back to the
                    // original page via PreprocessedImage::inverseTransform.
                    // The other options keep their defaults (current behavior).
                    OcrPreprocessOptions preprocessOpts;
                    preprocessOpts.orientDetect = orientDetect;
                    pipeline.setPreprocessing(preprocessOpts);
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

        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, viewerPtr, filePath, page, pageImg, resultsArr, mergedWords, error]() {
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
            // §9.4 P0: cache this run so Accept can persist a searchable copy.
            self->m_lastOcrPageImage = pageImg;
            self->m_lastOcrWords = mergedWords;
            self->m_lastOcrPage = page;
            self->m_lastOcrSourcePath = filePath;
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
        tr("Selected: %1 (%2x%3 at %4,%5) — use the floating menu to edit.")
            .arg(name)
            .arg(placement.width(), 0, 'f', 1)
            .arg(placement.height(), 0, 'f', 1)
            .arg(placement.x(), 0, 'f', 1)
            .arg(placement.y(), 0, 'f', 1),
        5000);

    // §9.2 P0: surface the fully-built but previously unreachable
    // Rotate/Replace/Delete image backends as an immediate action menu on
    // selection (right-click equivalent at the current cursor position).
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor || _imageEditPage < 0) return;

    QMenu menu(viewer);
    QAction* rotCw  = menu.addAction(tr("Rotate 90° Clockwise"));
    QAction* rotCcw = menu.addAction(tr("Rotate 90° Counter-Clockwise"));
    menu.addSeparator();
    QAction* replaceAct = menu.addAction(tr("Replace…"));
    QAction* deleteAct  = menu.addAction(tr("Delete"));

    QAction* chosen = menu.exec(QCursor::pos());
    if (!chosen) return; // selection alone is fine — no-op, honestly

    if (chosen == rotCw || chosen == rotCcw) {
        const double degrees = (chosen == rotCw) ? 90.0 : -90.0;
        _ctx->document->setPath(viewer->filePath());
        _ctx->undoStack->push(new RotateImageCommand(
            _ctx->pdfEditor.get(), _ctx->document.get(), _imageEditPage, name, degrees));
    } else if (chosen == replaceAct) {
        const QString newPath = QFileDialog::getOpenFileName(
            _mainWindow, tr("Replacement Image"), QString(),
            tr("Images (*.png *.jpg *.jpeg *.bmp)"));
        if (newPath.isEmpty()) return;
        const QByteArray backup = _ctx->pdfEditor->extractPageAsBytes(viewer->filePath(), _imageEditPage);
        _ctx->document->setPath(viewer->filePath());
        _ctx->undoStack->push(new ReplaceImageCommand(
            _ctx->pdfEditor.get(), _ctx->document.get(), _imageEditPage, name, newPath, backup));
    } else if (chosen == deleteAct) {
        const auto reply = QMessageBox::question(
            _mainWindow, tr("Delete Image"),
            tr("Delete image %1 from page %2?").arg(name).arg(_imageEditPage + 1));
        if (reply != QMessageBox::Yes) return;
        const QByteArray backup = _ctx->pdfEditor->extractPageAsBytes(viewer->filePath(), _imageEditPage);
        _ctx->document->setPath(viewer->filePath());
        _ctx->undoStack->push(new DeleteImageCommand(
            _ctx->pdfEditor.get(), _ctx->document.get(), _imageEditPage, name, backup));
    }
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

// §9.2 P0: minimal clipboard support — Copy places a raster snapshot of the
// selected object's region on the system clipboard (no in-document paste yet;
// Paste stays honestly disabled).
bool EditController::copySelectionToClipboard() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) return false;

    const int sel = viewer->annotationLayer()->selectedIndex();
    const auto annos = viewer->annotations();
    if (sel < 0 || sel >= annos.size()) {
        _mainWindow->statusBar()->showMessage(tr("Nothing selected to copy."), 3000);
        return false;
    }
    const AnnotationItem& a = annos[sel];

    // Render the page at 2x and crop to the annotation rect.
    const QImage page = viewer->renderPage(a.pageIndex, 2.0);
    if (page.isNull()) return false;
    const qreal s = 2.0;
    QRectF px(a.rect.x() * s, a.rect.y() * s,
              qMax<qreal>(1, a.rect.width() * s), qMax<qreal>(1, a.rect.height() * s));
    const QImage crop = page.copy(px.toRect());
    QApplication::clipboard()->setImage(crop);
    _mainWindow->statusBar()->showMessage(tr("Copied snapshot of selection to clipboard."), 3000);
    return true;
}

// §9.2 P0: erase — delete the content object under the click.
void EditController::onEraseRequested(int pageIndex, QPointF pos) {
    if (!_ctx || !_ctx->pdfEditor || pageIndex < 0) return;
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) return;
    if (_ctx->pdfEditor->deleteObjectAt(pageIndex, pos)) {
        _mainWindow->statusBar()->showMessage(tr("Object erased."), 3000);
        viewer->reload();
    } else {
        _mainWindow->statusBar()->showMessage(tr("Nothing to erase at that point."), 3000);
    }
}

// §9.4 P0 test seam: assemble the per-page OCR payload for exportMrcPdfA.
PageOcrResult EditController::buildPageOcrResult(int pageIndex, const QList<MergedOcrWord>& words)
{
    PageOcrResult r;
    r.pageIndex = pageIndex;
    r.words = words;
    r.success = !words.isEmpty();
    return r;
}

// §9.4 honesty: the interactive Accept flow persists a ONE-PAGE MRC PDF/A
// (runOcr recognises the current page only). The save dialog and the success
// status must say so — a dialog titled "Save Searchable Copy" on a 40-page
// document reads as a whole-document searchable export, which it is not.
// Single-page documents need no scope note: the one-page copy IS the document.
QString EditController::ocrSaveDialogTitle(int totalPages, int pageIndex) {
    if (totalPages <= 1)
        return EditController::tr("Save Searchable (OCR) Copy");
    return EditController::tr("Save Searchable (OCR) Copy — Current Page Only (%1 of %2)")
        .arg(pageIndex + 1).arg(totalPages);
}

QString EditController::ocrSavedStatus(int totalPages, int pageIndex, const QString& fileName) {
    if (totalPages <= 1)
        return EditController::tr("Searchable copy saved: %1").arg(fileName);
    return EditController::tr("Searchable copy saved (current page %1 of %2 only): %3")
        .arg(pageIndex + 1).arg(totalPages).arg(fileName);
}

// §9.4 P0: Accept persists the recognised text as a searchable MRC PDF/A
// copy — the same production writer Batch Mode uses — instead of only
// showing a status message while the searchable layer silently vanished.
void EditController::onOcrAcceptRequested() {
    if (!_ctx || !_ctx->pdfEditor) return;
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) return;

    const QString currentPath = viewer->filePath();
    if (m_lastOcrPageImage.isNull() || m_lastOcrWords.isEmpty()
        || m_lastOcrSourcePath != currentPath) {
        _mainWindow->statusBar()->showMessage(
            tr("No fresh OCR results to save — run OCR first."), 5000);
        return;
    }

    const QFileInfo fi(currentPath);
    const int totalPages = viewer->pageCount();
    const int pageIndex = viewer->currentPage();
    const QString outPath = QFileDialog::getSaveFileName(
        _mainWindow, ocrSaveDialogTitle(totalPages, pageIndex),
        fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName()
            + QStringLiteral("_ocr.pdf"),
        tr("PDF Files (*.pdf)"));
    if (outPath.isEmpty()) return;

    const PageOcrResult pageResult = EditController::buildPageOcrResult(m_lastOcrPage, m_lastOcrWords);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = _ctx->pdfEditor->exportMrcPdfA(
        outPath, {m_lastOcrPageImage}, {pageResult});
    QApplication::restoreOverrideCursor();

    if (ok)
        _mainWindow->statusBar()->showMessage(
            ocrSavedStatus(totalPages, pageIndex, QFileInfo(outPath).fileName()), 8000);
    else
        QMessageBox::warning(_mainWindow, tr("OCR Export Failed"),
            tr("Could not write the searchable MRC PDF/A copy. See the application log."));
}

} // namespace gp
