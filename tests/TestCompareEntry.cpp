// SPDX-License-Identifier: Apache-2.0
// §9.10 P0 — Document Comparison entry point. The Myers-diff engine and
// CompareMode UI were built and unit-tested, but compareFiles() had no caller,
// so every menu route landed on an empty screen. A "Compare Docs…" button now
// drives promptAndCompare() -> startComparison() -> pathsAreComparable() ->
// compareFiles(). This tests the pure validator seam that guards that path
// (the file pickers themselves are modal UI and are not unit-tested here).
//
// §9.10 P1-adjacent P0 — change-type filter for the CHANGES tree. The diff
// engine already tags every change as text / move / pixel / page-move; the
// filter must gate only the VIEW (which tree rows are shown), never mutate the
// underlying DiffResult, and re-apply immediately when a toggle flips.
#include <QtTest>
#include <QPainter>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QTreeWidget>
#include <QToolButton>

#include "modes/CompareMode.h"

class TestCompareEntry : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    // A minimal but real one-page PDF on disk.
    QString makePdf(const QString& name)
    {
        const QString path = m_dir.filePath(name);
        QPdfWriter w(path);
        w.setPageSize(QPageSize(QPageSize::A4));
        QPainter p(&w);
        p.drawText(100, 100, name);
        p.end();
        return path;
    }

    // A diff result exercising every change-type tag the engine produces:
    //   page 0: text-only   (1 added, 1 removed)
    //   page 1: move-only   (one moved token)
    //   page 2: pixel-only  (42 pixels differ)
    //   page 3: text+pixel  (mixed row must stay visible while EITHER of its
    //                        tags is still checked)
    // plus one whole-page reorder (page-move row).
    static DiffResult makeSampleResult()
    {
        DiffResult r;
        r.isIdentical = false;
        r.pageCount1 = 4;
        r.pageCount2 = 4;

        PageDiff textOnly;
        textOnly.pageIndex = 0;
        textOnly.textAdded << QStringLiteral("gamma");
        textOnly.textRemoved << QStringLiteral("delta");
        r.pages << textOnly;

        PageDiff moveOnly;
        moveOnly.pageIndex = 1;
        moveOnly.moves << MoveOperation{QStringLiteral("alpha"), 3, 1};
        r.pages << moveOnly;

        PageDiff pixelOnly;
        pixelOnly.pageIndex = 2;
        pixelOnly.pixelDiffCount = 42;
        r.pages << pixelOnly;

        PageDiff textAndPixel;
        textAndPixel.pageIndex = 3;
        textAndPixel.textAdded << QStringLiteral("omega");
        textAndPixel.pixelDiffCount = 7;
        r.pages << textAndPixel;

        r.pageMoves << DiffResult::PageMove{0, 2, QStringLiteral("excerpt")};
        return r;
    }

    // Count of top-level CHANGES-tree rows the user can actually see.
    static int visibleTopLevelRows(QTreeWidget* tree)
    {
        int visible = 0;
        for (int i = 0; i < tree->topLevelItemCount(); ++i)
            if (!tree->topLevelItem(i)->isHidden())
                ++visible;
        return visible;
    }

    static QToolButton* filterButton(QWidget& mode, const char* objectName)
    {
        return mode.findChild<QToolButton*>(QString::fromLatin1(objectName));
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    // Two distinct, existing PDFs are accepted for comparison.
    void distinctExistingFilesAreComparable()
    {
        const QString a = makePdf("orig.pdf");
        const QString b = makePdf("revised.pdf");
        QString why;
        QVERIFY2(gp::CompareMode::pathsAreComparable(a, b, &why), qPrintable(why));
    }

    // Comparing a file with itself is rejected (would produce an empty diff and
    // read as broken) — the guard reports why.
    void sameFileIsRejected()
    {
        const QString a = makePdf("same.pdf");
        QString why;
        QVERIFY(!gp::CompareMode::pathsAreComparable(a, a, &why));
        QVERIFY2(!why.isEmpty(), "rejection must explain itself to the user");
    }

    // A non-existent path is rejected with a "not found" reason.
    void missingFileIsRejected()
    {
        const QString a = makePdf("present.pdf");
        QString why;
        QVERIFY(!gp::CompareMode::pathsAreComparable(a, m_dir.filePath("absent.pdf"), &why));
        QVERIFY2(why.contains("not found", Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("expected a not-found reason, got: %1").arg(why)));
    }

    // ── §9.10: change-type filter — pure seam ────────────────────────────────────
    // rowsVisibleForFilters() counts the rows the CHANGES tree would show for a
    // given set of toggles. All-checked must reproduce the current behaviour
    // exactly (every produced row visible).
    void changeTypeFilterSeamCountsVisibleRows()
    {
        const DiffResult r = makeSampleResult();  // 4 page rows + 1 page-move row

        // Baseline: every toggle on = today's behaviour, 5 rows.
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, true, true, true, true), 5);

        // Text off: the text-only page disappears, but the mixed text+pixel page
        // stays visible through its still-checked pixel tag.
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, false, true, true, true), 4);

        // Move off: the move-only page disappears.
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, true, false, true, true), 4);

        // Pixel off: only the pixel-only page disappears; the mixed text+pixel
        // page and all other rows (4 of 5) remain visible.
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, true, true, false, true), 4);

        // Page-move off: the reorder row disappears.
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, true, true, true, false), 4);

        // Only pixel+page-move on.
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, false, false, true, true), 3);

        // Only moves on: exactly the move-only page row.
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, false, true, false, false), 1);

        // Everything off: nothing shown (the data itself is untouched).
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, false, false, false, false), 0);
    }

    // ── §9.10: change-type filter — widget wiring ────────────────────────────────
    // Four checkable toggles named cmpFilterText/cmpFilterMove/cmpFilterPixel/
    // cmpFilterPageMove must exist above the tree and default to CHECKED, so
    // filters ship as a no-op until the user asks for one.
    void filterTogglesExistCheckedByDefaultAndWired()
    {
        gp::CompareMode mode;
        const char* names[] = {"cmpFilterText", "cmpFilterMove",
                               "cmpFilterPixel", "cmpFilterPageMove"};
        for (const char* name : names) {
            auto* b = filterButton(mode, name);
            QVERIFY2(b, qPrintable(QStringLiteral("%1 not found").arg(name)));
            QVERIFY2(b->isCheckable(), qPrintable(QStringLiteral("%1 must be checkable").arg(name)));
            QVERIFY2(b->isChecked(), qPrintable(QStringLiteral("%1 must default to checked").arg(name)));
        }
    }

    // Toggling a filter hides matching tree rows IMMEDIATELY (signal-driven,
    // no re-diff, no modal) and never mutates the underlying DiffResult.
    void togglingFiltersHidesRowsImmediatelyWithoutMutatingResult()
    {
        gp::CompareMode mode;
        mode.showDiffResult(makeSampleResult());

        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY2(tree, "CHANGES tree must carry objectName cmpChangesTree");
        QCOMPARE(tree->topLevelItemCount(), 5);          // 4 page rows + 1 page-move row
        QCOMPARE(visibleTopLevelRows(tree), 5);          // defaults: everything visible

        // Text off → the text-only page row (index 0) hides, the mixed
        // text+pixel row (index 3) survives on its pixel tag.
        auto* textBtn = filterButton(mode, "cmpFilterText");
        QVERIFY(textBtn);
        textBtn->setChecked(false);
        QCOMPARE(visibleTopLevelRows(tree), 4);
        QVERIFY(tree->topLevelItem(0)->isHidden());
        QVERIFY(!tree->topLevelItem(3)->isHidden());
        QVERIFY(!tree->topLevelItem(4)->isHidden());     // page-move row unaffected

        // Page-move off too → the reorder row hides as well.
        auto* pageMoveBtn = filterButton(mode, "cmpFilterPageMove");
        QVERIFY(pageMoveBtn);
        pageMoveBtn->setChecked(false);
        QCOMPARE(visibleTopLevelRows(tree), 3);
        QVERIFY(tree->topLevelItem(4)->isHidden());

        // Re-checking text re-applies immediately while page-move stays off.
        textBtn->setChecked(true);
        QCOMPARE(visibleTopLevelRows(tree), 4);
        QVERIFY(!tree->topLevelItem(0)->isHidden());

        // The filter is display-layer only: the diff result behind the view is
        // byte-for-byte the same shape as before any toggling.
        QCOMPARE(mode.lastResult().pages.size(), 4);
        QCOMPARE(mode.lastResult().pageMoves.size(), 1);
        QCOMPARE(mode.lastResult().pages[0].textAdded.size(), 1);
        QCOMPARE(mode.lastResult().pages[0].textRemoved.size(), 1);
        QCOMPARE(mode.lastResult().pages[1].moves.size(), 1);
        QCOMPARE(mode.lastResult().pages[2].pixelDiffCount, 42);

        // Restore defaults so the widget is left in the shipped state.
        textBtn->setChecked(true);
        pageMoveBtn->setChecked(true);
        QCOMPARE(visibleTopLevelRows(tree), 5);
    }
};

QTEST_MAIN(TestCompareEntry)
#include "TestCompareEntry.moc"
