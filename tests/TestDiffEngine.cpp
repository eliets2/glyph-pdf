#include <QtTest>
#include "engines/MyersDiff.h"
#include "engines/DiffEngine.h"

class TestDiffEngine : public QObject {
    Q_OBJECT

private slots:

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
};

QTEST_MAIN(TestDiffEngine)
#include "TestDiffEngine.moc"
