#include "EditController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "engines/OcrEngine.h"
#include "core/interfaces/IOcrEngine.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "commands/RedactCommand.h"
#include "commands/EditAnnotationCommand.h"
#include "commands/MoveImageCommand.h"
#include "commands/ResizeImageCommand.h"
#include "commands/RotateImageCommand.h"
#include "commands/ReplaceImageCommand.h"
#include "commands/DeleteImageCommand.h"
#include "ui/AnnotationLayer.h"
#include "ui/FindBar.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QPointer>
#include <QMetaObject>
#include <QCoreApplication>
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
#include <QUndoStack>
#include "shell/StatusBar.h"

namespace gp {

EditController::EditController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

bool EditController::handles(const QString& toolId) const {
    return toolId == "search" || toolId == "find" ||
           toolId == "ocr" ||
           toolId == "editText" || toolId == "edit-text" ||
           toolId == "hand" || toolId == "select" ||
           toolId == "selectObj" || toolId == "editObject" ||
           toolId == "highlight" || toolId == "underline" ||
           toolId == "pencil" || toolId == "freehand" ||
           toolId == "textbox" || toolId == "addText" ||
           toolId == "note" || toolId == "comment" ||
           toolId == "markRedact" || toolId == "signature" ||
           toolId == "rect" || toolId == "rectangle" ||
           toolId == "oval" || toolId == "ellipse" ||
           toolId == "line" || toolId == "arrow" ||
           toolId == "image" || toolId == "editImage" ||
           toolId == "squiggly" || toolId == "strike" || toolId == "strikeout";
}

void EditController::activate(const QString& toolId) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    const QHash<QString, ToolMode> toolModes = {
        {"hand", ToolMode::HandTool},
        {"select", ToolMode::SelectText},
        {"selectObj", ToolMode::EditObject},
        {"editObject", ToolMode::EditObject},
        {"highlight", ToolMode::Highlight},
        {"underline", ToolMode::Underline},
        {"squiggly", ToolMode::Underline},
        {"strike", ToolMode::Underline},
        {"strikeout", ToolMode::Underline},
        {"pencil", ToolMode::DrawFreehand},
        {"freehand", ToolMode::DrawFreehand},
        {"textbox", ToolMode::AddTextBox},
        {"addText", ToolMode::AddTextBox},
        {"note", ToolMode::AddComment},
        {"comment", ToolMode::AddComment},
        {"markRedact", ToolMode::Redact},
        {"signature", ToolMode::AddSignature},
        {"rect", ToolMode::DrawRectangle},
        {"rectangle", ToolMode::DrawRectangle},
        {"oval", ToolMode::DrawEllipse},
        {"ellipse", ToolMode::DrawEllipse},
        {"line", ToolMode::DrawLine},
        {"arrow", ToolMode::DrawArrow},
    };

    if (toolId == "search" || toolId == "find") {
        _mainWindow->toggleFindBar();
    } else if (toolId == "ocr") {
        runOcr();
    } else if (toolId == "editText" || toolId == "edit-text") {
        editPdfText();
    } else if (toolId == "image" || toolId == "editImage") {
        enterImageEditMode();
    } else if (toolModes.contains(toolId)) {
        viewer->setToolMode(toolModes.value(toolId));
        _mainWindow->statusBar()->showMessage(tr("Tool active: %1").arg(toolId), 2500);
    }
}

void EditController::onSearchRequested(const QString &text, bool forward, bool matchCase, bool wholeWords) {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer) {
        viewer->searchDocument(text, forward, matchCase, wholeWords);
    }
}

void EditController::onRedactAllRequested(const QString &text, bool matchCase, bool wholeWords) {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer && _ctx && _ctx->document && _ctx->undoStack) {
        _ctx->document->setPath(viewer->filePath());
        _ctx->undoStack->push(new RedactCommand(viewer, _ctx->document.get(), text, matchCase, wholeWords));
        _mainWindow->statusBar()->showMessage(tr("Applied redactions to all search results for '%1'").arg(text), 5000);
    }
}

void EditController::runOcr() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->ocr || _ocrRunning) return;

    const QString filePath = viewer->filePath();
    const int page = viewer->currentPage();
    if (filePath.isEmpty() || page < 0) return;

    _ocrRunning = true;
    _mainWindow->statusBar()->showMessage(tr("Processing OCR (Tesseract Engine)..."));

    QPointer<EditController> self(this);
    QPointer<PdfViewerWidget> viewerPtr(viewer);

    QThread *worker = QThread::create([self, viewerPtr, filePath, page]() {
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
                OcrEngine engine;
                if (!engine.initialize("eng")) {
                    error = QStringLiteral("OCR failed: English language data is unavailable.");
                } else {
                    resultsArr = engine.processImage(pageImg);
                }
            }
        }

        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, viewerPtr, filePath, page, resultsArr, error]() {
            if (!self) return;

            self->_ocrRunning = false;

            if (!error.isEmpty()) {
                self->_mainWindow->statusBar()->showMessage(error, 5000);
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

void EditController::editPdfText() {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer) {
        viewer->setToolMode(ToolMode::EditText);
        if (_ctx && _ctx->pdfEditor) {
            _ctx->pdfEditor->loadDocumentForEditing(viewer->filePath());
        }
        _mainWindow->statusBar()->showMessage(tr("Direct Text Editing Mode. Click a text block to modify its contents."), 5000);
    }
}

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
