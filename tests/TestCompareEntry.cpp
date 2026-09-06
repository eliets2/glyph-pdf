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
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QPdfView>
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
    // U04: every page also carries a DISTINCT diffImage (red/green/blue/white)
    // so the per-change overlay tests can pin exactly which page's image the
    // overlay pushes for a given selected change.
    static QImage solidImage(int r, int g, int b)
    {
        QImage img(4, 4, QImage::Format_ARGB32);
        img.fill(QColor(r, g, b));
        return img;
    }

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
        textOnly.diffImage = solidImage(255, 0, 0);
        r.pages << textOnly;

        PageDiff moveOnly;
        moveOnly.pageIndex = 1;
        moveOnly.moves << MoveOperation{QStringLiteral("alpha"), 3, 1};
        moveOnly.diffImage = solidImage(0, 255, 0);
        r.pages << moveOnly;

        PageDiff pixelOnly;
        pixelOnly.pageIndex = 2;
        pixelOnly.pixelDiffCount = 42;
        pixelOnly.diffImage = solidImage(0, 0, 255);
        r.pages << pixelOnly;

        PageDiff textAndPixel;
        textAndPixel.pageIndex = 3;
        textAndPixel.textAdded << QStringLiteral("omega");
        textAndPixel.pixelDiffCount = 7;
        textAndPixel.diffImage = solidImage(255, 255, 255);
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

    // ── U04: the one filtered change sequence drives everything ────────────

    // U04 acceptance: "Filtered counts and navigation must agree." The
    // widget's filtered change count must equal CompareMode's pure
    // rowsVisibleForFilters() seam for EVERY toggle combination, on both a
    // token-mixed and a structural-only fixture — so the tree and the
    // navigable sequence can never drift apart.
    void filteredChangeCountMatchesRowsVisibleForFiltersForAllCombinations()
    {
        CompareWidget widget;
        const DiffResult sample = makeSampleResult();
        const DiffResult structural = makeStructuralResult();

        for (int mask = 0; mask < 32; ++mask) {
            CompareChangeFilter f;
            f.showText          = mask & 1;
            f.showMove          = mask & 2;
            f.showPixel         = mask & 4;
            f.showPageMove      = mask & 8;
            f.showPageAddRemove = mask & 16;

            widget.setDiffResult(sample);
            widget.setChangeFilter(f);
            const int expected = gp::CompareMode::rowsVisibleForFilters(
                sample, f.showText, f.showMove, f.showPixel,
                f.showPageMove, f.showPageAddRemove);
            QVERIFY2(widget.changeCount() == expected,
                     qPrintable(QStringLiteral(
                                    "sample mask %1 (T%d M%d P%d PM%d AR%d): "
                                    "changeCount %2 != rowsVisibleForFilters %3")
                                    .arg(mask)
                                    .arg(f.showText).arg(f.showMove).arg(f.showPixel)
                                    .arg(f.showPageMove).arg(f.showPageAddRemove)
                                    .arg(widget.changeCount())
                                    .arg(expected)));

            widget.setDiffResult(structural);
            widget.setChangeFilter(f);
            const int expectedS = gp::CompareMode::rowsVisibleForFilters(
                structural, f.showText, f.showMove, f.showPixel,
                f.showPageMove, f.showPageAddRemove);
            QVERIFY2(widget.changeCount() == expectedS,
                     qPrintable(QStringLiteral(
                                    "structural mask %1: changeCount %2 != rowsVisible %3")
                                    .arg(mask)
                                    .arg(widget.changeCount())
                                    .arg(expectedS)));
        }
    }

    // Anchor order: structural changes lead the sequence (canonical
    // pageChanges order), then one entry per visible page row in pages order;
    // filtered-out changes are skipped and the survivors renumber
    // contiguously.
    void filteredAnchorOrderIsStructuralFirstThenVisiblePages()
    {
        CompareWidget widget;
        widget.setDiffResult(makeSampleResult());

        // All-on: [structural PageMoved(0→2), page0, page1, page2, page3].
        QCOMPARE(widget.changeCount(), 5);
        QCOMPARE(widget.anchorAt(0).structuralIndex, 0);
        QCOMPARE(widget.anchorAt(0).pageDiffIndex, -1);
        QCOMPARE(widget.anchorAt(0).oldPage, 0);   // R11 exact old/new sides
        QCOMPARE(widget.anchorAt(0).newPage, 2);
        for (int j = 1; j <= 4; ++j) {
            QCOMPARE(widget.anchorAt(j).structuralIndex, -1);
            QCOMPARE(widget.anchorAt(j).pageDiffIndex, j - 1);
            QCOMPARE(widget.anchorAt(j).oldPage, j - 1);
            QCOMPARE(widget.anchorAt(j).newPage, j - 1);
        }

        // Page-move gate off: the structural anchor disappears, page anchors
        // keep their relative order and renumber from 0.
        CompareChangeFilter noPageMove;
        noPageMove.showPageMove = false;
        widget.setChangeFilter(noPageMove);
        QCOMPARE(widget.changeCount(), 4);
        QCOMPARE(widget.anchorIndexForStructuralChange(0), -1);
        for (int j = 0; j < 4; ++j)
            QCOMPARE(widget.anchorAt(j).pageDiffIndex, j);

        // Add/remove + page-move off: only token page rows remain.
        CompareChangeFilter tokensOnly;
        tokensOnly.showPageMove = false;
        tokensOnly.showPageAddRemove = false;
        widget.setChangeFilter(tokensOnly);
        QCOMPARE(widget.changeCount(), 4);
        QCOMPARE(widget.anchorIndexForPage(2), 2);

        // Pixel-only off: page2's row drops, page3 survives on its text tag.
        CompareChangeFilter noPixel;
        noPixel.showPixel = false;
        widget.setChangeFilter(noPixel);
        QCOMPARE(widget.changeCount(), 4);
        QCOMPARE(widget.anchorIndexForPage(2), -1);
        QCOMPARE(widget.anchorIndexForPage(3), 3);
    }

    // U04 acceptance: "A zero-results filter says no changes match the filter
    // without claiming the files are identical." Pinned on the nav label, the
    // text diff panel and the toolbar status.
    void zeroFilterSaysNoMatchNeverIdentical()
    {
        CompareWidget widget;
        widget.setDiffResult(makeSampleResult());

        CompareChangeFilter none;
        none.showText = none.showMove = none.showPixel = false;
        none.showPageMove = none.showPageAddRemove = false;
        widget.setChangeFilter(none);
        QCOMPARE(widget.changeCount(), 0);

        auto* nav = widget.findChild<QLabel*>(QStringLiteral("cmpNavLabel"));
        QVERIFY2(nav, "CompareWidget nav label must carry objectName cmpNavLabel");
        QVERIFY2(nav->text().contains(QStringLiteral("match the filter"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("nav label: %1").arg(nav->text())));
        QVERIFY2(!nav->text().contains(QStringLiteral("identical"), Qt::CaseInsensitive),
                 qPrintable("zero-results filter must never claim the files are identical"));

        auto* browser = widget.findChild<QTextBrowser*>();
        QVERIFY2(browser, "CompareWidget must own its text diff browser");
        const QString panel = browser->toPlainText();
        QVERIFY2(panel.contains(QStringLiteral("match the filter"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("text panel: %1").arg(panel)));
        QVERIFY2(!panel.contains(QStringLiteral("identical"), Qt::CaseInsensitive),
                 "text panel must not claim identical on a zero-results filter");

        // The toolbar status mirrors the same rule (existing-API half).
        gp::CompareMode mode;
        mode.showDiffResult(makeSampleResult());
        for (const char* name : {"cmpFilterText", "cmpFilterMove", "cmpFilterPixel",
                                 "cmpFilterPageMove", "cmpFilterPageAddRemove"}) {
            auto* b = filterButton(mode, name);
            QVERIFY2(b, qPrintable(QStringLiteral("%1 not found").arg(name)));
            b->setChecked(false);
        }
        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY2(status, "status label must carry objectName cmpStatusLabel");
        QVERIFY2(status->text().contains(QStringLiteral("MATCH THE FILTER")),
                 qPrintable(QStringLiteral("status: %1").arg(status->text())));
        QVERIFY2(!status->text().contains(QStringLiteral("IDENTICAL")),
                 qPrintable("status must not claim IDENTICAL on a zero-results filter"));
    }

    // The pixel overlay follows the SELECTED change (replaces the old
    // pages.first() shortcut), and the overlay toggle remains the one owner:
    // switching it off clears whatever a change selection set.
    void overlayFollowsSelectedChangeAndClearsWithToggle()
    {
        CompareWidget widget;
        widget.setDiffResult(makeSampleResult());

        widget.setShowPixelDiff(true);
        // No selection yet: the overlay falls back to the FIRST change of the
        // shared sequence — the structural PageMoved (0→2) maps to the revised
        // page 3's diff image (pageIndex 2), not pages.first().
        QCOMPARE(widget.currentOverlayImage(), makeSampleResult().pages[2].diffImage);

        widget.showOverlayForChange(1);   // page 0 (text-only) anchor → red
        QCOMPARE(widget.currentOverlayImage(), makeSampleResult().pages[0].diffImage);
        widget.showOverlayForChange(2);   // page 1 (move-only) anchor → green
        QCOMPARE(widget.currentOverlayImage(), makeSampleResult().pages[1].diffImage);
        widget.showOverlayForChange(3);   // page 2 (pixel-only) anchor → blue
        QCOMPARE(widget.currentOverlayImage(), makeSampleResult().pages[2].diffImage);

        // Toggle off clears; a change selection while off cannot re-set it.
        widget.setShowPixelDiff(false);
        QVERIFY(widget.currentOverlayImage().isNull());
        widget.showOverlayForChange(1);
        QVERIFY(widget.currentOverlayImage().isNull());

        // Selection-driven: select page 1's anchor, re-enable → its image.
        widget.scrollToChange(2);
        widget.setShowPixelDiff(true);
        QCOMPARE(widget.currentOverlayImage(), makeSampleResult().pages[1].diffImage);
    }

    // Added/removed pages stay navigable with an explanatory placeholder on
    // the missing side (never a stale page); selecting an anchor whose page
    // exists on both sides clears both placeholders.
    void placeholdersExplainMissingSidesAndClearOnBothSideAnchors()
    {
        const QString base = createPagePdf("ph_base.pdf", {"First page"});
        const QString extended =
            createPagePdf("ph_ext.pdf", {"First page", "Appendix page"});
        QVERIFY(!base.isEmpty() && !extended.isEmpty());

        DiffEngine engine;
        const DiffResult added = engine.compare(base, extended);
        QCOMPARE(added.pageChanges.size(), 1);
        QCOMPARE(added.pageChanges.first().type, DiffResult::PageChangeType::PageAdded);

        CompareWidget widget;
        QVERIFY(widget.loadDocuments(base, extended));
        widget.setDiffResult(added);

        auto* leftPh  = widget.findChild<QLabel*>(QStringLiteral("cmpLeftPlaceholder"));
        auto* rightPh = widget.findChild<QLabel*>(QStringLiteral("cmpRightPlaceholder"));
        QVERIFY2(leftPh, "cmpLeftPlaceholder must exist");
        QVERIFY2(rightPh, "cmpRightPlaceholder must exist");
        QVERIFY(!leftPh->isVisibleTo(&widget));
        QVERIFY(!rightPh->isVisibleTo(&widget));

        // Select the added page: the ORIGINAL side has no page 2 — it must
        // explain itself instead of showing a stale page; the revised side
        // follows the change exactly.
        widget.scrollToChange(0);
        QVERIFY2(leftPh->isVisibleTo(&widget),
                 "added page must show the missing-side placeholder on the original viewer");
        QVERIFY(!rightPh->isVisibleTo(&widget));
        QVERIFY2(leftPh->text().contains(QStringLiteral("added in the revised document")),
                 qPrintable(QStringLiteral("left placeholder: %1").arg(leftPh->text())));

        auto* split = widget.findChild<QSplitter*>();
        QVERIFY2(split, "CompareWidget must own the side-by-side viewer splitter");
        auto* left  = qobject_cast<PdfViewerWidget*>(split->widget(0));
        auto* right = qobject_cast<PdfViewerWidget*>(split->widget(1));
        QVERIFY(left && right);
        QTRY_COMPARE(right->currentPage(), 1);
        QCOMPARE(left->currentPage(), 0);   // no bogus page navigation

        // A both-sides anchor (token change on page 1) clears the placeholder.
        DiffResult mixed = added;
        PageDiff token;
        token.pageIndex = 0;
        token.textAdded << QStringLiteral("extra");
        mixed.pages << token;
        widget.setDiffResult(mixed);
        QVERIFY(!leftPh->isVisibleTo(&widget));   // fresh result resets selection
        widget.scrollToChange(1);                 // page 0's token anchor
        QVERIFY(!leftPh->isVisibleTo(&widget));
        QVERIFY(!rightPh->isVisibleTo(&widget));

        // Removed page: the REVISED side explains itself.
        const DiffResult removed = engine.compare(extended, base);
        CompareWidget w2;
        QVERIFY(w2.loadDocuments(extended, base));
        w2.setDiffResult(removed);
        auto* rph2 = w2.findChild<QLabel*>(QStringLiteral("cmpRightPlaceholder"));
        auto* lph2 = w2.findChild<QLabel*>(QStringLiteral("cmpLeftPlaceholder"));
        QVERIFY(rph2 && lph2);
        w2.scrollToChange(0);
        QVERIFY2(rph2->isVisibleTo(&w2),
                 "removed page must show the missing-side placeholder on the revised viewer");
        QVERIFY(!lph2->isVisibleTo(&w2));
        QVERIFY2(rph2->text().contains(QStringLiteral("removed from the original document")),
                 qPrintable(QStringLiteral("right placeholder: %1").arg(rph2->text())));
    }

    // Linked scrolling maps the leader's scroll RATIO onto the follower
    // (page-index mapping), works both directions, and unlinking stops the
    // follow. Driven through the production seam (mapLinkedScroll) so the
    // assertions don't depend on offscreen layout.
    void linkedScrollMapsRatioToFollowerPage()
    {
        const QString three = createPagePdf("ls_three.pdf", {"p1", "p2", "p3"});
        const QString seven =
            createPagePdf("ls_seven.pdf", {"a", "b", "c", "d", "e", "f", "g"});
        QVERIFY(!three.isEmpty() && !seven.isEmpty());

        CompareWidget widget;
        QVERIFY(widget.loadDocuments(three, seven));
        QVERIFY2(widget.isLinkedScrolling(), "linked scrolling must default ON");
        auto* split = widget.findChild<QSplitter*>();
        QVERIFY(split);
        auto* left  = qobject_cast<PdfViewerWidget*>(split->widget(0));
        auto* right = qobject_cast<PdfViewerWidget*>(split->widget(1));
        QVERIFY(left && right);
        QCOMPARE(left->pageCount(), 3);
        QCOMPARE(right->pageCount(), 7);

        // Ratio 0.5 over 7 follower pages → page index 3 (int(0.5 * 7)).
        widget.mapLinkedScroll(left, 5, 10);
        QTRY_COMPARE(right->currentPage(), 3);
        // Ratio 0.9 → int(6.3) = 6.
        widget.mapLinkedScroll(left, 9, 10);
        QTRY_COMPARE(right->currentPage(), 6);
        // Reverse direction: ratio 0.5 over 3 follower pages → page 1.
        widget.mapLinkedScroll(right, 5, 10);
        QTRY_COMPARE(left->currentPage(), 1);

        // Unlinked: the follower stays put.
        widget.setLinkedScrolling(false);
        QVERIFY(!widget.isLinkedScrolling());
        const int rightBefore = right->currentPage();
        widget.mapLinkedScroll(left, 0, 10);
        QCOMPARE(right->currentPage(), rightBefore);

        // Relinking resumes the follow.
        widget.setLinkedScrolling(true);
        widget.mapLinkedScroll(left, 0, 10);
        QTRY_COMPARE(right->currentPage(), 0);
    }

    // The guard pins: one leader change moves the follower at most once and
    // never fights back into the leader (no ping-pong). Uses the REAL
    // scrollbar connections, so the widget is shown offscreen to get real
    // scroll ranges.
    void linkedScrollGuardPreventsPingPong()
    {
        const QString three = createPagePdf("g_three.pdf", {"p1", "p2", "p3"});
        const QString seven =
            createPagePdf("g_seven.pdf", {"a", "b", "c", "d", "e", "f", "g"});
        QVERIFY(!three.isEmpty() && !seven.isEmpty());

        CompareWidget widget;
        QVERIFY(widget.loadDocuments(three, seven));
        widget.resize(900, 700);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        auto* split = widget.findChild<QSplitter*>();
        QVERIFY(split);
        auto* left  = qobject_cast<PdfViewerWidget*>(split->widget(0));
        auto* right = qobject_cast<PdfViewerWidget*>(split->widget(1));
        QVERIFY(left && right);
        auto* leftBar =
            left->findChild<QPdfView*>(QStringLiteral("pdfView"))->verticalScrollBar();
        auto* rightBar =
            right->findChild<QPdfView*>(QStringLiteral("pdfView"))->verticalScrollBar();
        QVERIFY(leftBar && rightBar);
        QTRY_VERIFY(leftBar->maximum() > 0);
        QTRY_VERIFY(rightBar->maximum() > 0);
        QTest::qWait(100);   // let layout settle before snapshotting ranges

        const int leftMax = leftBar->maximum();
        const int rightMax = rightBar->maximum();
        QVERIFY(leftMax > 0 && rightMax > 0);

        // Real path: leader scrollbar moves → follower maps proportionally.
        leftBar->setValue(leftMax / 2);
        const qreal ratio = qreal(leftMax / 2) / leftMax;
        QTRY_COMPARE(rightBar->value(), qRound(ratio * rightMax));
        const int leftStable = leftBar->value();
        QTest::qWait(60);    // an unguarded echo would move the leader here
        QCOMPARE(leftBar->value(), leftStable);

        // Seam-driven: exactly one follower movement per leader change.
        QSignalSpy rightSpy(rightBar, &QScrollBar::valueChanged);
        widget.mapLinkedScroll(left, leftMax / 4, leftMax);
        QTRY_COMPARE(rightBar->value(),
                     qRound(qreal(leftMax / 4) / leftMax * rightMax));
        QTest::qWait(60);
        QVERIFY2(rightSpy.count() <= 1,
                 qPrintable(QStringLiteral(
                                "follower scrollbar moved %1 times for one "
                                "leader change — re-entrancy guard is broken")
                                .arg(rightSpy.count())));
    }

    // Status counter: the filtered change count IS the status total, and it
    // moves live with the toggles. (Existing-API test — pins the CompareMode
    // wiring without touching the new widget API.)
    void statusCounterMatchesFilteredChangeCount()
    {
        gp::CompareMode mode;
        mode.showDiffResult(makeSampleResult());
        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY2(status, "status label must carry objectName cmpStatusLabel");
        QVERIFY2(status->text().contains(QStringLiteral("5 CHANGES")),
                 qPrintable(QStringLiteral("initial status: %1").arg(status->text())));

        auto* pixelBtn = filterButton(mode, "cmpFilterPixel");
        QVERIFY(pixelBtn);
        pixelBtn->setChecked(false);
        QVERIFY2(status->text().contains(QStringLiteral("4 CHANGES")),
                 qPrintable(QStringLiteral("status after pixel filter: %1").arg(status->text())));
        pixelBtn->setChecked(true);
        QVERIFY2(status->text().contains(QStringLiteral("5 CHANGES")),
                 qPrintable(QStringLiteral("status after re-check: %1").arg(status->text())));

        gp::CompareMode mode2;
        mode2.showDiffResult(makeStructuralResult());
        auto* status2 = mode2.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY(status2);
        QVERIFY2(status2->text().contains(QStringLiteral("3 CHANGES")),
                 qPrintable(QStringLiteral("structural status: %1").arg(status2->text())));
        auto* addRmBtn = filterButton(mode2, "cmpFilterPageAddRemove");
        QVERIFY(addRmBtn);
        addRmBtn->setChecked(false);
        QVERIFY2(status2->text().contains(QStringLiteral("1 CHANGES")),
                 qPrintable(QStringLiteral("structural status filtered: %1").arg(status2->text())));
    }

    // EVERY CHANGES tree row — token page rows included, not just structural
    // rows — maps into the filtered shared sequence via kAnchorIndexRole, and
    // selecting it navigates that same sequence. Filtering re-maps the roles
    // (hidden rows drop to -1; survivors renumber).
    void everyVisibleTreeRowMapsIntoFilteredSharedSequence()
    {
        gp::CompareMode mode;
        mode.showDiffResult(makeSampleResult());
        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY2(tree, "CHANGES tree must carry objectName cmpChangesTree");
        QCOMPARE(tree->topLevelItemCount(), 5);

        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            const QVariant anchor = tree->topLevelItem(i)->data(0, gp::CompareMode::kAnchorIndexRole);
            QVERIFY2(anchor.isValid() && anchor.toInt() >= 0,
                     qPrintable(QStringLiteral("tree row %1 must map into the shared sequence")
                                    .arg(i)));
        }

        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY2(widget, "CompareMode must own a CompareWidget");
        auto* nav = widget->findChild<QLabel*>(QStringLiteral("cmpNavLabel"));
        QVERIFY2(nav, "CompareWidget nav label must carry objectName cmpNavLabel");

        // Tree row 2 is the pixel-only page (p.3) — the 4th change of the
        // shared sequence (structural first). Selecting it must drive the
        // sequence, not sit still.
        tree->setCurrentItem(tree->topLevelItem(2));
        QVERIFY2(nav->text().contains(QStringLiteral("change 4 of 5")),
                 qPrintable(QStringLiteral("nav label after page-row selection: %1").arg(nav->text())));

        // Text off: the text-only row hides and drops to -1; the pixel row
        // re-numbers from 3 to 2 in the 4-change sequence.
        filterButton(mode, "cmpFilterText")->setChecked(false);
        QVERIFY(tree->topLevelItem(0)->isHidden());
        QCOMPARE(tree->topLevelItem(0)->data(0, gp::CompareMode::kAnchorIndexRole).toInt(), -1);
        QCOMPARE(tree->topLevelItem(2)->data(0, gp::CompareMode::kAnchorIndexRole).toInt(), 2);
        QCOMPARE(widget->changeCount(), 4);

        // Re-checking restores the mapping.
        filterButton(mode, "cmpFilterText")->setChecked(true);
        QVERIFY(!tree->topLevelItem(0)->isHidden());
        QCOMPARE(tree->topLevelItem(0)->data(0, gp::CompareMode::kAnchorIndexRole).toInt(), 1);
        QCOMPARE(widget->changeCount(), 5);
    }

    // Export honors the same selected scope and filter state the UI
    // describes: filtered-out changes drop from the report, and a zero-scope
    // report says "no changes match" — never "identical".
    void exportReportsHonorCurrentFilter()
    {
        gp::CompareMode mode;
        mode.showDiffResult(makeSampleResult());

        // Backward compatibility: the no-arg overloads still produce the full
        // report for the default (all-on) filter.
        QVERIFY2(mode.buildTextReport().contains(QStringLiteral("gamma")),
                 "full text report must contain the text-only page tokens");
        QVERIFY2(mode.buildHtmlReport().contains(QStringLiteral("gamma")),
                 "full html report must contain the text-only page tokens");

        CompareChangeFilter noText;
        noText.showText = false;
        const QString txt = mode.buildTextReport(noText);
        QVERIFY2(!txt.contains(QStringLiteral("gamma")) && !txt.contains(QStringLiteral("delta")),
                 qPrintable(QStringLiteral("text-filtered report leaked text-only tokens: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("alpha")),
                 "move content stays in scope with text off");
        QVERIFY2(txt.contains(QStringLiteral("pixels differ")),
                 "pixel note stays in scope with text off");
        // The sample fixture's structural PageMoved is (old 0 → new 2).
        QVERIFY2(txt.contains(QStringLiteral("Page 1 moved to position 3")),
                 "structural entries stay in scope with text off");

        const QString html = mode.buildHtmlReport(noText);
        QVERIFY2(!html.contains(QStringLiteral("gamma")),
                 "html-filtered report leaked text-only tokens");
        QVERIFY2(html.contains(QStringLiteral("alpha")),
                 "html report keeps move content in scope");

        CompareChangeFilter none;
        none.showText = none.showMove = none.showPixel = false;
        none.showPageMove = none.showPageAddRemove = false;
        const QString emptyTxt = mode.buildTextReport(none);
        QVERIFY2(emptyTxt.contains(QStringLiteral("no changes match the filter"),
                                   Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("empty text report: %1").arg(emptyTxt)));
        QVERIFY2(!emptyTxt.contains(QStringLiteral("identical"), Qt::CaseInsensitive),
                 "empty report must never claim the files are identical");
        const QString emptyHtml = mode.buildHtmlReport(none);
        QVERIFY2(emptyHtml.contains(QStringLiteral("no changes match the filter"),
                                    Qt::CaseInsensitive),
                 qPrintable("empty html report must say no changes match"));
        QVERIFY2(!emptyHtml.contains(QStringLiteral("identical"), Qt::CaseInsensitive),
                 "empty html report must never claim identical");
    }

    // Toolbar: linked-scroll toggle (on by default, drives the widget seam),
    // swap action, and the Old/New files label surfacing page counts.
    void toolbarSurfacesLinkToggleSwapAndPageCounts()
    {
        const QString a = createPagePdf("tb_a.pdf", {"a1", "a2", "a3", "a4"});
        const QString b = createPagePdf("tb_b.pdf", {"b1", "b2", "b3", "b4"});
        QVERIFY(!a.isEmpty() && !b.isEmpty());

        gp::CompareMode mode;
        auto* link = filterButton(mode, "cmpBtnLinkScroll");
        QVERIFY2(link, "cmpBtnLinkScroll toggle must exist");
        QVERIFY(link->isCheckable() && link->isChecked());
        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY2(widget, "CompareMode must own a CompareWidget");
        QVERIFY(widget->isLinkedScrolling());
        link->setChecked(false);
        QVERIFY(!widget->isLinkedScrolling());
        link->setChecked(true);
        QVERIFY(widget->isLinkedScrolling());

        auto* swap = mode.findChild<QToolButton*>(QStringLiteral("cmpSwapButton"));
        QVERIFY2(swap, "cmpSwapButton must exist");

        auto* files = mode.findChild<QLabel*>(QStringLiteral("cmpFilesLabel"));
        QVERIFY2(files, "files label must carry objectName cmpFilesLabel");
        mode.compareFiles(a, b);
        const QString label = files->text();
        QVERIFY2(label.contains(QStringLiteral("tb_a.pdf")) && label.contains(QStringLiteral("tb_b.pdf")),
                 qPrintable(QStringLiteral("files label: %1").arg(label)));
        QVERIFY2(label.contains(QStringLiteral("(4 pp)")),
                 qPrintable(QStringLiteral("files label must surface page counts: %1").arg(label)));

        // Swap re-runs the comparison with the sides exchanged (enabled once
        // the running comparison finishes).
        QTRY_VERIFY2(swap->isEnabled(), "swap re-enables once the comparison finishes");
        swap->click();
        const QString swapped = files->text();
        QVERIFY2(swapped.indexOf(QStringLiteral("tb_b.pdf"))
                     < swapped.indexOf(QStringLiteral("tb_a.pdf")),
                 qPrintable(QStringLiteral("swapped label: %1").arg(swapped)));
        QTRY_VERIFY(!mode.isBusy());   // clean shutdown before the test ends
    }
};

QTEST_MAIN(TestCompareEntry)
#include "TestCompareEntry.moc"
