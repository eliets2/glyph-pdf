// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QMenuBar>
#include <QString>
#include <QList>

class QMenu;

namespace gp {

/// How a menu action's toolId is dispatched. Mirrors the ribbon's
/// planned-vs-wired gating so TestMenuBarIntegrity can verify that no menu item
/// silently no-ops.
enum class MenuDispatch {
    Registry,  ///< Routed to MainWindow::onToolActivated → ToolRegistry. Must
               ///< resolve to a ToolId AND have a controller handler.
    Local,     ///< Handled directly inside MenuBar (e.g. find, about). Must be
               ///< listed in MenuBar::localHandlerIds().
    Disabled,  ///< Intentionally disabled ("planned" / unshipped). Rendered
               ///< greyed-out with a tooltip; never fires.
};

/// One menu action the MenuBar constructs. The constructor builds the real
/// QActions from this table, and TestMenuBarIntegrity enumerates the same table
/// to guarantee every menu item either resolves to a wired handler or is an
/// intentional Local/Disabled entry. Keep the two in lock-step by editing only
/// MenuBar::actionSpecs().
struct MenuActionSpec {
    const char*  toolId;
    MenuDispatch dispatch;
};

class MenuBar : public QMenuBar {
    Q_OBJECT
public:
    explicit MenuBar(QWidget* parent = nullptr);

    /** Rebuild the recent files submenu from QSettings. */
    void refreshRecentFiles();

    /// Every actionable menu item, with its dispatch classification. Single
    /// source of truth shared by the constructor and the integrity test.
    static const QList<MenuActionSpec>& actionSpecs();

    /// toolIds that MenuBar handles inline (not via the ToolRegistry). These are
    /// valid by construction; the test checks every MenuDispatch::Local item
    /// appears here so a stale Local classification can't hide a dead item.
    static const QList<QString>& localHandlerIds();

private:
    QMenu* m_recentMenu = nullptr;
};
} // namespace gp
