#pragma once

#include <QObject>
#include <QString>

struct AppContext;

namespace gp {

class MainWindow;

class SecurityController : public QObject {
    Q_OBJECT
public:
    SecurityController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    bool handles(const QString& toolId) const;
    void activate(const QString& toolId);

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
