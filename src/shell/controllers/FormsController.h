// SPDX-License-Identifier: Apache-2.0
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
    // ARC07: shared read-only gate (see shell/EditPolicy.h).
    bool isEnabled(ToolId id) const override;

private:
    void autoDetectFields();
    void editTabOrder();

    void onImportDataRequested();
    void onExportDataRequested();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
