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
        manageForms();
        break;
    case ToolId::TextField:
        addFormTextField();
        break;
    case ToolId::Checkbox:
        addFormCheckbox();
        break;
    case ToolId::Radio:
        addFormRadioButton();
        break;
    case ToolId::Dropdown:
        addFormDropdown();
        break;
    case ToolId::ListBox:
        addFormListBox();
        break;
    case ToolId::DateField:
        addFormDateField();
        break;
    case ToolId::NumField:
        addFormNumField();
        break;
    case ToolId::Button:
    case ToolId::SigField:
    case ToolId::AutoDetect:
    case ToolId::Tabs:
        QMessageBox::information(_mainWindow, tr("Interactive Forms"),
            tr("Advanced interactive fields and automated form detection are scheduled for the next engine update."));
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

void FormsController::manageForms() {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer) {
        viewer->setToolMode(ToolMode::EditObject);
        _mainWindow->statusBar()->showMessage(tr("Form Management Mode. AcroForm fields are now editable."), 5000);
    }
}

void FormsController::addFormTextField() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->forms) return;

    bool ok;
    QString name = QInputDialog::getText(_mainWindow, tr("Add Text Field"), tr("Field name:"), QLineEdit::Normal, "TextField1", &ok);
    if (!ok || name.isEmpty()) return;

    QRectF rect(100, 100, 200, 30);
    _ctx->document->setPath(viewer->filePath());
    AddFormFieldCommand(
        _ctx->forms.get(), _ctx->document.get(), AddFormFieldCommand::FieldType::Text,
        viewer->currentPage(), rect, name).redo();
    _mainWindow->statusBar()->showMessage(tr("Text field '%1' added to page %2").arg(name).arg(viewer->currentPage() + 1), 5000);
}

void FormsController::addFormCheckbox() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->forms) return;

    bool ok;
    QString name = QInputDialog::getText(_mainWindow, tr("Add Checkbox"), tr("Field name:"), QLineEdit::Normal, "Checkbox1", &ok);
    if (!ok || name.isEmpty()) return;

    QRectF rect(100, 100, 20, 20);
    _ctx->document->setPath(viewer->filePath());
    AddFormFieldCommand(
        _ctx->forms.get(), _ctx->document.get(), AddFormFieldCommand::FieldType::Checkbox,
        viewer->currentPage(), rect, name).redo();
    _mainWindow->statusBar()->showMessage(tr("Checkbox '%1' added to page %2").arg(name).arg(viewer->currentPage() + 1), 5000);
}

void FormsController::addFormRadioButton() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->forms) return;

    bool ok;
    QString name = QInputDialog::getText(_mainWindow, tr("Add Radio Button"), tr("Field name:"), QLineEdit::Normal, "Radio1", &ok);
    if (!ok || name.isEmpty()) return;

    QRectF rect(100, 100, 20, 20);
    _ctx->document->setPath(viewer->filePath());
    AddFormFieldCommand(
        _ctx->forms.get(), _ctx->document.get(), AddFormFieldCommand::FieldType::Radio,
        viewer->currentPage(), rect, name).redo();
    _mainWindow->statusBar()->showMessage(tr("Radio button '%1' added to page %2").arg(name).arg(viewer->currentPage() + 1), 5000);
}

void FormsController::addFormDropdown() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->forms) return;

    bool ok;
    QString name = QInputDialog::getText(_mainWindow, tr("Add Dropdown"), tr("Field name:"), QLineEdit::Normal, "Dropdown1", &ok);
    if (!ok || name.isEmpty()) return;

    QString optionsStr = QInputDialog::getText(_mainWindow, tr("Dropdown Options"), tr("Enter options separated by commas:"), QLineEdit::Normal, "Option1,Option2,Option3", &ok);
    if (!ok || optionsStr.isEmpty()) return;

    QStringList options = optionsStr.split(',', Qt::SkipEmptyParts);
    for (auto &opt : options) opt = opt.trimmed();

    QRectF rect(100, 100, 200, 25);
    _ctx->document->setPath(viewer->filePath());
    AddFormFieldCommand(
        _ctx->forms.get(), _ctx->document.get(), AddFormFieldCommand::FieldType::Dropdown,
        viewer->currentPage(), rect, name, options).redo();
    _mainWindow->statusBar()->showMessage(tr("Dropdown '%1' added to page %2").arg(name).arg(viewer->currentPage() + 1), 5000);
}

void FormsController::addFormListBox() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->forms) return;

    bool ok;
    QString name = QInputDialog::getText(_mainWindow, tr("Add List Box"), tr("Field name:"), QLineEdit::Normal, "ListBox1", &ok);
    if (!ok || name.isEmpty()) return;

    QString optionsStr = QInputDialog::getText(_mainWindow, tr("List Box Options"), tr("Enter options separated by commas:"), QLineEdit::Normal, "Item1,Item2,Item3", &ok);
    if (!ok || optionsStr.isEmpty()) return;

    QStringList options = optionsStr.split(',', Qt::SkipEmptyParts);
    for (auto &opt : options) opt = opt.trimmed();

    QRectF rect(100, 100, 200, 100);
    _ctx->document->setPath(viewer->filePath());
    AddFormFieldCommand(
        _ctx->forms.get(), _ctx->document.get(), AddFormFieldCommand::FieldType::ListBox,
        viewer->currentPage(), rect, name, options).redo();
    _mainWindow->statusBar()->showMessage(tr("ListBox '%1' added to page %2").arg(name).arg(viewer->currentPage() + 1), 5000);
}

void FormsController::addFormDateField() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->forms) return;

    bool ok;
    QString name = QInputDialog::getText(_mainWindow, tr("Add Date Field"), tr("Field name:"), QLineEdit::Normal, "Date1", &ok);
    if (!ok || name.isEmpty()) return;

    QRectF rect(100, 100, 200, 30);
    _ctx->document->setPath(viewer->filePath());
    AddFormFieldCommand(
        _ctx->forms.get(), _ctx->document.get(), AddFormFieldCommand::FieldType::Date,
        viewer->currentPage(), rect, name).redo();
    _mainWindow->statusBar()->showMessage(tr("Date field '%1' added to page %2").arg(name).arg(viewer->currentPage() + 1), 5000);
}

void FormsController::addFormNumField() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->forms) return;

    bool ok;
    QString name = QInputDialog::getText(_mainWindow, tr("Add Numeric Field"), tr("Field name:"), QLineEdit::Normal, "Numeric1", &ok);
    if (!ok || name.isEmpty()) return;

    QRectF rect(100, 100, 200, 30);
    _ctx->document->setPath(viewer->filePath());
    AddFormFieldCommand(
        _ctx->forms.get(), _ctx->document.get(), AddFormFieldCommand::FieldType::Numeric,
        viewer->currentPage(), rect, name).redo();
    _mainWindow->statusBar()->showMessage(tr("Numeric field '%1' added to page %2").arg(name).arg(viewer->currentPage() + 1), 5000);
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
