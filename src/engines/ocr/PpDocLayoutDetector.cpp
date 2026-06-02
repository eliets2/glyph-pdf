// SPDX-License-Identifier: Apache-2.0
// src/engines/ocr/PpDocLayoutDetector.cpp
// PP-DocLayoutV2 ONNX implementation.
//
// Model I/O (from inference.yml + PROVENANCE.md):
//   Input:  "image"        [1, 3, 800, 800]  float32 (resize + CHW; mean 0, std 1)
//           "scale_factor" [1, 2]             float32 (scale_h, scale_w used by model)
//   Output: RT-DETR style — varies by runtime; we probe output names at init.
//           Common: pred_logits [1, N, 25], pred_boxes [1, N, 4] (cx,cy,w,h, normalized)
//           OR merged output named "boxes" / "scores".
//
// Post-processing:
//   1. Apply sigmoid to logits → class probabilities.
//   2. Filter by max-class confidence > kConfThreshold.
//   3. Convert cx,cy,w,h (normalized [0,1]) → pixel QRectF in original-image coords.
//   4. Map PaddlePaddle class index → RegionType.

#include "engines/ocr/PpDocLayoutDetector.h"

#include <QDebug>
#include <QFile>
#include <QImage>
#include <QMutexLocker>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#ifdef HAS_RAPIDOCR
#include <onnxruntime_cxx_api.h>
#endif

// ── constants ───────────────────────────────────────────────────────────────

namespace {

constexpr int    kModelW         = 800;
constexpr int    kModelH         = 800;
constexpr double kConfThreshold  = 0.5;  // minimum per-class sigmoid confidence
constexpr int    kNumClasses     = 25;

// PP-DocLayoutV2 class index → RegionType mapping.
// Source: PROVENANCE.md class list (0-indexed order from inference.yml labels).
RegionType classToRegionType(int classIdx)
{
    switch (classIdx) {
    case 0:  return RegionType::Paragraph;  // abstract
    case 1:  return RegionType::Other;      // algorithm
    case 2:  return RegionType::Paragraph;  // aside_text
    case 3:  return RegionType::Figure;     // chart
    case 4:  return RegionType::Paragraph;  // content
    case 5:  return RegionType::Equation;   // display_formula
    case 6:  return RegionType::Title;      // doc_title
    case 7:  return RegionType::Caption;    // figure_title
    case 8:  return RegionType::Footer;     // footer
    case 9:  return RegionType::Figure;     // footer_image
    case 10: return RegionType::Paragraph;  // footnote
    case 11: return RegionType::Equation;   // formula_number
    case 12: return RegionType::Header;     // header
    case 13: return RegionType::Figure;     // header_image
    case 14: return RegionType::Figure;     // image
    case 15: return RegionType::Equation;   // inline_formula
    case 16: return RegionType::Other;      // number
    case 17: return RegionType::Title;      // paragraph_title
    case 18: return RegionType::Reference;  // reference
    case 19: return RegionType::Reference;  // reference_content
    case 20: return RegionType::Other;      // seal
    case 21: return RegionType::Table;      // table
    case 22: return RegionType::Paragraph;  // text
    case 23: return RegionType::Paragraph;  // vertical_text
    case 24: return RegionType::Other;      // vision_footnote
    default: return RegionType::Other;
    }
}

// Sigmoid helper
inline float sigmoid(float x)
{
    return 1.0f / (1.0f + std::exp(-x));
}

} // namespace

// ── Private ─────────────────────────────────────────────────────────────────

class PpDocLayoutDetector::Private {
public:
#ifdef HAS_RAPIDOCR
    std::unique_ptr<Ort::Env>            env;
    std::unique_ptr<Ort::SessionOptions> sessionOptions;
    std::unique_ptr<Ort::Session>        session;

    // Output tensor names probed at initialize().
    std::vector<std::string>  outputNameStrs;
    std::vector<const char *> outputNames;
    bool hasLogitsBoxes = false;   // true → pred_logits + pred_boxes layout
    bool hasFused       = false;   // true → single fused output (boxes+scores+labels)
#endif

    bool initialized = false;
    QString loadedPath;
    QRecursiveMutex mutex;
};

// ── Construction ─────────────────────────────────────────────────────────────

PpDocLayoutDetector::PpDocLayoutDetector()
    : d(std::make_unique<Private>())
{}

PpDocLayoutDetector::~PpDocLayoutDetector() = default;

bool PpDocLayoutDetector::isInitialized() const
{
    return d->initialized;
}

// ── initialize ───────────────────────────────────────────────────────────────

