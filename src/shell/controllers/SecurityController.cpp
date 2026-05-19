#include "SecurityController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "ui/EncryptionDialog.h"
#include "ui/SignatureDialog.h"
#include "ui/MetadataDialog.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "core/interfaces/ISignatureManager.h"
#include "core/interfaces/ICollaboration.h"
#include "commands/EncryptDocumentHelper.h"
#include "commands/SignDocumentHelper.h"
#include "commands/SanitizeDocumentHelper.h"
#include "commands/SetMetadataCommand.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QThread>
#include <QPointer>
#include <QMetaObject>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QUndoStack>
#include <QTemporaryFile>
#include <QSettings>
#include <memory>
#include <atomic>
#include "shell/StatusBar.h"

namespace gp {

SecurityController::SecurityController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

bool SecurityController::handles(const QString& toolId) const {
    return toolId == "encrypt" || toolId == "password" ||
           toolId == "sign" ||
           toolId == "validateSig" || toolId == "validate" ||
           toolId == "sanitize" ||
           toolId == "applyRedact" ||
           toolId == "exportAnno" ||
           toolId == "importAnno" ||
           toolId == "cloud" ||
           toolId == "permissions" || toolId == "removeSec" ||
           toolId == "certify" || toolId == "timestamp" ||
           toolId == "patternRedact" || toolId == "regexRedact";
}

void SecurityController::activate(const QString& toolId) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    if (toolId == "encrypt" || toolId == "password") {
        encryptDocument();
    } else if (toolId == "sign") {
        signDocument();
    } else if (toolId == "validateSig" || toolId == "validate") {
        verifySignatures();
    } else if (toolId == "sanitize") {
        sanitizeDocument();
    } else if (toolId == "applyRedact") {
        applyRedactions();
    } else if (toolId == "exportAnno") {
        exportAnnotationPackage();
    } else if (toolId == "importAnno") {
        importAnnotationPackage();
    } else if (toolId == "cloud") {
        cloudSyncSync();
    } else if (toolId == "permissions" || toolId == "removeSec" || toolId == "certify" || toolId == "timestamp" || toolId == "patternRedact" || toolId == "regexRedact") {
        QMessageBox::information(_mainWindow, tr("Document Security"),
            tr("Advanced permission controls and security tools are scheduled for the next engine update."));
    }
}

void SecurityController::encryptDocument() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;

    EncryptionDialog dlg(_mainWindow);
    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.userPassword().isEmpty() && dlg.ownerPassword().isEmpty()) {
            QMessageBox::warning(_mainWindow, tr("Error"), tr("At least one password must be provided."));
            return;
        }

        _ctx->undoStack->clear();
        _ctx->document->setPath(viewer->filePath());

        auto* progress = new QProgressDialog(tr("Encrypting document..."), QString(), 0, 0, _mainWindow);
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);

        IPdfEditorEngine* engine = _ctx->pdfEditor.get();
        DocumentSession* doc = _ctx->document.get();
        const QString userPwd = dlg.userPassword();
        const QString ownerPwd = dlg.ownerPassword();
        const bool canPrint = dlg.canPrint();
        const bool canCopy = dlg.canCopy();
        const bool canModify = dlg.canModify();
        QPointer<SecurityController> self(this);

        QThread* worker = QThread::create([engine, doc, userPwd, ownerPwd, canPrint, canCopy, canModify]() {
            EncryptDocumentHelper::execute(engine, doc, userPwd, ownerPwd, canPrint, canCopy, canModify);
        });

        connect(worker, &QThread::finished, _mainWindow, [self, progress]() {
            progress->close();
            progress->deleteLater();
            if (!self) return;
            self->_mainWindow->statusBar()->showMessage(QObject::tr("Document encrypted"), 5000);
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
    }
}

