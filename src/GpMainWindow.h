// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QMainWindow>
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

class HomeController;
class ViewController;
class EditController;
class PagesController;
class ConvertController;
class FormsController;
class SecurityController;
class ToolRegistry;

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
    Sidebar*        _left        = nullptr;
    Sidebar*        _right       = nullptr;
    ModeController* _modes       = nullptr;
    QStackedWidget* _rootStack   = nullptr;   // [0]=welcome, [1]=workspace
    WelcomeWidget*  _welcome     = nullptr;
    AIChatPanel*    _ai          = nullptr;
    SignaturesPanel* _sigPanel   = nullptr;
    PdfAValidationPanel* _pdfaPanel = nullptr;
    UpdateChecker*  _updater     = nullptr;
    QFrame*         _updateBar   = nullptr;
    bool            _aiVisible   = false;
    bool            _isDark      = true;

    void applyTheme();
    void replaceRight(QWidget* w);
    void initUpdateChecker();
};

} // namespace gp