bool PpDocLayoutDetector::initialize(const QString &modelPath)
{
    QMutexLocker lock(&d->mutex);
    if (d->initialized && d->loadedPath == modelPath)
        return true;

    if (modelPath.isEmpty() || !QFile::exists(modelPath)) {
        qWarning() << "PpDocLayoutDetector: model not found at" << modelPath;
        return false;
    }

#ifdef HAS_RAPIDOCR
    try {
        d->env = std::make_unique<Ort::Env>(
            ORT_LOGGING_LEVEL_WARNING, "PpDocLayoutDetector");

        d->sessionOptions = std::make_unique<Ort::SessionOptions>();
        d->sessionOptions->SetIntraOpNumThreads(2);
        d->sessionOptions->SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef _WIN32
        d->session = std::make_unique<Ort::Session>(
            *d->env,
            modelPath.toStdWString().c_str(),
            *d->sessionOptions);
#else
        d->session = std::make_unique<Ort::Session>(
            *d->env,
            modelPath.toUtf8().constData(),
            *d->sessionOptions);
#endif

        // Probe output tensor names so we can handle different RT-DETR
        // export conventions.
        Ort::AllocatorWithDefaultOptions allocator;
        const size_t nOut = d->session->GetOutputCount();
        d->outputNameStrs.clear();
        d->outputNames.clear();
        for (size_t i = 0; i < nOut; ++i) {
            auto name = d->session->GetOutputNameAllocated(i, allocator);
            d->outputNameStrs.push_back(name.get());
        }
        for (auto &s : d->outputNameStrs)
            d->outputNames.push_back(s.c_str());

        // Detect layout variant
        d->hasLogitsBoxes = false;
        d->hasFused       = false;
        for (const auto &n : d->outputNameStrs) {
            if (n == "pred_logits")  d->hasLogitsBoxes = true;
            if (n == "boxes")        d->hasFused       = true;
        }
        if (!d->hasLogitsBoxes && !d->hasFused) {
            // Unknown variant — just use index 0 as logits, 1 as boxes if available
            d->hasLogitsBoxes = (nOut >= 2);
        }

        d->initialized = true;
        d->loadedPath  = modelPath;
        qDebug() << "PpDocLayoutDetector: loaded" << modelPath
                 << "| outputs:" << nOut
                 << "| variant:" << (d->hasLogitsBoxes ? "logits+boxes" : "fused");
        return true;

    } catch (const Ort::Exception &e) {
        qWarning() << "PpDocLayoutDetector: ORT exception during init:" << e.what();
        return false;
    }
#else
    Q_UNUSED(modelPath)
    qWarning() << "PpDocLayoutDetector: built without HAS_RAPIDOCR; ONNX unavailable";
    return false;
#endif
}

// ── detect ───────────────────────────────────────────────────────────────────

