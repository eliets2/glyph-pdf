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
#include <QShortcut>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QRegularExpression>

#include "ui/ShortcutHelpDialog.h"
#include "ui/PreferencesDialog.h"
#include "ui/RecoveryDialog.h"
#include "engines/AutosaveManager.h"
#include "ui/ErrorDialog.h"
#include "core/ErrorInfo.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "core/UpdateChecker.h"

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTimer>

namespace gp {

MainWindow::MainWindow(const AppContext* ctx, QWidget* parent) : QMainWindow(parent), _ctx(ctx) {
    setWindowTitle(tr("Glyph PDF — [No Document]"));
    setAccessibleName(tr("Glyph PDF main window"));
    resize(1480, 920);

    // Enable drag-and-drop (D5)
    setAcceptDrops(true);

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
    _modes->setAppContext(ctx);

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
    connect(_findBar, &FindBar::searchRequested,     _edit, &EditController::onSearchRequested);
    connect(_findBar, &FindBar::replaceRequested,    _edit, &EditController::onReplaceRequested);
    connect(_findBar, &FindBar::replaceAllRequested, _edit, &EditController::onReplaceAllRequested);
    connect(_findBar, &FindBar::redactAllRequested,  _edit, &EditController::onRedactAllRequested);
    connect(_findBar, &FindBar::closeRequested,      this,  &MainWindow::toggleFindBar);

    // Viewer signals wiring for page display in Status Bar
    connect(_modes->viewer(), &PdfViewerWidget::pageChanged, this, [this](int currentPage, int totalPages) {
        _status->setPage(currentPage + 1, totalPages);
        auto* viewer = pdfViewer();
        if (viewer) {
            _status->updateFromDocument(_ctx->pdfEditor.get(), viewer->filePath());
        }
    });

    // Jump-to-page from status bar
    connect(_status, &StatusBar::jumpToPageRequested, this, [this](int page) {
        auto* viewer = pdfViewer();
        if (viewer) viewer->goToPage(page);
    });

    // Back/forward keyboard shortcuts (Alt+Left, Alt+Right)
    auto* backShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Left), this);
    connect(backShortcut, &QShortcut::activated, this, [this]() {
        auto* viewer = pdfViewer();
        if (viewer) viewer->goBack();
    });
    auto* forwardShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Right), this);
    connect(forwardShortcut, &QShortcut::activated, this, [this]() {
        auto* viewer = pdfViewer();
        if (viewer) viewer->goForward();
    });

    // === Accessibility keyboard shortcuts ===

    // F6: Cycle major regions (ribbon → left sidebar → central view → right sidebar → status bar)
    auto* f6Shortcut = new QShortcut(QKeySequence(Qt::Key_F6), this);
    connect(f6Shortcut, &QShortcut::activated, this, [this]() {
        QWidget* regions[] = { _ribbon, _left, _modes, _right, _status };
        constexpr int N = 5;
        QWidget* current = QApplication::focusWidget();
        int startIdx = 0;
        for (int i = 0; i < N; ++i) {
            if (regions[i] && regions[i]->isAncestorOf(current)) {
                startIdx = (i + 1) % N;
                break;
            }
        }
        for (int j = 0; j < N; ++j) {
            QWidget* target = regions[(startIdx + j) % N];
            if (target && target->isVisible()) {
                target->setFocus(Qt::ShortcutFocusReason);
                return;
            }
        }
    });

    // Shift+F6: Reverse cycle
    auto* shiftF6Shortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F6), this);
    connect(shiftF6Shortcut, &QShortcut::activated, this, [this]() {
        QWidget* regions[] = { _ribbon, _left, _modes, _right, _status };
        constexpr int N = 5;
        QWidget* current = QApplication::focusWidget();
        int startIdx = N - 1;
        for (int i = 0; i < N; ++i) {
            if (regions[i] && regions[i]->isAncestorOf(current)) {
                startIdx = (i - 1 + N) % N;
                break;
            }
        }
        for (int j = 0; j < N; ++j) {
            QWidget* target = regions[(startIdx - j + N) % N];
            if (target && target->isVisible()) {
                target->setFocus(Qt::ShortcutFocusReason);
                return;
            }
        }
    });

    // F1: Open keyboard shortcuts help dialog
    auto* f1Shortcut = new QShortcut(QKeySequence(Qt::Key_F1), this);
    connect(f1Shortcut, &QShortcut::activated, this, [this]() {
        ShortcutHelpDialog dlg(this);
        dlg.exec();
    });

    // Ctrl+,: Open preferences dialog
    auto* prefsShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), this);
    connect(prefsShortcut, &QShortcut::activated, this, [this]() {
        PreferencesDialog dlg(this);
        dlg.exec();
    });

    // Selection word count display (D6)
    connect(_modes->viewer(), &PdfViewerWidget::textSelected, this, [this](const QString& text) {
        if (text.isEmpty()) {
            _status->setSelection(QString());
        } else {
            int words = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts).size();
            _status->setSelection(tr("%1 words").arg(words));
        }
    });

    // Page operations wiring
    connect(_modes->viewer(), &PdfViewerWidget::cropRequested, _pages, &PagesController::onCropRequested);
    if (auto* thumbSidebar = _left->findChild<ThumbnailSidebar*>()) {
        connect(thumbSidebar, &ThumbnailSidebar::pageReordered, _pages, &PagesController::onPageReordered);
    }

    initUpdateChecker();

    applyTheme();

    if (_ctx && _ctx->autosave) {
        _ctx->autosave->start();
    }

    // Auto-prune missing recent files on startup (if enabled in Preferences)
    if (_home && QSettings().value("recent/autoPrune", false).toBool()) {
        _home->pruneMissingRecents();
        if (_menu) _menu->refreshRecentFiles();
    }

    if (_ctx && _ctx->document && _home) {
        QTimer::singleShot(0, this, [this]() {
            QStringList recent = _home->recentFiles();
            QStringList orphans = DocumentSession::findOrphanedAutosaves(recent);
            if (!orphans.isEmpty()) {
                RecoveryDialog dlg(orphans, this);
                int res = dlg.exec();
                if (res == RecoveryDialog::Recover) {
                    QStringList selected = dlg.selectedFiles();
                    for (const auto &file : selected) {
                        recoverDocument(file);
                    }
                } else if (res == RecoveryDialog::Discard) {
                    for (const auto &file : orphans) {
                        QFile::remove(file + ".autosave.pdf");
                    }
                }
            }
        });
    }
}

