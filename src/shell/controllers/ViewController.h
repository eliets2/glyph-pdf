#pragma once

#include <QObject>
#include <QString>

struct AppContext;

namespace gp {

class MainWindow;

class ViewController : public QObject {
    Q_OBJECT
public:
    ViewController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    bool handles(const QString& toolId) const;
    void activate(const QString& toolId);

private:
    void toggleFullScreen();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
    bool _isFullScreen = false;
};

} // namespace gp
