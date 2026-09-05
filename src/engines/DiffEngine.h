// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QList>
#include <QImage>
#include <QStringList>
#include "engines/MyersDiff.h"

struct PageDiff {
    int         pageIndex      = 0;
    QImage      diffImage;           ///< visual pixel-diff overlay
    QStringList textRemoved;         ///< tokens deleted (non-move deletes)
    QStringList textAdded;           ///< tokens inserted (non-move inserts)
    QList<MoveOperation> moves;      ///< tokens that moved position
    int         pixelDiffCount = 0;
};

struct DiffResult {
    bool isIdentical = false;
    int  pageCount1  = 0;
    int  pageCount2  = 0;
    QList<PageDiff> pages;

    /// A page from doc1 that appears at a different index in doc2 (reorder).
    struct PageMove { int fromPage; int toPage; QString excerpt; };
    QList<PageMove> pageMoves;

    /// R11: explicit structural page changes. A page can exist on only one
    /// side (added/removed) or on both sides at different positions (moved).
    /// A side with no page carries -1 — an explicit "missing" marker, never a
    /// valid page-zero sentinel (check hasOldSide()/hasNewSide() first).
    enum class PageChangeType { PageAdded, PageRemoved, PageMoved };
    struct PageChange {
        PageChangeType type = PageChangeType::PageAdded;
        int oldPage = -1;   ///< 0-based index in doc1; -1 = no such page (added)
        int newPage = -1;   ///< 0-based index in doc2; -1 = no such page (removed)
        QString excerpt;    ///< start of the page's text ("" for blank pages)
        bool hasOldSide() const { return oldPage >= 0; }
        bool hasNewSide() const { return newPage >= 0; }
    };
    /// Single canonical structural sequence, in deterministic page order. The
    /// CHANGES tree, the change-type filters, the next/previous sequence, the
    /// status totals and the exported reports all read this one list (moved
    /// pages appear here exactly once — pageMoves above stays populated only
    /// for backward compatibility).
    QList<PageChange> pageChanges;
};

class DiffEngine {
public:
    DiffEngine();
    ~DiffEngine();

    DiffResult compare(const QString &file1, const QString &file2, int dpi = 150);
};
