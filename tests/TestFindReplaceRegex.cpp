// SPDX-License-Identifier: Apache-2.0
//
// §9.15 — focused unit tests for the regex / whole-word document-text match
// extraction behind EditController's Find / Find & Replace. EditController
// builds its document-search patterns in exactly three ways, and each is
// exercised here against PatternRedactor::findMatches (PDFium per-character
// box extraction) on a tiny generated PDF fixture:
//   1. literal     -> QRegularExpression::escape(text)
//   2. whole word  -> "\\b" + escape(text) + "\\b"   (regex off)
//   3. user regex  -> the pattern verbatim (never double-wrapped)
// plus the invalid-pattern guard, case-option handling, and the batch
// (multi-page) overload that EditController's search loop calls.

#include "engines/PatternRedactor.h"

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QList>
#include <QRectF>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

class TestFindReplaceRegex : public QObject {
    Q_OBJECT

private slots:
    void escapedLiteralFindsEveryOccurrence();
    void wholeWordWrappingHonoursBoundaries();
    void userRegexIsUsedVerbatim();
    void invalidPatternYieldsNoMatches();
    void caseSensitivityFollowsPatternOptions();
    void batchOverloadMapsPagesToHits();

private:
    // Build a minimal but well-formed N-page PDF whose page i draws
    // pageContentStreams[i] as visible Helvetica text.
    static QByteArray makeMultiPagePdf(const QStringList &pageContentStreams);
    // Write the generated PDF into `dir`; returns false on I/O failure.
    static bool writeFixture(const QTemporaryDir &dir, const QStringList &pageStreams,
                             QString *pathOut);
};

QByteArray TestFindReplaceRegex::makeMultiPagePdf(const QStringList &pageContentStreams)
{
    const int pageCount = pageContentStreams.size();
    QByteArray pdf;
    pdf += "%PDF-1.4\n";

    QList<qint64> offsets;
    auto emitObj = [&pdf, &offsets](const QByteArray &body) {
        offsets.append(static_cast<qint64>(pdf.size()));
        pdf += QByteArray::number(offsets.size()) + " 0 obj\n" + body + "\nendobj\n";
    };

    // Object layout: 1 = catalog, 2 = pages tree, then per page (page,
    // contents), and the shared font object last.
    QString kids;
    for (int i = 0; i < pageCount; ++i)
        kids += QString("%1 0 R ").arg(3 + 2 * i);
    const int fontObjNum = 3 + 2 * pageCount;

    emitObj("<< /Type /Catalog /Pages 2 0 R >>");
    emitObj(QString("<< /Type /Pages /Kids [%1] /Count %2 >>")
                .arg(kids.trimmed()).arg(pageCount).toLatin1());
    for (int i = 0; i < pageCount; ++i) {
        const int pageNum = 3 + 2 * i;
        const int contentNum = pageNum + 1;
        emitObj(QString("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
                        "/Resources << /Font << /F1 %1 0 R >> >> /Contents %2 0 R >>")
                    .arg(fontObjNum).arg(contentNum).toLatin1());
        const QByteArray stream = pageContentStreams.at(i).toLatin1();
        emitObj("<< /Length " + QByteArray::number(stream.size())
                + " >>\nstream\n" + stream + "\nendstream");
    }
    emitObj("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");

    const qint64 xrefStart = static_cast<qint64>(pdf.size());
    pdf += "xref\n0 " + QByteArray::number(offsets.size() + 1) + "\n0000000000 65535 f \n";
    for (qint64 off : offsets)
        pdf += QString("%1 00000 n \n").arg(off, 10, 10, QLatin1Char('0')).toLatin1();
    pdf += "trailer\n<< /Size " + QByteArray::number(offsets.size() + 1)
           + " /Root 1 0 R >>\nstartxref\n" + QByteArray::number(xrefStart) + "\n%%EOF\n";
    return pdf;
}

bool TestFindReplaceRegex::writeFixture(const QTemporaryDir &dir, const QStringList &pageStreams,
                                        QString *pathOut)
{
    *pathOut = dir.filePath("fixture.pdf");
    QFile f(*pathOut);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const QByteArray pdf = makeMultiPagePdf(pageStreams);
    if (f.write(pdf) != pdf.size())
        return false;
    f.close();
    return true;
}

void TestFindReplaceRegex::escapedLiteralFindsEveryOccurrence()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path;
    QVERIFY(writeFixture(dir, { QStringLiteral("BT /F1 24 Tf 72 700 Td (Alpha beta gamma Alpha) Tj ET") },
                         &path));

    // Exactly what EditController builds for a literal search routed through
    // the regex engine: escape the user text, no \b wrapping.
    const QRegularExpression rx(QRegularExpression::escape(QStringLiteral("Alpha")));
    const QList<QRectF> hits = PatternRedactor::findMatches(path, 0, rx);
    QCOMPARE(hits.size(), 2);
    for (const QRectF &r : hits) {
        QVERIFY(!r.isNull());
        QVERIFY(r.width() > 0.0);
        QVERIFY(r.height() > 0.0);
    }
}

