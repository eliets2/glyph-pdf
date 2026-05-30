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

    void addRecentFile(const QString& filePath);
    QStringList recentFiles() const;

private:
    void onSave();
    void onSaveAs();
    void onShare();
    void onPrint();
    void onPrintPreview();
    void onPageSetup();
    void onExportPresets();
    void showProperties();
    void onImportOffice();
    void onImagesToPdf();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
