#pragma once
#include <QString>
#include <QVector>

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
};

} // namespace gp
