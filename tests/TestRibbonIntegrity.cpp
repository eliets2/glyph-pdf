// SPDX-License-Identifier: Apache-2.0
// TestRibbonIntegrity — R2-4 D5
// Asserts that every ENABLED ribbon button (i.e. not in RibbonModel::plannedTools())
// resolves to a known ToolId AND has a handler in at least one controller.
// Guards against future silent-button regressions.

#include <QtTest>
#include <QApplication>

#include "core/AppContext.h"
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"
#include "shell/RibbonModel.h"
#include "shell/controllers/HomeController.h"
#include "shell/controllers/ViewController.h"
#include "shell/controllers/EditController.h"
#include "shell/controllers/PagesController.h"
#include "shell/controllers/ConvertController.h"
#include "shell/controllers/FormsController.h"
#include "shell/controllers/SecurityController.h"
#include "shell/controllers/TaskNavController.h"   // R15: task-surface routes

class TestRibbonIntegrity : public QObject {
    Q_OBJECT

private:
    AppContext m_ctx;

private slots:
    void testEveryEnabledToolHasHandler() {
        gp::HomeController     home(&m_ctx, nullptr);
        gp::ViewController     view(&m_ctx, nullptr);
        gp::EditController     edit(&m_ctx, nullptr);
        gp::PagesController    pages(&m_ctx, nullptr);
        gp::ConvertController  convert(&m_ctx, nullptr);
        gp::FormsController    forms(&m_ctx, nullptr);
        gp::SecurityController security(&m_ctx, nullptr);
        gp::TaskNavController  taskNav(&m_ctx, nullptr);   // R15: task-surface routes

        QVector<IToolController*> controllers = {
            &home, &view, &edit, &pages, &convert, &forms, &security, &taskNav
        };

        // Collect all ToolIds that have at least one controller
        QSet<ToolId> handled;
        for (auto* ctrl : controllers) {
            for (ToolId id : ctrl->handledTools())
                handled.insert(id);
        }

        const QSet<QString>& planned = gp::RibbonModel::plannedTools();
        QStringList failures;

        for (const auto& tab : gp::RibbonModel::tabs()) {
            for (const auto& grp : tab.groups) {
                for (const auto& tool : grp.tools) {
                    // Skip planned tools — they are intentionally disabled
                    if (planned.contains(tool.id))
                        continue;

                    // Every enabled tool must map to a known ToolId
                    auto optId = toolIdFromString(tool.id);
                    if (!optId.has_value()) {
                        failures << QString("Ribbon tool '%1' in tab '%2' / group '%3' "
                                            "does not map to any ToolId (add alias in ToolId.cpp or "
                                            "add to RibbonModel::plannedTools())")
                                        .arg(tool.id, tab.name, grp.title);
                        continue;
                    }

                    // It must also have a controller handler
                    if (!handled.contains(optId.value())) {
                        failures << QString("Ribbon tool '%1' (ToolId::%2) in tab '%3' / group '%4' "
                                            "has no controller handler (wire it or add to plannedTools())")
                                        .arg(tool.id,
                                             toolIdToString(optId.value()),
                                             tab.name,
                                             grp.title);
                    }
                }
            }
        }

        QVERIFY2(failures.isEmpty(),
            qPrintable("Ribbon integrity failures:\n  " + failures.join("\n  ")));
    }

