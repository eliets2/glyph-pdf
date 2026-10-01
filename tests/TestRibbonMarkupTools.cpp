// SPDX-License-Identifier: Apache-2.0
// Audit 9.3 P0 regression test: ONE authoritative markup surface must expose
// ALL implemented markup tools. The live surface is the ribbon's Comment tab;
// this pins that Highlight/Underline/Strikeout/Squiggly/Stamp/Callout and the
// drawing tools are discoverable there.
//
// History: originally named TestAnnotationToolBar, from when the floating
// AnnotationToolBar class still existed. That class left the build in 6aac22c
// and was deleted outright on feat/ui-polish under the parity scorecard's
// sign-off; the test has always pinned the RibbonModel, never the dead class —
// renamed to say what it actually tests.
#include <QtTest/QtTest>
#include "shell/RibbonModel.h"
#include "core/PdfEnums.h"

class TestRibbonMarkupTools : public QObject {
    Q_OBJECT
private slots:
    void allMarkupToolsPresent();
};

void TestRibbonMarkupTools::allMarkupToolsPresent() {
    // Collect every tool id on the ribbon's Comment tab.
    QStringList commentToolIds;
    for (const auto& tab : gp::RibbonModel::tabs()) {
        if (tab.name != QLatin1String("Comment")) continue;
        for (const auto& grp : tab.groups)
            for (const auto& tool : grp.tools)
                commentToolIds << tool.id;
    }
    QVERIFY2(!commentToolIds.isEmpty(), "Comment tab must exist");

    // All implemented markup types must be discoverable there.
    const QStringList required = {
        "highlight", "underline", "strike", "squiggly",
        "note", "textbox", "callout", "stamp",
        "pencil", "line", "arrow", "rect", "oval"
    };
    for (const QString& id : required)
        QVERIFY2(commentToolIds.contains(id),
                 qPrintable(QStringLiteral("authoritative markup surface missing '%1'").arg(id)));
}
QTEST_MAIN(TestRibbonMarkupTools)
#include "TestRibbonMarkupTools.moc"
