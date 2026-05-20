#include "PagesController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "commands/RotatePageCommand.h"
#include "commands/DeletePageCommand.h"
#include "commands/InsertPageCommand.h"
#include "commands/CropPageCommand.h"
#include "commands/ReorderPageCommand.h"
#include "ui/PageManagementDialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QUndoStack>
#include "shell/StatusBar.h"

namespace gp {

PagesController::PagesController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> PagesController::handledTools() const {
    return {
        ToolId::RotateCW, ToolId::RotateCCW,
        ToolId::DeletePage, ToolId::InsertPage,
        ToolId::Extract, ToolId::Split, ToolId::Reorder,
        ToolId::Crop, ToolId::Resize, ToolId::AddHeader, ToolId::AddFooter,
        ToolId::AddPageNumbers, ToolId::BatesNumber
    };
}

void PagesController::activate(ToolId id) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    switch (id) {
    case ToolId::RotateCW:
        rotateRight();
        break;
    case ToolId::RotateCCW:
        rotateLeft();
        break;
    case ToolId::DeletePage:
        if (_ctx && _ctx->undoStack && viewer->pageCount() > 1) {
            _ctx->document->setPath(viewer->filePath());
            _ctx->undoStack->push(new DeletePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage()));
            _mainWindow->statusBar()->showMessage(tr("Deleted current page."), 3000);
        } else {
            _mainWindow->statusBar()->showMessage(tr("Cannot delete the last page of the document."), 3000);
        }
        break;
    case ToolId::InsertPage:
        if (_ctx && _ctx->undoStack) {
            _ctx->document->setPath(viewer->filePath());
            _ctx->undoStack->push(new InsertPageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage() + 1));
            _mainWindow->statusBar()->showMessage(tr("Inserted blank page."), 3000);
        }
        break;
    case ToolId::Extract:
        showPageManagement();
        break;
    case ToolId::Split:
    case ToolId::Reorder:
        QMessageBox::information(_mainWindow, tr("Page Organizer"),
            tr("Splitting and advanced reordering require the multi-page page arranger scheduled for the upcoming engine update."));
        break;
    case ToolId::Crop:
        viewer->setToolMode(ToolMode::Crop);
        _mainWindow->statusBar()->showMessage(tr("Crop Tool: Drag a rectangle on the page to crop."), 5000);
        break;
    case ToolId::Resize:
    case ToolId::AddHeader:
    case ToolId::AddFooter:
    case ToolId::AddPageNumbers:
    case ToolId::BatesNumber:
        QMessageBox::information(_mainWindow, tr("Page Operations"),
            tr("Advanced page formatting options (Resize, Headers, Footers, Bates) will be available in the detailed dialogs being finalized for the next phase."));
        break;
    default:
        break;
    }
}

void PagesController::rotateLeft() {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer && _ctx && _ctx->undoStack) {
        _ctx->undoStack->push(new RotatePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage(), -90));
    }
}

void PagesController::rotateRight() {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer && _ctx && _ctx->undoStack) {
        _ctx->undoStack->push(new RotatePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage(), 90));
    }
}

void PagesController::showPageManagement() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx) return;

    PageManagementDialog dlg(viewer->pageCount(), _mainWindow);
    if (dlg.exec() == QDialog::Accepted) {
        PageManagementDialog::Operation op = dlg.selectedOperation();
        int from = dlg.fromPage() - 1;
        int to = dlg.toPage() - 1;

        if (op == PageManagementDialog::Operation::ExtractPages) {
            QString outputFile = QFileDialog::getSaveFileName(_mainWindow, tr("Save Extracted Pages"), "", tr("PDF Files (*.pdf)"));
            if (outputFile.isEmpty()) return;
            viewer->extractPages(from, to, outputFile);
        } else {
            _ctx->document->setPath(viewer->filePath());
            _ctx->undoStack->beginMacro(tr("Page Management"));
            if (op == PageManagementDialog::Operation::DeletePages) {
                for (int i = to; i >= from; --i) {
                    _ctx->undoStack->push(new DeletePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), i));
                }
                _mainWindow->statusBar()->showMessage(tr("Deleted pages %1 to %2").arg(from + 1).arg(to + 1), 5000);
            } else if (op == PageManagementDialog::Operation::RotatePages) {
                for (int i = from; i <= to; ++i) {
                    _ctx->undoStack->push(new RotatePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), i, dlg.rotationAngle()));
                }
                _mainWindow->statusBar()->showMessage(tr("Rotated pages %1 to %2").arg(from + 1).arg(to + 1), 5000);
            } else if (op == PageManagementDialog::Operation::InsertBlankPage) {
                _ctx->undoStack->push(new InsertPageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), from + 1));
                _mainWindow->statusBar()->showMessage(tr("Inserted blank page at %1").arg(from + 2), 5000);
            }
            _ctx->undoStack->endMacro();
        }
    }
}

void PagesController::onPageReordered(int from, int to) {
    if (_ctx && _ctx->undoStack && _mainWindow->pdfViewer()) {
        _ctx->document->setPath(_mainWindow->pdfViewer()->filePath());
        _ctx->undoStack->push(new ReorderPageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), from, to));
        _mainWindow->statusBar()->showMessage(tr("Reordered page %1 to %2.").arg(from + 1).arg(to + 1), 3000);
    }
}

void PagesController::onCropRequested(int pageIndex, QRectF rect) {
    if (_ctx && _ctx->undoStack && _mainWindow->pdfViewer()) {
        _ctx->document->setPath(_mainWindow->pdfViewer()->filePath());
        _ctx->undoStack->push(new CropPageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), pageIndex, rect));
        _mainWindow->statusBar()->showMessage(tr("Cropped page %1.").arg(pageIndex + 1), 3000);
    }
}

} // namespace gp
