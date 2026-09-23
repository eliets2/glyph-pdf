// SPDX-License-Identifier: Apache-2.0
// N17 — certificate-encryption controller. See CertEncryptController.h.
#include "CertEncryptController.h"

#include "core/AppContext.h"
#include "core/ErrorInfo.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"
#include "shell/EditPolicy.h"
#include "GpMainWindow.h"
#include "ui/RecipientPickerDialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QThread>
#include <QUndoStack>

#include "shell/StatusBar.h"

namespace gp {

CertEncryptController::CertEncryptController(const AppContext* ctx, MainWindow* mainWindow,
                                             QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> CertEncryptController::handledTools() const
{
    return { ToolId::CertEncrypt };
}

QString CertEncryptController::displayName(ToolId id) const
{
    Q_UNUSED(id);
    return tr("Encrypt (Certificates)");
}

bool CertEncryptController::isEnabled(ToolId id) const
{
    // Honest enablement: needs an open document AND the shared read-only gate.
    // With no document the tool stays VISIBLE but disabled (never hidden).
    if (!_ctx || !_ctx->document || _ctx->document->path().isEmpty())
        return false;
    return !EditPolicy::toolRefusedByReadOnly(_ctx->document.get(), id);
}

QString CertEncryptController::multiRecipientWarning(int recipientCount)
{
    if (recipientCount <= 1)
        return {};
    return QObject::tr(
        "This document is encrypted for multiple recipients (%1).\n\n"
        "Re-encrypting in place will change the session key. Other recipients "
        "will no longer be able to open the document unless their access is "
        "re-granted.\n\n"
        "Continue? All recipients must be re-specified after saving.")
        .arg(recipientCount);
}

void CertEncryptController::activate(ToolId id)
{
    Q_UNUSED(id);
    // Fail safe: no document → nothing to encrypt. isEnabled() already keeps
    // the action disabled; this guard covers direct dispatch (tests, hotkeys).
    if (!_ctx || !_ctx->pdfEditor || !_ctx->document
        || _ctx->document->path().isEmpty())
        return;

    // ER-3: re-encrypting a multi-recipient document changes the session key,
    // locking out recipients whose envelopes were computed against the
    // original FEK. Disclose BEFORE the dialog, same wording as the
    // password-encryption flow.
    const int existingRecipients = _ctx->pdfEditor->recipientCount();
    const QString warn = multiRecipientWarning(existingRecipients);
    if (!warn.isEmpty()) {
        const auto choice = QMessageBox::warning(
            _mainWindow, tr("Multi-Recipient Document"), warn,
            QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
        if (choice != QMessageBox::Ok)
            return;
    }

    RecipientPickerDialog dlg(_mainWindow);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QStringList certPaths = dlg.recipientCertPaths();
    if (certPaths.isEmpty()) {
        // Unreachable through the dialog (OK is gated), but never lie if a
        // future caller bypasses the gate.
        QMessageBox::warning(_mainWindow, tr("Encrypt (Certificates)"),
                             tr("No recipient certificate was provided."));
        return;
    }

    const QString inputPath = _ctx->document->path();
    const QString outputPath = QFileDialog::getSaveFileName(
        _mainWindow, tr("Save Encrypted Document"), QString(),
        tr("PDF Files (*.pdf)"));
    if (outputPath.isEmpty())
        return;

    if (_ctx->undoStack)
        _ctx->undoStack->clear();

    // AR-7 D3 idiom: immediate progress dialog with a real Cancel button.
    auto* progress = new QProgressDialog(tr("Encrypting to recipient certificates..."),
                                         tr("Cancel"), 0, 0, _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    std::weak_ptr<IPdfEditorEngine> weakEngine = _ctx->pdfEditor;
    std::weak_ptr<DocumentSession> weakDoc = _ctx->document;
    QPointer<CertEncryptController> self(this);
    auto result = std::make_shared<std::atomic<bool>>(false);

    QThread* worker = QThread::create([weakEngine, weakDoc, inputPath, outputPath,
                                       certPaths, result]() {
        auto engine = weakEngine.lock();
        auto doc = weakDoc.lock();
        if (!engine || !doc)
            return;
        try {
            result->store(engine->encryptWithCertificate(inputPath, outputPath, certPaths));
        } catch (const std::exception& e) {
            qCritical() << "CertEncrypt worker thread threw:" << e.what();
            result->store(false);
        } catch (...) {
            qCritical() << "CertEncrypt worker thread threw an unknown exception";
            result->store(false);
        }
    });

    connect(worker, &QThread::finished, _mainWindow, [self, progress, outputPath, result]() {
        progress->close();
        progress->deleteLater();
        if (!self)
            return;
        if (result->load()) {
            self->_mainWindow->statusBar()->showMessage(
                QObject::tr("Document encrypted for recipients"), 5000);
            if (self->_ctx && self->_ctx->document)
                self->_ctx->document->markReload();
        } else {
            // Honest failure: the source file is unchanged (the engine's
            // path-based mutator commits once, only on success).
            QMessageBox::critical(self->_mainWindow, tr("Encryption Failed"),
                tr("The document could not be encrypted to the selected recipient "
                   "certificates. The original file was left unchanged."));
            self->_mainWindow->statusBar()->showMessage(
                tr("Certificate encryption failed."), 5000);
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

} // namespace gp
