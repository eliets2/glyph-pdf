// SPDX-License-Identifier: Apache-2.0
#include "engines/TextMatchFinder.h"

#include <QDebug>
#include <QElapsedTimer>

#ifdef HAS_PDFIUM
#include <fpdfview.h>
#include <fpdf_text.h>
#include "engines/pdfium/PdfiumEnvironment.h"
#endif

QRegularExpression TextMatchFinder::buildPattern(const QString& search, bool matchCase,
                                                 bool wholeWords, bool useRegex) {
    QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
    if (!matchCase) opts |= QRegularExpression::CaseInsensitiveOption;

    QString pattern;
    if (useRegex) {
        pattern = wholeWords ? QStringLiteral("\\b(?:%1)\\b").arg(search) : search;
    } else {
        pattern = QRegularExpression::escape(search);
        if (wholeWords) pattern = QStringLiteral("\\b%1\\b").arg(pattern);
    }

    QRegularExpression rx(pattern);
    rx.setPatternOptions(opts);
    return rx;
}

QList<TextMatchFinder::ReflowWarning> TextMatchFinder::reflowWarnings(
    const QList<TextMatch>& matches, const QString& replacement) {
    QList<ReflowWarning> out;
    for (const auto& m : matches) {
        // A same-length replacement on a monospace-ish metric is still not
        // guaranteed width-equal, but length is the honest pre-apply signal
        // available without choosing the substitute font; the engine reports
        // the MEASURED drawn widths after apply and the caller surfaces the
        // count of width-changed replacements.
        if (m.text != replacement) {
            out.append(ReflowWarning{m.pageIndex, m.text, replacement});
        }
    }
    return out;
}

#ifdef HAS_PDFIUM

namespace {

// One decoded character of the page text with its geometry + font size.
struct CharBox {
    QString ch;
    QRectF bbox;      // Qt top-left user space
    double fontSize;  // points; 0 when unknown
};

// Extract per-character boxes + font sizes for one page of an open document.
// Same coordinate normalization as PatternRedactor::extractCharsFromOpenDoc
// (qtY = pageHeight - pdf_top), extended with FPDFText_GetFontSize.
QList<CharBox> extractCharBoxes(FPDF_DOCUMENT doc, int pageIndex) {
    QList<CharBox> result;
    const int pageCount = FPDF_GetPageCount(doc);
    if (pageIndex < 0 || pageIndex >= pageCount) return result;

    FPDF_PAGE page = FPDF_LoadPage(doc, pageIndex);
    if (!page) return result;

    const double pageHeight = static_cast<double>(FPDF_GetPageHeightF(page));
    FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
    if (!textPage) {
        FPDF_ClosePage(page);
        return result;
    }

    const int charCount = FPDFText_CountChars(textPage);
    result.reserve(charCount);
    for (int ci = 0; ci < charCount; ++ci) {
        const unsigned int codepoint = FPDFText_GetUnicode(textPage, ci);

        double pdf_left = 0, pdf_right = 0, pdf_bottom = 0, pdf_top = 0;
        double size = 0;
        QRectF box;
        if (FPDFText_GetCharBox(textPage, ci, &pdf_left, &pdf_right, &pdf_bottom, &pdf_top)) {
            const double qtY = pageHeight - pdf_top;
            box = QRectF(pdf_left, qtY, pdf_right - pdf_left, pdf_top - pdf_bottom);
        }
        size = FPDFText_GetFontSize(textPage, ci);
        if (size < 0) size = 0;

        char32_t cp = static_cast<char32_t>(codepoint);
        result.append(CharBox{QString::fromUcs4(&cp, 1), box, size});
    }

    FPDFText_ClosePage(textPage);
    FPDF_ClosePage(page);
    return result;
}

// Run `pattern` over the reconstructed page text; produce one TextMatch per
// hit with the union box of its characters and the largest font size in the
// span. Carries the M-3 input cap; the packa-F4 budget (deadline +
// cancellation) is checked BETWEEN matches at the top of the loop — see
// MatchBudget for the honest statement of what that does and does not bound.
//
// packa-F2: match offsets are UTF-16 units in `pageText`, but a CharBox's
// text can span MORE than one UTF-16 unit (supplementary code points). The
// unitToChar map is the single translation layer between the two worlds, so
// the geometry stays correct regardless of how many UTF-16 units one
// CharBox contributes (verified for supplementary-before-match,
// supplementary-inside-match and neighbor preservation in TestFindReplace).
QList<TextMatch> matchCharBoxes(const QList<CharBox>& chars, int pageIndex,
                                const QRegularExpression& pattern,
                                const QElapsedTimer& jobTimer,
                                TextMatchFinder::MatchBudget* budget) {
    QList<TextMatch> results;
    if (chars.isEmpty()) return results;

    QString pageText;
    pageText.reserve(chars.size());
    QList<int> unitToChar;   // pageText UTF-16 offset -> index into `chars`
    unitToChar.reserve(chars.size());
    for (int ci = 0; ci < chars.size(); ++ci) {
        pageText.append(chars[ci].ch);
        for (int u = 0, n = chars[ci].ch.size(); u < n; ++u)
            unitToChar.append(ci);
    }

    constexpr int kMaxRegexInput = 256 * 1024;   // chars (M-3)

    if (pageText.size() > kMaxRegexInput) {
        qWarning() << "TextMatchFinder — page text truncated from" << pageText.size()
                   << "to" << kMaxRegexInput << "chars to bound regex backtracking (M-3)";
        pageText.truncate(kMaxRegexInput);
        unitToChar.resize(pageText.size());   // shrink with the capped text
    }

    QRegularExpressionMatchIterator it = pattern.globalMatch(pageText);
    while (it.hasNext()) {
        if (budget && budget->shouldStop(jobTimer.nsecsElapsed())) {
            qWarning() << "TextMatchFinder — match loop stopped (budget deadline"
                       << budget->deadlineMs << "ms or cancellation; partial results)";
            break;
        }
        const QRegularExpressionMatch m = it.next();
        const int startIdx = m.capturedStart();
        const int endIdx = m.capturedEnd();  // exclusive
        if (startIdx < 0 || endIdx <= startIdx) continue;
        if (startIdx >= unitToChar.size()) continue;   // defensive: past cap

        // UTF-16 span -> CharBox span through the map (packa F2).
        const int firstChar = unitToChar[startIdx];
        const int lastChar = unitToChar[qMin(endIdx, unitToChar.size()) - 1];

        QRectF box;
        double size = 0;
        bool first = true;
        for (int i = firstChar; i <= lastChar && i < chars.size(); ++i) {
            const CharBox& cb = chars[i];
            if (cb.bbox.isNull() || cb.bbox.isEmpty()) continue;
            box = first ? cb.bbox : box.united(cb.bbox);
            first = false;
            size = qMax(size, cb.fontSize);
        }
        if (first) continue;  // no drawable geometry for this span

        TextMatch tm;
        tm.pageIndex = pageIndex;
        tm.rect = box;
        tm.text = m.captured(0);
        tm.fontSize = size;
        results.append(tm);
    }
    return results;
}

} // namespace

