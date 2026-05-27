#pragma once
#include <QMainWindow>
#include "core/AppContext.h"

class PdfViewerWidget;
class FindBar;
class QFrame;

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
    explicit MainWindow(const AppContext* ctx, QWidget* parent = nullptr);
    ~MainWindow() override;

    PdfViewerWidget* pdfViewer() const;
    StatusBar* statusBar() const { return _status; }

    void openDocument(const QString& filePath);
    void recoverDocument(const QString& originalPath);
    const AppContext* appContext() const { return _ctx; }
    void toggleFindBar();
    void setFullScreenMode(bool fullscreen);
    void updateTitle();
    MenuBar* menuBarWidget() const { return _menu; }

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

private:
    const AppContext* _ctx  = nullptr;

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

