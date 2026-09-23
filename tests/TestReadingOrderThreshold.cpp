// SPDX-License-Identifier: Apache-2.0
// §9.14 P1: the reading-order check's ">2 position slots" rule used to be a
// bare literal buried in PdfAValidationPanel.cpp. It is now the named,
// documented constant gp::kReadingOrderSlotTolerance — framed as a HEURISTIC
// (a triage aid per common PDF/UA practice; human review remains
// authoritative). These tests pin the boundary EXACTLY at the constant:
// displacement of 2 slots is NOT flagged, displacement of 3 slots IS.
//
// The seam under test is the pure function gp::analyzeReadingOrder() driven
// with hand-built tagged-PDF fixtures (PoDoFo-built StructTreeRoot whose
// elements carry /A /BBox layout attributes — the /BBox top edge determines
// visual order; structure order is the /K document order).
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>

#include "modes/PdfAValidationPanel.h"

namespace {

// Build a single-page tagged PDF whose struct elements (type /P) are in
// document order but carry the given /BBox top edges (PDF user space, y-up),
// so the visual order (top-of-page first) can be forced independently of the
// structural order.
bool makeTaggedPdf(const QString& path, const QList<double>& topYs) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

        auto& cat = doc.GetCatalog().GetDictionary();
        PoDoFo::PdfDictionary markInfo;
        markInfo.AddKey("Marked", PoDoFo::PdfObject(true));
        cat.AddKey("MarkInfo", PoDoFo::PdfObject(markInfo));

        auto& root = doc.GetObjects().CreateDictionaryObject();
        root.GetDictionary().AddKey("Type", PoDoFo::PdfObject(PoDoFo::PdfName("StructTreeRoot")));
        cat.AddKey("StructTreeRoot", root.GetIndirectReference());

        PoDoFo::PdfArray kids;
        for (const double topY : topYs) {
            auto& el = doc.GetObjects().CreateDictionaryObject();
            el.GetDictionary().AddKey("Type", PoDoFo::PdfObject(PoDoFo::PdfName("StructElem")));
            el.GetDictionary().AddKey("S", PoDoFo::PdfObject(PoDoFo::PdfName("P")));
            el.GetDictionary().AddKey("Pg",
                PoDoFo::PdfObject(page.GetObject().GetIndirectReference()));
            // Layout /A with /BBox [x1 y1 x2 y2]; the analyzer's topY is the
            // upper edge (max of y1/y2).
            PoDoFo::PdfDictionary layout;
            PoDoFo::PdfArray bbox;
            bbox.Add(PoDoFo::PdfObject(0.0));
            bbox.Add(PoDoFo::PdfObject(topY - 12.0));
            bbox.Add(PoDoFo::PdfObject(500.0));
            bbox.Add(PoDoFo::PdfObject(topY));
            layout.AddKey("BBox", PoDoFo::PdfObject(bbox));
            el.GetDictionary().AddKey("A", PoDoFo::PdfObject(layout));
            kids.Add(el.GetIndirectReference());
        }
        root.GetDictionary().AddKey("K", PoDoFo::PdfObject(kids));

        doc.Save(path.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "makeTaggedPdf failed:" << e.what();
        return false;
    } catch (...) {
        return false;
    }
}

} // namespace

class TestReadingOrderThreshold : public QObject {
    Q_OBJECT

private slots:
    // The rule must stay a NAMED constant — pinning both its value and its
    // existence so the heuristic can never silently drift.
    void toleranceConstantIsNamedAndEqualsTwo() {
        QCOMPARE(gp::kReadingOrderSlotTolerance, 2);
    }

    // Boundary below-or-at the tolerance: an element displaced by exactly
    // kReadingOrderSlotTolerance (2) slots is NOT flagged.
    //
    // Structure order A B C; visual order (top-first) C A B — C moves from
    // structure position 3 to visual position 1: |3-1| = 2 slots.
    void displacementOfExactlyTwoIsNotFlagged() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdf = tmp.filePath("disp2.pdf");
        // topY: A=100, B=90, C=300 → visual order C, A, B
        QVERIFY2(makeTaggedPdf(pdf, { 100.0, 90.0, 300.0 }),
                 "fixture build failed");

        const gp::ReadingOrderResult r = gp::analyzeReadingOrder(pdf);
        QVERIFY2(r.tagged, "fixture must parse as a tagged PDF");
        QCOMPARE(r.elementCount, 3);
        QVERIFY2(r.issues.isEmpty(),
                 qPrintable(QStringLiteral(
                     "displacement of exactly %1 slots must NOT be flagged; got: %2")
                     .arg(gp::kReadingOrderSlotTolerance)
                     .arg(r.issues.join(QStringLiteral("; ")))));
    }

    // Boundary above the tolerance: an element displaced by exactly
    // kReadingOrderSlotTolerance + 1 (3) slots IS flagged — with an issue
    // that names the type, the structure position, the visual position and
    // the page (so a human reviewer can jump straight to it).
    //
    // Structure order A B C D; visual order D A B C — D moves from structure
    // position 4 to visual position 1: |4-1| = 3 slots.
    void displacementOfExactlyThreeIsFlagged() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdf = tmp.filePath("disp3.pdf");
        // topY: A=100, B=90, C=80, D=300 → visual order D, A, B, C
        QVERIFY2(makeTaggedPdf(pdf, { 100.0, 90.0, 80.0, 300.0 }),
                 "fixture build failed");

        const gp::ReadingOrderResult r = gp::analyzeReadingOrder(pdf);
        QVERIFY2(r.tagged, "fixture must parse as a tagged PDF");
        QCOMPARE(r.elementCount, 4);
        QCOMPARE(r.issues.size(), 1);
        QCOMPARE(r.issuePages, QList<int>{ 0 });
        // The issue names element type, structure position (1-based), visual
        // position (1-based) and the page.
        if (r.issues.size() == 1) {
            const QString& issue = r.issues.first();
            QVERIFY2(issue.contains(QStringLiteral("P")),
                     qPrintable(QStringLiteral("issue must name the element type: %1").arg(issue)));
            QVERIFY2(issue.contains(QStringLiteral("structure position 4")),
                     qPrintable(QStringLiteral("issue must name structure position 4: %1").arg(issue)));
            QVERIFY2(issue.contains(QStringLiteral("visual position 1")),
                     qPrintable(QStringLiteral("issue must name visual position 1: %1").arg(issue)));
            QVERIFY2(issue.contains(QStringLiteral("page 1")),
                     qPrintable(QStringLiteral("issue must name the page: %1").arg(issue)));
        }
    }
};

QTEST_MAIN(TestReadingOrderThreshold)
#include "TestReadingOrderThreshold.moc"
