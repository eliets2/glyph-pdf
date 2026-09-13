// SPDX-License-Identifier: Apache-2.0
#include "engines/pdfium/PdfiumBackend.h"
#include <QMutexLocker>
#include <QDebug>
#include <cmath>
#include <cstdint>

// AR-6 D1 — SERIAL-RENDER CONTRACT (justification).
// Every public method here takes m_mutex, so all render/extract/search calls
// against this backend are serialised. This is INTENTIONAL, not an oversight:
// PDFium page objects loaded from a single FPDF_DOCUMENT are not safe to render
// concurrently (FPDF_LoadPage/RenderPageBitmap mutate shared document state).
// Genuinely concurrent rendering would require a POOL of independent backend
// instances, each owning its own FPDF_LoadDocument handle to the same file —
// a larger ownership change (pool lifecycle, per-thread instance routing from
// RenderCache, N× the document memory). We deliberately keep render serial and
// instead prevent the one real harm of serial render — background prefetch
// stalling the foreground UI — by running prefetch at LowestPriority
// (RenderCache::prefetchViewport, AR-6 D1). LaneScheduler/CrossPagePipeline
// parallelism remains real for the CPU/GPU OCR stages, which do NOT funnel
// through this single backend mutex.

#ifdef HAS_PDFIUM

PdfiumBackend::PdfiumBackend() {
}

PdfiumBackend::~PdfiumBackend() {
    closeDocument();
}

bool PdfiumBackend::loadDocument(const QString &filePath) {
    QMutexLocker locker(&m_mutex);
    closeDocument();

    m_filePath = filePath;
    m_document = FPDF_LoadDocument(filePath.toUtf8().constData(), nullptr);
    if (!m_document) {
        qWarning() << "PDFium failed to load document:" << filePath;
        return false;
    }
    return true;
}

void PdfiumBackend::closeDocument() {
    if (m_document) {
        FPDF_CloseDocument(m_document);
        m_document = nullptr;
    }
    m_filePath.clear();
}

bool PdfiumBackend::isLoaded() const {
    QMutexLocker locker(&m_mutex);
    return m_document != nullptr;
}

int PdfiumBackend::pageCount() const {
    QMutexLocker locker(&m_mutex);
    if (!m_document) return 0;
    return FPDF_GetPageCount(m_document);
}

QSizeF PdfiumBackend::pageSize(int pageIndex) const {
    QMutexLocker locker(&m_mutex);
    if (!m_document) return QSizeF();

    FPDF_PAGE page = FPDF_LoadPage(m_document, pageIndex);
    if (!page) return QSizeF();

    double width = FPDF_GetPageWidthF(page);
    double height = FPDF_GetPageHeightF(page);
    FPDF_ClosePage(page);

    return QSizeF(width, height);
}

double PdfiumBackend::pageHeight(int pageIndex) const {
    QMutexLocker locker(&m_mutex);
    if (!m_document) return 0.0;

    FPDF_PAGE page = FPDF_LoadPage(m_document, pageIndex);
    if (!page) return 0.0;

    double height = FPDF_GetPageHeightF(page);
    FPDF_ClosePage(page);

    return height;
}

