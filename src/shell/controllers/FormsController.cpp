#include "FormsController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "core/interfaces/IFormManager.h"
#include "commands/AddFormFieldCommand.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QUndoStack>
#include "shell/StatusBar.h"

namespace gp {

FormsController::FormsController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

bool FormsController::handles(const QString& toolId) const {
    return toolId == "textField" || toolId == "text-field" ||
           toolId == "checkbox" ||
           toolId == "radio" ||
           toolId == "dropdown" ||
           toolId == "createForm" ||
           toolId == "listbox" || toolId == "button" ||
           toolId == "dateField" || toolId == "numField" || toolId == "sigField" ||
           toolId == "autoDetect" || toolId == "tabs";
}

void FormsController::activate(const QString& toolId) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    if (toolId == "createForm") {
        manageForms();
    } else if (toolId == "textField" || toolId == "text-field") {
        addFormTextField();
    } else if (toolId == "checkbox") {
        addFormCheckbox();
    } else if (toolId == "radio") {
        addFormRadioButton();
    } else if (toolId == "dropdown") {
        addFormDropdown();
    } else if (toolId == "listbox" || toolId == "button" || toolId == "dateField" || toolId == "numField" || toolId == "sigField" || toolId == "autoDetect" || toolId == "tabs") {
        QMessageBox::information(_mainWindow, tr("Interactive Forms"),
            tr("Advanced interactive fields and automated form detection are scheduled for the next engine update."));
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

} // namespace gp
