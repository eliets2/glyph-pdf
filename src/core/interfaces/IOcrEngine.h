// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QImage>
#include <QList>
#include "core/OcrTypes.h"

class IOcrEngine {
public:
    virtual ~IOcrEngine() = default;
    virtual bool initialize(const QString &language = "eng", const QString &dataPath = "") = 0;
    virtual QList<OcrResult> processImage(const QImage &image) = 0;
    virtual QString getRawText(const QImage &image) = 0;
    virtual bool isMockImplementation() const { return false; }
protected:
    IOcrEngine() = default;
    IOcrEngine(const IOcrEngine&) = delete;
    IOcrEngine& operator=(const IOcrEngine&) = delete;
};