QImage PdfiumBackend::renderPage(int pageIndex, int dpi) {
    QMutexLocker locker(&m_mutex);
    if (!m_document) return QImage();

    FPDF_PAGE page = FPDF_LoadPage(m_document, pageIndex);
    if (!page) return QImage();

    double width = FPDF_GetPageWidthF(page);
    double height = FPDF_GetPageHeightF(page);
    qreal scale = dpi / 72.0;

    int widthPixels = static_cast<int>(width * scale);
    int heightPixels = static_cast<int>(height * scale);

    // NF-4: Clamp render dimensions to prevent OOM/DoS from a crafted large
    // /MediaBox + /UserUnit yielding a multi-GB allocation or int overflow.
    if (widthPixels <= 0 || heightPixels <= 0 ||
        widthPixels > 20000 || heightPixels > 20000 ||
        static_cast<int64_t>(widthPixels) * heightPixels > 120000000) {
        qWarning() << "PdfiumBackend::renderPage: page" << pageIndex
                   << "render dimensions" << widthPixels << "x" << heightPixels
                   << "exceed safe limits — rejecting to prevent OOM/DoS";
        FPDF_ClosePage(page);
        return QImage();
    }

    QImage image(widthPixels, heightPixels, QImage::Format_ARGB32);
    image.fill(Qt::white);

    FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(widthPixels, heightPixels, FPDFBitmap_BGRA, image.bits(), image.bytesPerLine());
    if (bitmap) {
        FPDF_RenderPageBitmap(bitmap, page, 0, 0, widthPixels, heightPixels, 0, FPDF_LCD_TEXT | FPDF_ANNOT);
        FPDFBitmap_Destroy(bitmap);
    }

    FPDF_ClosePage(page);
    return image;
}

QImage PdfiumBackend::renderTile(int pageIndex, const QRectF &subRect, int dpi) {
    QMutexLocker locker(&m_mutex);
    if (!m_document) return QImage();

    FPDF_PAGE page = FPDF_LoadPage(m_document, pageIndex);
    if (!page) return QImage();

    double height = FPDF_GetPageHeightF(page);
    qreal scale = dpi / 72.0;

    int tileW = static_cast<int>(subRect.width() * scale);
    int tileH = static_cast<int>(subRect.height() * scale);

    // NF-4: Clamp tile dimensions to prevent OOM/DoS from a crafted large
    // /MediaBox + /UserUnit yielding a multi-GB allocation or int overflow.
    if (tileW <= 0 || tileH <= 0 ||
        tileW > 20000 || tileH > 20000 ||
        static_cast<int64_t>(tileW) * tileH > 120000000) {
        qWarning() << "PdfiumBackend::renderTile: tile dimensions" << tileW << "x" << tileH
                   << "exceed safe limits — rejecting to prevent OOM/DoS";
        FPDF_ClosePage(page);
        return QImage();
    }

    QImage tileImage(tileW, tileH, QImage::Format_ARGB32);
    tileImage.fill(Qt::white);

    FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(tileW, tileH, FPDFBitmap_BGRA, tileImage.bits(), tileImage.bytesPerLine());
    if (bitmap) {
        // Construct matrix for translating and scaling
        FS_MATRIX matrix;
        matrix.a = static_cast<float>(scale);
        matrix.b = 0.0f;
        matrix.c = 0.0f;
        matrix.d = static_cast<float>(-scale);
        matrix.e = static_cast<float>(-subRect.x() * scale);
        matrix.f = static_cast<float>((height - subRect.y()) * scale);

        FS_RECTF clip;
        clip.left = 0.0f;
        clip.top = 0.0f;
        clip.right = static_cast<float>(tileW);
        clip.bottom = static_cast<float>(tileH);

        FPDF_RenderPageBitmapWithMatrix(bitmap, page, &matrix, &clip, FPDF_LCD_TEXT | FPDF_ANNOT);
        FPDFBitmap_Destroy(bitmap);
    }

    FPDF_ClosePage(page);
    return tileImage;
}

