// SPDX-License-Identifier: Apache-2.0
// U02 — the slim status bar.
//
// What these tests pin (plan U02 + research Q-U02 Module 5 / Finding U02-5):
//   1. The default bar shows only useful document state: page n of m, zoom,
//      saved/unsaved, plus the transient operation channel. The mode/screen/
//      tool/selection echo cells and the always-on debug values (PDF version,
//      page dimensions, doc info, OCR language) are GONE from the bar.
//   2. The "page 1 / 000" empty state is dead: with no document the total
//      reads "/ —" and the page spin box is disabled.
//   3. Operations go through the QStatusBar showMessage contract:
//      setOperation() is a message with no invented timeout, clearOperation()
//      clears it.
//   4. PDF version / dimensions / selection / OCR language / current task /
//      current tool live in a details affordance fed by a DocumentFacts
//      struct — the popup content is exactly detailsText().
//   5. PDF version parsing upgrades from "PDF --" only when a real %PDF-
//      header exists.
#include <QtTest>
#include <QLabel>
#include <QMenu>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QToolButton>

#include "shell/StatusBar.h"

using namespace gp;

namespace {
// The bar carries exactly: page spin box, page total, zoom, unsaved, details
// button (the popup label lives inside the button's menu).
class TestBar : public gp::StatusBar {
public:
    using gp::StatusBar::StatusBar;
};

QLabel* findLabel(const QWidget* w, const QString& objectName) {
    return w->findChild<QLabel*>(objectName);
}
} // namespace

class TestStatusBarSlim : public QObject {
    Q_OBJECT

private slots:
    void emptyStateHasNoPage000();
    void emptyStateDisablesPageJump();
    void permanentSetIsTheSlimSet();
    void noEchoCellsInBar();
    void operationChannelUsesShowMessage();
    void parsePdfVersionRealAndMissing();
    void factsReflectNavAndDocument();
    void detailsTextCarriesTheFacts();
    void detailsPopupShowsDetailsText();
};

void TestStatusBarSlim::emptyStateHasNoPage000()
{
    TestBar bar;

    for (QLabel* l : bar.findChildren<QLabel*>())
        QVERIFY2(!l->text().contains(QStringLiteral("/ 000")),
                 qPrintable(QStringLiteral("label still shows / 000: %1").arg(l->text())));

    QLabel* total = findLabel(&bar, QStringLiteral("statusPageTotal"));
    QVERIFY(total != nullptr);
    QCOMPARE(total->text(), QStringLiteral("/ \u2014"));   // "/ —" (em dash)
}

void TestStatusBarSlim::emptyStateDisablesPageJump()
{
    TestBar bar;
    auto* spin = bar.findChild<QSpinBox*>();
    QVERIFY(spin != nullptr);
    QVERIFY2(!spin->isEnabled(), "page spin box must be disabled with no document");

    // A real page report enables it again (the "document-specific controls
    // with no document open" rule).
    bar.setPage(3, 12);
    QVERIFY(spin->isEnabled());
    QCOMPARE(spin->value(), 3);
    QCOMPARE(findLabel(&bar, QStringLiteral("statusPageTotal"))->text(),
             QStringLiteral("/ 12"));

    // Losing the document disables it again.
    bar.updateFromDocument(nullptr, QString());
    QVERIFY(!spin->isEnabled());
    QCOMPARE(findLabel(&bar, QStringLiteral("statusPageTotal"))->text(),
             QStringLiteral("/ \u2014"));
}

void TestStatusBarSlim::permanentSetIsTheSlimSet()
{
    TestBar bar;
    // Exactly: one page spin box, one details button, and the three kept
    // cells (page total, zoom, unsaved). The popup label is a child of the
    // details menu, so it is filtered out by name here.
    QCOMPARE(bar.findChildren<QSpinBox*>().size(), 1);
    QCOMPARE(bar.findChildren<QToolButton*>().size(), 1);
    QCOMPARE(bar.findChild<QToolButton*>()->objectName(), QStringLiteral("statusDetails"));

    QStringList labelNames;
    for (QLabel* l : bar.findChildren<QLabel*>())
        labelNames << l->objectName();
    labelNames.removeAll(QStringLiteral("statusDetailsLabel"));
    labelNames.sort();
    QCOMPARE(labelNames.join(QLatin1Char(',')),
             QStringLiteral("statusPageTotal,statusUnsaved,statusZoom"));
}

