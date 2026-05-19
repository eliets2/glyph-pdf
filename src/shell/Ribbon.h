#pragma once
#include <QWidget>
#include <QHash>

class QTabBar;
class QStackedWidget;
class QToolButton;

namespace gp {

// Ribbon = QTabBar on top + a swap-on-tab QStackedWidget body of tool groups.
class Ribbon : public QWidget {
    Q_OBJECT
public:
    explicit Ribbon(QWidget* parent = nullptr);

    void setActiveTool(const QString& toolId);
    QString activeTool() const { return _activeTool; }

signals:
    void toolActivated(const QString& toolId);
    void tabChanged(const QString& tabName);

private:
    QWidget* buildBody(int tabIdx);
    QToolButton* makeTool(const QString& id, const QString& label,
                          const QString& icon, bool big);

    QTabBar*        _tabs       = nullptr;
    QStackedWidget* _bodyStack  = nullptr;
    QString         _activeTool = QStringLiteral("highlight");
    // tool-id -> QToolButton* (cached across all built tabs) for active-state updates
    QHash<QString, QToolButton*> _buttons;
};

} // namespace gp
