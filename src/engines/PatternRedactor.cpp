// SPDX-License-Identifier: Apache-2.0
#include "engines/PatternRedactor.h"

#include <QHash>
#include <QDebug>
#include <mutex>

// PGR-52 (2026-09-30): the bounded matcher compiles and runs redact patterns
// against the SAME 16-bit PCRE2 build Qt6Core itself links (libpcre2-16-0.dll),
// because QRegularExpression exposes no match timeout (verified in the Qt 6.11
// headers — MatchOption/PatternOption carry nothing of the sort) and PCRE2's
// implicit default MATCH_LIMIT only made a hostile pattern yield a SILENT
// "no match" (a redaction false negative), not an honest failure. The
// interpreter (not JIT) is used so the per-match budget is always enforced.
#define PCRE2_CODE_UNIT_WIDTH 16
#include <pcre2.h>

#ifdef HAS_PDFIUM
#include <fpdfview.h>
#include <fpdf_text.h>
#include "engines/pdfium/PdfiumEnvironment.h"
#endif

// ---------------------------------------------------------------------------
// PGR-52: the bounded PCRE2-16 matcher
// ---------------------------------------------------------------------------

// Compile options mirror what QRegularExpression does for an equivalent
// default-constructed pattern (Qt 6 compiles every pattern with PCRE2_UTF
// because QString is UTF-16; the pattern-syntax options beyond that are
// opt-in and would already be embedded in the pattern text). The SUBJECT is
// UTF-16 QString data and is checked (no PCRE2_NO_UTF_CHECK) — a page text
// with unpaired surrogates yields no matches on that page, exactly what the
// previous QRegularExpression path did.
static constexpr quint32 kPcre2CompileOptions = PCRE2_UTF;

class PatternRedactor::BoundedMatcher {
public:
    BoundedMatcher(const QRegularExpression& pattern, quint64 matchLimit) {
        if (pattern.pattern().isEmpty())
            return;
        PCRE2_SIZE erroffset = 0;
        int errcode = 0;
        m_code = pcre2_compile_16(
            reinterpret_cast<PCRE2_SPTR16>(pattern.pattern().constData()),
            PCRE2_ZERO_TERMINATED,
            kPcre2CompileOptions,
            &errcode, &erroffset,
            nullptr /* ccontext */);
        if (!m_code)
            return;
        m_matchContext = pcre2_match_context_create_16(nullptr);
        if (!m_matchContext) {
            pcre2_code_free_16(m_code);
            m_code = nullptr;
            return;
        }
        // THE seam: the per-match compute budget. Both limits are set
        // explicitly so the budget does not drift with PCRE2 defaults.
        pcre2_set_match_limit_16(m_matchContext,
                                 static_cast<uint32_t>(matchLimit));
        pcre2_set_depth_limit_16(m_matchContext,
                                 static_cast<uint32_t>(matchLimit));
        m_matchData = pcre2_match_data_create_from_pattern_16(m_code, nullptr);
        if (!m_matchData) {
            pcre2_match_context_free_16(m_matchContext);
            m_matchContext = nullptr;
            pcre2_code_free_16(m_code);
            m_code = nullptr;
        }
    }

    ~BoundedMatcher() {
        if (m_matchData)
            pcre2_match_data_free_16(m_matchData);
        if (m_matchContext)
            pcre2_match_context_free_16(m_matchContext);
        if (m_code)
            pcre2_code_free_16(m_code);
    }

    BoundedMatcher(const BoundedMatcher&) = delete;
    BoundedMatcher& operator=(const BoundedMatcher&) = delete;

    bool valid() const { return m_code != nullptr; }