QList<QRectF> PdfiumBackend::searchText(int pageIndex, const QString &query) {
    QList<QRectF> results;
    if (query.isEmpty()) return results;

    QMutexLocker locker(&m_mutex);
    if (!m_document) return results;

    FPDF_PAGE page = FPDF_LoadPage(m_document, pageIndex);
    if (!page) return results;

    FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
    if (!textPage) {
        FPDF_ClosePage(page);
        return results;
    }

    double height = FPDF_GetPageHeightF(page);

    // Convert query to UTF-16
    QList<unsigned short> query_w(query.length() + 1);
    int len = query.toWCharArray(reinterpret_cast<wchar_t*>(query_w.data()));
    query_w[len] = 0;

    FPDF_SCHHANDLE search = FPDFText_FindStart(textPage, query_w.data(), 0, 0);
    if (search) {
        while (FPDFText_FindNext(search)) {
            int charIndex = FPDFText_GetSchResultIndex(search);
            int count = FPDFText_GetSchCount(search);
            if (count <= 0) continue;

            double left = 0, right = 0, bottom = 0, top = 0;
            bool first = true;
            for (int i = 0; i < count; ++i) {
                double c_left = 0, c_right = 0, c_bottom = 0, c_top = 0;
                if (FPDFText_GetCharBox(textPage, charIndex + i, &c_left, &c_right, &c_bottom, &c_top)) {
                    if (first) {
                        left = c_left;
                        right = c_right;
                        bottom = c_bottom;
                        top = c_top;
                        first = false;
                    } else {
                        left = qMin(left, c_left);
                        right = qMax(right, c_right);
                        bottom = qMin(bottom, c_bottom);
                        top = qMax(top, c_top);
                    }
                }
            }

            if (!first) {
                results.append(QRectF(left, height - top, right - left, top - bottom));
            }
        }
        FPDFText_FindClose(search);
    }

    FPDFText_ClosePage(textPage);
    FPDF_ClosePage(page);
    return results;
}

QString PdfiumBackend::extractText(int pageIndex) {
    QMutexLocker locker(&m_mutex);
    if (!m_document) return QString();

    FPDF_PAGE page = FPDF_LoadPage(m_document, pageIndex);
    if (!page) return QString();

    FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
    if (!textPage) {
        FPDF_ClosePage(page);
        return QString();
    }

    int charCount = FPDFText_CountChars(textPage);
    if (charCount <= 0) {
        FPDFText_ClosePage(textPage);
        FPDF_ClosePage(page);
        return QString();
    }

    QList<unsigned short> buffer(charCount + 1);
    int written = FPDFText_GetText(textPage, 0, charCount, buffer.data());

    // R13 (PERF-05): FPDFText_GetText's return count INCLUDES the trailing NUL
    // terminator it writes (fpdf_text.h: "Number of characters written into
    // the result buffer, including the trailing terminator"). Handing that
    // count straight to QString::fromUtf16 appended U+0000 to every page's
    // text, so the LAST word token of every page leaked a NUL into the diff
    // tokens and every other extraction consumer. Strip EXACTLY that one
    // terminator: validate the count against the buffer first and drop the
    // final character only when it actually is NUL. Interior U+0000 characters
    // (a ToUnicode CMap may legitimately map a glyph to 0x0000) are document
    // content and are preserved — only the API's own terminator is removed.
    int textLen = written;
    if (written > 0 && written <= buffer.size() && buffer[written - 1] == 0)
        --textLen;  // the single API terminator, nothing else

    QString text = QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.data()),
                                      textLen > 0 ? textLen : 0);

    FPDFText_ClosePage(textPage);
    FPDF_ClosePage(page);
    return text;
}