void TestFindReplaceRegex::wholeWordWrappingHonoursBoundaries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path;
    QVERIFY(writeFixture(dir, { QStringLiteral("BT /F1 24 Tf 72 700 Td (Alpha beta gamma Alpha) Tj ET") },
                         &path));

    // Whole-word mode (regex off): \b-wrapped escaped literal. The full word
    // matches twice; a fragment must NOT match inside "Alpha".
    const QRegularExpression wholeWord(
        QStringLiteral("\\b") + QRegularExpression::escape(QStringLiteral("Alpha")) + QStringLiteral("\\b"));
    QCOMPARE(PatternRedactor::findMatches(path, 0, wholeWord).size(), 2);

    const QRegularExpression fragment(
        QStringLiteral("\\b") + QRegularExpression::escape(QStringLiteral("lpha")) + QStringLiteral("\\b"));
    QVERIFY(PatternRedactor::findMatches(path, 0, fragment).isEmpty());
}

void TestFindReplaceRegex::userRegexIsUsedVerbatim()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path;
    QVERIFY(writeFixture(dir, { QStringLiteral("BT /F1 18 Tf 72 700 Td (mail bob@example.com thanks) Tj ET") },
                         &path));

    // Regex mode: the user pattern is passed through untouched (no wrapping).
    const QRegularExpression email(
        QStringLiteral("[A-Za-z0-9._%+\\-]+@[A-Za-z0-9.\\-]+\\.[A-Za-z]{2,}"));
    const QList<QRectF> hits = PatternRedactor::findMatches(path, 0, email);
    QCOMPARE(hits.size(), 1);
    QVERIFY(!hits.first().isNull());

    // Alternation over words present on the page.
    const QRegularExpression alt(QStringLiteral("(mail|thanks)"));
    QCOMPARE(PatternRedactor::findMatches(path, 0, alt).size(), 2);
}

void TestFindReplaceRegex::invalidPatternYieldsNoMatches()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path;
    QVERIFY(writeFixture(dir, { QStringLiteral("BT /F1 24 Tf 72 700 Td (Alpha beta) Tj ET") }, &path));

    QRegularExpression bad(QStringLiteral("(?P<"));   // unclosed named group
    QVERIFY(!bad.isValid());
    QVERIFY(PatternRedactor::findMatches(path, 0, bad).isEmpty());

    const QList<int> pages{ 0 };   // named list: binds the batch overload
    QVERIFY(PatternRedactor::findMatches(path, pages, bad).isEmpty());
}

void TestFindReplaceRegex::caseSensitivityFollowsPatternOptions()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path;
    QVERIFY(writeFixture(dir, { QStringLiteral("BT /F1 24 Tf 72 700 Td (Alpha beta gamma Alpha) Tj ET") },
                         &path));

    // Case-insensitive (FindBar's unchecked "Aa"): lowercase query finds both.
    QRegularExpression insensitive(QRegularExpression::escape(QStringLiteral("alpha")),
                                   QRegularExpression::CaseInsensitiveOption);
    QCOMPARE(PatternRedactor::findMatches(path, 0, insensitive).size(), 2);

    // Case-sensitive default: lowercase query finds none (text has "Alpha").
    QRegularExpression sensitive(QRegularExpression::escape(QStringLiteral("alpha")));
    QVERIFY(PatternRedactor::findMatches(path, 0, sensitive).isEmpty());
}

void TestFindReplaceRegex::batchOverloadMapsPagesToHits()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path;
    QVERIFY(writeFixture(dir, {
        QStringLiteral("BT /F1 24 Tf 72 700 Td (Alpha report) Tj ET"),
        QStringLiteral("BT /F1 24 Tf 72 700 Td (notes Alpha Alpha) Tj ET"),
    }, &path));

    // This is the call shape EditController uses: a named QList<int> binds the
    // batch findMatches(pdfPath, pages, pattern) overload, which parses the
    // PDF once and returns per-page hit rectangles.
    const QList<int> pages{ 0, 1 };
    const QRegularExpression rx(QRegularExpression::escape(QStringLiteral("Alpha")));
    const QHash<int, QList<QRectF>> perPage = PatternRedactor::findMatches(path, pages, rx);
    QCOMPARE(perPage.value(0).size(), 1);
    QCOMPARE(perPage.value(1).size(), 2);
    QVERIFY(perPage.value(2).isEmpty());   // absent page omitted
}

QTEST_MAIN(TestFindReplaceRegex)
#include "TestFindReplaceRegex.moc"