// SPDX-License-Identifier: Apache-2.0
#include "TaskNav.h"

#include <QHash>

namespace gp {

const QVector<TaskSpec>& TaskNav::tasks() {
    // ── The ONE table ────────────────────────────────────────────────────────
    // Order = ScreenNav display order. Ids frozen (compared in
    // MainWindow::onScreenSelected and ModeController::setScreen).
    //
    // entryIsRoute=true is the U02 "one behavior per task" decision, chosen
    // and documented as: an OCR entry opens the OCR Verify screen (running a
    // recognition stays inside it, via the screen's Run button); Compare,
    // Compress and Watermark entries open their one task surface instead of
    // dispatching a second, parallel controller path.
    static const QVector<TaskSpec> table = {
        // id          title          kind                entryTool           route   pill       tab
        { "",          "Standard",    TaskKind::Standard,  ToolId::COUNT,      false, "view",    "Home"     },
        { "ocr",       "OCR Verify",  TaskKind::Workspace, ToolId::Ocr,        true,  "edit",    "Edit"     },
        { "redact",    "Redaction",   TaskKind::Workspace, ToolId::MarkRedact, false, "protect", "Protect"  },
        { "signature", "Signatures",  TaskKind::Panel,     ToolId::COUNT,      false, "protect", "Protect"  },
        { "compare",   "Compare",     TaskKind::Workspace, ToolId::Compare,    true,  "",        "View"     },
        { "pages",     "Pages",       TaskKind::Workspace, ToolId::COUNT,      false, "",        "Organize" },
        { "batch",     "Batch",       TaskKind::Workspace, ToolId::COUNT,      false, "",        "Convert"  },
        { "ai",        "AI Chat",     TaskKind::Toggle,    ToolId::COUNT,      false, "",        ""         },
        { "form",      "Form Builder",TaskKind::Workspace, ToolId::CreateForm, false, "form",    "Forms"    },
        { "compress",  "Compress",    TaskKind::Dialog,    ToolId::Compress,   true,  "",        "Convert"  },
        { "pdfa",      "PDF/A",       TaskKind::Panel,     ToolId::COUNT,      false, "",        "Convert"  },
        { "watermark", "Watermark",   TaskKind::Dialog,    ToolId::Watermark,  true,  "",        "Organize" },
    };
    return table;
}

const TaskSpec* TaskNav::forScreen(const QString& id) {
    static const QHash<QString, const TaskSpec*> byId = []() {
        QHash<QString, const TaskSpec*> m;
        for (const auto& t : tasks())
            m.insert(QString::fromLatin1(t.id), &t);
        return m;
    }();
    return byId.value(id, nullptr);
}

QString TaskNav::title(const QString& id) {
    if (const TaskSpec* spec = forScreen(id))
        return QString::fromLatin1(spec->title);
    return QString();
}

QString TaskNav::screenForTool(ToolId id) {
    // The documented sync set (research Q-U02 Module 1). Every entryTool in
    // the table round-trips to its own task; the rest return "" — zero
    // behavior change for the other ~60 ToolIds.
    switch (id) {
    case ToolId::Ocr:
        return QStringLiteral("ocr");
    case ToolId::MarkRedact:
    case ToolId::Sanitize:
    case ToolId::PatternRedact:
    case ToolId::RegexRedact:
        return QStringLiteral("redact");
    case ToolId::Compare:
        return QStringLiteral("compare");
    case ToolId::Compress:
        return QStringLiteral("compress");
    case ToolId::Watermark:
        return QStringLiteral("watermark");
    case ToolId::TextField:
    case ToolId::Checkbox:
    case ToolId::Radio:
    case ToolId::Dropdown:
    case ToolId::CreateForm:
    case ToolId::ListBox:
    case ToolId::Button:
    case ToolId::CalcField:
    case ToolId::DateField:
    case ToolId::NumField:
    case ToolId::SigField:
    case ToolId::AutoDetect:
    case ToolId::Tabs:
        return QStringLiteral("form");
    default:
        return QString();
    }
}

bool TaskNav::isEntryRoute(ToolId id) {
    if (const TaskSpec* spec = forScreen(screenForTool(id)))
        return spec->entryTool == id && spec->entryIsRoute;
    return false;
}

bool TaskNav::isKnownScreen(const QString& id) {
    return forScreen(id) != nullptr;
}

} // namespace gp
