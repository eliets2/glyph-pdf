// SPDX-License-Identifier: Apache-2.0
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

    // G11 (QUALITY-GATE-2026-09-09): REAL readiness check for the model set in
    // `modelsDir`, shared with the engine — it builds the same Ort::Session
    // objects initialize() would build over the detector and recognizer files
    // in that directory (the textline classifier stays optional). File
    // presence alone is NOT readiness: arbitrary non-empty .onnx payloads
    // fail ONNX protobuf parsing, and only an actual load proves the set is
    // usable. No shared state (a throw-away environment per call); on failure
    // *errorOut (when non-null) carries the engine's own reason. Builds
    // without onnxruntime always report false — they cannot run the models.
    static bool verifyModelsIn(const QString &modelsDir, QString *errorOut = nullptr);

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