// R09 (F07) — page text WITH geometry, through PDFium's decoded text path.
//
// Ownership: a private FPDF_PAGE + FPDF_TEXTPAGE are loaded per call from this
// backend's own document and closed again here — the caller never sees a raw
// handle. ConversionManager builds a per-operation PdfiumBackend instance for
// that reason (never the live viewer's handles across threads).
//
// Geometry is normalized exactly once, here: each char contributes its box
// (FPDFText_GetCharBox) and its baseline origin (FPDFText_GetCharOrigin); a
// run is anchored at its first char's baseline origin, with width = real
// glyph extent. Consumers (conversion rows, HTML/PPTX overlays) receive one
// consistent user-space rect per run instead of re-deriving positions.
//
// Order: chars are consumed strictly in PDFium char-index order — the same
// order FPDFText_GetText presents. Runs are only ever SPLIT (line change or
// newline), never reordered, so the logical order PDFium computed for
// RTL/bidi/mixed-direction content is preserved.
QList<PdfiumBackend::TextRun> PdfiumBackend::extractPageTextRuns(int pageIndex) {
    QMutexLocker locker(&m_mutex);
    QList<TextRun> runs;
    if (!m_document) return runs;

    FPDF_PAGE page = FPDF_LoadPage(m_document, pageIndex);
    if (!page) return runs;

    FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
    if (!textPage) {
        FPDF_ClosePage(page);
        return runs;
    }

    const int charCount = FPDFText_CountChars(textPage);

    // V03 (PARITY-BRANCH-REVIEW-2026-09-05): line grouping alone collapsed
    // table columns — "Name" @x=72 and "Amount" @x=300 on one baseline became
    // ONE run and then one Excel/CSV cell. Line grouping (baseline distance)
    // must stay separate from CELL boundaries: runs also split at wide
    // horizontal gaps, wide space-separator glyphs, and font-run changes.
    // Documented tolerances:
    //   * column gap  — a char starting beyond the open run's right edge by
    //     more than 1 em of its font size (1pt floor) starts a new cell.
    //     Ordinary inter-word advances in text fonts are ~0.25-0.35 em, so
    //     prose is never split at every ordinary space; real table gaps are
    //     many em wide.
    //   * space separator — a SPACE char whose own box is wider than 0.8 em
    //     is layout, not content (PDFium generates gap-marker spaces whose
    //     box spans the whole jump; justified/stretched spaces behave the
    //     same). It is dropped instead of being appended to a cell.
    //   * font runs — a change of the real base-font name splits the run so
    //     downstream font consumers see the same run boundaries the PDF does.
    constexpr double kColumnGapEm = 1.0;
    constexpr double kSpaceSeparatorEm = 0.8;

    TextRun run;
    bool runOpen = false;
    double runRight = 0.0;

    auto flushRun = [&]() {
        if (runOpen && !run.text.isEmpty()) {
            run.rect = QRectF(run.rect.x(), run.rect.y(),
                              qMax(0.0, runRight - run.rect.x()), run.fontSize);
            runs.append(run);
        }
        run = TextRun();
        runOpen = false;
    };

    for (int i = 0; i < charCount; ++i) {
        const unsigned int u = FPDFText_GetUnicode(textPage, i);
        if (u == 0) continue;
        // PDFium emits generated \r\n markers at paragraph/line ends; a hard
        // newline ends the current line run. The terminator itself is not
        // content — drop it and close the run.
        if (u == '\r' || u == '\n') {
            flushRun();
            continue;
        }

        double originX = 0.0, originY = 0.0;
        bool hasOrigin = FPDFText_GetCharOrigin(textPage, i, &originX, &originY) != 0;
        double left = 0.0, right = 0.0, bottom = 0.0, top = 0.0;
        const bool hasBox = FPDFText_GetCharBox(textPage, i, &left, &right, &bottom, &top) != 0;
        const double size = FPDFText_GetFontSize(textPage, i);
        // Real base-font name (UTF-8), read for EVERY char so a font-run
        // change can split the run (V03); the run-start branch below just
        // adopts the current char's name.
        char nameBuf[128];
        int flags = 0;
        QString fontName;
        const unsigned long nameLen = FPDFText_GetFontInfo(
            textPage, i, nameBuf, sizeof(nameBuf), &flags);
        if (nameLen > 0 && nameLen <= sizeof(nameBuf))
            fontName = QString::fromUtf8(nameBuf,
                                         static_cast<qsizetype>(nameLen - 1));

        // V03: a wide space glyph is a cell separator — drop it and close the
        // run. (Checked while a run is open; a separator before any content
        // is likewise not content and is skipped.)
        if (u == ' ' && hasBox
                && (right - left) > kSpaceSeparatorEm * qMax(size, 1.0)) {
            flushRun();
            continue;
        }

        // Line clustering tolerance (documented): two chars share a line when
        // their baseline origins differ by at most half the larger font size
        // (1pt floor for degenerate 0-size runs). Superscripts/subscripts
        // stay on the line; genuinely different baselines do not.
        if (runOpen) {
            const double tol = qMax(1.0, 0.5 * qMax(size, run.fontSize));
            if (hasOrigin && std::fabs(originY - run.rect.y()) > tol) {
                flushRun();
            }
        }

        // V03: a char starting a full em beyond the open run's right edge is
        // a new cell/column, not a continuation of the same run. Spaces
        // appended immediately before the gap (PDFium's generated gap markers
        // carry a degenerate box, so the wide-space rule above cannot see
        // them) are layout, not content — they are dropped with the boundary,
        // which keeps cell text exact ("Name", never "Name ").
        if (runOpen && hasBox
                && left - runRight > kColumnGapEm * qMax(size, 1.0)) {
            while (run.text.endsWith(QChar(u' '))) run.text.chop(1);
            flushRun();
        }

        // V03: a font-run change splits the run (same baseline stays one LINE
        // for grouping, but the font consumers get separate runs).
        if (runOpen && !run.fontName.isEmpty() && !fontName.isEmpty()
                && fontName != run.fontName) {
            flushRun();
        }

        if (!runOpen) {
            runOpen = true;
            run.rect = QRectF(hasOrigin ? originX : (hasBox ? left : 0.0),
                              hasOrigin ? originY : (hasBox ? bottom : 0.0),
                              0.0, qMax(0.0, size));
            run.fontSize = qMax(0.0, size);
            runRight = hasBox ? right : run.rect.x();
            run.fontName = fontName;
        } else {
            run.fontSize = qMax(run.fontSize, qMax(0.0, size));
            if (hasBox && right > runRight) runRight = right;
        }

        if (u > 0xFFFF && u <= 0x10FFFF) {
            // Supplementary-plane code point -> UTF-16 surrogate pair.
            const char32_t cp = u - 0x10000;
            run.text.append(QChar(static_cast<char16_t>(0xD800 + (cp >> 10))));
            run.text.append(QChar(static_cast<char16_t>(0xDC00 + (cp & 0x3FF))));
        } else {
            run.text.append(QChar(static_cast<char16_t>(u)));
        }
    }
    flushRun();

    FPDFText_ClosePage(textPage);
    FPDF_ClosePage(page);
    return runs;
}

