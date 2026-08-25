// SPDX-License-Identifier: Apache-2.0
// PRD 9.15 - focused tests for the regex / whole-word document-text Find path.
//
// EditController routes regex and whole-word searches through
// PatternRedactor::findMatches() (PDFium per-character boxes plus
// QRegularExpression), so these tests exercise that extraction directly
// against a tiny generated PDF: match discovery, regex alternation,
// whole-word wrapping (no substring hits), case-insensitivity, and the
// invalid-pattern guard.

#include <QtTest>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>

#include "engines/PatternRedactor.h"

namespace {

// Write a minimal single-page PDF containing "alpha beta gamma alpha".
// Objects are emitted with a hand-built xref table so PDFium parses it cleanly.
bool writeTinyPdf(const QString &path)
{
    QByteArray pdf;
    qint64 offset[6] = { 0, 0, 0, 0, 0, 0 };

    auto emitObj = [&pdf, &offset](int num, const QByteArray &body) {
        offset[num] = pdf.size();
        pdf += QByteArray::number(num) + " 0 obj\n" + body + "\nendobj\n";
    };

    pdf += "%PDF-1.4\n";
    emitObj(1, "<< /Type /Catalog /Pages 2 0 R >>");
    emitObj(2, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    emitObj(3, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
                  "/Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>");
    const QByteArray content = "BT /F1 24 Tf 72 700 Td (alpha beta gamma alpha) Tj ET\n";
    emitObj(4, "<< /Length " + QByteArray::number(content.size())
                  + " >>\nstream\n" + content + "endstream");
    emitObj(5, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");

    const qint64 xrefStart = pdf.size();
    pdf += "xref\n0 6\n0000000000 65535 f \n";
    for (int i = 1; i <= 5; ++i) {
        pdf += QString("%1 00000 n \n").arg(offset[i], 10, 10, QLatin1Char('0')).toLatin1();
    }
    pdf += "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n"
           + QByteArray::number(xrefStart) + "\n%%EOF\n";

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const bool ok = f.write(pdf) == pdf.size();
    f.close();
    return ok;
}

} // namespace

class TestFindReplaceRegex : public QObject
{
    Q_OBJECT

private slots:
    // Baseline: a plain-literal regex finds every occurrence with valid rects.
    void literalPatternFindsAllOccurrences()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pdf = dir.filePath("tiny.pdf");
        QVERIFY(writeTinyPdf(pdf));

        const auto perPage = PatternRedactor::findMatches(
            pdf, QList<int>{ 0 }, QRegularExpression(QStringLiteral("alpha")));
        QCOMPARE(perPage.size(), 1);
        QVERIFY(perPage.contains(0));
        QCOMPARE(perPage.value(0).size(), 2);          // "alpha" appears twice
        for (const QRectF &r : perPage.value(0)) {
            QVERIFY(!r.isNull());
            QVERIFY(r.width() > 0.0);
            QVERIFY(r.height() > 0.0);
        }
    }

    // Regex-specific capability the literal QPdfSearchModel path lacks.
    void alternationPatternFindsDisjointWords()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pdf = dir.filePath("tiny.pdf");
        QVERIFY(writeTinyPdf(pdf));

        const auto perPage = PatternRedactor::findMatches(
            pdf, QList<int>{ 0 }, QRegularExpression(QStringLiteral("alpha|gamma")));
        QCOMPARE(perPage.value(0).size(), 3);          // alpha, gamma, alpha
    }

    // Whole-word semantics: a boundary-wrapped literal must NOT match inside
    // a longer word.
    void wholeWordWrapExcludesSubstringHits()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pdf = dir.filePath("tiny.pdf");
        QVERIFY(writeTinyPdf(pdf));

        // "bet" occurs only inside "beta", never as a standalone word.
        const QString wrapped = QStringLiteral("\\b")
                                + QRegularExpression::escape(QStringLiteral("bet"))
                                + QStringLiteral("\\b");
        const auto perPage = PatternRedactor::findMatches(
            pdf, QList<int>{ 0 }, QRegularExpression(wrapped));
        QVERIFY(!perPage.contains(0));                 // no whole-word hit

        // Sanity: unwrapped, the substring IS found.
        const auto sub = PatternRedactor::findMatches(
            pdf, QList<int>{ 0 },
            QRegularExpression(QRegularExpression::escape(QStringLiteral("bet"))));
        QCOMPARE(sub.value(0).size(), 1);
    }

    // Case-insensitivity travels with the pattern (matchCase=false path).
    void caseInsensitiveOptionIsHonoured()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pdf = dir.filePath("tiny.pdf");
        QVERIFY(writeTinyPdf(pdf));

        QRegularExpression rx(QStringLiteral("ALPHA"),
                              QRegularExpression::CaseInsensitiveOption);
        QCOMPARE(PatternRedactor::findMatches(pdf, QList<int>{ 0 }, rx).value(0).size(), 2);

        rx.setPatternOptions(QRegularExpression::NoPatternOption);
        QVERIFY(PatternRedactor::findMatches(pdf, QList<int>{ 0 }, rx).isEmpty());
    }

    // A syntactically invalid user pattern yields no matches (and never hangs):
    // EditController reports it instead of feeding garbage to the matcher.
    void invalidPatternReturnsEmptyWithoutHanging()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pdf = dir.filePath("tiny.pdf");
        QVERIFY(writeTinyPdf(pdf));

        QRegularExpression bad(QStringLiteral("(?P<"));   // unclosed named group
        QVERIFY(!bad.isValid());
        QVERIFY(PatternRedactor::findMatches(pdf, QList<int>{ 0 }, bad).isEmpty());
    }
};

QTEST_MAIN(TestFindReplaceRegex)
#include "TestFindReplaceRegex.moc"
