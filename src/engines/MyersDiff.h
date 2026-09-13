// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

/// One operation in a Myers LCS edit script.
struct EditOp {
    enum class Type { Keep, Insert, Delete };
    Type     type;
    QString  token;
    int      indexA = -1;  ///< 0-based index in sequence A (valid for Keep / Delete)
    int      indexB = -1;  ///< 0-based index in sequence B (valid for Keep / Insert)
};

/// A token that moved from one position to another (detected by post-pass on the edit script).
struct MoveOperation {
    QString token;
    int     fromIndex = -1;  ///< position in sequence A where it was deleted
    int     toIndex   = -1;  ///< position in sequence B where it was inserted
};

/// Myers 1986 O((N+M)D) LCS / shortest-edit-script algorithm for token sequences,
/// R11-bounded: common prefix/suffix trimming, a retained-trace memory budget
/// and an honest coarse fallback ("diff truncated") when the budget cannot
/// hold an exact minimal script.
class MyersDiff {
public:
    MyersDiff() = delete;

    /// R11: resource knobs for compute().
    struct Options {
        /// Peak bytes the algorithm may retain for its backtrack trace
        /// (one snapshot of the frontier per edit-distance step). The
        /// default bounds the worst case far below the unbounded O((N+M)D)
        /// growth (PERF-03: ~134 MiB peak for 2,000 divergent tokens/side).
        /// When the exact script does not fit the budget, compute() emits a
        /// coarse (non-minimal) script and reports truncated=true instead of
        /// growing without bound.
        qint64 traceBudgetBytes = 16 * 1024 * 1024;

        /// Optional cancellation probe, checked while the expensive forward
        /// pass runs. When it returns true, compute() stops early and emits
        /// the coarse fallback (truncated=true) rather than running on.
        std::function<bool()> cancelled;
    };

    /// Compute the shortest edit script (LCS) for two token sequences with
    /// default Options. Convenience overload preserving the historical API.
    static QList<EditOp> compute(const QStringList& a, const QStringList& b);

    /// Compute an edit script under an explicit resource budget.
    /// Returns an ordered list of EditOp (Keep / Insert / Delete).
    /// Exact (minimal) whenever the budget can hold the trace for the input's
    /// true edit distance; otherwise a coarse script anchored on the furthest
    /// intermediate progress, with \p truncated set to true. The caller must
    /// disclose truncation to the user rather than presenting it as minimal.
    /// Complexity: O((N+M) * D) time; trace memory bounded by
    /// Options::traceBudgetBytes.
    static QList<EditOp> compute(const QStringList& a, const QStringList& b,
                                 const Options& opt, bool* truncated = nullptr);

    /// Move-detection post-pass over an edit script produced by compute().
    ///
    /// Identifies Delete+Insert pairs where the same token was deleted from sequence A
    /// at position i and inserted into sequence B at position j (i != j).
    /// Those ops remain in \p edits but the returned list describes each move.
    ///
    /// The pairing strategy: for each distinct token, greedily pair the earliest
    /// available delete with the earliest available insert.
    static QList<MoveOperation> detectMoves(const QList<EditOp>& edits);
};
