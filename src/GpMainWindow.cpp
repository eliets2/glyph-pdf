// SPDX-License-Identifier: Apache-2.0
#include "GpMainWindow.h"

#include "shell/MenuBar.h"
#include "shell/Ribbon.h"
#include "shell/ModeStrip.h"
#include "shell/ScreenNav.h"
#include "shell/StatusBar.h"
#include "shell/Sidebar.h"
#include "shell/TaskNav.h"
#include "shell/TaskStateSync.h"

#include "modes/ModeController.h"
#include "modes/OCRMode.h"   // R07: lifecycle recovery is relayed to the review panel
#include "ui/WelcomeWidget.h"
#include "core/ToolId.h"
#include <QStackedWidget>
#include "modes/AIChatPanel.h"
#include "modes/SignaturesPanel.h"
#include "modes/PdfAValidationPanel.h"
#include "modes/MeasureMode.h"
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
#include "shell/EditPolicy.h"
#include "ui/FindBar.h"
#include "engines/DocumentSession.h"
#include "engines/PdfEditorEngine.h"
#include <QUndoStack>   // ARC01: history is scoped to one document at the open boundary
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
#include <QCloseEvent>
#include <QMimeData>
#include <QRegularExpression>

#include "ui/ShortcutHelpDialog.h"
#include "ui/PreferencesDialog.h"
#include "ui/UpdateDialog.h"
#include "ui/RecoveryDialog.h"
#include "engines/AutosaveManager.h"
#include "engines/ConversionManager.h"   // §9.16 P1: office/images → PDF (same engine as the Welcome cards)
#include "core/TempFileManager.h"       // §9.16 P1: tracked output dir for unified-flow conversions
#include "ui/ErrorDialog.h"
#include "core/ErrorInfo.h"

#include <QProgressDialog>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QDesktopServices>
#include <QUrl>
#include <QThread>
#include <QPointer>
#include <functional>
#include "core/interfaces/IPdfEditorEngine.h"
#include "core/UpdateChecker.h"
#include "engines/SafeSave.h"

#include <QFrame>
#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTimer>

