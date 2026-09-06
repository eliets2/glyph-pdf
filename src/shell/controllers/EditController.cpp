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
        case SignatureContent::Kind::Initials:
        case SignatureContent::Kind::Upload: {
            // §9.7 P1: Initials shares the TYPED placement path — the variant
            // is only a different render of the same image-stamp annotation
            // (no new ToolMode; PdfEnums.h ordinals are frozen).
            const bool typed = picker.acceptedKind() != SignatureContent::Kind::Upload;
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

// R07 (F11): pre-dispatch validation as a pure seam so the "no document" exit
// is testable without the application shell. Empty string == dispatchable.
QString EditController::ocrDispatchBlocker(const QString& filePath, int page)
{
    if (filePath.isEmpty() || page < 0)
        return EditController::tr("OCR needs an open document — open a PDF and run OCR again.");
    return QString();
}

// R07 (F11): one classification for every terminal outcome of a dispatched job.
// Order matters: a superseded generation wins (the newer job owns the user's
// attention), then the worker error, then source-identity staleness.
EditController::OcrJobVerdict EditController::classifyOcrJobCompletion(
    qint64 jobGeneration, qint64 currentGeneration,
    const QString& jobSourcePath, int jobPage,
    const QString& currentSourcePath, int currentPage,
    const QString& workerError, QString* messageOut)
{
    const auto setMessage = [messageOut](const QString& m) {
        if (messageOut) *messageOut = m;
    };

    // 1) A newer request supersedes this job's results entirely.
    if (jobGeneration != currentGeneration) {
        setMessage(EditController::tr("OCR run superseded by a newer request — discarding its results."));
        return OcrJobVerdict::Stale;
    }
    // 2) Worker failure (missing language/model data, render failure, engine error).
    if (!workerError.isEmpty()) {
        setMessage(workerError);
        return OcrJobVerdict::Failed;
    }
    // 3) The viewer/editor disappeared or the source identity changed.
    if (currentSourcePath.isEmpty() || currentPage < 0) {
        setMessage(EditController::tr("OCR finished, but the editor was closed — results discarded."));
        return OcrJobVerdict::Stale;
    }
    if (currentSourcePath != jobSourcePath) {
        setMessage(EditController::tr("OCR finished, but the document changed — results discarded."));
        return OcrJobVerdict::Stale;
    }
    if (currentPage != jobPage) {
        setMessage(EditController::tr("OCR finished, but the page changed — results discarded."));
        return OcrJobVerdict::Stale;
    }
    return OcrJobVerdict::Deliver;
}

// ── R08 (F04) review-session seams ──────────────────────────────────────────

// May this review session still be saved against the live viewer? A stale
// session (source changed, or the document revision changed) is rejected with
// a reason instead of exporting the wrong page/document.
bool EditController::ocrSessionIsExportable(const OcrReviewSession& session,
                                            const QString& currentSourcePath,
                                            int currentPageCount,
                                            QString* reasonOut)
{
    const auto reject = [reasonOut](const QString& r) {
        if (reasonOut) *reasonOut = r;
        return false;
    };

    if (!session.isValid())
        return reject(EditController::tr("No OCR results to save — run OCR first."));
    if (session.sourcePath != currentSourcePath)
        return reject(EditController::tr(
            "The reviewed page belongs to another document — run OCR on the current document before saving."));
    if (currentPageCount >= 0 && session.sourcePageCount != currentPageCount)
        return reject(EditController::tr(
            "The document changed since this OCR run (page count differs) — run OCR again."));
    return true;
}

// Merge the panel's reviewed records into the session and build the export
// payload. Reviewed words are authoritative; deleted/empty words are dropped;
// the source box of every kept word is the ORIGINAL recognized box.
PageOcrResult EditController::buildReviewedPageOcrResult(
    const OcrReviewSession& session,
    const QList<OcrReviewedWord>& reviewedWords,
    QString* errorOut)
{
    PageOcrResult r;
    r.pageIndex = session.sourcePage;
    const auto fail = [errorOut](const QString& e) {
        if (errorOut) *errorOut = e;
        return PageOcrResult{};
    };

    if (!session.isValid())
        return fail(EditController::tr("No OCR results to save — run OCR first."));

    // Stale-interaction guard: the panel's records must describe the same
    // delivery as the session. An empty list means "unedited review".
    QList<OcrReviewedWord> effective = reviewedWords;
    if (!effective.isEmpty()) {
        if (effective.size() != session.words.size())
            return fail(EditController::tr(
                "The review no longer matches the recognized page — run OCR again."));
        for (int i = 0; i < effective.size(); ++i) {
            if (effective[i].stableId != session.words[i].stableId)
                return fail(EditController::tr(
                    "The review no longer matches the recognized page — run OCR again."));
        }
    } else {
        effective = session.words;
    }

    r.words.reserve(effective.size());
    for (const auto& rec : effective) {
        if (rec.deleted || rec.reviewedText.trimmed().isEmpty()) continue;
        MergedOcrWord w;
        // Reviewed text wins; the box is the ORIGINAL source box (review
        // edits never move, split or invent coordinates).
        w.text         = rec.reviewedText;
        w.boundingBox  = rec.boundingBox;
        w.confidence   = rec.confidence;
        w.sourceEngine = rec.sourceEngine;
        r.words.append(w);
    }
    r.success = !r.words.isEmpty();
    return r;
}

void EditController::runOcr() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || _ocrRunning) return;

    const QString filePath = viewer->filePath();
    const int page = viewer->currentPage();

    // R07 (F11): a blocked dispatch must still complete the panel's lifecycle —
    // emit ocrRunFailed so the review screen returns to a retryable state
    // (previously these early returns left Run disabled forever).
    const QString blocker = ocrDispatchBlocker(filePath, page);
    if (!blocker.isEmpty()) { emit ocrRunFailed(blocker); return; }

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
    // already guaranteed availability above, so it is exempt. R07: emit the
    // failure so the review panel recovers instead of staying in Running.
    if (!autoSelect && (wantRapid || wantEnsemble) && !onnxAvailable) {
        emit ocrRunFailed(
            tr("OCR failed: PP-OCRv5 ONNX models not found. "
               "Change the OCR engine in Preferences → Engines, or install the models."));
        return;
    }

    _ocrRunning = true;

    // R07: generation identity of this job — the completion callback drops the
    // results if a newer request was issued in the meantime. R08: the page
    // count snapshot is the session's cheap revision proxy (page insert/delete
    // invalidates the session at accept time).
    ++_ocrJobGeneration;
    const qint64 jobGeneration = _ocrJobGeneration;
    const int sourcePageCount = viewer->pageCount();

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
    // §9.4: the OCRMode preprocessing checkboxes are persisted prefs — the
    // pipeline honors all four (defaults match the struct's long-standing
    // behavior: deskew/binarize/denoise on).
    OcrPreprocessOptions preprocessPrefs;
    preprocessPrefs.deskew   = QSettings().value(QStringLiteral("ocr/preprocessDeskew"), true).toBool();
    preprocessPrefs.binarize = QSettings().value(QStringLiteral("ocr/preprocessBinarize"), true).toBool();
    preprocessPrefs.denoise  = QSettings().value(QStringLiteral("ocr/preprocessDenoise"), true).toBool();
    preprocessPrefs.orientDetect = orientDetect;
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
                                       wantRapid, wantEnsemble, ocrLang, preprocessPrefs,
                                       jobGeneration, sourcePageCount]() {
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
                    // §9.4 P0: honor the persisted preprocessing prefs
                    // (Auto-Rotate included); word boxes still map back to the
                    // original page via PreprocessedImage::inverseTransform.
                    pipeline.setPreprocessing(preprocessPrefs);
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

        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, viewerPtr, filePath, page, pageImg, resultsArr, mergedWords, error, jobGeneration, sourcePageCount]() {
            // R07: a destroyed controller (and its panels) receives no callbacks.
            if (!self) return;

            // Every terminal path restores job dispatch exactly once, first.
            self->_ocrRunning = false;

            // R07 (F11): classify this completion — success, worker failure, or
            // stale/cancelled — and drive the matching recovery path. Widget
            // updates happen via the signals' host lambdas on the UI thread.
            QString message;
            const QString currentPath    = viewerPtr ? viewerPtr->filePath() : QString();
            const int currentPage        = viewerPtr ? viewerPtr->currentPage() : -1;
            const OcrJobVerdict verdict  = classifyOcrJobCompletion(
                jobGeneration, self->_ocrJobGeneration, filePath, page,
                currentPath, currentPage, error, &message);

            if (verdict == OcrJobVerdict::Failed) {
                emit self->ocrRunFailed(message);
                return;
            }
            if (verdict == OcrJobVerdict::Stale) {
                emit self->ocrRunAbandoned(message);
                return;
            }

            viewerPtr->setOcrResults(resultsArr);
            viewerPtr->setToolMode(ToolMode::SelectText);
            // R08: cache the review session — source identity/revision, the
            // page image the words belong to, and the words with stable IDs.
            // Acceptance merges the panel's reviewed records into this.
            OcrReviewSession session;
            session.generation      = jobGeneration;
            session.sourcePath      = filePath;
            session.sourcePage      = page;
            session.sourcePageCount = sourcePageCount;
            session.pageImage       = pageImg;
            session.words.reserve(mergedWords.size());
            for (int i = 0; i < mergedWords.size(); ++i) {
                OcrReviewedWord rec;
                rec.stableId      = i;
                rec.originalText  = mergedWords[i].text;
                rec.reviewedText  = mergedWords[i].text;
                rec.deleted       = false;
                rec.boundingBox   = mergedWords[i].boundingBox;
                rec.confidence    = mergedWords[i].confidence;
                rec.sourceEngine  = mergedWords[i].sourceEngine;
                session.words.append(rec);
            }
            self->m_reviewSession = session;
            // Feed the OCR Verify screen (if open) so it shows real recognised words
            // for review instead of an empty/decorative panel.
            emit self->ocrResultsReady(mergedWords);
            // U03: right after the words, deliver the FULL review session —
            // the source page image the words were recognized on travels by
            // implicit sharing, so the scan pane shows the real source image
            // and the zoom pane can crop actual pixels. Emitted after
            // ocrResultsReady so the words path always runs first.
            emit self->ocrReviewReady(session);
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
// R07 (F11): every exit reports its outcome via ocrSaveFinished so the
// review panel's Saving state always completes (cancelled saves retain the
// review edits and re-enable Accept; failed saves retain data for retry).
// R08 (F04): the REVIEWED words are authoritative for the export; the save
// dialog and the payload both use the session's REVIEWED page index, so the
// displayed page can no longer differ from the page being saved.
void EditController::onOcrAcceptRequested(const QList<OcrReviewedWord>& reviewedWords) {
    if (!_ctx || !_ctx->pdfEditor) {
        emit ocrSaveFinished(false, false, tr("No document is open — nothing to save."));
        return;
    }
    auto* viewer = _mainWindow->pdfViewer();

    // R08: validate the review session against the live viewer BEFORE doing
    // any work — a stale session (source/revision change) is a validation
    // failure, and the panel's Saving state must recover from it.
    QString reason;
    const QString currentPath = viewer ? viewer->filePath() : QString();
    const int currentCount    = viewer ? viewer->pageCount() : -1;
    if (!ocrSessionIsExportable(m_reviewSession, currentPath, currentCount, &reason)) {
        emit ocrSaveFinished(false, false, reason);
        return;
    }

    // R08: merge the panel's reviewed records into the session.
    QString mergeError;
    const PageOcrResult pageResult =
        buildReviewedPageOcrResult(m_reviewSession, reviewedWords, &mergeError);
    if (!mergeError.isEmpty()) {
        emit ocrSaveFinished(false, false, mergeError);
        return;
    }
    if (pageResult.words.isEmpty()) {
        // Every word was removed (or nothing was recognized) — nothing
        // searchable to write; the review stays editable for retry.
        emit ocrSaveFinished(false, false,
            tr("No reviewed words remain to save — restore the removed words or run OCR again."));
        return;
    }

    // R07/R08: dialog title and payload identity come from the REVIEWED
    // session (page + page count snapshot), never from the displayed page.
    const QFileInfo fi(m_reviewSession.sourcePath);
    const int totalPages  = m_reviewSession.sourcePageCount;
    const int pageIndex   = m_reviewSession.sourcePage;
    const QString outPath = QFileDialog::getSaveFileName(
        _mainWindow, ocrSaveDialogTitle(totalPages, pageIndex),
        fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName()
            + QStringLiteral("_ocr.pdf"),
        tr("PDF Files (*.pdf)"));
    if (outPath.isEmpty()) {
        // Cancelled save: nothing written, review edits retained.
        emit ocrSaveFinished(false, true, QString());
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    // R08: the original page image from the session is exported (never a
    // re-render of the currently displayed page), with the reviewed words as
    // the searchable text layer (Unicode).
    const bool ok = _ctx->pdfEditor->exportMrcPdfA(
        outPath, {m_reviewSession.pageImage}, {pageResult});
    QApplication::restoreOverrideCursor();

    if (ok)
        emit ocrSaveFinished(true, false,
            ocrSavedStatus(totalPages, pageIndex, QFileInfo(outPath).fileName()));
    else
        emit ocrSaveFinished(false, false,
            tr("Could not write the searchable MRC PDF/A copy. See the application log."));
}

void EditController::onOcrAcceptRequested() {
    onOcrAcceptRequested({});   // no panel records: unedited review
}

} // namespace gp