QList<LayoutRegion> PpDocLayoutDetector::detect(const QImage &page, gp::Lane /*lane*/)
{
    if (page.isNull() || !d->initialized)
        return {};

    // The `lane` parameter hints LaneScheduler routing for the caller (LayoutEnsemble).
    // Within detect() we execute synchronously on whatever thread LaneScheduler
    // assigns us — we don't sub-submit to another lane here.

#ifdef HAS_RAPIDOCR
    QMutexLocker lock(&d->mutex);

    try {
        // ── 1. Resize page to [kModelW × kModelH] ───────────────────────────
        const int origW = page.width();
        const int origH = page.height();
        QImage resized = page.scaled(kModelW, kModelH, Qt::IgnoreAspectRatio,
                                     Qt::SmoothTransformation)
                              .convertToFormat(QImage::Format_RGB888);

        const float scaleW = static_cast<float>(origW) / kModelW;
        const float scaleH = static_cast<float>(origH) / kModelH;

        // ── 2. Build CHW float32 tensor (no mean/std per inference.yml) ──────
        std::vector<float> imageData(1 * 3 * kModelH * kModelW);
        for (int y = 0; y < kModelH; ++y) {
            const uchar *row = resized.constScanLine(y);
            for (int x = 0; x < kModelW; ++x) {
                const int px  = y * kModelW + x;
                imageData[0 * kModelH * kModelW + px] = row[x * 3 + 0] / 255.0f; // R
                imageData[1 * kModelH * kModelW + px] = row[x * 3 + 1] / 255.0f; // G
                imageData[2 * kModelH * kModelW + px] = row[x * 3 + 2] / 255.0f; // B
            }
        }

        // scale_factor tensor [1, 2] = { scaleH, scaleW }
        std::vector<float> scaleFactor = { scaleH, scaleW };

        // ── 3. Build ORT tensors ─────────────────────────────────────────────
        Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);

        std::array<int64_t, 4> imgShape  = { 1, 3, kModelH, kModelW };
        std::array<int64_t, 2> sfShape   = { 1, 2 };

        Ort::Value imageTensor = Ort::Value::CreateTensor<float>(
            memInfo,
            imageData.data(), imageData.size(),
            imgShape.data(), imgShape.size());

        Ort::Value sfTensor = Ort::Value::CreateTensor<float>(
            memInfo,
            scaleFactor.data(), scaleFactor.size(),
            sfShape.data(), sfShape.size());

        // ── 4. Run inference ─────────────────────────────────────────────────
        // Input names from the model: "image" and "scale_factor"
        static const char *kInputNames[]  = { "image", "scale_factor" };
        std::vector<Ort::Value> inputs;
        inputs.push_back(std::move(imageTensor));
        inputs.push_back(std::move(sfTensor));

        auto outputs = d->session->Run(
            Ort::RunOptions{nullptr},
            kInputNames, inputs.data(), inputs.size(),
            d->outputNames.data(), d->outputNames.size());

        // ── 5. Post-process: extract regions ────────────────────────────────
        QList<LayoutRegion> regions;

        if (d->hasLogitsBoxes && outputs.size() >= 2) {
            // Find the logits and boxes tensors by name
            int logitsIdx = -1, boxesIdx = -1;
            for (int i = 0; i < (int)d->outputNameStrs.size(); ++i) {
                if (d->outputNameStrs[i] == "pred_logits") logitsIdx = i;
                if (d->outputNameStrs[i] == "pred_boxes")  boxesIdx  = i;
            }
            if (logitsIdx < 0) logitsIdx = 0;
            if (boxesIdx  < 0) boxesIdx  = 1;

            // Use non-const references — GetTensorMutableData is non-const in ORT 1.17
            Ort::Value &logitsTensor = outputs[logitsIdx];
            Ort::Value &boxesTensor  = outputs[boxesIdx];

            auto logitsShape = logitsTensor.GetTensorTypeAndShapeInfo().GetShape();
            // logitsShape: [1, numQueries, kNumClasses]
            // boxesShape:  [1, numQueries, 4]
            const int numQueries = (int)logitsShape[1];
            const float *logitsData = logitsTensor.GetTensorMutableData<float>();
            const float *boxesData  = boxesTensor.GetTensorMutableData<float>();

            for (int q = 0; q < numQueries; ++q) {
                // Find best class
                const float *row = logitsData + q * kNumClasses;
                int bestClass  = 0;
                float bestConf = sigmoid(row[0]);
                for (int c = 1; c < kNumClasses; ++c) {
                    float conf = sigmoid(row[c]);
                    if (conf > bestConf) { bestConf = conf; bestClass = c; }
                }

                if (bestConf < kConfThreshold)
                    continue;

                // cx, cy, w, h normalized [0,1] → pixel coords in original image
                const float *b  = boxesData + q * 4;
                float cx = b[0], cy = b[1], bw = b[2], bh = b[3];
                float x1 = (cx - bw * 0.5f) * origW;
                float y1 = (cy - bh * 0.5f) * origH;
                float x2 = (cx + bw * 0.5f) * origW;
                float y2 = (cy + bh * 0.5f) * origH;

                LayoutRegion r;
                r.bbox       = QRectF(x1, y1, x2 - x1, y2 - y1)
                                   .intersected(QRectF(0, 0, origW, origH));
                r.type       = classToRegionType(bestClass);
                r.confidence = bestConf;
                if (!r.bbox.isEmpty())
                    regions.append(r);
            }
        } else if (!outputs.empty()) {
            // Fused output: try shape [1, N, 6] where each row is
            // [x1, y1, x2, y2, score, classIdx] in pixel coords (post scale_factor)
            Ort::Value &fused = outputs[0];
            auto shape        = fused.GetTensorTypeAndShapeInfo().GetShape();
            if (shape.size() == 3 && shape[2] >= 6) {
                const int N = (int)shape[1];
                const float *data = fused.GetTensorMutableData<float>();
                for (int i = 0; i < N; ++i) {
                    const float *row  = data + i * shape[2];
                    float x1   = row[0], y1  = row[1], x2  = row[2], y2  = row[3];
                    float score = row[4];
                    int   cls   = (int)row[5];
                    if (score < kConfThreshold) continue;
                    LayoutRegion r;
                    r.bbox       = QRectF(x1, y1, x2 - x1, y2 - y1)
                                       .intersected(QRectF(0, 0, origW, origH));
                    r.type       = classToRegionType(cls);
                    r.confidence = score;
                    if (!r.bbox.isEmpty())
                        regions.append(r);
                }
            }
        }

        // ── 6. Assign preliminary reading order (top-to-bottom, left-to-right) ─
        // LayoutEnsemble will re-compute this after merging; this is a fallback.
        std::sort(regions.begin(), regions.end(),
                  [](const LayoutRegion &a, const LayoutRegion &b) {
                      if (std::abs(a.bbox.top() - b.bbox.top()) > 20.0)
                          return a.bbox.top() < b.bbox.top();
                      return a.bbox.left() < b.bbox.left();
                  });
        for (int i = 0; i < regions.size(); ++i)
            regions[i].readingOrderIndex = i;

        return regions;

    } catch (const Ort::Exception &e) {
        qWarning() << "PpDocLayoutDetector::detect ORT exception:" << e.what();
        return {};
    }
#else
    Q_UNUSED(page)
    return {};
#endif
}
