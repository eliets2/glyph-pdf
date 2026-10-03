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
#include <QStringDecoder>
#include <QStringList>
#include <QTemporaryDir>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfWriter>

#include <vector>

#include <podofo/podofo.h>

#include "core/PageLabels.h"
#include "engines/SafeSave.h"

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
// always written by the writer, but defaults to 1 when absent per the spec;
// /P is decoded when present and stays empty when absent). Empty result when
// /PageLabels is absent or malformed.
//
// The /P decode is §7.9.2.2-aware (wave-2b F-15): PoDoFo's GetString()
// returns UTF-8 whether the stored text string was PDFDocEncoding/ASCII or
// UTF-16BE, so a plain fromUtf8 of GetString() cannot tell them apart and
// masked the raw-UTF-8 writer bug (it self-roundtripped). The helper below
// inspects the RAW stored bytes: a FE FF BOM marks the UTF-16BE form.
static QString decodeStoredPdfTextString(const PoDoFo::PdfString& s)
{
    const std::string_view raw = s.GetRawData();
    const QByteArray bytes(raw.data(), static_cast<qsizetype>(raw.size()));
    if (bytes.size() >= 2
            && static_cast<unsigned char>(bytes.at(0)) == 0xFE
            && static_cast<unsigned char>(bytes.at(1)) == 0xFF) {
        QStringDecoder dec(QStringConverter::Utf16BE);
        return dec.decode(bytes.mid(2));
    }
    return QString::fromUtf8(bytes);
}

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
            const PoDoFo::PdfObject* p = value.GetDictionary().FindKey("P");
            if (p && p->IsString())
                e.prefix = decodeStoredPdfTextString(p->GetString());
            entries.append(e);
        }
    } catch (const std::exception& e) {
        qWarning("readNumberTree failed: %s", e.what());
        entries.clear();
    }
    return entries;
}

// G13 (QUALITY-GATE-2026-09-09): a CONTENT-BEARING fixture — real text (a
// Standard-14 Helvetica run, extractable by consumers) and a real embedded
// RGB image XObject on two pages. PoDoFo keeps the source device open for
// lazy object loading on such documents, which is exactly what the old
// same-path Save destroyed (the reviewer's 20,667-byte fixture class).
QString makeContentBearingPdf(const QString& path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        std::vector<unsigned char> pixels(24 * 24 * 3);
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 24; ++x) {
                const bool mark = (x + y) % 5 == 0;
                pixels[(y * 24 + x) * 3 + 0] = mark ? 250 : 30;
                pixels[(y * 24 + x) * 3 + 1] = mark ? 220 : 90;
                pixels[(y * 24 + x) * 3 + 2] = mark ? 40 : 200;
            }
        for (int page = 0; page < 2; ++page) {
            auto& pg = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(pg);
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText(page == 0 ? "LABELS PAGE ONE" : "LABELS PAGE TWO", 50, 700);
            if (page == 0) {
                auto img = doc.CreateImage();
                img->SetData(PoDoFo::bufferview(
                                 reinterpret_cast<const char*>(pixels.data()),
                                 pixels.size()),
                             24, 24, PoDoFo::PdfPixelFormat::RGB24);
                painter.DrawImage(*img, 300, 400, 2.0, 2.0);
            }
            painter.FinishDrawing();
        }
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning("makeContentBearingPdf failed: %s", e.what());
        return path;
    }
    return path;
}

