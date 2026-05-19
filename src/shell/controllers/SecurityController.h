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
    void cloudSyncSync();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