QList<TextMatch> TextMatchFinder::findMatches(const QString& pdfPath,
                                              const QList<int>& pages,
                                              const QRegularExpression& pattern,
                                              MatchBudget* budget) {
    QList<TextMatch> out;
    if (!pattern.isValid()) {
        qWarning() << "TextMatchFinder::findMatches — invalid pattern:" << pattern.errorString();
        return out;
    }
    if (pattern.pattern().isEmpty() || pages.isEmpty()) return out;

    // packa-F4: the budget spans the WHOLE job (document load + every page),
    // not one deadline per page — a 1000-page recount is bounded once.
    // Callers that pass no budget get the default job budget, not none.
    MatchBudget defaultBudget;
    MatchBudget* effectiveBudget = budget ? budget : &defaultBudget;
    QElapsedTimer jobTimer;
    jobTimer.start();

    PdfiumEnvironment env;
    FPDF_DOCUMENT doc = FPDF_LoadDocument(pdfPath.toLocal8Bit().constData(), nullptr);
    if (!doc) {
        qWarning() << "TextMatchFinder::findMatches — could not open PDF" << pdfPath;
        return out;
    }

    for (int pg : pages) {
        if (effectiveBudget->shouldStop(jobTimer.nsecsElapsed())) {
            qWarning() << "TextMatchFinder — page scan stopped after page index"
                       << (pg - 1) << "(budget deadline or cancellation; partial results)";
            break;
        }
        const QList<CharBox> chars = extractCharBoxes(doc, pg);
        out.append(matchCharBoxes(chars, pg, pattern, jobTimer, effectiveBudget));
    }

    FPDF_CloseDocument(doc);
    return out;
}

#else // !HAS_PDFIUM

// R22 (2026-09-14): signature aligned with the header — packa-F4 (88d5686)
// added the MatchBudget* parameter to the declaration and the HAS_PDFIUM
// branch but not this stub, which Windows never compiles (pdfium is always
// vendored there); every engine-less Linux build hard-fails.
QList<TextMatch> TextMatchFinder::findMatches(const QString& pdfPath,
                                              const QList<int>& pages,
                                              const QRegularExpression& pattern,
                                              MatchBudget* budget) {
    Q_UNUSED(pdfPath);
    Q_UNUSED(pages);
    Q_UNUSED(pattern);
    Q_UNUSED(budget);
    qWarning() << "TextMatchFinder: PDFium not available — cannot locate text matches.";
    return {};
}

#endif // HAS_PDFIUM
