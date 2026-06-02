// SPDX-License-Identifier: Apache-2.0
// src/engines/conversion/DjvuImporter.cpp
// DjvuImporter — optional DjVu → MRC PDF/A importer.
//
// DjVuLibre API usage (when HAS_DJVU=ON):
//   - ddjvu_context_create() → allocate context
//   - ddjvu_document_create_by_filename_utf8() → open file
//   - ddjvu_document_get_pagenum() → page count
//   - Per page: ddjvu_page_create_by_pageno() → render via ddjvu_page_render()
//   - ddjvu_page_get_text() → extract hidden text
//   - Proper RAII cleanup via ddjvu_context_release() and ddjvu_document_release()
//
// Without HAS_DJVU: all methods return error stubs. No DjVuLibre headers included.
#include "engines/conversion/DjvuImporter.h"

#include <QDebug>
#include <QFileInfo>

#ifdef HAS_DJVU
#include <libdjvu/ddjvuapi.h>
#include <libdjvu/miniexp.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────

/*static*/ bool DjvuImporter::isAvailable()
{
#ifdef HAS_DJVU
    return true;
#else
    return false;
#endif
}

DjvuImportResult DjvuImporter::importFile(const QString& filePath, int dpi) const
{
    DjvuImportResult result;

#ifdef HAS_DJVU
    // ── Validate file ──────────────────────────────────────────────────────
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isReadable()) {
        result.errorMessage = QStringLiteral("DjvuImporter: file not found or not readable: %1")
                              .arg(filePath);
        return result;
    }

    QString suffix = fi.suffix().toLower();
    if (suffix != QLatin1String("djvu") && suffix != QLatin1String("djv")) {
        result.errorMessage = QStringLiteral("DjvuImporter: expected .djvu or .djv file, got: %1")
                              .arg(filePath);
        return result;
    }

    // ── Open context + document ────────────────────────────────────────────
    ddjvu_context_t* ctx = ddjvu_context_create("GlyphPDF-DjvuImporter");
    if (!ctx) {
        result.errorMessage = QStringLiteral("DjvuImporter: ddjvu_context_create failed");
        return result;
    }

    // ddjvu_document_create_by_filename_utf8 is available in DjVuLibre >= 3.5.
    ddjvu_document_t* doc = ddjvu_document_create_by_filename_utf8(
        ctx, filePath.toUtf8().constData(), 1 /*cache*/);
    if (!doc) {
        ddjvu_context_release(ctx);
        result.errorMessage = QStringLiteral("DjvuImporter: cannot open DjVu file: %1")
                              .arg(filePath);
        return result;
    }

    // Wait for document to be decoded
    ddjvu_message_t* msg;
    while ((msg = ddjvu_message_wait(ctx)) != nullptr) {
        if (msg->m_any.tag == DDJVU_DOCINFO) break;
        if (msg->m_any.tag == DDJVU_ERROR) {
            QString err = QString::fromUtf8(msg->m_error.message);
            ddjvu_message_pop(ctx);
            ddjvu_document_release(doc);
            ddjvu_context_release(ctx);
            result.errorMessage = QStringLiteral("DjvuImporter: error decoding document: %1").arg(err);
            return result;
        }
        ddjvu_message_pop(ctx);
    }
    if (msg) ddjvu_message_pop(ctx);

    int pageCount = ddjvu_document_get_pagenum(doc);
    result.pageCount = pageCount;
    result.pageImages.reserve(pageCount);
    result.pageTexts.reserve(pageCount);

    // ── Per-page rendering ─────────────────────────────────────────────────
    ddjvu_format_t* fmt = ddjvu_format_create(DDJVU_FORMAT_RGB24, 0, nullptr);
    ddjvu_format_set_row_order(fmt, 1 /*top to bottom*/);

    for (int pi = 0; pi < pageCount; ++pi) {
        ddjvu_page_t* page = ddjvu_page_create_by_pageno(doc, pi);
        if (!page) {
            result.pageImages.append(QImage());
            result.pageTexts.append(QString());
            continue;
        }

        // Wait for page decode
        while ((msg = ddjvu_message_wait(ctx)) != nullptr) {
            if (msg->m_any.tag == DDJVU_PAGEINFO) break;
            if (msg->m_any.tag == DDJVU_ERROR) break;
            ddjvu_message_pop(ctx);
        }
        if (msg) ddjvu_message_pop(ctx);

        int pageW = ddjvu_page_get_width(page);
        int pageH = ddjvu_page_get_height(page);

        // Scale to requested DPI (DjVu pages have a natural resolution)
        int natDpi = ddjvu_page_get_resolution(page);
        if (natDpi <= 0) natDpi = 100;  // fallback

        int renderW = (int)(pageW * dpi / (double)natDpi);
        int renderH = (int)(pageH * dpi / (double)natDpi);
        if (renderW < 1) renderW = 1;
        if (renderH < 1) renderH = 1;

        ddjvu_rect_t prect;
        prect.x = 0; prect.y = 0; prect.w = (unsigned)renderW; prect.h = (unsigned)renderH;
        ddjvu_rect_t rrect = prect;

        // Allocate RGB24 buffer
        int stride = renderW * 3;
        QByteArray imgBuf(stride * renderH, '\0');

        int ok = ddjvu_page_render(page, DDJVU_RENDER_COLOR,
                                   &prect, &rrect, fmt,
                                   (unsigned long)stride,
                                   imgBuf.data());

        QImage qi;
        if (ok) {
            // Construct QImage from RGB24 buffer
            qi = QImage(renderW, renderH, QImage::Format_RGB888);
            for (int y = 0; y < renderH; ++y) {
                std::memcpy(qi.scanLine(y),
                            imgBuf.constData() + y * stride,
                            stride);
            }
        }
        result.pageImages.append(qi);

        // ── Extract hidden text ───────────────────────────────────────────
        miniexp_t textExp = ddjvu_document_get_pagetext(doc, pi, "word");
        QString pageText;
        // Walk the s-expression to extract text tokens
        // Text s-expression format: (page ...children... (word x1 y1 x2 y2 "text"))
        // We do a simple recursive scan for "word" atoms
        std::function<void(miniexp_t)> extractText = [&](miniexp_t exp) {
            if (miniexp_stringp(exp)) {
                const char* str = miniexp_to_str(exp);
                if (str && *str) {
                    if (!pageText.isEmpty()) pageText += QLatin1Char(' ');
                    pageText += QString::fromUtf8(str);
                }
                return;
            }
            if (!miniexp_listp(exp)) return;
            for (miniexp_t it = exp; it != miniexp_nil; it = miniexp_cdr(it)) {
                miniexp_t child = miniexp_car(it);
                if (miniexp_symbolp(child)) continue;  // skip symbol type tags
                extractText(child);
            }
        };
        if (textExp != miniexp_nil && textExp != miniexp_dummy)
            extractText(textExp);
        result.pageTexts.append(pageText);

        ddjvu_page_release(page);
    }

    ddjvu_format_release(fmt);
    ddjvu_document_release(doc);
    ddjvu_context_release(ctx);

    result.success = true;
    return result;

#else
    // HAS_DJVU not defined — return informative stub
    Q_UNUSED(filePath)
    Q_UNUSED(dpi)
    result.errorMessage = QStringLiteral(
        "DjVu support is not enabled in this build. "
        "Reconfigure with -DHAS_DJVU=ON to enable DjVuLibre integration. "
        "Note: DjVuLibre is GPL-2.0+; enabling it accepts the GPL-2.0+ dependency.");
    return result;
#endif
}
