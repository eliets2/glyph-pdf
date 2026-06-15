// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QImage>
#include <QRectF>
#include <QSizeF>
#include <QString>

class IPdfRenderer {
public:
    virtual ~IPdfRenderer() = default;
    virtual QImage renderPage(int pageIndex, int dpi) = 0;
    virtual QImage renderTile(int pageIndex, const QRectF &subRect, int dpi) = 0;

    /// Returns the size of the page at \a pageIndex in points.
    /// Returns QSizeF() if \a pageIndex is out of range.
    virtual QSizeF pageSize(int pageIndex) const = 0;

    /// Extracts and returns the plain text content of the page at \a pageIndex.
    /// Returns an empty string if \a pageIndex is out of range or text extraction is unsupported.
    virtual QString extractText(int pageIndex) = 0;
};
