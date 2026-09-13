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

    /// R15 (PP06 / UI01): a planned ribbon entry with its HONEST disclosure.
    /// Planned tools ship visibly DISABLED — never hidden — and every one
    /// carries a user-facing reason why it is unavailable and a supported
    /// alternative route that exists in the app today (the Capability
    /// whyNot+alternative pattern from U08, applied to the ribbon model).
    struct PlannedToolSpec {
        QString id;
        QString reason;       // why the control is disabled (one or two sentences)
        QString alternative;  // a supported route available in this release
    };

    /// The planned-entry table (id → reason + alternative). Remove an id here
    /// in the same commit that wires its real route.
    static const QVector<PlannedToolSpec>& plannedToolSpecs();

    /// Convenience membership set derived from plannedToolSpecs().
    static const QSet<QString>& plannedTools();

    /// The disclosure for one planned id, or nullptr when the id is wired.
    static const PlannedToolSpec* plannedSpecFor(const QString& id);
};

} // namespace gp
