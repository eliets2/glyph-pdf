// SPDX-License-Identifier: Apache-2.0
// SEP13 lead 12 — CompareMode::applyChangeTypeFilters recomputes the
// anchor-index role for EVERY row on EVERY filter toggle, and each lookup
// (CompareWidget::anchorIndexForPage / anchorIndexForStructuralChange) used to
// be a LINEAR SCAN over the anchor list — O(rows x anchors) per toggle on the
// GUI thread (quadratic in the change count).
//
// PARITY-GLM-REVIEW-2026-09-13 lead (CompareMode.cpp ~392). Impact:
// perf-only (each toggle on a large diff stalls the GUI thread); the mapping
// result itself is correct (U04 rebuilds it each time).
//
// FIXED (follow-ups lane, 2026-09-15): CompareWidget memoizes pageDiffIndex →
// anchor index in m_anchorIndexByPage, rebuilt in the ONE funnel that rebuilds
// m_anchors (buildHtml), so anchorIndexForPage is a hash lookup and the
// per-toggle recompute is O(rows) instead of O(rows x anchors).
//
// This probe is now the GUARD against regression: it measures exactly the
// per-toggle work (N lookups over N anchors) at two sizes 16x apart and
// asserts the growth stays NEAR-LINEAR. Each size calibrates its own repeat
// count to a 100 ms floor per sample, so timer quantization never decides.
// History: the first version used sizes 4x apart (2000/8000) with a < 10x
// line between fixed (~6-8x) and regressed (~16x). The optimized -flto
// Release build pushed the FIXED code to 8.8-12x (the memo table leaving
// L1/L2 is a bigger share of a cheaper lookup) and failed both checks in
// turn — so the spread is now 16x, where the two regimes sit an order of
// magnitude apart (see kCeiling for the measured evidence).
#include <QtTest/QtTest>
#include <QElapsedTimer>

#include "engines/DiffEngine.h"
#include "ui/CompareWidget.h"

// DiffEngine / CompareWidget types live in the GLOBAL namespace.

namespace {

DiffResult makeResultWithChanges(int rows) {
    DiffResult r;
    r.isIdentical = false;
    r.pageCount1 = rows;
    r.pageCount2 = rows;
    for (int i = 0; i < rows; ++i) {
        PageDiff p;
        p.pageIndex = i;
        p.oldPage = i;
        p.newPage = i;
        p.textRemoved.append(QStringLiteral("word%1").arg(i));
        p.textAdded.append(QStringLiteral("word%1b").arg(i));
        r.pages.append(p);
    }
    return r;
}

// PGR-10 triage (lead-12 completion): the STRUCTURAL half of the same
// finding — N added/removed page changes give N structural rows, and
// applyChangeTypeFilters maps every visible row per toggle through
// anchorIndexForStructuralChange, which was still a linear scan over the
// anchors after the page half was memoized.
DiffResult makeResultWithStructuralChanges(int rows) {
    DiffResult r;
    r.isIdentical = false;
    r.pageCount1 = rows;
    r.pageCount2 = rows;
    for (int i = 0; i < rows; i += 2)
        r.pageChanges.append({DiffResult::PageChangeType::PageRemoved,
                              i, -1, QString()});
    for (int i = 1; i < rows; i += 2)
        r.pageChanges.append({DiffResult::PageChangeType::PageAdded,
                              -1, i, QString()});
    return r;
}

qint64 measureToggleWorkMs(CompareWidget& widget, int rows, int repeats = 1) {
    // One filter toggle's worth of anchor-role recomputation,
    // exactly what applyChangeTypeFilters does per visible row,
    // repeated to stay above the ms-timer resolution floor.
    QElapsedTimer timer;
    timer.start();
    volatile qint64 sink = 0;
    for (int r = 0; r < repeats; ++r)
        for (int i = 0; i < rows; ++i)
            sink += widget.anchorIndexForPage(i);
    return timer.elapsed();
}

qint64 measureStructuralToggleWorkMs(CompareWidget& widget, int rows, int repeats = 1) {
    // The structural counterpart: one toggle's worth of
    // anchorIndexForStructuralChange lookups over the pageChanges sequence.
    QElapsedTimer timer;
    timer.start();
    volatile qint64 sink = 0;
    for (int r = 0; r < repeats; ++r)
        for (int i = 0; i < rows; ++i)
            sink += widget.anchorIndexForStructuralChange(i);
    return timer.elapsed();
}

// Per-pass time (ms) at `rows`, each size calibrated on its own so every
// sample spans >= 100 ms — timer quantization can never dominate either side.
double calibratedPassMs(CompareWidget& widget, int rows,
                        qint64 (*measure)(CompareWidget&, int, int)) {
    int repeats = 1;
    qint64 total = measure(widget, rows, repeats);
    while (total < 100 && repeats < (1 << 24)) {
        repeats *= 4;
        total = measure(widget, rows, repeats);
    }
    return double(total) / repeats;
}

// The guard compares two sizes kSpread apart. A 4x spread could not separate
// the two regimes on an optimized (-flto) Release build: the FIXED memo lookup
// is cheap enough that cache effects alone (the 8000-entry table leaves L1/L2)
// pushed its ratio to 8.8-12x, straddling the old < 10x line that the
// O(rows x anchors) regression (~16x) sat just above. At 16x the regimes are
// an order of magnitude apart: linear is ~16x plus the same cache factor,
// quadratic is ~256x before cache effects.
constexpr int kSmall = 500;
constexpr int kLarge = 8000;
// Measured 2026-09-29 (release verification, 500 -> 8000 rows):
//   FIXED memo      dev Release 27.7-46.7x, -flto Release 30.8-42.4x
//   REGRESSED scan  (negative control: anchorIndexFor* linear scan) 228.9-291.8x
// 100x is the geometric midpoint: >= 2.1x headroom above the worst fixed
// sample and >= 2.3x below the best regressed one.
constexpr double kCeiling = 100.0;

} // namespace

