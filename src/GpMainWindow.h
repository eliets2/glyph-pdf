// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QMainWindow>
#include <functional>
#include "core/AppContext.h"

class PdfViewerWidget;
class FindBar;
class QFrame;
class WelcomeWidget;
class QStackedWidget;

namespace gp {

class UpdateChecker;
class MenuBar;
class Ribbon;
class ModeStrip;
class ScreenNav;
class StatusBar;
class Sidebar;
class ModeController;
class AIChatPanel;
class SignaturesPanel;
class PdfAValidationPanel;
class MeasureMode;

class HomeController;
class ViewController;
class EditController;
class PagesController;
class ConvertController;
class FormsController;
class SecurityController;
class ToolRegistry;
class TaskStateSync;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // AR-10 D2: the window OWNS its AppContext by value (it is a cheap aggregate
    // of shared_ptrs) rather than holding a raw pointer to a caller-owned
    // (often stack-local) context. This removes the stack-lifetime risk the
    // audit flagged at main.cpp without changing the const AppContext* seen by
    // controllers (they read _ctx, which points at the owned copy).
    explicit MainWindow(AppContext ctx, QWidget* parent = nullptr);
    ~MainWindow() override;

    PdfViewerWidget* pdfViewer() const;
    StatusBar* statusBar() const { return _status; }

    void openDocument(const QString& filePath);
    void recoverDocument(const QString& originalPath);
    const AppContext* appContext() const { return _ctx; }

    // === §9.16 P1: unified open/drag-drop routing ==========================
    // The audit: "Office/image import hidden behind separate menus instead of
    // unified File>Open with drag-and-drop." File>Open, the Welcome Open card,
    // recent files and drag-and-drop all funnel through openDocument()/the
    // drop handlers; non-PDF targets are classified here and routed to the
    // SAME conversion paths the explicit Welcome cards run (LibreOffice
    // office import, images-to-PDF). The cards REMAIN as explicit entry
    // points with their own pick/save dialogs — nothing is hidden or removed.
    //
    // Pure (extension-based) so the policy is headlessly testable — pinned by
    // tests/TestOpenRouting.cpp; dragEnterEvent/dropEvent/openDocument are
    // thin wrappers over these statics.
    enum class OpenRoute {
        PdfDirect,       // load in the viewer as-is
        OfficeConvert,   // LibreOffice conversion → open resulting PDF
        ImagesConvert,   // images-to-PDF conversion → open resulting PDF
        Unsupported      // not routable — pre-existing PDF-load-error path
    };
    static OpenRoute routeForFile(const QString& path);

    // Pure drop policy: what a drop of these local paths should do.
    //   1. the FIRST PDF wins and opens directly (pre-existing behavior),
    //   2. otherwise ALL images in the drop combine into ONE PDF (mirrors
    //      multi-select in the Images-to-PDF card),
    //   3. otherwise the FIRST Office file is converted,
    //   4. otherwise nothing is planned (drop not accepted, as before).
    struct DropPlan {
        QString     pdfToOpen;
        QStringList imagesToConvert;   // 1+ images → one combined PDF
        QString     officeToConvert;
        bool isEmpty() const {
            return pdfToOpen.isEmpty() && imagesToConvert.isEmpty()
                && officeToConvert.isEmpty();
        }
    };
    static DropPlan planDrop(const QStringList& localPaths);
    // Navigate to a named screen (delegates to onScreenSelected).
    // Usable by controllers that hold a MainWindow* but not ModeController*.
    void activateScreen(const QString& id);
    void toggleFindBar();
    void setFullScreenMode(bool fullscreen);
    void updateTitle();
    MenuBar* menuBarWidget() const { return _menu; }
    void pruneMissingRecents();
    // Welcome/start screen <-> workspace switching. Shown on launch and when no
    // document is open; the workspace replaces it once a document loads.
    void showWelcome();
    void showWorkspace();

public slots:
    void onScreenSelected(const QString& id);
    void toggleTheme();
    void onToolActivated(const QString& id);

private slots:
    void onTabChanged(const QString& tab);
    void onModeChanged(const QString& m);
    void toggleAi();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    // AR-7 D4: prompt Save / Discard / Cancel when quitting with unsaved changes.
    void closeEvent(QCloseEvent* event) override;

private:
    AppContext        _ownedCtx;           // owned; outlives all controllers
    const AppContext* _ctx  = nullptr;     // points at _ownedCtx

    HomeController*     _home = nullptr;
    ViewController*     _view = nullptr;
    EditController*     _edit = nullptr;
    PagesController*    _pages = nullptr;
    ConvertController*  _convert = nullptr;
    FormsController*    _forms = nullptr;
    SecurityController* _security = nullptr;
    ToolRegistry*       _toolRegistry = nullptr;

    MenuBar*        _menu        = nullptr;
    Ribbon*         _ribbon      = nullptr;
    ModeStrip*      _modeStrip   = nullptr;
    FindBar*        _findBar     = nullptr;
    ScreenNav*      _screenNav   = nullptr;
    StatusBar*      _status      = nullptr;
    TaskStateSync*  _taskSync    = nullptr;   // U02: the single visible-state writer
    Sidebar*        _left        = nullptr;
    Sidebar*        _right       = nullptr;
    ModeController* _modes       = nullptr;
    QStackedWidget* _rootStack   = nullptr;   // [0]=welcome, [1]=workspace
    WelcomeWidget*  _welcome     = nullptr;
    AIChatPanel*    _ai          = nullptr;
    SignaturesPanel* _sigPanel   = nullptr;
    PdfAValidationPanel* _pdfaPanel = nullptr;
    MeasureMode* _measurePanel = nullptr;
    UpdateChecker*  _updater     = nullptr;
    QFrame*         _updateBar   = nullptr;
    bool            _aiVisible   = false;
    bool            _isDark      = true;

    void applyTheme();
    void replaceRight(QWidget* w);
    // ARC06: give the PDF/A panel the ACTIVE document (viewer identity) and
    // refresh it on successful document changes while the panel is the
    // active right panel. Empty path = the honest "No document loaded." state.
    void refreshPdfAPanel();
    void initUpdateChecker();
    // §9.16 P1: unified-flow conversions (same engines/progress/failure
    // handling as the Welcome cards in HomeController, minus their pick/save
    // dialogs — the unified flow's output goes to a tracked temp dir so a
    // File>Open never silently overwrites a user file).
    void convertAndOpenOffice(const QString& officePath);
    void convertAndOpenImages(const QStringList& imagePaths);
    void runConversion(const QString& progressLabel,
                       const std::function<bool()>& work,
                       const QString& outputPath,
                       const QString& successMessage,
                       const QString& failureMessage);
};

} // namespace gp

