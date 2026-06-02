// TestLayoutEnsemble — unit tests for M5-P2 D4 LayoutEnsemble IoU reconciliation.
//
// Tests:
//   1. computeIoU — zero when non-overlapping, correct value when overlapping.
//   2. Single-detector pass-through (all regions returned, reading-order assigned).
//   3. Two detectors agree on same region → single merged region (no duplication).
//   4. Two detectors disagree (no IoU overlap) → both regions kept when conf ≥ 0.3.
//   5. Low-confidence unmatched region from secondary suppressed when conf < threshold.
//   6. Type vote: two detectors disagree on type; higher-confidence wins.
//   7. Reading-order re-computed correctly (top-to-bottom, left-to-right).
//   8. Empty page → empty result.
//   9. No detectors → empty result.
//  10. Single-detector mode with null scheduler (no crash).

#include <QtTest>
#include <QImage>

#include "engines/ocr/ILayoutDetector.h"
#include "engines/ocr/LayoutEnsemble.h"

// ── Stub detector ────────────────────────────────────────────────────────────

/// A configurable stub ILayoutDetector for testing.
class StubDetector final : public ILayoutDetector {
public:
    explicit StubDetector(const QString &name, QList<LayoutRegion> regions)
        : m_name(name), m_regions(std::move(regions)) {}

    QList<LayoutRegion> detect(const QImage &, gp::Lane) override { return m_regions; }
    QString name() const override { return m_name; }

    void setRegions(const QList<LayoutRegion> &r) { m_regions = r; }

private:
    QString             m_name;
    QList<LayoutRegion> m_regions;
};

// ── Helper builders ──────────────────────────────────────────────────────────

static LayoutRegion makeRegion(double x, double y, double w, double h,
                                RegionType type = RegionType::Paragraph,
                                double conf = 0.9)
{
    LayoutRegion r;
    r.bbox       = QRectF(x, y, w, h);
    r.type       = type;
    r.confidence = conf;
    return r;
}

// ── Test class ───────────────────────────────────────────────────────────────

class TestLayoutEnsemble : public QObject
{
    Q_OBJECT

private slots:

    // 1. IoU computation
    void testIoU_nonOverlapping()
    {
        QRectF a(0, 0, 10, 10);
        QRectF b(20, 20, 10, 10);
        QCOMPARE(LayoutEnsemble::computeIoU(a, b), 0.0);
    }

    void testIoU_fullyOverlapping()
    {
        QRectF a(0, 0, 10, 10);
        QCOMPARE(LayoutEnsemble::computeIoU(a, a), 1.0);
    }

    void testIoU_partialOverlap()
    {
        // 5×10 overlap between two 10×10 boxes placed 5px apart → 50/150 ≈ 0.333
        QRectF a(0, 0, 10, 10);
        QRectF b(5, 0, 10, 10);
        double iou = LayoutEnsemble::computeIoU(a, b);
        QVERIFY2(std::abs(iou - 50.0/150.0) < 0.001,
                 qPrintable(QString("IoU expected ~0.333, got %1").arg(iou)));
    }

    // 2. Single-detector pass-through
    void testSingleDetectorPassThrough()
    {
        QList<LayoutRegion> regions = {
            makeRegion(0, 0, 100, 50, RegionType::Title, 0.95),
            makeRegion(0, 60, 100, 200, RegionType::Paragraph, 0.88),
        };
        QList<QList<LayoutRegion>> input = { regions };
        auto result = LayoutEnsemble::mergeRegionLists(input);

        QCOMPARE(result.size(), 2);
        // Reading order must be assigned
        QCOMPARE(result[0].readingOrderIndex, 0);
        QCOMPARE(result[1].readingOrderIndex, 1);
    }