qint64 fileBytes(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return -1;
    const QByteArray all = f.readAll();
    f.close();
    return all.size();
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
            range.GetDictionary().AddKey("St", PoDoFo::PdfObject(static_cast<std::int64_t>(99)));
            nums.Add(PoDoFo::PdfObject(static_cast<std::int64_t>(0)));
            nums.Add(range);
            nums.Add(PoDoFo::PdfObject(static_cast<std::int64_t>(1)));
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

    // ── /P prefix (ISO 32000 Table 159): text string before the number ─────

    // Write→re-read with a prefix: a non-empty prefix is stored as the
    // range's /P text string and round-trips through the saved file; the
    // computed number part is untouched by it (the prefix precedes it).
    void writeNumberTree_prefix_readback() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("pfx.pdf"));
        QVERIFY(makeNPdf(path, 3));

        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        QVERIFY(writeNumberTree(doc, 4, Style::UppercaseRoman, 3,
                                QStringLiteral("Fig.")));
        doc.Save(path.toUtf8().constData());

        // Structural readback: exactly one entry {page 0, /S (R), /St 4,
        // /P "Fig."}.
        const QList<PageLabelNumEntry> tree = readNumberTree(path);
        QCOMPARE(tree, numberTreeEntries(4, Style::UppercaseRoman, 3,
                                         QStringLiteral("Fig.")));
        QCOMPARE(tree.size(), 1);
        QCOMPARE(tree[0].pageNum, 0);
        QCOMPARE(tree[0].style, QStringLiteral("R"));
        QCOMPARE(tree[0].startValue, 4);
        QCOMPARE(tree[0].prefix, QStringLiteral("Fig."));

        // Consumer readback: the computed part regenerates unchanged; the
        // prefix precedes it (Table 159), so page 0's full label is "Fig.IV".
        QCOMPARE(labelsFor(tree[0].startValue, styleFromName(tree[0].style), 3),
                 QStringList({"IV", "V", "VI"}));
        QCOMPARE(tree[0].prefix + labelsFor(tree[0].startValue,
                                            styleFromName(tree[0].style), 3).first(),
                 QStringLiteral("Fig.IV"));
    }

    // Empty prefix: NO /P key may appear in the saved range dictionary — an
    // empty prefix must never be written (key absent, not empty-valued).
    void writeNumberTree_emptyPrefixNotWritten() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("nopfx.pdf"));
        QVERIFY(makeNPdf(path, 2));

        QVERIFY(writeNumberTree(path, 4, Style::Decimal));   // no prefix arg

        // Raw structural check: the saved range dictionary carries /S and
        // /St but no /P key at all.
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        const PoDoFo::PdfObject* labels =
            doc.GetCatalog().GetDictionary().FindKey("PageLabels");
        QVERIFY(labels != nullptr);
        if (labels->IsReference())   // PoDoFo may hand back the ref or the
            labels =                 // resolved object depending on load state
                doc.GetObjects().GetObject(labels->GetReference());
        QVERIFY(labels && labels->IsDictionary());
        const PoDoFo::PdfObject* nums =
            labels->GetDictionary().FindKey("Nums");
        QVERIFY(nums && nums->IsArray());
        QVERIFY(nums->GetArray().GetSize() == 2);
        const PoDoFo::PdfObject* range = nums->GetArray().FindAt(1);
        QVERIFY(range && range->IsDictionary());
        QVERIFY(range->GetDictionary().FindKey("S") != nullptr);
        QVERIFY(range->GetDictionary().FindKey("St") != nullptr);
        QVERIFY2(range->GetDictionary().FindKey("P") == nullptr,
                 "an empty prefix must never produce a /P key");

        // Decoded readback agrees: prefix stays empty, entry equals the
        // default (prefix-less) expectation.
        const QList<PageLabelNumEntry> tree = readNumberTree(path);
        QCOMPARE(tree, numberTreeEntries(4, Style::Decimal, 2));
        QVERIFY(tree[0].prefix.isEmpty());
    }

    // Prefix + style composition: "App-" + Decimal from 7 → the written tree
    // carries both, and the composed labels are "App-7" … "App-10".
    void writeNumberTree_prefixStyleComposition() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("comp.pdf"));
        QVERIFY(makeNPdf(path, 4));

        QVERIFY(writeNumberTree(path, 7, Style::Decimal,
                                QStringLiteral("App-")));

        const QList<PageLabelNumEntry> tree = readNumberTree(path);
        QCOMPARE(tree, numberTreeEntries(7, Style::Decimal, 4,
                                         QStringLiteral("App-")));
        QCOMPARE(tree[0].style, QStringLiteral("D"));
        QCOMPARE(tree[0].startValue, 7);
        QCOMPARE(tree[0].prefix, QStringLiteral("App-"));

        QStringList composed;
        const QStringList computed = labelsFor(7, Style::Decimal, 4);
        for (const QString& label : computed)
            composed.append(QStringLiteral("App-") + label);
        QCOMPARE(composed, QStringList({"App-7", "App-8", "App-9", "App-10"}));
    }

    // ── Wave-2b F-15: a NON-ASCII /P prefix is a §7.9.2.2 text string ──────
    // The spec demands PDFDocEncoding or UTF-16BE (with the FE FF BOM); the
    // pre-fix writer handed PoDoFo raw UTF-8 bytes, which SELF-ROUNDTRIP
    // through a UTF-8 decoder (the in-tree gate was blind) but render as
    // mojibake in every spec-conforming viewer. Two oracles:
    //  1. QPdfDocument (pdfium) — a real third-party consumer reading the
    //     page label per spec — must show "Kapitel–4", not mojibake.
    //  2. The stored /P string's RAW bytes must carry the UTF-16BE BOM.
    void writeNumberTree_prefixNonAsciiIsUtf16BETextString() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("pfx16be.pdf"));
        QVERIFY(makeNPdf(path, 3));
        const QString prefix = QStringLiteral("Kapitel\u2013"); // en dash — outside ASCII/PDFDocEncoding

        QVERIFY(writeNumberTree(path, 4, Style::Decimal, prefix));

        // Oracle 1: pdfium's page-label decoder.
        {
            QPdfDocument doc;
            QCOMPARE(doc.load(path), QPdfDocument::Error::None);
            QVERIFY2(doc.pageCount() == 3, "fixture pages must survive");
            QCOMPARE(doc.pageLabel(0), prefix + QStringLiteral("4"));
            QCOMPARE(doc.pageLabel(1), prefix + QStringLiteral("5"));
        }

        // Oracle 2: the stored text string is the UTF-16BE form (BOM'd).
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        const PoDoFo::PdfObject* labels =
            doc.GetCatalog().GetDictionary().FindKey("PageLabels");
        QVERIFY(labels != nullptr);
        if (labels->IsReference())
            labels = doc.GetObjects().GetObject(labels->GetReference());
        QVERIFY(labels && labels->IsDictionary());
        const PoDoFo::PdfObject* nums = labels->GetDictionary().FindKey("Nums");
        QVERIFY(nums && nums->IsArray());
        const PoDoFo::PdfObject* range0 = nums->GetArray().FindAt(1);
        QVERIFY(range0 && range0->IsDictionary());
        const PoDoFo::PdfObject* p = range0->GetDictionary().FindKey("P");
        QVERIFY2(p && p->IsString(), "non-empty prefix must produce a /P string");
        const std::string_view raw = p->GetString().GetRawData();
        const QByteArray rawBytes(raw.data(), static_cast<qsizetype>(raw.size()));
        QVERIFY2(rawBytes.size() >= 2
                     && static_cast<unsigned char>(rawBytes.at(0)) == 0xFE
                     && static_cast<unsigned char>(rawBytes.at(1)) == 0xFF,
                 "a non-ASCII /P must be stored as the UTF-16BE+BOM text "
                 "string, not raw UTF-8 bytes");

        // Spec-aware readback round-trips through readNumberTree too.
        const QList<PageLabelNumEntry> tree = readNumberTree(path);
        QCOMPARE(tree, numberTreeEntries(4, Style::Decimal, 3, prefix));
        QCOMPARE(tree[0].prefix, prefix);
    }

    // The PagesMode staged SafeSave flow (candidate → label → commit) must
    // carry the prefix into the committed original, with content intact.
    void writeNumberTreePrefixSafeSaveRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString original = dir.filePath(QStringLiteral("pfx_g13.pdf"));
        makeContentBearingPdf(original);

        // PagesMode::onApplyPageLabels: candidate → copy → label → commit.
        QString candidate;
        QString err;
        QVERIFY(gp::SafeSave::makeUniqueCandidate(&candidate, &err));
        QFile::remove(candidate);
        QVERIFY(QFile::copy(original, candidate));
        QVERIFY2(writeNumberTree(candidate, 1, Style::LowercaseRoman,
                                 QStringLiteral("pref-")),
                 "labeling the staged candidate with a prefix must succeed");
        QVERIFY2(gp::SafeSave::commitFileToDestination(candidate, original, &err),
                 qPrintable(err));
        QFile::remove(candidate);

        const QList<PageLabelNumEntry> tree = readNumberTree(original);
        QCOMPARE(tree, numberTreeEntries(1, Style::LowercaseRoman, 2,
                                         QStringLiteral("pref-")));
        QCOMPARE(tree[0].prefix, QStringLiteral("pref-"));

        QPdfDocument doc;
        QCOMPARE(doc.load(original), QPdfDocument::Error::None);
        QVERIFY2(doc.getAllText(0).text().contains(QStringLiteral("LABELS PAGE ONE")),
                 "the committed original must keep its content");
    }

    // ── G13 (QUALITY-GATE-2026-09-09) ────────────────────────────────────────
    // THE anchor: the path overload used to Load() and Save() the SAME file.
    // PoDoFo keeps the source device open for lazy object loading, so the
    // same-path Save truncated the device before the deferred streams were
    // flushed: this content-bearing fixture shrank from ~20 KB to 0 bytes,
    // the call returned false, and the file could not be reopened.
    void writeNumberTreeContentBearingFileSurvivesAndCarriesTree()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("g13_content.pdf"));
        makeContentBearingPdf(path);
        const qint64 before = fileBytes(path);
        // The hazard class is a LAZY-LOADED content stream, not absolute size:
        // a two-page text+image document (Standard-14 text run, embedded RGB
        // XObject) defers object parsing; the old same-path Save truncated it.
        QVERIFY2(before > 1000,
                 qPrintable(QStringLiteral("G13 precondition: the fixture must be a "
                                           "real content-bearing document (got %1 bytes)")
                                .arg(before)));

        QVERIFY2(writeNumberTree(path, 1, Style::Decimal),
                 "G13: labeling a content-bearing document must succeed");

        // The file survived: non-empty, reopens, page count intact.
        const qint64 after = fileBytes(path);
        QVERIFY2(after > 0, "G13: the labeled file must not be truncated");
        QVERIFY2(after > before / 2,
                 qPrintable(QStringLiteral("G13: the labeled file must keep the document "
                                           "content (before=%1 after=%2)").arg(before).arg(after)));

        const QList<PageLabelNumEntry> tree = readNumberTree(path);
        QCOMPARE(tree, numberTreeEntries(1, Style::Decimal, 2));

        // The actual page CONTENT survived (lazy streams flushed, not lost):
        // both text markers readable via a real consumer.
        QPdfDocument doc;
        QCOMPARE(doc.load(path), QPdfDocument::Error::None);
        QCOMPARE(doc.pageCount(), 2);
        QVERIFY2(doc.getAllText(0).text().contains(QStringLiteral("LABELS PAGE ONE")),
                 "G13: page-one text must survive the labeling transaction");
        QVERIFY2(doc.getAllText(1).text().contains(QStringLiteral("LABELS PAGE TWO")),
                 "G13: page-two text must survive the labeling transaction");
    }

    // A refused COMMIT must leave the caller's file byte-identical — direct
    // API callers must not lose their file even when the commit is blocked.
    void writeNumberTreeFailingCommitLeavesFileByteIdentical()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("g13_fault.pdf"));
        makeContentBearingPdf(path);
        QByteArray before;
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadOnly));
            before = f.readAll();
        }

        gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
        const bool ok = writeNumberTree(path, 4, Style::Decimal);
        gp::SafeSave::setCommitFaultForTesting(gp::SafeSave::CommitFaultForTesting::None);
        QVERIFY2(!ok, "G13: a faulted commit must report failure");

        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray after = f.readAll();
        f.close();
        QVERIFY2(before == after,
                 "G13: a failed commit must leave the destination byte-identical");
        QVERIFY(readNumberTree(path).isEmpty());   // no half-applied tree either
    }

    // The actual PagesMode caller path (staged SafeSave candidate →
    // writeNumberTree on the candidate → commit to the original) must label
    // content-bearing documents end-to-end.
    void writeNumberTreePagesModeCandidateFlowEndToEnd()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString original = dir.filePath(QStringLiteral("g13_ui.pdf"));
        makeContentBearingPdf(original);

        // PagesMode::onApplyPageLabels: candidate → copy → label → commit.
        QString candidate;
        QString err;
        QVERIFY(gp::SafeSave::makeUniqueCandidate(&candidate, &err));
        QFile::remove(candidate);
        QVERIFY(QFile::copy(original, candidate));
        QVERIFY2(writeNumberTree(candidate, 3, Style::LowercaseRoman),
                 "G13: labeling the staged candidate must succeed for a "
                 "content-bearing document");
        QVERIFY2(gp::SafeSave::commitFileToDestination(candidate, original, &err),
                 qPrintable(err));
        QFile::remove(candidate);

        const QList<PageLabelNumEntry> tree = readNumberTree(original);
        QCOMPARE(tree, numberTreeEntries(3, Style::LowercaseRoman, 2));
        QPdfDocument doc;
        QCOMPARE(doc.load(original), QPdfDocument::Error::None);
        QVERIFY2(doc.getAllText(0).text().contains(QStringLiteral("LABELS PAGE ONE")),
                 "G13: the committed original must keep its content");
    }
};

QTEST_MAIN(TestPageLabels)
#include "TestPageLabels.moc"
