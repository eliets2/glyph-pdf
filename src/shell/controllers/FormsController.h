#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"

struct AppContext;

namespace gp {

class MainWindow;

class FormsController : public QObject, public IToolController {
    Q_OBJECT
public:
    FormsController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

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
