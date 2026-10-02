// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QStringList>
#include <QSet>

// ── shared standard-14 drawing toolkit (single-contract discipline) ─────────
// The printable writers draw untrusted, document-derived strings through
// PoDoFo's standard-14 Helvetica (WinAnsi / CP1252). The two rules below are
// THE contract for every such writer — kept in one header so the exporters
// cannot drift, exactly like ConversionManager::csvFormulaSafeCell does for
// the CSV writers:
//
//   * gp::sanitizeForStandard14 (W1-04): a per-string encoding sanitizer at
//     the ONE draw boundary — every codepoint the WinAnsi table cannot
//     encode is replaced 1:1 (counted for the artifact's honesty note)
//     instead of throwing PdfErrorCode::InvalidFontData and aborting the
//     whole export over one TAB or CJK character.
//   * gp::wrapText: char-counted word wrap matched to simple DrawText
//     calls; 1:1 replacement keeps its bounds valid.
//
// Originally landed in engines/ReviewSummaryWriter.cpp (R27 tail / W1-04);
// hoisted verbatim when the accessibility summary writer joined.
namespace gp {

struct WinAnsiSafeString {
    QString text;
    int substituted = 0;
};

inline WinAnsiSafeString sanitizeForStandard14(const QString& in) {
    static const char16_t kCp1252Specials[] = {
        0x20AC, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6,
        0x2030, 0x0160, 0x2039, 0x0152, 0x017D, 0x2018, 0x2019, 0x201C,
        0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A,
        0x0153, 0x017E, 0x0178 };
    static const QSet<char16_t> kSpecials = [] {
        QSet<char16_t> s;
        for (char16_t c : kCp1252Specials) s.insert(c);
        return s;
    }();

    WinAnsiSafeString out;
    out.text.reserve(in.size());
    for (const QChar& qc : in) {
        const char16_t ch = qc.unicode();
        bool keep = false;
        if (ch == 0x09 || ch == 0x0A || ch == 0x0D) {
            out.text += QLatin1Char(' ');          // whitespace → whitespace
            ++out.substituted;
            continue;
        }
        if (ch >= 0x20 && ch <= 0x7E) keep = true;             // printable ASCII
        else if (ch >= 0xA0 && ch <= 0xFF) keep = true;        // Latin-1 = CP1252 here
        else if (kSpecials.contains(ch)) keep = true;          // CP1252 specials
        if (keep) {
            out.text += qc;
        } else {
            out.text += QLatin1Char('?');   // NUL, C0/C1 rest, DEL, CJK, emoji…
            ++out.substituted;
        }
    }
    return out;
}

// Rough character wrap at ~`maxChars` body characters — a printable line,
// drawn by simple DrawText calls (the same standard-14 drawing the
// Bates/header-footer writers use).
inline QStringList wrapText(const QString& text, int maxChars) {
    QStringList lines;
    QString normalized = text;
    normalized.replace(QLatin1Char('\r'), QString());
    for (const QString& para : normalized.split(QLatin1Char('\n'))) {
        if (para.isEmpty()) { lines.append(QString()); continue; }
        int start = 0;
        while (start < para.length()) {
            int len = qMin(maxChars, para.length() - start);
            if (start + len < para.length()) {
                // Back off to the last space so words are not split.
                const int lastSpace = para.lastIndexOf(QLatin1Char(' '), start + len);
                if (lastSpace > start)
                    len = lastSpace - start;
            }
            lines.append(para.mid(start, len));
            start += len;
            while (start < para.length() && para.at(start) == QLatin1Char(' '))
                ++start;
        }
    }
    return lines;
}

} // namespace gp