class TestSep13LeadComparePerf : public QObject {
    Q_OBJECT

private slots:
    void anchorRoleRecomputeStaysNearLinear() {
        CompareWidget widget;

        widget.setDiffResult(makeResultWithChanges(kSmall));
        const double tSmall = calibratedPassMs(widget, kSmall, measureToggleWorkMs);
        widget.setDiffResult(makeResultWithChanges(kLarge));
        const double tLarge = calibratedPassMs(widget, kLarge, measureToggleWorkMs);

        const double ratio = tLarge / tSmall;
        qInfo() << "anchor-role recompute: rows =" << kSmall << "->" << tSmall << "ms/pass;"
                << "rows =" << kLarge << "->" << tLarge << "ms/pass; ratio =" << ratio
                << "(linear expectation ~16x plus cache effects; the O(rows x anchors)"
                   " regression is ~256x before cache effects)";
        QVERIFY2(ratio < kCeiling,
                 QStringLiteral("SEP13 lead 12 REGRESSED: anchor-index recompute grows "
                 "superlinearly again (ratio %1 at 16x rows; linear is ~16x plus cache "
                 "effects, the pre-fix O(rows x anchors) scan is ~256x) — is "
                 "CompareWidget::m_anchorIndexByPage still consulted by "
                 "anchorIndexForPage, and is it still rebuilt in buildHtml?")
                     .arg(ratio, 0, 'f', 1).toUtf8().constData());
    }

    void structuralAnchorLookupStaysNearLinear() {
        // PGR-10 triage completion of the same lead-12 finding: the structural
        // half of applyChangeTypeFilters' per-row mapping was still a linear
        // scan (the page half was memoized by the follow-ups lane). Same
        // two-sizes-16x-apart near-linear guard, same kCeiling.
        CompareWidget widget;

        widget.setDiffResult(makeResultWithStructuralChanges(kSmall));
        const double tSmall = calibratedPassMs(widget, kSmall, measureStructuralToggleWorkMs);
        widget.setDiffResult(makeResultWithStructuralChanges(kLarge));
        const double tLarge = calibratedPassMs(widget, kLarge, measureStructuralToggleWorkMs);

        const double ratio = tLarge / tSmall;
        qInfo() << "structural anchor lookup: rows =" << kSmall << "->" << tSmall << "ms/pass;"
                << "rows =" << kLarge << "->" << tLarge << "ms/pass; ratio =" << ratio
                << "(linear expectation ~16x plus cache effects; the O(rows x anchors)"
                   " regression is ~256x before cache effects)";
        QVERIFY2(ratio < kCeiling,
                 QStringLiteral("lead-12 structural half REGRESSED: "
                 "anchorIndexForStructuralChange grows superlinearly again (ratio %1 "
                 "at 16x rows; linear is ~16x plus cache effects) — is "
                 "CompareWidget::m_anchorIndexByStructuralChange still consulted by "
                 "anchorIndexForStructuralChange, and is it still rebuilt in "
                 "buildHtml?").arg(ratio, 0, 'f', 1).toUtf8().constData());
    }
};

#include "TestSep13LeadComparePerf.moc"
QTEST_MAIN(TestSep13LeadComparePerf)
