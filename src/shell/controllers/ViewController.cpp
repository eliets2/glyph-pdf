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

bool ViewController::handles(const QString& toolId) const {
    return toolId == "zoomIn" || toolId == "zoom-in" ||
           toolId == "zoomOut" || toolId == "zoom-out" ||
           toolId == "actual" || toolId == "actual-size" ||
           toolId == "fitWidth" || toolId == "fit-width" ||
           toolId == "fitPage" || toolId == "fit-page" ||
           toolId == "single" || toolId == "single-page" ||
           toolId == "continuous" ||
           toolId == "two" || toolId == "two-page" ||
           toolId == "presentation" || toolId == "fullscreen" ||
           toolId == "darkMode" || toolId == "eyeCare";
}

void ViewController::activate(const QString& toolId) {
    auto* viewer = _mainWindow->pdfViewer();
    if (!viewer) {
        if (toolId != "darkMode" && toolId != "eyeCare") {
            _mainWindow->statusBar()->showMessage(tr("No document is open."), 3000);
            return;
        }
    }

    if (toolId == "zoomIn" || toolId == "zoom-in") {
        viewer->zoomIn();
    } else if (toolId == "zoomOut" || toolId == "zoom-out") {
        viewer->zoomOut();
    } else if (toolId == "actual" || toolId == "actual-size") {
        viewer->setZoomLevel(1.0);
    } else if (toolId == "fitWidth" || toolId == "fit-width") {
        viewer->zoomFitWidth();
    } else if (toolId == "fitPage" || toolId == "fit-page") {
        viewer->zoomFitPage();
    } else if (toolId == "single" || toolId == "single-page") {
        viewer->setPageMode(QPdfView::PageMode::SinglePage);
        _mainWindow->statusBar()->showMessage(tr("Single Page mode active."), 3000);
    } else if (toolId == "continuous") {
        viewer->setPageMode(QPdfView::PageMode::MultiPage);
        _mainWindow->statusBar()->showMessage(tr("Continuous Scroll mode active."), 3000);
    } else if (toolId == "two" || toolId == "two-page") {
        QMessageBox::information(_mainWindow, tr("View Mode"),
            tr("Two-Page view mode requires a custom QGraphicsView renderer and is scheduled for a future engine update."));
    } else if (toolId == "presentation" || toolId == "fullscreen") {
        toggleFullScreen();
    } else if (toolId == "darkMode") {
        _mainWindow->toggleTheme();
    } else if (toolId == "eyeCare") {
        _mainWindow->statusBar()->showMessage(tr("Eye Strain (Sepia) mode is scheduled for a future update."), 3000);
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
