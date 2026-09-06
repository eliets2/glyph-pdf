/**
 * TestPageLabels — §9.9 P1 groundwork: pure /PageLabels seam.
 *
 * Scope: gp::PageLabels generates PDF /PageLabels number-tree entries and the
 * matching human-readable page-label strings for a single (startValue, style,
 * pageCount) range covering the four PDF /S naming styles — Decimal (D),
 * LowercaseRoman (r), UppercaseRoman (R), LowercaseLetters (a) and
 * UppercaseLetters (A). Groundwork for a future Page-Labels mode; the suite
 * pins the label math and the number-tree shape BEFORE any document writer
 * exists (verified: Bates numbering stamps visible text only and never
 * writes /PageLabels).
 *
 * Pure data test — no GUI, no document I/O. Runs with QTEST_GUILESS_MAIN.
 *
 * Run:
 *   ctest -R TestPageLabels --output-on-failure
 */

#include <QtTest/QtTest>
#include <QString>
#include <QStringList>

#include "core/PageLabels.h"

using gp::PageLabelNumEntry;
using gp::PageLabels::labelsFor;
using gp::PageLabels::numberTreeEntries;
using gp::PageLabels::Style;
using gp::PageLabels::styleName;

class TestPageLabels : public QObject {
    Q_OBJECT

private slots:
    // ── style → PDF /S name mapping (ISO 32000 Table 159) ────────────────
    void styleNameMapping() {
        QCOMPARE(styleName(Style::Decimal), QStringLiteral("D"));
        QCOMPARE(styleName(Style::LowercaseRoman), QStringLiteral("r"));
        QCOMPARE(styleName(Style::UppercaseRoman), QStringLiteral("R"));
        QCOMPARE(styleName(Style::LowercaseLetters), QStringLiteral("a"));
        QCOMPARE(styleName(Style::UppercaseLetters), QStringLiteral("A"));
    }

    // ── Decimal labels: plain 1-based counting from startValue ───────────
    void labelsFor_decimal() {
        QCOMPARE(labelsFor(4, Style::Decimal, 3), QStringList({"4", "5", "6"}));
        QCOMPARE(labelsFor(1, Style::Decimal, 1), QStringList({"1"}));
        QCOMPARE(labelsFor(98, Style::Decimal, 3), QStringList({"98", "99", "100"}));
    }

    // ── Roman numerals, uppercase ─────────────────────────────────────────
    void labelsFor_uppercaseRoman() {
        QCOMPARE(labelsFor(1, Style::UppercaseRoman, 4),
                 QStringList({"I", "II", "III", "IV"}));
        QCOMPARE(labelsFor(9, Style::UppercaseRoman, 3),
                 QStringList({"IX", "X", "XI"}));
        QCOMPARE(labelsFor(1990, Style::UppercaseRoman, 1),
                 QStringList({"MCMXC"}));
    }

    // ── Roman numerals, lowercase ─────────────────────────────────────────
    void labelsFor_lowercaseRoman() {
        QCOMPARE(labelsFor(1, Style::LowercaseRoman, 3),
                 QStringList({"i", "ii", "iii"}));
        QCOMPARE(labelsFor(4, Style::LowercaseRoman, 3),
                 QStringList({"iv", "v", "vi"}));
    }

    // ── Letters: bijective base-26 (…Y, Z, AA, AB…) ──────────────────────
    void labelsFor_letters() {
        QCOMPARE(labelsFor(1, Style::UppercaseLetters, 3),
                 QStringList({"A", "B", "C"}));
        QCOMPARE(labelsFor(25, Style::UppercaseLetters, 3),
                 QStringList({"Y", "Z", "AA"}));
        QCOMPARE(labelsFor(1, Style::LowercaseLetters, 2),
                 QStringList({"a", "b"}));
        QCOMPARE(labelsFor(27, Style::LowercaseLetters, 2),
                 QStringList({"aa", "ab"}));
    }

    // ── Invalid arguments produce empty output (no guessed labels) ───────
    void labelsFor_invalidArgs() {
        QVERIFY(labelsFor(1, Style::Decimal, 0).isEmpty());
        QVERIFY(labelsFor(1, Style::Decimal, -3).isEmpty());
        QVERIFY(labelsFor(0, Style::Decimal, 5).isEmpty());
        QVERIFY(labelsFor(-2, Style::UppercaseRoman, 5).isEmpty());
    }

    // ── Roman is only representable up to 3999 — beyond that the label ──
    // position is honestly empty rather than silently wrong.
    void labelsFor_romanBeyond3999() {
        QCOMPARE(labelsFor(3999, Style::UppercaseRoman, 2),
                 QStringList({"MMMCMXCIX", QString()}));
    }

    // ── Number-tree entries: one /Nums pair covering the whole range ─────
    void numberTreeEntries_singleRange() {
        const QList<PageLabelNumEntry> decimal = numberTreeEntries(4, Style::Decimal, 3);
        QCOMPARE(decimal.size(), 1);
        QCOMPARE(decimal[0].pageNum, 0);
        QCOMPARE(decimal[0].style, QStringLiteral("D"));
        QCOMPARE(decimal[0].startValue, 4);

        const QList<PageLabelNumEntry> roman = numberTreeEntries(1, Style::LowercaseRoman, 12);
        QCOMPARE(roman.size(), 1);
        QCOMPARE(roman[0].pageNum, 0);
        QCOMPARE(roman[0].style, QStringLiteral("r"));
        QCOMPARE(roman[0].startValue, 1);
    }

    void numberTreeEntries_invalidArgs() {
        QVERIFY(numberTreeEntries(1, Style::Decimal, 0).isEmpty());
        QVERIFY(numberTreeEntries(0, Style::Decimal, 5).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestPageLabels)
#include "TestPageLabels.moc"
