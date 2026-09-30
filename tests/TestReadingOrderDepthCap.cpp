// SPDX-License-Identifier: Apache-2.0
// PARITY-SCORECARD-2026-09-30 §4 #19: the struct-tree walk's depth cap
// (formerly the bare literal `depth > 60` in PdfAValidationPanel.cpp) must be
// a NAMED, surfaced constant whose truncation is DISCLOSED — a structure tree
// deeper than the cap stops the walk silently today, so a 200-deep tree
// reports a plain "clean" verdict while most of it was never analyzed.
//
// Pins:
//   * the cap is the named constant gp::kReadingOrderMaxStructDepth (60);
//   * a 200-deep struct tree reports depthTruncated honestly at the default;
//   * raising the override (a clamped settings value, range 60..500) makes
//     the analysis actually run deeper and find the inversion the truncated
//     walk could not see.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <podofo/podofo.h>

#include "modes/PdfAValidationPanel.h"

namespace {

using PoDoFo::PdfName;
using PoDoFo::PdfObject;

// A single-page PDF whose structure tree is a CHAIN of nested /P elements
// (root → e1 → … → e196) ending in four leaf elements whose /A /BBox top
// edges put leafD visually FIRST (visual order D,A,B,C vs structural
// A,B,C,D — displacement 3 > kReadingOrderSlotTolerance). With the default
// cap the walk never reaches the leaves; with a raised override it does.
//
// Fixture shape mirrors TestReadingOrderThreshold: the chain elements carry
// no /BBox, so they keep structural order via the struct-index nudge and the
// leaves (which carry /BBox) sort after them — the leaves' relative
// displacement is what the two runs must differ on.
bool makeDeepTaggedPdf(const QString& path, int chainLen) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

        auto& cat = doc.GetCatalog().GetDictionary();
        PoDoFo::PdfDictionary markInfo;
        markInfo.AddKey("Marked", PdfObject(true));
        cat.AddKey("MarkInfo", PdfObject(markInfo));

        // Build deepest-first so each chain element can reference its child.
        // Leaves: A=100, B=90, C=80, D=300 (visual order D,A,B,C).
        const double leafTops[4] = { 100.0, 90.0, 80.0, 300.0 };
        PoDoFo::PdfArray leafKids;
        for (const double topY : leafTops) {
            auto& leaf = doc.GetObjects().CreateDictionaryObject();
            leaf.GetDictionary().AddKey("Type", PdfObject(PdfName("StructElem")));
            leaf.GetDictionary().AddKey("S", PdfObject(PdfName("P")));
            leaf.GetDictionary().AddKey("Pg",
                PdfObject(page.GetObject().GetIndirectReference()));
            PoDoFo::PdfArray bbox;
            bbox.Add(PdfObject(0.0));
            bbox.Add(PdfObject(topY - 12.0));
            bbox.Add(PdfObject(500.0));
            bbox.Add(PdfObject(topY));
            PoDoFo::PdfDictionary layout;   // /A is a layout dict holding /BBox
            layout.AddKey("BBox", PdfObject(bbox));
            leaf.GetDictionary().AddKey("A", PdfObject(layout));
            leafKids.Add(leaf.GetIndirectReference());
        }

        // Deepest chain element wraps the leaves; each parent wraps the child
        // (PdfObject is non-copyable, so the chain threads /K by reference).
        auto& deepest = doc.GetObjects().CreateDictionaryObject();
        deepest.GetDictionary().AddKey("Type", PdfObject(PdfName("StructElem")));
        deepest.GetDictionary().AddKey("S", PdfObject(PdfName("P")));
        deepest.GetDictionary().AddKey("Pg",
            PdfObject(page.GetObject().GetIndirectReference()));
        deepest.GetDictionary().AddKey("K", PdfObject(leafKids));
        PoDoFo::PdfReference childRef = deepest.GetIndirectReference();
        for (int i = 1; i < chainLen; ++i) {
            auto& el = doc.GetObjects().CreateDictionaryObject();
            el.GetDictionary().AddKey("Type", PdfObject(PdfName("StructElem")));
            el.GetDictionary().AddKey("S", PdfObject(PdfName("P")));
            el.GetDictionary().AddKey("Pg",
                PdfObject(page.GetObject().GetIndirectReference()));
            PoDoFo::PdfArray one;
            one.Add(PdfObject(childRef));
            el.GetDictionary().AddKey("K", PdfObject(one));
            childRef = el.GetIndirectReference();
        }

        auto& root = doc.GetObjects().CreateDictionaryObject();
        root.GetDictionary().AddKey("Type", PdfObject(PdfName("StructTreeRoot")));
        root.GetDictionary().AddKey("K", PdfObject(childRef));
        cat.AddKey("StructTreeRoot", root.GetIndirectReference());

        doc.Save(path.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning() << "makeDeepTaggedPdf failed:" << e.what();
        return false;
    } catch (...) {
        return false;
    }
}

} // namespace

