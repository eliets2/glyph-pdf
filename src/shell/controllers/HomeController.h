#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"

struct AppContext;
class PdfViewerWidget;

namespace gp {

class MainWindow;

class HomeController : public QObject, public IToolController {
    Q_OBJECT
public:
    HomeController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

private:
    void onSave();
    void onSaveAs();
    void onShare();
    void onPrint();
    void showProperties();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
