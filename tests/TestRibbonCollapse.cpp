// SPDX-License-Identifier: Apache-2.0
// U02 — the collapsible ribbon.
//
// What these tests pin (plan U02 + research Q-U02 Module 7 / Finding U02-4):
//   1. Collapsed = commands hide, the tab row stays. The body stack
//      (#ribbonBody, fixed RibbonBodyH) is hidden, never re-parented, and
//      the ribbon's layout height shrinks by exactly the body height.
//   2. The active task stays visible while collapsed (a non-empty
//      "<tab> · <tool>" label appears next to the tabs).
//   3. raiseTab(name) maps a RibbonModel tab name to the index and raises it
//      — and works while collapsed (the TaskStateSync channel must not
//      depend on the expansion state).
//   4. Triggers: the chevron button, double-click on the tab bar, and
//      Ctrl+F1 all toggle the collapsed state and announce it via
//      collapsedChanged(bool).
//   5. Clicking a tab while collapsed expands and stays expanded
//      (documented v1 simplification of the Microsoft Fluent pattern).
//   6. No animation, no QDockWidget/QToolBar tricks — plain hide/show of the
//      existing body stack.
#include <QtTest>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QLabel>

#include "shell/Ribbon.h"
#include "shell/RibbonModel.h"
#include "util/GpTheme.h"

using namespace gp;

namespace {
// Brings a fresh Ribbon into a laid-out, shown state for geometry assertions.
struct ShownRibbon {
    Ribbon ribbon;
    int expandedHeight = 0;

    ShownRibbon() {
        ribbon.setObjectName(QStringLiteral("ribbonUnderTest"));
        ribbon.resize(900, 300);
        ribbon.show();
        QVERIFY(QTest::qWaitForWindowExposed(&ribbon));
        QApplication::processEvents();
        expandedHeight = ribbon.height();
    }
    QStackedWidget* body() const {
        return ribbon.findChild<QStackedWidget*>(QStringLiteral("ribbonBody"));
    }
    QTabBar* tabs() const {
        return ribbon.findChild<QTabBar*>(QStringLiteral("ribbonTabs"));
    }
};
} // namespace

class TestRibbonCollapse : public QObject {
    Q_OBJECT

private slots:
    void expandedShowsBody();
    void collapseHidesBodyKeepsTabs();
    void collapseShrinksHeightByBodyHeight();
    void collapsedChangedSignal();
    void activeTaskLabelVisibleAndPopulatedWhenCollapsed();
    void raiseTabWorksWhileCollapsed();
    void chevronToggles();
    void ctrlF1Toggles();
    void doubleClickOnTabsToggles();
    void clickingATabWhileCollapsedExpands();

private:
    static QToolButton* chevron(const Ribbon& r) {
        return r.findChild<QToolButton*>(QStringLiteral("ribbonCollapseBtn"));
    }
};

void TestRibbonCollapse::expandedShowsBody()
{
    ShownRibbon s;
    QVERIFY(!s.ribbon.isCollapsed());
    QVERIFY(s.body() != nullptr);
    QVERIFY(s.body()->isVisible());
    QCOMPARE(s.ribbon.bodyHeight(), Theme::RibbonBodyH);
    QCOMPARE(s.ribbon.activeTabName(), RibbonModel::tabs().first().name);
}

void TestRibbonCollapse::collapseHidesBodyKeepsTabs()
{
    ShownRibbon s;

    s.ribbon.setCollapsed(true);

    QVERIFY(s.ribbon.isCollapsed());
    QVERIFY(!s.body()->isVisible());
    QVERIFY(s.tabs()->isVisible());
    QCOMPARE(s.ribbon.bodyHeight(), 0);

    s.ribbon.setCollapsed(false);

    QVERIFY(!s.ribbon.isCollapsed());
    QVERIFY(s.body()->isVisible());
    QCOMPARE(s.ribbon.bodyHeight(), Theme::RibbonBodyH);
}

void TestRibbonCollapse::collapseShrinksHeightByBodyHeight()
{
    ShownRibbon s;
    const int expandedHint = s.ribbon.sizeHint().height();
    QVERIFY(expandedHint >= Theme::RibbonBodyH + s.tabs()->height());

    // A window keeps its explicit height when the ribbon collapses (the
    // freed space goes to the content area, like Office) — the LAYOUT
    // contract is that the ribbon's size hint shrinks by exactly the body.
    s.ribbon.setCollapsed(true);
    QApplication::processEvents();
    const int collapsedHint = s.ribbon.sizeHint().height();

    QVERIFY(collapsedHint < expandedHint);
    QCOMPARE(expandedHint - collapsedHint, Theme::RibbonBodyH);

    s.ribbon.setCollapsed(false);
    QApplication::processEvents();
    QCOMPARE(s.ribbon.sizeHint().height(), expandedHint);
}

