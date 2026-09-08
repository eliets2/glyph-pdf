/**
 * TestPageLabels — §9.9 P1: pure /PageLabels seam + the catalog WRITER.
 *
 * Scope: gp::PageLabels generates PDF /PageLabels number-tree entries and the
 * matching human-readable page-label strings for a single (startValue, style,
 * pageCount) range covering the PDF /S naming styles — Decimal (D),
 * LowercaseRoman (r), UppercaseRoman (R), LowercaseLetters (a) and
 * UppercaseLetters (A) — and WRITES them into a document catalog as a proper
 * /Nums number tree (writeNumberTree), replacing any pre-existing tree.
 *
 * Contract pinned here for the writer: after writeNumberTree, re-reading the
 * saved document with PoDoFo shows catalog /PageLabels with /Nums entries that
 * reproduce labelsFor's output (PoDoFo 1.1.0 has no page-label read API, so
 * the readback decodes the tree structure directly per ISO 32000 Table 159).
 *
 * Runs with QTEST_GUILESS_MAIN (offscreen); needs the podofo DLL at runtime.
 *
 * Run:
 *   ctest -R TestPageLabels --output-on-failure
 */

#include <QtTest/QtTest>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include <podofo/podofo.h>

#include "core/PageLabels.h"

using gp::PageLabelNumEntry;
using gp::PageLabels::labelsFor;
using gp::PageLabels::numberTreeEntries;
using gp::PageLabels::Style;
using gp::PageLabels::styleName;
using gp::PageLabels::writeNumberTree;

