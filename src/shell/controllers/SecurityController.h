// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"
#include "core/interfaces/ISignatureManager.h"

struct AppContext;
class QProgressDialog;

namespace gp {

class MainWindow;

class SecurityController : public QObject, public IToolController {
    Q_OBJECT
public:
    SecurityController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

    // §9.7 P0: pure summary builder for Validate All Signatures — exposed
    // static so the presentation logic is unit-testable without a MainWindow.
    static QString buildValidationSummary(const QList<SignatureInfo>& infos);

    // §9.7 P1: pure degradation-wording builder for a PARTIAL signing outcome.
    // Names EXACTLY which long-term-validation piece is missing (DSS
    // dictionary / archive timestamp) so the warning is actionable; returns an
    // empty string for every non-degradation outcome. `certified` picks the
    // verb — the certify flow gets the same exact wording.
    static QString buildSigningOutcomeWarning(SignOutcome outcome, const QString &outputPath,
                                              const SignatureOutcomeDetail &detail,
                                              bool certified = false);

private:
    void encryptDocument();
    void signDocument();
    void verifySignatures();
    void sanitizeDocument();
    void applyRedactions();
    void exportAnnotationPackage();
    void importAnnotationPackage();
    void permissionsDocument();
    void removeSecurity();
    void certifyDocument();
    void timestampDocument();
    void setExpiryDocument();

    // §9.7 P1: capture of ONE signing/certifying request (defined in the .cpp)
    // — everything runSigning() needs to RE-RUN the exact same crypto
    // operation after a PartialLtvMissing "Retry" without re-prompting for
    // the certificate/password.
    struct SigningRequest;
    void runSigning(const SigningRequest &request);

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
    // U05: progress dialog of the running transactional redaction. Deleted only
    // when the NEXT operation starts (or with this controller) — never from the
    // operation's finished handler: a modal QProgressDialog::setValue() pumps
    // the event loop, so a deleteLater delivered inside that pump frees the
    // dialog under the still-executing setValue frame (use-after-free).
    QProgressDialog* _redactProgress = nullptr;
};

} // namespace gp
