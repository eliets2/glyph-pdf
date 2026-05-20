#pragma once

#include <QImage>
#include <QList>
#include <QString>
#include <memory>

#include "core/OcrTypes.h"
#include "core/interfaces/IOcrEngine.h"
#include "engines/ocr/OcrPreprocessor.h"

/// Strategy for multi-engine routing.
enum class OcrStrategy {
    PrimaryOnly,          // Run primary engine (Tesseract) only
    ConfidenceWeighted,   // Run primary; invoke secondary on spans with MeanTextConf < 70
    RoverVote             // Run both engines, word-level ROVER merge via IoU > 0.5
};

/// A merged OCR result word with provenance metadata.
struct MergedOcrWord {
    QString text;
    QRectF  boundingBox;
    int     confidence = 0;
    QString sourceEngine;  // "Tesseract", "RapidOCR", or "ROVER"
};

/// The OcrPipeline orchestrates preprocessing, multi-engine execution,
/// and word-level confidence-weighted ROVER merging.
class OcrPipeline {
public:
    /// Construct with primary engine.  Secondary is optional.
    explicit OcrPipeline(std::shared_ptr<IOcrEngine> primary,
                         std::shared_ptr<IOcrEngine> secondary = nullptr);
    ~OcrPipeline();

    /// Set the merging strategy.
    void setStrategy(OcrStrategy strategy);
    OcrStrategy strategy() const;

    /// Enable/disable preprocessing.
    void setPreprocessing(const OcrPreprocessOptions &opts);
    OcrPreprocessOptions preprocessingOptions() const;

    /// Run the full pipeline on a page image.
    /// Returns merged word-level results in original-image coordinates.
    QList<MergedOcrWord> run(const QImage &pageImage);

    // ── ROVER helpers (public for testability) ──────────────────────────

    /// Compute Intersection-over-Union of two bounding boxes.
    static double computeIoU(const QRectF &a, const QRectF &b);

    /// Align two word lists by IoU > threshold and merge using confidence.
    static QList<MergedOcrWord> roverMerge(
        const QList<OcrResult> &primary,   const QString &primaryName,
        const QList<OcrResult> &secondary, const QString &secondaryName,
        double iouThreshold = 0.5);

private:
    class Private;
    std::unique_ptr<Private> d;
};
