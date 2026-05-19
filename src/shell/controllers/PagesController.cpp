#include "PagesController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "commands/RotatePageCommand.h"
#include "commands/DeletePageCommand.h"
#include "commands/InsertPageCommand.h"
#include "ui/PageManagementDialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QUndoStack>
#include "shell/StatusBar.h"

namespace gp {

PagesController::PagesController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

bool PagesController::handles(const QString& toolId) const {
    return toolId == "rotate" || toolId == "rotR" || toolId == "rotate-cw" ||
           toolId == "rotL" || toolId == "rotate-ccw" ||
           toolId == "deletePage" || toolId == "delete-page" ||
           toolId == "insertPage" || toolId == "insert-page" ||
           toolId == "extract" || toolId == "split" || toolId == "reorder";
}

void PagesController::activate(const QString& toolId) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    if (toolId == "rotate" || toolId == "rotR" || toolId == "rotate-cw") {
        rotateRight();
    } else if (toolId == "rotL" || toolId == "rotate-ccw") {
        rotateLeft();
    } else if (toolId == "deletePage" || toolId == "delete-page") {
        if (_ctx && _ctx->undoStack && viewer->pageCount() > 1) {
            _ctx->document->setPath(viewer->filePath());
            _ctx->undoStack->push(new DeletePageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage()));
            _mainWindow->statusBar()->showMessage(tr("Deleted current page."), 3000);
        } else {
            _mainWindow->statusBar()->showMessage(tr("Cannot delete the last page of the document."), 3000);
        }
    } else if (toolId == "insertPage" || toolId == "insert-page") {
        if (_ctx && _ctx->undoStack) {
            _ctx->document->setPath(viewer->filePath());
            _ctx->undoStack->push(new InsertPageCommand(_ctx->pdfEditor.get(), _ctx->document.get(), viewer->currentPage() + 1));
            _mainWindow->statusBar()->showMessage(tr("Inserted blank page."), 3000);
        }
    } else if (toolId == "extract") {
        showPageManagement();
    } else if (toolId == "split" || toolId == "reorder") {
        QMessageBox::information(_mainWindow, tr("Page Organizer"),
            tr("Splitting and advanced reordering require the multi-page page arranger scheduled for the upcoming engine update."));
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

} // namespace gp