MainWindow::~MainWindow() {
    if (_ctx && _ctx->autosave) {
        _ctx->autosave->stop();
    }
}

void MainWindow::recoverDocument(const QString& originalPath) {
    QString autosavePath = originalPath + ".autosave.pdf";
    auto* viewer = pdfViewer();
    if (!viewer) return;

    if (viewer->loadDocument(autosavePath)) {
        if (_ctx && _ctx->document) {
            _ctx->document->setPath(originalPath);
            _ctx->document->markDirty();
        }
        _home->addRecentFile(originalPath);
        _menu->refreshRecentFiles();

        updateTitle();
        _status->setPage(viewer->currentPage() + 1, viewer->pageCount());
        _status->updateFromDocument(_ctx->pdfEditor.get(), originalPath);
        _status->updateUnsaved(true);
        statusBar()->showMessage(tr("Recovered from autosave. Please Save to restore permanently."));
    }
}

PdfViewerWidget* MainWindow::pdfViewer() const {
    return _modes ? _modes->viewer() : nullptr;
}

void MainWindow::activateScreen(const QString& id) {
    onScreenSelected(id);
}

void MainWindow::updateTitle() {
    auto* viewer = pdfViewer();
    if (!viewer || viewer->filePath().isEmpty()) {
        setWindowTitle(tr("Glyph PDF"));
        return;
    }
    QString name = QFileInfo(viewer->filePath()).fileName();
    bool dirty = _ctx && _ctx->document && _ctx->document->isDirty();
    setWindowTitle(tr("Glyph PDF — %1%2").arg(name).arg(dirty ? " *" : ""));
}

void MainWindow::openDocument(const QString& filePath) {
    if (filePath.isEmpty()) return;
    auto* viewer = pdfViewer();
    if (!viewer) return;

    if (viewer->loadDocument(filePath)) {
        if (_ctx && _ctx->document) {
            _ctx->document->setPath(filePath);
            _ctx->document->setClean();
        }
        // Track recent files (D4)
        _home->addRecentFile(filePath);
        _menu->refreshRecentFiles();

        updateTitle();
        _status->setPage(viewer->currentPage() + 1, viewer->pageCount());
        _status->updateFromDocument(_ctx->pdfEditor.get(), filePath);
        _status->updateUnsaved(false);

        // Check if the engine reported a repair warning (D4)
        if (_ctx && _ctx->pdfEditor) {
            ErrorInfo err = _ctx->pdfEditor->lastError();
            if (err && err.severity == ErrorInfo::Warning) {
                ErrorDialog::show(err, this);
                _ctx->pdfEditor->clearError();
            }
        }
    } else {
        // Build error info — prefer engine detail, fall back to generic
        ErrorInfo err;
        if (_ctx && _ctx->pdfEditor) {
            err = _ctx->pdfEditor->lastError();
            _ctx->pdfEditor->clearError();
        }
        if (err.isOk()) {
            err = ErrorInfo::error(
                tr("Could not open the PDF document."),
                tr("Path: %1").arg(filePath),
                ErrorInfo::Retry);
        }
        err.sourceFile = filePath;

        int result = ErrorDialog::show(err, this);
        if (result == QDialog::Accepted) {
            // User clicked Retry
            openDocument(filePath);
        }
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
        CompressDialog(_ctx, this).exec();
        _screenNav->setActive("");
        _modes->setScreen("");
        _status->setScreen("");
        return;
    }
    if (id == "watermark") {
        WatermarkDialog(_ctx, this).exec();
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
    // Cycle: Dark → Light → HighContrast → Dark
    if (Theme::current() == Theme::Dark) {
        Theme::setMode(Theme::Light);
        _isDark = false;
    } else if (Theme::current() == Theme::Light) {
        Theme::setMode(Theme::HighContrast);
        _isDark = false;
    } else {
        Theme::setMode(Theme::Dark);
        _isDark = true;
    }
    applyTheme();
}

void MainWindow::applyTheme() {
    QFile f(Theme::sheetForMode(Theme::current()));
    if (f.open(QFile::ReadOnly)) {
        qApp->setStyleSheet(QString::fromUtf8(f.readAll()));
    }
}

// === Drag-and-drop support (D5) ==========================================

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.toLocalFile().toLower().endsWith(".pdf")) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;

    // If multiple PDFs dropped, open the first
    for (const QUrl& url : urls) {
        QString path = url.toLocalFile();
        if (path.toLower().endsWith(".pdf")) {
            openDocument(path);
            break;
        }
    }
    event->acceptProposedAction();
}