void TestStatusBarSlim::noEchoCellsInBar()
{
    TestBar bar;
    // No cell may echo navigation state the ScreenNav/ModeStrip already show,
    // and no permanent debug value may be displayed by default.
    const QStringList bannedPrefixes = {
        QStringLiteral("MODE "), QStringLiteral("SCREEN "), QStringLiteral("TOOL "),
        QStringLiteral("SEL "),  QStringLiteral("PDF "),   QStringLiteral("OCR \u00B7"),
    };
    for (QLabel* l : bar.findChildren<QLabel*>()) {
        for (const QString& prefix : bannedPrefixes)
            QVERIFY2(!l->text().startsWith(prefix),
                     qPrintable(QStringLiteral("bar echoes state: '%1'").arg(l->text())));
    }
}

void TestStatusBarSlim::operationChannelUsesShowMessage()
{
    TestBar bar;
    QVERIFY(bar.currentMessage().isEmpty());

    bar.setOperation(QStringLiteral("Saving page 2\u2026"));
    QCOMPARE(bar.currentMessage(), QStringLiteral("Saving page 2\u2026"));

    bar.clearOperation();
    QVERIFY(bar.currentMessage().isEmpty());
}

void TestStatusBarSlim::parsePdfVersionRealAndMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("fake.pdf"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("%PDF-1.7\n%\xE2\xE3\xCF\xD3\n");
    f.close();

    QCOMPARE(StatusBar::parsePdfVersion(path), QStringLiteral("PDF 1.7"));
    QCOMPARE(StatusBar::parsePdfVersion(dir.filePath(QStringLiteral("missing.pdf"))),
             QStringLiteral("PDF --"));

    // A file that does not start with %PDF- never upgrades the placeholder.
    const QString notPdf = dir.filePath(QStringLiteral("not.pdf"));
    QFile n(notPdf);
    QVERIFY(n.open(QIODevice::WriteOnly));
    n.write("PK\x03\x04NOTAPDF");
    n.close();
    QCOMPARE(StatusBar::parsePdfVersion(notPdf), QStringLiteral("PDF --"));
}

void TestStatusBarSlim::factsReflectNavAndDocument()
{
    TestBar bar;
    StatusBar::DocumentFacts facts = bar.currentFacts();
    QCOMPARE(facts.currentTask, QStringLiteral("Standard"));
    QCOMPARE(facts.pdfVersion, QStringLiteral("PDF --"));
    QCOMPARE(facts.pageSize, QStringLiteral("--\u00D7--"));   // "--×--"
    QCOMPARE(facts.selection, QStringLiteral("\u2014"));  // em dash
    QVERIFY(!facts.ocrLanguage.isEmpty());

    bar.setScreen(QStringLiteral("ocr"));
    bar.setTool(QStringLiteral("search"));
    bar.setSelection(QStringLiteral("3 words"));

    facts = bar.currentFacts();
    QCOMPARE(facts.currentTask, QStringLiteral("OCR Verify"));
    QCOMPARE(facts.tool, QStringLiteral("search"));
    QCOMPARE(facts.selection, QStringLiteral("3 words"));

    bar.setSelection(QString());
    QCOMPARE(bar.currentFacts().selection, QStringLiteral("\u2014"));
}

void TestStatusBarSlim::detailsTextCarriesTheFacts()
{
    TestBar bar;
    bar.setScreen(QStringLiteral("ocr"));
    bar.setTool(QStringLiteral("ocr"));

    const QString text = bar.detailsText();
    QVERIFY2(text.contains(QStringLiteral("OCR Verify")),
             "details must carry the current task title");
    QVERIFY2(text.contains(QStringLiteral("PDF --")),
             "details must carry the PDF version placeholder before a document loads");
    QVERIFY2(text.contains(QStringLiteral("--\u00D7--")),
             "details must carry the page-size placeholder before a document loads");
}

void TestStatusBarSlim::detailsPopupShowsDetailsText()
{
    TestBar bar;
    bar.setScreen(QStringLiteral("redact"));
    bar.show();   // visibility assertions need a shown widget

    bar.showDetailsPopup();
    QLabel* popupLabel = bar.findChild<QLabel*>(QStringLiteral("statusDetailsLabel"));
    QVERIFY(popupLabel != nullptr);
    QCOMPARE(popupLabel->text(), bar.detailsText());
    QVERIFY2(popupLabel->text().contains(QStringLiteral("Redaction")),
             "popup must show the current task");

    // The affordance itself is a real, visible control of the bar.
    auto* btn = bar.findChild<QToolButton*>(QStringLiteral("statusDetails"));
    QVERIFY(btn != nullptr);
    QVERIFY(btn->isVisible());

    if (auto* menu = bar.findChild<QMenu*>())
        menu->close();
}

QTEST_MAIN(TestStatusBarSlim)
#include "TestStatusBarSlim.moc"
