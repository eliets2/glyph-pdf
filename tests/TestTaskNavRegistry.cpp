// SPDX-License-Identifier: Apache-2.0
// U02 — the TaskNav table is the ONE navigation registry (ids frozen).
//
// What these tests pin (plan U02 + research Q-U02 Module 1):
//   1. Every task id is unique; every task has a non-empty title and a kind.
//   2. Titles are human task names — the "01 OCR VERIFY" numbering is gone
//      (no title starts with a digit).
//   3. Workspace ids are exactly the screens ModeController swaps into the
//      center; Dialog ids are exactly the modal screens in
//      MainWindow::onScreenSelected; Panel ids are the right-dock swaps.
//   4. Every non-empty ribbonTab names a real RibbonModel tab; every
//      modePill names a real ModeStrip pill.
//   5. Every entryTool round-trips: TaskNav::screenForTool(entryTool) is the
//      task's own id — so activating the entry tool can only ever land on
//      its one task (the U02 acceptance rule).
//   6. screenForTool covers exactly the documented set (OCR entry, redaction
//      marking/operations, compare, compress, watermark, form-field tools)
//      and returns "" for every other tool — zero behavior change for them.
#include <QtTest>
#include <QSet>

#include "shell/TaskNav.h"
#include "shell/RibbonModel.h"
#include "core/ToolId.h"

using namespace gp;

namespace {
// ModeController ctor registers exactly these swap-in screens
// (src/modes/ModeController.cpp:16-22).
const QSet<QString> kWorkspaceScreens = {
    QStringLiteral("ocr"), QStringLiteral("redact"), QStringLiteral("compare"),
    QStringLiteral("pages"), QStringLiteral("batch"), QStringLiteral("form"),
};
// Modal snap-back screens in MainWindow::onScreenSelected.
const QSet<QString> kDialogScreens = {
    QStringLiteral("compress"), QStringLiteral("watermark"),
};
// Right-dock panel swaps in MainWindow::onScreenSelected.
const QSet<QString> kPanelScreens = {
    QStringLiteral("signature"), QStringLiteral("pdfa"),
};
// ModeStrip pill ids (src/shell/ModeStrip.cpp:39-45).
const QSet<QString> kModePills = {
    QStringLiteral("view"), QStringLiteral("edit"), QStringLiteral("comment"),
    QStringLiteral("form"), QStringLiteral("protect"),
};
} // namespace

class TestTaskNavRegistry : public QObject {
    Q_OBJECT

private slots:
    void idsAreUnique();
    void everyTaskHasTitleAndKind();
    void noNumberedTitles();
    void standardTaskIsEmptyId();
    void workspaceIdsMatchModeController();
    void dialogIdsAreTheModalScreens();
    void panelIdsAreTheRightDockSwaps();
    void toggleIsAi();
    void ribbonTabsExist();
    void modePillsAreRealPills();
    void entryToolRoundTrips();
    void screenForToolDocumentedSet();
    void screenForToolLeavesOtherToolsAlone();
    void entryRouteIsExactlyTheDocumentedTools();
    void titleLookup();
};

void TestTaskNavRegistry::idsAreUnique()
{
    QSet<QString> seen;
    const auto tasks = TaskNav::tasks();
    for (const auto& t : tasks) {
        const QString id = QString::fromLatin1(t.id);
        QVERIFY2(!seen.contains(id), qPrintable(QStringLiteral("duplicate task id: %1").arg(id)));
        seen.insert(id);
    }
    QCOMPARE(seen.size(), tasks.size());
}

void TestTaskNavRegistry::everyTaskHasTitleAndKind()
{
    for (const auto& t : TaskNav::tasks()) {
        QVERIFY2(t.id != nullptr, "task id must not be null");
        QVERIFY2(t.title != nullptr, "task title must not be null");
        QVERIFY2(qstrlen(t.title) > 0,
                 qPrintable(QStringLiteral("task %1 has no title").arg(t.id)));
        QVERIFY2(t.kind == TaskKind::Standard || t.kind == TaskKind::Workspace ||
                 t.kind == TaskKind::Panel || t.kind == TaskKind::Dialog ||
                 t.kind == TaskKind::Toggle,
                 qPrintable(QStringLiteral("task %1 has an out-of-range kind").arg(t.id)));
        QVERIFY2(t.modePill != nullptr && t.ribbonTab != nullptr,
                 qPrintable(QStringLiteral("task %1 has null pill/tab").arg(t.id)));
    }
}

