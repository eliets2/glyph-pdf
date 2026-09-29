// SPDX-License-Identifier: Apache-2.0
// UI redesign Phase 0 (plan 06 §7.1): gp::CommandRegistry — the command
// metadata catalogue the new shell renders from. These pins guard the
// contract every later phase relies on:
//   * the bundled resources/commands.json loads (schema 1) in any binary
//     that links pdfws_ui, tests included;
//   * command ids ARE canonical ToolId strings (never an alias spelling);
//   * every ToolId the app ships has a command, or is recorded as internal
//     with a reason — nothing is dropped silently;
//   * every command carries a label and a description, and every planned
//     command states why it is disabled and what to use instead;
//   * malformed or wrong-schema files are refused, leaving the registry empty.
#include <QtTest/QtTest>

#include "core/CommandRegistry.h"
#include "core/ToolId.h"

using gp::CommandRegistry;
using gp::CommandSpec;

class TestCommandRegistry : public QObject {
    Q_OBJECT

private slots:
    void bundledRegistryLoads() {
        const CommandRegistry& reg = CommandRegistry::instance();
        QCOMPARE(reg.schema(), CommandRegistry::kSchema);
        QVERIFY2(reg.size() >= 300, qPrintable(QStringLiteral("only %1 commands").arg(reg.size())));
    }

    void everyShippedToolIdHasACommandOrIsInternal() {
        const CommandRegistry& reg = CommandRegistry::instance();
        QStringList missing;
        for (int i = 0; i < int(ToolId::COUNT); ++i) {
            const QString id = toolIdToString(static_cast<ToolId>(i));
            if (reg.find(id) || reg.internalToolIds().contains(id))
                continue;
            missing << id;
        }
        QVERIFY2(missing.isEmpty(),
                 qPrintable(QStringLiteral("ToolIds with no command and no internal reason: %1")
                                .arg(missing.join(QStringLiteral(", ")))));
        for (auto it = reg.internalToolIds().begin(); it != reg.internalToolIds().end(); ++it) {
            QVERIFY2(isValidToolIdString(it.key()),
                     qPrintable(QStringLiteral("internal id %1 is not a ToolId").arg(it.key())));
            QVERIFY2(!it.value().trimmed().isEmpty(),
                     qPrintable(QStringLiteral("internal id %1 has no reason").arg(it.key())));
        }
    }

    void commandIdsAreCanonicalToolIdStrings() {
        const CommandRegistry& reg = CommandRegistry::instance();
        QStringList aliases;
        for (const QString& id : reg.ids()) {
            const auto tool = toolIdFromString(id);
            if (tool && toolIdToString(*tool) != id)
                aliases << QStringLiteral("%1 -> %2").arg(id, toolIdToString(*tool));
        }
        QVERIFY2(aliases.isEmpty(),
                 qPrintable(QStringLiteral("ids spelled as aliases: %1").arg(aliases.join(QStringLiteral(", ")))));
    }

    void everyCommandCarriesLabelAndDescription() {
        const CommandRegistry& reg = CommandRegistry::instance();
        for (const QString& id : reg.ids()) {
            const CommandSpec* spec = reg.find(id);
            QVERIFY(spec);
            QVERIFY2(!spec->label.trimmed().isEmpty(), qPrintable(id + QStringLiteral(": empty label")));
            QVERIFY2(!spec->description.trimmed().isEmpty(),
                     qPrintable(id + QStringLiteral(": empty description")));
            QVERIFY2(!spec->displayName().trimmed().isEmpty(), qPrintable(id));
        }
    }

    void plannedCommandsExplainThemselves() {
        const CommandRegistry& reg = CommandRegistry::instance();
        int planned = 0;
        for (const QString& id : reg.ids()) {
            const CommandSpec* spec = reg.find(id);
            if (!spec->isPlanned())
                continue;
            ++planned;
            QVERIFY2(!spec->plannedAlternative.trimmed().isEmpty(),
                     qPrintable(id + QStringLiteral(": planned without a supported alternative")));
        }
        QVERIFY2(planned > 0, "the registry should carry the honest planned entries");
    }

    void lookupByToolIdMatchesLookupByString() {
        const CommandRegistry& reg = CommandRegistry::instance();
        const CommandSpec* byEnum = reg.find(ToolId::Undo);
        QVERIFY(byEnum);
        QCOMPARE(byEnum, reg.find(toolIdToString(ToolId::Undo)));
        QCOMPARE(byEnum->label, QStringLiteral("Undo"));
        QCOMPARE(byEnum->shortcut, QStringLiteral("Ctrl+Z"));
        QVERIFY(!byEnum->isPlanned());
        QVERIFY(reg.find(QStringLiteral("no-such-command")) == nullptr);
    }

    void malformedDocumentsAreRejected_data() {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("not json") << QByteArray("{");
        QTest::newRow("not an object") << QByteArray("[]");
        QTest::newRow("wrong schema")
            << QByteArray(R"({"_meta":{"schema":2},"commands":{"undo":{"label":"Undo"}}})");
        QTest::newRow("no commands") << QByteArray(R"({"_meta":{"schema":1}})");
        QTest::newRow("no label")
            << QByteArray(R"({"_meta":{"schema":1},"commands":{"undo":{"description":"x"}}})");
        QTest::newRow("planned without reason")
            << QByteArray(R"({"_meta":{"schema":1},"commands":{"x":{"label":"X","planned":{"alternative":"y"}}}})");
    }

    void malformedDocumentsAreRejected() {
        QFETCH(QByteArray, json);
        CommandRegistry reg;
        QVERIFY(reg.load(QByteArray(R"({"_meta":{"schema":1},"commands":{"undo":{"label":"Undo"}}})")));
        QCOMPARE(reg.size(), 1);
        QString error;
        QVERIFY(!reg.load(json, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(reg.size(), 0);   // a refused load never leaves stale commands behind
        QCOMPARE(reg.schema(), 0);
    }
};

QTEST_GUILESS_MAIN(TestCommandRegistry)
#include "TestCommandRegistry.moc"
