// SPDX-License-Identifier: Apache-2.0
#include "engines/MyersDiff.h"
#include <QHash>
#include <QVector>

// ---------------------------------------------------------------------------
// Myers 1986 O((N+M)D) shortest edit script
//
// Algorithm reference: E. W. Myers, "An O(ND) Difference Algorithm and Its
// Variations", Algorithmica 1(2), 1986.
//
// The edit graph has N+1 columns (0..N) and M+1 rows (0..M).
// Diagonal k = x - y.  V[k] = rightmost x reached along diagonal k.
// We use a 1-D array V of size 2*(N+M)+1, offset by (N+M) so index k maps
// to V[k + MAX].
//
// R11 (PERF-03) bounding. The unbounded implementation retained one full
// frontier snapshot per edit-distance step — O((N+M)D) retained memory, which
// measured ~134 MiB of peak working set for 2,000 entirely-divergent tokens
// per side. The bounded implementation:
//   1. trims the common prefix and suffix first (exact, and typical small
//      edits become nearly free),
//   2. retains snapshots only while Options::traceBudgetBytes allows,
//   3. when the exact script cannot fit the budget, anchors on the diagonal
//      with the furthest (x+y) progress of the last completed step,
//      backtracks the exact path to that anchor, and emits the remaining
//      middle as one coarse delete+insert hunk — a valid, non-minimal edit
//      script flagged truncated=true so callers can disclose it.
// ---------------------------------------------------------------------------

