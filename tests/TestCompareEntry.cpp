// SPDX-License-Identifier: Apache-2.0
// §9.10 P0 — Document Comparison entry point. The Myers-diff engine and
// CompareMode UI were built and unit-tested, but compareFiles() had no caller,
// so every menu route landed on an empty screen. A "Compare Docs…" button now
// drives promptAndCompare() -> startComparison() -> pathsAreComparable() ->
// compareFiles(). This tests the pure validator seam that guards that path
// (the file pickers themselves are modal UI and are not unit-tested here).
#include <QtTest>
#include <QPainter>
#include <QTemporaryDir>
#include <QPdfWriter>

#include "modes/CompareMode.h"

class TestCompareEntry : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    // A minimal but real one-page PDF on disk.
    QString makePdf(const QString& name)
    {
        const QString path = m_dir.filePath(name);
        QPdfWriter w(path);
        w.setPageSize(QPageSize(QPageSize::A4));
        QPainter p(&w);
        p.drawText(100, 100, name);
        p.end();
        return path;
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    // Two distinct, existing PDFs are accepted for comparison.
    void distinctExistingFilesAreComparable()
    {
        const QString a = makePdf("orig.pdf");
        const QString b = makePdf("revised.pdf");
        QString why;
        QVERIFY2(gp::CompareMode::pathsAreComparable(a, b, &why), qPrintable(why));
    }

    // Comparing a file with itself is rejected (would produce an empty diff and
    // read as broken) — the guard reports why.
    void sameFileIsRejected()
    {
        const QString a = makePdf("same.pdf");
        QString why;
        QVERIFY(!gp::CompareMode::pathsAreComparable(a, a, &why));
        QVERIFY2(!why.isEmpty(), "rejection must explain itself to the user");
    }

    // A non-existent path is rejected with a "not found" reason.
    void missingFileIsRejected()
    {
        const QString a = makePdf("present.pdf");
        QString why;
        QVERIFY(!gp::CompareMode::pathsAreComparable(a, m_dir.filePath("absent.pdf"), &why));
        QVERIFY2(why.contains("not found", Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("expected a not-found reason, got: %1").arg(why)));
    }
};

QTEST_MAIN(TestCompareEntry)
#include "TestCompareEntry.moc"
