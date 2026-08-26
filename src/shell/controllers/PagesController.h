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

    // §9.9 P0: pure helper — convert a single move (from→to) into the full
    // page permutation consumed by ReorderPermutationCommand. Exposed static
    // so the consolidation is unit-testable without a MainWindow.
    static QList<int> buildMovePermutation(int pageCount, int from, int to);

public slots:
    void onPageReordered(int from, int to);
    void onCropRequested(int pageIndex, QRectF rect);
    // §9.1 P0: viewer rotation requests land here so the REAL page bitmap
    // rotates (engine-side /Rotate + reload), not just the overlay.
    void onPageRotateRequested(int degrees);

private:
    void rotateLeft();
    void rotateRight();
    void showPageManagement();

    const AppContext* _ctx = nullptr;
    MainWindow* _mainWindow = nullptr;
};

} // namespace gp
