#include "engines/PatternRedactor.h"

#include <QHash>
#include <QDebug>

#ifdef HAS_PDFIUM
#include <fpdfview.h>
#include <fpdf_text.h>
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
        return QRegularExpression(); // invalid
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

QList<PatternRedactor::CharInfo>
PatternRedactor::extractCharsWithPositions(const QString& pdfPath, int pageIndex) {
    QList<CharInfo> result;

#ifdef HAS_PDFIUM
    // FPDF_InitLibrary is idempotent; callers may call multiple times safely.
    static bool s_pdfiumInitialized = false;
    if (!s_pdfiumInitialized) {
        FPDF_InitLibrary();
        s_pdfiumInitialized = true;
    }

    FPDF_DOCUMENT doc = FPDF_LoadDocument(pdfPath.toLocal8Bit().constData(), nullptr);
    if (!doc) {
        qWarning() << "PatternRedactor: could not open PDF" << pdfPath;
        return result;
    }

    const int pageCount = FPDF_GetPageCount(doc);
    if (pageIndex < 0 || pageIndex >= pageCount) {
        FPDF_CloseDocument(doc);
        return result;
    }

    FPDF_PAGE page = FPDF_LoadPage(doc, pageIndex);
    if (!page) {
        FPDF_CloseDocument(doc);
        return result;
    }

    const double pageHeight = static_cast<double>(FPDF_GetPageHeightF(page));

    FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
    if (!textPage) {
        FPDF_ClosePage(page);
        FPDF_CloseDocument(doc);
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
            result.append(CharInfo{ QChar(static_cast<uint>(codepoint)), QRectF() });
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

        result.append(CharInfo{
            QChar(static_cast<uint>(codepoint)),
            QRectF(qtX, qtY, qtW, qtH)
        });
    }

    FPDFText_ClosePage(textPage);
    FPDF_ClosePage(page);
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
    for (int i = startIdx; i < endIdx && i < chars.size(); ++i) {
        const QRectF& box = chars[i].bbox;
        if (box.isNull() || box.isEmpty()) continue;
        if (first) {
            merged = box;
            first = false;
        } else {
            merged = merged.united(box);
        }
    }
    return merged;
}

// ---------------------------------------------------------------------------
// Public API: findMatches
// ---------------------------------------------------------------------------

QList<QRectF> PatternRedactor::findMatches(const QString& pdfPath,
                                            int             pageIndex,
                                            const QRegularExpression& pattern) {
    QList<QRectF> results;

    // Guard: invalid pattern
    if (!pattern.isValid()) {
        qWarning() << "PatternRedactor::findMatches — invalid pattern:"
                   << pattern.errorString();
        return results;
    }
    if (pattern.pattern().isEmpty()) {
        return results;
    }

    const QList<CharInfo> chars = extractCharsWithPositions(pdfPath, pageIndex);
    if (chars.isEmpty()) {
        return results;
    }

    // Reconstruct the page text from the char sequence so we can run the regex.
    QString pageText;
    pageText.reserve(chars.size());
    for (const CharInfo& ci : chars) {
        pageText.append(ci.ch);
    }

    // Run the regex against the full page text
    QRegularExpressionMatchIterator it = pattern.globalMatch(pageText);
    while (it.hasNext()) {
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
