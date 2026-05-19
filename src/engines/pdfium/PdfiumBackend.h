#pragma once

#include "core/interfaces/IPdfRenderer.h"
#include "core/interfaces/IPdfSearcher.h"
#include <QString>
#include <QList>
#include <QRectF>
#include <QImage>
#include <QSizeF>
#include <QMutex>

#ifdef HAS_PDFIUM
#include <fpdfview.h>
#include <fpdf_text.h>
#endif

class PdfiumBackend final : public IPdfRenderer, public IPdfSearcher {
public:
    PdfiumBackend();
    ~PdfiumBackend() override;

    bool loadDocument(const QString &filePath);
    void closeDocument();
    bool isLoaded() const;
    int pageCount() const;
    QSizeF pageSize(int pageIndex) const;
    double pageHeight(int pageIndex) const;

    // IPdfRenderer interface
    QImage renderPage(int pageIndex, int dpi) override;
    QImage renderTile(int pageIndex, const QRectF &subRect, int dpi) override;

    // IPdfSearcher interface
    QList<QRectF> searchText(int pageIndex, const QString &query) override;

    // Text extraction
    QString extractText(int pageIndex);

private:
#ifdef HAS_PDFIUM
    FPDF_DOCUMENT m_document = nullptr;
#endif
    QString m_filePath;
    mutable QMutex m_mutex;
};
