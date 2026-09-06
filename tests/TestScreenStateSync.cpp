// SPDX-License-Identifier: Apache-2.0
// U02 — TaskStateSync is the SINGLE writer of visible navigation state.
//
// What these tests pin (plan U02 + research Q-U02 Module 3):
//   1. apply(screenId) drives exactly four visible-state widgets and nothing
//      else: ScreenNav checked-state, the status bar's current-task line, the
//      ModeStrip pill (only when the task names one), and the Ribbon tab
//      (only when the task names one).
//   2. Entering OCR selects "edit" + the Edit tab; entering Form Builder
//      selects the "form" pill — the active label, tool behavior target and
//      available controls agree after each transition.
//   3. Tasks with an empty modePill/ribbonTab leave those widgets untouched
//      ("" means "leave unchanged").
//   4. apply is idempotent: applying the same screen twice emits no further
//      visible-state change.
//   5. TaskStateSync is headless-constructible and tolerates null widgets
//      (a stubbed harness must not crash).
#include <QtTest>
#include <QSignalSpy>
#include <QToolButton>

#include "shell/TaskStateSync.h"
#include "shell/TaskNav.h"
#include "shell/ScreenNav.h"
#include "shell/StatusBar.h"
#include "shell/ModeStrip.h"
#include "shell/Ribbon.h"
#include "shell/RibbonModel.h"

using namespace gp;

namespace {
QToolButton* navButton(ScreenNav* nav, const QString& id)
{
    return nav->findChild<QToolButton*>(QStringLiteral("screenNav_") + id);
}
} // namespace

class TestScreenStateSync : public QObject {
    Q_OBJECT

private slots:
    void applyOcrAgreesEverywhere();
    void applyStandardIsTheReadingState();
    void applyFormSelectsFormPill();
    void emptyPillAndTabMeanLeaveUnchanged();
    void applyIsIdempotent();
    void nullWidgetsAreTolerated();

private:
    // A real widget root owns the four widgets; everything lives and dies
    // with the stack-allocated Harness.
    struct Harness : QWidget {
        ScreenNav* nav = nullptr;
        StatusBar* status = nullptr;
        ModeStrip* strip = nullptr;
        Ribbon* ribbon = nullptr;
        TaskStateSync* sync = nullptr;

        Harness() {
            nav    = new ScreenNav(this);
            status = new StatusBar(this);
            strip  = new ModeStrip(this);
            ribbon = new Ribbon(this);
            sync   = new TaskStateSync(nav, status, strip, ribbon, this);
            sync->apply(QString());   // defined startup state (Standard)
        }
    };
};

void TestScreenStateSync::applyOcrAgreesEverywhere()
{
    Harness h;

    h.sync->apply(QStringLiteral("ocr"));

    QCOMPARE(h.sync->currentScreen(), QStringLiteral("ocr"));
    QCOMPARE(h.nav->active(), QStringLiteral("ocr"));
    QToolButton* b = navButton(h.nav, QStringLiteral("ocr"));
    QVERIFY(b != nullptr);
    QVERIFY2(b->isChecked(), "ScreenNav OCR button must be checked after apply(\"ocr\")");
    // The status line carries the human task name (no numbering, no raw id).
    QCOMPARE(h.status->currentTask(), QStringLiteral("OCR Verify"));
    // OCR lives in the Edit working mode and the Edit ribbon tab.
    QCOMPARE(h.strip->mode(), QStringLiteral("edit"));
    QCOMPARE(h.ribbon->activeTabName(), QStringLiteral("Edit"));
}

void TestScreenStateSync::applyStandardIsTheReadingState()
{
    Harness h;
    h.sync->apply(QStringLiteral("ocr"));
    QVERIFY(h.sync->currentScreen() == QStringLiteral("ocr"));

    h.sync->apply(QString());

    QCOMPARE(h.sync->currentScreen(), QString());
    QCOMPARE(h.nav->active(), QString());
    QCOMPARE(h.status->currentTask(), QStringLiteral("Standard"));
    QCOMPARE(h.strip->mode(), QStringLiteral("view"));
    QCOMPARE(h.ribbon->activeTabName(), QStringLiteral("Home"));
}

void TestScreenStateSync::applyFormSelectsFormPill()
{
    Harness h;

    h.sync->apply(QStringLiteral("form"));

    QCOMPARE(h.nav->active(), QStringLiteral("form"));
    QCOMPARE(h.status->currentTask(), QStringLiteral("Form Builder"));
    QCOMPARE(h.strip->mode(), QStringLiteral("form"));
    QCOMPARE(h.ribbon->activeTabName(), QStringLiteral("Forms"));
}

void TestScreenStateSync::emptyPillAndTabMeanLeaveUnchanged()
{
    Harness h;
    // Defined start: Standard → view / Home.
    QCOMPARE(h.strip->mode(), QStringLiteral("view"));
    QCOMPARE(h.ribbon->activeTabName(), QStringLiteral("Home"));

    // Compare names no pill ("leave unchanged") but does name its View tab
    // (the Compare tool lives there). The pill must stay untouched.
    h.sync->apply(QStringLiteral("compare"));

    QCOMPARE(h.sync->currentScreen(), QStringLiteral("compare"));
    QCOMPARE(h.nav->active(), QStringLiteral("compare"));
    QCOMPARE(h.status->currentTask(), QStringLiteral("Compare"));
    QCOMPARE(h.strip->mode(), QStringLiteral("view"));
    QCOMPARE(h.ribbon->activeTabName(), QStringLiteral("View"));
}

void TestScreenStateSync::applyIsIdempotent()
{
    Harness h;
    QSignalSpy tabSpy(h.ribbon, &Ribbon::tabChanged);
    QSignalSpy appliedSpy(h.sync, &TaskStateSync::applied);

    h.sync->apply(QStringLiteral("ocr"));
    const int tabsAfterFirst = tabSpy.count();
    const int appliedAfterFirst = appliedSpy.count();
    QVERIFY(tabsAfterFirst >= 1);   // "Edit" was raised
    QVERIFY(appliedAfterFirst == 1);

    // Second identical apply must change nothing visible.
    h.sync->apply(QStringLiteral("ocr"));

    QCOMPARE(tabSpy.count(), tabsAfterFirst);
    QCOMPARE(appliedSpy.count(), appliedAfterFirst + 1);   // the call itself happened…
    QCOMPARE(h.nav->active(), QStringLiteral("ocr"));
    QCOMPARE(h.strip->mode(), QStringLiteral("edit"));
    QCOMPARE(h.ribbon->activeTabName(), QStringLiteral("Edit"));
}

void TestScreenStateSync::nullWidgetsAreTolerated()
{
    // A stubbed harness (no widgets at all) must not crash — the sync object
    // guards every writer.
    TaskStateSync sync(nullptr, nullptr, nullptr, nullptr, this);
    sync.apply(QStringLiteral("ocr"));
    sync.apply(QString());
    QCOMPARE(sync.currentScreen(), QString());
}

QTEST_MAIN(TestScreenStateSync)
#include "TestScreenStateSync.moc"