void TestTaskNavRegistry::noNumberedTitles()
{
    // Plan U02: replace numbered screen labels such as "01 OCR VERIFY" with
    // task names. The numbering must live nowhere in the titles.
    const QRegularExpression leadingDigits(QStringLiteral("^\\d"));
    for (const auto& t : TaskNav::tasks()) {
        const QString title = QString::fromLatin1(t.title);
        QVERIFY2(!title.contains(leadingDigits),
                 qPrintable(QStringLiteral("title still numbered: %1").arg(title)));
        QVERIFY2(!title.contains(QStringLiteral("  ")),
                 qPrintable(QStringLiteral("title uses column padding: %1").arg(title)));
    }
}

void TestTaskNavRegistry::standardTaskIsEmptyId()
{
    const TaskSpec* standard = TaskNav::forScreen(QString());
    QVERIFY(standard != nullptr);
    QCOMPARE(standard->kind, TaskKind::Standard);
    QCOMPARE(TaskNav::title(QString()), QStringLiteral("Standard"));
}

void TestTaskNavRegistry::workspaceIdsMatchModeController()
{
    QSet<QString> workspaceIds;
    for (const auto& t : TaskNav::tasks())
        if (t.kind == TaskKind::Workspace)
            workspaceIds.insert(QString::fromLatin1(t.id));

    QCOMPARE(workspaceIds, kWorkspaceScreens);
}

void TestTaskNavRegistry::dialogIdsAreTheModalScreens()
{
    QSet<QString> dialogIds;
    for (const auto& t : TaskNav::tasks())
        if (t.kind == TaskKind::Dialog)
            dialogIds.insert(QString::fromLatin1(t.id));

    QCOMPARE(dialogIds, kDialogScreens);
}

void TestTaskNavRegistry::panelIdsAreTheRightDockSwaps()
{
    QSet<QString> panelIds;
    for (const auto& t : TaskNav::tasks())
        if (t.kind == TaskKind::Panel)
            panelIds.insert(QString::fromLatin1(t.id));

    QCOMPARE(panelIds, kPanelScreens);
}

void TestTaskNavRegistry::toggleIsAi()
{
    for (const auto& t : TaskNav::tasks()) {
        if (t.kind == TaskKind::Toggle)
            QCOMPARE(QString::fromLatin1(t.id), QStringLiteral("ai"));
    }
}

void TestTaskNavRegistry::ribbonTabsExist()
{
    QStringList tabNames;
    for (const auto& def : RibbonModel::tabs())
        tabNames << def.name;

    for (const auto& t : TaskNav::tasks()) {
        const QString tab = QString::fromLatin1(t.ribbonTab);
        if (tab.isEmpty())
            continue;
        QVERIFY2(tabNames.contains(tab),
                 qPrintable(QStringLiteral("task %1 names unknown ribbon tab %2").arg(t.id, tab)));
    }
}

void TestTaskNavRegistry::modePillsAreRealPills()
{
    for (const auto& t : TaskNav::tasks()) {
        const QString pill = QString::fromLatin1(t.modePill);
        if (pill.isEmpty())
            continue;
        QVERIFY2(kModePills.contains(pill),
                 qPrintable(QStringLiteral("task %1 names unknown mode pill %2").arg(t.id, pill)));
    }
}

void TestTaskNavRegistry::entryToolRoundTrips()
{
    // The critical U02 invariant: activating a task's entry tool can only
    // ever land on that task's one screen.
    for (const auto& t : TaskNav::tasks()) {
        if (t.entryTool == ToolId::COUNT)
            continue;
        const QString landed = TaskNav::screenForTool(t.entryTool);
        QVERIFY2(landed == QString::fromLatin1(t.id),
                 qPrintable(QStringLiteral("entry tool %1 of task %2 lands on '%3'")
                                .arg(toolIdToString(t.entryTool), t.id, landed)));
    }
}