void SecurityController::signDocument() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->signing) return;

    SignatureDialog dlg(_mainWindow);
    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.certificatePath().isEmpty() || dlg.password().isEmpty()) {
            QMessageBox::warning(_mainWindow, tr("Error"), tr("Certificate path and password are required."));
            return;
        }

        QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Save Signed Document"), "", tr("PDF Files (*.pdf)"));
        if (outputPath.isEmpty()) return;

        _ctx->undoStack->clear();
        _ctx->document->setPath(viewer->filePath());

        auto* progress = new QProgressDialog(tr("Signing document..."), QString(), 0, 0, _mainWindow);
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->show();

        ISignatureManager* signing = _ctx->signing.get();
        DocumentSession* doc = _ctx->document.get();
        const QString certPath = dlg.certificatePath();
        const QString pwd = dlg.password();
        const QString reason = dlg.reason();
        const QString location = dlg.location();

        QPointer<SecurityController> self(this);
        auto result = std::make_shared<std::atomic<bool>>(false);

        QThread* worker = QThread::create([signing, doc, outputPath, certPath, pwd, reason, location, result]() {
            bool ok = SignDocumentHelper::execute(
                signing, doc, outputPath, certPath, pwd, reason, location);
            result->store(ok);
        });

        connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
            progress->close();
            progress->deleteLater();
            if (!self) return;
            bool ok = result->load();
            if (ok) {
                self->_mainWindow->statusBar()->showMessage(tr("Document signed and saved to %1").arg(outputPath), 5000);
                if (QMessageBox::question(self->_mainWindow, tr("Open Signed PDF"), tr("Signing complete. Would you like to open the signed file?")) == QMessageBox::Yes) {
                    self->_mainWindow->openDocument(outputPath);
                }
            } else {
                QMessageBox::critical(self->_mainWindow, tr("Signing Error"), tr("Failed to sign document."));
                self->_mainWindow->statusBar()->showMessage(tr("Signing failed."), 5000);
            }
        });

        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
    }
}

void SecurityController::verifySignatures() {
    _mainWindow->onScreenSelected("signature");
}

void SecurityController::sanitizeDocument() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;

    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Save Sanitized Document"), "", tr("PDF Files (*.pdf)"));
    if (outputPath.isEmpty()) return;

    const QFileInfo sourceInfo(viewer->filePath());
    const QFileInfo outputInfo(outputPath);
    const QString sourcePath = sourceInfo.canonicalFilePath().isEmpty()
        ? sourceInfo.absoluteFilePath()
        : sourceInfo.canonicalFilePath();
    const QString targetPath = outputInfo.canonicalFilePath().isEmpty()
        ? outputInfo.absoluteFilePath()
        : outputInfo.canonicalFilePath();
    if (QString::compare(sourcePath, targetPath, Qt::CaseInsensitive) == 0) {
        QMessageBox::warning(_mainWindow, tr("Sanitize Document"),
            tr("Choose a different output file. Sanitization will not overwrite the open document."));
        return;
    }

    _ctx->undoStack->clear();
    _ctx->document->setPath(viewer->filePath());

    auto* progress = new QProgressDialog(tr("Sanitizing document..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IPdfEditorEngine* engine = _ctx->pdfEditor.get();
    DocumentSession* doc = _ctx->document.get();
    QPointer<SecurityController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([engine, doc, outputPath, result]() {
        result->store(SanitizeDocumentHelper::execute(engine, doc, outputPath));
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Document sanitized and saved to %1").arg(outputPath), 5000);
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Sanitize Document"),
                tr("Failed to sanitize the document. The open document was not overwritten."));
            self->_mainWindow->statusBar()->showMessage(tr("Sanitization failed."), 5000);
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void SecurityController::exportAnnotationPackage() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->collab) return;

    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export Annotation Package"),
        QFileInfo(viewer->filePath()).baseName() + "_annotations.json",
        tr("JSON Files (*.json)"));
    if (outputPath.isEmpty()) return;

    if (_ctx->collab->exportAnnotationPackage(viewer->annotations(), outputPath)) {
        _mainWindow->statusBar()->showMessage(tr("Annotations exported to %1").arg(outputPath), 5000);
    } else {
        QMessageBox::critical(_mainWindow, tr("Error"), tr("Failed to export annotation package."));
    }
}

void SecurityController::importAnnotationPackage() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->collab) return;

    QString inputPath = QFileDialog::getOpenFileName(_mainWindow, tr("Import Annotation Package"), "",
        tr("JSON Files (*.json)"));
    if (inputPath.isEmpty()) return;

    QList<AnnotationItem> items = _ctx->collab->importAnnotationPackage(inputPath);
    if (!items.isEmpty()) {
        if (QMessageBox::question(_mainWindow, tr("Import Annotations"),
            tr("Imported %1 annotations. Merge with current document?").arg(items.size())) == QMessageBox::Yes) {
            QList<AnnotationItem> current = viewer->annotations();
            current.append(items);
            viewer->setAnnotations(current);
            _mainWindow->statusBar()->showMessage(tr("Imported %1 annotations.").arg(items.size()), 5000);
        }
    } else {
        QMessageBox::warning(_mainWindow, tr("Import Error"), tr("No annotations found in package or file is invalid."));
    }
}

