// SPDX-License-Identifier: Apache-2.0
// §9.15 regression test: Match Case / Whole Words / Use Regex must actually
// shape the document-text search. The flags are applied by
// EditController::pageTextPattern(), the shared matcher used by both the
// document-text search path and the redact-all path.
#include <QtTest/QtTest>
#include "shell/controllers/EditController.h"

using gp::EditController;

class TestSearchFlags : public QObject {
    Q_OBJECT
private slots:
    void noFlagsMeansInactive();          // plain substring path (QPdfSearchModel)
    void matchCaseIsCaseSensitive();
    void matchCaseOffIsCaseInsensitive();
    void wholeWordsRejectsSubstrings();
    void regexPatternIsHonoured();
    void invalidRegexIsReported();
    void wholeWordsWrapsRegex();
};

void TestSearchFlags::noFlagsMeansInactive() {
    const auto pt = EditController::pageTextPattern(QStringLiteral("invoice"),
                                                    /*matchCase*/false,
                                                    /*wholeWords*/false,
                                                    /*useRegex*/false);
    QVERIFY(!pt.active);
}

void TestSearchFlags::matchCaseIsCaseSensitive() {
    const auto pt = EditController::pageTextPattern(QStringLiteral("Invoice"),
                                                    /*matchCase*/true, false, false);
    QVERIFY(pt.active);
    QVERIFY(pt.rx.isValid());
    QVERIFY(pt.rx.match(QStringLiteral("see the Invoice attached")).hasMatch());
    QVERIFY(!pt.rx.match(QStringLiteral("see the invoice attached")).hasMatch());
}

void TestSearchFlags::matchCaseOffIsCaseInsensitive() {
    // matchCase=false alone is the inactive (QPdfSearchModel) path; exercise
    // case-insensitivity through an active combination (wholeWords on).
    const auto pt = EditController::pageTextPattern(QStringLiteral("Invoice"),
                                                    /*matchCase*/false,
                                                    /*wholeWords*/true, false);
    QVERIFY(pt.active);
    QVERIFY(pt.rx.match(QStringLiteral("see the invoice attached")).hasMatch());
    QVERIFY(pt.rx.match(QStringLiteral("see the INVOICE attached")).hasMatch());
}

void TestSearchFlags::wholeWordsRejectsSubstrings() {
    const auto pt = EditController::pageTextPattern(QStringLiteral("cat"),
                                                    false, /*wholeWords*/true, false);
    QVERIFY(pt.active);
    QVERIFY(pt.rx.match(QStringLiteral("the cat sat")).hasMatch());
    // Substring inside a longer word must NOT match when Whole Words is set.
    QVERIFY(!pt.rx.match(QStringLiteral("the category")).hasMatch());
    // Punctuation counts as a word boundary.
    QVERIFY(pt.rx.match(QStringLiteral("(cat), dog")).hasMatch());
}

void TestSearchFlags::regexPatternIsHonoured() {
    const auto pt = EditController::pageTextPattern(QStringLiteral("INV-\\d{4}"),
                                                    false, false, /*useRegex*/true);
    QVERIFY(pt.active);
    QVERIFY(pt.rx.isValid());
    QVERIFY(pt.rx.match(QStringLiteral("reference INV-2024 filed")).hasMatch());
    QVERIFY(!pt.rx.match(QStringLiteral("reference INV-24 filed")).hasMatch());
}

void TestSearchFlags::invalidRegexIsReported() {
    const auto pt = EditController::pageTextPattern(QStringLiteral("INV-(\\d{4}"),
                                                    false, false, /*useRegex*/true);
    QVERIFY(pt.active);
    QVERIFY(!pt.rx.isValid()); // caller must surface "Invalid regular expression."
}

void TestSearchFlags::wholeWordsWrapsRegex() {
    const auto pt = EditController::pageTextPattern(QStringLiteral("cat"),
                                                    false, /*wholeWords*/true,
                                                    /*useRegex*/true);
    QVERIFY(pt.active);
    QVERIFY(pt.rx.isValid());
    QVERIFY(pt.rx.match(QStringLiteral("the cat sat")).hasMatch());
    QVERIFY(!pt.rx.match(QStringLiteral("the category")).hasMatch());
}

QTEST_MAIN(TestSearchFlags)
#include "TestSearchFlags.moc"
