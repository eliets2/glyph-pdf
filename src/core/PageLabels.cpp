// SPDX-License-Identifier: Apache-2.0
/**
 * PageLabels.cpp — /PageLabels seam implementation: pure entry/label
 * generation plus the §9.9 P1 catalog writer. See PageLabels.h for the
 * scope and contracts (pinned by tests/TestPageLabels.cpp).
 */
#include "PageLabels.h"

#include <QFile>
#include "engines/SafeSave.h"

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
    // G13 (QUALITY-GATE-2026-09-09): this overload used to Load() and Save()
    // the SAME path. PoDoFo keeps the source device open for lazy object
    // loading, so saving over the same file truncated the device before the
    // deferred streams were flushed: a content-bearing document serialized as
    // 0 bytes, the call returned false, and the caller's file was destroyed
    // (a 20,667-byte two-page text fixture reproducibly became 0 bytes and
    // could not be reopened). The transaction is now the R01 safe-save shape:
    // serialize the COMPLETE mutation to a DISTINCT candidate, validate the
    // candidate by re-reading it, then commit the checked bytes. A
    // lazy-loaded file is never saved over itself, and a failed commit
    // leaves the destination byte-identical.
    if (startValue < 1)
        return false;

    QString candidate;
    QString err;
    if (!SafeSave::makeUniqueCandidate(&candidate, &err)) {
        qWarning("PageLabels::writeNumberTree: %s", qPrintable(err));
        return false;
    }
    QFile::remove(candidate);   // the reserved handle is released; we own the path now

    bool ok = false;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfPath.toUtf8().constData());
        const int pageCount = static_cast<int>(doc.GetPages().GetCount());
        if (writeNumberTree(doc, startValue, style, pageCount)) {
            doc.Save(candidate.toUtf8().constData());

            // Validate the candidate: it must re-open (proving no lazy-stream
            // truncation) and carry exactly the tree this call was asked to
            // write.
            PoDoFo::PdfMemDocument check;
            check.Load(candidate.toUtf8().constData());
            const auto expected = numberTreeEntries(startValue, style, pageCount);
            const PoDoFo::PdfObject* labels =
                check.GetCatalog().GetDictionary().FindKey(PoDoFo::PdfName("PageLabels"));
            if (labels && labels->IsReference())
                labels = check.GetObjects().GetObject(labels->GetReference());
            const PoDoFo::PdfObject* nums =
                labels && labels->IsDictionary()
                    ? labels->GetDictionary().FindKey(PoDoFo::PdfName("Nums"))
                    : nullptr;
            ok = nums && nums->IsArray()
                     && static_cast<qsizetype>(nums->GetArray().GetSize())
                            == expected.size() * 2
                     && static_cast<int>(check.GetPages().GetCount()) == pageCount;
        }
    } catch (const PoDoFo::PdfError& e) {
        qWarning("PageLabels::writeNumberTree(%s): %s",
                 qPrintable(pdfPath), e.what());
        ok = false;
    } catch (const std::exception& e) {
        qWarning("PageLabels::writeNumberTree(%s): %s",
                 qPrintable(pdfPath), e.what());
        ok = false;
    }
    if (!ok) {
        QFile::remove(candidate);
        return false;
    }

    // Checked commit: the candidate atomically replaces the destination; on a
    // refused commit the original stays byte-identical.
    if (!SafeSave::commitFileToDestination(candidate, pdfPath, &err)) {
        qWarning("PageLabels::writeNumberTree: commit to %s failed: %s",
                 qPrintable(pdfPath), qPrintable(err));
        QFile::remove(candidate);
        return false;
    }
    QFile::remove(candidate);   // committed bytes were replaced; drop the candidate
    return true;
}

} // namespace PageLabels
} // namespace gp