    void testPlannedSpecsAreDisclosedAndCovered() {
        // R15 (PP06/UI01): every planned entry must (a) carry a non-empty
        // truthful reason AND a non-empty supported alternative, (b) actually
        // appear in the ribbon model (no orphan specs), and (c) NOT resolve to
        // a controller handler (promoted entries leave the table with their
        // wiring commit).
        gp::HomeController     home(&m_ctx, nullptr);
        gp::ViewController     view(&m_ctx, nullptr);
        gp::EditController     edit(&m_ctx, nullptr);
        gp::PagesController    pages(&m_ctx, nullptr);
        gp::ConvertController  convert(&m_ctx, nullptr);
        gp::FormsController    forms(&m_ctx, nullptr);
        gp::SecurityController security(&m_ctx, nullptr);
        gp::TaskNavController  taskNav(&m_ctx, nullptr);

        QSet<ToolId> handled;
        const QVector<IToolController*> controllers = {
            &home, &view, &edit, &pages, &convert, &forms, &security, &taskNav
        };
        for (auto* ctrl : controllers)
            for (ToolId id : ctrl->handledTools())
                handled.insert(id);

        QSet<QString> ribbonIds;
        for (const auto& tab : gp::RibbonModel::tabs())
            for (const auto& grp : tab.groups)
                for (const auto& tool : grp.tools)
                    ribbonIds.insert(tool.id);

        QStringList failures;
        for (const auto& spec : gp::RibbonModel::plannedToolSpecs()) {
            if (spec.reason.trimmed().isEmpty() || spec.alternative.trimmed().isEmpty())
                failures << QString("Planned entry '%1' must carry a reason AND an alternative")
                                    .arg(spec.id);
            if (!ribbonIds.contains(spec.id))
                failures << QString("Planned entry '%1' does not appear in the ribbon model")
                                    .arg(spec.id);
            const auto optId = toolIdFromString(spec.id);
            if (optId.has_value() && handled.contains(optId.value()))
                failures << QString("Planned entry '%1' resolves to a wired controller — "
                                    "remove it from plannedToolSpecs()").arg(spec.id);
        }
        QVERIFY2(failures.isEmpty(),
            qPrintable("Planned-spec failures:\n  " + failures.join("\n  ")));
    }

    void testCloudOrphansNotInRibbon() {
        // UX-02: sendForm, collect, submit, auditLog, dlp, policy must not appear
        // anywhere in the ribbon — they were removed as cloud-orphans in R2-4.
        const QStringList cloudOrphans = {
            "sendForm", "collect", "submit", "auditLog", "dlp", "policy"
        };

        QStringList found;
        for (const auto& tab : gp::RibbonModel::tabs()) {
            for (const auto& grp : tab.groups) {
                for (const auto& tool : grp.tools) {
                    if (cloudOrphans.contains(tool.id))
                        found << QString("'%1' still present in tab '%2' / group '%3'")
                                     .arg(tool.id, tab.name, grp.title);
                }
            }
        }

        QVERIFY2(found.isEmpty(),
            qPrintable("Cloud-orphan tools still in ribbon:\n  " + found.join("\n  ")));
    }

    void testNoToolAppearsInBothPlannedAndRibbon() {
        // Sanity: a tool can be in the ribbon AND in planned (that is fine — planned ones
        // are disabled). But it must NOT be in planned AND wired to a controller at the
        // same time (that would be a logic error — a disabled button that somehow fires).
        gp::HomeController     home(&m_ctx, nullptr);
        gp::ViewController     view(&m_ctx, nullptr);
        gp::EditController     edit(&m_ctx, nullptr);
        gp::PagesController    pages(&m_ctx, nullptr);
        gp::ConvertController  convert(&m_ctx, nullptr);
        gp::FormsController    forms(&m_ctx, nullptr);
        gp::SecurityController security(&m_ctx, nullptr);
        gp::TaskNavController  taskNav(&m_ctx, nullptr);   // R15: task-surface routes

        QVector<IToolController*> controllers = {
            &home, &view, &edit, &pages, &convert, &forms, &security, &taskNav
        };

        QSet<ToolId> handled;
        for (auto* ctrl : controllers)
            for (ToolId id : ctrl->handledTools())
                handled.insert(id);

        const QSet<QString>& planned = gp::RibbonModel::plannedTools();
        QStringList conflicts;

        for (const QString& pid : planned) {
            auto optId = toolIdFromString(pid);
            if (optId.has_value() && handled.contains(optId.value())) {
                conflicts << QString("Tool '%1' is both in plannedTools() AND handled by a controller "
                                     "— remove it from plannedTools()")
                                 .arg(pid);
            }
        }

        QVERIFY2(conflicts.isEmpty(),
            qPrintable("Planned/handler conflicts:\n  " + conflicts.join("\n  ")));
    }
};

QTEST_MAIN(TestRibbonIntegrity)
#include "TestRibbonIntegrity.moc"
