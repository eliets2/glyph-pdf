// SPDX-License-Identifier: Apache-2.0
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
#include <atomic>
#include <QFileInfo>
#include "shell/StatusBar.h"
#include "engines/ConversionManager.h"
#include "modes/CompressDialog.h"

namespace gp {

ConvertController::ConvertController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> ConvertController::handledTools() const {
    return {
        ToolId::Combine, ToolId::ToWord, ToolId::ToExcel, ToolId::ToCsv,
        ToolId::ToHtml, ToolId::ToText, ToolId::Compress,
        ToolId::ToPPT, ToolId::ToImage, ToolId::Linearize, ToolId::PdfA
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
        exportToHtml();
        break;
    case ToolId::ToText:
        exportToText();
        break;
    case ToolId::ToPPT:
        exportToPowerPoint();
        break;
    case ToolId::ToImage:
        exportToImage();
        break;
    case ToolId::Compress:
        openCompressDialog();
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
    auto* convMgr = dynamic_cast<ConversionManager*>(conv);
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);
    auto fallback = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, convMgr, inputPath, outputPath, result, fallback]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::Word);
        result->store(ok);
        // §9.16 P0: detect whether the real OOXML writer or the mislabeled
        // HTML fallback produced this file.
        if (convMgr && ok)
            fallback->store(convMgr->lastWordExportEngine() == ConversionManager::ExportEngine::Fallback);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result, fallback]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1").arg(outputPath), 5000);
            if (fallback->load()) {
                // Honest disclosure: the .docx is actually HTML bytes.
                QMessageBox::warning(self->_mainWindow, tr("Export Format Notice"),
                    tr("This build lacks the native Word (OOXML) writer, so the exported file\n%1\n"
                       "contains HTML content under a .docx extension. Word may show a repair prompt.\n\n"
                       "For best results, choose HTML export instead.").arg(outputPath));
            }
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
    auto* convMgr = dynamic_cast<ConversionManager*>(conv);
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);
    auto fallback = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, convMgr, inputPath, outputPath, result, fallback]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::Excel);
        result->store(ok);
        // §9.16 P0: detect whether the real OOXML writer or the mislabeled
        // CSV fallback produced this file.
        if (convMgr && ok)
            fallback->store(convMgr->lastExcelExportEngine() == ConversionManager::ExportEngine::Fallback);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result, fallback]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1").arg(outputPath), 5000);
            if (fallback->load()) {
                QMessageBox::warning(self->_mainWindow, tr("Export Format Notice"),
                    tr("This build lacks the native Excel (OOXML) writer, so the exported file\n%1\n"
                       "contains CSV content under a .xlsx extension. Excel may show a repair prompt.\n\n"
                       "For best results, choose CSV export instead.").arg(outputPath));
            }
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
    auto ok = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([files, outputFile, ok]() {
        ok->store(PdfViewerWidget::mergeDocuments(files, outputFile));
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, files, outputFile, ok]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        if (!ok->load()) {
            QMessageBox::critical(self->_mainWindow, QObject::tr("Merge Failed"),
                QObject::tr("Merging %1 files failed. The output file was not written (or is incomplete).\n\n"
                            "Check that the input files are valid PDFs and the output location is writable.")
                    .arg(files.size()));
            self->_mainWindow->statusBar()->showMessage(QObject::tr("Merge failed."), 5000);
            return;
        }
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


void ConvertController::exportToHtml() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to HTML"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".html",
        tr("HTML Files (*.html)"));
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(tr("Converting to HTML..."));

    auto* progress = new QProgressDialog(tr("Converting to HTML..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, inputPath, outputPath, result]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::Html);
        result->store(ok);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1").arg(outputPath), 5000);
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"), tr("Export to HTML complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to convert document to HTML."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportToText() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to Text"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".txt",
        tr("Text Files (*.txt)"));
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(tr("Converting to Text..."));

    auto* progress = new QProgressDialog(tr("Converting to Text..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, inputPath, outputPath, result]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::Text);
        result->store(ok);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1").arg(outputPath), 5000);
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"), tr("Export to Text complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to convert document to Text."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportToPowerPoint() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to PowerPoint"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".pptx",
        tr("PowerPoint Presentations (*.pptx)"));
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(tr("Converting to PowerPoint..."));

    auto* progress = new QProgressDialog(tr("Converting to PowerPoint..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, inputPath, outputPath, result]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::PowerPoint);
        result->store(ok);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1").arg(outputPath), 5000);
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"), tr("Export to PowerPoint complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to convert document to PowerPoint."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::exportToImage() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->conversion) return;
    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export to Image"),
        QFileInfo(viewer->filePath()).path() + "/" + QFileInfo(viewer->filePath()).baseName() + ".png",
        tr("PNG Images (*.png);;JPEG Images (*.jpg);;TIFF Images (*.tif)"));
    if (outputPath.isEmpty()) return;

    _mainWindow->statusBar()->showMessage(tr("Exporting to image..."));

    auto* progress = new QProgressDialog(tr("Exporting to image..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IConversionEngine* conv = _ctx->conversion.get();
    const QString inputPath = viewer->filePath();
    QPointer<ConvertController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([conv, inputPath, outputPath, result]() {
        bool ok = conv->convertTo(inputPath, outputPath, IConversionEngine::TargetFormat::Image);
        result->store(ok);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Export complete: %1").arg(outputPath), 5000);
            if (QMessageBox::question(self->_mainWindow, tr("Export Success"), tr("Export to image complete. Open file?")) == QMessageBox::Yes) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath));
            }
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Export Error"), tr("Failed to export document to image."));
            self->_mainWindow->statusBar()->showMessage(tr("Export failed."));
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void ConvertController::openCompressDialog() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;
    
    CompressDialog dialog(_ctx, _mainWindow);
    dialog.exec();
}

} // namespace gp
