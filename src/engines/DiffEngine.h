#pragma once
#include <QString>
#include <QList>
#include <QImage>
#include <QStringList>

struct PageDiff {
    int pageIndex;
    QImage diffImage; // Visual diff overlay
    QStringList textRemoved;
    QStringList textAdded;
    int pixelDiffCount;
};

struct DiffResult {
    bool isIdentical;
    int pageCount1;
    int pageCount2;
    QList<PageDiff> pages;
};

class DiffEngine {
public:
    DiffEngine();
    ~DiffEngine();

    DiffResult compare(const QString &file1, const QString &file2, int dpi = 150);
};
