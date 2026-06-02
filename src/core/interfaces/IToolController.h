// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/ToolId.h"
#include <QList>
#include <QString>

/// Pure-virtual interface that every tool controller must implement.
/// ToolRegistry queries handledTools() to build its dispatch map.
class IToolController {
public:
    virtual ~IToolController() = default;

    /// The set of ToolIds this controller is responsible for.
    virtual QList<ToolId> handledTools() const = 0;

    /// Execute the action associated with `id`.
    virtual void activate(ToolId id) = 0;

    /// Whether the tool is currently enabled (default: always true).
    virtual bool isEnabled(ToolId id) const { Q_UNUSED(id); return true; }

    /// Human-readable display name for the tool (falls back to canonical string).
    virtual QString displayName(ToolId id) const { return toolIdToString(id); }
};
