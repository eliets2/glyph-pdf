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
// per-toggle work (N lookups over N anchors) at two sizes 4x apart and asserts
// the growth stays NEAR-LINEAR. Measured at the tip (evidence, followups
// lane): regressed (linear scan) ≈ 15.8–16.5x; fixed (memo) ≈ 6–8x — the
// residual growth above the naive ~4x is memory hierarchy (the memo table
// itself outgrows cache at 8000 entries), which is why the guard sits at
// < 10.0x, the measured midpoint with wide margins on both sides. The probe
// calibrates its repeat count to a 100 ms floor per sample so the ratio is
// stable run-to-run (a 20 ms floor produced 6.3–8.2x scatter on the FIXED
// code — the guard must never flake on timer quantization).
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

} // namespace

class TestSep13LeadComparePerf : public QObject {
    Q_OBJECT

private slots:
    void anchorRoleRecomputeStaysNearLinear() {
        CompareWidget widget;

        const int small = 2000;
        const int large = 8000;

        widget.setDiffResult(makeResultWithChanges(small));
        int repeats = 1;
        qint64 tSmallTotal = measureToggleWorkMs(widget, small, repeats);
        while (tSmallTotal < 100 && repeats < (1 << 20)) {
            repeats *= 4;
            tSmallTotal = measureToggleWorkMs(widget, small, repeats);
        }
        const double tSmall = double(tSmallTotal) / repeats;

        widget.setDiffResult(makeResultWithChanges(large));
        const qint64 tLargeTotal = measureToggleWorkMs(widget, large, repeats);
        const double tLarge = double(tLargeTotal) / repeats;

        const double ratio = tLarge / tSmall;
        qInfo() << "anchor-role recompute: rows =" << small << "->" << tSmall << "ms/pass"
                << "over" << repeats << "repeats;"
                << "rows =" << large << "->" << tLarge << "ms/pass; ratio =" << ratio
                << "(linear expectation ~4x, memo cache effects push the fixed "
                   "baseline to ~6-8x; the O(rows x anchors) regression measured "
                   "15.8-16.5x)";
        QVERIFY2(ratio < 10.0,
                 QStringLiteral("SEP13 lead 12 REGRESSED: anchor-index recompute grows "
                 "superlinearly again (ratio %1 at 4x rows; fixed baseline is ~6-8x, "
                 "the pre-fix O(rows x anchors) scan measured 15.8-16.5x) — is "
                 "CompareWidget::m_anchorIndexByPage still consulted by "
                 "anchorIndexForPage, and is it still rebuilt in buildHtml?")
                     .arg(ratio, 0, 'f', 1).toUtf8().constData());
    }
};

#include "TestSep13LeadComparePerf.moc"
QTEST_MAIN(TestSep13LeadComparePerf)