namespace {

// Build a minimal n-page PDF at `path` via PoDoFo. Returns true on success.
bool makeNPdf(const QString& path, int pageCount)
{
    try {
        PoDoFo::PdfMemDocument doc;
        for (int i = 0; i < pageCount; ++i) {
            doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        }
        doc.Save(path.toUtf8().constData());
        return true;
    } catch (const std::exception& e) {
        qWarning("makeNPdf failed: %s", e.what());
        return false;
    }
}

// The Style whose /S name is `name` — inverse of styleName(); Decimal on an
// unknown name (never produced by the writer).
Style styleFromName(const QString& name)
{
    if (name == QLatin1String("r")) return Style::LowercaseRoman;
    if (name == QLatin1String("R")) return Style::UppercaseRoman;
    if (name == QLatin1String("a")) return Style::LowercaseLetters;
    if (name == QLatin1String("A")) return Style::UppercaseLetters;
    return Style::Decimal;
}

// Read back a SAVED document's catalog /PageLabels number tree, decoding the
// flat /Nums array [pageNum, dict, …] into PageLabelNumEntry values (/St is
// always written by the writer, but defaults to 1 when absent per the spec).
// Empty result when /PageLabels is absent or malformed.
QList<PageLabelNumEntry> readNumberTree(const QString& path)
{
    QList<PageLabelNumEntry> entries;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        const PoDoFo::PdfObject* labels =
            doc.GetCatalog().GetDictionary().FindKey("PageLabels");
        if (!labels || !labels->IsDictionary()) return entries;
        const PoDoFo::PdfObject* nums =
            labels->GetDictionary().FindKey("Nums");
        if (!nums || !nums->IsArray()) return entries;
        const PoDoFo::PdfArray& arr = nums->GetArray();
        if (arr.GetSize() % 2 != 0) return entries; // malformed pairs
        for (unsigned i = 0; i + 1 < arr.GetSize(); i += 2) {
            const PoDoFo::PdfObject& key   = arr[i];
            const PoDoFo::PdfObject& value = arr[i + 1];
            if (!key.IsNumber() || !value.IsDictionary()) return entries;
            PageLabelNumEntry e;
            e.pageNum = static_cast<int>(key.GetNumber());
            const PoDoFo::PdfObject* s = value.GetDictionary().FindKey("S");
            if (s && s->IsName())
                e.style = QString::fromUtf8(s->GetName().GetString());
            const PoDoFo::PdfObject* st = value.GetDictionary().FindKey("St");
            e.startValue = (st && st->IsNumber())
                ? static_cast<int>(st->GetNumber()) : 1;
            entries.append(e);
        }
    } catch (const std::exception& e) {
        qWarning("readNumberTree failed: %s", e.what());
        entries.clear();
    }
    return entries;
}

} // namespace

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

    // ── Letters: PDF repeated-letter cycles (a-z, aa, bb … zz, aaa, bbb …) ──
    // N02 correction: ISO 32000-1 Table 159 lowercase/uppercase-letter styles
    // repeat the SAME letter per cycle (27='aa', 28='bb', 52='zz',
    // 53='aaa'), matching PDFium's decoder — NOT spreadsheet bijective
    // base-26 (which wrongly gives 27='aa', 28='ab'). The previous
    // expectations here pinned the spreadsheet scheme; they are corrected to
    // the spec scheme this commit. Pinned boundaries per the review: 1,
    // 25–28, 52–54 and a nondefault start.
    void labelsFor_letters() {
        QCOMPARE(labelsFor(1, Style::UppercaseLetters, 3),
                 QStringList({"A", "B", "C"}));
        QCOMPARE(labelsFor(25, Style::UppercaseLetters, 3),
                 QStringList({"Y", "Z", "AA"}));
        QCOMPARE(labelsFor(27, Style::UppercaseLetters, 2),
                 QStringList({"AA", "BB"}));
        QCOMPARE(labelsFor(52, Style::UppercaseLetters, 3),
                 QStringList({"ZZ", "AAA", "BBB"}));
        QCOMPARE(labelsFor(1, Style::LowercaseLetters, 2),
                 QStringList({"a", "b"}));
        QCOMPARE(labelsFor(26, Style::LowercaseLetters, 3),
                 QStringList({"z", "aa", "bb"}));
        QCOMPARE(labelsFor(28, Style::LowercaseLetters, 1),
                 QStringList({"bb"}));
        QCOMPARE(labelsFor(52, Style::LowercaseLetters, 2),
                 QStringList({"zz", "aaa"}));
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

    // ── §9.9 P1 WRITER ─────────────────────────────────────────────────────
    // Contract: after writeNumberTree, re-reading the saved doc with PoDoFo
    // shows catalog /PageLabels /Nums matching numberTreeEntries/labelsFor.

    // Decimal style, non-default start value, saved to disk and re-read.
    void writeNumberTree_decimal_readback() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("dec.pdf"));
        QVERIFY(makeNPdf(path, 3));

        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        QVERIFY(writeNumberTree(doc, 4, Style::Decimal, 3));
        doc.Save(path.toUtf8().constData());

        // Structural readback: exactly one entry {page 0, /S (D), /St 4}.
        const QList<PageLabelNumEntry> tree = readNumberTree(path);
        QCOMPARE(tree, numberTreeEntries(4, Style::Decimal, 3));
        QCOMPARE(tree.size(), 1);
        QCOMPARE(tree[0].pageNum, 0);
        QCOMPARE(tree[0].style, QStringLiteral("D"));
        QCOMPARE(tree[0].startValue, 4);

        // Consumer readback: the decoded entry regenerates labelsFor output.
        QCOMPARE(labelsFor(tree[0].startValue, styleFromName(tree[0].style), 3),
                 QStringList({"4", "5", "6"}));
    }

    // Roman style from 1: /St 1 must be written explicitly so the readback
    // is exact, and the decoded entry must regenerate the roman sequence.
    void writeNumberTree_roman_readback() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("rom.pdf"));
        QVERIFY(makeNPdf(path, 12));

        QVERIFY(writeNumberTree(path, 1, Style::LowercaseRoman));

        const QList<PageLabelNumEntry> tree = readNumberTree(path);
        QCOMPARE(tree, numberTreeEntries(1, Style::LowercaseRoman, 12));
        QCOMPARE(tree.size(), 1);
        QCOMPARE(tree[0].style, QStringLiteral("r"));
        QCOMPARE(tree[0].startValue, 1);
        QCOMPARE(labelsFor(tree[0].startValue, styleFromName(tree[0].style), 12),
                 labelsFor(1, Style::LowercaseRoman, 12));
        QCOMPARE(labelsFor(1, Style::LowercaseRoman, 12).last(),
                 QStringLiteral("xii"));
    }

    // Writing onto a document that ALREADY carries a /PageLabels tree must
    // REPLACE it (no stale /Nums entries survive from the previous tree).
    void writeNumberTree_replacesExistingTree() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("repl.pdf"));
        QVERIFY(makeNPdf(path, 2));

        // Pre-seed a bogus two-entry tree directly in the file.
        {
            PoDoFo::PdfMemDocument doc;
            doc.Load(path.toUtf8().constData());
            auto& labels = doc.GetObjects().CreateDictionaryObject();
            PoDoFo::PdfArray nums;
            auto& range = doc.GetObjects().CreateDictionaryObject();
            range.GetDictionary().AddKey("S", PoDoFo::PdfObject(PoDoFo::PdfName("A")));
            range.GetDictionary().AddKey("St", PoDoFo::PdfObject(static_cast<long long>(99)));
            nums.Add(PoDoFo::PdfObject(static_cast<long long>(0)));
            nums.Add(range);
            nums.Add(PoDoFo::PdfObject(static_cast<long long>(1)));
            nums.Add(range);
            labels.GetDictionary().AddKey("Nums", PoDoFo::PdfObject(nums));
            doc.GetCatalog().GetDictionary().AddKey("PageLabels",
                PoDoFo::PdfObject(labels.GetIndirectReference()));
            doc.Save(path.toUtf8().constData());
        }
        QCOMPARE(readNumberTree(path).size(), 2); // seeded junk is really there

        // Overwrite with a single-entry decimal tree via the path overload.
        QVERIFY(writeNumberTree(path, 4, Style::Decimal));

        const QList<PageLabelNumEntry> tree = readNumberTree(path);
        QCOMPARE(tree.size(), 1);                      // old entries are gone
        QCOMPARE(tree, numberTreeEntries(4, Style::Decimal, 2));
    }

    // Start-value offset through the path overload: /St carries the offset.
    void writeNumberTree_startValueOffset() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("off.pdf"));
        QVERIFY(makeNPdf(path, 4));

        QVERIFY(writeNumberTree(path, 7, Style::Decimal));

        const QList<PageLabelNumEntry> tree = readNumberTree(path);
        QCOMPARE(tree.size(), 1);
        QCOMPARE(tree[0].pageNum, 0);
        QCOMPARE(tree[0].startValue, 7);
        QCOMPARE(labelsFor(tree[0].startValue, styleFromName(tree[0].style), 4),
                 QStringList({"7", "8", "9", "10"}));
    }

    // Invalid arguments: no tree is written, none is created, and the call
    // reports failure instead of silently producing a broken dictionary.
    void writeNumberTree_invalidArgs() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("inv.pdf"));
        QVERIFY(makeNPdf(path, 2));

        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        QVERIFY(!writeNumberTree(doc, 0, Style::Decimal, 3));
        QVERIFY(!writeNumberTree(doc, 1, Style::Decimal, 0));
        QVERIFY(!writeNumberTree(doc, -2, Style::UppercaseRoman, 5));
        QVERIFY(doc.GetCatalog().GetDictionary().FindKey("PageLabels") == nullptr);

        // Path overload on the same file: still no catalog /PageLabels.
        QVERIFY(!writeNumberTree(path, 0, Style::Decimal));
        QVERIFY(readNumberTree(path).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestPageLabels)
#include "TestPageLabels.moc"