    /// Global matching over `subject`, replicating QRegularExpression::
    /// globalMatch's zero-length-match advance algorithm (the PCRE2-recommended
    /// NOTEMPTY_ATSTART|ANCHORED retry at the same offset, then step one unit).
    /// Zero-length spans are NOT emitted (the consumer skips them; the loop
    /// rules keep them from stalling the scan). Sets *budgetExceeded and stops
    /// at the FIRST budget trip — the caller converts that to an honest
    /// failure; partial spans before the trip are returned.
    QVector<QPair<int, int>> globalMatches(const QString& subject,
                                           bool* budgetExceeded) const {
        QVector<QPair<int, int>> spans;
        if (budgetExceeded)
            *budgetExceeded = false;
        if (!valid())
            return spans;

        const int length = subject.size();
        const auto* data = reinterpret_cast<PCRE2_SPTR16>(subject.constData());
        PCRE2_SIZE* const ovec = pcre2_get_ovector_pointer_16(m_matchData);
        quint32 options = 0;

        int offset = 0;
        while (offset <= length) {
            const int rc = pcre2_match_16(m_code, data, length,
                                          offset, options,
                                          m_matchData, m_matchContext);
            if (rc == PCRE2_ERROR_NOMATCH) {
                if (options == 0)
                    break;                       // genuinely no more matches
                options = 0;                     // anchored retry failed
                ++offset;
                continue;
            }
            if (rc == PCRE2_ERROR_MATCHLIMIT || rc == PCRE2_ERROR_DEPTHLIMIT) {
                // PGR-52: the pattern burned its per-match budget. HONEST
                // failure — surfaced, never a silent "no match here".
                if (budgetExceeded)
                    *budgetExceeded = true;
                break;
            }
            if (rc < 0) {
                // UTF-validity or internal errors: no match on this subject —
                // the same observable result the QRegularExpression path had.
                break;
            }

            const int start = static_cast<int>(ovec[0]);
            const int end   = static_cast<int>(ovec[1]);
            if (end > start)
                spans.append({start, end});

            if (end == start) {
                options = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
                // offset stays — the retry looks for a non-empty match HERE.
            } else {
                options = 0;
                offset = end;
            }
        }
        return spans;
    }

private:
    pcre2_code_16*          m_code         = nullptr;
    pcre2_match_data_16*    m_matchData    = nullptr;
    pcre2_match_context_16* m_matchContext = nullptr;
};

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

        // PGR-37 (D2 delta review 2026-09-23): the boxes are RAW PDF USER
        // space — FPDFText_GetCharBox values taken verbatim, stored y-up
        // (QRectF::y() = the LOWER edge). The former `pageHeight - pdf_top`
        // flip produced a space that is viewer-space ONLY on /Rotate 0
        // origin-0 pages; fed through PageSpace::viewerToUser (the SEP13 L8
        // law at the excision boundary) it transposed marks on rotated pages
        // and shifted them by the MediaBox origin — a silent redaction false
        // success. Raw user space is the ONE space every consumer can share:
        // PoDoFoBackend::applyRedactionsUserSpace consumes it verbatim, and
        // the viewer placement (RedactMode) applies the one display flip it
        // needs at ITS boundary.
        const double w = pdf_right - pdf_left;
        const double h = pdf_top - pdf_bottom;

        char32_t cp = static_cast<char32_t>(codepoint);
        result.append(CharInfo{
            QString::fromUcs4(&cp, 1),
            QRectF(pdf_left, pdf_bottom, w, h)
        });
    }

    FPDFText_ClosePage(textPage);
    FPDF_ClosePage(page);
    return result;
}
#endif

// extractCharsWithPositions (the per-call FPDF_LoadDocument path) was removed
// with PGR-52: both findMatches variants now run through findMatchesBounded,
// which opens the document itself and carries the per-match budget.

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
                                          BoundedMatcher& matcher,
                                          bool* budgetExceeded) {
    QList<QRectF> results;
    if (budgetExceeded)
        *budgetExceeded = false;
    if (chars.isEmpty()) {
        return results;
    }

    // Reconstruct the page text from the char sequence so we can run the regex.
    QString pageText;
    pageText.reserve(chars.size());
    for (const CharInfo& ci : chars) {
        pageText.append(ci.ch);
    }

    // M-3 (input cap) + PGR-52 (per-match compute budget). The pattern may be
    // a user-supplied custom regex, and a page may carry a large amount of
    // text, so the work is bounded two ways:
    //   (1) the input length fed to the regex is capped (kMaxRegexInput) — the
    //       primary driver of backtracking blow-up; and
    //   (2) every pcre2_match call runs under an explicit match_limit /
    //       depth_limit budget (the BoundedMatcher context) — a hostile
    //       pattern like `(a+)+$` is cut DETERMINISTICALLY mid-backtrack and
    //       reported through *budgetExceeded so the caller fails the file
    //       honestly ("pattern exceeded its match budget").
    // The former wall-clock loop budget (kMatchBudgetMs) is GONE: it could
    // only fire BETWEEN matches, so a single pathological match still ran to
    // completion before it could matter — and its "partial results, keep
    // going" outcome was precisely the silent skip PGR-52 outlaws. A budget
    // trip now aborts the scan and is reported.
    // Built-in named patterns are unaffected — they are linear and finish
    // orders of magnitude under the budget.
    constexpr int kMaxRegexInput = 256 * 1024;  // chars

    if (pageText.size() > kMaxRegexInput) {
        qWarning() << "PatternRedactor::matchChars — page text truncated from"
                   << pageText.size() << "to" << kMaxRegexInput
                   << "chars to bound regex backtracking (M-3)";
        pageText.truncate(kMaxRegexInput);
    }

    const QVector<QPair<int, int>> spans = matcher.globalMatches(pageText,
                                                                 budgetExceeded);
    for (const auto& span : spans) {
        const int startIdx = span.first;
        const int endIdx   = span.second;   // exclusive
        if (startIdx < 0 || endIdx <= startIdx) continue;

        const QRectF bbox = mergeCharBoxes(chars, startIdx, endIdx);
        if (!bbox.isNull() && !bbox.isEmpty()) {
            results.append(bbox);
        }
    }

    return results;
}

