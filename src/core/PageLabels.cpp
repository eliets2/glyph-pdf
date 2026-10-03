// SPDX-License-Identifier: Apache-2.0
/**
 * PageLabels.cpp — /PageLabels seam implementation: pure entry/label
 * generation plus the §9.9 P1 catalog writer. See PageLabels.h for the
 * scope and contracts (pinned by tests/TestPageLabels.cpp).
 */
#include "PageLabels.h"

#include <QFile>
#include <QStringDecoder>
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

// ── §7.9.2.2 text-string encoding (wave-2b F-15) ────────────────────────────
// ISO 32000 §7.9.2.2: a TEXT string is PDFDocEncoding or UTF-16BE with the
// FE FF byte-order mark. PoDoFo's PdfString(UTF-8) constructor carries the
// UTF-8 bytes as a simple (non-unicode) string: it self-roundtrips through a
// UTF-8 decoder — so an in-tree readback gate speaking UTF-8 cannot see the
// bug — but a spec-conforming viewer decodes the bytes as PDFDocEncoding and
// renders mojibake. ASCII(-safe) prefixes keep the byte-identical literal
// form (PDFDocEncoding is ASCII-identical in 0x00–0x7F); anything outside is
// written as the UTF-16BE+BOM text string every consumer must accept.
bool prefixIsAsciiSafe(const QString& prefix)
{
    for (const QChar ch : prefix) {
        if (ch.unicode() >= 0x80)
            return false;
    }
    return true;
}

QByteArray utf16beTextStringBytes(const QString& prefix)
{
    QByteArray be;
    be.reserve(2 + prefix.size() * 2);
    be += char(0xFE);
    be += char(0xFF);
    const ushort* units = prefix.utf16();
    for (qsizetype i = 0; i < prefix.size(); ++i) {
        be += char((units[i] >> 8) & 0xFF);
        be += char(units[i] & 0xFF);
    }
    return be;
}

