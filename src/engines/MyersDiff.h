// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QList>
#include <QString>
#include <QStringList>

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

/// Myers 1986 O((N+M)D) LCS / shortest-edit-script algorithm for token sequences.
class MyersDiff {
public:
    MyersDiff() = delete;

    /// Compute the shortest edit script (LCS) for two token sequences.
    /// Returns an ordered list of EditOp (Keep / Insert / Delete) operations.
    /// Complexity: O((N+M) * D) time, O((N+M) * D) space for trace snapshots.
    static QList<EditOp> compute(const QStringList& a, const QStringList& b);

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
