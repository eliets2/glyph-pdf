// SPDX-License-Identifier: Apache-2.0
// R26 send-for-signing P1 — see SendForSigningController.h.
#include "SendForSigningController.h"

#include "core/AppContext.h"
#include "core/SigningRequestModel.h"
#include "core/SigningRequestRunner.h"
#include "core/interfaces/IPdfEditorEngine.h" // releaseResidentFile (F4d-D1, runA11yFix/FormsController precedent)
#include "engines/DocumentSession.h"
#include "engines/SignatureManager.h"
#include "shell/EditPolicy.h"
#include "shell/controllers/SecurityController.h" // readSigningConfig/preflight/outcome wording (statics reused, file untouched)
#include "ui/SignatureDialog.h"
#include "ui/SigningProgressPanel.h"
#include "ui/SigningRequestDialog.h"
#include "GpMainWindow.h"
#include "shell/StatusBar.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QThread>

namespace gp {

SendForSigningController::SendForSigningController(const AppContext *ctx,
                                                   MainWindow *mainWindow,
                                                   QObject *parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> SendForSigningController::handledTools() const
{
    return { ToolId::PrepareSigningRequest };
}

bool SendForSigningController::isEnabled(ToolId id) const
{
    Q_UNUSED(id);
    if (!_ctx || !_ctx->document || _ctx->document->path().isEmpty())
        return false;
    return !EditPolicy::toolRefusedByReadOnly(_ctx->document.get(), id);
}

void SendForSigningController::activate(ToolId id)
{
    Q_UNUSED(id);
    if (!_ctx || !_ctx->signing || !_ctx->document
        || _ctx->document->path().isEmpty())
        return;   // isEnabled() keeps the action disabled; this covers direct dispatch
    openPrepareDialog();
}

void SendForSigningController::openPrepareDialog()
{
    const QString docPath = _ctx->document->path();
    auto *concreteEngine = dynamic_cast<SignatureManager *>(_ctx->signing.get());
    if (!concreteEngine) return;   // honest engine unavailability — no dialog
    SigningRequestDialog dlg(concreteEngine, docPath, _mainWindow);
    connect(&dlg, &SigningRequestDialog::requestSaved, this, [this](bool fieldsCreated) {
        if (fieldsCreated) {
            // The document changed on disk (signature fields created) — the
            // session must reload it before anything else edits it.
            _ctx->document->markReload();
            _mainWindow->statusBar()->showMessage(
                tr("Signature fields created and signing request saved."), 5000);
        } else {
            _mainWindow->statusBar()->showMessage(
                tr("Signing request saved alongside the document."), 5000);
        }
    });
    if (dlg.exec() == QDialog::Accepted)
        showProgressPanel();
}

void SendForSigningController::onDocumentOpened(const QString &docPath)
{
    if (!_ctx || !_ctx->signing || docPath.isEmpty()) return;
    const QString sidecar = SigningRequestModel::sidecarPathFor(docPath);
    if (!QFileInfo::exists(sidecar)) return;   // unprepared document: no noise

    const auto loaded = SigningRequestModel::load(sidecar);
    switch (loaded.error) {
    case SigningRequestModel::LoadError::None:
        break;   // show the surface below
    case SigningRequestModel::LoadError::FileNotFound:
        return;
    default:
        // Handshake refusal: disclosed and IGNORED — the fill flow never runs
        // from a sidecar this build cannot fully validate.
        _mainWindow->statusBar()->showMessage(
            tr("Signing request ignored: %1").arg(loaded.detail), 8000);
        return;
    }
    showProgressPanel();
}

void SendForSigningController::showProgressPanel()
{
    const QString docPath = _ctx->document->path();
    // PGR-36 (D2 delta review 2026-09-23): the panel is MODELESS and bound to
    // the document it was created for. Re-raising a panel bound to a PREVIOUS
    // document showed request A's signers over document B — and every click
    // then executed signer N of B's sidecar (cross-document replay through a
    // stale surface). A panel whose document changed is closed and replaced.
    if (_panel && _panel->documentPath() != docPath) {
        _panel->close();   // WA_DeleteOnClose
        _panel = nullptr;
    }
    if (_panel) {
        _panel->refresh();
        _panel->show();
        _panel->raise();
        _panel->activateWindow();
        return;
    }
    _panel = new SigningProgressPanel(
        dynamic_cast<SignatureManager *>(_ctx->signing.get()), docPath, _mainWindow);
    _panel->setAttribute(Qt::WA_DeleteOnClose);
    connect(_panel, &SigningProgressPanel::signRequested, this,
            [this](int idx) { runSignStep(idx); });
    _panel->show();
    _panel->raise();
}

void SendForSigningController::runSignStep(int signerIndex)
{
    if (!_ctx || !_ctx->signing || !_ctx->document) return;
    const QString docPath = _ctx->document->path();
    if (docPath.isEmpty()) return;

    // PGR-36 (D2 delta review 2026-09-23): defense in depth behind
    // showProgressPanel's replace-on-document-change. A stale panel can
    // still be on screen when the new document carries NO sidecar
    // (onDocumentOpened returned early and never reached showProgressPanel).
    // Clicking Sign on it must never execute this document's sidecar behind
    // a request rendered for a different document — refuse and let the
    // panel be replaced by the next open/prepare.
    if (_panel && _panel->documentPath() != docPath) {
        QMessageBox::warning(_mainWindow, tr("Signing Panel Is For Another Document"),
                             tr("This signing progress belongs to %1, but the open "
                                "document is %2 — the step was NOT run. Close the panel "
                                "and reopen it from the document you want to sign.")
                                 .arg(_panel->documentPath(),
                                      QFileInfo(docPath).fileName()));
        if (_panel) {
            _panel->close();
            _panel = nullptr;
        }
        return;
    }

    // The model snapshot comes from DISK every step (the sidecar is the
    // workflow's state, not the panel's memory).
    const auto loaded =
        SigningRequestModel::load(SigningRequestModel::sidecarPathFor(docPath));
    if (loaded.error != SigningRequestModel::LoadError::None) {
        QMessageBox::warning(_mainWindow, tr("Signing Request Unreadable"),
                             tr("The signing request could not be read and was NOT "
                                "used:\n%1").arg(loaded.detail));
        if (_panel) _panel->refresh();
        return;
    }

    // R19(b)-equivalent: the settings-derived signing configuration is applied
    // to this step, with the SAME honest pre-flight the sign flow runs — a
    // level above B-B with no TSA URL would be silently downgraded.
    const SecurityController::SigningConfig cfg = SecurityController::readSigningConfig();
    const QString preflight =
        SecurityController::signingPreflightRefusal(cfg.level, cfg.tsaUrl);
    if (!preflight.isEmpty()) {
        QMessageBox::warning(_mainWindow, tr("Signing Not Attempted"), preflight);
        return;
    }

    // The REAL sign flow: the same SignatureDialog the Sign command opens —
    // P12 picker, passphrase, reason/location, optional appearance image.
    SignatureDialog dlg(_mainWindow);
    if (dlg.exec() != QDialog::Accepted) return;
    if (dlg.certificatePath().isEmpty() || dlg.password().isEmpty()) {
        QMessageBox::warning(_mainWindow, tr("Error"),
                             tr("Certificate path and password are required."));
        return;
    }

    SigningRequestRunner::FillStepInput input;
    input.docPath = docPath;
    input.model = loaded.model;
    input.signerIndex = signerIndex;
    input.certPath = dlg.certificatePath();
    input.password = dlg.password();
    input.reason = dlg.reason();
    input.location = dlg.location();
    input.appearance = SignatureManager::takePendingAppearanceImage();
    input.requestedLevel = cfg.level;
    input.tsaUrl = cfg.tsaUrl;

    // Mutation gate with the re-confirm loop: changed bytes refuse the step;
    // the user can re-confirm the request against the changed document.
    {
        auto *concrete = dynamic_cast<SignatureManager *>(_ctx->signing.get());
        if (!concrete) {
            QMessageBox::warning(_mainWindow, tr("Signing Step Refused"),
                                 tr("The signing engine is unavailable."));
            return;
        }
        const SigningRequestRunner::Refusal pre =
            SigningRequestRunner::precheck(*concrete, input);
        if (pre.code == SigningRequestRunner::StepRefusal::DocumentChanged) {
            const auto choice = QMessageBox::question(
                _mainWindow, tr("Document Changed Since Preparation"),
                tr("%1\n\nRe-confirm this signing request against the document as it "
                   "is now?").arg(pre.message),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (choice != QMessageBox::Yes) return;
            SigningRequestModel confirmed = loaded.model;
            confirmed.reconfirmedSha256 = pre.documentSha256;
            confirmed.preparedUtc =
                QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            QString saveErr;
            if (!confirmed.save(SigningRequestModel::sidecarPathFor(docPath), &saveErr)) {
                QMessageBox::warning(_mainWindow, tr("Signing Request Not Updated"),
                                     saveErr);
                return;
            }
            input.model = confirmed;
            // SWEEP-W1 F3: the gate's re-confirm authorization is OUT OF BAND —
            // set only here, after the user's Yes. The sidecar copy is a
            // display/record value; a reconfirmedSha256 read back from the
            // (unsigned) file never skips this dialog.
            input.userReconfirmedSha256 = pre.documentSha256;
        } else if (pre.code != SigningRequestRunner::StepRefusal::None) {
            QMessageBox::warning(_mainWindow, tr("Signing Step Refused"), pre.message);
            return;
        }
    }

    auto *progress = new QProgressDialog(tr("Signing document…"), QString(), 0, 0,
                                         _mainWindow);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->show();

    // F4d-D1 (SWEEP-W3 UX): the fill step commits IN PLACE onto docPath from
    // the signing worker thread (lazy field placement + the signed-candidate
    // commit, both through SafeSave). Two OS devices pin that file while it is
    // open: the viewer's QPdfDocument (released by the shell's SafeSave
    // handle coordinator, which marshals park/restore to the GUI thread) and
    // the editing engine's file-backed resident, whose lazy-parse input device
    // stays open until dropped — the engine's own same-file save re-seats it
    // away, but this foreign writer cannot, so the shell does it here, exactly
    // like runA11yFix and the FormsController form import (V01). The next
    // resolveDocument lazily re-loads from disk: the committed result on
    // success, the preserved original on a failed step — truthful either way.
    // M2 (PR-review §4): the fill step commits IN PLACE onto the open
    // document — unsaved session changes would be silently dropped by the
    // rewrite. Checked save-first prompt (Save / Discard / Cancel) before
    // the resident document is parked.
    if (_ctx->pdfEditor) {
        if (!_mainWindow->confirmSaveBeforeInPlaceWrite(
                tr("filling the signing request"))) {
            _mainWindow->statusBar()->showMessage(
                tr("Signing step canceled — the document has unsaved changes."),
                5000);
            return;
        }
        _ctx->pdfEditor->releaseResidentFile(docPath);
    }

    // Worker thread (runSigning idiom): the engine call runs off the GUI
    // thread; the result is read only after the worker finished.
    std::weak_ptr<ISignatureManager> weakSigning = _ctx->signing;
    std::weak_ptr<DocumentSession> weakDoc = _ctx->document;
    QPointer<SendForSigningController> self(this);
    auto result = std::make_shared<SigningRequestRunner::FillStepResult>();
    auto inputBox = std::make_shared<SigningRequestRunner::FillStepInput>(input);

    QThread *worker = QThread::create([weakSigning, inputBox, result]() {
        if (auto signing = weakSigning.lock()) {
            auto *concrete = dynamic_cast<SignatureManager *>(signing.get());
            if (concrete)
                *result = SigningRequestRunner::runFillStep(*concrete, *inputBox);
            else
                result->error = QObject::tr("The signing engine is unavailable.");
        } else {
            result->error = QObject::tr("The signing engine is unavailable.");
        }
    });
    connect(worker, &QThread::finished, _mainWindow, [self, progress, result, inputBox,
                                                      weakDoc, weakSigning]() {
        progress->close();
        progress->deleteLater();
        if (!self) return;
        const SigningRequestRunner::FillStepResult &r = *result;
        if (!r.attempted || !r.committed) {
            // The step's own lazy field creation is a workflow-attributed
            // mutation: advance the request's prepared identity over it even
            // when the signing half failed, so a RETRY does not refuse the
            // creation it just performed.
            if (r.fieldCreated && !r.documentSha256.isEmpty()) {
                SigningRequestModel model = inputBox->model;
                model.preparedSha256 = r.documentSha256;
                model.preparedUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
                QString retryErr;
                if (!model.save(SigningRequestModel::sidecarPathFor(inputBox->docPath),
                                &retryErr))
                    QMessageBox::warning(self->_mainWindow,
                                         tr("Signing Request Not Updated"), retryErr);
            }
            QMessageBox::critical(self->_mainWindow, tr("Signing Step Failed"), r.error);
            if (self->_panel) self->_panel->refresh();
            return;
        }

        // Fold the engine-attested result into the sidecar — on the SAME
        // model snapshot the step validated against (inputBox holds it).
        SigningRequestModel model = inputBox->model;
        SigningRequestRunner::applyStepToModel(model, inputBox->signerIndex, r);
        QString saveErr;
        const bool saved = model.save(
            SigningRequestModel::sidecarPathFor(inputBox->docPath), &saveErr);

        if (auto doc = weakDoc.lock())
            doc->markReload();   // the document bytes changed on disk

        if (r.outcome == SignOutcome::PartialLtvMissing) {
            // E-02 honesty: the signature IS on disk — name the exact missing
            // long-term-validation piece (the sign flow's own wording).
            const QString warning = SecurityController::buildSigningOutcomeWarning(
                r.outcome, inputBox->docPath, r.detail, false, inputBox->requestedLevel);
            QMessageBox::warning(self->_mainWindow,
                                 tr("Long-Term Validation Incomplete"), warning);
        }
        if (!r.fieldMatch) {
            const QString bound = inputBox->signerIndex >= 0
                                      && inputBox->signerIndex < model.signers.size()
                                      ? model.signers[inputBox->signerIndex].fieldName
                                      : QString();
            QMessageBox::warning(self->_mainWindow, tr("Field Mismatch Recorded"),
                                 tr("The signature was written to field %1, not the field "
                                    "bound to this signer (%2). The request records what "
                                    "actually happened — review the order disclosure.")
                                     .arg(r.signedFieldName, bound));
        }
        if (!saved) {
            QMessageBox::warning(self->_mainWindow, tr("Signing Request Not Updated"),
                                 saveErr);
        }
        self->_mainWindow->statusBar()->showMessage(
            tr("Signer step complete (PAdES %1).").arg(r.attainedLevel), 5000);
        if (self->_panel) self->_panel->refresh();
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

} // namespace gp
