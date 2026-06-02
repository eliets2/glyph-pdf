// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"

struct AppContext;

namespace gp {

class MainWindow;

class PagesController : public QObject, public IToolController {
    Q_OBJECT
public:
    PagesController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent = nullptr);

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;

public slots:
    void onPageReordered(int from, int to);
    void onCropRequested(int pageIndex, QRectF rect);

private:
    void rotateLeft();
    void rotateRight();
    void showPageManagement();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
