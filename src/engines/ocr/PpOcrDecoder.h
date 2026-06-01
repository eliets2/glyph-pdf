#pragma once

// PpOcrDecoder — PP-OCRv5 pre/post-processing for the RapidOcrEngine ONNX pipeline.
//
// This class owns NO ONNX sessions; it borrows the det/cls/rec Ort::Session*
// constructed and warm-kept by RapidOcrEngine, and implements the tensor
// pre/post-processing that PaddlePaddle's PP-OCRv5 mobile models require:
//
//   DBNet detection  → connected-component polygons (D2)
//   perspective warp → axis-aligned 48xW rec crop      (D3)
//   PP-LCNet cls     → 0/180 orientation               (D4)
//   SVTR  recognition→ CTC greedy decode               (D4)
//
// The geometry + CTC helpers (solveHomography, bilinearSample, connectedComponents,
// ctcGreedyDecode) are deliberately ONNX-free and `static`, so D2/D3 unit tests can
// exercise them without loading a model. No OpenCV, no PaddlePaddle runtime.

#include <QImage>
#include <QList>
#include <QPolygonF>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <array>
#include <vector>

#include "core/OcrTypes.h"

#ifdef HAS_RAPIDOCR
namespace Ort { struct Session; }
#endif

class PpOcrDecoder
{
public:
    // Decoded recognition output for one crop.
    struct RecResult {
        QString text;
        double  confidence = 0.0;   // mean per-timestep max probability, 0..1
    };

    PpOcrDecoder();
    ~PpOcrDecoder();

#ifdef HAS_RAPIDOCR
    // Borrow warm sessions (non-owning). cls may be null (orientation skipped).
    void setSessions(Ort::Session *det, Ort::Session *cls, Ort::Session *rec);
#endif

    // Load the recognition vocabulary file (one char per line, UTF-8).
    // The CTC blank is index 0; dict line i maps to class index i+1; a trailing
    // space class is appended (matches PP-OCRv5 rec output dim = lines + 2).
    bool loadDictionary(const QString &dictPath);
    int  vocabularySize() const { return static_cast<int>(m_vocab.size()); }

    // ── D2: DBNet detection ────────────────────────────────────────────────
    // Run det session on `image`, threshold the probability map, dilate, extract
    // connected components, and return one axis-aligned polygon per text region
    // in ORIGINAL image coordinates. Empty if det session unset or no text.
    QList<QPolygonF> detect(const QImage &image) const;

    // ── D4: orchestrated full pipeline ─────────────────────────────────────
    // detect → warp → cls → rec for every region. Results in original coords.
    QList<OcrResult> run(const QImage &image) const;

    // Recognise a single, already-cropped text-line image (no detection).
    // Used by the whole-image fallback and by unit tests.
    RecResult recognizeCrop(const QImage &crop) const;

    // Orientation of a crop: returns 0 or 180 (degrees). 0 if cls unset.
    int classifyOrientation(const QImage &crop) const;

    // ── D2 helper (ONNX-free, static, testable) ────────────────────────────
    // 4-connected labelling of a boolean mask (row-major, w*h). Returns, for each
    // component with >= minArea pixels, its inclusive pixel bounding box.
    static QList<QRect> connectedComponentBoxes(const std::vector<uint8_t> &mask,
                                                int w, int h, int minArea = 6);

    // 3x3 binary dilation of a mask in place-returning form (ONNX-free, static).
    static std::vector<uint8_t> dilate3x3(const std::vector<uint8_t> &mask, int w, int h);

    // ── D3 helpers (ONNX-free, static, testable) ───────────────────────────
    // Solve the 3x3 homography H mapping the 4 src points to the 4 dst points
    // (src[i] -> dst[i]), returned row-major (9 doubles). Returns false if singular.
    static bool solveHomography(const std::array<QPointF, 4> &src,
                                const std::array<QPointF, 4> &dst,
                                std::array<double, 9> &Hout);

    // Warp `src` so that the 4 corners of `quad` (TL,TR,BR,BL order is computed
    // internally) map onto the (outW x outH) destination rect, bilinear-sampled.
    static QImage warpPerspective(const QImage &src, const QPolygonF &quad,
                                  int outW, int outH);

    // ── D4 helper (ONNX-free, static, testable) ────────────────────────────
    // CTC greedy decode. `probs` is row-major [T x C] already-softmaxed values,
    // blankIndex defaults to 0. `vocab[c]` is the string for class c. Returns the
    // collapsed text and mean confidence over emitted (non-blank, non-repeat) steps.
    static RecResult ctcGreedyDecode(const float *probs, int T, int C,
                                     const QStringList &vocab, int blankIndex = 0);

private:
    QStringList m_vocab;   // index 0 = blank sentinel, 1..N = chars, N+1 = space

#ifdef HAS_RAPIDOCR
    Ort::Session *m_det = nullptr;   // non-owning
    Ort::Session *m_cls = nullptr;   // non-owning
    Ort::Session *m_rec = nullptr;   // non-owning
#endif
};
