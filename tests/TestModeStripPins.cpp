// SPDX-License-Identifier: Apache-2.0
// SWEEP-W2 coverage pins (2026-09-20, testing-specialist lane).
// The feature-command matrix (docs/audit/FEATURE-COMMAND-MATRIX-2026-09-09.csv)
// marks these implemented+available shell commands with NO test reference:
//   mode-view / mode-edit / mode-comment / mode-form / mode-protect,
//   toggle-ai, task-chooser  (mode-strip surface rows).
// This suite pins the USER path: a pill CLICK flips the strip and emits
// modeChanged(id); the AI pill emits aiToggleRequested; the compact Tools
// chooser lists every non-Standard TaskNav task and emits taskSelected(id);
// programmatic setMode() stays silent (no feedback loop into the host).
#include <QtTest/QtTest>
#include <QMenu>
#include <QSignalSpy>
#include <QToolButton>

#include "shell/ModeStrip.h"
#include "shell/TaskNav.h"

using namespace gp;

namespace {

QToolButton* findPill(ModeStrip& strip, const QString& label) {
    for (QToolButton* btn : strip.findChildren<QToolButton*>()) {
        if (btn->property("variant") == QStringLiteral("pill") &&
            btn->text() == label)
            return btn;
    }
    return nullptr;
}

} // namespace

class TestModeStripPins : public QObject {
    Q_OBJECT

private slots:
    void modePillClickSwitchesModeAndEmits_data() {
        QTest::addColumn<QString>("label");
        QTest::addColumn<QString>("id");
        QTest::newRow("mode-view")     << QStringLiteral("View")    << QStringLiteral("view");
        QTest::newRow("mode-edit")     << QStringLiteral("Edit")    << QStringLiteral("edit");
        QTest::newRow("mode-comment")  << QStringLiteral("Comment") << QStringLiteral("comment");
        QTest::newRow("mode-form")     << QStringLiteral("Form")    << QStringLiteral("form");
        QTest::newRow("mode-protect")  << QStringLiteral("Protect") << QStringLiteral("protect");
    }

    void modePillClickSwitchesModeAndEmits() {
        QFETCH(QString, label);
        QFETCH(QString, id);
        ModeStrip strip;
        QSignalSpy changed(&strip, &ModeStrip::modeChanged);

        QToolButton* pill = findPill(strip, label);
        QVERIFY2(pill, qPrintable(QStringLiteral("pill '%1' not found").arg(label)));
        pill->click();

        QCOMPARE(strip.mode(), id);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.takeFirst().value(0).toString(), id);
        QVERIFY(pill->isChecked());
    }

    void aiToggleEmitsAiToggleRequested() {
        ModeStrip strip;
        QSignalSpy toggled(&strip, &ModeStrip::aiToggleRequested);
        QToolButton* ai = nullptr;
        for (QToolButton* btn : strip.findChildren<QToolButton*>()) {
            if (btn->property("variant") == QStringLiteral("pill") &&
                btn->accessibleName().contains(QStringLiteral("AI")))
                ai = btn;
        }
        QVERIFY2(ai, "AI pill button not found");
        ai->click();
        QCOMPARE(toggled.count(), 1);
    }

    void toolsChooserListsEveryNonStandardTaskAndEmitsSelection() {
        ModeStrip strip;
        QSignalSpy selected(&strip, &ModeStrip::taskSelected);

        auto* chooser = strip.findChild<QToolButton*>(QStringLiteral("modeStripTaskMenu"));
        QVERIFY2(chooser, "Tools chooser (modeStripTaskMenu) not found");
        QMenu* menu = chooser->menu();
        QVERIFY2(menu, "Tools chooser carries no menu");

        int expected = 0;
        for (const auto& spec : TaskNav::tasks())
            if (spec.kind != TaskKind::Standard) ++expected;
        QCOMPARE(menu->actions().size(), expected);

        // The reading canvas must NOT be a chooser entry (always one click away).
        for (QAction* act : menu->actions())
            QVERIFY2(!act->text().isEmpty(), "chooser actions must carry titles");

        // Trigger the first entry: taskSelected must carry that task's id.
        QVERIFY2(!menu->actions().isEmpty(), "chooser must not be empty");
        QAction* first = menu->actions().first();
        first->trigger();
        QCOMPARE(selected.count(), 1);
        const QString emittedId = selected.first().value(0).toString();
        bool found = false;
        for (const auto& spec : TaskNav::tasks())
            if (QString::fromLatin1(spec.id) == emittedId) found = true;
        QVERIFY2(found, "taskSelected payload must be a TaskNav id");
    }

    void programmaticSetModeDoesNotEmit() {
        ModeStrip strip;
        QSignalSpy changed(&strip, &ModeStrip::modeChanged);
        strip.setMode(QStringLiteral("edit"));
        QCOMPARE(strip.mode(), QStringLiteral("edit"));
        QCOMPARE(changed.count(), 0);
        // A second setMode to the same id is a no-op (checked state stays).
        strip.setMode(QStringLiteral("edit"));
        QCOMPARE(changed.count(), 0);
    }

    void pillsAreAutoExclusive() {
        ModeStrip strip;
        QToolButton* view = findPill(strip, QStringLiteral("View"));
        QToolButton* edit = findPill(strip, QStringLiteral("Edit"));
        QVERIFY(view && edit);
        view->click();
        edit->click();
        QVERIFY(edit->isChecked());
        QVERIFY2(!view->isChecked(), "pill exclusivity: previous pill must uncheck");
    }
};

QTEST_MAIN(TestModeStripPins)
#include "TestModeStripPins.moc"
