// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QList>
#include <QRectF>

class IPdfSearcher {
public:
    virtual ~IPdfSearcher() = default;
    virtual QList<QRectF> searchText(int pageIndex, const QString &query) = 0;
};
