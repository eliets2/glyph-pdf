// SPDX-License-Identifier: Apache-2.0
#include "ToolRegistry.h"
#include "core/interfaces/IToolController.h"

#include <QAction>
#include <QDebug>

namespace gp {

ToolRegistry::ToolRegistry(QObject* parent) : QObject(parent) {}

void ToolRegistry::registerController(IToolController* ctrl) {
    if (!ctrl) return;
    const auto tools = ctrl->handledTools();
    for (ToolId id : tools) {
        if (m_controllers.contains(id)) {
            qWarning() << "ToolRegistry: ToolId" << toolIdToString(id)
                        << "already registered — skipping duplicate.";
            continue;
        }
        m_controllers.insert(id, ctrl);
    }
}

void ToolRegistry::activate(ToolId id) {
    IToolController* ctrl = m_controllers.value(id, nullptr);
    if (!ctrl) {
        qWarning() << "ToolRegistry::activate — no controller for"
                    << toolIdToString(id);
        return;
    }
    // ARC07 (TEAM-ARCHITECTURE-REVIEW-2026-09-07): the shared mutation gate.
    // isEnabled() is the EditPolicy predicate every mutating controller
    // implements, so tool DISPATCH and action ENABLEMENT are one decision —
    // a read-only session refuses mutating tools here, before any controller
    // pushes a command or calls the engine.
    if (!ctrl->isEnabled(id)) {
        emit toolRefused(id);
        return;
    }
    ctrl->activate(id);
    emit toolActivated(id);
}

void ToolRegistry::refreshEnabledActions() {
    for (auto it = m_actions.begin(); it != m_actions.end(); ++it) {
        IToolController* ctrl = m_controllers.value(it.key(), nullptr);
        if (ctrl)
            it.value()->setEnabled(ctrl->isEnabled(it.key()));
    }
}

void ToolRegistry::activateFromString(const QString& str) {
    auto opt = toolIdFromString(str);
    if (opt.has_value()) {
        activate(opt.value());
    } else {
        qWarning() << "ToolRegistry::activateFromString — unknown tool string:"
                    << str;
    }
}

QAction* ToolRegistry::actionFor(ToolId id) {
    auto it = m_actions.find(id);
    if (it != m_actions.end())
        return it.value();

    // Lazy-create
    IToolController* ctrl = m_controllers.value(id, nullptr);
    QString name = ctrl ? ctrl->displayName(id) : toolIdToString(id);
    auto* action = new QAction(name, this);
    action->setEnabled(ctrl ? ctrl->isEnabled(id) : false);
    connect(action, &QAction::triggered, this, [this, id]() { activate(id); });
    m_actions.insert(id, action);
    return action;
}

IToolController* ToolRegistry::controllerFor(ToolId id) const {
    return m_controllers.value(id, nullptr);
}

} // namespace gp