#else // HAS_PDFIUM fallback

PdfiumBackend::PdfiumBackend() {}
PdfiumBackend::~PdfiumBackend() {}

bool PdfiumBackend::loadDocument(const QString &filePath) { Q_UNUSED(filePath); return false; }
void PdfiumBackend::closeDocument() {}
bool PdfiumBackend::isLoaded() const { return false; }
int PdfiumBackend::pageCount() const { return 0; }
QSizeF PdfiumBackend::pageSize(int pageIndex) const { Q_UNUSED(pageIndex); return QSizeF(); }
double PdfiumBackend::pageHeight(int pageIndex) const { Q_UNUSED(pageIndex); return 0.0; }

QImage PdfiumBackend::renderPage(int pageIndex, int dpi) {
    Q_UNUSED(pageIndex); Q_UNUSED(dpi);
    return QImage();
}

QImage PdfiumBackend::renderTile(int pageIndex, const QRectF &subRect, int dpi) {
    Q_UNUSED(pageIndex); Q_UNUSED(subRect); Q_UNUSED(dpi);
    return QImage();
}

QList<QRectF> PdfiumBackend::searchText(int pageIndex, const QString &query) {
    Q_UNUSED(pageIndex); Q_UNUSED(query);
    return QList<QRectF>();
}

QString PdfiumBackend::extractText(int pageIndex) {
    Q_UNUSED(pageIndex);
    return QString();
}

QList<PdfiumBackend::TextRun> PdfiumBackend::extractPageTextRuns(int pageIndex) {
    Q_UNUSED(pageIndex);
    return QList<PdfiumBackend::TextRun>();
}

#endif
