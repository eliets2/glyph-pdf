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
};

class DiffEngine {
public:
    DiffEngine();
    ~DiffEngine();

    DiffResult compare(const QString &file1, const QString &file2, int dpi = 150);
};