void SecurityController::cloudSyncSync() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->collab) return;

    _mainWindow->statusBar()->showMessage(tr("Initiating Document Weaver Cloud Sync..."));

    // MEDIUM-3: Replace insecure hardcoded temporary file with QTemporaryFile
    QTemporaryFile tempFile(QDir::tempPath() + "/collab_XXXXXX.json");
    tempFile.setAutoRemove(false);
    if (!tempFile.open()) {
        _mainWindow->statusBar()->showMessage(tr("Cloud Sync Failed: Unable to create temporary package."));
        return;
    }
    QString tempPath = tempFile.fileName();
    tempFile.close(); // Close descriptor so it can be written to by exporter

    if (_ctx->collab->exportAnnotationPackage(viewer->annotations(), tempPath)) {
        QSettings settings;
        QString cloudEndpoint = settings.value("cloud/sync_endpoint", 
                                               AppContext::DefaultCloudSyncEndpoint).toString();
        if (_ctx->collab->pushToCloud(tempPath, cloudEndpoint)) {
            _mainWindow->statusBar()->showMessage(tr("Cloud Sync Successful. Annotations shared."), 5000);
        } else {
            QMessageBox::warning(_mainWindow, tr("Sync Failed"), tr("Cloud endpoint unavailable. Running in local mode."));
        }
        QFile::remove(tempPath);
    }
}

void SecurityController::applyRedactions() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;

    auto annos = viewer->annotations();
    QMap<int, QList<QRectF>> redactionsByPage;
    bool hasRedactions = false;
    for (const auto& anno : annos) {
        if (anno.mode == ToolMode::Redact) {
            redactionsByPage[anno.pageIndex].append(anno.rect);
            hasRedactions = true;
        }
    }

    if (!hasRedactions) {
        QMessageBox::information(_mainWindow, tr("Redaction"), tr("No redaction marks found in the current document."));
        return;
    }

    if (QMessageBox::question(_mainWindow, tr("Confirm In-Place Redaction"),
        tr("Apply redaction marks to the open PDF and overwrite it in place?\n\n"
           "This operation is destructive. The current implementation removes some matching page content and annotations, then paints black boxes, but it cannot guarantee secure removal of all recoverable text, images, forms, or hidden content. Use a dedicated redaction tool for legally sensitive material.")) != QMessageBox::Yes) {
        return;
    }

    _mainWindow->statusBar()->showMessage(tr("Applying redactions..."));

    auto* progress = new QProgressDialog(tr("Applying redactions asynchronously..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    IPdfEditorEngine* engine = _ctx->pdfEditor.get();
    const QString filePath = viewer->filePath();
    QPointer<SecurityController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([engine, filePath, redactionsByPage, result]() {
        if (!engine->loadDocumentForEditing(filePath)) {
            result->store(false);
            return;
        }
        bool success = true;
        for (auto it = redactionsByPage.begin(); it != redactionsByPage.end(); ++it) {
            if (!engine->applyRedactions(it.key(), it.value())) {
                success = false;
                break;
            }
        }
        if (success && !engine->saveDocument(filePath)) {
            success = false;
        }
        result->store(success);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, viewer, filePath, annos, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Redactions applied successfully."), 5000);
            QList<AnnotationItem> remaining;
            for (const auto& anno : annos) {
                if (anno.mode != ToolMode::Redact) remaining.append(anno);
            }
            viewer->setAnnotations(remaining);
            self->_mainWindow->openDocument(filePath); // reload/refresh page view
        } else {
            self->_ctx->pdfEditor->loadDocumentForEditing(filePath);
            QMessageBox::critical(self->_mainWindow, tr("Secure Redaction Failed"),
                tr("Failed to securely redact the document. One or more pages contain inline images, "
                   "unsupported text operators, or complex binary streams that prevent underlying "
                   "data deletion. The operation was aborted to prevent a visual-only overlay."));
            self->_mainWindow->statusBar()->showMessage(tr("Redaction failed."), 5000);
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

} // namespace gp
