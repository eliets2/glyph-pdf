#pragma once

#include <QObject>
#include <QString>

struct AppContext;

namespace gp {

class MainWindow;

class ConvertController : public QObject {
    Q_OBJECT
public:
    ConvertController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    bool handles(const QString& toolId) const;
    void activate(const QString& toolId);

private:
    void exportToWord();
    void exportToExcel();
    void exportToCsv();
    void mergePdfs();
    void linearizeDocument();
    void exportAsPdfA();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