void TestRibbonCollapse::collapsedChangedSignal()
{
    ShownRibbon s;
    QSignalSpy spy(&s.ribbon, &Ribbon::collapsedChanged);

    s.ribbon.setCollapsed(true);
    s.ribbon.setCollapsed(true);   // no change — no second emission
    s.ribbon.setCollapsed(false);

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.first().at(0).toBool(), true);
    QCOMPARE(spy.last().at(0).toBool(), false);
}

void TestRibbonCollapse::activeTaskLabelVisibleAndPopulatedWhenCollapsed()
{
    ShownRibbon s;
    auto* label = s.ribbon.findChild<QLabel*>(QStringLiteral("ribbonActiveTaskLabel"));
    QVERIFY(label != nullptr);
    QVERIFY(!label->isVisible());

    s.ribbon.setCollapsed(true);

    QVERIFY2(label->isVisible(), "active-task label must be visible while collapsed");
    QVERIFY2(!s.ribbon.collapsedLabel().trimmed().isEmpty(),
             "collapsed label must be non-empty (active task stays visible)");
    QVERIFY2(s.ribbon.collapsedLabel().contains(s.ribbon.activeTabName()),
             qPrintable(QStringLiteral("collapsed label '%1' must contain the active tab '%2'")
                            .arg(s.ribbon.collapsedLabel(), s.ribbon.activeTabName())));
}

void TestRibbonCollapse::raiseTabWorksWhileCollapsed()
{
    ShownRibbon s;
    s.ribbon.setCollapsed(true);

    s.ribbon.raiseTab(QStringLiteral("Protect"));

    QCOMPARE(s.ribbon.activeTabName(), QStringLiteral("Protect"));
    QVERIFY(!s.body()->isVisible());   // raising a tab while collapsed must not force-expand
    QVERIFY(s.ribbon.isCollapsed());

    // Unknown tab names are ignored.
    const QString before = s.ribbon.activeTabName();
    s.ribbon.raiseTab(QStringLiteral("NoSuchTab"));
    QCOMPARE(s.ribbon.activeTabName(), before);
}

void TestRibbonCollapse::chevronToggles()
{
    ShownRibbon s;
    QToolButton* btn = chevron(s.ribbon);
    QVERIFY(btn != nullptr);
    QVERIFY(btn->isVisible());

    btn->click();
    QVERIFY(s.ribbon.isCollapsed());
    btn->click();
    QVERIFY(!s.ribbon.isCollapsed());
}

void TestRibbonCollapse::ctrlF1Toggles()
{
    ShownRibbon s;
    s.ribbon.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&s.ribbon));

    QTest::keyClick(&s.ribbon, Qt::Key_F1, Qt::ControlModifier);
    QVERIFY(s.ribbon.isCollapsed());

    QTest::keyClick(&s.ribbon, Qt::Key_F1, Qt::ControlModifier);
    QVERIFY(!s.ribbon.isCollapsed());
}

void TestRibbonCollapse::doubleClickOnTabsToggles()
{
    ShownRibbon s;
    QTabBar* tabs = s.tabs();
    QVERIFY(tabs != nullptr);

    QTest::mouseDClick(tabs, Qt::LeftButton, Qt::NoModifier, tabs->rect().center());
    QVERIFY(s.ribbon.isCollapsed());

    QTest::mouseDClick(tabs, Qt::LeftButton, Qt::NoModifier, tabs->rect().center());
    QVERIFY(!s.ribbon.isCollapsed());
}

void TestRibbonCollapse::clickingATabWhileCollapsedExpands()
{
    ShownRibbon s;
    s.ribbon.setCollapsed(true);

    // v1 simplification of the Fluent pattern: selecting a tab expands the
    // ribbon and stays expanded (documented in the U02 ledger). This is a
    // real user click on the tab — unlike the TaskStateSync raiseTab()
    // channel, which must never change the expansion state.
    QTabBar* tabs = s.tabs();
    int organize = -1;
    for (int i = 0; i < RibbonModel::tabs().size(); ++i) {
        if (RibbonModel::tabs().at(i).name == QLatin1String("Organize")) {
            organize = i;
            break;
        }
    }
    QVERIFY(organize >= 0);
    QTest::mouseClick(tabs, Qt::LeftButton, Qt::NoModifier,
                      tabs->tabRect(organize).center());

    QVERIFY(!s.ribbon.isCollapsed());
    QVERIFY(s.body()->isVisible());
    QCOMPARE(s.ribbon.activeTabName(), QStringLiteral("Organize"));
}

QTEST_MAIN(TestRibbonCollapse)
#include "TestRibbonCollapse.moc"
