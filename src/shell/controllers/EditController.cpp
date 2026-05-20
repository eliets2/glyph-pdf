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
#include <QPdfDocument>
#include <QPdfDocumentRenderOptions>
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

    // Map ToolIds that simply set a tool mode
    static const QHash<ToolId, ToolMode> toolModes = {
        { ToolId::Hand,          ToolMode::HandTool },
        { ToolId::Select,        ToolMode::SelectText },
        { ToolId::SelectObject,  ToolMode::EditObject },
        { ToolId::EditObject,    ToolMode::EditObject },
        { ToolId::Highlight,     ToolMode::Highlight },
        { ToolId::Underline,     ToolMode::Underline },
        { ToolId::Squiggly,      ToolMode::Underline },
        { ToolId::Strikeout,     ToolMode::Underline },
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
        QRectF rect(pos.x(), pos.y(), 200, 50); // Default placeholder size
        _ctx->document->setPath(viewer->filePath());
        _ctx->undoStack->push(new EditTextInlineCommand(_ctx->pdfEditor.get(), _ctx->document.get(), pageIndex, rect, newText,
                                                        _fontFamily, _fontSize, _fontColor, _fontBold, _fontItalic, _fontAlignment));
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
