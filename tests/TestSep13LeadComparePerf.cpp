// SPDX-License-Identifier: Apache-2.0
// SEP13 lead 12 — CompareMode::applyChangeTypeFilters recomputes the
// anchor-index role for EVERY row on EVERY filter toggle, and each lookup
// (CompareWidget::anchorIndexForPage / anchorIndexForStructuralChange) is a
// LINEAR SCAN over the anchor list — O(rows x anchors) per toggle on the GUI
// thread (quadratic in the change count).
//
// PARITY-GLM-REVIEW-2026-09-13 lead (CompareMode.cpp ~392). Impact:
// perf-only (each toggle on a large diff stalls the GUI thread); the mapping
// result itself is correct (U04 rebuilds it each time).
//
// This probe measures exactly the per-toggle work (N lookups over N anchors)
// at two sizes and asserts SUPERLINEAR growth — the O(N^2) signature. The
// measured numbers are the confirmation evidence; the assert is the formality.
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

qint64 measureToggleWorkMs(CompareWidget& widget, int rows) {
    // One filter toggle's worth of anchor-role recomputation:
    // exactly what applyChangeTypeFilters does per visible row.
    QElapsedTimer timer;
    timer.start();
    volatile qint64 sink = 0;
    for (int i = 0; i < rows; ++i)
        sink += widget.anchorIndexForPage(i);
    return timer.elapsed();
}

} // namespace

class TestSep13LeadComparePerf : public QObject {
    Q_OBJECT

private slots:
    void anchorRoleRecomputeIsSuperlinear() {
        CompareWidget widget;

        const int small = 2000;
        const int large = 8000;

        widget.setDiffResult(makeResultWithChanges(small));
        const qint64 tSmall = measureToggleWorkMs(widget, small);

        widget.setDiffResult(makeResultWithChanges(large));
        const qint64 tLarge = measureToggleWorkMs(widget, large);

        const double ratio = tSmall > 0 ? double(tLarge) / double(tSmall) : -1.0;
        qInfo() << "anchor-role recompute: rows =" << small << "->" << tSmall << "ms;"
                << "rows =" << large << "->" << tLarge << "ms; ratio =" << ratio
                << "(linear expectation ~4x, O(rows x anchors) expectation ~16x)";

        QVERIFY2(tSmall > 0, "measurement must be non-zero (increase sizes if too fast)");
        QVERIFY2(ratio >= 8.0,
                 QStringLiteral("SEP13 lead 12 CONFIRMED: anchor-index recompute grows superlinearly "
                 "(ratio %1 at 4x rows) — applyChangeTypeFilters is O(rows x anchors) "
                 "per filter toggle on the GUI thread")
                     .arg(ratio, 0, 'f', 1).toUtf8().constData());
    }
};

#include "TestSep13LeadComparePerf.moc"
QTEST_MAIN(TestSep13LeadComparePerf)
