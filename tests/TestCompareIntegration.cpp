// SPDX-License-Identifier: Apache-2.0
// §9.10 P1 — compare INTEGRATION tests end-to-end. The July plan noted only
// the isolated Myers algorithm was tested; R11/U04 landed pageChanges,
// filter-aware anchors and the CompareWidget integration with their own
// (mostly synthetic-fixture) tests. This file closes the remaining gap:
// every test here drives the REAL DiffEngine (real PDFs on disk through the
// pdfium text extraction + pixel pipeline) feeding the REAL CompareWidget and
// the REAL CompareMode entry point (compareFiles' async watcher path), and
// asserts only through public seams:
//   - DiffResult via CompareMode::lastResult()
//   - CHANGES tree rows + CompareMode data roles (objectName cmpChangesTree)
//   - CompareWidget::changeCount()/anchorAt()/nextChange()/prevChange()
//   - PdfViewerWidget::currentPage()/pageCount() through the splitter
//   - report builders buildTextReport()/buildHtmlReport() (+ filter overloads)
// Hand-built N-page PDFs use TestCompareEntry::createPagePdf's idiom: QPdfWriter
// embeds subset fonts that extract as garbage, so DiffEngine-driven fixtures
// need raw string literals (an empty string yields a blank page).
//
// Page-alignment note (characterization): the engine aligns two pages as
// "same" when their word-set similarity is >= 0.80, so the text-edit fixture
// changes one word of a six-word page (similarity 5/6 ≈ 0.83 — stays aligned,
// no structural noise), while the trailing-page fixture adds a page sharing no
// words with any existing page (a clean PageAdded).
#include <QtTest>
#include <QLabel>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QToolButton>

#include "engines/DiffEngine.h"
#include "modes/CompareMode.h"
#include "ui/CompareWidget.h"
#include "ui/PdfViewerWidget.h"  // viewer assertions need the complete type

namespace {

// Hand-built real N-page PDF (TestCompareEntry idiom): deterministic bytes,
// one Helvetica string per page, a blank page for an empty string.
QString createPagePdf(const QString& path, const QStringList& pageTexts)
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

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(pdf);
    return path;
}

} // namespace

class TestCompareIntegration : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString pagePdf(const QString& name, const QStringList& pageTexts)
    {
        const QString path = m_dir.filePath(name);
        const QString written = createPagePdf(path, pageTexts);
        if (written.isEmpty())
            qWarning("failed to write %s", qPrintable(path));
        return written;
    }

    // compareFiles() runs DiffEngine::compare on a QtConcurrent thread; the
    // first post-finish observable is the status label leaving "COMPARING...".
    void waitForDiffFinished(gp::CompareMode& mode)
    {
        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY2(status, "status label must carry objectName cmpStatusLabel");
        QTRY_VERIFY2(!status->text().contains(QStringLiteral("COMPARING")),
                     qPrintable(QStringLiteral("diff never finished, status: %1")
                                    .arg(status->text())));
    }

    static QSplitter* viewerSplitter(CompareWidget& widget)
    {
        // nullptr on failure — call sites QVERIFY2 the result (QVERIFY may
        // only be used in void functions).
        return widget.findChild<QSplitter*>();
    }

