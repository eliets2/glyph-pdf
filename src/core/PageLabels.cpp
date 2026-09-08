// SPDX-License-Identifier: Apache-2.0
/**
 * PageLabels.cpp — /PageLabels seam implementation: pure entry/label
 * generation plus the §9.9 P1 catalog writer. See PageLabels.h for the
 * scope and contracts (pinned by tests/TestPageLabels.cpp).
 */
#include "PageLabels.h"

#include <podofo/podofo.h>

namespace gp {

namespace {

// Classic subtractive roman numerals for 1..3999; empty beyond the
// representable range (PDF viewers would render nothing sensible either).
QString romanNumeral(int value, bool uppercase)
{
    if (value < 1 || value > 3999)
        return QString();

    struct Symbol { int value; const char* glyphs; };
    static constexpr Symbol kTable[] = {
        {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"},
        {100, "C"},  {90, "XC"},  {50, "L"},  {40, "XL"},
        {10, "X"},   {9, "IX"},   {5, "V"},   {4, "IV"},
        {1, "I"},
    };

    QString out;
    for (const Symbol& s : kTable) {
        while (value >= s.value) {
            out += QLatin1String(s.glyphs);
            value -= s.value;
        }
    }
    return uppercase ? out : out.toLower();
}

// PDF letter styles (/S (a) and /S (A), ISO 32000-1 Table 159) use
// repeated-letter cycles, NOT spreadsheet bijective base-26: 1..26 are
// a..z, then each cycle repeats the SAME letter prefix — 27='aa',
// 28='bb', 52='zz', 53='aaa' (N02; matches PDFium's decoder). For value n
// the cycle letter is 'a' + (n-1)%26 and it repeats (n-1)/26 + 1 times.
// Unbounded for practical int values.
QString letters(int value, bool uppercase)
{
    if (value < 1)
        return QString();
    const int repeats = (value - 1) / 26 + 1;
    const QLatin1Char letter((uppercase ? 'A' : 'a') + (value - 1) % 26);
    return QString(repeats, letter);
}

} // namespace

namespace PageLabels {

QString styleName(Style style)
{
    switch (style) {
    case Style::Decimal:          return QStringLiteral("D");
    case Style::LowercaseRoman:   return QStringLiteral("r");
    case Style::UppercaseRoman:   return QStringLiteral("R");
    case Style::LowercaseLetters: return QStringLiteral("a");
    case Style::UppercaseLetters: return QStringLiteral("A");
    }
    return QStringLiteral("D");
}

QStringList labelsFor(int startValue, Style style, int pageCount)
{
    QStringList labels;
    if (pageCount <= 0 || startValue < 1)
        return labels;

    labels.reserve(pageCount);
    for (int i = 0; i < pageCount; ++i) {
        const int value = startValue + i;
        switch (style) {
        case Style::Decimal:
            labels.append(QString::number(value));
            break;
        case Style::LowercaseRoman:
            labels.append(romanNumeral(value, /*uppercase=*/false));
            break;
        case Style::UppercaseRoman:
            labels.append(romanNumeral(value, /*uppercase=*/true));
            break;
        case Style::LowercaseLetters:
            labels.append(letters(value, /*uppercase=*/false));
            break;
        case Style::UppercaseLetters:
            labels.append(letters(value, /*uppercase=*/true));
            break;
        }
    }
    return labels;
}

QList<PageLabelNumEntry> numberTreeEntries(int startValue, Style style, int pageCount)
{
    QList<PageLabelNumEntry> entries;
    if (pageCount <= 0 || startValue < 1)
        return entries;

    PageLabelNumEntry entry;
    entry.pageNum    = 0; // the range starts at the first page of the document
    entry.style      = styleName(style);
    entry.startValue = startValue;
    entries.append(entry);
    return entries;
}

bool writeNumberTree(PoDoFo::PdfMemDocument& doc, int startValue, Style style,
                     int pageCount)
{
    // Validate FIRST: an invalid range must leave the catalog untouched.
    const QList<PageLabelNumEntry> entries =
        numberTreeEntries(startValue, style, pageCount);
    if (entries.isEmpty())
        return false;

    try {
        auto& objects = doc.GetObjects();

        // One flat /Nums array over all pages (ISO 32000 7.9.3 number tree;
        // /Kids is only needed for sparse branching — a whole-document
        // uniform range is a single pair).
        auto& labels = objects.CreateDictionaryObject();
        PoDoFo::PdfArray nums;
        for (const PageLabelNumEntry& e : entries) {
            nums.Add(PoDoFo::PdfObject(static_cast<long long>(e.pageNum)));

            PoDoFo::PdfObject range{PoDoFo::PdfDictionary()};
            range.GetDictionary().AddKey(
                "S", PoDoFo::PdfObject(PoDoFo::PdfName(e.style.toStdString())));
            // /St is spec-defaulted to 1, but writing it explicitly keeps
            // the readback exact (no default-reconstruction in consumers).
            range.GetDictionary().AddKey(
                "St", PoDoFo::PdfObject(static_cast<long long>(e.startValue)));
            nums.Add(range);
        }
        labels.GetDictionary().AddKey("Nums", PoDoFo::PdfObject(nums));

        // Replace, never merge: a stale tree (e.g. from a previous labeling)
        // must not survive next to the new one.
        auto& catDict = doc.GetCatalog().GetDictionary();
        catDict.RemoveKey("PageLabels");
        catDict.AddKey("PageLabels",
                       PoDoFo::PdfObject(labels.GetIndirectReference()));
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning("PageLabels::writeNumberTree: %s", e.what());
        return false;
    }
}

bool writeNumberTree(const QString& pdfPath, int startValue, Style style)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfPath.toUtf8().constData());
        const int pageCount = static_cast<int>(doc.GetPages().GetCount());
        if (!writeNumberTree(doc, startValue, style, pageCount))
            return false;
        doc.Save(pdfPath.toUtf8().constData());
        return true;
    } catch (const PoDoFo::PdfError& e) {
        qWarning("PageLabels::writeNumberTree(%s): %s",
                 qPrintable(pdfPath), e.what());
        return false;
    }
}

} // namespace PageLabels
} // namespace gp
