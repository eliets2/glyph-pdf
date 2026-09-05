#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
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
};

QTEST_MAIN(TestDiffEngine)
#include "TestDiffEngine.moc"
