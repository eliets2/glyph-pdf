// SPDX-License-Identifier: Apache-2.0
#include "engines/pdfium/PdfiumBackend.h"
#include <QMutexLocker>
#include <QDebug>
#include <cstdint>

#ifdef HAS_PDFIUM

static int s_pdfiumRefCount = 0;
static QMutex s_pdfiumMutex;

PdfiumBackend::PdfiumBackend() {
    QMutexLocker locker(&s_pdfiumMutex);
    if (s_pdfiumRefCount == 0) {
        FPDF_InitLibrary();
    }
    s_pdfiumRefCount++;
}

PdfiumBackend::~PdfiumBackend() {
    closeDocument();
    QMutexLocker locker(&s_pdfiumMutex);
    s_pdfiumRefCount--;
    if (s_pdfiumRefCount == 0) {
        FPDF_DestroyLibrary();
    }
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

    QString text = QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.data()), written);

    FPDFText_ClosePage(textPage);
    FPDF_ClosePage(page);
    return text;
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

#endif
