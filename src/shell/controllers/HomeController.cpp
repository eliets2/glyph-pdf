#include "HomeController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "ui/PageSetupDialog.h"
#include "ui/ExportPresetsPanel.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <mapi.h>
#endif

#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QSettings>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPainter>
#include "shell/StatusBar.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include <QUndoStack>
#include <QPdfDocument>

namespace gp {

HomeController::HomeController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> HomeController::handledTools() const {
    return {
        ToolId::Open, ToolId::Save, ToolId::SaveAs,
        ToolId::Print, ToolId::PrintPreview, ToolId::PageSetup,
        ToolId::ExportPresets, ToolId::Share, ToolId::Properties
    };
}

void HomeController::activate(ToolId id) {
    switch (id) {
    case ToolId::Open: {
        QString fileName = QFileDialog::getOpenFileName(
            _mainWindow, tr("Open PDF Document"), QString(),
            tr("PDF Files (*.pdf);;All Files (*)"));
        if (!fileName.isEmpty()) {
            _mainWindow->openDocument(fileName);
        }
        break;
    }
    case ToolId::Save:
        onSave();
        break;
    case ToolId::SaveAs:
        onSaveAs();
        break;
    case ToolId::Print:
        onPrint();
        break;
    case ToolId::PrintPreview:
        onPrintPreview();
        break;
    case ToolId::PageSetup:
        onPageSetup();
        break;
    case ToolId::ExportPresets:
        onExportPresets();
        break;
    case ToolId::Share:
        onShare();
        break;
    case ToolId::Properties:
        showProperties();
        break;
    default:
        break;
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
    if (viewer->saveDocumentAs(fileName)) {
        _mainWindow->statusBar()->showMessage(tr("Document saved to %1").arg(fileName), 5000);
    } else {
        QMessageBox::warning(
            _mainWindow,
            tr("Save Failed"),
            tr("Could not save the document to '%1'. See application log for details.").arg(fileName));
        _mainWindow->statusBar()->showMessage(tr("Save failed: %1").arg(fileName), 5000);
    }
}

void HomeController::onShare() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) return;
    
    QString filePath = viewer->filePath();
    QString fileName = QFileInfo(filePath).fileName();
    QString subject = tr("PDF Document: %1").arg(fileName);
    QString body = tr("Please find the attached PDF document.");

#ifdef Q_OS_WIN
    HMODULE hMapi = LoadLibraryA("mapi32.dll");
    if (hMapi) {
        LPMAPISENDMAIL pfnSendMail = (LPMAPISENDMAIL)GetProcAddress(hMapi, "MAPISendMail");
        if (pfnSendMail) {
            MapiFileDesc fileDesc;
            memset(&fileDesc, 0, sizeof(MapiFileDesc));
            fileDesc.nPosition = (ULONG)-1;
            
            QByteArray pathStr = QDir::toNativeSeparators(filePath).toLocal8Bit();
            QByteArray nameStr = fileName.toLocal8Bit();
            fileDesc.lpszPathName = pathStr.data();
            fileDesc.lpszFileName = nameStr.data();

            MapiMessage message;
            memset(&message, 0, sizeof(MapiMessage));
            
            QByteArray subjStr = subject.toLocal8Bit();
            QByteArray bodyStr = body.toLocal8Bit();
            message.lpszSubject = subjStr.data();
            message.lpszNoteText = bodyStr.data();
            message.nFileCount = 1;
            message.lpFiles = &fileDesc;

            pfnSendMail(0, reinterpret_cast<ULONG_PTR>(_mainWindow->winId()), &message, MAPI_DIALOG, 0);
        }
        FreeLibrary(hMapi);
        return;
    }
#endif

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

void HomeController::onPrintPreview() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !viewer->document()) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    QString filePath = viewer->filePath();
    auto ps = PageSetupDialog::loadForDocument(filePath);
    printer.setPageSize(QPageSize(ps.pageSize));
    printer.setPageOrientation(ps.orientation == QPageLayout::Landscape
                                   ? QPageLayout::Landscape : QPageLayout::Portrait);
    printer.setPageMargins(QMarginsF(ps.marginLeft, ps.marginTop,
                                     ps.marginRight, ps.marginBottom), QPageLayout::Millimeter);
    Q_UNUSED(ps);  // Settings applied to printer above; render uses fixed 150 DPI

    QPrintPreviewDialog preview(&printer, _mainWindow);
    preview.setWindowTitle(tr("Print Preview"));

    QPdfDocument* doc = viewer->document();
    connect(&preview, &QPrintPreviewDialog::paintRequested, _mainWindow,
        [doc](QPrinter* p) {
            if (!doc || doc->pageCount() == 0) return;

            QPainter painter(p);
            QRectF printable = p->pageRect(QPrinter::DevicePixel);
            constexpr int renderDpi = 150;

            for (int i = 0; i < doc->pageCount(); ++i) {
                if (i > 0) p->newPage();

                QSizeF pageSize = doc->pagePointSize(i);
                QImage img = doc->render(i, (pageSize * renderDpi / 72.0).toSize());
                if (img.isNull()) continue;

                // Scale to fit printable area
                QSizeF scaled = img.size().scaled(printable.size().toSize(), Qt::KeepAspectRatio);
                double x = (printable.width()  - scaled.width())  / 2.0;
                double y = (printable.height() - scaled.height()) / 2.0;
                painter.drawImage(QRectF(x, y, scaled.width(), scaled.height()), img);
            }
            painter.end();
        });

    preview.exec();
}

void HomeController::onPageSetup() {
    auto* viewer = _mainWindow->pdfViewer();
    QString path = viewer ? viewer->filePath() : QString();
    PageSetupDialog dlg(path, _mainWindow);
    dlg.exec();
}

void HomeController::onExportPresets() {
    ExportPresetsPanel panel(_mainWindow);
    panel.exec();
}

// ── Recent files ────────────────────────────────────────────────────────

void HomeController::addRecentFile(const QString& filePath) {
    if (filePath.isEmpty()) return;
    QSettings settings;
    QStringList recent = settings.value("recentFiles").toStringList();
    recent.removeAll(filePath);
    recent.prepend(filePath);
    while (recent.size() > 20)
        recent.removeLast();
    settings.setValue("recentFiles", recent);
}

QStringList HomeController::recentFiles() const {
    QSettings settings;
    return settings.value("recentFiles").toStringList();
}

} // namespace gp
