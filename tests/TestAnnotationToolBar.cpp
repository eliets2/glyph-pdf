// SPDX-License-Identifier: Apache-2.0
// Audit 9.3 P0 regression test: ONE authoritative markup surface must expose
// ALL implemented markup tools. The live surface is the ribbon's Comment tab
// (the old floating AnnotationToolBar is not compiled into the app); this
// pins that Strikeout/Squiggly/Stamp/Callout are discoverable there.
#include <QtTest/QtTest>
#include "shell/RibbonModel.h"
#include "core/PdfEnums.h"

class TestAnnotationToolBar : public QObject {
    Q_OBJECT
private slots:
    void allMarkupToolsPresent();
};

void TestAnnotationToolBar::allMarkupToolsPresent() {
    // Collect every tool id on the ribbon's Comment tab.
    QStringList commentToolIds;
    for (const auto& tab : gp::RibbonModel::tabs()) {
        if (tab.name != QLatin1String("Comment")) continue;
        for (const auto& grp : tab.groups)
            for (const auto& tool : grp.tools)
                commentToolIds << tool.id;
    }
    QVERIFY2(!commentToolIds.isEmpty(), "Comment tab must exist");

    // All 10 implemented markup types must be discoverable there.
    const QStringList required = {
        "highlight", "underline", "strike", "squiggly",
        "note", "textbox", "callout", "stamp",
        "pencil", "line", "arrow", "rect", "oval"
    };
    for (const QString& id : required)
        QVERIFY2(commentToolIds.contains(id),
                 qPrintable(QStringLiteral("authoritative markup surface missing '%1'").arg(id)));
}
QTEST_MAIN(TestAnnotationToolBar)
#include "TestAnnotationToolBar.moc"
