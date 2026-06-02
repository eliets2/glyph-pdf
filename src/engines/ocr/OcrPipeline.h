// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QFuture>
#include <QImage>
#include <QList>
#include <QString>
#include <memory>

#include "core/OcrTypes.h"
#include "core/interfaces/IOcrEngine.h"
#include "engines/ocr/ILayoutDetector.h"
#include "engines/ocr/OcrPreprocessor.h"
#include "engines/scheduling/LaneScheduler.h"
#include "docmodel/SemanticDocument.h"

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

/// Per-page OCR output from recognizeDocument().
struct PageOcrResult {
    int                  pageIndex = 0;
    QList<LayoutRegion>  layoutRegions;   // layout detected by LayoutEnsemble
    QList<MergedOcrWord> words;           // ROVER-merged word results
    bool                 success = false;
    QString              errorMessage;
};

/// The OcrPipeline orchestrates preprocessing, multi-engine execution,
/// and word-level confidence-weighted ROVER merging.
class OcrPipeline {
public:
    /// Construct with primary engine.  Secondary is optional.
    explicit OcrPipeline(std::shared_ptr<IOcrEngine> primary,
                         std::shared_ptr<IOcrEngine> secondary = nullptr);
    ~OcrPipeline();

    /// Attach a LayoutEnsemble and LaneScheduler for cross-page document OCR.
    /// Both are non-owning; caller must keep them alive for recognizeDocument's lifetime.
    void setLayoutEnsemble(class LayoutEnsemble *ensemble);
    void setScheduler(gp::LaneScheduler *scheduler);

    /// Set the merging strategy.
    void setStrategy(OcrStrategy strategy);
    OcrStrategy strategy() const;

    /// Enable/disable preprocessing.
    void setPreprocessing(const OcrPreprocessOptions &opts);
    OcrPreprocessOptions preprocessingOptions() const;

    /// Run the full pipeline on a page image.
    /// Returns merged word-level results in original-image coordinates.
    QList<MergedOcrWord> run(const QImage &pageImage);

    /// Cross-page document OCR using LaneScheduler CrossPagePipeline.
    ///
    /// Stage1 (GPU): LayoutEnsemble detects regions for page P+1.
    /// Stage2 (GPU/CPU fanout): per-region ROVER-merged OCR via the existing
    ///   primary+secondary engines (falls back to whole-page OCR if no layout).
    /// Stage3 (CPU): result fusion + OrderedResultQueue ordering.
    ///
    /// `pageImages` must be pre-rendered at the desired DPI (caller's responsibility).
    /// Returns a QFuture that resolves to a PageOcrResult list in page order.
    ///
    /// Requires setLayoutEnsemble() and setScheduler() to be called first.
    /// Falls back to sequential single-page run() if scheduler is null.
    QFuture<QList<PageOcrResult>> recognizeDocument(const QList<QImage> &pageImages);

    /// High-level convenience: run the full pipeline and map the result to a
    /// SemanticDocument via OcrDjotMapper.
    ///
    /// \param pdfPath  Source PDF path — embedded in every BornOCR provenance node.
    /// \param pageImages  Pre-rendered page images (one per page).
    ///
    /// Mapping runs on the LaneScheduler CPU lane (if scheduler is set) or inline
    /// on the calling thread (sequential fallback).
    ///
    /// Back-compat: recognizeDocument() is unchanged.
    QFuture<docmodel::SemanticDocument> recognizeDocumentAsDjot(
        const QString& pdfPath,
        const QList<QImage>& pageImages);

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
