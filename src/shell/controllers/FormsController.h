#pragma once

#include <QObject>
#include <QString>

struct AppContext;

namespace gp {

class MainWindow;

class FormsController : public QObject {
    Q_OBJECT
public:
    FormsController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    bool handles(const QString& toolId) const;
    void activate(const QString& toolId);

private:
    void manageForms();
    void addFormTextField();
    void addFormCheckbox();
    void addFormRadioButton();
    void addFormDropdown();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
