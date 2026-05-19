#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"

struct AppContext;

namespace gp {

class MainWindow;

class ViewController : public QObject, public IToolController {
    Q_OBJECT
public:
    ViewController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

private:
    void toggleFullScreen();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
    bool _isFullScreen = false;
};

} // namespace gp