void MainWindow::initUpdateChecker() {
    QSettings settings;
    if (!settings.value("update/checkOnStartup", true).toBool())
        return;

    // --- Notification bar (hidden until update is found) ---
    _updateBar = new QFrame(this);
    _updateBar->setObjectName("updateBar");
    _updateBar->setStyleSheet(
        "QFrame#updateBar { background:#1a5fb4; border-bottom:1px solid #144a8a; }"
        "QFrame#updateBar QLabel { color:#fff; font-size:12px; }"
        "QFrame#updateBar QPushButton { color:#fff; background:#2e7de0;"
        " border:1px solid #5096e6; border-radius:3px; padding:3px 12px; }"
        "QFrame#updateBar QPushButton:hover { background:#4a93e8; }");
    _updateBar->setVisible(false);

    auto* barLay = new QHBoxLayout(_updateBar);
    barLay->setContentsMargins(12, 6, 12, 6);
    auto* barLabel = new QLabel;
    barLabel->setObjectName("updateBarLabel");
    barLay->addWidget(barLabel, 1);

    auto* downloadBtn = new QPushButton(tr("Download"));
    downloadBtn->setObjectName("updateDownloadBtn");
    barLay->addWidget(downloadBtn);

    auto* installBtn = new QPushButton(tr("Install && Restart"));
    installBtn->setObjectName("updateInstallBtn");
    installBtn->setVisible(false);
    barLay->addWidget(installBtn);

    auto* dismissBtn = new QPushButton(tr("Dismiss"));
    barLay->addWidget(dismissBtn);

    // Insert the bar right below the ribbon/modeStrip area
    auto* central = centralWidget();
    auto* col = qobject_cast<QVBoxLayout*>(central->layout());
    if (col) col->insertWidget(2, _updateBar);  // after ribbon + modeStrip

    // --- UpdateChecker ---
    _updater = new UpdateChecker(this);

    QString channel = settings.value("update/channel", "stable").toString();
    if (channel == "beta") {
        _updater->setManifestUrl(QUrl("https://glyphpdf.com/updates/beta.json"));
    }

    connect(_updater, &UpdateChecker::updateAvailable, this,
        [barLabel, downloadBtn, installBtn, this](const UpdateChecker::UpdateInfo& info) {
            barLabel->setText(tr("Update available: GlyphPDF v%1 (%2)")
                .arg(info.version, info.releaseDate));
            _updateBar->setVisible(true);
        });

    connect(downloadBtn, &QPushButton::clicked, this, [downloadBtn, barLabel, this]() {
        downloadBtn->setEnabled(false);
        barLabel->setText(tr("Downloading update..."));
        _updater->downloadUpdate();
    });

    connect(_updater, &UpdateChecker::downloadProgressChanged, this,
        [barLabel](int pct) {
            barLabel->setText(tr("Downloading update... %1%").arg(pct));
        });

    connect(_updater, &UpdateChecker::downloadReady, this,
        [barLabel, downloadBtn, installBtn](const QString&) {
            barLabel->setText(tr("Update downloaded — ready to install."));
            downloadBtn->setVisible(false);
            installBtn->setVisible(true);
        });

    connect(_updater, &UpdateChecker::downloadFailed, this,
        [barLabel, downloadBtn](const QString& reason) {
            barLabel->setText(tr("Download failed: %1").arg(reason));
            downloadBtn->setEnabled(true);
        });

    connect(installBtn, &QPushButton::clicked, this, [this]() {
        _updater->applyUpdate();
    });

    connect(_updater, &UpdateChecker::updateLaunched, this, []() {
        qApp->quit();
    });

    connect(dismissBtn, &QPushButton::clicked, this, [this]() {
        _updateBar->setVisible(false);
    });

    // Check after a short delay so the window finishes painting first
    QTimer::singleShot(3000, _updater, &UpdateChecker::checkForUpdates);
}

void MainWindow::pruneMissingRecents()
{
    if (!_home) return;
    const int removed = _home->pruneMissingRecents();
    if (_menu) _menu->refreshRecentFiles();
    if (removed > 0)
        _status->showMessage(
            tr("Removed %n missing recent file(s).", nullptr, removed), 4000);
    else
        _status->showMessage(tr("No missing recent files found."), 3000);
}

} // namespace gp