// ---------------------------------------------------------------------------
// PGR-52: the bounded batch search (one parse, per-match budget)
// ---------------------------------------------------------------------------

PatternRedactor::BoundedMatchResult
PatternRedactor::findMatchesBounded(const QString& pdfPath,
                                    const QList<int>& pages,
                                    const QRegularExpression& pattern,
                                    quint64 matchLimit) {
    BoundedMatchResult out;

    // Guard: invalid / empty pattern (same contract as the legacy variants).
    if (!pattern.isValid()) {
        qWarning() << "PatternRedactor::findMatchesBounded — invalid pattern:"
                   << pattern.errorString();
        return out;
    }
    if (pattern.pattern().isEmpty() || pages.isEmpty()) {
        return out;
    }

    BoundedMatcher matcher(pattern, matchLimit);
    if (!matcher.valid()) {
        // Qt already gated validity upstream — this is defensive. An empty
        // result is a no-match, never a budget claim.
        qWarning() << "PatternRedactor::findMatchesBounded — the bounded engine "
                      "refused to compile the pattern:" << pattern.pattern();
        return out;
    }

#ifdef HAS_PDFIUM
    PdfiumEnvironment env;

    // Open (parse) the document ONCE for the whole page set (unchanged from
    // the batch overload this function carries forward).
    FPDF_DOCUMENT doc = FPDF_LoadDocument(pdfPath.toLocal8Bit().constData(), nullptr);
    if (!doc) {
        qWarning() << "PatternRedactor::findMatchesBounded — could not open PDF" << pdfPath;
        return out;
    }

    for (int pg : pages) {
        const QList<CharInfo> chars = extractCharsFromOpenDoc(static_cast<void*>(doc), pg);
        bool exceeded = false;
        const QList<QRectF> rects = matchChars(chars, matcher, &exceeded);
        if (exceeded) {
            // Honest abort: the payload so far is PARTIAL by definition — the
            // caller (engine/mode boundary) must fail the operation, never
            // redact from it.
            out.budgetExceeded = true;
            out.exceededPage = pg;
            break;
        }
        if (!rects.isEmpty()) {
            out.byPage.insert(pg, rects);
        }
    }

    FPDF_CloseDocument(doc);
#else
    // No PDFium: no chars can be extracted, so no match can trip a budget —
    // the legacy per-page path's behavior (empty results) is correct as-is.
    Q_UNUSED(pdfPath);
    Q_UNUSED(matcher);
#endif

    return out;
}

// ---------------------------------------------------------------------------
// Public API: findMatches (single page)
// ---------------------------------------------------------------------------

QList<QRectF> PatternRedactor::findMatches(const QString& pdfPath,
                                            int             pageIndex,
                                            const QRegularExpression& pattern) {
    // Delegate to the bounded search; this legacy surface keeps its historical
    // signature (and its no-error payload) for the read-only preview callers.
    return findMatchesBounded(pdfPath, {pageIndex}, pattern).byPage.value(pageIndex);
}

// ---------------------------------------------------------------------------
// Public API: findMatches (batch — parses the PDF exactly once)
// ---------------------------------------------------------------------------

QHash<int, QList<QRectF>>
PatternRedactor::findMatches(const QString& pdfPath,
                             const QList<int>& pages,
                             const QRegularExpression& pattern) {
    // Legacy surface: same payload contract as before (PGR-52 consumers use
    // findMatchesBounded so a budget trip is reported, not swallowed).
    return findMatchesBounded(pdfPath, pages, pattern).byPage;
}