// The §7.9.2.2 inverse used by the write-path validation gate: a BOM'd
// payload decodes as UTF-16BE; anything else is the byte form this writer
// emits for the ASCII-safe case (raw bytes == UTF-8 there).
QString decodePdfTextStringBytes(const QByteArray& raw)
{
    if (raw.size() >= 2
            && static_cast<unsigned char>(raw.at(0)) == 0xFE
            && static_cast<unsigned char>(raw.at(1)) == 0xFF) {
        QStringDecoder dec(QStringConverter::Utf16BE);
        return dec.decode(raw.mid(2));
    }
    return QString::fromUtf8(raw.constData(), raw.size());
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

QList<PageLabelNumEntry> numberTreeEntries(int startValue, Style style, int pageCount,
                                           const QString& prefix)
{
    QList<PageLabelNumEntry> entries;
    if (pageCount <= 0 || startValue < 1)
        return entries;

    PageLabelNumEntry entry;
    entry.pageNum    = 0; // the range starts at the first page of the document
    entry.style      = styleName(style);
    entry.startValue = startValue;
    entry.prefix     = prefix;
    entries.append(entry);
    return entries;
}

bool writeNumberTree(PoDoFo::PdfMemDocument& doc, int startValue, Style style,
                     int pageCount, const QString& prefix, QString* err)
{
    // Validate FIRST: an invalid range must leave the catalog untouched.
    const QList<PageLabelNumEntry> entries =
        numberTreeEntries(startValue, style, pageCount, prefix);
    if (entries.isEmpty()) {
        if (err) *err = QStringLiteral("invalid labeling request (pageCount <= 0 "
                                       "or startValue < 1) — nothing was written");
        return false;
    }

    try {
        auto& objects = doc.GetObjects();

        // One flat /Nums array over all pages (ISO 32000 7.9.3 number tree;
        // /Kids is only needed for sparse branching — a whole-document
        // uniform range is a single pair).
        auto& labels = objects.CreateDictionaryObject();
        PoDoFo::PdfArray nums;
        for (const PageLabelNumEntry& e : entries) {
            // std::int64_t, not `long long`: PoDoFo 1.1's PdfObject has an
            // exact int64_t overload, and on LP64 (Linux) int64_t is `long`,
            // so a `long long` argument was ambiguous against PdfObject(double)
            // (a Windows-lane compile success that broke the Linux build).
            nums.Add(PoDoFo::PdfObject(static_cast<std::int64_t>(e.pageNum)));

            PoDoFo::PdfObject range{PoDoFo::PdfDictionary()};
            range.GetDictionary().AddKey(
                "S", PoDoFo::PdfObject(PoDoFo::PdfName(e.style.toStdString())));
            // /St is spec-defaulted to 1, but writing it explicitly keeps
            // the readback exact (no default-reconstruction in consumers).
            range.GetDictionary().AddKey(
                "St", PoDoFo::PdfObject(static_cast<std::int64_t>(e.startValue)));
            // /P (ISO 32000 Table 159): a text string PREPENDED to every
            // computed label of the range (prefix precedes the number).
            // Written ONLY for a non-empty prefix — an empty prefix must
            // never produce a /P key. Encoded per §7.9.2.2 (wave-2b F-15):
            // ASCII-safe stays literal, anything else goes out as the
            // UTF-16BE+BOM text string.
            if (!e.prefix.isEmpty()) {
                if (prefixIsAsciiSafe(e.prefix)) {
                    range.GetDictionary().AddKey(
                        "P", PoDoFo::PdfObject(
                                 PoDoFo::PdfString(e.prefix.toStdString())));
                } else {
                    const QByteArray be = utf16beTextStringBytes(e.prefix);
                    range.GetDictionary().AddKey(
                        "P", PoDoFo::PdfObject(PoDoFo::PdfString::FromRaw(
                                 PoDoFo::bufferview(
                                     be.constData(),
                                     static_cast<size_t>(be.size())),
                                 /*hex=*/false)));
                }
            }
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
        if (err) *err = QStringLiteral("the page-label tree could not be written: %1")
                            .arg(QString::fromUtf8(e.what()));
        return false;
    }
}

bool writeNumberTree(const QString& pdfPath, int startValue, Style style,
                     const QString& prefix, QString* err)
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
    if (startValue < 1) {
        if (err) *err = QStringLiteral("invalid labeling request (startValue < 1) "
                                       "— nothing was written");
        return false;
    }

    QString candidate;
    QString errLocal;
    QString& reason = err ? *err : errLocal;   // one sink either way
    if (!SafeSave::makeUniqueCandidate(&candidate, &reason)) {
        qWarning("PageLabels::writeNumberTree: %s", qPrintable(reason));
        return false;
    }
    QFile::remove(candidate);   // the reserved handle is released; we own the path now

    bool ok = false;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdfPath.toUtf8().constData());
        const int pageCount = static_cast<int>(doc.GetPages().GetCount());
        if (writeNumberTree(doc, startValue, style, pageCount, prefix, &reason)) {
            doc.Save(candidate.toUtf8().constData());

            // Validate the candidate: it must re-open (proving no lazy-stream
            // truncation) and carry exactly the tree this call was asked to
            // write.
            PoDoFo::PdfMemDocument check;
            check.Load(candidate.toUtf8().constData());
            const auto expected = numberTreeEntries(startValue, style, pageCount, prefix);
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
            // The written /P must match the request exactly: present iff the
            // prefix was non-empty, and text-string-equal to it when present
            // — decoded per §7.9.2.2 (UTF-16BE+BOM or the ASCII literal
            // form), not blindly as UTF-8 (the blind decode was exactly what
            // made the raw-UTF-8 writer self-certify, wave-2b F-15).
            if (ok) {
                const PoDoFo::PdfObject* range0 = nums->GetArray().FindAt(1);
                const PoDoFo::PdfObject* p =
                    range0 && range0->IsDictionary()
                        ? range0->GetDictionary().FindKey(PoDoFo::PdfName("P"))
                        : nullptr;
                ok = prefix.isEmpty()
                         ? p == nullptr
                         : p && p->IsString()
                               && decodePdfTextStringBytes(QByteArray(
                                      p->GetString().GetString().data(),
                                      static_cast<qsizetype>(
                                          p->GetString().GetString().size())))
                                      == prefix;
            }
        }
    } catch (const PoDoFo::PdfError& e) {
        qWarning("PageLabels::writeNumberTree(%s): %s",
                 qPrintable(pdfPath), e.what());
        reason = QStringLiteral("the document could not be labeled: %1")
                     .arg(QString::fromUtf8(e.what()));
        ok = false;
    } catch (const std::exception& e) {
        qWarning("PageLabels::writeNumberTree(%s): %s",
                 qPrintable(pdfPath), e.what());
        reason = QStringLiteral("the document could not be labeled: %1")
                     .arg(QString::fromUtf8(e.what()));
        ok = false;
    }
    if (!ok) {
        // Every false leaves a reason: a written tree that failed its own
        // read-back validation names the mismatch (no silent path).
        if (reason.isEmpty())
            reason = QStringLiteral("the labeled candidate failed validation "
                                    "— nothing was written");
        QFile::remove(candidate);
        return false;
    }

    // Checked commit: the candidate atomically replaces the destination; on a
    // refused commit the original stays byte-identical.
    if (!SafeSave::commitFileToDestination(candidate, pdfPath, &reason)) {
        qWarning("PageLabels::writeNumberTree: commit to %s failed: %s",
                 qPrintable(pdfPath), qPrintable(reason));
        QFile::remove(candidate);
        return false;
    }
    QFile::remove(candidate);   // committed bytes were replaced; drop the candidate
    return true;
}

} // namespace PageLabels
} // namespace gp
