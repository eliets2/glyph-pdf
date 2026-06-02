// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QRectF>

struct OcrResult {
    QString text;
    QRectF boundingBox;
    int confidence;
};
