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
// ---------------------------------------------------------------------------

namespace {

/// Backtrack through the trace snapshots to produce the ordered edit script.
static QList<EditOp> backtrack(
    const QStringList&          a,
    const QStringList&          b,
    const QVector<QVector<int>>& trace,  // trace[d] = V after step d
    int                          D)
{
    const int N   = a.size();
    const int M   = b.size();
    const int MAX = N + M;

    QList<EditOp> ops;
    int x = N, y = M;

    for (int d = D; d > 0; --d) {
        const QVector<int>& Vd = trace[d];      // V after step d
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
        (void)Vd;  // Vd was used during forward pass; not needed here

        // Snake: diagonal moves from (editX, editY) to (x, y)
        for (int sx = x - 1, sy = y - 1; sx >= editX && sy >= editY; --sx, --sy) {
            ops.prepend(EditOp{EditOp::Type::Keep, a[sx], sx, sy});
        }

        // The single edit step
        if (prevK < k) {
            // delete: a[prevX] was consumed moving right
            ops.prepend(EditOp{EditOp::Type::Delete, a[prevX], prevX, -1});
        } else {
            // insert: b[prevY] was consumed moving down
            ops.prepend(EditOp{EditOp::Type::Insert, b[prevY], -1, prevY});
        }

        x = prevX;
        y = prevY;
    }

    // Remaining snake at d=0 (from (0,0) onwards)
    for (int sx = x - 1, sy = y - 1; sx >= 0 && sy >= 0; --sx, --sy) {
        ops.prepend(EditOp{EditOp::Type::Keep, a[sx], sx, sy});
    }

    return ops;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QList<EditOp> MyersDiff::compute(const QStringList& a, const QStringList& b)
{
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

    const int MAX = N + M;
    // V[k + MAX] = rightmost x reached on diagonal k
    QVector<int> V(2 * MAX + 1, 0);
    // trace[d] = snapshot of V after processing step d
    QVector<QVector<int>> trace;
    trace.reserve(MAX + 1);

    // Initial state: d=0, starting snake from (0,0)
    // Before step 0 we need a "trace[-1]"-equivalent.  We push a zeroed V first
    // so that d=1 backtracking can look up trace[0] = state before step 1 = after step 0.
    // Actually we push BEFORE updating V each step so trace[d] = V before step d.
    // → We push trace AFTER updating so trace[d] = V after step d.

    for (int D = 0; D <= MAX; ++D) {
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
            while (x < N && y < M && a[x] == b[y]) {
                ++x; ++y;
            }
            V[k + MAX] = x;

            if (x >= N && y >= M) {
                found = true;
                break;
            }
        }

        trace.push_back(V);  // snapshot after step D
        if (found) return backtrack(a, b, trace, D);
    }

    // Should not reach here for finite sequences
    return {};
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
