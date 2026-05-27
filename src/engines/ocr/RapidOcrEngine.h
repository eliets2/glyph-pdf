#pragma once

#include <QString>
#include <QImage>
#include <QRectF>
#include <QList>
#include <memory>

#include "core/interfaces/IOcrEngine.h"

class RapidOcrEngine final : public IOcrEngine
{
public:
    RapidOcrEngine();
    ~RapidOcrEngine() override;

    // Initialize ONNXRuntime environment and load det/cls/rec models
    bool initialize(const QString &language = "eng", const QString &dataPath = "") override;

    // Process a single image through the ONNX pipeline
    QList<OcrResult> processImage(const QImage &image) override;

    // Get raw text (concat of recognized words)
    QString getRawText(const QImage &image) override;

    bool isMockImplementation() const override;

private:
    class Private;
    std::unique_ptr<Private> d;
};
