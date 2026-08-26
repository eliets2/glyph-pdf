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
#include "commands/EncryptDocumentHelper.h"
#include "commands/SignDocumentHelper.h"
#include "commands/SanitizeDocumentHelper.h"
#include "commands/SetMetadataCommand.h"
#include "core/AnnotationSerializer.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include "engines/PdfEditorEngine.h"
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
        ToolId::ExportAnno, ToolId::ImportAnno,
        ToolId::Permissions, ToolId::RemoveSecurity, ToolId::Certify,
        ToolId::Timestamp, ToolId::PatternRedact, ToolId::RegexRedact,
        ToolId::ExpiryDate
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
    case ToolId::ExpiryDate:
        setExpiryDocument();
        break;
    default:
        break;
    }
}

void SecurityController::setExpiryDocument() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;
    // §9.11 upgrade path: setExpiryDate lives on the concrete engine only;
    // promote it to IPdfEditorEngine when a second caller appears.
    auto* engine = dynamic_cast<PdfEditorEngine*>(_ctx->pdfEditor.get());
    if (!engine) {
        QMessageBox::warning(_mainWindow, tr("Document Expiry"),
            tr("Expiry dates are not supported by this engine."));
        return;
    }

    QDialog dlg(_mainWindow);
    dlg.setWindowTitle(tr("Set Expiry Date"));
    auto* lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel(tr("After this date the document opens in read-only mode:"), &dlg));
    auto* picker = new QDateEdit(QDate::currentDate().addMonths(1), &dlg);
    picker->setCalendarPopup(true);
    picker->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    lay->addWidget(picker);
    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(btns);
    if (dlg.exec() != QDialog::Accepted) return;
    const QDate date = picker->date();

    const QString inputPath = viewer->filePath();
    if (engine->setExpiryDate(inputPath, date, inputPath)) {
        viewer->loadDocument(inputPath);
        _mainWindow->statusBar()->showMessage(
            tr("Document expiry set to %1").arg(date.toString(Qt::ISODate)), 5000);
    } else {
        QMessageBox::critical(_mainWindow, tr("Document Expiry"),
            tr("Failed to write the expiry date into the document."));
    }
}

