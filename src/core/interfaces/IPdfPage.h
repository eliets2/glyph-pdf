#pragma once
#include <QSizeF>

class IPdfPage {
public:
    virtual ~IPdfPage() = default;
    virtual int index() const = 0;
    virtual QSizeF size() const = 0;
    virtual int rotation() const = 0;
};