namespace gp {

// The GUI-held-handle coordinator is process-wide (SafeSave is a namespace of
// free functions), and exactly one MainWindow owns it at a time. The owner
// check makes a stale lambda from a destroyed window inert (tests create and
// destroy windows in sequence).
namespace {
QPointer<MainWindow> g_fileHandleCoordinatorOwner;
}

MainWindow::MainWindow(AppContext ctx, QWidget* parent)
    : QMainWindow(parent), _ownedCtx(std::move(ctx)), _ctx(&_ownedCtx) {
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
    _modes->setAppContext(_ctx);

    _left->init(_ctx, _modes->viewer());
    _right->init(_ctx, _modes->viewer());

    rowLay->addWidget(_left);
    rowLay->addWidget(_modes, 1);
    rowLay->addWidget(_right);

    col->addWidget(row, 1);

    // === Bottom chrome
    _screenNav = new ScreenNav(this);
    col->addWidget(_screenNav);

    // === Welcome/start screen wraps the workspace. The whole workspace host
    // (ribbon + panels + viewer) lives at index 1; the welcome page at index 0 is
    // shown on launch and whenever no document is open. A document load switches to
    // the workspace (see openDocument()/showWorkspace()). Previously WelcomeWidget
    // was compiled but never instantiated, so the app opened straight into the
    // empty doc UI.
    _welcome = new WelcomeWidget(this);
    _rootStack = new QStackedWidget(this);
    _rootStack->addWidget(_welcome);   // index 0
    _rootStack->addWidget(central);    // index 1
    _rootStack->setCurrentWidget(_welcome);
    setCentralWidget(_rootStack);

    _status = new StatusBar(this);
    setStatusBar(_status);

    // === Instantiate Controllers
    _home = new HomeController(_ctx, this, this);
    _view = new ViewController(_ctx, this, this);
    _edit = new EditController(_ctx, this, this);
    _pages = new PagesController(_ctx, this, this);
    _convert = new ConvertController(_ctx, this, this);
    _forms = new FormsController(_ctx, this, this);
    _security = new SecurityController(_ctx, this, this);

    _toolRegistry = new ToolRegistry(this);
    _toolRegistry->registerController(_home);
    _toolRegistry->registerController(_view);
    _toolRegistry->registerController(_edit);
    _toolRegistry->registerController(_pages);
    _toolRegistry->registerController(_convert);
    _toolRegistry->registerController(_forms);
    _toolRegistry->registerController(_security);

    // Engine-lane residual (step-2 ledger note, EC01 follow-up): same-path
    // engine writes failed "Access is denied" at the SafeSave commit while the
    // viewer's QPdfDocument held the file open — in-place rotate/save-in-place
    // were unusable regardless of backend readiness. The shared commit
    // boundary now asks the shell to release and restore the viewer handle
    // around every replacement. Installed ONCE here, so every engine mutation,
    // save-in-place, redaction commit and form import coordinates through the
    // same boundary (no per-call-site coordination).
    g_fileHandleCoordinatorOwner = this;
    SafeSave::setFileHandleCoordinator(
        [this](const QString &p) {
            if (g_fileHandleCoordinatorOwner != this || QThread::currentThread() != thread())
                return;   // background same-path writers keep the honest failure
            if (auto *v = pdfViewer()) v->parkDocumentForWrite(p);
        },
        [this](const QString &p) {
            if (g_fileHandleCoordinatorOwner != this || QThread::currentThread() != thread())
                return;
            if (auto *v = pdfViewer()) v->restoreDocumentAfterWrite(p);
        });

    // === Welcome-screen actions (route to the same handlers as the ribbon/menu).
    if (_welcome && _home) {
        _welcome->setRecentFiles(_home->recentFiles());
        connect(_welcome, &WelcomeWidget::openFileRequested, this,
                [this]{ _home->activate(ToolId::Open); });
        connect(_welcome, &WelcomeWidget::importOfficeRequested, this,
                [this]{ _home->activate(ToolId::ImportOffice); });
        connect(_welcome, &WelcomeWidget::imagesToPdfRequested, this,
                [this]{ _home->activate(ToolId::ImagesToPdf); });
        // Convert/Protect operate on a loaded PDF and have no standalone screen —
        // open a document first; the relevant tools then live in the ribbon.
        connect(_welcome, &WelcomeWidget::convertRequested, this,
                [this]{ _home->activate(ToolId::Open); });
        connect(_welcome, &WelcomeWidget::protectRequested, this,
                [this]{ _home->activate(ToolId::Open); });
        // Merge card → Combine tool (ConvertController). Combine is the one
        // Convert tool that runs with no document open (it gathers its own
        // file list), so route it through the registry rather than forcing an
        // Open first.
        connect(_welcome, &WelcomeWidget::mergeFilesRequested, this,
                [this]{ _toolRegistry->activate(ToolId::Combine); });
        connect(_welcome, &WelcomeWidget::recentFileRequested, this,
                [this](const QString& p){ openDocument(p); });
        connect(_welcome, &WelcomeWidget::removeRecentFileRequested, this,
                [this](const QString& p){
                    _home->removeFromRecents(p);
                    _welcome->setRecentFiles(_home->recentFiles());
                });
    }

    _modeStrip->init(_ctx);

    if (_ctx && _ctx->document) {
        connect(_ctx->document.get(), &DocumentSession::dirtyChanged, this, [this](bool dirty) {
            _status->updateUnsaved(dirty);
            if (!dirty) {
                _modeStrip->setAutosaveTime(QDateTime::currentDateTime());
            }
            updateTitle();
        });
        // §9.1 P0 (priority defect): page-mutation commands (rotate/resize/
        // header-footer/Bates/reorder/crop) call DocumentSession::markReload(),
        // but the emitted reloadRequested() was connected nowhere — the visible
        // QPdfView never reloaded and e.g. rotation only spun the overlay.
        // Honor the signal once, here, for every command that trusts it.
        connect(_ctx->document.get(), &DocumentSession::reloadRequested, this, [this]() {
            if (auto* viewer = pdfViewer())
                viewer->reload();
        });
        // Step-3 history truthfulness (EC03/EC05/V02): a command whose initial
        // mutation or restoration FAILED reports it — the status bar is the
        // one surface every controller already shares.
        connect(_ctx->document.get(), &DocumentSession::mutationFailed, this, [this](const QString &reason) {
            statusBar()->showMessage(reason, 7000);
        });
    }

    // ARC04: annotation edits dirty the session through the SAME pipeline as
    // command mutations. The viewer suppresses its own (re)loads, so open and
    // switch never dirty the freshly published identity.
    if (_ctx && _ctx->document) {
        connect(_modes->viewer(), &PdfViewerWidget::annotationEdited, this, [this]() {
            if (_ctx && _ctx->document)
                _ctx->document->markDirty();
        });
        // G14 (QUALITY-GATE-2026-09-09): a document whose sidecar recorded
        // UNEMBEDDED annotation work reopens PENDING — the session goes dirty
        // so the unsaved-PDF state survives the reopen (the annotations used
        // to come back with a CLEAN session while the PDF on disk lacked
        // them). Committed sidecars reopen clean; loads never fabricate work.
        connect(_modes->viewer(), &PdfViewerWidget::pendingEmbedAnnotationsRestored, this, [this](bool pending) {
            if (pending && _ctx && _ctx->document)
                _ctx->document->markDirty();
        });
    }

    // ARC07 (TEAM-ARCHITECTURE-REVIEW-2026-09-07): the session is the ONE
    // read-only authority. The viewer mirrors it (its cursor/tool gate keeps
    // working as defense in depth) and the registry's action enablement
    // re-syncs from the same signal — the open boundary decides the state,
    // this wiring only consumes it.
    if (_ctx && _ctx->document) {
        connect(_ctx->document.get(), &DocumentSession::readOnlyChanged, this,
                [this](bool readOnly) {
                    if (auto* viewer = pdfViewer())
                        viewer->setReadOnly(readOnly);
                    if (_toolRegistry)
                        _toolRegistry->refreshEnabledActions();
                });
    }
    // ARC07: the shared dispatch gate refused a mutating tool. One refusal
    // wording in ONE place instead of scattered per-controller messages.
    connect(_toolRegistry, &ToolRegistry::toolRefused, this, [this]() {
        statusBar()->showMessage(EditPolicy::readOnlyMessage(), 5000);
    });

    // === Wire signals
    connect(_ribbon, &Ribbon::toolActivated, this, &MainWindow::onToolActivated);
    connect(_ribbon, &Ribbon::tabChanged,    this, &MainWindow::onTabChanged);
    connect(_modeStrip, &ModeStrip::modeChanged,         this, &MainWindow::onModeChanged);
    connect(_modeStrip, &ModeStrip::themeToggleRequested, this, &MainWindow::toggleTheme);
    connect(_modeStrip, &ModeStrip::aiToggleRequested,    this, &MainWindow::toggleAi);
    connect(_modeStrip, &ModeStrip::taskSelected,         this, &MainWindow::activateScreen);
    connect(_screenNav, &ScreenNav::screenSelected,       this, &MainWindow::onScreenSelected);

    // === U02: one writer for visible navigation state.
    // The sync object owns every ScreenNav checked-state / status current-task
    // line / ModeStrip pill / Ribbon tab update from here on. Applying Standard
    // once at startup replaces the widgets' divergent defaults with one
    // coherent reading state.
    _taskSync = new TaskStateSync(_screenNav, _status, _modeStrip, _ribbon, this);
    _taskSync->apply(_modes->currentScreen());

    // OCR Verify screen <-> real OCR pipeline: the screen's Run button drives
    // EditController::runOcr, and recognised words flow back to the review panes.
    connect(_modes, &ModeController::ocrRunRequested, _edit, &EditController::runOcr);
    connect(_edit, &EditController::ocrResultsReady, _modes, &ModeController::deliverOcrResults);
    // U03: the full review session (source page image + word boxes) follows
    // the words so the scan pane shows the real source image instead of a
    // text-only word list. Same-thread direct connection — the image buffer
    // is shared, never re-rendered or copied.
    connect(_edit, &EditController::ocrReviewReady, _modes, &ModeController::deliverOcrReview);
    // R07 (F11): every terminal OCR outcome reaches the review panel so
    // Run/Accept are never left stuck. The panel is located at emit time (it is
    // created lazily by ModeController); a destroyed panel is simply not found,
    // so it can never receive a callback.
    connect(_edit, &EditController::ocrRunFailed, this, [this](const QString& message) {
        statusBar()->showMessage(message, 7000);
        if (auto* om = _modes->findChild<OCRMode*>())
            om->notifyOcrFailed(message);
    });
    connect(_edit, &EditController::ocrRunAbandoned, this, [this](const QString& message) {
        statusBar()->showMessage(message, 5000);
        if (auto* om = _modes->findChild<OCRMode*>())
            om->notifyOcrCanceled(message);
    });
    connect(_edit, &EditController::ocrSaveFinished, this,
            [this](bool saved, bool canceled, const QString& message) {
        if (!message.isEmpty())
            statusBar()->showMessage(message, saved ? 8000 : 7000);
        if (auto* om = _modes->findChild<OCRMode*>())
            om->notifySaveFinished(saved, canceled, message);
    });
    // OCR review workflow. Accept: the recognised text was already delivered to
    // the review panes (and is applied via the OCR pipeline), so confirm it.
    // Reject: OCRMode has already cleared its overlay/results locally; surface a
    // status message. Re-OCR region: re-run OCR (whole page until per-region
    // bbox mapping ships — same EditController slot as the Run button).
    connect(_modes, &ModeController::ocrReviewAccepted, this, [this]() {
        // §9.4 P0: Accept persists the searchable text layer via the same
        // production MRC PDF/A writer Batch Mode uses — the PRD headline
        // OCR promise, previously a silent no-op.
        // R08 (F04): the review panel's reviewed word records travel with
        // acceptance — corrections are authoritative, the plain-text pane is
        // only a preview — and the controller validates the session's source
        // identity/page before exporting.
        QList<OcrReviewedWord> reviewed;
        if (auto* om = _modes->findChild<OCRMode*>())
            reviewed = om->reviewedWords();
        _edit->onOcrAcceptRequested(reviewed);
    });
    connect(_modes, &ModeController::ocrReviewRejected, this, [this]() {
        statusBar()->showMessage(tr("OCR results rejected — overlay cleared."), 3000);
    });
    connect(_modes, &ModeController::ocrReRunRegionRequested, _edit,
            [this](QRectF) { _edit->runOcr(); });
    // §9.8 P0: RedactMode marking feedback lands on the status bar.
    connect(_modes, &ModeController::redactStatusMessage, this,
            [this](const QString& msg) { statusBar()->showMessage(msg, 6000); });
    // §9.8 P1: RedactMode's Cancel/Exit returns to the standard canvas — the
    // same "back" contract the modal task surfaces use after their dialog
    // closes (onScreenSelected("") → setScreen("") + one-writer state sync).
    // Placed marks stay on the viewer; the message says so honestly.
    connect(_modes, &ModeController::redactExitRequested, this, [this]() {
        activateScreen(QString());
        // N07 (review 2026-09-07): the empty screen shows the shared viewer
        // again, so the visible canvas must be left in the neutral navigation
        // state — the marking tool staying armed would let an ordinary drag
        // place a new mark. RedactMode disarms its own tool; this host-side
        // sync also covers a Redact tool armed by any other path (e.g. the
        // ribbon's Mark-Redact) before the exit.
        if (auto* viewer = pdfViewer())
            viewer->setToolMode(ToolMode::HandTool);
        statusBar()->showMessage(
            tr("Redaction closed — placed marks are kept on the document."), 6000);
    });

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
    // §9.1 P0: viewer rotate buttons drive the real engine-side page rotation.
    connect(_modes->viewer(), &PdfViewerWidget::requestPageRotation, _pages, &PagesController::onPageRotateRequested);
    // §9.1: inject the engine's link reader so the viewer can fetch clickable
    // link annotations (URI + GoTo) without depending on a concrete backend.
    if (_ctx && _ctx->pdfEditor) {
        std::weak_ptr<IPdfEditorEngine> weakEngine = _ctx->pdfEditor;
        _modes->viewer()->setLinkReader([weakEngine](const QString &path, int page) {
            if (auto engine = weakEngine.lock())
                return engine->extractLinks(path, page);
            return QList<PdfLinkInfo>();
        });
    }
    if (auto* thumbSidebar = _left->findChild<ThumbnailSidebar*>()) {
        connect(thumbSidebar, &ThumbnailSidebar::pageReordered, _pages, &PagesController::onPageReordered);
    }

    initUpdateChecker();

    applyTheme();

    if (_ctx && _ctx->autosave) {
        _ctx->autosave->start();
    }

    // Auto-prune missing recent files on startup
    if (_home) {
        QTimer::singleShot(0, this, [this]() {
            const int pruned = _home->pruneMissingRecents();
            if (pruned > 0)
                qDebug() << "[HomeController] Pruned" << pruned << "missing recent file(s).";
            if (_menu) _menu->refreshRecentFiles();
        });
    }

    if (_ctx && _ctx->document && _home) {
        QTimer::singleShot(0, this, [this]() {
            QStringList recent = _home->recentFiles();
            QStringList orphans = DocumentSession::findOrphanedAutosaves(recent);
            // G05 integration fix (merge break, 2026-09-09): a recovery pair
            // whose session is LIVE in this window is not an orphan prompt.
            // recoverDocument() publishes the original to recents while the
            // recovery is unsaved BY DESIGN, and the recovery input is newer
            // than the original by construction — findOrphanedAutosaves'
            // "autosave newer" heuristic would therefore flag the very
            // document the user is ALREADY recovering and pop this modal over
            // the active session (observed as a test hang whenever the pair's
            // mtimes differ). Suppress only the CURRENT session's pair; every
            // genuine leftover still prompts.
            if (_ctx && _ctx->document && !_ctx->document->recoverySource().isEmpty()) {
                orphans.removeAll(_ctx->document->path());
            }
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
    // The coordinator lambda captures `this` — clear it BEFORE the members it
    // touches (viewer, controllers) are torn down, unless a newer window owns it.
    if (g_fileHandleCoordinatorOwner == this) {
        g_fileHandleCoordinatorOwner = nullptr;
        SafeSave::setFileHandleCoordinator({}, {});
    }
    if (_ctx && _ctx->autosave) {
        _ctx->autosave->stop();
    }
}

void MainWindow::recoverDocument(const QString& originalPath) {
    QString autosavePath = originalPath + ".autosave.pdf";
    auto* viewer = pdfViewer();
    if (!viewer) return;

    // ARC05/ARC01: recovery is an open — establish the editing backend first
    // (on the autosave copy's bytes) and refuse the recovery if that fails,
    // instead of publishing an identity the engine cannot serve.
    if (!_ctx || !_ctx->pdfEditor || !_ctx->pdfEditor->loadDocumentForEditing(autosavePath)) {
        if (_ctx && _ctx->pdfEditor) _ctx->pdfEditor->clearError();
        statusBar()->showMessage(
            tr("Could not recover %1: the autosave copy could not be opened for editing.")
                .arg(originalPath), 5000);
        return;
    }

    if (viewer->loadDocument(autosavePath)) {
        // ARC01: new document identity for the recovered document (history
        // cleared below); the recovered copy is by definition still unsaved.
        if (_ctx && _ctx->document) {
            _ctx->document->beginDocument(originalPath);
            _ctx->document->markDirty();
            // G05 (QUALITY-GATE-2026-09-09): publish the recovery identity —
            // the session's path (the DESTINATION) is the original, while the
            // editing inputs hold the recovery copy (the INPUT). Save routes
            // the recovered content to the destination and clears this
            // binding only after the committed revision is re-anchored.
            _ctx->document->setRecoverySource(autosavePath);
            // WP-R09b (WHOLE-ARCHITECTURE-REVIEW A05): prime the DESTINATION
            // baseline. The recovery session edits the autosave INPUT while
            // Save commits to the ORIGINAL — the first recovery Save must be
            // conflict-guarded against the original's bytes as they are NOW,
            // so capture the baseline at bind time, before any edits.
            if (_ctx->pdfEditor)
                _ctx->pdfEditor->primeSourceBaseline(originalPath);
            // ARC07: the recovered copy is a NEW document identity — decide
            // its editability explicitly. The autosave copy carries the
            // original's §9.11 expiry metadata, so a recovered expired
            // document stays read-only; anything else recovers editable.
            const QDate recoveredExpiry = PdfEditorEngine::readExpiryDate(autosavePath);
            _ctx->document->setReadOnly(recoveredExpiry.isValid()
                                        && recoveredExpiry < QDate::currentDate());
        }
        if (_ctx && _ctx->undoStack) {
            _ctx->undoStack->clear();
        }
        _home->addRecentFile(originalPath);
        _menu->refreshRecentFiles();

        updateTitle();
        _status->setPage(viewer->currentPage() + 1, viewer->pageCount());
        _status->updateFromDocument(_ctx->pdfEditor.get(), originalPath);
        _status->updateUnsaved(true);
        // ARC06: same re-binding as openDocument — the recovered document is
        // a new identity and the panel must follow it while active.
        if (_pdfaPanel && _modes && _modes->currentScreen() == QLatin1String("pdfa"))
            refreshPdfAPanel();
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

void MainWindow::showWelcome() {
    if (_rootStack && _welcome) {
        if (_home) _welcome->setRecentFiles(_home->recentFiles());
        _rootStack->setCurrentWidget(_welcome);
    }
}

void MainWindow::showWorkspace() {
    if (_rootStack && _rootStack->count() > 1)
        _rootStack->setCurrentIndex(1);   // index 1 == workspace host
}

void MainWindow::openDocument(const QString& filePath) {
    if (filePath.isEmpty()) return;

    // §9.16 P1: openDocument is THE open choke point — File>Open (HomeController
    // ToolId::Open), the Welcome Open card, recent files and drag-and-drop all
    // land here. Non-PDF targets are routed to the SAME conversions the
    // explicit Welcome cards run, so the main open flow no longer refuses e.g.
    // a .docx with a bare "Could not open the PDF document" error. Unsupported
    // extensions fall through to the pre-existing load/error path unchanged.
    const OpenRoute route = routeForFile(filePath);
    if (route == OpenRoute::OfficeConvert) {
        convertAndOpenOffice(filePath);
        return;
    }
    if (route == OpenRoute::ImagesConvert) {
        convertAndOpenImages({ filePath });
        return;
    }

    auto* viewer = pdfViewer();
    if (!viewer) return;

    // G14 (P2, QUALITY-GATE-2026-09-09): explicit checked transition policy
    // when leaving UNEMBEDDED annotation work. Sidecar persistence keeps the
    // work alive, but the PDF on disk does not carry the annotations until a
    // Save commits them — so switching documents while the displayed document
    // has pending-embed annotations runs the SAME Save / Discard / Cancel
    // policy as the close boundary (closeEvent): Save must be a CHECKED
    // success (the annotations are really in the PDF); Discard proceeds and
    // keeps the sidecar-only durability; Cancel aborts the open with nothing
    // changed. Command-mutation dirty is deliberately NOT prompted here — the
    // accepted ARC01 switch semantics (history-scoped, mutations already on
    // disk) stay untouched.
    if (viewer->hasPendingEmbedAnnotations()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Unsaved Changes"));
        msgBox.setText(tr("The annotations on this document are not saved into the PDF yet."));
        msgBox.setInformativeText(tr("Do you want to save them into the PDF before switching?"));
        msgBox.setIcon(QMessageBox::Warning);
        auto* saveBtn    = msgBox.addButton(tr("Save"),    QMessageBox::AcceptRole);
        auto* discardBtn = msgBox.addButton(tr("Discard"), QMessageBox::DestructiveRole);
        msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
        msgBox.setDefaultButton(saveBtn);
        msgBox.exec();

        if (msgBox.clickedButton() == saveBtn) {
            const auto outcome = _home
                ? _home->saveNow()
                : HomeController::SaveOutcome::Failed;
            if (outcome != HomeController::SaveOutcome::Saved) {
                // The commit failed or was refused — the annotations are NOT
                // in the PDF; the open is aborted and the work stays open.
                statusBar()->showMessage(
                    tr("Still on %1 — the annotations could not be saved into the PDF.")
                        .arg(QFileInfo(viewer->filePath()).fileName()), 8000);
                return;
            }
        } else if (msgBox.clickedButton() != discardBtn) {
            return;   // Cancel: abort the open, keep the current document
        }
        // Discard (or a checked Saved): proceed — the viewer's identity
        // switch below flushes the sidecar state for the old path.
    }

    // ARC05 (P1, TEAM-ARCHITECTURE-REVIEW-2026-09-07): a successful Open must
    // establish BOTH sessions — the Qt viewer document AND the shared editing
    // backend — here, at the one open choke point. Previously the engine kept
    // its default-constructed (backend-less) state until some ad hoc tool flow
    // happened to initialize it, so immediate mutations failed with "No
    // document is open for editing" and pathless engine queries could still
    // describe a PREVIOUSLY loaded document after a switch. The engine load
    // runs BEFORE the new identity is published below: a failed load shows the
    // backend's own error and destroys nothing (retry re-runs the whole open).
    const bool engineReady = _ctx && _ctx->pdfEditor
        && _ctx->pdfEditor->loadDocumentForEditing(filePath);
    if (!engineReady) {
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
        return;
    }

    if (viewer->loadDocument(filePath)) {
        showWorkspace();   // leave the welcome screen now that a document is loaded
        // ARC01 (P1): publish a NEW document identity and scope the undo
        // history to it, at this single session boundary. Without this, A's
        // commands survived the switch and — because commands resolve the
        // session path at execution time — undoing them mutated B (or the
        // reloaded A). Same-path reopen (A→A) is included: it is a new
        // revision loaded from disk, not a continuation of the old history.
        if (_ctx && _ctx->document) {
            _ctx->document->beginDocument(filePath);
            // G14 (QUALITY-GATE-2026-09-09): the viewer reports pending-embed
            // sidecar work during the load — BEFORE this boundary clears the
            // dirty baseline — so re-assert it on the freshly published
            // identity: a reopened document whose annotations are not yet in
            // the PDF opens DIRTY, never clean-with-hidden-debt.
            if (viewer->hasPendingEmbedAnnotations())
                _ctx->document->markDirty();
        }
        if (_ctx && _ctx->undoStack) {
            _ctx->undoStack->clear();
        }
        // Track recent files (D4)
        _home->addRecentFile(filePath);
        _menu->refreshRecentFiles();

        updateTitle();
        _status->setPage(viewer->currentPage() + 1, viewer->pageCount());
        _status->updateFromDocument(_ctx->pdfEditor.get(), filePath);
        _status->updateUnsaved(false);

        // Document expiry (§9.11): if a glyph:ExpiryDate is set and has passed,
        // open the document read-only and warn the user.
        // ARC07: read-only is decided ONCE per open, ON THE SESSION (the
        // shared authority every controller consults via shell/EditPolicy.h);
        // the viewer mirrors it through readOnlyChanged. Every successful
        // open decides explicitly, so a previously read-only session cannot
        // leak into a fresh, non-expired document.
        const QDate expiry = PdfEditorEngine::readExpiryDate(filePath);
        const bool expired = expiry.isValid() && expiry < QDate::currentDate();
        if (_ctx && _ctx->document)
            _ctx->document->setReadOnly(expired);
        // Same decision, applied to the display gate directly: the mirror
        // signal only fires on a CHANGE of session state, so a fresh open
        // must also sync the viewer explicitly to stay in lockstep.
        viewer->setReadOnly(expired);
        if (expired) {
            QMessageBox box(QMessageBox::Warning, tr("Document Expired"),
                tr("This document expired on %1. It has been opened in read-only mode.")
                    .arg(expiry.toString(Qt::ISODate)),
                QMessageBox::Ok, this);
            box.exec();
        }

        // Check if the engine reported a repair warning (D4)
        if (_ctx && _ctx->pdfEditor) {
            ErrorInfo err = _ctx->pdfEditor->lastError();
            // A repair warning carries a message but is not an error-level
            // condition (isOk() is severity-based since AR-10 D3), so test the
            // message presence explicitly rather than operator bool().
            if (!err.userMessage.isEmpty() && err.severity == ErrorInfo::Warning) {
                ErrorDialog::show(err, this);
                _ctx->pdfEditor->clearError();
            }
        }

        // ARC06: a successful document change re-binds the PDF/A panel while
        // it is the active right panel (entering the screen re-binds it in
        // refreshPdfAPanel's other caller; a switch must not leave the panel
        // describing the previous document — or the empty state).
        if (_pdfaPanel && _modes && _modes->currentScreen() == QLatin1String("pdfa"))
            refreshPdfAPanel();
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
    const auto tool = toolIdFromString(id);

    // U02 — one behavior per task. Entry-route tools ARE task entries: every
    // chrome layer that activates them lands on the task's one screen and the
    // raw controller action is NOT also dispatched. This is the documented
    // decision for the verified defect where the ribbon OCR button ran the
    // pipeline (EditController::activate(ToolId::Ocr) → runOcr) while the
    // menu and ScreenNav opened the OCR Verify screen: an OCR entry now opens
    // the verify screen everywhere, and running a recognition stays inside it
    // (the screen's Run button drives EditController::runOcr). Compare,
    // Compress and Watermark entries route the same way, so the modal task
    // surfaces open exactly once through onScreenSelected.
    if (tool && TaskNav::isEntryRoute(*tool)) {
        _status->setTool(id);
        activateScreen(TaskNav::screenForTool(*tool));
        return;
    }

    _status->setTool(id);
    _toolRegistry->activateFromString(id);   // behavior source — unchanged

    // U02: after a real tool action ran, make visible state agree everywhere
    // (e.g. ribbon Mark-Redact lands on the Redaction screen, form-field
    // tools land on the Form Builder). Tools with no screen cause no sync.
    if (tool) {
        const QString screen = TaskNav::screenForTool(*tool);
        if (!screen.isEmpty())
            activateScreen(screen);
    }
}

void MainWindow::onTabChanged(const QString& tab) {
    Q_UNUSED(tab);
}

void MainWindow::onModeChanged(const QString& m) {
    // U02: the Form pill is a real navigation entry — it must land on the
    // Form Builder screen (which names "form" as its modePill), not merely
    // change text somewhere. Other pills stay mode-local in stage 1.
    if (m == QLatin1String("form"))
        activateScreen(QStringLiteral("form"));
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

    // U02: the sync object is the single writer of the visible state — nav
    // check-state, status current-task line, mode pill, ribbon tab. (The old
    // hand-written triplet missed ScreenNav on menu/ribbon-initiated
    // navigation; the funnel can no longer drift.)
    _taskSync->apply(id);

    // Swap right panel for screens that own it.
    if (id == "signature") {
        if (!_sigPanel) {
            _sigPanel = new SignaturesPanel(this);
            // Route "Place Signature" through the same ribbon Sign flow
            // (activate() is the public IToolController entry point; it also
            // guards on an open document before invoking signDocument()).
            connect(_sigPanel, &SignaturesPanel::placeSignatureRequested,
                    this, [this]() { if (_security) _security->activate(ToolId::Sign); });
        }
        // Populate the DIGITAL ID card with the real signatures in the open file.
        ISignatureManager* signing = _ctx ? _ctx->signing.get() : nullptr;
        auto* viewer = pdfViewer();
        const QString path = viewer ? viewer->filePath() : QString();
        _sigPanel->setDocument(path, signing);
        replaceRight(_sigPanel);
    } else if (id == "pdfa") {
        if (!_pdfaPanel) _pdfaPanel = new PdfAValidationPanel(this);
        _pdfaPanel->setExportPdfACallback(
            [this](const QString& dest, int level) -> bool {
                return _ctx && _ctx->pdfEditor && _ctx->pdfEditor->exportPdfA(dest, level);
            });
        // ARC06 (P2, TEAM-ARCHITECTURE-REVIEW-2026-09-07): entering the panel
        // must GIVE it the active document. setDocument() is the panel's only
        // production path setter and validation entry point — without this
        // call the panel kept its empty-path state ("No document loaded.")
        // even though the viewer had a PDF open.
        refreshPdfAPanel();
        replaceRight(_pdfaPanel);
    } else if (id == "measure") {
        if (!_measurePanel) {
            _measurePanel = new MeasureMode(this);
            // The panel drives the viewer's measure tool modes and reads
            // committed measurements back from its annotation layer; status
            // messages (calibration errors, disclosures) reach the status bar.
            _measurePanel->setViewer(pdfViewer());
            connect(_measurePanel, &MeasureMode::statusMessageRequested, this,
                    [this](const QString& m) { _status->setOperation(m); });
        }
        _measurePanel->setViewer(pdfViewer());
        replaceRight(_measurePanel);
    } else {
        replaceRight(_right);
    }
}

// ARC06: bind the PDF/A panel to the CURRENT document identity. Called on
// entry into the panel and after every successful document change (open,
// recovery, switch) while the panel is the active right panel — so the
// panel's validation, reading-order analysis and export all describe the
// document the user is looking at, never a stale or empty path.
void MainWindow::refreshPdfAPanel() {
    if (!_pdfaPanel) return;
    auto* viewer = pdfViewer();
    const QString path = viewer ? viewer->filePath() : QString();
    _pdfaPanel->setDocument(path);
}

void MainWindow::replaceRight(QWidget* w) {
    auto* row = _modes->parentWidget();
    auto* rowLay = qobject_cast<QHBoxLayout*>(row->layout());
    if (!rowLay) return;

    // Hide all known right-side candidates, show only `w`.
    for (QWidget* candidate : QWidgetList{ _right, _sigPanel, _pdfaPanel, _measurePanel, _ai }) {
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
// §9.16 P1: the accepted set is no longer PDF-only — Office documents and
// images are accepted too and routed (via planDrop, below) to the SAME
// LibreOffice / images-to-PDF conversions the Welcome cards run. The pure
// decision logic lives in routeForFile()/planDrop() and is pinned by
// tests/TestOpenRouting.cpp; these handlers are thin wrappers over it.

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        for (const QUrl& url : event->mimeData()->urls()) {
            if (routeForFile(url.toLocalFile()) != OpenRoute::Unsupported) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;

    QStringList localPaths;
    for (const QUrl& url : urls) {
        const QString path = url.toLocalFile();
        if (!path.isEmpty()) localPaths << path;
    }

    const DropPlan plan = planDrop(localPaths);
    if (plan.isEmpty())
        return;   // nothing routable — do not accept (pre-existing behavior)

    if (!plan.pdfToOpen.isEmpty())
        openDocument(plan.pdfToOpen);               // first PDF, as before
    else if (!plan.imagesToConvert.isEmpty())
        convertAndOpenImages(plan.imagesToConvert); // all images → one PDF
    else
        convertAndOpenOffice(plan.officeToConvert); // first Office file
    event->acceptProposedAction();
}

// === §9.16 P1: unified open routing =======================================

MainWindow::OpenRoute MainWindow::routeForFile(const QString& path) {
    // The extension sets mirror the existing Welcome-card dialogs EXACTLY, so
    // anything the explicit cards accept, the unified flow accepts too:
    //   - HomeController::onImportOffice: *.docx *.doc *.xlsx *.xls *.pptx
    //     *.ppt *.odt *.ods *.odp *.rtf *.csv *.txt  (.txt included —
    //     LibreOffice converts plain text)
    //   - HomeController::onImagesToPdf:  *.png *.jpg *.jpeg *.tif *.tiff *.bmp
    static const QStringList kOfficeExts {
        QStringLiteral("docx"), QStringLiteral("doc"),
        QStringLiteral("xlsx"), QStringLiteral("xls"),
        QStringLiteral("pptx"), QStringLiteral("ppt"),
        QStringLiteral("odt"),  QStringLiteral("ods"),
        QStringLiteral("odp"),  QStringLiteral("rtf"),
        QStringLiteral("csv"),  QStringLiteral("txt") };
    static const QStringList kImageExts {
        QStringLiteral("png"), QStringLiteral("jpg"),
        QStringLiteral("jpeg"), QStringLiteral("tif"),
        QStringLiteral("tiff"), QStringLiteral("bmp") };

    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("pdf"))
        return OpenRoute::PdfDirect;
    if (kOfficeExts.contains(ext))
        return OpenRoute::OfficeConvert;
    if (kImageExts.contains(ext))
        return OpenRoute::ImagesConvert;
    return OpenRoute::Unsupported;
}

MainWindow::DropPlan MainWindow::planDrop(const QStringList& localPaths) {
    DropPlan plan;
    // 1. First PDF wins (pre-existing drop behavior).
    for (const QString& p : localPaths) {
        if (routeForFile(p) == OpenRoute::PdfDirect) {
            plan.pdfToOpen = p;
            break;
        }
    }
    if (!plan.pdfToOpen.isEmpty())
        return plan;
    // 2. No PDF: every image in the drop combines into one PDF (mirrors
    //    multi-select in the Images-to-PDF card).
    for (const QString& p : localPaths) {
        if (routeForFile(p) == OpenRoute::ImagesConvert)
            plan.imagesToConvert << p;
    }
    if (!plan.imagesToConvert.isEmpty())
        return plan;
    // 3. No images: the first Office file is converted.
    for (const QString& p : localPaths) {
        if (routeForFile(p) == OpenRoute::OfficeConvert) {
            plan.officeToConvert = p;
            break;
        }
    }
    return plan;
}

// Shared conversion runner: the SAME progress-dialog + cancelable-watcher
// pattern the Welcome-card flows use in HomeController, ending with the
// converted PDF opened in the viewer. Output paths point into a tracked temp
// dir (TempFileManager) so a unified-flow open never silently overwrites a
// user file next to the source.
void MainWindow::runConversion(const QString& progressLabel,
                               const std::function<bool()>& work,
                               const QString& outputPath,
                               const QString& successMessage,
                               const QString& failureMessage) {
    auto* progress = new QProgressDialog(progressLabel, tr("Cancel"), 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(500);

    auto* watcher = new QFutureWatcher<bool>(this);
    QObject::connect(progress, &QProgressDialog::canceled, watcher, &QFutureWatcher<bool>::cancel);
    QObject::connect(watcher, &QFutureWatcher<bool>::finished, this, [=]() {
        progress->close();
        progress->deleteLater();
        if (watcher->isCanceled()) {
            watcher->deleteLater();
            return;
        }
        const bool ok = watcher->result();
        watcher->deleteLater();
        if (ok) {
            _status->showMessage(successMessage.arg(QFileInfo(outputPath).fileName()), 6000);
            openDocument(outputPath);   // .pdf → PdfDirect — loads in the viewer
        } else {
            QMessageBox::warning(this, tr("Conversion Failed"), failureMessage);
        }
    });

    watcher->setFuture(QtConcurrent::run(work));
    progress->show();
}

void MainWindow::convertAndOpenOffice(const QString& officePath) {
    // Same runtime detection + wording as the Welcome Import-Office card
    // (HomeController::onImportOffice): no converter is bundled; we use the
    // one already present and point the user at the download when absent.
    if (!ConversionManager::isOfficeImportAvailable()) {
        QMessageBox box(this);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(tr("Office Import Needs a Converter"));
        box.setText(tr("Importing Word, Excel and PowerPoint files to PDF uses "
                       "LibreOffice, which doesn't appear to be installed."));
        box.setInformativeText(tr("LibreOffice is free and open source. Install it once, "
                                  "then this feature works automatically — no GlyphPDF "
                                  "restart required."));
        QPushButton *download = box.addButton(tr("Download LibreOffice…"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(download);
        box.exec();
        if (box.clickedButton() == download)
            QDesktopServices::openUrl(QUrl("https://www.libreoffice.org/download/download/"));
        return;
    }

    const QString outDir = TempFileManager::instance().createTempDir(QStringLiteral("glyphpdf-open"));
    const QString outputPath =
        outDir + QLatin1Char('/') + QFileInfo(officePath).completeBaseName() + QLatin1String(".pdf");

    runConversion(
        tr("Converting %1 to PDF…").arg(QFileInfo(officePath).fileName()),
        [officePath, outputPath]() {
            ConversionManager mgr;
            return mgr.convertOfficeToPdf(officePath, outputPath);
        },
        outputPath,
        tr("Converted to PDF: %1"),
        tr("Could not convert '%1' to PDF.\n"
           "Ensure LibreOffice is installed and the file is not password-protected.")
            .arg(QFileInfo(officePath).fileName()));
}

void MainWindow::convertAndOpenImages(const QStringList& imagePaths) {
    if (imagePaths.isEmpty())
        return;

    const QString outDir = TempFileManager::instance().createTempDir(QStringLiteral("glyphpdf-open"));
    // Name the result after the first image so the viewer title is useful
    // (mirrors the card's "images.pdf" default for a multi-image drop).
    const QString outputPath =
        outDir + QLatin1Char('/') + QFileInfo(imagePaths.first()).completeBaseName() + QLatin1String(".pdf");

    runConversion(
        tr("Building PDF from %1 image(s)…").arg(imagePaths.size()),
        [imagePaths, outputPath]() {
            ConversionManager mgr;
            return mgr.convertImagesToPdf(imagePaths, outputPath);
        },
        outputPath,
        tr("Images combined into PDF: %1"),
        tr("Could not combine images into a PDF.\n"
           "Ensure all selected files are valid image files."));
}

// AR-7 D4: prompt Save / Discard / Cancel when quitting with unsaved changes.
// ARC03 (P1, TEAM-ARCHITECTURE-REVIEW-2026-09-07): "save initiated" was
// treated as persistence — the window closed even when the save failed,
// silently discarding the user's work and their chance to retry. The close
// now proceeds ONLY on a checked Saved outcome (or the explicit Discard
// choice); a failed or canceled save keeps the window and the document open
// with the dirty state and history intact for a retry.
void MainWindow::closeEvent(QCloseEvent* event) {
    const bool dirty = _ctx && _ctx->document && _ctx->document->isDirty();
    if (dirty) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Unsaved Changes"));
        msgBox.setText(tr("The document has unsaved changes."));
        msgBox.setInformativeText(tr("Do you want to save before closing?"));
        msgBox.setIcon(QMessageBox::Warning);
        auto* saveBtn    = msgBox.addButton(tr("Save"),    QMessageBox::AcceptRole);
        auto* discardBtn = msgBox.addButton(tr("Discard"), QMessageBox::DestructiveRole);
        msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);
        msgBox.setDefaultButton(saveBtn);
        msgBox.exec();

        if (msgBox.clickedButton() == saveBtn) {
            const auto outcome = _home
                ? _home->saveNow()
                : HomeController::SaveOutcome::Failed;
            if (outcome != HomeController::SaveOutcome::Saved) {
                // The save failed or was refused — the work is NOT on disk.
                event->ignore();
                return;
            }
            event->accept();
        } else if (msgBox.clickedButton() == discardBtn) {
            event->accept();   // explicit Discard — the only non-Saved way past
        } else {
            // Cancel pressed: do not close.
            event->ignore();
            return;
        }
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::initUpdateChecker() {
    QSettings settings;
    // AR-8 D6: default OFF (audit preference; first-run consent notice in v1.3.1
    // explains the update check to users who later opt in via Preferences).
    if (!settings.value("update/checkOnStartup", false).toBool())
        return;

    // One-time transparency notice — tell the user we check for updates and how
    // to turn it off, then never prompt again.
    if (!settings.value("update/firstRunNoticeShown", false).toBool()) {
        settings.setValue("update/firstRunNoticeShown", true);
        QMessageBox::information(this, tr("Software updates"),
            tr("GlyphPDF checks GitHub for new versions when it starts and shows a "
               "dismissible banner if one is available. It never downloads or installs "
               "anything without your click, and sends no usage data.\n\n"
               "You can turn this off under Preferences \xE2\x96\xB8 Updates."));
    }

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

    auto* viewBtn = new QPushButton(tr("View Update"));
    viewBtn->setObjectName("updateViewBtn");
    barLay->addWidget(viewBtn);

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
        _updater->setManifestUrl(UpdateChecker::manifestUrlForChannel(channel));
    }

    // Latest update info, so "View Update" can open the dialog on demand.
    auto pending = std::make_shared<UpdateChecker::UpdateInfo>();

    connect(_updater, &UpdateChecker::updateAvailable, this,
        [this, barLabel, pending](const UpdateChecker::UpdateInfo& info) {
            *pending = info;
            barLabel->setText(tr("GlyphPDF v%1 is available.").arg(info.version));
            _updateBar->setVisible(true);
        });

    // The download/verify/install flow lives in the polished modal dialog.
    connect(viewBtn, &QPushButton::clicked, this, [this, pending]() {
        if (pending->version.isEmpty()) return;
        UpdateDialog dlg(_updater, *pending, this);
        dlg.exec();
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

