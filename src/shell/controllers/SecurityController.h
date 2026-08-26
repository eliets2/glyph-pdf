// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"
#include "core/interfaces/ISignatureManager.h"

struct AppContext;

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

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
