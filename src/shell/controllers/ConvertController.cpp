#include "ConvertController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "core/interfaces/IConversionEngine.h"
#include "core/interfaces/IPdfEditorEngine.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QProgressDialog>
#include <QThread>
#include <QPointer>
#include <QMetaObject>
#include <QCoreApplication>
#include <QFileInfo>
#include "shell/StatusBar.h"

namespace gp {

ConvertController::ConvertController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> ConvertController::handledTools() const {
    return {
        ToolId::Combine, ToolId::ToWord, ToolId::ToExcel, ToolId::ToCsv,
        ToolId::ToHtml, ToolId::ToText, ToolId::Compress,
        ToolId::Linearize, ToolId::PdfA
    };
}

void ConvertController::activate(ToolId id) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer && id != ToolId::Combine) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    switch (id) {
    case ToolId::Combine:
        mergePdfs();
        break;
    case ToolId::ToWord:
        exportToWord();
        break;
    case ToolId::ToExcel:
        exportToExcel();
        break;
    case ToolId::ToCsv:
        exportToCsv();
        break;
    case ToolId::ToHtml:
    case ToolId::ToText:
    case ToolId::Compress:
        QMessageBox::information(_mainWindow, tr("Conversion Engine"),
            tr("Advanced conversion and optimization tools are scheduled for the next engine update."));
        break;
    case ToolId::Linearize:
        linearizeDocument();
        break;
    case ToolId::PdfA:
        exportAsPdfA();
        break;
    default:
        break;
    }
}

