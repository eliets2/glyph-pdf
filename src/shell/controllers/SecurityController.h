// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"

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
    // Wave 1A §9.11: date-picker UI for the already-implemented
    // PdfEditorEngine::setExpiryDate(), which previously had zero UI callers.
    void setExpiryDateDocument();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