void SecurityController::encryptDocument() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer || !_ctx || !_ctx->pdfEditor) return;

    // ER-3: Re-encrypting a multi-recipient document changes the session key,
    // locking out other recipients whose envelopes were computed against the original FEK.
    if (_ctx->pdfEditor->recipientCount() > 1) {
        auto choice = QMessageBox::warning(
            _mainWindow, tr("Multi-Recipient Document"),
            tr("This document is encrypted for multiple recipients (%1).\n\n"
               "Re-encrypting in place will change the session key. "
               "Other recipients will no longer be able to open the document "
               "unless their access is re-granted.\n\n"
               "Continue? All recipients must be re-specified after saving.")
                .arg(_ctx->pdfEditor->recipientCount()),
            QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
        if (choice != QMessageBox::Ok) return;
    }

    EncryptionDialog dlg(_mainWindow);
    if (dlg.exec() == QDialog::Accepted) {
        if (dlg.userPassword().isEmpty() && dlg.ownerPassword().isEmpty()) {
            QMessageBox::warning(_mainWindow, tr("Error"), tr("At least one password must be provided."));
            return;
        }

        _ctx->undoStack->clear();
        _ctx->document->setPath(viewer->filePath());

        // AR-7 D3: show the dialog immediately with a real Cancel button.
        auto* progress = new QProgressDialog(tr("Encrypting document..."), tr("Cancel"), 0, 0, _mainWindow);
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->show();

        std::weak_ptr<IPdfEditorEngine> weakEngine = _ctx->pdfEditor;
        std::weak_ptr<DocumentSession> weakDoc = _ctx->document;
        const QString userPwd = dlg.userPassword();
        const QString ownerPwd = dlg.ownerPassword();
        DocumentPermissions perms;
        perms.print = dlg.canPrint();
        perms.copy = dlg.canCopy();
        perms.modify = dlg.canModify();

        QPointer<SecurityController> self(this);
        auto result = std::make_shared<std::atomic<bool>>(false);

        QThread* worker = QThread::create([weakEngine, weakDoc, userPwd, ownerPwd, perms, result]() {
            auto engine = weakEngine.lock();
            auto doc = weakDoc.lock();
            if (!engine || !doc) return;
            try {
                result->store(EncryptDocumentHelper::execute(engine.get(), doc.get(), userPwd, ownerPwd, perms));
            } catch (const std::exception& e) {
                qCritical() << "Encryption worker thread threw:" << e.what();
                result->store(false);
            } catch (...) {
                qCritical() << "Encryption worker thread threw an unknown exception";
                result->store(false);
            }
        });

        connect(worker, &QThread::finished, _mainWindow, [self, progress, result]() {
            progress->close();
            progress->deleteLater();
            if (!self) return;
            if (result->load()) {
                self->_mainWindow->statusBar()->showMessage(QObject::tr("Document encrypted"), 5000);
            } else {
                QMessageBox::critical(self->_mainWindow, tr("Encryption Failed"),
                    tr("The document could not be encrypted and saved. The original file "
                       "was left unchanged. Check that the file is not read-only and the "
                       "disk is not full."));
                self->_mainWindow->statusBar()->showMessage(tr("Encryption failed."), 5000);
            }
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

        std::weak_ptr<ISignatureManager> weakSigning = _ctx->signing;
        std::weak_ptr<DocumentSession> weakDoc = _ctx->document;
        const QString certPath = dlg.certificatePath();
        const QString pwd = dlg.password();
        const QString reason = dlg.reason();
        const QString location = dlg.location();

        QPointer<SecurityController> self(this);
        auto result = std::make_shared<std::atomic<int>>(static_cast<int>(SignOutcome::NotRun));

        QThread* worker = QThread::create([weakSigning, weakDoc, outputPath, certPath, pwd, reason, location, result]() {
            auto signing = weakSigning.lock();
            auto doc = weakDoc.lock();
            if (!signing || !doc) return;
            SignOutcome ok = SignDocumentHelper::execute(
                signing.get(), doc.get(), outputPath, certPath, pwd, reason, location);
            result->store(static_cast<int>(ok));
        });

        connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
            progress->close();
            progress->deleteLater();
            if (!self) return;
            const auto sigOutcome = static_cast<SignOutcome>(result->load());
            if (sigOutcome == SignOutcome::Success) {
                self->_mainWindow->statusBar()->showMessage(tr("Document signed and saved to %1").arg(outputPath), 5000);
                if (QMessageBox::question(self->_mainWindow, tr("Open Signed PDF"), tr("Signing complete. Would you like to open the signed file?")) == QMessageBox::Yes) {
                    self->_mainWindow->openDocument(outputPath);
                }
            } else if (sigOutcome == SignOutcome::PartialLtvMissing) {
                // E-02: the signature bytes ARE on disk — do NOT tell the user signing
                // failed (which would make them discard a validly-signed file). Warn
                // that only the long-term-validation enhancement could not be added.
                QMessageBox::warning(self->_mainWindow, tr("Signature Applied — Long-Term Validation Incomplete"),
                    tr("The document was signed and saved to %1, but the long-term "
                       "validation data (DSS / archive timestamp) could not be embedded. "
                       "The signature is valid now; please verify the TSA/OCSP "
                       "configuration if you require B-LT/B-LTA archival assurances.")
                        .arg(outputPath));
                self->_mainWindow->statusBar()->showMessage(tr("Signed (long-term validation data missing)."), 5000);
                if (QMessageBox::question(self->_mainWindow, tr("Open Signed PDF"), tr("Would you like to open the signed file?")) == QMessageBox::Yes) {
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

    std::weak_ptr<IPdfEditorEngine> weakEngine = _ctx->pdfEditor;
    std::weak_ptr<DocumentSession> weakDoc = _ctx->document;
    QPointer<SecurityController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([weakEngine, weakDoc, outputPath, result]() {
        auto engine = weakEngine.lock();
        auto doc = weakDoc.lock();
        if (!engine || !doc) return;
        result->store(SanitizeDocumentHelper::execute(engine.get(), doc.get(), outputPath));
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
    if (!viewer) return;

    QString outputPath = QFileDialog::getSaveFileName(_mainWindow, tr("Export Annotation Package"),
        QFileInfo(viewer->filePath()).baseName() + "_annotations.json",
        tr("JSON Files (*.json)"));
    if (outputPath.isEmpty()) return;

    QJsonObject root;
    root["version"]     = "1.0";
    root["exported_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["source"]      = "GlyphPDF";
    root["annotations"] = AnnotationSerializer::toJson(viewer->annotations()).array();

    QFile file(outputPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
        _mainWindow->statusBar()->showMessage(tr("Annotations exported to %1").arg(outputPath), 5000);
    } else {
        QMessageBox::critical(_mainWindow, tr("Error"), tr("Failed to export annotation package."));
    }
}

void SecurityController::importAnnotationPackage() {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) return;

    QString inputPath = QFileDialog::getOpenFileName(_mainWindow, tr("Import Annotation Package"), "",
        tr("JSON Files (*.json)"));
    if (inputPath.isEmpty()) return;

    QFile file(inputPath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(_mainWindow, tr("Import Error"), tr("Could not open the annotation package file."));
        return;
    }
    constexpr qint64 MaxPackageBytes = 50 * 1024 * 1024;
    if (file.size() > MaxPackageBytes) {
        QMessageBox::warning(_mainWindow, tr("Import Error"), tr("Annotation package exceeds the 50 MB size limit."));
        return;
    }
    QList<AnnotationItem> items = AnnotationSerializer::fromJson(QJsonDocument::fromJson(file.readAll()));
    file.close();

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

    // ER-2: Redacting a signed document via incremental save leaks excised bytes into
    // revision 1. Block in-place redaction and require the user to save an unsigned copy.
    if (_ctx->pdfEditor->hasPdfSignatures()) {
        QMessageBox::critical(
            _mainWindow, tr("Cannot Redact Signed Document"),
            tr("This document is digitally signed.\n\n"
               "Applying redactions and saving in place would leave the original "
               "content recoverable from the PDF revision history.\n\n"
               "To redact permanently:\n"
               "1. File > Save As — save an unsigned copy.\n"
               "2. Open the copy and apply redactions."));
        return;
    }

    if (QMessageBox::question(_mainWindow, tr("Confirm Redaction"),
        tr("Apply redaction marks to the open PDF?\n\n"
           "This operation permanently excises matched content from content streams, "
           "removes associated metadata (annotations, structure tree text, form data), "
           "sanitizes the document, and saves to a new file.\n\n"
           "The original file is preserved unmodified. This action cannot be undone.")) != QMessageBox::Yes) {
        return;
    }

    _mainWindow->statusBar()->showMessage(tr("Applying redactions..."));

    // AR-7 D3: plain user-facing label; redaction cannot be safely interrupted
    // mid-stream (it modifies content streams atomically), so no Cancel is offered.
    auto* progress = new QProgressDialog(tr("Applying redactions..."), QString(), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    std::weak_ptr<IPdfEditorEngine> weakEngine = _ctx->pdfEditor;
    const QString filePath = viewer->filePath();
    QPointer<SecurityController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([weakEngine, filePath, redactionsByPage, result]() {
        auto engine = weakEngine.lock();
        if (!engine) return;
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

    // ER-3: Re-encrypting a multi-recipient document changes the session key,
    // locking out other recipients whose envelopes were computed against the original FEK.
    if (_ctx->pdfEditor->recipientCount() > 1) {
        auto choice = QMessageBox::warning(
            _mainWindow, tr("Multi-Recipient Document"),
            tr("This document is encrypted for multiple recipients (%1).\n\n"
               "Re-encrypting in place will change the session key. "
               "Other recipients will no longer be able to open the document "
               "unless their access is re-granted.\n\n"
               "Continue? All recipients must be re-specified after saving.")
                .arg(_ctx->pdfEditor->recipientCount()),
            QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
        if (choice != QMessageBox::Ok) return;
    }

    PermissionsDialog dlg(_mainWindow);
    if (dlg.exec() == QDialog::Accepted) {
        _ctx->undoStack->clear();
        _ctx->document->setPath(viewer->filePath());

        // AR-7 D3: show the dialog immediately with a real Cancel button.
        auto* progress = new QProgressDialog(tr("Updating document permissions..."), tr("Cancel"), 0, 0, _mainWindow);
        progress->setWindowModality(Qt::WindowModal);
        progress->setMinimumDuration(0);
        progress->show();

        std::weak_ptr<IPdfEditorEngine> weakEngine = _ctx->pdfEditor;
        std::weak_ptr<DocumentSession> weakDoc = _ctx->document;
        const QString userPwd = dlg.userPassword();
        const QString ownerPwd = dlg.ownerPassword();
        const DocumentPermissions perms = dlg.permissions();

        QPointer<SecurityController> self(this);
        auto result = std::make_shared<std::atomic<bool>>(false);

        QThread* worker = QThread::create([weakEngine, weakDoc, userPwd, ownerPwd, perms, result]() {
            auto engine = weakEngine.lock();
            auto doc = weakDoc.lock();
            if (!engine || !doc) return;
            try {
                result->store(EncryptDocumentHelper::execute(engine.get(), doc.get(), userPwd, ownerPwd, perms));
            } catch (const std::exception& e) {
                qCritical() << "permissionsDocument worker thread threw:" << e.what();
                result->store(false);
            } catch (...) {
                qCritical() << "permissionsDocument worker thread threw an unknown exception";
                result->store(false);
            }
        });

        connect(worker, &QThread::finished, _mainWindow, [self, progress, result]() {
            progress->close();
            progress->deleteLater();
            if (!self) return;
            if (result->load()) {
                self->_mainWindow->statusBar()->showMessage(QObject::tr("Document permissions updated"), 5000);
            } else {
                QMessageBox::critical(self->_mainWindow, tr("Permissions Update Failed"),
                    tr("The document permissions could not be saved. The original file "
                       "was left unchanged. Check that the file is not read-only and the "
                       "disk is not full."));
                self->_mainWindow->statusBar()->showMessage(tr("Permissions update failed."), 5000);
            }
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
        // UX-04: check the saveDocument return value. If the write fails (disk
        // full, file locked, etc.) the file on disk is still encrypted. Do NOT
        // reload the viewer or claim "security removed" — that would leave the
        // UI showing an unlocked document while the file on disk is still locked.
        if (!engine->saveDocument(viewer->filePath())) {
            QMessageBox::critical(_mainWindow, tr("Remove Security"),
                tr("The security settings were removed in memory, but the file could "
                   "not be saved. The file on disk is unchanged. Check that the file "
                   "is not read-only and the disk is not full."));
            _mainWindow->statusBar()->showMessage(tr("Remove security: save failed."), 5000);
            return;
        }
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

        std::weak_ptr<ISignatureManager> weakSigning = _ctx->signing;
        std::weak_ptr<DocumentSession> weakDoc = _ctx->document;
        const QString certPath = dlg.certificatePath();
        const QString pwd = dlg.password();
        const QString reason = dlg.reason();
        const QString location = dlg.location();
        // Just hardcode level 1 (no changes allowed) for now since UI doesn't expose it
        int certLevel = 1;

        QPointer<SecurityController> self(this);
        auto result = std::make_shared<std::atomic<int>>(static_cast<int>(SignOutcome::NotRun));

        QThread* worker = QThread::create([weakSigning, weakDoc, outputPath, certPath, pwd, certLevel, reason, location, result]() {
            auto signing = weakSigning.lock();
            auto doc = weakDoc.lock();
            if (!signing || !doc) return;
            SignOutcome outcome = signing->certifyDocument(doc->path(), outputPath, certPath, pwd, certLevel, reason, location);
            if (outcome == SignOutcome::Success || outcome == SignOutcome::PartialLtvMissing) doc->markReload();
            result->store(static_cast<int>(outcome));
        });

        connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
            progress->close();
            progress->deleteLater();
            if (!self) return;
            const auto certOutcome = static_cast<SignOutcome>(result->load());
            if (certOutcome == SignOutcome::Success) {
                self->_mainWindow->statusBar()->showMessage(tr("Document certified and saved to %1").arg(outputPath), 5000);
                if (QMessageBox::question(self->_mainWindow, tr("Open Certified PDF"), tr("Certification complete. Would you like to open the certified file?")) == QMessageBox::Yes) {
                    self->_mainWindow->openDocument(outputPath);
                }
            } else if (certOutcome == SignOutcome::PartialLtvMissing) {
                QMessageBox::warning(self->_mainWindow, tr("Certified — Long-Term Validation Incomplete"),
                    tr("The document was certified and saved to %1, but long-term validation data could not be embedded.").arg(outputPath));
                self->_mainWindow->statusBar()->showMessage(tr("Certified (LTV data missing)."), 5000);
                if (QMessageBox::question(self->_mainWindow, tr("Open Certified PDF"), tr("Would you like to open the certified file?")) == QMessageBox::Yes) {
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

    std::weak_ptr<ISignatureManager> weakSigning = _ctx->signing;
    std::weak_ptr<DocumentSession> weakDoc = _ctx->document;

    QPointer<SecurityController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([weakSigning, weakDoc, outputPath, result]() {
        auto signing = weakSigning.lock();
        auto doc = weakDoc.lock();
        if (!signing || !doc) return;
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