    // 3. Two detectors agree (IoU > 0.5) → single merged region
    void testTwoDetectorsAgreeMerge()
    {
        // Same region, slight coordinate variation (IoU well above 0.5)
        LayoutRegion r1 = makeRegion(10, 10, 100, 50, RegionType::Table, 0.9);
        LayoutRegion r2 = makeRegion(10, 10, 100, 50, RegionType::Table, 0.85);

        QList<QList<LayoutRegion>> input = { {r1}, {r2} };
        auto result = LayoutEnsemble::mergeRegionLists(input);

        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].type, RegionType::Table);
    }

    // 4. Two detectors disagree (no IoU overlap) → both kept when conf ≥ threshold
    void testTwoDetectorsBothKeptWhenNoOverlap()
    {
        LayoutRegion r1 = makeRegion(0,   0,  100, 50, RegionType::Title,     0.9);
        LayoutRegion r2 = makeRegion(200, 0,  100, 50, RegionType::Paragraph, 0.8);

        QList<QList<LayoutRegion>> input = { {r1}, {r2} };
        auto result = LayoutEnsemble::mergeRegionLists(input, 0.5, 0.3);

        QCOMPARE(result.size(), 2);
    }

    // 5. Low-confidence unmatched region from secondary is suppressed
    void testLowConfidenceUnpairedSuppressed()
    {
        LayoutRegion r1 = makeRegion(0, 0, 100, 50, RegionType::Title, 0.9);
        LayoutRegion r2Low = makeRegion(200, 0, 100, 50, RegionType::Other, 0.1); // below threshold

        QList<QList<LayoutRegion>> input = { {r1}, {r2Low} };
        auto result = LayoutEnsemble::mergeRegionLists(input, 0.5, 0.3);

        // r2Low should be suppressed (conf 0.1 < 0.3 threshold)
        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].type, RegionType::Title);
    }

    // 6. Type vote: higher-confidence type wins on merge
    void testTypeVoteHigherConfidenceWins()
    {
        // Detector 0 says Table (high confidence), Detector 1 says Figure (lower)
        LayoutRegion r1 = makeRegion(10, 10, 100, 80, RegionType::Table,  0.95);
        LayoutRegion r2 = makeRegion(10, 10, 100, 80, RegionType::Figure, 0.60);

        QList<QList<LayoutRegion>> input = { {r1}, {r2} };
        auto result = LayoutEnsemble::mergeRegionLists(input, 0.5, 0.3);

        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].type, RegionType::Table);  // Table wins (higher conf)
    }

    // 7. Reading order: top-to-bottom then left-to-right
    void testReadingOrderTopToBottom()
    {
        QList<LayoutRegion> regions = {
            makeRegion(0,  300, 100, 50),  // bottom
            makeRegion(0,  100, 100, 50),  // top
            makeRegion(0,  200, 100, 50),  // middle
        };
        QList<QList<LayoutRegion>> input = { regions };
        auto result = LayoutEnsemble::mergeRegionLists(input);

        QCOMPARE(result.size(), 3);
        QCOMPARE(result[0].readingOrderIndex, 0);
        // Verify top region is first
        QVERIFY(result[0].bbox.top() < result[1].bbox.top());
        QVERIFY(result[1].bbox.top() < result[2].bbox.top());
    }

    // 8. Empty page → empty result
    void testEmptyPage()
    {
        QList<QList<LayoutRegion>> input = { {} };
        auto result = LayoutEnsemble::mergeRegionLists(input);
        QVERIFY(result.isEmpty());
    }

    // 9. No detectors → detect() returns empty
    void testNoDetectors()
    {
        LayoutEnsemble ensemble;
        QImage img(100, 100, QImage::Format_RGB888);
        img.fill(Qt::white);
        auto result = ensemble.detect(img);
        QVERIFY(result.isEmpty());
    }

    // 10. Single-detector mode with null scheduler (no crash)
    void testSingleDetectorNoScheduler()
    {
        QList<LayoutRegion> regions = {
            makeRegion(0, 0, 200, 100, RegionType::Title, 0.92),
        };
        StubDetector stub(QStringLiteral("TestDet"), regions);

        LayoutEnsemble ensemble;  // no scheduler
        ensemble.addDetector(&stub);

        QImage img(400, 800, QImage::Format_RGB888);
        img.fill(Qt::white);
        auto result = ensemble.detect(img);

        QCOMPARE(result.size(), 1);
        QCOMPARE(result[0].type, RegionType::Title);
        QCOMPARE(result[0].readingOrderIndex, 0);
    }
};

QTEST_MAIN(TestLayoutEnsemble)
#include "TestLayoutEnsemble.moc"
