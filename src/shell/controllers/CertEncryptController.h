// SPDX-License-Identifier: Apache-2.0
#ifndef CERTENCRYPTCONTROLLER_H
#define CERTENCRYPTCONTROLLER_H

#include "core/interfaces/IToolController.h"
#include <QObject>

struct AppContext;

namespace gp {

class MainWindow;

// N17 (backlog row 33): certificate-encryption recipient picker command.
// Owns ToolId::CertEncrypt; opens the RecipientPickerDialog over the EXISTING
// engine seam `IEncryptor::encryptWithCertificate` and runs it on a worker
// thread with the same SafeSave/progress/completion discipline the
// password-encryption flow uses (SecurityController::encryptDocument idiom —
// that file is lane-locked, hence this sibling controller).
class CertEncryptController : public QObject, public IToolController {
    Q_OBJECT
public:
    CertEncryptController(const AppContext* ctx, MainWindow* mainWindow,
                          QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;
    // ARC07: shared read-only gate — encrypting mutates the session document.
    bool isEnabled(ToolId id) const override;
    QString displayName(ToolId id) const override;

    // ER-3 honest disclosure — pure wording builder (unit-testable without the
    // shell, the SecurityController::buildValidationSummary idiom). Empty when
    // `recipientCount` is 0 or 1 (nothing to disclose); otherwise the exact
    // multi-recipient session-key warning, carrying the count.
    static QString multiRecipientWarning(int recipientCount);

private:
    const AppContext* _ctx;
    MainWindow* _mainWindow;
};

} // namespace gp

#endif // CERTENCRYPTCONTROLLER_H
