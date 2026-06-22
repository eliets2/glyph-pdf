// SPDX-License-Identifier: Apache-2.0
#include "engines/PatternRedactor.h"

#include <QHash>
#include <QDebug>
#include <QElapsedTimer>
#include <mutex>

#ifdef HAS_PDFIUM
#include <fpdfview.h>
#include <fpdf_text.h>
#include "engines/pdfium/PdfiumEnvironment.h"
#endif

// ---------------------------------------------------------------------------
// Named pattern definitions (QRegularExpression, Qt syntax)
// ---------------------------------------------------------------------------

static const QHash<QString, QString>& patternTable() {
    static const QHash<QString, QString> tbl = {
        { QStringLiteral("email"),
          QStringLiteral(R"(\b[A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}\b)") },

        { QStringLiteral("phone-us"),
          QStringLiteral(R"(\b(?:\+?1[-.\s]?)?\(?\d{3}\)?[-.\s]?\d{3}[-.\s]?\d{4}\b)") },

        { QStringLiteral("phone-intl"),
          QStringLiteral(R"(\+\d{1,3}[-.\s]?\(?\d{1,4}\)?[-.\s]?\d{1,14}\b)") },

        { QStringLiteral("ssn"),
          QStringLiteral(R"(\b\d{3}[-\s]?\d{2}[-\s]?\d{4}\b)") },

        { QStringLiteral("credit-card"),
          QStringLiteral(R"(\b(?:\d{4}[-\s]?){3}\d{4}\b)") },

        { QStringLiteral("ipv4"),
          QStringLiteral(R"(\b(?:\d{1,3}\.){3}\d{1,3}\b)") },

        { QStringLiteral("ipv6"),
          QStringLiteral(R"(\b[0-9A-Fa-f]{1,4}(?::[0-9A-Fa-f]{1,4}){7}\b)") },

        { QStringLiteral("iban"),
          QStringLiteral(R"(\b[A-Z]{2}\d{2}[A-Z0-9]{4}\d{7}(?:[A-Z0-9]{0,16})?\b)") },

        { QStringLiteral("uk-postcode"),
          QStringLiteral(R"(\b[A-Z]{1,2}\d[A-Z\d]?\s?\d[A-Z]{2}\b)") },

        { QStringLiteral("us-zip"),
          QStringLiteral(R"(\b\d{5}(?:-\d{4})?\b)") },

        { QStringLiteral("date-iso"),
          QStringLiteral(R"(\b\d{4}-\d{2}-\d{2}\b)") },

        { QStringLiteral("date-us"),
          QStringLiteral(R"(\b\d{1,2}/\d{1,2}/\d{2,4}\b)") },
    };
    return tbl;
}

QRegularExpression PatternRedactor::namedPattern(const QString& name) {
    const auto& tbl = patternTable();
    const auto it = tbl.find(name);
    if (it == tbl.end()) {
        // Return an explicitly invalid regex for unknown keys.
        // Default QRegularExpression() is valid in Qt6 (matches empty string),
        // so we use a syntactically broken pattern instead.
        return QRegularExpression(QStringLiteral("(?P<"));  // invalid: unclosed named group
    }
    return QRegularExpression(*it);
}

QStringList PatternRedactor::availablePatterns() {
    // Return in a stable display order (not QHash iteration order)
    return {
        QStringLiteral("email"),
        QStringLiteral("phone-us"),
        QStringLiteral("phone-intl"),
        QStringLiteral("ssn"),
        QStringLiteral("credit-card"),
        QStringLiteral("ipv4"),
        QStringLiteral("ipv6"),
        QStringLiteral("iban"),
        QStringLiteral("uk-postcode"),
        QStringLiteral("us-zip"),
        QStringLiteral("date-iso"),
        QStringLiteral("date-us"),
    };
}

// ---------------------------------------------------------------------------
// PDFium per-character extraction
// ---------------------------------------------------------------------------

