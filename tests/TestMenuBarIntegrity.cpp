// SPDX-License-Identifier: Apache-2.0
// TestMenuBarIntegrity — UI-wiring hardening (2026-06-22)
// The menu bar, unlike the ribbon, had no integrity gate: several QActions
// dispatched toolId strings that resolved to no ToolId/handler and silently
// no-op'd. This test mirrors TestRibbonIntegrity for MenuBar::actionSpecs():
// every menu item must either
//   (a) be MenuDispatch::Registry  → resolve to a known ToolId AND be handled by
//       a controller (so onToolActivated actually fires something), or
//   (b) be MenuDispatch::Local     → be listed in MenuBar::localHandlerIds()
//       (handled inline in MenuBar, valid by construction), or
//   (c) be MenuDispatch::Disabled  → an intentionally greyed-out / planned item.
// This would have caught the dead Edit-menu items and the missing Forms/Document
// kebab aliases.

#include <QtTest>
#include <QApplication>

#include "core/AppContext.h"
#include "core/ToolId.h"
#include "core/interfaces/IToolController.h"
#include "shell/MenuBar.h"
#include "shell/controllers/HomeController.h"
#include "shell/controllers/ViewController.h"
#include "shell/controllers/EditController.h"
#include "shell/controllers/PagesController.h"
#include "shell/controllers/ConvertController.h"
#include "shell/controllers/FormsController.h"
#include "shell/controllers/SecurityController.h"

class TestMenuBarIntegrity : public QObject {
    Q_OBJECT

private:
    AppContext m_ctx;

    QSet<ToolId> handledTools() {
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
        return handled;
    }

private slots:
    // Every Registry menu item must resolve to a wired controller handler.
    void testEveryRegistryItemHasHandler() {
        const QSet<ToolId> handled = handledTools();
        QStringList failures;

        for (const auto& spec : gp::MenuBar::actionSpecs()) {
            if (spec.dispatch != gp::MenuDispatch::Registry)
                continue;

            const QString id = QString::fromLatin1(spec.toolId);

            auto optId = toolIdFromString(id);
            if (!optId.has_value()) {
                failures << QString("Menu item '%1' is Registry-dispatched but maps to no "
                                    "ToolId (add an alias in ToolId.cpp, or reclassify it "
                                    "Local/Disabled in MenuBar::actionSpecs())").arg(id);
                continue;
            }
            if (!handled.contains(optId.value())) {
                failures << QString("Menu item '%1' (ToolId::%2) has no controller handler "
                                    "(wire it, or reclassify it Disabled in "
                                    "MenuBar::actionSpecs())")
                                .arg(id, toolIdToString(optId.value()));
            }
        }

        QVERIFY2(failures.isEmpty(),
            qPrintable("MenuBar integrity failures:\n  " + failures.join("\n  ")));
    }

    // Every Local menu item must actually be handled inline in MenuBar.
    void testEveryLocalItemIsDeclared() {
        const QList<QString>& localIds = gp::MenuBar::localHandlerIds();
        QStringList failures;

        for (const auto& spec : gp::MenuBar::actionSpecs()) {
            if (spec.dispatch != gp::MenuDispatch::Local)
                continue;
            const QString id = QString::fromLatin1(spec.toolId);
            if (!localIds.contains(id)) {
                failures << QString("Menu item '%1' is classified Local but is not in "
                                    "MenuBar::localHandlerIds() — it would silently no-op")
                                .arg(id);
            }
        }

        QVERIFY2(failures.isEmpty(),
            qPrintable("MenuBar Local-handler failures:\n  " + failures.join("\n  ")));
    }

    // Guard against drift: no toolId may carry two conflicting dispatch classes,
    // and the spec table must not be empty.
    void testNoDuplicateOrEmptySpecs() {
        const auto& specs = gp::MenuBar::actionSpecs();
        QVERIFY2(!specs.isEmpty(), "MenuBar::actionSpecs() is empty");

        QHash<QString, gp::MenuDispatch> seen;
        QStringList conflicts;
        for (const auto& spec : specs) {
            const QString id = QString::fromLatin1(spec.toolId);
            auto it = seen.find(id);
            if (it != seen.end() && it.value() != spec.dispatch) {
                conflicts << QString("Menu item '%1' appears with conflicting dispatch "
                                     "classes").arg(id);
            }
            seen.insert(id, spec.dispatch);
        }

        QVERIFY2(conflicts.isEmpty(),
            qPrintable("MenuBar spec conflicts:\n  " + conflicts.join("\n  ")));
    }
};

QTEST_MAIN(TestMenuBarIntegrity)
#include "TestMenuBarIntegrity.moc"
