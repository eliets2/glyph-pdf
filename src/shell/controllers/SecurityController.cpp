// SPDX-License-Identifier: Apache-2.0
#include "SecurityController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "modes/RedactMode.h"
#include "ui/PdfViewerWidget.h"
#include "ui/EncryptionDialog.h"
#include "ui/PermissionsDialog.h"
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
#include <QInputDialog>
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

QList<ToolId> SecurityController::handledTools() const {
    return {
        ToolId::Encrypt, ToolId::Password, ToolId::Sign,
        ToolId::ValidateSig, ToolId::Sanitize, ToolId::ApplyRedact,
        ToolId::ExportAnno, ToolId::ImportAnno, ToolId::Cloud,
        ToolId::Permissions, ToolId::RemoveSecurity, ToolId::Certify,
        ToolId::Timestamp, ToolId::PatternRedact, ToolId::RegexRedact
    };
}

void SecurityController::activate(ToolId id) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
        return;
    }

    switch (id) {
    case ToolId::Encrypt:
    case ToolId::Password:
        encryptDocument();
        break;
    case ToolId::Sign:
        signDocument();
        break;
    case ToolId::ValidateSig:
        verifySignatures();
        break;
    case ToolId::Sanitize:
        sanitizeDocument();
        break;
    case ToolId::ApplyRedact:
        applyRedactions();
        break;
    case ToolId::ExportAnno:
        exportAnnotationPackage();
        break;
    case ToolId::ImportAnno:
        importAnnotationPackage();
        break;
    case ToolId::Cloud:
        cloudSyncSync();
        break;
    case ToolId::Permissions:
        permissionsDocument();
        break;
    case ToolId::RemoveSecurity:
        removeSecurity();
        break;
    case ToolId::Certify:
        certifyDocument();
        break;
    case ToolId::Timestamp:
        timestampDocument();
        break;
    case ToolId::PatternRedact:
        // Switch to Redact mode — the RedactMode panel provides full pattern UI
        _mainWindow->activateScreen(QStringLiteral("redact"));
        break;
    case ToolId::RegexRedact:
        // Switch to Redact mode pre-selecting the Custom regex option
        _mainWindow->activateScreen(QStringLiteral("redact"));
        // After the mode is activated, find the live RedactMode and pre-select Custom
        if (auto* redactWidget = _mainWindow->findChild<gp::RedactMode*>()) {
            redactWidget->activateCustomRegex();
        }
        break;
    default:
        break;
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
        DocumentPermissions perms;
        perms.print = dlg.canPrint();
        perms.copy = dlg.canCopy();
        perms.modify = dlg.canModify();
        
        QPointer<SecurityController> self(this);

        QThread* worker = QThread::create([engine, doc, userPwd, ownerPwd, perms]() {
            EncryptDocumentHelper::execute(engine, doc, userPwd, ownerPwd, perms);
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
    // v1.0.0: Cloud Sync backend is a stub (see CollaborationManager::pushToCloud
    // "Cloud Sync Stub (Simulation)"). Do NOT pretend it works — inform the user
    // and bail out before any work is done. The stub flow below is preserved
    // (compiled out) so it can be reinstated when a real backend lands in v1.1+.
    QMessageBox::information(
        _mainWindow,
        tr("Cloud Sync"),
        tr("Cloud Sync is not available in v1.0.0. This feature is scheduled for a later release."));

#if 0
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->collab) return;

    _mainWindow->statusBar()->showMessage(tr("Initiating Document Weaver Cloud Sync..."));

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
#endif
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

void SecurityController::permissionsDocument() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;

    PermissionsDialog dlg(_mainWindow);
    if (dlg.exec() == QDialog::Accepted) {
        _ctx->undoStack->clear();
        _ctx->document->setPath(viewer->filePath());

        auto* progress = new QProgressDialog(tr("Encrypting document..."), QString(), 0, 0, _mainWindow);
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);

        IPdfEditorEngine* engine = _ctx->pdfEditor.get();
        DocumentSession* doc = _ctx->document.get();
        const QString userPwd = dlg.userPassword();
        const QString ownerPwd = dlg.ownerPassword();
        const DocumentPermissions perms = dlg.permissions();
        
        QPointer<SecurityController> self(this);

        QThread* worker = QThread::create([engine, doc, userPwd, ownerPwd, perms]() {
            EncryptDocumentHelper::execute(engine, doc, userPwd, ownerPwd, perms);
        });

        connect(worker, &QThread::finished, _mainWindow, [self, progress]() {
            progress->close();
            progress->deleteLater();
            if (!self) return;
            self->_mainWindow->statusBar()->showMessage(QObject::tr("Document permissions updated"), 5000);
        });
        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
    }
}