#ifdef HAS_PDFIUM
// Extract per-character boxes for one page of an ALREADY-OPEN document.
// Factored out so a batch caller can parse the PDF once and reuse the handle
// across many pages instead of reloading the whole document per page.
// Declared as a private static member (see header) so it can name CharInfo.
QList<PatternRedactor::CharInfo>
PatternRedactor::extractCharsFromOpenDoc(void* docHandle, int pageIndex) {
    QList<CharInfo> result;

    FPDF_DOCUMENT doc = static_cast<FPDF_DOCUMENT>(docHandle);
    const int pageCount = FPDF_GetPageCount(doc);
    if (pageIndex < 0 || pageIndex >= pageCount) {
        return result;
    }

    FPDF_PAGE page = FPDF_LoadPage(doc, pageIndex);
    if (!page) {
        return result;
    }

    const double pageHeight = static_cast<double>(FPDF_GetPageHeightF(page));

    FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
    if (!textPage) {
        FPDF_ClosePage(page);
        return result;
    }

    const int charCount = FPDFText_CountChars(textPage);
    result.reserve(charCount);

    for (int ci = 0; ci < charCount; ++ci) {
        // FPDFText_GetUnicode returns unsigned int (Unicode code point)
        const unsigned int codepoint = FPDFText_GetUnicode(textPage, ci);

        double pdf_left = 0, pdf_right = 0, pdf_bottom = 0, pdf_top = 0;
        if (!FPDFText_GetCharBox(textPage, ci,
                                  &pdf_left, &pdf_right, &pdf_bottom, &pdf_top)) {
            // Could not get bounding box — push a zero-size placeholder so indices stay aligned
            char32_t cp = static_cast<char32_t>(codepoint);
            result.append(CharInfo{ QString::fromUcs4(&cp, 1), QRectF() });
            continue;
        }

        // PDFium coordinate space: origin bottom-left.
        // Qt PDF user-space: origin top-left.
        // Conversion: qtY = pageHeight - pdf_top
        //             qtH = pdf_top - pdf_bottom
        const double qtX = pdf_left;
        const double qtY = pageHeight - pdf_top;
        const double qtW = pdf_right - pdf_left;
        const double qtH = pdf_top - pdf_bottom;

        char32_t cp = static_cast<char32_t>(codepoint);
        result.append(CharInfo{
            QString::fromUcs4(&cp, 1),
            QRectF(qtX, qtY, qtW, qtH)
        });
    }

    FPDFText_ClosePage(textPage);
    FPDF_ClosePage(page);
    return result;
}
#endif

QList<PatternRedactor::CharInfo>
PatternRedactor::extractCharsWithPositions(const QString& pdfPath, int pageIndex) {
    QList<CharInfo> result;

#ifdef HAS_PDFIUM
    PdfiumEnvironment env;

    FPDF_DOCUMENT doc = FPDF_LoadDocument(pdfPath.toLocal8Bit().constData(), nullptr);
    if (!doc) {
        qWarning() << "PatternRedactor: could not open PDF" << pdfPath;
        return result;
    }

    result = extractCharsFromOpenDoc(static_cast<void*>(doc), pageIndex);

    FPDF_CloseDocument(doc);

#else
    Q_UNUSED(pdfPath);
    Q_UNUSED(pageIndex);
    qWarning() << "PatternRedactor: PDFium not available — cannot extract character positions.";
#endif

    return result;
}

QRectF PatternRedactor::mergeCharBoxes(const QList<CharInfo>& chars, int startIdx, int endIdx) {
    // endIdx is exclusive
    QRectF merged;
    bool first = true;
    bool hasNull = false;
    for (int i = startIdx; i < endIdx && i < chars.size(); ++i) {
        const QRectF& box = chars[i].bbox;
        if (box.isNull() || box.isEmpty()) {
            hasNull = true;
            continue;
        }
        if (first) {
            merged = box;
            first = false;
        } else {
            merged = merged.united(box);
        }
    }

    if (hasNull) {
        // Expand to fill the gap using surrounding characters if possible
        double leftX = merged.isNull() ? 1e9 : merged.left();
        double rightX = merged.isNull() ? -1e9 : merged.right();
        double top = merged.isNull() ? 1e9 : merged.top();
        double bottom = merged.isNull() ? -1e9 : merged.bottom();

        // Find a valid box before the match
        for (int i = startIdx - 1; i >= 0; --i) {
            if (!chars[i].bbox.isNull() && !chars[i].bbox.isEmpty()) {
                leftX = std::min(leftX, chars[i].bbox.right());
                top = std::min(top, chars[i].bbox.top());
                bottom = std::max(bottom, chars[i].bbox.bottom());
                break;
            }
        }
        // Find a valid box after the match
        for (int i = endIdx; i < chars.size(); ++i) {
            if (!chars[i].bbox.isNull() && !chars[i].bbox.isEmpty()) {
                rightX = std::max(rightX, chars[i].bbox.left());
                top = std::min(top, chars[i].bbox.top());
                bottom = std::max(bottom, chars[i].bbox.bottom());
                break;
            }
        }

        if (leftX <= rightX && top <= bottom && leftX != 1e9 && rightX != -1e9) {
            merged = QRectF(QPointF(leftX, top), QPointF(rightX, bottom));
        } else if (merged.isNull()) {
            // Completely unresolvable fallback
            merged = QRectF(0, 0, 100, 20); // Arbitrary fallback
        }
    }
    return merged;
}

// ---------------------------------------------------------------------------
// Regex matching against extracted chars (shared by both findMatches variants)
// ---------------------------------------------------------------------------