class TestReadingOrderDepthCap : public QObject {
    Q_OBJECT
private slots:
    // The cap must stay a NAMED constant so it can never silently drift
    // (same discipline as kReadingOrderSlotTolerance).
    void depthCapIsNamedConstant() {
        QCOMPARE(gp::kReadingOrderMaxStructDepth, 60);
        // The settings override is clamped into a reasonable range whose
        // lower bound is the fail-safe default.
        QCOMPARE(gp::kReadingOrderStructDepthLimit, 500);
    }

    // A 200-deep struct tree is TRUNCATED at the default cap — the result
    // must say so (depthTruncated + the cap in effect) instead of silently
    // stopping, and the unseen inversion must not be claimed as analyzed.
    void deepTreeReportsTruncationAtDefaultCap() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdf = tmp.filePath("deep200.pdf");
        QVERIFY2(makeDeepTaggedPdf(pdf, 196), "fixture build failed");

        const gp::ReadingOrderResult r = gp::analyzeReadingOrder(pdf);
        QVERIFY2(r.tagged, "fixture must parse as a tagged PDF");
        QVERIFY2(r.depthTruncated,
                 "a 200-deep tree must disclose truncation at the default cap");
        QCOMPARE(r.depthLimit, gp::kReadingOrderMaxStructDepth);
    }

    // With the override raised, deeper analysis RUNS: the walk reaches the
    // leaves and reports the inversion the truncated run could not see.
    void raisedOverrideAnalyzesDeeper() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdf = tmp.filePath("deep200_override.pdf");
        QVERIFY2(makeDeepTaggedPdf(pdf, 196), "fixture build failed");

        const gp::ReadingOrderResult r =
            gp::analyzeReadingOrder(pdf, gp::kReadingOrderStructDepthLimit);
        QVERIFY2(!r.depthTruncated,
                 "a 200-deep tree fits entirely under the 500 override");
        QVERIFY2(!r.issues.isEmpty(),
                 "the deep inversion (leafD displaced 3 slots) must be found "
                 "once the cap is raised");
        const QString joined = r.issues.join(QStringLiteral("; "));
        QVERIFY2(joined.contains(QStringLiteral("structure position 200")),
                 qPrintable(QStringLiteral("issue must name the deep leaf: %1")
                               .arg(joined)));
    }

    // A shallow tree must NOT be flagged truncated (the disclosure is honest
    // in both directions).
    void shallowTreeIsNotTruncated() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdf = tmp.filePath("shallow.pdf");
        QVERIFY2(makeDeepTaggedPdf(pdf, 3), "fixture build failed");

        const gp::ReadingOrderResult r = gp::analyzeReadingOrder(pdf);
        QVERIFY2(!r.depthTruncated, "a shallow tree is not truncated");
    }

    // Panel-level honesty pin: the 200-deep tree's truncation warning is
    // SURFACED (a non-error row in the issues list), and the panel never
    // answers a truncated analysis with the "Reading order: OK" dialog.
    void panelSurfacesTruncationWarning() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString pdf = tmp.filePath("deep200_panel.pdf");
        QVERIFY2(makeDeepTaggedPdf(pdf, 196), "fixture build failed");

        gp::PdfAValidationPanel panel;
        panel.setDocument(pdf);

        // Let the sibling veraPDF validation settle FIRST: the issues list is
        // shared between the two features and a validation completion clears
        // its rows — pin the warning only from a quiescent panel.
        QLabel* status = panel.findChild<QLabel*>(QStringLiteral("pdfaStatusLabel"));
        QVERIFY(status != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(
            !status->text().contains(QStringLiteral("Validating")), 15000);

        QPushButton* roBtn = nullptr;
        const QList<QPushButton*> buttons = panel.findChildren<QPushButton*>();
        for (auto* b : buttons) {
            if (b->text().contains(QStringLiteral("Check Reading Order"),
                                    Qt::CaseInsensitive)) {
                roBtn = b;
                break;
            }
        }
        QVERIFY2(roBtn, "panel must expose the Check Reading Order button");

        // A truncated analysis must NEVER take the "Reading order: OK" path —
        // if a regression routes it there, the modal is captured and failed on
        // instead of blocking the test.
        bool modalShown = false;
        QTimer closer;
        closer.setInterval(25);
        QObject::connect(&closer, &QTimer::timeout, [&modalShown]() {
            QWidget* m = QApplication::activeModalWidget();
            if (m) { modalShown = true; m->close(); }
        });
        closer.start();

        roBtn->click();
        int waited = 0;
        while (!roBtn->isEnabled() && waited < 10000) {
            QTest::qWait(50);
            waited += 50;
        }
        QVERIFY2(roBtn->isEnabled(),
                 "reading-order worker must finish and re-enable the button");
        closer.stop();
        QVERIFY2(!modalShown,
                 "a truncated analysis must not surface the OK dialog");

        // The warning row is visible with the cap named.
        bool warned = false;
        for (const QLabel* l : panel.findChildren<QLabel*>()) {
            if (l->text().contains(QStringLiteral("analysis truncated"))) {
                warned = true;
                break;
            }
        }
        QVERIFY2(warned,
                 "panel must surface the struct-tree truncation warning row");
    }
};

QTEST_MAIN(TestReadingOrderDepthCap)
#include "TestReadingOrderDepthCap.moc"
