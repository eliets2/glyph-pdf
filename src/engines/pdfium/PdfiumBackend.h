// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "core/interfaces/IPdfRenderer.h"
#include "core/interfaces/IPdfSearcher.h"
#include <QString>
#include <QList>
#include <QRectF>
#include <QImage>
#include <QSizeF>
#include <QMutex>
#include "engines/pdfium/PdfiumEnvironment.h"

#ifdef HAS_PDFIUM
#include <fpdfview.h>
#include <fpdf_text.h>
#endif

class PdfiumBackend final : public IPdfRenderer, public IPdfSearcher {
public:
    // R09 (F07): one decoded text run of a page, with normalized geometry.
    // `rect` is anchored at the run's baseline origin in PDF user space
    // (origin bottom-left, Y up) with the run's real glyph extent as width —
    // the same anchor contract the conversion row-grouping and the PPTX/HTML
    // exporters already build on (top-of-page consumers flip Y themselves).
    // Runs arrive in PDFium char order (the same order FPDFText_GetText
    // yields), which is the content-stream/logical order — never re-sorted by
    // X, so RTL/bidi ordering PDFium already applied survives.
    //
    // W2c residual (SWEEP-W2C 2026-09-20): the run's vertical ink extent comes
    // from the PDFium char boxes (which span the font's ascender..descender —
    // the N08 metric-derived pattern), NOT a guessed size multiplier: the
    // redaction proof's string attribution intersects this band against the
    // mark, and a blanket multiplier over-attributed neighbor lines into a
    // correct redaction's mark band (false alarm). Degenerate runs with no
    // char box keep hasInkBox=false and consumers fall back conservatively.
    struct TextRun {
        QString text;
        QRectF rect;
        double fontSize = 0.0;
        QString fontName;
        double inkTop = 0.0;      // max char-box top across the run (Y up)
        double inkBottom = 0.0;   // min char-box bottom across the run (Y up)
        bool hasInkBox = false;   // at least one char contributed a real box
    };

    PdfiumBackend();
    ~PdfiumBackend() override;

    bool loadDocument(const QString &filePath);
    void closeDocument();
    bool isLoaded() const;
    int pageCount() const;
    QSizeF pageSize(int pageIndex) const override;
    double pageHeight(int pageIndex) const;

    // IPdfRenderer interface
    QImage renderPage(int pageIndex, int dpi) override;
    QImage renderTile(int pageIndex, const QRectF &subRect, int dpi) override;

    // IPdfSearcher interface
    QList<QRectF> searchText(int pageIndex, const QString &query) override;

    // Text extraction
    QString extractText(int pageIndex) override;

    // R09 (F07): smallest page-text-with-boxes method on this boundary.
    // Decoded Unicode (through the font/encoding/ToUnicode machinery, never
    // raw glyph codes) plus per-run baseline geometry, for consumers that need
    // positioning (conversion row grouping, HTML/PPTX overlays). For plain
    // text use extractText(). A page without a text layer yields an empty
    // list — callers must not fabricate text for it.
    QList<TextRun> extractPageTextRuns(int pageIndex);

private:
#ifdef HAS_PDFIUM
    FPDF_DOCUMENT m_document = nullptr;
#endif
    PdfiumEnvironment m_env;
    QString m_filePath;
    mutable QMutex m_mutex;
};
