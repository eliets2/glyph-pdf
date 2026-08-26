// SPDX-License-Identifier: Apache-2.0
#include "MenuBar.h"
#include "GpMainWindow.h"
#include "ui/PdfViewerWidget.h"
#include "ui/ShortcutHelpDialog.h"
#include "ui/PreferencesDialog.h"
#include "core/UpdateChecker.h"
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QApplication>
#include <QFileInfo>
#include <QSettings>
#include <QStatusBar>
#include <QHash>

namespace gp {

// ── Single source of truth ───────────────────────────────────────────────
// Every actionable menu item, classified by how it is dispatched. The
// constructor consults this table (via the dispatch lookup below) to decide
// which items must be rendered disabled, and TestMenuBarIntegrity enumerates
// the same table to prove no item silently no-ops. When you add or remove a
// menu item, update this table in the same commit.
const QList<MenuActionSpec>& MenuBar::actionSpecs() {
    static const QList<MenuActionSpec> specs = {
        // ── File ──
        { "new-window",   MenuDispatch::Disabled },  // multi-window not shipped (ribbon: "newWin")
        { "open",         MenuDispatch::Registry },
        { "close",        MenuDispatch::Local    },
        { "save",         MenuDispatch::Registry },
        { "saveAs",       MenuDispatch::Registry },
        { "save-copy",    MenuDispatch::Local    },  // routed to SaveAs
        { "printPreview", MenuDispatch::Registry },
        { "pageSetup",    MenuDispatch::Registry },
        { "print",        MenuDispatch::Registry },
        { "exportPresets",MenuDispatch::Registry },
        { "share",        MenuDispatch::Registry },
        { "properties",   MenuDispatch::Registry },
        { "exit",         MenuDispatch::Local    },

        // ── Edit ──
        { "undo",         MenuDispatch::Registry },
        { "redo",         MenuDispatch::Registry },
        { "cut",          MenuDispatch::Disabled },  // clipboard editing not shipped
        { "copy",         MenuDispatch::Disabled },
        { "paste",        MenuDispatch::Disabled },
        { "delete",       MenuDispatch::Disabled },  // ribbon: "delete"
        { "select-all",   MenuDispatch::Disabled },
        { "find",         MenuDispatch::Local    },
        { "find-replace", MenuDispatch::Local    },
        { "preferences",  MenuDispatch::Local    },

        // ── View ──
        { "zoomIn",       MenuDispatch::Registry },
        { "zoomOut",      MenuDispatch::Registry },
        { "actual",       MenuDispatch::Registry },
        { "fitPage",      MenuDispatch::Registry },
        { "fitWidth",     MenuDispatch::Registry },
        { "single",       MenuDispatch::Registry },
        { "continuous",   MenuDispatch::Registry },
        { "two-page",     MenuDispatch::Registry },
        { "presentation", MenuDispatch::Registry },
        { "fullscreen",   MenuDispatch::Local    },
        { "darkMode",     MenuDispatch::Local    },
        { "rulers",       MenuDispatch::Disabled },  // view guides not shipped
        { "guides",       MenuDispatch::Disabled },
        { "grid",         MenuDispatch::Disabled },

        // ── Document ──
        { "insert-page",     MenuDispatch::Registry },
        { "delete-page",     MenuDispatch::Registry },
        { "extract-page",    MenuDispatch::Registry },  // alias → Extract
        { "rotate-cw",       MenuDispatch::Registry },
        { "rotate-ccw",      MenuDispatch::Registry },
        { "crop",            MenuDispatch::Registry },
        { "resize",          MenuDispatch::Registry },
        { "page-numbers",    MenuDispatch::Registry },
        { "headers-footers", MenuDispatch::Registry },  // alias → AddHeader

        // ── Tools ──
        { "ocr",          MenuDispatch::Local    },
        { "redact",       MenuDispatch::Local    },
        { "compare",      MenuDispatch::Local    },
        { "compress",     MenuDispatch::Registry },
        { "watermark",    MenuDispatch::Registry },
        { "measure-dist", MenuDispatch::Disabled },  // ribbon: "measure"/"distance"
        { "measure-area", MenuDispatch::Disabled },  // ribbon: "area"

        // ── Comments ──
        { "highlight",       MenuDispatch::Registry },
        { "underline",       MenuDispatch::Registry },
        { "strike",          MenuDispatch::Registry },  // alias → Strikeout
        { "squiggly",        MenuDispatch::Registry },
        { "note",            MenuDispatch::Registry },
        { "textbox",         MenuDispatch::Registry },
        { "callout",         MenuDispatch::Registry },
        { "custom-stamp",    MenuDispatch::Disabled },  // ribbon: "customStamp"
        { "pencil",          MenuDispatch::Registry },
        { "line",            MenuDispatch::Registry },
        { "arrow",           MenuDispatch::Registry },
        { "rect",            MenuDispatch::Registry },
        { "oval",            MenuDispatch::Registry },
        { "polyline",        MenuDispatch::Disabled },  // ribbon: "poly"
        { "toggle-comments", MenuDispatch::Disabled },  // ribbon: "comments"

        // ── Forms ──
        { "text-field",      MenuDispatch::Registry },
        { "checkbox",        MenuDispatch::Registry },
        { "radio",           MenuDispatch::Registry },
        { "dropdown",        MenuDispatch::Registry },
        { "date-field",      MenuDispatch::Registry },  // alias → DateField
        { "num-field",       MenuDispatch::Registry },  // alias → NumField
        { "calc-field",      MenuDispatch::Registry },  // alias → CalcField (FormsController)
        { "signature-field", MenuDispatch::Registry },  // alias → SigField
        { "button",          MenuDispatch::Registry },
        { "autodetect",      MenuDispatch::Registry },
        { "tab-order",       MenuDispatch::Registry },
        { "import-data",     MenuDispatch::Registry },
        { "export-data",     MenuDispatch::Registry },
        { "expiry-date",     MenuDispatch::Registry },  // alias → ExpiryDate (SecurityController)

        // ── Window ──
        { "minimize",     MenuDispatch::Local    },
        { "maximize",     MenuDispatch::Local    },
        { "tile",         MenuDispatch::Disabled },  // single-window app, no tiling

        // ── Help ──
        { "shortcuts",    MenuDispatch::Local    },
        { "guide",        MenuDispatch::Local    },
        { "updates",      MenuDispatch::Disabled },  // manual check via Preferences ▸ Updates
        { "about",        MenuDispatch::Local    },
    };
    return specs;
}

const QList<QString>& MenuBar::localHandlerIds() {
    // toolIds handled inline in the addActionToMenu lambda below (not via the
    // ToolRegistry). Keep in lock-step with the lambda's if/else chain.
    static const QList<QString> ids = {
        "find", "find-replace", "exit", "close", "save-copy",
        "minimize", "maximize", "fullscreen", "darkMode",
        "ocr", "redact", "compare", "guide", "about",
        "shortcuts", "preferences",
    };
    return ids;
}

namespace {
// dispatch class for a given toolId, built once from actionSpecs().
MenuDispatch dispatchFor(const QString& toolId) {
    static const QHash<QString, MenuDispatch> map = []() {
        QHash<QString, MenuDispatch> m;
        for (const auto& s : MenuBar::actionSpecs())
            m.insert(QString::fromLatin1(s.toolId), s.dispatch);
        return m;
    }();
    return map.value(toolId, MenuDispatch::Registry);
}
} // namespace

MenuBar::MenuBar(QWidget* parent) : QMenuBar(parent) {
    auto* mainWindow = qobject_cast<MainWindow*>(parent);
    if (!mainWindow) return;

    auto addActionToMenu = [mainWindow](QMenu* menu, const QString& label, const QString& toolId,
                                        const QKeySequence& shortcut = QKeySequence(),
                                        bool checkable = false, bool checked = false) {
        auto* action = menu->addAction(label);
        if (!shortcut.isEmpty()) {
            action->setShortcut(shortcut);
        }
        if (checkable) {
            action->setCheckable(true);
            action->setChecked(checked);
        }

        // Items with no shipped backing are rendered disabled rather than left
        // as silent no-ops — mirrors the ribbon's plannedTools() treatment.
        if (dispatchFor(toolId) == MenuDispatch::Disabled) {
            action->setEnabled(false);
            action->setToolTip(tr("Planned for a future release."));
            action->setStatusTip(tr("Planned for a future release."));
            return action;
        }

        connect(action, &QAction::triggered, mainWindow, [mainWindow, toolId, action]() {
            if (toolId == "find" || toolId == "find-replace") {
                // The FindBar contains both search and replace controls, so
                // "Find & Replace" opens the same bar as "Find".
                mainWindow->toggleFindBar();
            } else if (toolId == "exit") {
                qApp->quit();
            } else if (toolId == "close") {
                mainWindow->close();
            } else if (toolId == "save-copy") {
                // "Save a Copy" reuses the Save As flow (writes to a new path
                // without rebinding the open document).
                mainWindow->onToolActivated("saveAs");
            } else if (toolId == "minimize") {
                mainWindow->showMinimized();
            } else if (toolId == "maximize") {
                if (mainWindow->isMaximized()) {
                    mainWindow->showNormal();
                } else {
                    mainWindow->showMaximized();
                }
            } else if (toolId == "fullscreen") {
                mainWindow->setFullScreenMode(!mainWindow->isFullScreen());
            } else if (toolId == "darkMode") {
                mainWindow->toggleTheme();
            } else if (toolId == "ocr") {
                mainWindow->onScreenSelected("ocr");
            } else if (toolId == "redact") {
                mainWindow->onScreenSelected("redact");
            } else if (toolId == "compare") {
                mainWindow->onScreenSelected("compare");
            } else if (toolId == "guide") {
                QMessageBox::information(mainWindow, tr("User Guide"),
                    tr("Glyph PDF Editor User Guide is available online at https://glyph.app/guide"));
            } else if (toolId == "about") {
                // AR-8 D2: version is single-sourced from CMake PROJECT_VERSION via
                // QCoreApplication::applicationVersion() (set in main.cpp).
                // Never hardcode the version string here.
                QMessageBox::about(mainWindow, tr("About GlyphPDF"),
                    tr("<h3>GlyphPDF</h3>"
                       "<p>Version %1</p>"
                       "<p>A privacy-first PDF workstation for Windows. "
                       "No telemetry, no cloud, no subscription.</p>"
                       "<p>Built with C++17, Qt 6.11, PoDoFo 1.1, and PDFium.</p>"
                       "<p>&copy; 2026 Glyph. Licensed under the "
                       "<a href=\"https://www.apache.org/licenses/LICENSE-2.0\">Apache License 2.0</a>.</p>"
                       "<p>This product bundles open-source components under the "
                       "MIT, Apache-2.0, BSD, and LGPL licenses. Full notices are in "
                       "<b>LICENSE-3RD-PARTY.md</b>, installed alongside the application.</p>")
                    .arg(UpdateChecker::currentVersion()));
            } else if (toolId == "shortcuts") {
                ShortcutHelpDialog dlg(mainWindow);
                dlg.exec();
            } else if (toolId == "preferences") {
                PreferencesDialog dlg(mainWindow);
                dlg.exec();
            } else {
                mainWindow->onToolActivated(toolId);
            }
        });
        return action;
    };

    // ==========================================
    // 1. FILE MENU
    // ==========================================
    auto* fileMenu = addMenu(tr("&File"));
    addActionToMenu(fileMenu, tr("&New Window"), "new-window");
    addActionToMenu(fileMenu, tr("&Open…"), "open", QKeySequence::Open);

    m_recentMenu = fileMenu->addMenu(tr("Open &Recent"));
    refreshRecentFiles();

    addActionToMenu(fileMenu, tr("&Close"), "close", QKeySequence::Close);
    fileMenu->addSeparator();
    addActionToMenu(fileMenu, tr("&Save"), "save", QKeySequence::Save);
    addActionToMenu(fileMenu, tr("Save &As…"), "saveAs", QKeySequence::SaveAs);
    addActionToMenu(fileMenu, tr("Save a &Copy…"), "save-copy");
    fileMenu->addSeparator();
    addActionToMenu(fileMenu, tr("Print Pre&view…"), "printPreview");
    addActionToMenu(fileMenu, tr("Page Set&up…"), "pageSetup");
    addActionToMenu(fileMenu, tr("&Print…"), "print", QKeySequence::Print);
    fileMenu->addSeparator();
    addActionToMenu(fileMenu, tr("E&xport Presets…"), "exportPresets");
    addActionToMenu(fileMenu, tr("&Share…"), "share");
    addActionToMenu(fileMenu, tr("Document &Properties…"), "properties");
    fileMenu->addSeparator();
    addActionToMenu(fileMenu, tr("E&xit"), "exit");

    // ==========================================
    // 2. EDIT MENU
    // ==========================================
    auto* editMenu = addMenu(tr("&Edit"));
    addActionToMenu(editMenu, tr("&Undo"), "undo", QKeySequence::Undo);
    addActionToMenu(editMenu, tr("&Redo"), "redo", QKeySequence::Redo);
    editMenu->addSeparator();
    addActionToMenu(editMenu, tr("Cu&t"), "cut", QKeySequence::Cut);
    addActionToMenu(editMenu, tr("&Copy"), "copy", QKeySequence::Copy);
    addActionToMenu(editMenu, tr("&Paste"), "paste", QKeySequence::Paste);
    addActionToMenu(editMenu, tr("&Delete"), "delete", QKeySequence::Delete);
    addActionToMenu(editMenu, tr("Select &All"), "select-all", QKeySequence::SelectAll);
    editMenu->addSeparator();
    addActionToMenu(editMenu, tr("&Find…"), "find", QKeySequence::Find);
    addActionToMenu(editMenu, tr("Find & Replace…"), "find-replace", QKeySequence(Qt::CTRL | Qt::Key_H));
    editMenu->addSeparator();
    addActionToMenu(editMenu, tr("&Preferences…"), "preferences");

    // ==========================================
    // 3. VIEW MENU
    // ==========================================
    auto* viewMenu = addMenu(tr("&View"));
    addActionToMenu(viewMenu, tr("Zoom &In"), "zoomIn", QKeySequence::ZoomIn);
    addActionToMenu(viewMenu, tr("Zoom &Out"), "zoomOut", QKeySequence::ZoomOut);
    addActionToMenu(viewMenu, tr("&Actual Size"), "actual", QKeySequence(Qt::CTRL | Qt::Key_0));
    addActionToMenu(viewMenu, tr("Fit &Page"), "fitPage");
    addActionToMenu(viewMenu, tr("Fit &Width"), "fitWidth");
    viewMenu->addSeparator();
    addActionToMenu(viewMenu, tr("&Single Page View"), "single");
    addActionToMenu(viewMenu, tr("&Continuous Scrolling"), "continuous");
    addActionToMenu(viewMenu, tr("&Two-Page View"), "two-page");
    addActionToMenu(viewMenu, tr("&Presentation Mode"), "presentation");
    viewMenu->addSeparator();
    addActionToMenu(viewMenu, tr("&Full Screen"), "fullscreen", QKeySequence(Qt::Key_F11));
    viewMenu->addSeparator();
    addActionToMenu(viewMenu, tr("&Dark Mode"), "darkMode", QKeySequence(), true, true);
    addActionToMenu(viewMenu, tr("&Rulers"), "rulers", QKeySequence(), true, false);
    addActionToMenu(viewMenu, tr("&Guides"), "guides", QKeySequence(), true, false);
    addActionToMenu(viewMenu, tr("G&rid"), "grid", QKeySequence(), true, false);

    // ==========================================
    // 4. DOCUMENT MENU
    // ==========================================
    auto* docMenu = addMenu(tr("&Document"));
    addActionToMenu(docMenu, tr("&Insert Pages…"), "insert-page");
    addActionToMenu(docMenu, tr("&Delete Pages…"), "delete-page");
    addActionToMenu(docMenu, tr("&Extract Pages…"), "extract-page");
    docMenu->addSeparator();
    addActionToMenu(docMenu, tr("Rotate &Clockwise"), "rotate-cw");
    addActionToMenu(docMenu, tr("Rotate &Counter-Clockwise"), "rotate-ccw");
    docMenu->addSeparator();
    addActionToMenu(docMenu, tr("&Crop Pages…"), "crop");
    addActionToMenu(docMenu, tr("&Resize Pages…"), "resize");
    docMenu->addSeparator();
    addActionToMenu(docMenu, tr("Page &Numbers…"), "page-numbers");
    addActionToMenu(docMenu, tr("&Headers & Footers…"), "headers-footers");

    // ==========================================
    // 5. TOOLS MENU
    // ==========================================
    auto* toolsMenu = addMenu(tr("&Tools"));
    addActionToMenu(toolsMenu, tr("&OCR Document"), "ocr", QKeySequence(), true, false);
    addActionToMenu(toolsMenu, tr("&Redaction Mode"), "redact", QKeySequence(), true, false);
    addActionToMenu(toolsMenu, tr("&Compare Documents…"), "compare");
    addActionToMenu(toolsMenu, tr("C&ompress Document…"), "compress");
    addActionToMenu(toolsMenu, tr("&Watermark…"), "watermark");
    addActionToMenu(toolsMenu, tr("Set E&xpiry Date…"), "expiry-date");
    toolsMenu->addSeparator();
    addActionToMenu(toolsMenu, tr("Measure &Distance"), "measure-dist");
    addActionToMenu(toolsMenu, tr("Measure &Area"), "measure-area");

    // ==========================================
    // 6. COMMENTS MENU
    // ==========================================
    auto* commentsMenu = addMenu(tr("&Comments"));
    addActionToMenu(commentsMenu, tr("&Highlight"), "highlight");
    addActionToMenu(commentsMenu, tr("&Underline"), "underline");
    addActionToMenu(commentsMenu, tr("&Strikeout"), "strike");
    addActionToMenu(commentsMenu, tr("&Squiggly"), "squiggly");
    commentsMenu->addSeparator();
    addActionToMenu(commentsMenu, tr("Sticky &Note"), "note");
    addActionToMenu(commentsMenu, tr("&Text Box"), "textbox");
    addActionToMenu(commentsMenu, tr("&Callout"), "callout");
    commentsMenu->addSeparator();

    auto* stampsMenu = commentsMenu->addMenu(tr("&Stamps"));
    stampsMenu->addAction(tr("Approved"));
    stampsMenu->addAction(tr("Draft"));
    stampsMenu->addAction(tr("Confidential"));
    stampsMenu->addSeparator();
    addActionToMenu(stampsMenu, tr("Custom &Stamp…"), "custom-stamp");

    auto* drawingMenu = commentsMenu->addMenu(tr("&Drawing Tools"));
    addActionToMenu(drawingMenu, tr("&Pencil"), "pencil");
    addActionToMenu(drawingMenu, tr("&Line"), "line");
    addActionToMenu(drawingMenu, tr("&Arrow"), "arrow");
    addActionToMenu(drawingMenu, tr("&Rectangle"), "rect");
    addActionToMenu(drawingMenu, tr("&Oval"), "oval");
    addActionToMenu(drawingMenu, tr("&Polyline"), "polyline");

    commentsMenu->addSeparator();
    addActionToMenu(commentsMenu, tr("Show/Hide &Comments Panel"), "toggle-comments");

    // ==========================================
    // 7. FORMS MENU
    // ==========================================
    auto* formsMenu = addMenu(tr("F&orms"));
    addActionToMenu(formsMenu, tr("Add &Text Field"), "text-field");
    addActionToMenu(formsMenu, tr("Add &Checkbox"), "checkbox");
    addActionToMenu(formsMenu, tr("Add &Radio Button"), "radio");
    addActionToMenu(formsMenu, tr("Add &Dropdown"), "dropdown");
    formsMenu->addSeparator();
    addActionToMenu(formsMenu, tr("Add &Date Field"), "date-field");
    addActionToMenu(formsMenu, tr("Add &Number Field"), "num-field");
    addActionToMenu(formsMenu, tr("Add &Calculated Field"), "calc-field");
    formsMenu->addSeparator();
    addActionToMenu(formsMenu, tr("Add &Signature Field"), "signature-field");
    addActionToMenu(formsMenu, tr("Add &Button"), "button");
    formsMenu->addSeparator();
    addActionToMenu(formsMenu, tr("&Auto-Detect Fields"), "autodetect");
    addActionToMenu(formsMenu, tr("&Tab Order"), "tab-order");
    addActionToMenu(formsMenu, tr("&Import Data…"), "import-data");
    addActionToMenu(formsMenu, tr("&Export Data…"), "export-data");

    // ==========================================
    // 8. WINDOW MENU
    // ==========================================
    auto* windowMenu = addMenu(tr("&Window"));
    addActionToMenu(windowMenu, tr("&Minimize"), "minimize");
    addActionToMenu(windowMenu, tr("&Maximize"), "maximize");
    addActionToMenu(windowMenu, tr("&Tile Windows"), "tile");
    windowMenu->addSeparator();

    // Dynamic Active Document display
    auto* viewer = mainWindow->pdfViewer();
    QString docName = (viewer && !viewer->filePath().isEmpty())
                      ? QFileInfo(viewer->filePath()).fileName()
                      : tr("[No Active Document]");
    auto* activeDocAct = windowMenu->addAction(docName);
    activeDocAct->setEnabled(false);
    activeDocAct->setCheckable(true);
    activeDocAct->setChecked(viewer && !viewer->filePath().isEmpty());

    // ==========================================
    // 9. HELP MENU
    // ==========================================
    auto* helpMenu = addMenu(tr("&Help"));
    addActionToMenu(helpMenu, tr("&Keyboard Shortcuts"), "shortcuts", QKeySequence(Qt::CTRL | Qt::Key_Slash));
    addActionToMenu(helpMenu, tr("&Documentation"), "guide");
    addActionToMenu(helpMenu, tr("Check for &Updates…"), "updates");
    helpMenu->addSeparator();
    addActionToMenu(helpMenu, tr("&About Glyph PDF"), "about");
}

void MenuBar::refreshRecentFiles() {
    if (!m_recentMenu) return;
    m_recentMenu->clear();

    auto* mainWindow = qobject_cast<MainWindow*>(parentWidget());
    QSettings settings;
    QStringList recent = settings.value("recentFiles").toStringList();

    if (recent.isEmpty()) {
        auto* act = m_recentMenu->addAction(tr("No Recent Documents"));
        act->setEnabled(false);
        return;
    }

    for (const QString& path : recent) {
        QFileInfo fi(path);
        QString label = fi.fileName();
        if (label.isEmpty()) label = path;

        auto* act = m_recentMenu->addAction(label);
        act->setToolTip(path);
        act->setStatusTip(path);
        if (!fi.exists())
            act->setEnabled(false);

        connect(act, &QAction::triggered, mainWindow, [mainWindow, path]() {
            if (mainWindow) mainWindow->openDocument(path);
        });
    }

    m_recentMenu->addSeparator();

    // Count missing entries to decide whether "Prune Missing" should be enabled
    int missingCount = 0;
    for (const QString& p : recent) {
        if (!QFileInfo::exists(p)) ++missingCount;
    }
    auto* pruneAct = m_recentMenu->addAction(tr("Prune Missing Files"));
    pruneAct->setEnabled(missingCount > 0);
    pruneAct->setToolTip(tr("Remove %n missing file(s) from the recent list", nullptr, missingCount));
    connect(pruneAct, &QAction::triggered, mainWindow, [mainWindow]() {
        if (mainWindow) mainWindow->pruneMissingRecents();
    });

    auto* clearAct = m_recentMenu->addAction(tr("Clear Recent Files"));
    connect(clearAct, &QAction::triggered, this, [this]() {
        QSettings s;
        s.remove("recentFiles");
        refreshRecentFiles();
    });
}

} // namespace gp
