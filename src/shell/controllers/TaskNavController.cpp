// SPDX-License-Identifier: Apache-2.0
#include "shell/controllers/TaskNavController.h"
#include "GpMainWindow.h"

namespace gp {

QList<ToolId> TaskNavController::handledTools() const {
    return {
        // T1 measure task panel (the real measure route).
        ToolId::Measure, ToolId::MeasureDistance, ToolId::MeasureArea,
        // OCR Verify screen (verify panes + language selection live there).
        ToolId::OcrVerify, ToolId::OcrLanguage,
        // View ▸ Panes: sidebar pane switches.
        ToolId::PanePages, ToolId::PaneBookmarks,
        ToolId::PaneComments, ToolId::PaneLayers,
        // Batch workspace (BatchMode owns the hot-folder section).
        ToolId::BatchConvert, ToolId::WatchFolder,
    };
}

void TaskNavController::activate(ToolId id) {
    if (!_mainWindow) return;
    switch (id) {
    case ToolId::Measure:
    case ToolId::MeasureDistance:
    case ToolId::MeasureArea:
        _mainWindow->activateScreen(QStringLiteral("measure"));
        break;
    case ToolId::OcrVerify:
    case ToolId::OcrLanguage:
        _mainWindow->activateScreen(QStringLiteral("ocr"));
        break;
    case ToolId::PanePages:
        _mainWindow->showSidebarPane(QStringLiteral("pages"));
        break;
    case ToolId::PaneBookmarks:
        _mainWindow->showSidebarPane(QStringLiteral("bookmarks"));
        break;
    case ToolId::PaneComments:
        _mainWindow->showSidebarPane(QStringLiteral("comments"));
        break;
    case ToolId::PaneLayers:
        _mainWindow->showSidebarPane(QStringLiteral("layers"));
        break;
    case ToolId::BatchConvert:
    case ToolId::WatchFolder:
        _mainWindow->activateScreen(QStringLiteral("batch"));
        break;
    default:
        break;
    }
}

} // namespace gp
