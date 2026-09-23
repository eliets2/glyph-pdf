// SPDX-License-Identifier: Apache-2.0
#ifndef SENDFORSIGNINGCONTROLLER_H
#define SENDFORSIGNINGCONTROLLER_H

#include "core/interfaces/IToolController.h"
#include <QObject>
#include <QPointer>

struct AppContext;
class SigningProgressPanel;

namespace gp {
class MainWindow;

// ── R26 send-for-signing P1 — the workflow controller ────────────────────────
//
// Owns ToolId::PrepareSigningRequest (Protect ▸ Sign ▸ "Prepare Request") and
// the fill-flow surface. Sibling of SecurityController (that file's sign flow
// stays untouched — the CertEncryptController precedent): the fill step drives
// the SAME public seams the SecurityController sign flow drives — the real
// SignatureDialog (P12/password/appearance picker) and
// SignatureManager::signDocumentWithAppearance — orchestrated through the
// pure SigningRequestRunner, with the sidecar updated only from
// engine-attested results.
class SendForSigningController : public QObject, public IToolController {
    Q_OBJECT
public:
    SendForSigningController(const AppContext *ctx, MainWindow *mainWindow,
                             QObject *parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;
    bool isEnabled(ToolId id) const override;

    // Document-open hook (GpMainWindow::openDocument, after beginDocument):
    // a document with a sidecar opens its signing-progress surface (modeless,
    // dismissible). A handshake-refusing sidecar is disclosed on the status
    // bar and IGNORED — never half-parsed. No sidecar → silence (no noise for
    // unprepared documents).
    void onDocumentOpened(const QString &docPath);

private:
    void openPrepareDialog();
    void showProgressPanel();
    void runSignStep(int signerIndex);

    const AppContext *_ctx = nullptr;
    MainWindow *_mainWindow = nullptr;
    QPointer<SigningProgressPanel> _panel;
};

} // namespace gp

#endif // SENDFORSIGNINGCONTROLLER_H
