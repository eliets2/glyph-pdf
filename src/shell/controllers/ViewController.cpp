#include "ViewController.h"
#include "core/AppContext.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"

#include <QMessageBox>
#include "shell/StatusBar.h"
#include <QPdfView>
#include <QTimer>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QApplication>

namespace gp {

ViewController::ViewController(const AppContext* ctx, MainWindow* mainWindow, QObject* parent)
    : QObject(parent), _ctx(ctx), _mainWindow(mainWindow), _presentationTimer(new QTimer(this)) {
    connect(_presentationTimer, &QTimer::timeout, this, &ViewController::advancePresentation);
}

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
        viewer->setTwoPageMode(true);
        _mainWindow->statusBar()->showMessage(tr("Two-Page view mode active."), 3000);
        break;
    case ToolId::Presentation:
        togglePresentationMode();
        break;
    case ToolId::Fullscreen:
        toggleFullScreen();
        break;
    case ToolId::DarkMode:
        _mainWindow->toggleTheme();
        break;
    case ToolId::EyeCare:
        viewer->toggleEyeCareMode();
        _mainWindow->statusBar()->showMessage(tr("Eye Care mode toggled."), 3000);
        break;
    default:
        break;
    }
}

void ViewController::toggleFullScreen() {
    _isFullScreen = !_isFullScreen;
    if (auto* viewer = _mainWindow->pdfViewer()) {
        if (_isFullScreen) {
            viewer->setPageMode(QPdfView::PageMode::SinglePage);
            viewer->zoomFitPage();
        } else {
            viewer->setPageMode(QPdfView::PageMode::MultiPage);
        }
    }
    _mainWindow->setFullScreenMode(_isFullScreen);
}

void ViewController::togglePresentationMode() {
    _isPresentationMode = !_isPresentationMode;
    auto* viewer = _mainWindow->pdfViewer();
    
    if (_isPresentationMode) {
        if (!_isFullScreen) toggleFullScreen();
        
        qApp->installEventFilter(this);
        _presentationTimer->start(5000); // Auto advance every 5 seconds
        if (viewer) viewer->setPageMode(QPdfView::PageMode::SinglePage);
        _mainWindow->statusBar()->showMessage(tr("Presentation mode started. ESC to exit."), 3000);
    } else {
        exitPresentationMode();
    }
}

void ViewController::exitPresentationMode() {
    if (!_isPresentationMode) return;
    
    _isPresentationMode = false;
    _presentationTimer->stop();
    qApp->removeEventFilter(this);
    
    if (_isFullScreen) toggleFullScreen();
    _mainWindow->statusBar()->showMessage(tr("Presentation mode ended."), 3000);
}

void ViewController::advancePresentation() {
    if (auto* viewer = _mainWindow->pdfViewer()) {
        if (viewer->currentPage() < viewer->pageCount() - 1) {
            viewer->goToPage(viewer->currentPage() + 1);
        } else {
            exitPresentationMode();
        }
    }
}

bool ViewController::eventFilter(QObject *watched, QEvent *event) {
    if (_isPresentationMode) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(event);
            if (ke->key() == Qt::Key_Escape) {
                exitPresentationMode();
                return true;
            } else if (ke->key() == Qt::Key_Space || ke->key() == Qt::Key_Right || ke->key() == Qt::Key_Down) {
                _presentationTimer->start(5000); // Reset timer
                advancePresentation();
                return true;
            } else if (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Up) {
                _presentationTimer->start(5000); // Reset timer
                if (auto* viewer = _mainWindow->pdfViewer()) {
                    if (viewer->currentPage() > 0) {
                        viewer->goToPage(viewer->currentPage() - 1);
                    }
                }
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                _presentationTimer->start(5000); // Reset timer
                advancePresentation();
                return true;
            } else if (me->button() == Qt::RightButton) {
                _presentationTimer->start(5000); // Reset timer
                if (auto* viewer = _mainWindow->pdfViewer()) {
                    if (viewer->currentPage() > 0) {
                        viewer->goToPage(viewer->currentPage() - 1);
                    }
                }
                return true;
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

} // namespace gp
