// SPDX-License-Identifier: Apache-2.0
/**
 * PageLabels.cpp — pure /PageLabels seam implementation. See PageLabels.h
 * for the scope and contracts (pinned by tests/TestPageLabels.cpp).
 */
#include "PageLabels.h"

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

} // namespace PageLabels
} // namespace gp