void SecurityController::removeSecurity() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;

    bool ok;
    QString pwd = QInputDialog::getText(_mainWindow, tr("Remove Security"),
                                        tr("Enter owner password to remove security:"),
                                        QLineEdit::Password,
                                        "", &ok);
    if (!ok) return;

    IPdfEditorEngine* engine = _ctx->pdfEditor.get();
    if (!engine->loadDocumentForEditing(viewer->filePath())) {
        QMessageBox::warning(_mainWindow, tr("Remove Security"), tr("Failed to open the document for editing."));
        return;
    }
    if (engine->removeEncryption(pwd)) {
        engine->saveDocument(viewer->filePath());
        viewer->reload();
        _mainWindow->statusBar()->showMessage(tr("Document security removed."), 5000);
    } else {
        QMessageBox::warning(_mainWindow, tr("Remove Security"), tr("Failed to remove security. Incorrect password?"));
    }
}

void SecurityController::certifyDocument() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->signing) return;

    SignatureDialog dlg(_mainWindow);
    dlg.setWindowTitle(tr("Certify Document"));
    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.certificatePath().isEmpty() || dlg.password().isEmpty()) {
            QMessageBox::warning(_mainWindow, tr("Error"), tr("Certificate path and password are required."));
            return;
        }

        QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Save Certified Document"), "", tr("PDF Files (*.pdf)"));
        if (outputPath.isEmpty()) return;

        _ctx->undoStack->clear();
        _ctx->document->setPath(viewer->filePath());

        auto* progress = new QProgressDialog(tr("Certifying document..."), QString(), 0, 0, _mainWindow);
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->show();

        ISignatureManager* signing = _ctx->signing.get();
        DocumentSession* doc = _ctx->document.get();
        const QString certPath = dlg.certificatePath();
        const QString pwd = dlg.password();
        const QString reason = dlg.reason();
        const QString location = dlg.location();
        // Just hardcode level 1 (no changes allowed) for now since UI doesn't expose it
        int certLevel = 1;

        QPointer<SecurityController> self(this);
        auto result = std::make_shared<std::atomic<bool>>(false);

        QThread* worker = QThread::create([signing, doc, outputPath, certPath, pwd, certLevel, reason, location, result]() {
            bool ok = signing->certifyDocument(doc->path(), outputPath, certPath, pwd, certLevel, reason, location);
            if (ok) doc->markReload();
            result->store(ok);
        });

        connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
            progress->close();
            progress->deleteLater();
            if (!self) return;
            bool ok = result->load();
            if (ok) {
                self->_mainWindow->statusBar()->showMessage(tr("Document certified and saved to %1").arg(outputPath), 5000);
                if (QMessageBox::question(self->_mainWindow, tr("Open Certified PDF"), tr("Certification complete. Would you like to open the certified file?")) == QMessageBox::Yes) {
                    self->_mainWindow->openDocument(outputPath);
                }
            } else {
                QMessageBox::critical(self->_mainWindow, tr("Certification Error"), tr("Failed to certify document."));
                self->_mainWindow->statusBar()->showMessage(tr("Certification failed."), 5000);
            }
        });

        connect(worker, &QThread::finished, worker, &QObject::deleteLater);
        worker->start();
    }
}

void SecurityController::timestampDocument() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->signing) return;

    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Save Timestamped Document"), "", tr("PDF Files (*.pdf)"));
    if (outputPath.isEmpty()) return;

    auto* progress = new QProgressDialog(tr("Timestamping document..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    ISignatureManager* signing = _ctx->signing.get();
    DocumentSession* doc = _ctx->document.get();

    QPointer<SecurityController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([signing, doc, outputPath, result]() {
        bool ok = signing->addDocTimeStamp(doc->path(), outputPath);
        if (ok) doc->markReload();
        result->store(ok);
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        bool ok = result->load();
        if (ok) {
            self->_mainWindow->statusBar()->showMessage(tr("Document timestamped and saved to %1").arg(outputPath), 5000);
            if (QMessageBox::question(self->_mainWindow, tr("Open Timestamped PDF"), tr("Timestamping complete. Would you like to open the file?")) == QMessageBox::Yes) {
                self->_mainWindow->openDocument(outputPath);
            }
        } else {
            QMessageBox::critical(self->_mainWindow, tr("Timestamp Error"), tr("Failed to add document timestamp."));
            self->_mainWindow->statusBar()->showMessage(tr("Timestamp failed."), 5000);
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

} // namespace gp