void ConvertController::exportToWord() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to Word"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".docx",
        tr("Word Documents (*.docx)"));
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(tr("Converting to Word..."));

    auto* progress = new QProgressDialog(tr("Converting to Word..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, inputPath, outputPath, result]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::Word);
        result->store(ok);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1").arg(outputPath), 5000);
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"), tr("Export to Word complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to convert document to Word."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportToExcel() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to Excel"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".xlsx",
        tr("Excel Workbooks (*.xlsx)"));
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(tr("Converting to Excel..."));

    auto* progress = new QProgressDialog(tr("Converting to Excel..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, inputPath, outputPath, result]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::Excel);
        result->store(ok);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1").arg(outputPath), 5000);
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"), tr("Export to Excel complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to convert document to Excel."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportToCsv() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to CSV"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".csv",
        tr("CSV Files (*.csv)"));
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(tr("Extracting to CSV..."));

    auto* progress = new QProgressDialog(tr("Extracting to CSV..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, inputPath, outputPath, result]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::Csv);
        result->store(ok);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1").arg(outputPath), 5000);
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to extract data to CSV."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::mergePdfs() {
    QStringList files = QFileDialog::getOpenFileNames(_mainWindow, tr("Select PDFs to Merge"), "", tr("PDF Files (*.pdf)"));
    if (files.isEmpty()) return;
    QString outputFile = QFileDialog::getSaveFileName(_mainWindow, tr("Save Merged PDF"), "", tr("PDF Files (*.pdf)"));
    if (outputFile.isEmpty()) return;

    auto* progress = new QProgressDialog(tr("Merging documents..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);

    QPointer<ConvertController> self(this);

    QThread* worker = QThread::create([files, outputFile]() {
        PdfViewerWidget::mergeDocuments(files, outputFile);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, files, outputFile]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        self->_mainWindow->statusBar()->showMessage(
            QObject::tr("Successfully merged %1 files to %2").arg(files.size()).arg(outputFile), 5000);
        if (QMessageBox::question(self->_mainWindow, QObject::tr("Open Merged PDF"),
                QObject::tr("Merge complete. Would you like to open the output file?")) == QMessageBox::Yes) {
            self->_mainWindow->openDocument(outputFile);
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::linearizeDocument() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Save Linearized (Web-Optimized) PDF"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + "_optimized.pdf",
        tr("PDF Files (*.pdf)"));
    if (outputPath.isEmpty()) return;

    auto* progress = new QProgressDialog(tr("Linearizing document (Fast Web View)..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);

    const QString inputPath = viewer->filePath();
    IPdfEditorEngine* engine = _ctx->pdfEditor.get();
    QPointer<ConvertController> self(this);

    // Fix L: delete any pre-existing file at the target path so QFileInfo::exists
    // is a real success signal, not a leftover-file false positive.
    if (QFileInfo::exists(outputPath) && !QFile::remove(outputPath)) {
        progress->close();
        progress->deleteLater();
        QMessageBox::critical(_mainWindow, tr("Error"),
            tr("Could not overwrite existing file at: %1").arg(outputPath));
        return;
    }

    QThread* worker = QThread::create([engine, inputPath, outputPath]() {
        if (engine->currentFile() != inputPath)
            engine->loadDocumentForEditing(inputPath);
        engine->linearizeDocument(outputPath);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, engine, inputPath]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        if (QFileInfo::exists(outputPath)) {
            self->_mainWindow->statusBar()->showMessage(QObject::tr("Optimization complete: %1").arg(outputPath), 5000);
            if (QMessageBox::question(self->_mainWindow, QObject::tr("Linearization Success"),
                    QObject::tr("Linearization complete. Open file?")) == QMessageBox::Yes) {
                self->_mainWindow->openDocument(outputPath);
            }
        } else {
            QMessageBox::critical(self->_mainWindow, QObject::tr("Error"), QObject::tr("Failed to linearize document."));
            self->_mainWindow->statusBar()->showMessage(QObject::tr("Linearization failed."));
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportAsPdfA() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;
    QStringList levels;
    levels << tr("PDF/A-1b (ISO 19005-1)") << tr("PDF/A-2b (ISO 19005-2)") << tr("PDF/A-3b (ISO 19005-3)");
    bool ok;
    QString selected = QInputDialog::getItem(_mainWindow, tr("PDF/A Conformance Level"),
        tr("Select archival conformance level:"), levels, 0, false, &ok);
    if (!ok) return;

    int level = 1;
    if (selected.contains("2b")) level = 2;
    else if (selected.contains("3b")) level = 3;

    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export as PDF/A"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + "_pdfa.pdf",
        tr("PDF Files (*.pdf)"));
    if (outputPath.isEmpty()) return;

    auto* progress = new QProgressDialog(tr("Exporting as PDF/A..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);

    const QString inputPath = viewer->filePath();
    IPdfEditorEngine* engine = _ctx->pdfEditor.get();
    QPointer<ConvertController> self(this);

    // Fix L: delete any pre-existing file at the target path so QFileInfo::exists
    // is a real success signal, not a leftover-file false positive.
    if (QFileInfo::exists(outputPath) && !QFile::remove(outputPath)) {
        progress->close();
        progress->deleteLater();
        QMessageBox::critical(_mainWindow, tr("Error"),
            tr("Could not overwrite existing file at: %1").arg(outputPath));
        return;
    }

    QThread* worker = QThread::create([engine, inputPath, outputPath, level]() {
        engine->loadDocumentForEditing(inputPath);
        engine->exportPdfA(outputPath, level);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        if (QFileInfo::exists(outputPath)) {
            self->_mainWindow->statusBar()->showMessage(QObject::tr("PDF/A export complete: %1").arg(outputPath), 5000);
            if (QMessageBox::question(self->_mainWindow, QObject::tr("Export Success"),
                    QObject::tr("PDF/A export complete. Open file?")) == QMessageBox::Yes) {
                self->_mainWindow->openDocument(outputPath);
            }
        } else {
            QMessageBox::critical(self->_mainWindow, QObject::tr("Error"), QObject::tr("Failed to export as PDF/A."));
            self->_mainWindow->statusBar()->showMessage(QObject::tr("PDF/A export failed."));
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

} // namespace gp
