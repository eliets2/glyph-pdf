// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"
#include "core/interfaces/ISignatureManager.h"

struct AppContext;
class QProgressDialog;
class QSettings;

namespace gp {

class MainWindow;

class SecurityController : public QObject, public IToolController {
    Q_OBJECT
public:
    SecurityController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;
    // ARC07: shared read-only gate (see shell/EditPolicy.h).
    bool isEnabled(ToolId id) const override;

    // §9.7 P0: pure summary builder for Validate All Signatures — exposed
    // static so the presentation logic is unit-testable without a MainWindow.
    static QString buildValidationSummary(const QList<SignatureInfo>& infos);

    // §9.7 P1: pure degradation-wording builder for a PARTIAL signing outcome.
    // Names EXACTLY which long-term-validation piece is missing (DSS
    // dictionary / archive timestamp) so the warning is actionable; returns an
    // empty string for every non-degradation outcome. `certified` picks the
    // verb — the certify flow gets the same exact wording. `requested` (R19c)
    // lets the wording carry the ATTAINED PAdES level (attainedLevelLabel),
    // not silently the requested one.
    static QString buildSigningOutcomeWarning(SignOutcome outcome, const QString &outputPath,
                                              const SignatureOutcomeDetail &detail,
                                              bool certified = false,
                                              PAdESLevel requested = PAdESLevel::B_T);

    // ── R19(a–c): settings-driven signing configuration ─────────────────────
    //
    // The signing settings (Preferences → Security → Signing) are the ONE
    // production surface that configures the PAdES level and the RFC 3161
    // timestamp authority; the controller consumes them BEFORE every dispatch
    // (runSigning / timestampDocument) — the callers PP05 found missing.
    struct SigningConfig {
        QString tsaUrl;                 // signing/tsaUrl (may be empty)
        PAdESLevel level = PAdESLevel::B_B; // signing/padesLevel
    };

    // Pure mapping of the stored combo value to the engine level. Unknown
    // values map to B-B — the only level that is honest without a TSA.
    static PAdESLevel padesLevelFromSetting(const QString& level);

    // Reads signing/tsaUrl + signing/padesLevel. `overrideSettings` (tests)
    // replaces the default application QSettings; production passes nullptr.
    static SigningConfig readSigningConfig(QSettings* overrideSettings = nullptr);

    // Pure pre-flight predicate (R19b): the exact refusal reason when the
    // configured request cannot honestly be attempted — a level above B-B
    // with no TSA URL would be SILENTLY downgraded by the engine (B-T token
    // fetch skipped when tsaUrl is empty, SignatureManager ComputeSignature),
    // so the controller refuses BEFORE any attempt instead. Empty string =
    // proceed; otherwise the message names what is missing and where to set
    // it. `forTimestamp` is the document-timestamp flow (always needs a TSA).
    static QString signingPreflightRefusal(PAdESLevel level, const QString& tsaUrl,
                                           bool forTimestamp = false);

    // R19c: pure attained-level label — the HIGHEST standard PAdES level whose
    // required pieces are all present given the outcome detail (B-LT needs the
    // DSS; B-LTA needs DSS + archive timestamp). B_LTA requested with a
    // missing archive timestamp attests "B-LT", etc. The engine's silent
    // B-T→B-B downgrade with an empty TSA URL is unreachable through the
    // controller (signingPreflightRefusal refuses it before any attempt).
    static QString attainedLevelLabel(PAdESLevel requested, const SignatureOutcomeDetail& detail);

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
