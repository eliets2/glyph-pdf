// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"

struct AppContext;

namespace gp {

class MainWindow;

// R15 (PP06 / UI02): one canonical controller for the promoted planned
// entries whose real route is a TASK SURFACE (a TaskNav screen or a sidebar
// pane) rather than a raw document mutation. Routing these ids through the
// ToolRegistry keeps the one-command-identity contract: the same
// dispatch/enablement seam every ribbon button, menu item and shortcut
// resolves through, instead of a parallel string->widget path.
//
// Navigation is a real effect (the task surface opens), so enablement is
// always true; enablement only ever narrows for read-only MUTATIONS, which
// this controller does not own.
class TaskNavController : public QObject, public IToolController {
    Q_OBJECT
public:
    TaskNavController(const AppContext* /*ctx*/, MainWindow* mainWindow, QObject* parent = nullptr)
        : QObject(parent), _mainWindow(mainWindow) {}

    // IToolController
    QList<ToolId> handledTools() const override;
    void activate(ToolId id) override;
    bool isEnabled(ToolId id) const override { Q_UNUSED(id); return true; }
    QString displayName(ToolId id) const override { return toolIdToString(id); }

private:
    MainWindow* _mainWindow;
};

} // namespace gp
