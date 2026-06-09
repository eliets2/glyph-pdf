// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QVector>
#include <QSet>

namespace gp {

struct Tool {
    QString id;
    QString label;
    QString icon;
    bool    big = false;
};

struct ToolGroup {
    QString title;
    QVector<Tool> tools;
};

struct RibbonTabDef {
    QString name;
    QVector<ToolGroup> groups;
};

// Single source of truth for the ribbon. Edit here, the UI rebuilds.
class RibbonModel {
public:
    static const QVector<RibbonTabDef>& tabs();

    /// Returns the set of tool string IDs that are planned for a future release.
    /// Ribbon renders these as setEnabled(false) + tooltip "Planned for a future release".
    /// These tools have no controller handler and must not reach ToolRegistry::activate.
    static const QSet<QString>& plannedTools();
};

} // namespace gp
