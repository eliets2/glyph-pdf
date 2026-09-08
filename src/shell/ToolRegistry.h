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

    /// ARC07: re-query every lazily created QAction's enabled state from its
    /// controller (called when the session's read-only state changes, so
    /// action enablement always mirrors the shared EditPolicy gate).
    void refreshEnabledActions();

signals:
    /// Emitted after a tool has been activated.
    void toolActivated(ToolId id);
    /// ARC07: a tool activation was refused by the shared editability gate
    /// (the owning controller's isEnabled() said no — read-only mutation).
    /// The host surfaces EditPolicy::readOnlyMessage().
    void toolRefused(ToolId id);

private:
    QHash<ToolId, IToolController*> m_controllers;
    QHash<ToolId, QAction*>         m_actions;
};

} // namespace gp
