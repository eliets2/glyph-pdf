// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QWidget>
#include <QHash>

class QTabBar;
class QStackedWidget;
class QToolButton;
class QLabel;
class QAction;

namespace gp {
class ToolRegistry;

// Ribbon = QTabBar on top + a swap-on-tab QStackedWidget body of tool groups.
// U02: the ribbon is collapsible (Microsoft Fluent contract — tabs stay
// visible, commands hide) via setCollapsed()/the chevron/double-click/Ctrl+F1.
// Collapse is plain hide/show of the existing body stack: no re-parenting, no
// QDockWidget/QToolBar tricks, no animation. While collapsed the active task
// stays visible in a label next to the tabs.
class Ribbon : public QWidget {
    Q_OBJECT
public:
    explicit Ribbon(QWidget* parent = nullptr);

    void setActiveTool(const QString& toolId);
    QString activeTool() const { return _activeTool; }

    // ── U02 collapse ──
    bool isCollapsed() const { return _collapsed; }
    void setCollapsed(bool collapsed);
    /// Navigation channel (TaskStateSync): raise a tab by RibbonModel name.
    /// Never changes the expansion state; unknown names are ignored.
    void raiseTab(const QString& tabName);

    // ── U02 state seams (tests / TaskStateSync) ──
    QString activeTabName() const;
    int bodyHeight() const;             // 0 while collapsed
    QString collapsedLabel() const;

    // ── R15: canonical command binding (UI02) ──
    // Give the ribbon the ONE enablement authority. Every non-planned button
    // resolves its ToolId through the registry's canonical QAction and mirrors
    // `enabledChanged`, so ribbon enablement is the same predicate that gates
    // dispatch (EditPolicy via the owning controller). Planned entries stay
    // disabled with their reason + alternative disclosure. Called once after
    // the controllers are registered; buttons built later bind at build time.
    void setToolRegistry(ToolRegistry* registry);

signals:
    void toolActivated(const QString& toolId);
    void tabChanged(const QString& tabName);
    void collapsedChanged(bool collapsed);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* buildBody(int tabIdx);
    QToolButton* makeTool(const QString& id, const QString& label,
                          const QString& icon, bool big);
    // R15: if the id is planned, leave the button visible but disabled and
    // attach its reason + alternative disclosure (tooltip, status tip and
    // accessible description — never tooltip-only). Returns true when planned.
    bool applyPlannedState(QToolButton* btn, const QString& id);
    // R15: mirror the canonical registry action's enablement onto the button.
    void bindButtonToRegistry(QToolButton* btn, const QString& id);
    int tabIndexFor(const QString& tabName) const;

    QTabBar*        _tabs       = nullptr;
    QStackedWidget* _bodyStack  = nullptr;
    QToolButton*    _collapseBtn = nullptr;
    QLabel*         _activeTaskLabel = nullptr;
    QString         _activeTool = QStringLiteral("highlight");
    bool            _collapsed  = false;
    bool            _raiseInProgress = false;   // raiseTab() → currentChanged guard
    // tool-id -> QToolButton* (cached across all built tabs) for active-state updates
    QHash<QString, QToolButton*> _buttons;
    // R15: the canonical enablement authority (null until setToolRegistry).
    ToolRegistry*   _registry   = nullptr;
};

} // namespace gp
