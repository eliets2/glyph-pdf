#pragma once

#include <QObject>
#include <QString>

struct AppContext;

namespace gp {

class MainWindow;

class PagesController : public QObject {
    Q_OBJECT
public:
    PagesController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    bool handles(const QString& toolId) const;
    void activate(const QString& toolId);

private:
    void rotateLeft();
    void rotateRight();
    void showPageManagement();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
