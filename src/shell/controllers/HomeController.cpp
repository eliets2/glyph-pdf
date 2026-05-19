#include "HomeController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include "shell/StatusBar.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include <QUndoStack>
#include <QPdfDocument>

namespace gp {

HomeController::HomeController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

bool HomeController::handles(const QString& toolId) const {
    return toolId == "open" || toolId == "save" || toolId == "saveAs" || toolId == "save-as" ||
           toolId == "print" || toolId == "share" || toolId == "properties";
}

void HomeController::activate(const QString& toolId) {
    if (toolId == "open") {
        QString fileName = QFileDialog::getOpenFileName(
            _mainWindow, tr("Open PDF Document"), QString(),
            tr("PDF Files (*.pdf);;All Files (*)"));
        if (!fileName.isEmpty()) {
            _mainWindow->openDocument(fileName);
        }
    } else if (toolId == "save") {
        onSave();
    } else if (toolId == "saveAs" || toolId == "save-as") {
        onSaveAs();
    } else if (toolId == "print") {
        onPrint();
    } else if (toolId == "share") {
        onShare();
    } else if (toolId == "properties") {
        showProperties();
    }
}

void HomeController::onSave() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }
    if (!_ctx->pdfEditor) {
        _mainWindow->statusBar()->showMessage(tr("Save unavailable: PDF engine is not ready."), 5000);
        return;
    }
    const QString filePath = viewer->filePath();
    if (filePath.isEmpty()) {
        _mainWindow->statusBar()->showMessage(tr("Save unavailable: current tab has no file path."), 5000);
        return;
    }
    viewer->saveAnnotations();
    const bool ok = _ctx->pdfEditor->saveDocument(filePath);
    if (ok) {
        if (_ctx->undoStack) _ctx->undoStack->setClean();
        _mainWindow->statusBar()->showMessage(tr("Document saved: %1").arg(filePath), 5000);
    } else {
        _mainWindow->statusBar()->showMessage(tr("Save failed: %1").arg(filePath), 5000);
    }
}

void HomeController::onSaveAs() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) return;
    QString fileName = QFileDialog::getSaveFileName(
        _mainWindow,
        tr("Save Document As"), QFileInfo(viewer->filePath()).fileName(),
        tr("PDF Files (*.pdf);;All Files (*)"));
    if (fileName.isEmpty()) return;
    viewer->saveDocumentAs(fileName);
    _mainWindow->statusBar()->showMessage(tr("Document saved to %1").arg(fileName), 5000);
}

void HomeController::onShare() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) return;
    QString subject = tr("PDF Document: %1").arg(QFileInfo(viewer->filePath()).fileName());
    QString body = tr("Please find the attached PDF document.");
    QString url = QString("mailto:?subject=%1&body=%2").arg(subject).arg(body);
    QDesktopServices::openUrl(QUrl(url, QUrl::TolerantMode));
}

void HomeController::onPrint() {
    auto* viewer = _mainWindow->pdfViewer();
    if (viewer) {
        viewer->printDocument();
    } else {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
    }
}

void HomeController::showProperties() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !viewer->document()) return;
    auto* doc = viewer->document();

    QString title = doc->metaData(QPdfDocument::MetaDataField::Title).toString();
    QString author = doc->metaData(QPdfDocument::MetaDataField::Author).toString();
    QString subject = doc->metaData(QPdfDocument::MetaDataField::Subject).toString();
    QString keywords = doc->metaData(QPdfDocument::MetaDataField::Keywords).toString();
    QString creator = doc->metaData(QPdfDocument::MetaDataField::Creator).toString();
    QString producer = doc->metaData(QPdfDocument::MetaDataField::Producer).toString();

    QString info = tr("<b>Title:</b> %1<br>"
                      "<b>Author:</b> %2<br>"
                      "<b>Subject:</b> %3<br>"
                      "<b>Keywords:</b> %4<br>"
                      "<b>Creator:</b> %5<br>"
                      "<b>Producer:</b> %6<br>"
                      "<b>Page Count:</b> %7")
                      .arg(title.isEmpty() ? tr("[None]") : title)
                      .arg(author.isEmpty() ? tr("[None]") : author)
                      .arg(subject.isEmpty() ? tr("[None]") : subject)
                      .arg(keywords.isEmpty() ? tr("[None]") : keywords)
                      .arg(creator.isEmpty() ? tr("[None]") : creator)
                      .arg(producer.isEmpty() ? tr("[None]") : producer)
                      .arg(doc->pageCount());

    QMessageBox::information(_mainWindow, tr("Document Properties"), info);
}

} // namespace gp
