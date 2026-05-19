#pragma once
#include <QImage>
#include <QRectF>

class IPdfRenderer {
public:
    virtual ~IPdfRenderer() = default;
    virtual QImage renderPage(int pageIndex, int dpi) = 0;
    virtual QImage renderTile(int pageIndex, const QRectF &subRect, int dpi) = 0;
};