void TestTaskNavRegistry::screenForToolDocumentedSet()
{
    // OCR entry.
    QCOMPARE(TaskNav::screenForTool(ToolId::Ocr), QStringLiteral("ocr"));
    // Redaction: marking + the redaction operations.
    QCOMPARE(TaskNav::screenForTool(ToolId::MarkRedact), QStringLiteral("redact"));
    QCOMPARE(TaskNav::screenForTool(ToolId::Sanitize), QStringLiteral("redact"));
    QCOMPARE(TaskNav::screenForTool(ToolId::PatternRedact), QStringLiteral("redact"));
    QCOMPARE(TaskNav::screenForTool(ToolId::RegexRedact), QStringLiteral("redact"));
    // Compare / compress / watermark tasks.
    QCOMPARE(TaskNav::screenForTool(ToolId::Compare), QStringLiteral("compare"));
    QCOMPARE(TaskNav::screenForTool(ToolId::Compress), QStringLiteral("compress"));
    QCOMPARE(TaskNav::screenForTool(ToolId::Watermark), QStringLiteral("watermark"));
    // Form Builder: every FormsController-owned field tool.
    for (ToolId id : { ToolId::TextField, ToolId::Checkbox, ToolId::Radio,
                       ToolId::Dropdown, ToolId::CreateForm, ToolId::ListBox,
                       ToolId::Button, ToolId::CalcField, ToolId::DateField,
                       ToolId::NumField, ToolId::SigField, ToolId::AutoDetect,
                       ToolId::Tabs }) {
        QVERIFY2(TaskNav::screenForTool(id) == QStringLiteral("form"),
                 qPrintable(QStringLiteral("%1 should land on 'form'")
                                .arg(toolIdToString(id))));
    }
}

void TestTaskNavRegistry::screenForToolLeavesOtherToolsAlone()
{
    // Tools with no task screen return "" and cause no sync — zero behavior
    // change for the rest of the registry. Signature stays an annotation tool
    // (not the Signatures panel); PdfA is an export action, not the panel.
    for (ToolId id : { ToolId::Open, ToolId::Save, ToolId::SaveAs, ToolId::Undo,
                       ToolId::ZoomIn, ToolId::Highlight, ToolId::Note,
                       ToolId::Signature, ToolId::Extract, ToolId::ToWord,
                       ToolId::Encrypt, ToolId::Sign, ToolId::PdfA,
                       ToolId::DeletePage, ToolId::Reorder, ToolId::Search,
                       ToolId::Combine, ToolId::RotateCW, ToolId::Crop }) {
        QVERIFY2(TaskNav::screenForTool(id).isEmpty(),
                 qPrintable(QStringLiteral("%1 must NOT sync a screen")
                                .arg(toolIdToString(id))));
    }
}

void TestTaskNavRegistry::entryRouteIsExactlyTheDocumentedTools()
{
    // Entry-route tools ARE task entries: their activation navigates to the
    // task instead of also dispatching the raw controller action. This is the
    // verified-defect fix: the ribbon "ocr" button ran the pipeline while the
    // menu/screen-nav opened the OCR Verify screen — after U02 both open the
    // verify screen, and running a recognition happens inside it.
    QVERIFY(TaskNav::isEntryRoute(ToolId::Ocr));
    QVERIFY(TaskNav::isEntryRoute(ToolId::Compare));
    QVERIFY(TaskNav::isEntryRoute(ToolId::Compress));
    QVERIFY(TaskNav::isEntryRoute(ToolId::Watermark));

    // Action-within-task tools keep their controller behavior and only sync
    // visible state afterwards.
    QVERIFY(!TaskNav::isEntryRoute(ToolId::MarkRedact));
    QVERIFY(!TaskNav::isEntryRoute(ToolId::CreateForm));
    QVERIFY(!TaskNav::isEntryRoute(ToolId::Sanitize));
    QVERIFY(!TaskNav::isEntryRoute(ToolId::Open));
    QVERIFY(!TaskNav::isEntryRoute(ToolId::Highlight));
}

void TestTaskNavRegistry::titleLookup()
{
    QCOMPARE(TaskNav::title(QStringLiteral("ocr")), QStringLiteral("OCR Verify"));
    QCOMPARE(TaskNav::title(QStringLiteral("redact")), QStringLiteral("Redaction"));
    QCOMPARE(TaskNav::title(QStringLiteral("form")), QStringLiteral("Form Builder"));
    QCOMPARE(TaskNav::title(QString()), QStringLiteral("Standard"));
    QCOMPARE(TaskNav::title(QStringLiteral("no-such-screen")), QString());
    QVERIFY(TaskNav::isKnownScreen(QStringLiteral("ocr")));
    QVERIFY(TaskNav::isKnownScreen(QString()));
    QVERIFY(!TaskNav::isKnownScreen(QStringLiteral("no-such-screen")));
}

QTEST_MAIN(TestTaskNavRegistry)
#include "TestTaskNavRegistry.moc"
