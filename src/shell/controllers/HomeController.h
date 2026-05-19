#pragma once

#include <QObject>
#include <QString>

struct AppContext;
class PdfViewerWidget;

namespace gp {

class MainWindow;

class HomeController : public QObject {
    Q_OBJECT
public:
    HomeController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    bool handles(const QString& toolId) const;
    void activate(const QString& toolId);

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
