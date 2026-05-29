#include "FormsController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "core/interfaces/IFormManager.h"
#include "commands/AddFormFieldCommand.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QUndoStack>
#include "shell/StatusBar.h"

namespace gp {

FormsController::FormsController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> FormsController::handledTools() const {
    return {
        ToolId::TextField, ToolId::Checkbox, ToolId::Radio, ToolId::Dropdown,
        ToolId::CreateForm, ToolId::ListBox, ToolId::Button, ToolId::DateField,
        ToolId::NumField, ToolId::SigField, ToolId::AutoDetect, ToolId::Tabs,
        ToolId::ImportData, ToolId::ExportData
    };
}

void FormsController::activate(ToolId id) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    switch (id) {
    case ToolId::CreateForm:
        _mainWindow->activateScreen("form");
        break;
    case ToolId::TextField:
        _mainWindow->activateScreen("form");
        if (viewer) viewer->setToolMode(ToolMode::FormAddText);
        break;
    case ToolId::Checkbox:
        _mainWindow->activateScreen("form");
        if (viewer) viewer->setToolMode(ToolMode::FormAddCheckbox);
        break;
    case ToolId::Radio:
        _mainWindow->activateScreen("form");
        if (viewer) viewer->setToolMode(ToolMode::FormAddRadio);
        break;
    case ToolId::Dropdown:
        _mainWindow->activateScreen("form");
        if (viewer) viewer->setToolMode(ToolMode::FormAddDropdown);
        break;
    case ToolId::ListBox:
        _mainWindow->activateScreen("form");
        if (viewer) viewer->setToolMode(ToolMode::FormAddListBox);
        break;
    case ToolId::DateField:
        _mainWindow->activateScreen("form");
        if (viewer) viewer->setToolMode(ToolMode::FormAddDate);
        break;
    case ToolId::NumField:
        _mainWindow->activateScreen("form");
        if (viewer) viewer->setToolMode(ToolMode::FormAddNumeric);
        break;
    case ToolId::Button:
        _mainWindow->activateScreen("form");
        if (viewer) viewer->setToolMode(ToolMode::FormAddButton);
        break;
    case ToolId::SigField:
        _mainWindow->activateScreen("form");
        if (viewer) viewer->setToolMode(ToolMode::FormAddSignature);
        break;
    case ToolId::AutoDetect:
        _mainWindow->activateScreen("form");
        QMessageBox::information(_mainWindow, tr("Auto-Detect"), tr("Auto-detect form fields from document structure is coming in v1.1."));
        break;
    case ToolId::Tabs:
        _mainWindow->activateScreen("form");
        QMessageBox::information(_mainWindow, tr("Tab Order"), tr("Use the Tab Order panel in the Form Builder screen to manage tab order."));
        break;
    case ToolId::ImportData:
        onImportDataRequested();
        break;
    case ToolId::ExportData:
        onExportDataRequested();
        break;
    default:
        break;
    }
}

void FormsController::onImportDataRequested() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->forms) return;

    QString dataPath = QFileDialog::getOpenFileName(_mainWindow, tr("Import Form Data"), "", tr("Form Data (*.fdf *.csv)"));
    if (dataPath.isEmpty()) return;

    QString outputPath = viewer->filePath() + ".tmp"; // Use temporary write or overwrite
    if (_ctx->forms->importFormData(viewer->filePath(), dataPath, outputPath)) {
        // Assume saving inplace or reloading the new path. In a real app we might load it back.
        viewer->loadDocument(outputPath);
        QFile::remove(viewer->filePath());
        QFile::rename(outputPath, viewer->filePath());
        _mainWindow->statusBar()->showMessage(tr("Successfully imported form data from %1").arg(QFileInfo(dataPath).fileName()), 5000);
    } else {
        QMessageBox::warning(_mainWindow, tr("Import Failed"), tr("Could not import form data."));
    }
}

void FormsController::onExportDataRequested() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->forms) return;

    QString savePath = QFileDialog::getSaveFileName(_mainWindow, tr("Export Form Data"), "", tr("FDF File (*.fdf);;CSV File (*.csv)"));
    if (savePath.isEmpty()) return;

    QString format = savePath.endsWith(".csv", Qt::CaseInsensitive) ? "csv" : "fdf";

    if (_ctx->forms->exportFormData(viewer->filePath(), savePath, format)) {
        _mainWindow->statusBar()->showMessage(tr("Successfully exported form data to %1").arg(QFileInfo(savePath).fileName()), 5000);
    } else {
        QMessageBox::warning(_mainWindow, tr("Export Failed"), tr("Could not export form data."));
    }
}

} // namespace gp