namespace {

/// Backtrack through the trace snapshots to produce the ordered edit script
/// for the sub-path that ends at (xEnd, yEnd) after step D. Tokens and
/// indices are absolute (caller passes offset views through a/b/offA/offB).
static QList<EditOp> backtrackRange(
    const QStringList&           a,
    const QStringList&           b,
    int                          offA,
    int                          offB,
    const QVector<QVector<int>>& trace,  // trace[d] = V after step d
    int                          D,
    int                          xEnd,
    int                          yEnd)
{
    const int MAX = trace.isEmpty() ? 0 : (trace.first().size() - 1) / 2;

    QList<EditOp> ops;
    int x = xEnd, y = yEnd;

    for (int d = D; d > 0; --d) {
        const QVector<int>& Vp = trace[d - 1];  // V after step d-1 (= before step d)
        const int k = x - y;

        // Determine which direction we came from at step d on diagonal k
        int prevK;
        if (k == -d || (k != d && Vp[k - 1 + MAX] < Vp[k + 1 + MAX])) {
            prevK = k + 1;   // came via insert (moved down, k+1 → k)
        } else {
            prevK = k - 1;   // came via delete (moved right, k-1 → k)
        }

        const int prevX = Vp[prevK + MAX];
        const int prevY = prevX - prevK;

        // Edit point: where we were immediately after the single insert/delete
        int editX, editY;
        if (prevK < k) {
            // delete: x increased by 1, y stayed
            editX = prevX + 1;
            editY = prevY;
        } else {
            // insert: x stayed, y increased by 1
            editX = prevX;
            editY = prevY + 1;
        }

        // Snake: diagonal moves from (editX, editY) to (x, y)
        for (int sx = x - 1, sy = y - 1; sx >= editX && sy >= editY; --sx, --sy) {
            ops.prepend(EditOp{EditOp::Type::Keep, a[offA + sx], offA + sx, offB + sy});
        }

        // The single edit step
        if (prevK < k) {
            // delete: a[offA + prevX] was consumed moving right
            ops.prepend(EditOp{EditOp::Type::Delete, a[offA + prevX], offA + prevX, -1});
        } else {
            // insert: b[offB + prevY] was consumed moving down
            ops.prepend(EditOp{EditOp::Type::Insert, b[offB + prevY], -1, offB + prevY});
        }

        x = prevX;
        y = prevY;
    }

    // Remaining snake at d=0 (from (0,0) onwards)
    for (int sx = x - 1, sy = y - 1; sx >= 0 && sy >= 0; --sx, --sy) {
        ops.prepend(EditOp{EditOp::Type::Keep, a[offA + sx], offA + sx, offB + sy});
    }

    return ops;
}

/// Bounded forward Myers over the middle ranges a[offA .. offA+lenA) and
/// b[offB .. offB+lenB), emitting absolute-index ops. Sets *truncated when
/// the budget (or cancellation) forced the coarse fallback.
static QList<EditOp> computeMiddleBounded(
    const QStringList& a, const QStringList& b,
    int offA, int offB, int lenA, int lenB,
    const MyersDiff::Options& opt, bool* truncated)
{
    *truncated = false;

    const int MAX = lenA + lenB;
    // V[k + MAX] = rightmost x reached on diagonal k
    QVector<int> V(2 * MAX + 1, 0);

    // R11: how many frontier snapshots fit in the retained-trace budget.
    const qint64 snapshotBytes = (2 * qint64(MAX) + 1) * qint64(sizeof(int));
    const int stepCap = snapshotBytes > 0
        ? int(qBound<qint64>(0, opt.traceBudgetBytes / snapshotBytes, qint64(MAX)))
        : MAX;

    QVector<QVector<int>> trace;
    trace.reserve(size_t(stepCap) + 1);

    int lastStep = -1;      // last step actually recorded in trace
    bool stoppedEarly = false;

    for (int D = 0; D <= stepCap; ++D) {
        if (opt.cancelled && opt.cancelled()) {  // R11: cancellation hook
            stoppedEarly = true;
            break;
        }

        bool found = false;
        for (int k = -D; k <= D; k += 2) {
            int x;
            if (k == -D || (k != D && V[k - 1 + MAX] < V[k + 1 + MAX])) {
                x = V[k + 1 + MAX];       // insert: move down from k+1
            } else {
                x = V[k - 1 + MAX] + 1;  // delete: move right from k-1
            }
            int y = x - k;

            // Diagonal snake
            while (x < lenA && y < lenB && a[offA + x] == b[offB + y]) {
                ++x; ++y;
            }
            V[k + MAX] = x;

            if (x >= lenA && y >= lenB) {
                found = true;
                break;
            }
        }

        trace.push_back(V);  // snapshot after step D
        lastStep = D;
        if (found) {
            // Exact minimal script fits the budget (and was not cancelled).
            return backtrackRange(a, b, offA, offB, trace, D, lenA, lenB);
        }
    }

    // ── R11 honest fallback: the exact script does not fit the budget ─────
    // Anchor on the diagonal with the furthest (x+y) progress among the
    // diagonals written by the last completed step (parity-matched: only
    // those hold a fresh, reachable frontier value), emit the exact path to
    // that anchor, then one coarse delete+insert hunk for the remainder.
    *truncated = true;
    (void)stoppedEarly;

    QList<EditOp> ops;
    if (lastStep < 0) {
        // No step ran (tiny budget or immediate cancellation): coarse hunk only.
        for (int x = 0; x < lenA; ++x)
            ops.append(EditOp{EditOp::Type::Delete, a[offA + x], offA + x, -1});
        for (int y = 0; y < lenB; ++y)
            ops.append(EditOp{EditOp::Type::Insert, b[offB + y], -1, offB + y});
        return ops;
    }

    int bestK = 0;
    qint64 bestSum = -1;
    for (int k = -lastStep; k <= lastStep; k += 2) {
        const int x = V[k + MAX];
        const int y = x - k;
        // R11: once an edit-graph edge is reachable (stepCap > lenA/lenB),
        // the forward loop can fabricate frontier values beyond the edges on
        // extreme diagonals. They are not real paths — never anchor on them
        // (anchoring on one reads tokens past the sequences' ends).
        if (x < 0 || x > lenA || y < 0 || y > lenB) continue;
        const qint64 sum = 2 * qint64(x) - k;   // x + y with y = x - k
        if (sum > bestSum) { bestSum = sum; bestK = k; }
    }
    if (bestSum < 0) {
        // No grid-valid anchor (defensive; diagonal parity-matching always
        // leaves at least one valid frontier): emit the coarse hunk outright.
        for (int x = 0; x < lenA; ++x)
            ops.append(EditOp{EditOp::Type::Delete, a[offA + x], offA + x, -1});
        for (int y = 0; y < lenB; ++y)
            ops.append(EditOp{EditOp::Type::Insert, b[offB + y], -1, offB + y});
        return ops;
    }
    const int xEnd = V[bestK + MAX];
    const int yEnd = xEnd - bestK;

    ops = backtrackRange(a, b, offA, offB, trace, lastStep, xEnd, yEnd);
    for (int x = xEnd; x < lenA; ++x)
        ops.append(EditOp{EditOp::Type::Delete, a[offA + x], offA + x, -1});
    for (int y = yEnd; y < lenB; ++y)
        ops.append(EditOp{EditOp::Type::Insert, b[offB + y], -1, offB + y});
    return ops;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QList<EditOp> MyersDiff::compute(const QStringList& a, const QStringList& b)
{
    return compute(a, b, Options(), nullptr);
}

QList<EditOp> MyersDiff::compute(const QStringList& a, const QStringList& b,
                                 const Options& opt, bool* truncated)
{
    if (truncated) *truncated = false;

    const int N = a.size();
    const int M = b.size();

    // Edge cases
    if (N == 0 && M == 0) return {};
    if (N == 0) {
        QList<EditOp> ops;
        ops.reserve(M);
        for (int j = 0; j < M; ++j)
            ops.append(EditOp{EditOp::Type::Insert, b[j], -1, j});
        return ops;
    }
    if (M == 0) {
        QList<EditOp> ops;
        ops.reserve(N);
        for (int i = 0; i < N; ++i)
            ops.append(EditOp{EditOp::Type::Delete, a[i], i, -1});
        return ops;
    }

    // ── R11: common prefix / suffix trim ────────────────────────────────────
    // Exact: keeps outside the divergent middle never need the edit graph,
    // which makes identical and near-identical inputs nearly free and keeps
    // the bounded algorithm's work proportional to the DIVERGENCE, not the
    // document size.
    int pre = 0;
    while (pre < N && pre < M && a[pre] == b[pre]) ++pre;
    int suf = 0;
    while (suf < N - pre && suf < M - pre && a[N - 1 - suf] == b[M - 1 - suf]) ++suf;

    const int midA = N - pre - suf;
    const int midB = M - pre - suf;

    QList<EditOp> ops;
    ops.reserve(N + M);
    for (int i = 0; i < pre; ++i)
        ops.append(EditOp{EditOp::Type::Keep, a[i], i, i});

    if (midB == 0) {
        for (int i = 0; i < midA; ++i)
            ops.append(EditOp{EditOp::Type::Delete, a[pre + i], pre + i, -1});
    } else if (midA == 0) {
        for (int j = 0; j < midB; ++j)
            ops.append(EditOp{EditOp::Type::Insert, b[pre + j], -1, pre + j});
    } else {
        bool midTruncated = false;
        ops.append(computeMiddleBounded(a, b, pre, pre, midA, midB, opt, &midTruncated));
        if (truncated) *truncated = midTruncated;
    }

    for (int i = 0; i < suf; ++i)
        ops.append(EditOp{EditOp::Type::Keep, a[N - suf + i], N - suf + i,
                          M - suf + i});

    return ops;
}

QList<MoveOperation> MyersDiff::detectMoves(const QList<EditOp>& edits)
{
    // Build maps: token → sorted list of indexA (deletes) and indexB (inserts)
    QHash<QString, QList<int>> deleted;   // token → positions in A
    QHash<QString, QList<int>> inserted;  // token → positions in B

    for (const EditOp& op : edits) {
        if (op.type == EditOp::Type::Delete)
            deleted[op.token].append(op.indexA);
        else if (op.type == EditOp::Type::Insert)
            inserted[op.token].append(op.indexB);
    }

    QList<MoveOperation> moves;

    // For each token that was both deleted and inserted, pair greedily
    for (auto it = deleted.begin(); it != deleted.end(); ++it) {
        const QString& tok = it.key();
        if (!inserted.contains(tok)) continue;

        QList<int>& fromList = it.value();
        QList<int>& toList   = inserted[tok];

        const int pairs = qMin(fromList.size(), toList.size());
        for (int p = 0; p < pairs; ++p) {
            const int fromIdx = fromList[p];
            const int toIdx   = toList[p];
            // Only classify as a move if position actually changed
            if (fromIdx != toIdx)
                moves.append(MoveOperation{tok, fromIdx, toIdx});
        }
    }

    return moves;
}
