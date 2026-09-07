#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <algorithm>
#include "engines/MyersDiff.h"
#include "engines/DiffEngine.h"

// ── R11 fixture: hand-built N-page text PDF ────────────────────────────────────
// Same idiom as TestExportPathBadge::createTextPdf, extended to N pages: one
// "BT /F1 12 Tf 72 720 Td (<text>) Tj ET" content stream per page, byte-exact
// xref. PDFium extracts the raw string literal, so DiffEngine's word diff and
// page fingerprints see real text. An empty string yields a page with an empty
// content stream (a genuinely blank page — still extractable as "").
static QString createPagePdf(const QString& dir, const QString& name,
                             const QStringList& pageTexts) {
    const int n = pageTexts.size();
    QByteArray pdf = "%PDF-1.4\n";
    QList<qint64> offsets;
    // Object layout: 1 catalog, 2 pages tree, page k at 3+2k, its content at
    // 4+2k, font at 3+2n. Object numbers stay dense (blank pages get an empty
    // stream object) so the xref stays byte-exact.
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
    const int objCount = 4 + 2 * n;  // objects 0 .. (3+2n)
    pdf += "xref\n0 " + QByteArray::number(objCount) + "\n0000000000 65535 f \n";
    for (qint64 off : offsets)
        pdf += QByteArray::number(static_cast<qulonglong>(off)).rightJustified(10, '0')
               + " 00000 n \n";
    pdf += "trailer<</Size " + QByteArray::number(objCount) + "/Root 1 0 R>>\nstartxref\n"
         + QByteArray::number(xrefOffset) + "\n%%EOF\n";

    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(pdf);
    return path;
}