QList<QRectF> PatternRedactor::matchChars(const QList<CharInfo>& chars,
                                          const QRegularExpression& pattern) {
    QList<QRectF> results;
    if (chars.isEmpty()) {
        return results;
    }

    // Reconstruct the page text from the char sequence so we can run the regex.
    QString pageText;
    pageText.reserve(chars.size());
    for (const CharInfo& ci : chars) {
        pageText.append(ci.ch);
    }

    // M-3: ReDoS bound. The pattern may be a user-supplied custom regex, and a
    // page may carry a large amount of text, so globalMatch() can catastrophically
    // backtrack and hang the UI thread. We bound the work two ways:
    //   (1) cap the input length fed to the regex (kMaxRegexInput), which is the
    //       primary driver of backtracking blow-up; and
    //   (2) abort the match loop after a wall-clock budget (kMatchBudgetMs).
    // Residual limit (documented): a SINGLE pathological QRegularExpressionMatch
    // step can still run past the budget before next() returns, because PCRE2's
    // own backtracking is not interrupted mid-step; the input cap keeps that
    // bounded in practice. A full fix (running the match on a cancellable worker)
    // is deliberately out of scope for this minimal hardening. Built-in named
    // patterns are unaffected — they are linear and finish well within the budget.
    constexpr int kMaxRegexInput  = 256 * 1024;  // chars
    constexpr qint64 kMatchBudgetMs = 1500;       // wall-clock ceiling

    if (pageText.size() > kMaxRegexInput) {
        qWarning() << "PatternRedactor::matchChars — page text truncated from"
                   << pageText.size() << "to" << kMaxRegexInput
                   << "chars to bound regex backtracking (M-3)";
        pageText.truncate(kMaxRegexInput);
    }

    QElapsedTimer timer;
    timer.start();

    // Run the regex against the (bounded) page text
    QRegularExpressionMatchIterator it = pattern.globalMatch(pageText);
    while (it.hasNext()) {
        if (timer.elapsed() > kMatchBudgetMs) {
            qWarning() << "PatternRedactor::matchChars — regex match aborted after"
                       << kMatchBudgetMs << "ms (possible ReDoS / pathological pattern, M-3);"
                       << "returning partial results";
            break;
        }
        const QRegularExpressionMatch match = it.next();
        const int startIdx = match.capturedStart();
        const int endIdx   = match.capturedEnd(); // exclusive
        if (startIdx < 0 || endIdx <= startIdx) continue;

        const QRectF bbox = mergeCharBoxes(chars, startIdx, endIdx);
        if (!bbox.isNull() && !bbox.isEmpty()) {
            results.append(bbox);
        }
    }

    return results;
}

// ---------------------------------------------------------------------------
// Public API: findMatches (single page)
// ---------------------------------------------------------------------------

QList<QRectF> PatternRedactor::findMatches(const QString& pdfPath,
                                            int             pageIndex,
                                            const QRegularExpression& pattern) {
    // Guard: invalid pattern
    if (!pattern.isValid()) {
        qWarning() << "PatternRedactor::findMatches — invalid pattern:"
                   << pattern.errorString();
        return {};
    }
    if (pattern.pattern().isEmpty()) {
        return {};
    }

    const QList<CharInfo> chars = extractCharsWithPositions(pdfPath, pageIndex);
    return matchChars(chars, pattern);
}

// ---------------------------------------------------------------------------
// Public API: findMatches (batch — parses the PDF exactly once)
// ---------------------------------------------------------------------------

QHash<int, QList<QRectF>>
PatternRedactor::findMatches(const QString& pdfPath,
                             const QList<int>& pages,
                             const QRegularExpression& pattern) {
    QHash<int, QList<QRectF>> out;

    // Guard: invalid / empty pattern (same contract as the single-page variant).
    if (!pattern.isValid()) {
        qWarning() << "PatternRedactor::findMatches(batch) — invalid pattern:"
                   << pattern.errorString();
        return out;
    }
    if (pattern.pattern().isEmpty() || pages.isEmpty()) {
        return out;
    }

#ifdef HAS_PDFIUM
    PdfiumEnvironment env;

    // Open (parse) the document ONCE for the whole page set. Previously each
    // page went through findMatches() -> extractCharsWithPositions() which called
    // FPDF_LoadDocument()/FPDF_CloseDocument() per page; in BatchMode that was
    // N_pages × N_patterns full parses. Now it is one parse per applyPatternRedactions.
    FPDF_DOCUMENT doc = FPDF_LoadDocument(pdfPath.toLocal8Bit().constData(), nullptr);
    if (!doc) {
        qWarning() << "PatternRedactor::findMatches(batch) — could not open PDF" << pdfPath;
        return out;
    }

    for (int pg : pages) {
        const QList<CharInfo> chars = extractCharsFromOpenDoc(static_cast<void*>(doc), pg);
        const QList<QRectF> rects = matchChars(chars, pattern);
        if (!rects.isEmpty()) {
            out.insert(pg, rects);
        }
    }

    FPDF_CloseDocument(doc);
#else
    // No PDFium: fall back to the per-page path (which logs the unavailability).
    for (int pg : pages) {
        const QList<QRectF> rects = findMatches(pdfPath, pg, pattern);
        if (!rects.isEmpty()) {
            out.insert(pg, rects);
        }
    }
#endif

    return out;
}
