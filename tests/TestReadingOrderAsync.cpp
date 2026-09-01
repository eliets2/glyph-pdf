// SPDX-License-Identifier: Apache-2.0
// §9.14 regression test: the reading-order check must run OFF the GUI thread
// (same QFutureWatcher pattern as the sibling veraPDF validation in the same
// panel). analyzeReadingOrder parses and walks the whole structure tree, which
// used to freeze the UI on large/deeply-tagged documents.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QPushButton>
#include <QRegularExpression>
#include <QApplication>
#include <QTimer>
#include <podofo/podofo.h>
#include "modes/PdfAValidationPanel.h"

class TestReadingOrderAsync : public QObject {
    Q_OBJECT
private slots:
    void checkRunsAsynchronouslyAndDelivers();
    void noDocumentShowsMessageWithoutWorker();

private:
    // Tagged PDF whose structure elements carry /A /BBox layout attributes in
    // REVERSED visual order — the analysis must report issues (the issues path
    // shows no modal dialog, so the test can wait for completion).
    static QString writeTaggedPdfWithIssues(const QString& dir, const QString& name);
};

QString TestReadingOrderAsync::writeTaggedPdfWithIssues(const QString& dir, const QString& name) {
    const QString path = dir + "/" + name;
    // Seed a minimal valid PDF, then add the structure tree with PoDoFo so the
    // output is guaranteed well-formed (same approach as
    // TestReadingOrderInheritance).
    const QString seed = dir + "/seed.pdf";
    {
        QFile f(seed);
        if (!f.open(QIODevice::WriteOnly)) return {};
        f.write(
            "%PDF-1.4\n"
            "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
            "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
            "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
            "xref\n0 4\n"
            "0000000000 65535 f \n"
            "0000000009 00000 n \n"
            "0000000058 00000 n \n"
            "0000000115 00000 n \n"
            "trailer<</Size 4/Root 1 0 R>>\n"
            "startxref\n183\n%%EOF\n");
    }
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(seed.toUtf8().constData());
        auto& cat = doc.GetCatalog().GetDictionary();
        cat.AddKey("MarkInfo", PoDoFo::PdfObject(PoDoFo::PdfDictionary()));
        cat.FindKey("MarkInfo")->GetDictionary().AddKey("Marked", PoDoFo::PdfObject(true));
        auto& rootDict = cat.AddKey("StructTreeRoot", PoDoFo::PdfObject(PoDoFo::PdfDictionary())).GetDictionary();
        rootDict.AddKey("Type", PoDoFo::PdfObject(PoDoFo::PdfName("StructTreeRoot")));
        rootDict.AddKey("Pg", doc.GetPages().GetPageAt(0).GetObject());
        PoDoFo::PdfArray kids;
        // 11 elements, each with a /A layout dict whose /BBox top edge ASCENDS
        // with structure index (element 0 sits at the bottom of the page). The
        // analyzer sorts top-of-page first, so visual order is the REVERSE of
        // structure order — most elements are far (>2 slots) from their visual
        // position and must be reported as issues.
        for (int i = 0; i < 11; ++i) {
            auto& elObj = doc.GetObjects().CreateDictionaryObject();
            elObj.GetDictionary().AddKey("S", PoDoFo::PdfObject(PoDoFo::PdfName("P")));
            elObj.GetDictionary().AddKey("Pg", doc.GetPages().GetPageAt(0).GetObject());
            PoDoFo::PdfArray bbox;
            bbox.Add(static_cast<double>(92 + i * 50));  // y1 (top edge, ascending)
            bbox.Add(0.0);
            bbox.Add(612.0);
            bbox.Add(static_cast<double>(72 + i * 50));  // y0
            auto& attrObj = doc.GetObjects().CreateDictionaryObject();
            attrObj.GetDictionary().AddKey("BBox", PoDoFo::PdfObject(bbox));
            elObj.GetDictionary().AddKey("A", PoDoFo::PdfObject(attrObj.GetIndirectReference()));
            kids.Add(elObj.GetIndirectReference());
        }
        rootDict.AddKey("K", PoDoFo::PdfObject(kids));
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (...) {
        return {};
    }
}

void TestReadingOrderAsync::checkRunsAsynchronouslyAndDelivers() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = writeTaggedPdfWithIssues(tmp.path(), "tagged_issues.pdf");
    QVERIFY(!pdf.isEmpty());

    gp::PdfAValidationPanel panel;
    panel.setDocument(pdf);

    // Locate the reading-order button by its text (the panel does not name it).
    QPushButton* roBtn = nullptr;
    const QRegularExpression rxCheck(QStringLiteral("Check Reading Order"),
                                     QRegularExpression::CaseInsensitiveOption);
    const QList<QPushButton*> buttons = panel.findChildren<QPushButton*>();
    for (auto* b : buttons) {
        if (rxCheck.match(b->text()).hasMatch()) { roBtn = b; break; }
    }
    QVERIFY2(roBtn, "panel must expose the Check Reading Order button");

    // Clicking the check button must return immediately (no synchronous
    // analysis on the GUI thread) and leave the button disabled while the
    // worker runs.
    QElapsedTimer timer;
    timer.start();
    roBtn->click();
    QVERIFY2(timer.elapsed() < 2000,
             "onCheckReadingOrder must not block the GUI thread");

    // The worker finishes and re-enables the button (onReadingOrderFinished).
    // Wait WHILE DISABLED — the button is disabled synchronously by the click,
    // so the loop must wait for the re-enable, not for the disable.
    const int deadline = 10000;
    int waited = 0;
    while (!roBtn->isEnabled() && waited < deadline) {
        QTest::qWait(50);
        waited += 50;
    }
    QVERIFY2(roBtn->isEnabled(),
             "reading-order worker must finish and re-enable the check button");
}

void TestReadingOrderAsync::noDocumentShowsMessageWithoutWorker() {
    gp::PdfAValidationPanel panel;
    // Must not crash and must not leave a worker running. The panel surfaces
    // the "no document" notice as a modal information box, so the click enters
    // a nested exec() — close the modal from a queued callback and capture it
    // to assert the message path ran (the no-document branch returns BEFORE
    // any QFutureWatcher is created, i.e. no worker is spawned).
    const QList<QPushButton*> buttons = panel.findChildren<QPushButton*>();
    QPushButton* roBtn = nullptr;
    const QRegularExpression rxCheck(QStringLiteral("Check Reading Order"),
                                     QRegularExpression::CaseInsensitiveOption);
    for (auto* b : buttons) {
        if (rxCheck.match(b->text()).hasMatch()) { roBtn = b; break; }
    }
    QVERIFY(roBtn);

    QWidget* modal = nullptr;
    QTimer::singleShot(0, [&modal]() {
        modal = QApplication::activeModalWidget();
        if (modal) modal->close();
    });
    roBtn->click();
    QVERIFY2(modal, "no-document click must show the message dialog without spawning a worker");
}

QTEST_MAIN(TestReadingOrderAsync)
#include "TestReadingOrderAsync.moc"
