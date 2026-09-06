// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QVector>
#include "core/ToolId.h"

namespace gp {

// U02 — the ONE navigation registry.
//
// ToolRegistry stays the authority for ACTIONS (what runs). TaskNav owns the
// NAVIGATION dimension: every task the app can be "in", its one human name,
// and where each entry layer must land so the active label, tool behavior,
// and available controls agree after every transition (plan U02).
//
// The ids are FROZEN: they are compared by MainWindow::onScreenSelected and
// ModeController::setScreen. Only titles/kinds/pills/tabs live here.

enum class TaskKind {
    Standard,   // "" — the reading canvas
    Workspace,  // ModeController swaps this screen into the center
    Panel,      // right-dock swap only (signature, pdfa)
    Dialog,     // modal dialog (compress, watermark) — snap-back semantics
    Toggle      // ai — a panel toggle, not a screen swap
};

struct TaskSpec {
    const char* id;          // ScreenNav/ModeController id ("ocr", "redact", "" ...)
    const char* title;       // Human task name ("OCR Verify") — NO numeric prefix
    TaskKind kind;
    ToolId entryTool;        // The authoritative action for the task, if any;
                             // ToolId::COUNT when the task has no single one.
    bool entryIsRoute;       // true: activating entryTool from ANY chrome layer
                             // routes to this task's screen INSTEAD of also
                             // dispatching the raw controller action (one
                             // behavior per task — the U02 defect fix for the
                             // ribbon OCR button). false: the tool action runs
                             // normally and only the visible state syncs.
    const char* modePill;    // ModeStrip pill to select when entered, or "" =
                             // leave unchanged.
    const char* ribbonTab;   // Ribbon tab to raise when entered, or "" = leave
                             // unchanged. Must match RibbonModel::tabs() names.
};

class TaskNav {
public:
    /// The ONE table, in ScreenNav display order.
    static const QVector<TaskSpec>& tasks();

    /// Lookup by screen id; nullptr for unknown ids.
    static const TaskSpec* forScreen(const QString& id);

    /// Human title for a screen id ("" → "Standard"; unknown → "").
    static QString title(const QString& id);

    /// Tool → screen sync: which screen's visible state must agree after the
    /// tool ran ("" = no sync — the overwhelming majority of tools).
    static QString screenForTool(ToolId id);

    /// True when the tool is a pure task ENTRY (its activation navigates to
    /// the task's screen instead of also dispatching the raw action).
    static bool isEntryRoute(ToolId id);

    static bool isKnownScreen(const QString& id);
};

} // namespace gp