class TestDiffEngine : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:

    void initTestCase() {
        QVERIFY2(m_dir.isValid(), "temporary fixture directory must be usable");
    }


    // ── Myers LCS correctness ─────────────────────────────────────────────

    void testMyersEmptyBoth() {
        const auto ops = MyersDiff::compute({}, {});
        QVERIFY(ops.isEmpty());
    }

    void testMyersEmptyA() {
        const QStringList b = {"x", "y"};
        const auto ops = MyersDiff::compute({}, b);
        QCOMPARE(ops.size(), 2);
        QCOMPARE(ops[0].type, EditOp::Type::Insert);
        QCOMPARE(ops[1].type, EditOp::Type::Insert);
        QCOMPARE(ops[0].token, QString("x"));
        QCOMPARE(ops[1].token, QString("y"));
    }

    void testMyersEmptyB() {
        const QStringList a = {"a", "b"};
        const auto ops = MyersDiff::compute(a, {});
        QCOMPARE(ops.size(), 2);
        for (const auto& op : ops)
            QCOMPARE(op.type, EditOp::Type::Delete);
    }

    void testMyersIdentical() {
        const QStringList seq = {"The", "quick", "brown", "fox"};
        const auto ops = MyersDiff::compute(seq, seq);
        for (const auto& op : ops)
            QCOMPARE(op.type, EditOp::Type::Keep);
        QCOMPARE(ops.size(), 4);
    }

    void testMyersOneInsert() {
        // a = [A, B, C]  b = [A, X, B, C]  — X inserted at index 1
        const QStringList a = {"A", "B", "C"};
        const QStringList b = {"A", "X", "B", "C"};
        const auto ops = MyersDiff::compute(a, b);
        int inserts = 0, keeps = 0, deletes = 0;
        for (const auto& op : ops) {
            if (op.type == EditOp::Type::Insert) ++inserts;
            else if (op.type == EditOp::Type::Keep) ++keeps;
            else ++deletes;
        }
        QCOMPARE(inserts, 1);
        QCOMPARE(keeps,   3);
        QCOMPARE(deletes, 0);
    }

    void testMyersOneDelete() {
        const QStringList a = {"A", "B", "C"};
        const QStringList b = {"A", "C"};
        const auto ops = MyersDiff::compute(a, b);
        int dels = 0, keeps = 0;
        for (const auto& op : ops) {
            if (op.type == EditOp::Type::Delete) ++dels;
            else if (op.type == EditOp::Type::Keep)   ++keeps;
        }
        QCOMPARE(dels,  1);
        QCOMPARE(keeps, 2);
    }

    void testMyersComplexLCS() {
        // Classic example from Myers paper: a=ABCABBA, b=CBABAC
        // LCS = 4 (BABA or CBAB depending on path)
        // Min edits = (7-4) + (6-4) = 5
        const QStringList a = {"A","B","C","A","B","B","A"};
        const QStringList b = {"C","B","A","B","A","C"};
        const auto ops = MyersDiff::compute(a, b);
        int keeps = 0, inserts = 0, deletes = 0;
        for (const auto& op : ops) {
            switch (op.type) {
            case EditOp::Type::Keep:   ++keeps;   break;
            case EditOp::Type::Insert: ++inserts; break;
            case EditOp::Type::Delete: ++deletes; break;
            default: break;
            }
        }
        // LCS length + inserts = b.size(), LCS length + deletes = a.size()
        QCOMPARE(keeps + inserts, b.size());
        QCOMPARE(keeps + deletes, a.size());
        // Edit distance must be minimal (≤ max possible)
        QVERIFY2(inserts + deletes <= a.size() + b.size(),
                 "edit distance should not exceed N+M");
    }

    void testMyersEditScriptOrdered() {
        // Result tokens must reconstruct b when inserts and keeps are taken in order
        const QStringList a = {"the", "quick", "fox"};
        const QStringList b = {"the", "fast", "fox", "jumps"};
        const auto ops = MyersDiff::compute(a, b);
        QStringList reconstructed;
        for (const auto& op : ops) {
            if (op.type == EditOp::Type::Insert || op.type == EditOp::Type::Keep)
                reconstructed.append(op.token);
        }
        QCOMPARE(reconstructed, b);
    }

    // ── Move detection ────────────────────────────────────────────────────

    void testMoveDetectNoMoves() {
        // Pure deletion/insertion — no moves
        const QStringList a = {"alpha", "beta"};
        const QStringList b = {"gamma", "delta"};
        const auto ops   = MyersDiff::compute(a, b);
        const auto moves = MyersDiff::detectMoves(ops);
        QVERIFY(moves.isEmpty());
    }

    void testMoveDetectSingleMove() {
        // "fox" moved from position 0 in A to position 2 in B
        const QStringList a = {"fox", "the", "quick"};
        const QStringList b = {"the", "quick", "fox"};
        const auto ops   = MyersDiff::compute(a, b);
        const auto moves = MyersDiff::detectMoves(ops);
        QVERIFY2(!moves.isEmpty(), "should detect 'fox' as moved");
        bool foundFox = false;
        for (const auto& mv : moves) {
            if (mv.token == "fox") { foundFox = true; break; }
        }
        QVERIFY2(foundFox, "'fox' must appear in move list");
    }

    void testMoveDetectParagraphReorder() {
        // Legal-document scenario: paragraph reordering
        // A: [clause1, clause2, clause3]
        // B: [clause3, clause1, clause2]
        // All three moved — Myers should keep at least 2 as common
        const QStringList a = {"clause1", "clause2", "clause3"};
        const QStringList b = {"clause3", "clause1", "clause2"};
        const auto ops   = MyersDiff::compute(a, b);
        const auto moves = MyersDiff::detectMoves(ops);
        // At minimum "clause3" moved (was at end, now at start)
        bool foundClause3Move = false;
        for (const auto& mv : moves) {
            if (mv.token == "clause3" && mv.fromIndex != mv.toIndex)
                foundClause3Move = true;
        }
        QVERIFY2(foundClause3Move, "clause3 must be detected as moved (not add+delete)");

        // Verify: no moved token appears only as delete+add pair
        // (i.e., the edit script accounts for moves, not raw set-difference)
        int deletes = 0, inserts = 0;
        for (const auto& op : ops) {
            if (op.type == EditOp::Type::Delete) ++deletes;
            if (op.type == EditOp::Type::Insert) ++inserts;
        }
        QVERIFY2(deletes + inserts < static_cast<int>(a.size()) + static_cast<int>(b.size()),
                 "LCS should share at least some tokens (not pure add+delete)");
    }

    // ── DiffResult integration ────────────────────────────────────────────

    void testDiffResultHasMoveField() {
        // Smoke test: DiffResult / PageDiff struct compiles with moves field
        DiffResult r;
        PageDiff pd;
        pd.moves.append(MoveOperation{"tok", 0, 1});
        r.pages.append(pd);
        QCOMPARE(r.pages.first().moves.size(), 1);
        QCOMPARE(r.pages.first().moves.first().token, QString("tok"));
    }

    // ── R11: explicit structural page changes (F06) ─────────────────────────
    // F06: one page versus the same page plus an appended appendix produced NO
    // entry for the added page, because compare() walked only
    // min(pageCount1, pageCount2) pages. The model must carry explicit
    // PageAdded / PageRemoved / PageMoved changes with old/new page positions,
    // and a missing side must be "no page" (-1), never a valid page-zero
    // sentinel.

    void pageChangeModelHasExplicitMissingSides() {
        // Default-constructed change must not read as a valid position on
        // either side.
        DiffResult::PageChange ch;
        QCOMPARE(ch.oldPage, -1);
        QCOMPARE(ch.newPage, -1);
        QVERIFY2(!ch.hasOldSide(), "default old side must be explicitly missing");
        QVERIFY2(!ch.hasNewSide(), "default new side must be explicitly missing");
    }

    void appendedUniquePageIsReportedAsAdded() {
        const QString base =
            createPagePdf(m_dir.path(), "f06_base.pdf", {"First page"});
        const QString extended = createPagePdf(m_dir.path(), "f06_extended.pdf",
                                               {"First page", "Appendix page"});
        QVERIFY(!base.isEmpty() && !extended.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(base, extended);

        QCOMPARE(r.pageCount1, 1);
        QCOMPARE(r.pageCount2, 2);
        QVERIFY2(!r.isIdentical, "an appended page must clear isIdentical");
        QCOMPARE(r.pageChanges.size(), 1);
        const DiffResult::PageChange& ch = r.pageChanges.first();
        QCOMPARE(ch.type, DiffResult::PageChangeType::PageAdded);
        QVERIFY2(!ch.hasOldSide(), "an added page has no old-side position");
        QVERIFY(ch.hasNewSide());
        QCOMPARE(ch.newPage, 1);  // 0-based position of the appended page
        QVERIFY2(ch.excerpt.contains("appendix"),
                 qPrintable(QStringLiteral("excerpt should name the added page, got: %1")
                                .arg(ch.excerpt)));
        // The shared page must not be re-reported (pages carries one entry per
        // compared page — the shared page's entry must hold no changes), and
        // the surplus page must not be misclassified as a move.
        for (const auto& pd : r.pages) {
            QVERIFY(pd.textAdded.isEmpty() && pd.textRemoved.isEmpty()
                    && pd.moves.isEmpty() && pd.pixelDiffCount == 0);
        }
        QVERIFY(r.pageMoves.isEmpty());
    }

    void appendedBlankPageIsStillAChange() {
        const QString base =
            createPagePdf(m_dir.path(), "blank_base.pdf", {"First page"});
        const QString extended =
            createPagePdf(m_dir.path(), "blank_extended.pdf", {"First page", ""});
        QVERIFY(!base.isEmpty() && !extended.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(base, extended);

        QVERIFY(!r.isIdentical);
        QCOMPARE(r.pageCount2, 2);
        QCOMPARE(r.pageChanges.size(), 1);
        const DiffResult::PageChange& ch = r.pageChanges.first();
        QCOMPARE(ch.type, DiffResult::PageChangeType::PageAdded);
        QVERIFY(!ch.hasOldSide());
        QCOMPARE(ch.newPage, 1);
        QVERIFY2(ch.excerpt.isEmpty(),
                 "a blank page must not inherit another page's excerpt");
    }

    void trailingPageRemovalIsReportedAsRemoved() {
        const QString full = createPagePdf(m_dir.path(), "full.pdf",
                                           {"First page", "Doomed page"});
        const QString trimmed =
            createPagePdf(m_dir.path(), "trimmed.pdf", {"First page"});
        QVERIFY(!full.isEmpty() && !trimmed.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(full, trimmed);

        QCOMPARE(r.pageCount1, 2);
        QCOMPARE(r.pageCount2, 1);
        QVERIFY(!r.isIdentical);
        QCOMPARE(r.pageChanges.size(), 1);
        const DiffResult::PageChange& ch = r.pageChanges.first();
        QCOMPARE(ch.type, DiffResult::PageChangeType::PageRemoved);
        QVERIFY(ch.hasOldSide());
        QVERIFY2(!ch.hasNewSide(), "a removed page has no new-side position");
        QCOMPARE(ch.oldPage, 1);  // the trailing page's old 0-based position
        // The surviving page's entry must hold no changes (pages carries one
        // entry per compared page, changed or not).
        for (const auto& pd : r.pages) {
            QVERIFY(pd.textAdded.isEmpty() && pd.textRemoved.isEmpty()
                    && pd.moves.isEmpty() && pd.pixelDiffCount == 0);
        }
    }

    void reversedOrderTurnsAdditionsIntoRemovals() {
        // Same fixtures as the F06 case, old/new swapped: the added page must
        // become a removed page, and the missing side flips with it.
        const QString base =
            createPagePdf(m_dir.path(), "rev_base.pdf", {"First page"});
        const QString extended = createPagePdf(m_dir.path(), "rev_extended.pdf",
                                               {"First page", "Appendix page"});
        QVERIFY(!base.isEmpty() && !extended.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(extended, base);

        QCOMPARE(r.pageChanges.size(), 1);
        const DiffResult::PageChange& ch = r.pageChanges.first();
        QCOMPARE(ch.type, DiffResult::PageChangeType::PageRemoved);
        QVERIFY(ch.hasOldSide());
        QVERIFY(!ch.hasNewSide());
        QCOMPARE(ch.oldPage, 1);
    }

    void reorderedPagesAreMovesNotAddRemove() {
        // Two pages swapped: the existing whole-page move detection must keep
        // working, and the moved page must NOT be double-counted as an
        // add+remove pair in the structural sequence.
        const QString a =
            createPagePdf(m_dir.path(), "order_a.pdf", {"Alpha page", "Beta page"});
        const QString b =
            createPagePdf(m_dir.path(), "order_b.pdf", {"Beta page", "Alpha page"});
        QVERIFY(!a.isEmpty() && !b.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(a, b);

        QCOMPARE(r.pageCount1, 2);
        QCOMPARE(r.pageCount2, 2);

        // Existing move detection intact (legacy API).
        QCOMPARE(r.pageMoves.size(), 1);
        QCOMPARE(r.pageMoves.first().fromPage, 1);
        QCOMPARE(r.pageMoves.first().toPage, 0);

        // Single structural entry, carrying both sides.
        QCOMPARE(r.pageChanges.size(), 1);
        const DiffResult::PageChange& ch = r.pageChanges.first();
        QCOMPARE(ch.type, DiffResult::PageChangeType::PageMoved);
        QVERIFY(ch.hasOldSide());
        QVERIFY(ch.hasNewSide());
        QCOMPARE(ch.oldPage, 1);
        QCOMPARE(ch.newPage, 0);
        for (const auto& any : r.pageChanges) {
            QVERIFY2(any.type == DiffResult::PageChangeType::PageMoved,
                     "a reordered page must not be double-counted as added/removed");
        }
    }

    void repeatedIdenticalPagesYieldSingleChange() {
        // Repeated identical pages are ambiguous for alignment; the engine must
        // fall back to exactly ONE structural change (an added page), never an
        // add+remove pair for the same content.
        const QString single =
            createPagePdf(m_dir.path(), "rep_one.pdf", {"Repeated page"});
        const QString doubled = createPagePdf(m_dir.path(), "rep_two.pdf",
                                              {"Repeated page", "Repeated page"});
        QVERIFY(!single.isEmpty() && !doubled.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(single, doubled);

        QVERIFY(!r.isIdentical);
        QCOMPARE(r.pageChanges.size(), 1);
        const DiffResult::PageChange& ch = r.pageChanges.first();
        QCOMPARE(ch.type, DiffResult::PageChangeType::PageAdded);
        QVERIFY(!ch.hasOldSide());
        QVERIFY(ch.hasNewSide());
        // Which of the two identical doc2 positions is reported depends on the
        // alignment tie-break (documented ambiguity fallback); either is honest.
        QVERIFY2(ch.newPage == 0 || ch.newPage == 1,
                 qPrintable(QStringLiteral("newPage=%1").arg(ch.newPage)));
    }

    void identicalPdfsProduceNoStructuralChanges() {
        const QStringList pages = {"First page", "Second page"};
        const QString a = createPagePdf(m_dir.path(), "ident_a.pdf", pages);
        const QString b = createPagePdf(m_dir.path(), "ident_b.pdf", pages);
        QVERIFY(!a.isEmpty() && !b.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(a, b);

        QVERIFY(r.isIdentical);
        QVERIFY(r.pageChanges.isEmpty());
    }

    // U04 contract: pageChanges is THE canonical sequence the filtered change
    // navigation walks, so a mixed diff (reorder + appended page) must appear
    // there once per structural change, each entry carrying its exact
    // old/new sides, in the deterministic doc2 reading order (moved pages
    // anchor on their new position, added pages follow).
    void mixedMoveAndAddYieldOrderedCanonicalSequence() {
        const QString a =
            createPagePdf(m_dir.path(), "mix_a.pdf", {"Alpha page", "Beta page"});
        const QString b = createPagePdf(m_dir.path(), "mix_b.pdf",
                                        {"Beta page", "Alpha page", "Epsilon page"});
        QVERIFY(!a.isEmpty() && !b.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(a, b);

        QVERIFY2(!r.isIdentical, "reorder + appended page must clear isIdentical");
        // Legacy move list stays populated exactly once for the reorder.
        QCOMPARE(r.pageMoves.size(), 1);
        QCOMPARE(r.pageMoves.first().fromPage, 1);
        QCOMPARE(r.pageMoves.first().toPage, 0);

        // One entry per structural change in the unified sequence: the
        // swapped page as PageMoved (both sides), the appended page as
        // PageAdded (no old side).
        QCOMPARE(r.pageChanges.size(), 2);
        int moved = -1, added = -1;
        for (int i = 0; i < r.pageChanges.size(); ++i) {
            const DiffResult::PageChange& ch = r.pageChanges.at(i);
            if (ch.type == DiffResult::PageChangeType::PageMoved) {
                QVERIFY2(moved == -1, "the reorder must appear exactly once");
                moved = i;
                QVERIFY(ch.hasOldSide() && ch.hasNewSide());
                QCOMPARE(ch.oldPage, 1);
                QCOMPARE(ch.newPage, 0);
            } else {
                QVERIFY2(ch.type == DiffResult::PageChangeType::PageAdded,
                         "no spurious structural entry for aligned pages");
                QVERIFY2(added == -1, "the appended page must appear exactly once");
                added = i;
                QVERIFY2(!ch.hasOldSide(), "an added page has no old-side position");
                QVERIFY(ch.hasNewSide());
                QCOMPARE(ch.newPage, 2);
            }
        }
        QVERIFY2(moved >= 0 && added >= 0, "both structural changes must be present");
        // Deterministic reading order (sorted by the page's doc2 position:
        // the moved page anchors at new position 0, the added page at 2).
        QVERIFY2(moved < added,
                 qPrintable(QStringLiteral("canonical order: moved@%1 added@%2")
                                .arg(moved).arg(added)));
    }

    // ── U04/R11 follow-up: middle-insertion alignment ────────────────────────
    // A page inserted BETWEEN existing pages must surface as exactly one
    // PageAdded at its true position — the surrounding pages stay matched,
    // never re-reported as removed+added chains. Reversed sides must flip the
    // change into a single PageRemoved. Repeated identical fingerprints are
    // resolved deterministically (tie-break pinned below).

    void middleInsertionAtPositionOneIsSingleTrueAddition() {
        // 3 pages, insertion at index 1 (not trailing): [A B C] → [A X B C].
        const QString three =
            createPagePdf(m_dir.path(), "mid_three.pdf",
                          {"Alpha page", "Beta page", "Gamma page"});
        const QString four =
            createPagePdf(m_dir.path(), "mid_four.pdf",
                          {"Alpha page", "Inserted page", "Beta page", "Gamma page"});
        QVERIFY(!three.isEmpty() && !four.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(three, four);

        QCOMPARE(r.pageCount1, 3);
        QCOMPARE(r.pageCount2, 4);
        QVERIFY2(!r.isIdentical, "a middle insertion must clear isIdentical");
        // Exactly ONE structural change: the inserted page itself.
        QCOMPARE(r.pageChanges.size(), 1);
        const DiffResult::PageChange& ch = r.pageChanges.first();
        QCOMPARE(ch.type, DiffResult::PageChangeType::PageAdded);
        QVERIFY2(!ch.hasOldSide(), "an inserted page has no old-side position");
        QVERIFY(ch.hasNewSide());
        QCOMPARE(ch.newPage, 1);  // its true 0-based position in the revised doc
        QVERIFY2(ch.excerpt.contains("inserted"),
                 qPrintable(QStringLiteral("excerpt should name the inserted page, got: %1")
                                .arg(ch.excerpt)));
        // The surrounding pages must stay matched — no remove/add chain for
        // them, and no move records either.
        QVERIFY2(r.pageMoves.isEmpty(),
                 "a pure insertion must not be misclassified as a page move");
    }

    void duplicateOfExistingPageInsertionPinsDeterministicTieBreak() {
        // [A B C] → [A B B C]: a duplicate of page 1 inserted at index 2.
        // Identical copies are ambiguous for alignment; the engine must fall
        // back to exactly ONE structural change and pick the reported
        // position deterministically: alignment consumes the LATEST doc2
        // occurrence of a repeated fingerprint, so the EARLIEST unmatched
        // occurrence surfaces as the insertion (newPage == 1 here).
        const QString three =
            createPagePdf(m_dir.path(), "dup_three.pdf",
                          {"Alpha page", "Beta page", "Gamma page"});
        const QString four =
            createPagePdf(m_dir.path(), "dup_four.pdf",
                          {"Alpha page", "Beta page", "Beta page", "Gamma page"});
        QVERIFY(!three.isEmpty() && !four.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(three, four);

        QVERIFY2(!r.isIdentical, "a duplicated page must clear isIdentical");
        QCOMPARE(r.pageChanges.size(), 1);
        const DiffResult::PageChange& ch = r.pageChanges.first();
        QCOMPARE(ch.type, DiffResult::PageChangeType::PageAdded);
        QVERIFY2(!ch.hasOldSide(), "the inserted copy has no old-side position");
        QVERIFY(ch.hasNewSide());
        // Tie-break pinned: with identical copies either position would be an
        // honest answer; the engine deterministically reports the first one.
        QCOMPARE(ch.newPage, 1);
        QVERIFY2(r.pageMoves.isEmpty(),
                 "a duplicate insertion is not a move");
    }

    void insertionWithTextEditOnOtherPageAlignsInsertionAtTruePosition() {
        // Regression for the ledgered defect: the inserted page X shares ≥80%
        // of its word set with page 0 (boilerplate twin — differs only in a
        // non-final word), and page 1 is reworded. The fuzzy pre-fingerprint
        // alignment paired old page 0 with X and reported the TRUE page 0 as
        // an extra addition — a remove+add mess instead of one true insertion.
        // old: [P, Q, R]  new: [P, X ≈ P, Q reworded, R]
        const QString three =
            createPagePdf(m_dir.path(), "mix3.pdf",
                          {"alpha beta gamma delta zeta", "omega psi", "final page here"});
        const QString four =
            createPagePdf(m_dir.path(), "mix4.pdf",
                          {"alpha beta gamma delta zeta",
                           "alpha beta eta gamma delta zeta",
                           "omega psi reworded",
                           "final page here"});
        QVERIFY(!three.isEmpty() && !four.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(three, four);

        QCOMPARE(r.pageCount1, 3);
        QCOMPARE(r.pageCount2, 4);
        QVERIFY2(!r.isIdentical, "insertion + edit must clear isIdentical");

        // (1) Page 0 is untouched — it must not appear on ANY change.
        for (int i = 0; i < r.pageChanges.size(); ++i) {
            const DiffResult::PageChange& ch = r.pageChanges.at(i);
            QVERIFY2(!(ch.hasOldSide() && ch.oldPage == 0),
                     qPrintable(QStringLiteral(
                                    "change %1 mispairs untouched page 0 (oldPage=%2)")
                                    .arg(i).arg(ch.oldPage)));
            QVERIFY2(!(ch.hasNewSide() && ch.newPage == 0),
                     qPrintable(QStringLiteral(
                                    "change %1 mispairs untouched page 0 (newPage=%2)")
                                    .arg(i).arg(ch.newPage)));
        }

        // (2) V04 re-pin: with the explicit substitution policy, the doc1
        // leftover Q pairs with the first doc2 leftover X (order-preserving,
        // in-order) as an ALIGNED MODIFIED pair — structurally silent. The
        // only structural change left is the rewritten page surfacing as ONE
        // PageAdded at its own position (newPage == 2). The former pin ("the
        // insertion surfaces once at its true position" and "a rewrite below
        // the similarity floor is a removal + an addition") encoded exactly
        // the remove+add classification that V04 overturns: a below-floor
        // rewrite is a content change, not a structural one.
        int added = 0, removed = 0;
        int addedQ = -1;
        for (int i = 0; i < r.pageChanges.size(); ++i) {
            const DiffResult::PageChange& ch = r.pageChanges.at(i);
            if (ch.type == DiffResult::PageChangeType::PageAdded) {
                ++added;
                addedQ = ch.newPage;
            } else if (ch.type == DiffResult::PageChangeType::PageRemoved) {
                ++removed;
            }
        }
        QCOMPARE(added, 1);
        QCOMPARE(removed, 0);
        QCOMPARE(addedQ, 2);

        // (3) No move may paper over the structural changes here.
        QVERIFY2(r.pageChanges.isEmpty()
                     || std::none_of(r.pageChanges.cbegin(), r.pageChanges.cend(),
                                     [](const DiffResult::PageChange& ch) {
                                         return ch.type ==
                                             DiffResult::PageChangeType::PageMoved;
                                     }),
                 "insertion + text edit must not be classified as page moves");

        // (4) V04: the below-floor rewrite is accounted as CONTENT changes on
        // the aligned pair, not as a structural removal. The page diffs for
        // the modified pair (old Q at index 1, reworded Q' at index 2) must
        // still carry the real word changes. (PDFium extraction carries a
        // trailing NUL — pinned §9.10-a behavior — so word-token checks are
        // substring matches on the joined list.)
        QVERIFY2(r.pages.at(1).textRemoved.join(QLatin1Char(' '))
                     .contains(QStringLiteral("omega"))
                     && r.pages.at(1).textAdded.join(QLatin1Char(' '))
                            .contains(QStringLiteral("alpha")),
                 "the aligned modified pair must report its content changes");
        QVERIFY2(r.pages.at(2).textRemoved.join(QLatin1Char(' '))
                     .contains(QStringLiteral("here"))
                     && r.pages.at(2).textAdded.join(QLatin1Char(' '))
                            .contains(QStringLiteral("reworded")),
                 "the shifted page pair must report its content changes");
    }

    void reversedSidesTurnMiddleInsertionIntoSingleRemoval() {
        // Old/new swapped relative to the middle-insertion case: the inserted
        // page must become exactly ONE PageRemoved at its old position — no
        // added entries, no moves.
        const QString three =
            createPagePdf(m_dir.path(), "revmid_three.pdf",
                          {"Alpha page", "Beta page", "Gamma page"});
        const QString four =
            createPagePdf(m_dir.path(), "revmid_four.pdf",
                          {"Alpha page", "Inserted page", "Beta page", "Gamma page"});
        QVERIFY(!three.isEmpty() && !four.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(four, three);

        QCOMPARE(r.pageCount1, 4);
        QCOMPARE(r.pageCount2, 3);
        QVERIFY2(!r.isIdentical, "a middle removal must clear isIdentical");
        QCOMPARE(r.pageChanges.size(), 1);
        const DiffResult::PageChange& ch = r.pageChanges.first();
        QCOMPARE(ch.type, DiffResult::PageChangeType::PageRemoved);
        QVERIFY(ch.hasOldSide());
        QVERIFY2(!ch.hasNewSide(), "a removed page has no new-side position");
        QCOMPARE(ch.oldPage, 1);
        QVERIFY2(r.pageMoves.isEmpty(),
                 "a pure middle removal must not be misclassified as a move");
    }

    // ── V04: an in-place page edit is a CONTENT change, not a structural one ─
    // The alignment leftovers used to fall through to PageRemoved + PageAdded
    // whenever their word-set similarity missed the 0.80 fuzzy floor — so a
    // one-page document whose text was rewritten ("Apple" → "Orange") was
    // reported as the page being destroyed and a new page being added, on top
    // of its ordinary text difference. Low text similarity is not proof that
    // the page structure changed: an aligned modified pair must be represented
    // as content changes only (result.pages), never as add/remove.

    void onePageTextEditReportsContentChangesWithoutStructuralChanges() {
        // THE V04 acceptance fixture: same one-page layout, "Apple" → "Orange".
        // Word sets {apple} vs {orange} have Jaccard 0.0 — far below any
        // similarity floor — so only the explicit substitution policy can keep
        // this out of the structural sequence.
        const QString apple =
            createPagePdf(m_dir.path(), "v04_apple.pdf", {"Apple page"});
        const QString orange =
            createPagePdf(m_dir.path(), "v04_orange.pdf", {"Orange page"});
        QVERIFY(!apple.isEmpty() && !orange.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(apple, orange);

        QCOMPARE(r.pageCount1, 1);
        QCOMPARE(r.pageCount2, 1);
        QVERIFY2(!r.isIdentical, "a text edit must clear isIdentical");
        QVERIFY2(r.pageChanges.isEmpty(),
                 qPrintable(QStringLiteral(
                                "a one-page content edit must report NO structural "
                                "changes (got %1) — page modifications are content "
                                "changes, not add/remove").arg(r.pageChanges.size())));
        QVERIFY(r.pageMoves.isEmpty());
        // The content change itself must be present as an ordinary page diff.
        // PDFium extraction carries a trailing NUL (pinned §9.10-a behavior),
        // so word-token checks go through a substring match on the joined list.
        QCOMPARE(r.pages.size(), 1);
        QVERIFY2(r.pages.first().textRemoved.join(QLatin1Char(' '))
                     .contains(QStringLiteral("Apple")),
                 "the removed word must be reported on the page diff");
        QVERIFY2(r.pages.first().textAdded.join(QLatin1Char(' '))
                     .contains(QStringLiteral("Orange")),
                 "the added word must be reported on the page diff");
    }

    void middlePageRewriteBelowSimilarityFloorIsContentOnly() {
        // Same page count, middle page rewritten below the fuzzy floor:
        // {"Beta page"} vs {"Beta rewritten"} share only "beta" → Jaccard
        // 1/3 < 0.80. The rewrite must stay a content change on page 2, with
        // the surrounding pages staying matched and structurally silent.
        const QString before =
            createPagePdf(m_dir.path(), "v04_mid_before.pdf",
                          {"Alpha page", "Beta page", "Gamma page"});
        const QString after =
            createPagePdf(m_dir.path(), "v04_mid_after.pdf",
                          {"Alpha page", "Beta rewritten", "Gamma page"});
        QVERIFY(!before.isEmpty() && !after.isEmpty());

        DiffEngine engine;
        const DiffResult r = engine.compare(before, after);

        QCOMPARE(r.pageCount1, 3);
        QCOMPARE(r.pageCount2, 3);
        QVERIFY2(!r.isIdentical, "a middle-page rewrite must clear isIdentical");
        QVERIFY2(r.pageChanges.isEmpty(),
                 qPrintable(QStringLiteral(
                                "a below-floor rewrite of the middle page must not "
                                "produce structural changes (got %1)")
                                .arg(r.pageChanges.size())));
        QVERIFY(r.pageMoves.isEmpty());
        QCOMPARE(r.pages.size(), 3);
        QVERIFY(r.pages.at(0).textRemoved.isEmpty()
                && r.pages.at(0).textAdded.isEmpty());
        // PDFium extraction carries a trailing NUL (pinned §9.10-a behavior):
        // match word tokens as substrings of the joined list.
        QVERIFY2(r.pages.at(1).textRemoved.join(QLatin1Char(' '))
                     .contains(QStringLiteral("page")),
                 "the old middle-page wording must be reported as content removed");
        QVERIFY2(r.pages.at(1).textAdded.join(QLatin1Char(' '))
                     .contains(QStringLiteral("rewritten")),
                 "the new middle-page wording must be reported as content added");
        QVERIFY(r.pages.at(2).textRemoved.isEmpty()
                && r.pages.at(2).textAdded.isEmpty());
    }
};

QTEST_MAIN(TestDiffEngine)
#include "TestDiffEngine.moc"
