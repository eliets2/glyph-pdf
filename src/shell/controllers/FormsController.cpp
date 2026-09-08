// SPDX-License-Identifier: Apache-2.0
#include "FormsController.h"
#include "ui/SignatureDialog.h"
#include "core/interfaces/ISignatureManager.h"

#include "core/AppContext.h"
#include "core/interfaces/IPdfEditorEngine.h"   // releaseResidentFile (V01 swap)
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "core/interfaces/IFormManager.h"
#include "commands/AddFormFieldCommand.h"
#include "commands/AutoDetectPlacement.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QUndoStack>
#include "engines/SafeSave.h"
#include "shell/StatusBar.h"

namespace gp {

FormsController::FormsController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> FormsController::handledTools() const {
    return {
        ToolId::TextField, ToolId::Checkbox, ToolId::Radio, ToolId::Dropdown,
        ToolId::CreateForm, ToolId::ListBox, ToolId::Button, ToolId::DateField,
        ToolId::NumField, ToolId::SigField, ToolId::AutoDetect, ToolId::Tabs,
        ToolId::ImportData, ToolId::ExportData, ToolId::CalcField
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
    case ToolId::CalcField:
        _mainWindow->activateScreen("form");
        if (viewer) viewer->setToolMode(ToolMode::FormAddCalculated);
        break;
    case ToolId::AutoDetect:
        autoDetectFields();
        break;
    case ToolId::Tabs:
        editTabOrder();
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




void FormsController::autoDetectFields() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->forms) return;

    auto suggestions = _ctx->forms->autoDetectFields(viewer->filePath(), viewer->currentPage());
    if (suggestions.isEmpty()) {
        QMessageBox::information(_mainWindow, tr("Auto Detect"), tr("No form fields detected on this page."));
        return;
    }

    // V06: the placement goes THROUGH the application undo stack as ONE
    // compound command (AutoDetectPlacement), with honest placed/failed
    // counts — the status message only promises what really happened.
    const AutoDetectPlacement::Outcome outcome =
        AutoDetectPlacement::apply(_ctx->forms.get(), _ctx->document.get(),
                                   _ctx->undoStack.get(), suggestions,
                                   viewer->currentPage());
    _mainWindow->statusBar()->showMessage(
        AutoDetectPlacement::statusMessage(outcome.placed, outcome.failed), 8000);
}

void FormsController::editTabOrder() {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer) {
        QMetaObject::invokeMethod(_mainWindow, "onModeChanged", Q_ARG(QString, "form"));
        QMessageBox::information(_mainWindow, tr("Tab Order Editor"), tr("To edit tab order, please open Form Builder Mode and click 'Tab Order' on the sidebar."));
    }
}

void FormsController::onImportDataRequested() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->forms) return;

    QString dataPath = QFileDialog::getOpenFileName(_mainWindow, tr("Import Form Data"), "", tr("Form Data (*.fdf *.csv)"));
    if (dataPath.isEmpty()) return;

    // V01 (PARITY-BRANCH-REVIEW): the OLD flow loaded the temporary output
    // into the viewer and then deleted `viewer->filePath()` — which was by
    // then the TEMP path — and renamed the temp onto itself: the import never
    // landed in the real document, whose bytes on disk stayed stale.
    // The repaired flow swaps the REAL path on disk and keeps the viewer on it.
    const QString originalPath = viewer->filePath();
    if (originalPath.isEmpty()) return;
    const QString outputPath = originalPath + ".tmp";
    QStringList unsupported;
    if (_ctx->forms->importFormData(originalPath, dataPath, outputPath, &unsupported)) {
        // Commit the imported bytes onto the real path through the shared
        // checked-commit boundary: the original is never destroyed unless the
        // replacement actually succeeded, and the viewer's held handle is
        // released/restored around the atomic rename by the SafeSave
        // coordinator (the same boundary every in-place write uses).
        //
        // The viewer is not the only holder: the open document is also
        // resident in the editing engine's PoDoFo backend, whose parser keeps
        // an OS device on the real path for lazy resolution — the same device
        // the engine's own same-file save re-seats away before its commit
        // (EC01). A shell-side replacement must release it explicitly (the
        // coordinator cannot: calling into the engine from there would
        // re-enter engine locks held by engine-side commits). The next engine
        // access lazily re-resolves from the real path — the imported result,
        // or the preserved original if the commit fails; truthful either way.
        if (_ctx->pdfEditor) _ctx->pdfEditor->releaseResidentFile(originalPath);
        QString commitErr;
        const bool committed = gp::SafeSave::commitFileToDestination(
            outputPath, originalPath, &commitErr);
        QFile::remove(outputPath);   // the candidate is consumed either way
        if (!committed) {
            QMessageBox::warning(_mainWindow, tr("Import Failed"),
                tr("The form data was imported, but the document could not be "
                   "replaced on disk (%1). The original document is unchanged.")
                    .arg(commitErr));
            return;
        }
        // The viewer still displays the pre-import bytes from its (restored)
        // handle — reload so what is shown is what was committed.
        viewer->loadDocument(originalPath);
        _mainWindow->statusBar()->showMessage(tr("Successfully imported form data from %1").arg(QFileInfo(dataPath).fileName()), 5000);
        // §9.6 P0: a bulk import that silently dropped values (radio/pushbutton
        // targets, unknown names) must not read as success.
        if (!unsupported.isEmpty()) {
            QMessageBox::warning(_mainWindow, tr("Import Incomplete"),
                tr("Form data was imported, but %1 field(s) could not be set and were skipped:\n\n%2\n\n"
                   "Radio groups and push buttons cannot be filled by import; check that field names match the document.")
                    .arg(unsupported.size()).arg(unsupported.join(", ")));
        }
    } else {
        QFile::remove(outputPath);   // never leave a half-written temp behind
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