private slots:
    void initTestCase() { QVERIFY2(m_dir.isValid(), "temporary dir must be valid"); }

    // ── (a) two identical documents → no changes, isIdentical ────────────────

    void identicalDocumentsProduceNoChanges()
    {
        const QStringList texts = {
            QStringLiteral("alpha bravo charlie delta echo foxtrot"),
            QStringLiteral("second page"),
            QStringLiteral("third page")};
        const QString a = pagePdf("ident_a.pdf", texts);
        const QString b = pagePdf("ident_b.pdf", texts);   // deterministic bytes
        QVERIFY(!a.isEmpty() && !b.isEmpty());

        // Real engine, direct: identical bytes short-circuit to isIdentical.
        DiffEngine engine;
        const DiffResult r = engine.compare(a, b);
        QVERIFY2(r.isIdentical, "identical documents must be flagged identical");
        QCOMPARE(r.pages.size(), 0);
        QCOMPARE(r.pageChanges.size(), 0);

        // Real mode, end-to-end through the async compareFiles path.
        gp::CompareMode mode;
        mode.compareFiles(a, b);
        waitForDiffFinished(mode);

        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY(status);
        QVERIFY2(status->text().contains(QStringLiteral("FILES ARE IDENTICAL")),
                 qPrintable(QStringLiteral("status: %1").arg(status->text())));

        QVERIFY(mode.lastResult().isIdentical);

        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY2(widget, "CompareMode must own a CompareWidget");
        QCOMPARE(widget->changeCount(), 0);

        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY2(tree, "CHANGES tree must carry objectName cmpChangesTree");
        QCOMPARE(tree->topLevelItemCount(), 0);

        // PREV/NEXT stay disabled when there is nothing to navigate to.
        QToolButton* prev = nullptr;
        QToolButton* next = nullptr;
        const auto buttons = mode.findChildren<QToolButton*>();
        for (auto* btn : buttons) {
            if (btn->text().contains(QStringLiteral("PREV"))) prev = btn;
            if (btn->text().contains(QStringLiteral("NEXT"))) next = btn;
        }
        QVERIFY2(prev && next, "PREV/NEXT buttons must exist");
        QVERIFY2(!prev->isEnabled(), "PREV must stay disabled on identical files");
        QVERIFY2(!next->isEnabled(), "NEXT must stay disabled on identical files");

        // The full report says identical (never a fabricated change list).
        QVERIFY2(mode.buildTextReport().contains(
                     QStringLiteral("The documents are identical.")),
                 qPrintable(QStringLiteral("text report: %1").arg(mode.buildTextReport())));
        QVERIFY2(mode.buildHtmlReport().contains(
                     QStringLiteral("The documents are identical.")),
                 "html report must say the documents are identical");
    }

    // ── (b) text edit on one page → text-diff section + correct anchor ───────

    void textEditOnOnePageYieldsTextSectionAndAnchor()
    {
        // One word of ten changes: the page-alignment word-set similarity is
        // 9/11 ≈ 0.82 >= 0.80, so the page stays ALIGNED (no structural rows)
        // while the token diff still fires. (With only six words the union
        // 5/7 ≈ 0.71 would fall under the threshold and the edited page would
        // legitimately be reported as removed+added — pinned by design in
        // TestDiffEngine's alignment contract, not a defect.)
        const QString base = pagePdf("edit_base.pdf", {
            QStringLiteral("first page"),
            QStringLiteral("alpha bravo charlie delta echo foxtrot golf hotel india juliet"),
            QStringLiteral("third page")});
        const QString revised = pagePdf("edit_rev.pdf", {
            QStringLiteral("first page"),
            QStringLiteral("alpha bravo charlie delta echo foxtrot gulf hotel india juliet"),
            QStringLiteral("third page")});
        QVERIFY(!base.isEmpty() && !revised.isEmpty());

        gp::CompareMode mode;
        mode.compareFiles(base, revised);
        waitForDiffFinished(mode);

        // Engine-level result: exactly one changed page, token-level only.
        const DiffResult& r = mode.lastResult();
        QVERIFY2(!r.isIdentical, "an edited document is not identical");
        QCOMPARE(r.pageCount1, 3);
        QCOMPARE(r.pageCount2, 3);
        QCOMPARE(r.pageChanges.size(), 0);   // alignment held — no structural rows
        QCOMPARE(r.pages.size(), 3);
        QVERIFY2(r.pages.at(0).textAdded.isEmpty()
                     && r.pages.at(0).textRemoved.isEmpty()
                     && r.pages.at(0).pixelDiffCount == 0,
                 "page 1 is untouched");
        QCOMPARE(r.pages.at(1).textAdded, QStringList{QStringLiteral("gulf")});
        QCOMPARE(r.pages.at(1).textRemoved, QStringList{QStringLiteral("golf")});
        QVERIFY2(r.pages.at(1).pixelDiffCount > 0,
                 "the rendered page with different text must differ in pixels");
        QVERIFY2(r.pages.at(2).textAdded.isEmpty()
                     && r.pages.at(2).textRemoved.isEmpty()
                     && r.pages.at(2).pixelDiffCount == 0,
                 "page 3 is untouched");

        // CHANGES tree: exactly the page-2 row.
        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(1), QStringLiteral("p.2"));
        QVERIFY(tree->topLevelItem(0)->data(0, gp::CompareMode::kHasTextRole).toBool());

        // Widget: one change in the shared sequence, anchored on page 2 of
        // BOTH sides (a token change exists on both sides of the page).
        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY(widget);
        QCOMPARE(widget->changeCount(), 1);
        const auto anchor = widget->anchorAt(0);
        QCOMPARE(anchor.structuralIndex, -1);
        QCOMPARE(anchor.pageDiffIndex, 1);
        QCOMPARE(anchor.oldPage, 1);
        QCOMPARE(anchor.newPage, 1);

        // The text-diff section shows the added and removed tokens.
        auto* browser = widget->findChild<QTextBrowser*>();
        QVERIFY2(browser, "CompareWidget must own its text diff browser");
        QVERIFY2(browser->toPlainText().contains(QStringLiteral("gulf")),
                 qPrintable(QStringLiteral("text panel: %1").arg(browser->toPlainText())));
        QVERIFY2(browser->toPlainText().contains(QStringLiteral("golf")),
                 qPrintable(QStringLiteral("text panel: %1").arg(browser->toPlainText())));
        QVERIFY2(browser->toPlainText().contains(QStringLiteral("Page 2")),
                 "the text-diff section must name the changed page");

        // Navigating the sequence lands both viewers on the changed page 2
        // (index 1) — a both-sides anchor moves both views.
        auto* nav = widget->findChild<QLabel*>(QStringLiteral("cmpNavLabel"));
        QVERIFY2(nav, "nav label must carry objectName cmpNavLabel");
        widget->nextChange();
        QVERIFY2(nav->text().contains(QStringLiteral("change 1 of 1")),
                 qPrintable(QStringLiteral("nav label: %1").arg(nav->text())));
        auto* split = viewerSplitter(*widget);
        QVERIFY2(split, "CompareWidget must own the side-by-side viewer splitter");
        auto* left  = qobject_cast<PdfViewerWidget*>(split->widget(0));
        auto* right = qobject_cast<PdfViewerWidget*>(split->widget(1));
        QVERIFY(left && right);
        QCOMPARE(left->pageCount(), 3);
        QCOMPARE(right->pageCount(), 3);
        QTRY_COMPARE(left->currentPage(), 1);
        QTRY_COMPARE(right->currentPage(), 1);

        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY(status);
        // After a navigation the toolbar status tracks the shared-sequence
        // position (CHANGE x OF y); the total form ("N CHANGES") is pinned in
        // filteredExportHonorsChangeTypeFilter().
        QVERIFY2(status->text().contains(QStringLiteral("CHANGE 1 OF 1")),
                 qPrintable(QStringLiteral("status: %1").arg(status->text())));
    }

    // ── (c) trailing page added → structural anchor + next/prev + viewers ────

    void trailingPageAddedNavigatesToStructuralAnchor()
    {
        const QString base = pagePdf("add_base.pdf", {
            QStringLiteral("first page"),
            QStringLiteral("second page")});
        const QString extended = pagePdf("add_ext.pdf", {
            QStringLiteral("first page"),
            QStringLiteral("second page"),
            QStringLiteral("appendix alpha")});   // shares no words → clean PageAdded
        QVERIFY(!base.isEmpty() && !extended.isEmpty());

        // Real engine, direct: the trailing page is a structural PageAdded.
        DiffEngine engine;
        const DiffResult r = engine.compare(base, extended);
        QCOMPARE(r.pageChanges.size(), 1);
        QCOMPARE(r.pageChanges.first().type, DiffResult::PageChangeType::PageAdded);
        QCOMPARE(r.pageChanges.first().oldPage, -1);
        QCOMPARE(r.pageChanges.first().newPage, 2);

        // Real mode, end-to-end.
        gp::CompareMode mode;
        mode.compareFiles(base, extended);
        waitForDiffFinished(mode);

        QCOMPARE(mode.lastResult().pageChanges.size(), 1);
        QCOMPARE(mode.lastResult().pages.size(), 2);   // min(base, ext)

        auto* tree = mode.findChild<QTreeWidget*>(QStringLiteral("cmpChangesTree"));
        QVERIFY(tree);
        QCOMPARE(tree->topLevelItemCount(), 1);
        QTreeWidgetItem* added = tree->topLevelItem(0);
        QVERIFY2(added->text(2).contains(QStringLiteral("Page 3 added in revised document")),
                 qPrintable(QStringLiteral("tree row: %1").arg(added->text(2))));
        QVERIFY(added->data(0, gp::CompareMode::kIsPageAddRemoveRole).toBool());
        QCOMPARE(added->data(0, gp::CompareMode::kAnchorIndexRole).toInt(), 0);

        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY(widget);
        QCOMPARE(widget->changeCount(), 1);
        const auto anchor = widget->anchorAt(0);
        QCOMPARE(anchor.structuralIndex, 0);
        QCOMPARE(anchor.pageDiffIndex, -1);
        QCOMPARE(anchor.oldPage, -1);   // missing side, never page-0 sentinel
        QCOMPARE(anchor.newPage, 2);

        // Selecting the structural CHANGES tree row navigates the shared
        // sequence (CompareMode wiring, not just the raw widget).
        auto* nav = widget->findChild<QLabel*>(QStringLiteral("cmpNavLabel"));
        QVERIFY(nav);
        tree->setCurrentItem(added);
        QVERIFY2(nav->text().contains(QStringLiteral("change 1 of 1")),
                 qPrintable(QStringLiteral("nav after tree selection: %1").arg(nav->text())));

        // next/prev wrap on a one-change sequence — the change stays selected
        // and the viewers stay on the pages the change names.
        widget->prevChange();
        QVERIFY2(nav->text().contains(QStringLiteral("change 1 of 1")),
                 qPrintable(QStringLiteral("nav after prev: %1").arg(nav->text())));
        widget->nextChange();
        QVERIFY2(nav->text().contains(QStringLiteral("change 1 of 1")),
                 qPrintable(QStringLiteral("nav after next: %1").arg(nav->text())));

        // Viewers end on the right pages: the revised side follows the change
        // to the added page 3 (index 2); the original side has no page 3 — it
        // stays put (no bogus navigation) and explains itself with the
        // missing-side placeholder.
        auto* split = viewerSplitter(*widget);
        QVERIFY2(split, "CompareWidget must own the side-by-side viewer splitter");
        auto* left  = qobject_cast<PdfViewerWidget*>(split->widget(0));
        auto* right = qobject_cast<PdfViewerWidget*>(split->widget(1));
        QVERIFY(left && right);
        QCOMPARE(left->pageCount(), 2);
        QCOMPARE(right->pageCount(), 3);
        QTRY_COMPARE(right->currentPage(), 2);
        QCOMPARE(left->currentPage(), 0);

        auto* leftPh = widget->findChild<QLabel*>(QStringLiteral("cmpLeftPlaceholder"));
        QVERIFY2(leftPh, "cmpLeftPlaceholder must exist");
        QVERIFY2(leftPh->isVisibleTo(widget),
                 "an added page must show the missing-side placeholder on the original viewer");
        QVERIFY2(leftPh->text().contains(QStringLiteral("added in the revised document")),
                 qPrintable(QStringLiteral("left placeholder: %1").arg(leftPh->text())));
    }

    // ── (d) export report contains the changed page references ───────────────

    void exportReportsContainChangedPageReferences()
    {
        // Real combined scenario: a token edit on page 1 AND a trailing page.
        // (Ten words so the edited page stays word-set aligned: 9/11 ≈ 0.82.)
        const QString base = pagePdf("rep_base.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot golf hotel india juliet"),
            QStringLiteral("second page")});
        const QString revised = pagePdf("rep_rev.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot gulf hotel india juliet"),
            QStringLiteral("second page"),
            QStringLiteral("appendix alpha")});
        QVERIFY(!base.isEmpty() && !revised.isEmpty());

        gp::CompareMode mode;
        mode.compareFiles(base, revised);
        waitForDiffFinished(mode);

        // Sanity: one token page row + one structural added page.
        QCOMPARE(mode.lastResult().pageChanges.size(), 1);
        QCOMPARE(mode.lastResult().pages.size(), 2);
        QVERIFY(mode.lastResult().pages.at(0).pixelDiffCount > 0
                || !mode.lastResult().pages.at(0).textAdded.isEmpty());

        const QString txt = mode.buildTextReport();
        QVERIFY2(txt.contains(QStringLiteral("Page 3 added in revised document")),
                 qPrintable(QStringLiteral("structural reference missing: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("--- Page 1 ---")),
                 qPrintable(QStringLiteral("token page reference missing: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("+ gulf")),
                 qPrintable(QStringLiteral("added token missing: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("- golf")),
                 qPrintable(QStringLiteral("removed token missing: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("2 -> 3")),
                 qPrintable(QStringLiteral("page-count transition missing: %1").arg(txt)));

        const QString html = mode.buildHtmlReport();
        QVERIFY2(html.contains(QStringLiteral("Page 3 added in revised document")),
                 "html report must name the added page");
        QVERIFY2(html.contains(QStringLiteral("gulf")) && html.contains(QStringLiteral("golf")),
                 "html report must carry the edited page's tokens");
        QVERIFY2(html.contains(QStringLiteral("<h2>Page 1</h2>")),
                 "html report must carry a section for the changed page");
    }

    // ── (e) filter on → export honors it ─────────────────────────────────────

    void filteredExportHonorsChangeTypeFilter()
    {
        const QString base = pagePdf("flt_base.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot golf hotel india juliet"),
            QStringLiteral("second page")});
        const QString revised = pagePdf("flt_rev.pdf", {
            QStringLiteral("alpha bravo charlie delta echo foxtrot gulf hotel india juliet"),
            QStringLiteral("second page"),
            QStringLiteral("appendix alpha")});
        QVERIFY(!base.isEmpty() && !revised.isEmpty());

        gp::CompareMode mode;
        mode.compareFiles(base, revised);
        waitForDiffFinished(mode);

        // Backward-compatible no-arg overloads: the full (all-on) report.
        QVERIFY2(mode.buildTextReport().contains(QStringLiteral("gulf")),
                 "full text report keeps the token page");
        QVERIFY2(mode.buildTextReport().contains(QStringLiteral("Page 3 added in revised document")),
                 "full text report keeps the structural entry");

        // Pure overload: hide added/removed pages → structural entry drops,
        // the token page row stays.
        CompareChangeFilter noAddRemove;
        noAddRemove.showPageAddRemove = false;
        const QString txt = mode.buildTextReport(noAddRemove);
        QVERIFY2(!txt.contains(QStringLiteral("added in revised document")),
                 qPrintable(QStringLiteral("filtered text report leaked the added page: %1").arg(txt)));
        QVERIFY2(txt.contains(QStringLiteral("+ gulf")),
                 "token page row stays in scope when only add/rm is gated off");

        const QString html = mode.buildHtmlReport(noAddRemove);
        QVERIFY2(!html.contains(QStringLiteral("added in revised document")),
                 "filtered html report leaked the added page");
        QVERIFY2(html.contains(QStringLiteral("gulf")),
                 "html report keeps the token page in scope");

        // Everything off: the report says no changes match — never "identical".
        CompareChangeFilter none;
        none.showText = none.showMove = none.showPixel = false;
        none.showPageMove = none.showPageAddRemove = false;
        const QString emptyTxt = mode.buildTextReport(none);
        QVERIFY2(emptyTxt.contains(QStringLiteral("No changes match the filter.")),
                 qPrintable(QStringLiteral("empty text report: %1").arg(emptyTxt)));
        QVERIFY2(!emptyTxt.contains(QStringLiteral("identical"), Qt::CaseInsensitive),
                 "an emptied report must never claim the files are identical");
        const QString emptyHtml = mode.buildHtmlReport(none);
        QVERIFY2(emptyHtml.contains(QStringLiteral("No changes match the filter.")),
                 "empty html report must say no changes match");
        QVERIFY2(!emptyHtml.contains(QStringLiteral("identical"), Qt::CaseInsensitive),
                 "empty html report must never claim identical");

        // UI path: the filter toggles ARE the export scope (onExportReport
        // exports currentFilter()). The widget's changeFilter() is the public
        // observable of that state, and the status total tracks it live.
        auto* addRmBtn = mode.findChild<QToolButton*>(QStringLiteral("cmpFilterPageAddRemove"));
        QVERIFY2(addRmBtn, "cmpFilterPageAddRemove toggle must exist");
        addRmBtn->setChecked(false);

        auto* widget = mode.findChild<CompareWidget*>();
        QVERIFY(widget);
        QVERIFY2(!widget->changeFilter().showPageAddRemove,
                 "the widget must observe the same filter state as the toggles");
        QCOMPARE(widget->changeCount(), 1);   // only the token page row survives

        auto* status = mode.findChild<QLabel*>(QStringLiteral("cmpStatusLabel"));
        QVERIFY(status);
        QVERIFY2(status->text().contains(QStringLiteral("1 CHANGES")),
                 qPrintable(QStringLiteral("status after add/rm filter: %1").arg(status->text())));

        // What export would now write: no added page, token page still there.
        const QString exported = mode.buildTextReport(widget->changeFilter());
        QVERIFY2(!exported.contains(QStringLiteral("added in revised document")),
                 "export scope must drop the gated structural row");
        QVERIFY2(exported.contains(QStringLiteral("+ gulf")),
                 "export scope must keep the surviving token row");

        // All five toggles off → zero-scope state, honored end-to-end.
        for (const char* name : {"cmpFilterText", "cmpFilterMove", "cmpFilterPixel",
                                 "cmpFilterPageMove", "cmpFilterPageAddRemove"}) {
            auto* b = mode.findChild<QToolButton*>(QString::fromLatin1(name));
            QVERIFY2(b, qPrintable(QStringLiteral("%1 not found").arg(name)));
            b->setChecked(false);
        }
        QCOMPARE(widget->changeCount(), 0);
        QVERIFY2(status->text().contains(QStringLiteral("NO CHANGES MATCH THE FILTER")),
                 qPrintable(QStringLiteral("status fully filtered: %1").arg(status->text())));
        QVERIFY2(mode.buildTextReport(widget->changeFilter())
                     .contains(QStringLiteral("No changes match the filter.")),
                 "zero-scope export says no changes match");

        // Restore the shipped defaults.
        for (const char* name : {"cmpFilterText", "cmpFilterMove", "cmpFilterPixel",
                                 "cmpFilterPageMove", "cmpFilterPageAddRemove"}) {
            auto* b = mode.findChild<QToolButton*>(QString::fromLatin1(name));
            QVERIFY(b);
            b->setChecked(true);
        }
        QCOMPARE(widget->changeCount(), 2);
    }
};

QTEST_MAIN(TestCompareIntegration)
#include "TestCompareIntegration.moc"
