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

        QVector<IToolController*> controllers = {
            &home, &view, &edit, &pages, &convert, &forms, &security
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

        QVector<IToolController*> controllers = {
            &home, &view, &edit, &pages, &convert, &forms, &security
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
