// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/ToolId.h"
#include <QObject>
#include <QHash>

class QAction;
class IToolController;

namespace gp {

/// Central registry that maps every ToolId to exactly one IToolController
/// and (optionally) a QAction.  Provides both typed and string-based
/// dispatch so the Ribbon can keep emitting toolActivated(QString) during
/// the transition period.
class ToolRegistry : public QObject {
    Q_OBJECT
public:
    explicit ToolRegistry(QObject* parent = nullptr);

    /// Register all tools declared by `ctrl->handledTools()`.
    /// Logs a warning if a ToolId is already claimed by another controller.
    void registerController(IToolController* ctrl);

    /// Typed dispatch — preferred path.
    void activate(ToolId id);

    /// String-based dispatch — alias-aware adapter for Ribbon signals.
    void activateFromString(const QString& str);

    /// Retrieve the QAction created for a ToolId (lazy-created on first request).
    QAction* actionFor(ToolId id);

    /// Look up which controller owns a ToolId (nullptr if unregistered).
    IToolController* controllerFor(ToolId id) const;

signals:
    /// Emitted after a tool has been activated.
    void toolActivated(ToolId id);

private:
    QHash<ToolId, IToolController*> m_controllers;
    QHash<ToolId, QAction*>         m_actions;
};

} // namespace gp
