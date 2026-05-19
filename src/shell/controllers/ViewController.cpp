#include "ViewController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"

#include <QMessageBox>
#include "shell/StatusBar.h"
#include <QPdfView>

namespace gp {

ViewController::ViewController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow) {}

QList<ToolId> ViewController::handledTools() const {
    return {
        ToolId::ZoomIn, ToolId::ZoomOut, ToolId::ActualSize,
        ToolId::FitWidth, ToolId::FitPage,
        ToolId::SinglePage, ToolId::Continuous, ToolId::TwoPage,
        ToolId::Presentation, ToolId::Fullscreen,
        ToolId::DarkMode, ToolId::EyeCare
    };
}

void ViewController::activate(ToolId id) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        if (id != ToolId::DarkMode && id != ToolId::EyeCare) {
            _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
            return;
        }
    }

    switch (id) {
    case ToolId::ZoomIn:
        viewer->zoomIn();
        break;
    case ToolId::ZoomOut:
        viewer->zoomOut();
        break;
    case ToolId::ActualSize:
        viewer->setZoomLevel(1.0);
        break;
    case ToolId::FitWidth:
        viewer->zoomFitWidth();
        break;
    case ToolId::FitPage:
        viewer->zoomFitPage();
        break;
    case ToolId::SinglePage:
        viewer->setPageMode(QPdfView::PageMode::SinglePage);
        _mainWindow->statusBar()->showMessage(tr("Single Page mode active."), 3000);
        break;
    case ToolId::Continuous:
        viewer->setPageMode(QPdfView::PageMode::MultiPage);
        _mainWindow->statusBar()->showMessage(tr("Continuous Scroll mode active."), 3000);
        break;
    case ToolId::TwoPage:
        QMessageBox::information(_mainWindow, tr("View Mode"),
            tr("Two-Page view mode requires a custom QGraphicsView renderer and is scheduled for a future engine update."));
        break;
    case ToolId::Presentation:
    case ToolId::Fullscreen:
        toggleFullScreen();
        break;
    case ToolId::DarkMode:
        _mainWindow->toggleTheme();
        break;
    case ToolId::EyeCare:
        _mainWindow->statusBar()->showMessage(tr("Eye Strain (Sepia) mode is scheduled for a future update."), 3000);
        break;
    default:
        break;
    }
}

void ViewController::toggleFullScreen() {
    _isFullScreen = !_isFullScreen;
    if (_isFullScreen) {
        if (auto* viewer = _mainWindow->pdfViewer()) {
            viewer->setPageMode(QPdfView::PageMode::SinglePage);
            viewer->zoomFitPage();
        }
    }
    _mainWindow->setFullScreenMode(_isFullScreen);
}

} // namespace gp
