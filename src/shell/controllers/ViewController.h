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
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void advancePresentation();

private:
    void toggleFullScreen();
    void togglePresentationMode();
    void exitPresentationMode();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
    bool _isFullScreen = false;
    bool _isPresentationMode = false;
    class QTimer* _presentationTimer = nullptr;
};

} // namespace gp
