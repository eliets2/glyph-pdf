#include "GpMainWindow.h"

#include "shell/MenuBar.h"
#include "shell/Ribbon.h"
#include "shell/ModeStrip.h"
#include "shell/ScreenNav.h"
#include "shell/StatusBar.h"
#include "shell/Sidebar.h"

#include "modes/ModeController.h"
#include "modes/AIChatPanel.h"
#include "modes/SignaturesPanel.h"
#include "modes/PdfAValidationPanel.h"
#include "modes/CompressDialog.h"
#include "modes/WatermarkDialog.h"

#include "shell/controllers/HomeController.h"
#include "shell/controllers/ViewController.h"
#include "shell/controllers/EditController.h"
#include "shell/controllers/PagesController.h"
#include "shell/controllers/ConvertController.h"
#include "shell/controllers/FormsController.h"
#include "shell/controllers/SecurityController.h"

#include "ui/PdfViewerWidget.h"
#include "ui/ThumbnailSidebar.h"
#include "shell/ToolRegistry.h"
#include "ui/FindBar.h"
#include "engines/DocumentSession.h"
#include "util/GpTheme.h"

#include <QApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QFileInfo>

namespace gp {

MainWindow::MainWindow(const AppContext* ctx, QWidget* parent) : QMainWindow(parent), _ctx(ctx) {
    setWindowTitle("Glyph PDF — [No Document]");
    resize(1480, 920);

    // === Top chrome
    _menu = new MenuBar(this);
    setMenuBar(_menu);

    _ribbon = new Ribbon(this);
    _modeStrip = new ModeStrip(this);

    // === Central host (ribbon + main row)
    auto* central = new QWidget(this);
    central->setObjectName("centralHost");
    auto* col = new QVBoxLayout(central);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    col->addWidget(_ribbon);
    col->addWidget(_modeStrip);

    // === FindBar insertion
    _findBar = new FindBar(this);
    _findBar->setVisible(false);
    col->addWidget(_findBar);

    // === Main row: left | center modes | right inspector | (optional) AI
    auto* row = new QWidget;
    auto* rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(0);

    _left = new Sidebar(Sidebar::Left, this);
    _right = new Sidebar(Sidebar::Right, this);
    _modes = new ModeController(this);

    _left->init(ctx, _modes->viewer());
    _right->init(ctx, _modes->viewer());

    rowLay->addWidget(_left);
    rowLay->addWidget(_modes, 1);
    rowLay->addWidget(_right);

    col->addWidget(row, 1);

    // === Bottom chrome
    _screenNav = new ScreenNav(this);
    col->addWidget(_screenNav);

    setCentralWidget(central);

    _status = new StatusBar(this);
    setStatusBar(_status);

    // === Instantiate Controllers
    _home = new HomeController(ctx, this, this);
    _view = new ViewController(ctx, this, this);
    _edit = new EditController(ctx, this, this);
    _pages = new PagesController(ctx, this, this);
    _convert = new ConvertController(ctx, this, this);
    _forms = new FormsController(ctx, this, this);
    _security = new SecurityController(ctx, this, this);

    _toolRegistry = new ToolRegistry(this);
    _toolRegistry->registerController(_home);
    _toolRegistry->registerController(_view);
    _toolRegistry->registerController(_edit);
    _toolRegistry->registerController(_pages);
    _toolRegistry->registerController(_convert);
    _toolRegistry->registerController(_forms);
    _toolRegistry->registerController(_security);

    _modeStrip->init(ctx);

    if (ctx && ctx->document) {
        connect(ctx->document.get(), &DocumentSession::dirtyChanged, this, [this](bool dirty) {
            _status->updateUnsaved(dirty);
            if (!dirty) {
                _modeStrip->setAutosaveTime(QDateTime::currentDateTime());
                _modeStrip->setSyncStatus(tr("⤺ SYNCED · v.1"));
            } else {
                _modeStrip->setSyncStatus(tr("⤺ NOT SYNCED"));
            }
            updateTitle();
        });
    }

    // === Wire signals
    connect(_ribbon, &Ribbon::toolActivated, this, &MainWindow::onToolActivated);
    connect(_ribbon, &Ribbon::tabChanged,    this, &MainWindow::onTabChanged);
    connect(_modeStrip, &ModeStrip::modeChanged,         this, &MainWindow::onModeChanged);
    connect(_modeStrip, &ModeStrip::themeToggleRequested, this, &MainWindow::toggleTheme);
    connect(_modeStrip, &ModeStrip::aiToggleRequested,    this, &MainWindow::toggleAi);
    connect(_screenNav, &ScreenNav::screenSelected,       this, &MainWindow::onScreenSelected);

    // FindBar wiring
    connect(_findBar, &FindBar::searchRequested,    _edit, &EditController::onSearchRequested);
    connect(_findBar, &FindBar::redactAllRequested, _edit, &EditController::onRedactAllRequested);
    connect(_findBar, &FindBar::closeRequested,     this,  &MainWindow::toggleFindBar);

    // Viewer signals wiring for page display in Status Bar
    connect(_modes->viewer(), &PdfViewerWidget::pageChanged, this, [this](int currentPage, int totalPages) {
        _status->setPage(currentPage + 1, totalPages);
        auto* viewer = pdfViewer();
        if (viewer) {
            _status->updateFromDocument(_ctx->pdfEditor.get(), viewer->filePath());
        }
    });

    // Page operations wiring
    connect(_modes->viewer(), &PdfViewerWidget::cropRequested, _pages, &PagesController::onCropRequested);
    if (auto* thumbSidebar = _left->findChild<ThumbnailSidebar*>()) {
        connect(thumbSidebar, &ThumbnailSidebar::pageReordered, _pages, &PagesController::onPageReordered);
    }

    applyTheme();
}

PdfViewerWidget* MainWindow::pdfViewer() const {
    return _modes ? _modes->viewer() : nullptr;
}

void MainWindow::updateTitle() {
    auto* viewer = pdfViewer();
    if (!viewer || viewer->filePath().isEmpty()) {
        setWindowTitle("Glyph PDF");
        return;
    }
    QString name = QFileInfo(viewer->filePath()).fileName();
    bool dirty = _ctx && _ctx->document && _ctx->document->isDirty();
    setWindowTitle(QString("Glyph PDF — %1%2").arg(name).arg(dirty ? " *" : ""));
}

void MainWindow::openDocument(const QString& filePath) {
    if (filePath.isEmpty()) return;
    auto* viewer = pdfViewer();
    if (!viewer) return;
    if (viewer->loadDocument(filePath)) {
        if (_ctx && _ctx->document) {
            _ctx->document->setPath(filePath);
            _ctx->document->setClean(); // Mark clean on load!
        }
        updateTitle();
        _status->setPage(viewer->currentPage() + 1, viewer->pageCount());
        _status->updateFromDocument(_ctx->pdfEditor.get(), filePath);
        _status->updateUnsaved(false);
    } else {
        QMessageBox::warning(this, tr("Error"),
            tr("Could not open the PDF document:\n%1").arg(filePath));
    }
}

void MainWindow::toggleFindBar() {
    if (_findBar) {
        _findBar->setVisible(!_findBar->isVisible());
        if (_findBar->isVisible()) {
            _findBar->setFocus();
        }
    }
}

void MainWindow::setFullScreenMode(bool fullscreen) {
    _menu->setVisible(!fullscreen);
    _ribbon->setVisible(!fullscreen);
    _modeStrip->setVisible(!fullscreen);
    _left->setVisible(!fullscreen);
    _right->setVisible(!fullscreen);
    _screenNav->setVisible(!fullscreen);
    if (fullscreen) {
        showFullScreen();
    } else {
        showMaximized();
    }
}

void MainWindow::onToolActivated(const QString& id) {
    _status->setTool(id);
    _toolRegistry->activateFromString(id);
}

void MainWindow::onTabChanged(const QString& tab) {
    Q_UNUSED(tab);
}

void MainWindow::onModeChanged(const QString& m) {
    _status->setMode(m);
}

void MainWindow::onScreenSelected(const QString& id) {
    // Modal dialogs: open, then snap screen-nav back to standard.
    if (id == "compress") {
        CompressDialog(this).exec();
        _screenNav->setActive("");
        _modes->setScreen("");
        _status->setScreen("");
        return;
    }
    if (id == "watermark") {
        WatermarkDialog(this).exec();
        _screenNav->setActive("");
        _modes->setScreen("");
        _status->setScreen("");
        return;
    }

    // AI is a toggle, not a screen swap.
    if (id == "ai") {
        toggleAi();
        // Restore previous screen-nav check; AI doesn't replace the center.
        _screenNav->setActive(_modes->currentScreen());
        return;
    }

    _modes->setScreen(id);
    _status->setScreen(id);

    // Swap right panel for screens that own it.
    if (id == "signature") {
        if (!_sigPanel) _sigPanel = new SignaturesPanel(this);
        replaceRight(_sigPanel);
    } else if (id == "pdfa") {
        if (!_pdfaPanel) _pdfaPanel = new PdfAValidationPanel(this);
        replaceRight(_pdfaPanel);
    } else {
        replaceRight(_right);
    }
}

void MainWindow::replaceRight(QWidget* w) {
    auto* row = _modes->parentWidget();
    auto* rowLay = qobject_cast<QHBoxLayout*>(row->layout());
    if (!rowLay) return;

    // Hide all known right-side candidates, show only `w`.
    for (QWidget* candidate : QWidgetList{ _right, _sigPanel, _pdfaPanel, _ai }) {
        if (!candidate) continue;
        if (rowLay->indexOf(candidate) == -1) continue;
        candidate->setVisible(false);
    }
    if (rowLay->indexOf(w) == -1) rowLay->addWidget(w);
    w->setVisible(true);
    if (_aiVisible && _ai) {
        if (rowLay->indexOf(_ai) == -1) rowLay->addWidget(_ai);
        _ai->setVisible(true);
    }
}

void MainWindow::toggleAi() {
    if (!_ai) _ai = new AIChatPanel(this);
    _aiVisible = !_aiVisible;
    auto* row = _modes->parentWidget();
    auto* rowLay = qobject_cast<QHBoxLayout*>(row->layout());
    if (!rowLay) return;
    if (_aiVisible) {
        if (rowLay->indexOf(_ai) == -1) rowLay->addWidget(_ai);
        _ai->setVisible(true);
    } else if (_ai) {
        _ai->setVisible(false);
    }
}

void MainWindow::toggleTheme() {
    _isDark = !_isDark;
    Theme::setMode(_isDark ? Theme::Dark : Theme::Light);
    applyTheme();
}

void MainWindow::applyTheme() {
    QFile f(_isDark ? Theme::darkSheet() : Theme::lightSheet());
    if (f.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
    }
}

} // namespace gp

