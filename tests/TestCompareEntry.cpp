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
#include <QLabel>
#include <QPainter>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QTreeWidget>
#include <QSplitter>
#include <QTextBrowser>
#include <QToolButton>

#include "modes/CompareMode.h"
#include "ui/CompareWidget.h"
#include "ui/PdfViewerWidget.h"  // R11 viewer-sync assertions need the complete type

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

    // R11 fixture: hand-built N-page text PDF (TestExportPathBadge::createTextPdf
    // idiom extended to N pages). QPdfWriter output embeds subset fonts that
    // extract as garbage, so DiffEngine-driven tests need raw string literals.
    // An empty string yields a page with an empty content stream (blank page).
    QString createPagePdf(const QString& name, const QStringList& pageTexts)
    {
        const int n = pageTexts.size();
        QByteArray pdf = "%PDF-1.4\n";
        QList<qint64> offsets;
        offsets.append(pdf.size());
        pdf += "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n";
        offsets.append(pdf.size());
        QByteArray kids;
        for (int k = 0; k < n; ++k)
            kids += QByteArray::number(3 + 2 * k) + " 0 R ";
        pdf += "2 0 obj<</Type/Pages/Kids[" + kids + "]/Count "
               + QByteArray::number(n) + ">>endobj\n";
        for (int k = 0; k < n; ++k) {
            const int pageNo = 3 + 2 * k;
            const int contNo = 4 + 2 * k;
            const QString line = pageTexts.at(k);
            QByteArray content;
            if (!line.isEmpty()) {
                QByteArray lit = line.toLatin1();
                lit.replace('\\', "\\\\").replace('(', "\\(").replace(')', "\\)");
                content = "BT /F1 12 Tf 72 720 Td (" + lit + ") Tj ET\n";
            }
            offsets.append(pdf.size());
            pdf += QByteArray::number(pageNo)
                 + " 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents "
                 + QByteArray::number(contNo)
                 + " 0 R/Resources<</Font<</F1 " + QByteArray::number(3 + 2 * n)
                 + " 0 R>>>>>>endobj\n";
            offsets.append(pdf.size());
            pdf += QByteArray::number(contNo) + " 0 obj<</Length "
                 + QByteArray::number(content.size()) + ">>stream\n"
                 + content + "endstream endobj\n";
        }
        offsets.append(pdf.size());
        pdf += QByteArray::number(3 + 2 * n)
             + " 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>endobj\n";
        const qint64 xrefOffset = pdf.size();
        const int objCount = 4 + 2 * n;
        pdf += "xref\n0 " + QByteArray::number(objCount) + "\n0000000000 65535 f \n";
        for (qint64 off : offsets)
            pdf += QByteArray::number(static_cast<qulonglong>(off))
                       .rightJustified(10, '0')
                   + " 00000 n \n";
        pdf += "trailer<</Size " + QByteArray::number(objCount)
               + "/Root 1 0 R>>\nstartxref\n" + QByteArray::number(xrefOffset)
               + "\n%%EOF\n";

        const QString path = m_dir.filePath(name);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return {};
        f.write(pdf);
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

        // R11: the engine records the same whole-page reorder in the unified
        // structural sequence as well — pageChanges is the single canonical
        // list the CHANGES tree, the filters, the totals and the reports read;
        // pageMoves stays populated for backward compatibility.
        r.pageChanges << DiffResult::PageChange{
            DiffResult::PageChangeType::PageMoved, 0, 2, QStringLiteral("excerpt")};
        return r;
    }

    // R11 synthetic result: one page added to the revised document, one page
    // removed from the original document, one whole-page reorder — and no
    // token-level page diffs, so every visible row is structural.
    static DiffResult makeStructuralResult()
    {
        DiffResult r;
        r.isIdentical = false;
        r.pageCount1 = 3;
        r.pageCount2 = 4;
        r.pageChanges << DiffResult::PageChange{
            DiffResult::PageChangeType::PageAdded, -1, 2, QStringLiteral("new bit")};
        r.pageChanges << DiffResult::PageChange{
            DiffResult::PageChangeType::PageRemoved, 0, -1, QStringLiteral("old bit")};
        r.pageChanges << DiffResult::PageChange{
            DiffResult::PageChangeType::PageMoved, 1, 3, QStringLiteral("shifted")};
        r.pageMoves << DiffResult::PageMove{1, 3, QStringLiteral("shifted")};
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

    // ── R11: structural page changes reach the tree with their own filter tag ───

    // rowsVisibleForFilters must count the unified structural sequence:
    // added/removed rows behind the new showPageAddRemove gate, moved rows
    // behind the existing showPageMove gate — never double-counted via the
    // legacy pageMoves list.
    void structuralFilterSeamCountsAddedRemovedRows()
    {
        const DiffResult r = makeStructuralResult();  // 1 added + 1 removed + 1 moved

        // Baseline: every toggle on = every structural row visible.
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, true, true, true, true, true), 3);

        // Add/remove gate off: added + removed rows hide, the move stays.
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, true, true, true, true, false), 1);

        // Page-move gate off: the move hides, added + removed stay.
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, true, true, true, false, true), 2);

        // Everything off: nothing shown.
        QCOMPARE(gp::CompareMode::rowsVisibleForFilters(r, false, false, false, false, false), 0);
    }

    // The CHANGES tree must show one row per structural change, tag added and
    // removed rows with their own filter role, and name the correct page AND
    // side in the description.
    void structuralRowsAppearWithOwnFilterTagAndSideNames()
    {
        gp::CompareMode mode;
        mode.showDiffResult(makeStructuralResult());

        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY2(tree, "CHANGES tree must carry objectName cmpChangesTree");
        QCOMPARE(tree->topLevelItemCount(), 3);

        // Added row: names the page and the revised side.
        QTreeWidgetItem* added = tree->topLevelItem(0);
        QVERIFY2(added->text(2).contains(QStringLiteral("Page 3 added in revised document")),
                 qPrintable(QStringLiteral("added row description: %1").arg(added->text(2))));
        QVERIFY2(added->text(1).contains(QStringLiteral("p.3")),
                 qPrintable(QStringLiteral("added row page column: %1").arg(added->text(1))));
        QVERIFY2(added->data(0, gp::CompareMode::kIsPageAddRemoveRole).toBool(),
                 "added row must carry the add/remove filter tag");
        QVERIFY(!added->data(0, gp::CompareMode::kIsPageMoveRole).toBool());

        // Removed row: names the page and the original side.
        QTreeWidgetItem* removed = tree->topLevelItem(1);
        QVERIFY2(removed->text(2).contains(QStringLiteral("Page 1 removed from original document")),
                 qPrintable(QStringLiteral("removed row description: %1").arg(removed->text(2))));
        QVERIFY2(removed->text(1).contains(QStringLiteral("p.1")),
                 qPrintable(QStringLiteral("removed row page column: %1").arg(removed->text(1))));
        QVERIFY2(removed->data(0, gp::CompareMode::kIsPageAddRemoveRole).toBool(),
                 "removed row must carry the add/remove filter tag");

        // Moved row keeps its existing page-move tag (not add/remove).
        QTreeWidgetItem* moved = tree->topLevelItem(2);
        QVERIFY2(moved->text(2).contains(QStringLiteral("Page 2 moved to position 4")),
                 qPrintable(QStringLiteral("moved row description: %1").arg(moved->text(2))));
        QVERIFY2(moved->data(0, gp::CompareMode::kIsPageMoveRole).toBool(),
                 "moved row must keep the page-move filter tag");
        QVERIFY(!moved->data(0, gp::CompareMode::kIsPageAddRemoveRole).toBool());

        // The new toggle exists, defaults to checked, and gates exactly the
        // added/removed rows.
        auto* addRmBtn = filterButton(mode, "cmpFilterPageAddRemove");
        QVERIFY2(addRmBtn, "cmpFilterPageAddRemove toggle must exist");
        QVERIFY(addRmBtn->isCheckable() && addRmBtn->isChecked());

        addRmBtn->setChecked(false);
        QCOMPARE(visibleTopLevelRows(tree), 1);
        QVERIFY(tree->topLevelItem(0)->isHidden());
        QVERIFY(tree->topLevelItem(1)->isHidden());
        QVERIFY(!tree->topLevelItem(2)->isHidden());

        auto* pageMoveBtn = filterButton(mode, "cmpFilterPageMove");
        QVERIFY(pageMoveBtn);
        pageMoveBtn->setChecked(false);
        QCOMPARE(visibleTopLevelRows(tree), 0);

        addRmBtn->setChecked(true);
        QCOMPARE(visibleTopLevelRows(tree), 2);
        QVERIFY(!tree->topLevelItem(0)->isHidden());
        QVERIFY(!tree->topLevelItem(1)->isHidden());
        QVERIFY(tree->topLevelItem(2)->isHidden());

        pageMoveBtn->setChecked(true);
        QCOMPARE(visibleTopLevelRows(tree), 3);

        // Filters stay display-layer only: the structural data behind the view
        // is untouched.
        QCOMPARE(mode.lastResult().pageChanges.size(), 3);
    }

    // The status total counts structural changes — surplus pages are changes.
    void statusTotalsCountStructuralChanges()
    {
        gp::CompareMode mode;
        mode.showDiffResult(makeStructuralResult());

        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY2(status, "status label must carry objectName cmpStatusLabel");
        QVERIFY2(status->text().contains(QStringLiteral("3 CHANGES")),
                 qPrintable(QStringLiteral("status label: %1").arg(status->text())));
    }

    // Text and HTML reports must name the correct page and side for every
    // structural change (R11 acceptance).
    void reportsNamePageAndSideForStructuralChanges()
    {
        gp::CompareMode mode;
        mode.showDiffResult(makeStructuralResult());

        const QString txt = mode.buildTextReport();
        QVERIFY2(txt.contains(QStringLiteral("Page 3 added in revised document")),
                 qPrintable(QStringLiteral("text report: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("Page 1 removed from original document")),
                 qPrintable(QStringLiteral("text report: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("Page 2 moved to position 4")),
                 qPrintable(QStringLiteral("text report: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("1 added, 1 removed, 1 moved")),
                 qPrintable(QStringLiteral("text report page-change summary: %1").arg(txt)));

        const QString html = mode.buildHtmlReport();
        QVERIFY2(html.contains(QStringLiteral("Page 3 added in revised document")),
                 "html report must name the added page and side");
        QVERIFY2(html.contains(QStringLiteral("Page 1 removed from original document")),
                 "html report must name the removed page and side");
        QVERIFY2(html.contains(QStringLiteral("Page 2 moved to position 4")),
                 "html report must keep the moved page entry");
    }

    // Selecting a structural CHANGES row must jump the one shared change
    // sequence (text panel + navigation state), not just sit there.
    void treeSelectionJumpsToStructuralChangeInSharedSequence()
    {
        gp::CompareMode mode;
        // Mirror the real onDiffFinished flow: the widget receives the result
        // (building the shared anchor sequence) before the tree is populated.
        mode.showDiffResult(makeStructuralResult());
        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY2(widget, "CompareMode must own a CompareWidget");
        widget->setDiffResult(makeStructuralResult());

        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY(tree);
        auto* nav = widget->findChild<QLabel*>(QStringLiteral("cmpNavLabel"));
        QVERIFY2(nav, "CompareWidget nav label must carry objectName cmpNavLabel");

        tree->setCurrentItem(tree->topLevelItem(0));  // the added page row
        QVERIFY2(nav->text().contains(QStringLiteral("change 1 of 3")),
                 qPrintable(QStringLiteral("nav label after tree selection: %1").arg(nav->text())));

        tree->setCurrentItem(tree->topLevelItem(2));  // the moved page row
        QVERIFY2(nav->text().contains(QStringLiteral("change 3 of 3")),
                 qPrintable(QStringLiteral("nav label after tree selection: %1").arg(nav->text())));
    }

    // next/previous navigation must reach structural changes, and the viewer
    // on the side that HAS the page must follow it (a side without the page
    // stays put — no bogus page navigation).
    void widgetNavigationIncludesStructuralChangesAndSyncsViewers()
    {
        const QString base = createPagePdf("nav_base.pdf", {"First page"});
        const QString extended =
            createPagePdf("nav_extended.pdf", {"First page", "Appendix page"});
        QVERIFY(!base.isEmpty() && !extended.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(base, extended);
        QCOMPARE(r.pageChanges.size(), 1);
        QCOMPARE(r.pageChanges.first().type, DiffResult::PageChangeType::PageAdded);

        CompareWidget widget;
        QVERIFY(widget.loadDocuments(base, extended));
        widget.setDiffResult(r);

        auto* nav = widget.findChild<QLabel*>(QStringLiteral("cmpNavLabel"));
        QVERIFY2(nav, "CompareWidget nav label must carry objectName cmpNavLabel");

        // The added page is a change in the shared sequence.
        widget.nextChange();
        QVERIFY2(nav->text().contains(QStringLiteral("change 1 of 1")),
                 qPrintable(QStringLiteral("nav label: %1").arg(nav->text())));

        auto* browser = widget.findChild<QTextBrowser*>();
        QVERIFY2(browser, "CompareWidget must own its text diff browser");
        QVERIFY2(browser->toPlainText().contains(
                     QStringLiteral("Page 2 added in revised document")),
                 qPrintable(QStringLiteral("text diff panel: %1").arg(browser->toPlainText())));

        // Viewer sync: the revised side follows the change to page 2 (index 1);
        // the original side has no such page and stays on page 1 (index 0).
        // Acquire the two viewers by splitter position — findChildren() order
        // is not stable once QSplitter re-parents them.
        auto* split = widget.findChild<QSplitter*>();
        QVERIFY2(split, "CompareWidget must own the side-by-side viewer splitter");
        auto* left  = qobject_cast<PdfViewerWidget*>(split->widget(0));
        auto* right = qobject_cast<PdfViewerWidget*>(split->widget(1));
        QVERIFY(left && right);
        QVERIFY(left->pageCount() == 1 && right->pageCount() == 2);  // base / extended
        // QPdfPageNavigator page updates may be deferred — process events.
        QTRY_COMPARE(right->currentPage(), 1);
        QCOMPARE(left->currentPage(), 0);
    }
};

QTEST_MAIN(TestCompareEntry)
#include "TestCompareEntry.moc"
