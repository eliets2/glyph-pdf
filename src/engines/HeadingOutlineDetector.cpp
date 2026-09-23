// SPDX-License-Identifier: Apache-2.0
#include "engines/HeadingOutlineDetector.h"

#include <QRegularExpression>
#include <algorithm>
#include <functional>
#include <numeric>

namespace {

// A line whose TRIMMED text matches this — "Title . . . 12" — is treated as
// a TOC entry: everything before the leader dots is the title, the trailing
// number the 1-based target page.
const QRegularExpression& tocLinePattern() {
    // PCRE2 has no \uXXXX escape — a raw C++ string would hand "\u00B7"
    // over literally and invalidate the WHOLE pattern. The middle-dot and
    // ellipsis leader characters are spelled as \x{...} codepoints instead.
    static const QRegularExpression rx(QStringLiteral(
        R"(^(.{3,120}?)[\s.\x{B7}\x{2026}]{2,}(\d{1,4})\s*$)"));
    Q_ASSERT(rx.isValid());
    return rx;
}

// Repeated identical lines at the same size on most pages are running
// headers/footers, not headings.
bool isRepeatedLine(const QString& text, double size,
                    const QList<HeadingOutlineDetector::TextLine>* allLines,
                    int pageCount) {
    if (!allLines) return false;
    int occurrences = 0;
    for (const auto& l : *allLines) {
        if (qFuzzyCompare(l.maxSize, size) && l.text.compare(text, Qt::CaseInsensitive) == 0) {
            ++occurrences;
            if (occurrences > qMax(2, pageCount / 2))
                return true;
        }
    }
    return false;
}

} // namespace

QList<HeadingOutlineDetector::TextLine>
HeadingOutlineDetector::groupRuns(const QList<PdfiumBackend::TextRun>& runs) {
    QList<TextLine> lines;
    for (const auto& run : runs) {
        const QString runText = run.text;
        if (runText.trimmed().isEmpty()) continue;

        bool merged = false;
        if (!lines.isEmpty()) {
            TextLine& last = lines.last();
            const double yDelta = qAbs(run.rect.top() - last.rect.top());
            const double gap = run.rect.left() - last.rect.right();
            if (yDelta <= qMax(2.0, run.fontSize * 0.35)
                && gap < qMax(18.0, run.fontSize * 1.2)) {
                // Same visual line: concatenate (space when there is a gap).
                last.text += (gap > run.fontSize * 0.15) ? QLatin1Char(' ') + runText
                                                         : runText;
                last.rect = last.rect.united(run.rect);
                last.maxSize = qMax(last.maxSize, run.fontSize);
                last.bold = last.bold || run.fontName.contains(
                    QStringLiteral("bold"), Qt::CaseInsensitive);
                merged = true;
            }
        }
        if (!merged) {
            TextLine l;
            l.text = runText;
            l.rect = run.rect;
            l.maxSize = run.fontSize;
            l.bold = run.fontName.contains(QStringLiteral("bold"), Qt::CaseInsensitive);
            lines.append(l);
        }
    }
    for (auto& l : lines)
        l.text = l.text.simplified();
    return lines;
}

QList<HeadingOutlineDetector::Candidate>
HeadingOutlineDetector::detect(const QList<QList<TextLine>>& pages) {
    QList<Candidate> out;

    // Body size = median of line sizes weighted by text length (the dominant
    // body text, not the biggest display size).
    QList<QPair<double, int>> sizes;   // (size, weight)
    for (const auto& pageLines : pages) {
        for (const auto& l : pageLines) {
            if (l.maxSize > 0)
                sizes.append({l.maxSize, qMax(1, l.text.length())});
        }
    }
    double bodySize = 0;
    {
        std::sort(sizes.begin(), sizes.end(),
                  [](const QPair<double, int>& a, const QPair<double, int>& b) {
                      return a.first < b.first;
                  });
        const qint64 total = std::accumulate(sizes.cbegin(), sizes.cend(), qint64(0),
            [](qint64 acc, const QPair<double, int>& s) { return acc + s.second; });
        qint64 acc = 0;
        for (const auto& s : sizes) {
            acc += s.second;
            if (acc * 2 >= total) { bodySize = s.first; break; }
        }
    }
    if (bodySize <= 0) return out;

    // Flatten once for the repeated-header check.
    QList<TextLine> allLines;
    for (const auto& pageLines : pages)
        allLines.append(pageLines);

    for (int p = 0; p < pages.size(); ++p) {
        for (const auto& l : pages[p]) {
            const QString text = l.text.trimmed();
            if (text.length() < 2 || text.length() > 120) continue;
            if (isRepeatedLine(text, l.maxSize, &allLines, pages.size())) continue;

            // TOC pattern: "Title ..... 12"
            const QRegularExpressionMatch m = tocLinePattern().match(text);
            if (m.hasMatch()) {
                Candidate c;
                c.title = m.captured(1).simplified();
                c.pageIndex = p;
                c.level = 1;
                c.fontSize = l.maxSize;
                c.fromToc = true;
                c.tocTargetPage = m.captured(2).toInt() - 1;   // 1-based → 0-based
                if (c.tocTargetPage >= 0 && c.tocTargetPage < pages.size())
                    out.append(c);
                continue;
            }

            // Font-size / bold heuristic.
            const double ratio = l.maxSize / bodySize;
            const bool sizeHeading = ratio >= 1.15;
            const bool boldHeading = l.bold && ratio >= 1.02;
            if (!sizeHeading && !boldHeading) continue;
            // A line ending in sentence punctuation is body prose, not a heading.
            if (text.endsWith(QLatin1Char('.')) && text.length() > 60) continue;

            Candidate c;
            c.title = text;
            c.pageIndex = p;
            c.level = ratio >= 1.6 ? 1 : (ratio >= 1.35 ? 2 : 3);
            c.fontSize = l.maxSize;
            c.fromToc = false;
            c.tocTargetPage = -1;
            out.append(c);

            if (out.size() >= kMaxCandidates) {
                qWarning() << "HeadingOutlineDetector: candidate cap"
                           << kMaxCandidates << "reached — results truncated";
                return out;
            }
        }
    }
    return out;
}

QList<OutlineEntry>
HeadingOutlineDetector::buildOutlineTree(const QList<Candidate>& accepted) {
    // Index-cursor recursion (never holding element pointers across appends —
    // QList growth invalidates them).
    int cursor = 0;
    std::function<QList<OutlineEntry>(int)> buildLevel =
        [&](int level) -> QList<OutlineEntry> {
        QList<OutlineEntry> out;
        while (cursor < accepted.size()) {
            const Candidate& c = accepted.at(cursor);
            const int lv = qBound(1, c.level, 3);
            if (lv < level)
                break;                       // shallower candidate: pop a level
            if (lv > level && !out.isEmpty()) {
                // Deeper candidate: nest under the most recent entry.
                out.last().children.append(buildLevel(level + 1));
                continue;
            }
            ++cursor;                        // stray deeper entry (no parent
                                             // yet): keep it at this level
            OutlineEntry e;
            e.title = c.title;
            e.targetPage = qMax(0, c.fromToc ? c.tocTargetPage : c.pageIndex);
            out.append(e);
        }
        return out;
    };
    return buildLevel(1);
}
