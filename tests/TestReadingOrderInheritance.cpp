// SPDX-License-Identifier: Apache-2.0
// Audit 9.14 P0 regression test: reading-order analysis must honor ISO
// 32000-2 §14.7.2 /Pg inheritance. A correctly-tagged PDF whose struct elems
// rely on inherited /Pg must NOT be flagged as out-of-order.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <podofo/podofo.h>
#include "modes/PdfAValidationPanel.h"

class TestReadingOrderInheritance : public QObject {
    Q_OBJECT
private slots:
    void untaggedPdfIsReported();
    void inheritedPgIsNotFlagged();
    void taggedFixtureLoadsInPodofo();
    void issuePagesParallelArray();
private:
    static QString writeTaggedPdf(const QString& dir, const QString& name);
    static QString writePlainPdf(const QString& dir, const QString& name);
};
QString TestReadingOrderInheritance::writePlainPdf(const QString& dir, const QString& name) {
    const QString path = dir + "/" + name;
    QFile f(path);
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
    return path;
}
QString TestReadingOrderInheritance::writeTaggedPdf(const QString& dir, const QString& name) {
    const QString path = dir + "/" + name;
    // Build the tagged document WITH PODOFO so the output is guaranteed
    // well-formed (hand-computed xref proved brittle).
    QTemporaryDir base;
    const QString seed = writePlainPdf(base.path(), "seed.pdf");
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
        // 11 elements: all /S /P; element index 1 (second) has NO /Pg (inherits).
        for (int i = 0; i < 11; ++i) {
            auto& elObj = doc.GetObjects().CreateDictionaryObject();
            elObj.GetDictionary().AddKey("S", PoDoFo::PdfObject(PoDoFo::PdfName("P")));
            if (i != 1)
                elObj.GetDictionary().AddKey("Pg", doc.GetPages().GetPageAt(0).GetObject());
            kids.Add(elObj.GetIndirectReference());
        }
        rootDict.AddKey("K", PoDoFo::PdfObject(kids));
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (...) {
        return {};
    }
}
void TestReadingOrderInheritance::inheritedPgIsNotFlagged() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = writeTaggedPdf(tmp.path(), "tagged_inherit.pdf");
    QVERIFY(!pdf.isEmpty());
    gp::ReadingOrderResult r;
    bool threw = false;
    try {
        r = gp::analyzeReadingOrder(pdf);
    } catch (...) {
        threw = true;
    }
    QFile d(QStringLiteral("ro_main.txt"));
    if (d.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&d);
        ts << "threw=" << threw << " tagged=" << r.tagged
           << " count=" << r.elementCount << " issues=" << r.issues.size() << "\n";
        for (const QString& i : r.issues) ts << "ISSUE: " << i << "\n";
    }
    QVERIFY(!threw);
    QVERIFY2(r.tagged, "fixture must load as a tagged PDF");
    QCOMPARE(r.elementCount, 11);
    QVERIFY2(r.issues.isEmpty(), qPrintable(QStringLiteral(
        "correctly-ordered elems with inherited /Pg must not be flagged; got: %1")
        .arg(r.issues.join("; "))));
}

void TestReadingOrderInheritance::untaggedPdfIsReported() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = writePlainPdf(tmp.path(), "plain.pdf");
    const gp::ReadingOrderResult r = gp::analyzeReadingOrder(pdf);
    QVERIFY(!r.tagged);
}
void TestReadingOrderInheritance::taggedFixtureLoadsInPodofo() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = writeTaggedPdf(tmp.path(), "probe_tagged.pdf");
    QVERIFY(!pdf.isEmpty());
    // Dump fixture + analysis for offline inspection (build/ is inside the repo).
    QFile::copy(pdf, QStringLiteral("tagged_probe_dump.pdf"));
    const gp::ReadingOrderResult rr = gp::analyzeReadingOrder(pdf);
    QFile diag(QStringLiteral("ro_diag.txt"));
    if (diag.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&diag);
        ts << "tagged=" << rr.tagged << " count=" << rr.elementCount << "\n";
        for (const QString& i : rr.issues) ts << "ISSUE: " << i << "\n";
    }
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        const PoDoFo::PdfObject* root =
            doc.GetCatalog().GetDictionary().FindKey("StructTreeRoot");
        QFile diag2(QStringLiteral("ro_diag2.txt"));
        if (diag2.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&diag2);
            ts << "loadOk root=" << (root != nullptr) << "\n";
        }
        QVERIFY2(root != nullptr, "catalog must expose /StructTreeRoot");
    } catch (const PoDoFo::PdfError& e) {
        QFile diag3(QStringLiteral("ro_diag3.txt"));
        if (diag3.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream ts(&diag3);
            ts << "EXC: " << e.what() << "\n";
        }
        QFAIL(qPrintable(QStringLiteral("PoDoFo failed to load tagged fixture: %1")
                         .arg(QString::fromLatin1(e.what()))));
    }
}

void TestReadingOrderInheritance::issuePagesParallelArray() {
    // issuePages must stay parallel to issues (§9.14 P0 jump-to-page wiring).
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = writeTaggedPdf(tmp.path(), "tagged_pages.pdf");
    QVERIFY(!pdf.isEmpty());
    const gp::ReadingOrderResult r = gp::analyzeReadingOrder(pdf);
    QCOMPARE(r.issuePages.size(), r.issues.size());
    for (int p : r.issuePages)
        QVERIFY(p >= -1); // 0-based page or unknown(-1); never garbage
}

QTEST_MAIN(TestReadingOrderInheritance)
#include "TestReadingOrderInheritance.moc"
