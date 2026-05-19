#pragma once

#include <QString>
#include <QImage>
#include <QRectF>
#include <QList>
#include <memory>

#include "core/interfaces/IOcrEngine.h"

class OcrEngine final : public IOcrEngine
{
public:
    OcrEngine();
    ~OcrEngine() override;

    // Initialize Tesseract API with language (e.g., "eng")
    bool initialize(const QString &language = "eng", const QString &dataPath = "") override;

    // Process a single rendered page image and return structured text bounding boxes
    QList<OcrResult> processImage(const QImage &image) override;

    // Get raw text
    QString getRawText(const QImage &image) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};
