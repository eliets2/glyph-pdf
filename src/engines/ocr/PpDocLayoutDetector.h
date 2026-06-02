// SPDX-License-Identifier: Apache-2.0
// src/engines/ocr/PpDocLayoutDetector.h
// PpDocLayoutDetector — PP-DocLayoutV2 ONNX layout detector (M5-P2 D2).
//
// Model: PaddlePaddle/PP-DocLayoutV2_onnx (Apache-2.0)
// Architecture: RT-DETR-based, input [1,3,800,800], 25 output classes.
// Model file: models/pp_doclayout/pp_doclayout_v2.onnx
//
// ONNX session is loaded once at construction and held warm — NOT spawned per
// page (non-negotiable anti-spawn-per-page rule, see 06-non-negotiables.md).
//
// Thread-safety: detect() is protected by a mutex because the ONNX session
// is single-threaded.  LayoutEnsemble submits two detectors to two separate
// LaneScheduler tasks; each detector serializes its own calls.
#pragma once

#include "engines/ocr/ILayoutDetector.h"

#include <QRecursiveMutex>
#include <QString>
#include <memory>

class PpDocLayoutDetector final : public ILayoutDetector {
public:
    /// Construct with an explicit path to the pp_doclayout_v2.onnx file.
    /// Call initialize() before the first detect().
    explicit PpDocLayoutDetector();
    ~PpDocLayoutDetector() override;

    /// Load the ONNX model from `modelPath` (absolute path to .onnx file).
    /// Returns true on success.  Safe to call multiple times; re-loads only
    /// if the path differs from the currently-loaded model.
    bool initialize(const QString &modelPath);

    /// Returns true if the ONNX session is warm and ready.
    bool isInitialized() const;

    // ILayoutDetector interface
    QList<LayoutRegion> detect(const QImage &page,
                               gp::Lane lane = gp::Lane::GPU) override;
    QString name() const override { return QStringLiteral("PP-DocLayoutV2"); }

private:
    class Private;
    std::unique_ptr<Private> d;
};
