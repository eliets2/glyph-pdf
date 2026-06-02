// SPDX-License-Identifier: Apache-2.0
#include "engines/ocr/PpOcrDecoder.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QImage>
#include <QtMath>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <vector>

#ifdef HAS_RAPIDOCR
#include <onnxruntime_cxx_api.h>
#endif

// ── PP-OCRv5 preprocessing constants (pinned from the official models; see
//    models/ppocrv5/PROVENANCE.md and the reference run in .context/ref_ppocr.py) ─
namespace {
constexpr int    kRecHeight      = 48;     // rec input is [N,3,48,W]
constexpr int    kClsW           = 160;    // cls input is [N,3,80,160]
constexpr int    kClsH           = 80;
constexpr int    kDetSideLimit   = 960;    // cap det long side, multiple of 32
constexpr double kDetThreshold   = 0.3;    // DBNet binarization threshold
constexpr float  kDetMean[3]     = {0.485f, 0.456f, 0.406f};  // ImageNet
constexpr float  kDetStd[3]      = {0.229f, 0.224f, 0.225f};
constexpr double kClsFlipConf    = 0.6;    // only flip if 180 prob exceeds this
} // namespace

PpOcrDecoder::PpOcrDecoder() = default;
PpOcrDecoder::~PpOcrDecoder() = default;

// ── Dictionary ───────────────────────────────────────────────────────────────

bool PpOcrDecoder::loadDictionary(const QString &dictPath)
{
    QFile f(dictPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "PpOcrDecoder: cannot open dictionary" << dictPath;
        return false;
    }
    m_vocab.clear();
    // index 0 = CTC blank sentinel (never emitted as a glyph)
    m_vocab.append(QStringLiteral("<blank>"));

    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        // PaddleOCR dict lines are exactly one character; do NOT trim spaces away
        // (some dict entries are meaningful whitespace), only strip CR/LF.
        while (line.endsWith(QLatin1Char('\n')) || line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        m_vocab.append(line);
    }
    // PP-OCRv5 CTC appends an implicit space class after the dict (output dim =
    // dict_lines + blank + space). Reproduce it so the last class decodes to ' '.
    m_vocab.append(QStringLiteral(" "));

    if (m_vocab.size() < 3) {
        qWarning() << "PpOcrDecoder: dictionary suspiciously small:" << m_vocab.size();
        return false;
    }
    return true;
}

// ── D2: 3x3 dilation (ONNX-free) ─────────────────────────────────────────────

std::vector<uint8_t> PpOcrDecoder::dilate3x3(const std::vector<uint8_t> &mask, int w, int h)
{
    std::vector<uint8_t> out(mask.size(), 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t v = 0;
            for (int dy = -1; dy <= 1 && !v; ++dy) {
                int yy = y + dy;
                if (yy < 0 || yy >= h) continue;
                for (int dx = -1; dx <= 1; ++dx) {
                    int xx = x + dx;
                    if (xx < 0 || xx >= w) continue;
                    if (mask[static_cast<size_t>(yy) * w + xx]) { v = 1; break; }
                }
            }
            out[static_cast<size_t>(y) * w + x] = v;
        }
    }
    return out;
}

// ── D2: connected components (ONNX-free, 4-connected BFS) ─────────────────────

QList<QRect> PpOcrDecoder::connectedComponentBoxes(const std::vector<uint8_t> &mask,
                                                   int w, int h, int minArea)
{
    QList<QRect> boxes;
    if (w <= 0 || h <= 0 || mask.size() != static_cast<size_t>(w) * h)
        return boxes;

    std::vector<uint8_t> seen(mask.size(), 0);
    const int dx4[4] = {1, -1, 0, 0};
    const int dy4[4] = {0, 0, 1, -1};

    for (int y0 = 0; y0 < h; ++y0) {
        for (int x0 = 0; x0 < w; ++x0) {
            size_t start = static_cast<size_t>(y0) * w + x0;
            if (!mask[start] || seen[start]) continue;

            int minx = x0, maxx = x0, miny = y0, maxy = y0, area = 0;
            std::queue<std::pair<int,int>> q;
            q.push({x0, y0});
            seen[start] = 1;
            while (!q.empty()) {
                auto [cx, cy] = q.front(); q.pop();
                ++area;
                minx = std::min(minx, cx); maxx = std::max(maxx, cx);
                miny = std::min(miny, cy); maxy = std::max(maxy, cy);
                for (int k = 0; k < 4; ++k) {
                    int nx = cx + dx4[k], ny = cy + dy4[k];
                    if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                    size_t ni = static_cast<size_t>(ny) * w + nx;
                    if (mask[ni] && !seen[ni]) { seen[ni] = 1; q.push({nx, ny}); }
                }
            }
            if (area >= minArea)
                boxes.append(QRect(minx, miny, maxx - minx + 1, maxy - miny + 1));
        }
    }
    return boxes;
}

// ── D3: homography solver (ONNX-free) ────────────────────────────────────────
// Solves the 8 unknowns of H (h22 fixed to 1) from 4 point correspondences via
// Gaussian elimination on the 8x8 DLT system. src[i] -> dst[i].

bool PpOcrDecoder::solveHomography(const std::array<QPointF, 4> &src,
                                   const std::array<QPointF, 4> &dst,
                                   std::array<double, 9> &Hout)
{
    double A[8][9];
    for (int i = 0; i < 4; ++i) {
        const double x = src[i].x(), y = src[i].y();
        const double u = dst[i].x(), v = dst[i].y();
        double *r0 = A[2 * i];
        double *r1 = A[2 * i + 1];
        r0[0]=x; r0[1]=y; r0[2]=1; r0[3]=0; r0[4]=0; r0[5]=0; r0[6]=-u*x; r0[7]=-u*y; r0[8]=u;
        r1[0]=0; r1[1]=0; r1[2]=0; r1[3]=x; r1[4]=y; r1[5]=1; r1[6]=-v*x; r1[7]=-v*y; r1[8]=v;
    }
    // Gaussian elimination with partial pivoting on the 8x9 augmented matrix.
    for (int col = 0; col < 8; ++col) {
        int piv = col;
        double best = std::fabs(A[col][col]);
        for (int r = col + 1; r < 8; ++r) {
            double a = std::fabs(A[r][col]);
            if (a > best) { best = a; piv = r; }
        }
        if (best < 1e-12) return false;            // singular
        if (piv != col)
            for (int c = 0; c < 9; ++c) std::swap(A[col][c], A[piv][c]);
        double inv = 1.0 / A[col][col];
        for (int c = col; c < 9; ++c) A[col][c] *= inv;
        for (int r = 0; r < 8; ++r) {
            if (r == col) continue;
            double f = A[r][col];
            if (f == 0.0) continue;
            for (int c = col; c < 9; ++c) A[r][c] -= f * A[col][c];
        }
    }
    for (int i = 0; i < 8; ++i) Hout[i] = A[i][8];
    Hout[8] = 1.0;
    return true;
}

// ── D3: perspective warp with bilinear sampling (ONNX-free) ──────────────────

namespace {
// Order the 4 polygon vertices as TL, TR, BR, BL using sum/diff heuristic.
std::array<QPointF, 4> orderQuad(const QPolygonF &poly)
{
    std::array<QPointF, 4> pts{};
    if (poly.size() < 4) {
        QRectF b = poly.boundingRect();
        pts[0] = b.topLeft();  pts[1] = b.topRight();
        pts[2] = b.bottomRight(); pts[3] = b.bottomLeft();
        return pts;
    }
    // Pick the 4 corners by extremes of x+y and x-y.
    int tl=0, br=0, tr=0, bl=0;
    double sMin= std::numeric_limits<double>::max(), sMax=-sMin;
    double dMin= sMin, dMax=-sMin;
    for (int i = 0; i < poly.size(); ++i) {
        double s = poly[i].x() + poly[i].y();
        double d = poly[i].x() - poly[i].y();
        if (s < sMin) { sMin = s; tl = i; }
        if (s > sMax) { sMax = s; br = i; }
        if (d < dMin) { dMin = d; bl = i; }
        if (d > dMax) { dMax = d; tr = i; }
    }
    pts[0] = poly[tl]; pts[1] = poly[tr]; pts[2] = poly[br]; pts[3] = poly[bl];
    return pts;
}

inline QRgb bilinear(const QImage &img, double sx, double sy)
{
    const int W = img.width(), H = img.height();
    if (sx < 0) sx = 0;
    if (sx > W - 1) sx = W - 1;
    if (sy < 0) sy = 0;
    if (sy > H - 1) sy = H - 1;
    int x0 = static_cast<int>(std::floor(sx));
    int y0 = static_cast<int>(std::floor(sy));
    int x1 = std::min(x0 + 1, W - 1);
    int y1 = std::min(y0 + 1, H - 1);
    double fx = sx - x0, fy = sy - y0;
    QRgb p00 = img.pixel(x0, y0), p10 = img.pixel(x1, y0);
    QRgb p01 = img.pixel(x0, y1), p11 = img.pixel(x1, y1);
    auto lerp = [&](int c00, int c10, int c01, int c11) {
        double top = c00 * (1 - fx) + c10 * fx;
        double bot = c01 * (1 - fx) + c11 * fx;
        return static_cast<int>(std::lround(top * (1 - fy) + bot * fy));
    };
    int r = lerp(qRed(p00), qRed(p10), qRed(p01), qRed(p11));
    int g = lerp(qGreen(p00), qGreen(p10), qGreen(p01), qGreen(p11));
    int b = lerp(qBlue(p00), qBlue(p10), qBlue(p01), qBlue(p11));
    return qRgb(r, g, b);
}
} // namespace

QImage PpOcrDecoder::warpPerspective(const QImage &srcIn, const QPolygonF &quad,
                                     int outW, int outH)
{
    if (outW <= 0 || outH <= 0 || srcIn.isNull()) return {};
    QImage src = srcIn.convertToFormat(QImage::Format_RGB888);

    std::array<QPointF, 4> s = orderQuad(quad);
    std::array<QPointF, 4> d = {
        QPointF(0, 0), QPointF(outW - 1, 0),
        QPointF(outW - 1, outH - 1), QPointF(0, outH - 1)
    };
    // We need dst->src mapping for the inverse warp, so solve H: d -> s.
    std::array<double, 9> H{};
    if (!solveHomography(d, s, H)) {
        // Degenerate quad: fall back to axis-aligned crop+scale.
        QRectF br = quad.boundingRect().toRect();
        QImage crop = src.copy(br.toRect());
        return crop.isNull() ? QImage()
                             : crop.scaled(outW, outH, Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation);
    }

    QImage out(outW, outH, QImage::Format_RGB888);
    for (int y = 0; y < outH; ++y) {
        for (int x = 0; x < outW; ++x) {
            double w = H[6] * x + H[7] * y + H[8];
            if (std::fabs(w) < 1e-12) w = 1e-12;
            double sx = (H[0] * x + H[1] * y + H[2]) / w;
            double sy = (H[3] * x + H[4] * y + H[5]) / w;
            out.setPixel(x, y, bilinear(src, sx, sy));
        }
    }
    return out;
}

// ── D4: CTC greedy decode (ONNX-free) ────────────────────────────────────────

PpOcrDecoder::RecResult PpOcrDecoder::ctcGreedyDecode(const float *probs, int T, int C,
                                                      const QStringList &vocab, int blankIndex)
{
    RecResult res;
    if (!probs || T <= 0 || C <= 0) return res;

    QString text;
    double confSum = 0.0;
    int    confCnt = 0;
    int    prev = -1;

    for (int t = 0; t < T; ++t) {
        const float *row = probs + static_cast<size_t>(t) * C;
        int   best = 0;
        float bestP = row[0];
        for (int c = 1; c < C; ++c) {
            if (row[c] > bestP) { bestP = row[c]; best = c; }
        }
        if (best != blankIndex && best != prev) {
            if (best >= 0 && best < vocab.size())
                text += vocab.at(best);
            confSum += bestP;
            ++confCnt;
        }
        prev = best;
    }
    res.text = text;
    res.confidence = (confCnt > 0) ? confSum / confCnt : 0.0;
    return res;
}

// ═════════════════════════════════════════════════════════════════════════════
//  ONNX-backed methods (require warm sessions)
// ═════════════════════════════════════════════════════════════════════════════

#ifdef HAS_RAPIDOCR

void PpOcrDecoder::setSessions(Ort::Session *det, Ort::Session *cls, Ort::Session *rec)
{
    m_det = det;
    m_cls = cls;
    m_rec = rec;
}

namespace {

// Query the (single) input and output tensor names of a session. PP-OCRv5 ONNX
// exports use non-obvious output names (e.g. "fetch_name_0"), so never hardcode.
struct IoNames {
    std::string inName;
    std::string outName;
};

IoNames ioNames(Ort::Session &s, Ort::AllocatorWithDefaultOptions &alloc)
{
    // AllocatedStringPtr has a non-default-constructible deleter, so copy the
    // names into owning std::strings and let the allocated buffers free here.
    Ort::AllocatedStringPtr in  = s.GetInputNameAllocated(0, alloc);
    Ort::AllocatedStringPtr out = s.GetOutputNameAllocated(0, alloc);
    IoNames n;
    n.inName  = in.get();
    n.outName = out.get();
    return n;
}

// Round up to a multiple of 32, clamped to >= 32.
int roundTo32(int v)
{
    int r = (v / 32) * 32;
    return r < 32 ? 32 : r;
}

} // namespace

// ── D2: DBNet detection ──────────────────────────────────────────────────────

QList<QPolygonF> PpOcrDecoder::detect(const QImage &imageIn) const
{
    QList<QPolygonF> polys;
    if (!m_det || imageIn.isNull()) return polys;

    QImage image = imageIn.convertToFormat(QImage::Format_RGB888);
    const int ow = image.width(), oh = image.height();

    // Resize so the long side <= kDetSideLimit and both sides are multiples of 32.
    double scale = 1.0;
    if (std::max(ow, oh) > kDetSideLimit)
        scale = static_cast<double>(kDetSideLimit) / std::max(ow, oh);
    int rw = roundTo32(static_cast<int>(std::lround(ow * scale)));
    int rh = roundTo32(static_cast<int>(std::lround(oh * scale)));
    QImage resized = image.scaled(rw, rh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                          .convertToFormat(QImage::Format_RGB888);

    // Map factors: detection-map coords -> original coords.
    const double sx = static_cast<double>(ow) / rw;
    const double sy = static_cast<double>(oh) / rh;

    // NCHW float, ImageNet-normalised.
    std::vector<float> input(static_cast<size_t>(3) * rh * rw);
    for (int y = 0; y < rh; ++y) {
        const uchar *line = resized.constScanLine(y);
        for (int x = 0; x < rw; ++x) {
            const uchar *px = line + x * 3;
            for (int c = 0; c < 3; ++c) {
                float v = px[c] / 255.0f;
                v = (v - kDetMean[c]) / kDetStd[c];
                input[(static_cast<size_t>(c) * rh + y) * rw + x] = v;
            }
        }
    }

    try {
        Ort::AllocatorWithDefaultOptions alloc;
        IoNames io = ioNames(*m_det, alloc);
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::array<int64_t, 4> shape = {1, 3, rh, rw};
        Ort::Value tin = Ort::Value::CreateTensor<float>(
            mem, input.data(), input.size(), shape.data(), shape.size());

        const char *inN = io.inName.c_str();
        const char *outN = io.outName.c_str();
        auto outs = m_det->Run(Ort::RunOptions{nullptr}, &inN, &tin, 1, &outN, 1);
        if (outs.empty()) return polys;

        auto info = outs[0].GetTensorTypeAndShapeInfo();
        std::vector<int64_t> os = info.GetShape();   // [1,1,H,W]
        if (os.size() != 4) return polys;
        const int mh = static_cast<int>(os[2]);
        const int mw = static_cast<int>(os[3]);
        const float *prob = outs[0].GetTensorData<float>();

        // Binarize.
        std::vector<uint8_t> mask(static_cast<size_t>(mw) * mh, 0);
        for (size_t i = 0; i < mask.size(); ++i)
            mask[i] = (prob[i] > static_cast<float>(kDetThreshold)) ? 1 : 0;

        mask = dilate3x3(mask, mw, mh);
        QList<QRect> boxes = connectedComponentBoxes(mask, mw, mh, /*minArea*/8);

        // The det map is at the resized resolution; map boxes back to original.
        const double mapX = sx * (static_cast<double>(rw) / mw);
        const double mapY = sy * (static_cast<double>(rh) / mh);

        for (const QRect &b : boxes) {
            // unclip approximation: expand ~30% of box height outward.
            int pad = std::max(2, static_cast<int>(0.30 * b.height()));
            double x0 = (b.left()   - pad) * mapX;
            double y0 = (b.top()    - pad) * mapY;
            double x1 = (b.right()  + 1 + pad) * mapX;
            double y1 = (b.bottom() + 1 + pad) * mapY;
            x0 = std::clamp(x0, 0.0, static_cast<double>(ow));
            y0 = std::clamp(y0, 0.0, static_cast<double>(oh));
            x1 = std::clamp(x1, 0.0, static_cast<double>(ow));
            y1 = std::clamp(y1, 0.0, static_cast<double>(oh));
            if (x1 - x0 < 3 || y1 - y0 < 3) continue;
            QPolygonF q;
            q << QPointF(x0, y0) << QPointF(x1, y0)
              << QPointF(x1, y1) << QPointF(x0, y1);
            polys.append(q);
        }
    } catch (const Ort::Exception &e) {
        qWarning() << "PpOcrDecoder::detect ONNX error:" << e.what();
    }
    return polys;
}

// ── D4: orientation classifier (PP-LCNet textline, 0/180) ────────────────────

int PpOcrDecoder::classifyOrientation(const QImage &cropIn) const
{
    if (!m_cls || cropIn.isNull()) return 0;
    QImage crop = cropIn.convertToFormat(QImage::Format_RGB888)
                        .scaled(kClsW, kClsH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                        .convertToFormat(QImage::Format_RGB888);

    std::vector<float> input(static_cast<size_t>(3) * kClsH * kClsW);
    for (int y = 0; y < kClsH; ++y) {
        const uchar *line = crop.constScanLine(y);
        for (int x = 0; x < kClsW; ++x) {
            const uchar *px = line + x * 3;
            for (int c = 0; c < 3; ++c) {
                float v = px[c] / 255.0f;
                v = (v - 0.5f) / 0.5f;
                input[(static_cast<size_t>(c) * kClsH + y) * kClsW + x] = v;
            }
        }
    }
    try {
        Ort::AllocatorWithDefaultOptions alloc;
        IoNames io = ioNames(*m_cls, alloc);
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::array<int64_t, 4> shape = {1, 3, kClsH, kClsW};
        Ort::Value tin = Ort::Value::CreateTensor<float>(
            mem, input.data(), input.size(), shape.data(), shape.size());
        const char *inN = io.inName.c_str(), *outN = io.outName.c_str();
        auto outs = m_cls->Run(Ort::RunOptions{nullptr}, &inN, &tin, 1, &outN, 1);
        if (outs.empty()) return 0;
        const float *p = outs[0].GetTensorData<float>();   // [1,2] -> {0deg, 180deg}
        // p[0] = prob(0deg), p[1] = prob(180deg). Flip only if confidently 180.
        if (p[1] > p[0] && p[1] > static_cast<float>(kClsFlipConf))
            return 180;
    } catch (const Ort::Exception &e) {
        qWarning() << "PpOcrDecoder::classifyOrientation ONNX error:" << e.what();
    }
    return 0;
}

// ── D4: SVTR recognition + CTC ───────────────────────────────────────────────

PpOcrDecoder::RecResult PpOcrDecoder::recognizeCrop(const QImage &cropIn) const
{
    RecResult res;
    if (!m_rec || cropIn.isNull()) return res;

    // Resize to fixed height 48, width preserves aspect ratio.
    QImage crop = cropIn.convertToFormat(QImage::Format_RGB888);
    double ar = static_cast<double>(crop.width()) / std::max(1, crop.height());
    int rw = std::max(1, static_cast<int>(std::ceil(kRecHeight * ar)));
    QImage r = crop.scaled(rw, kRecHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                   .convertToFormat(QImage::Format_RGB888);

    std::vector<float> input(static_cast<size_t>(3) * kRecHeight * rw);
    for (int y = 0; y < kRecHeight; ++y) {
        const uchar *line = r.constScanLine(y);
        for (int x = 0; x < rw; ++x) {
            const uchar *px = line + x * 3;
            for (int c = 0; c < 3; ++c) {
                float v = px[c] / 255.0f;
                v = (v - 0.5f) / 0.5f;
                input[(static_cast<size_t>(c) * kRecHeight + y) * rw + x] = v;
            }
        }
    }
    try {
        Ort::AllocatorWithDefaultOptions alloc;
        IoNames io = ioNames(*m_rec, alloc);
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::array<int64_t, 4> shape = {1, 3, kRecHeight, rw};
        Ort::Value tin = Ort::Value::CreateTensor<float>(
            mem, input.data(), input.size(), shape.data(), shape.size());
        const char *inN = io.inName.c_str(), *outN = io.outName.c_str();
        auto outs = m_rec->Run(Ort::RunOptions{nullptr}, &inN, &tin, 1, &outN, 1);
        if (outs.empty()) return res;

        auto info = outs[0].GetTensorTypeAndShapeInfo();
        std::vector<int64_t> os = info.GetShape();    // [1,T,C]
        if (os.size() != 3) return res;
        const int T = static_cast<int>(os[1]);
        const int C = static_cast<int>(os[2]);
        const float *probs = outs[0].GetTensorData<float>();   // already softmaxed in-graph
        res = ctcGreedyDecode(probs, T, C, m_vocab, /*blankIndex*/0);
    } catch (const Ort::Exception &e) {
        qWarning() << "PpOcrDecoder::recognizeCrop ONNX error:" << e.what();
    }
    return res;
}

// ── D4: full orchestrated pipeline ───────────────────────────────────────────

QList<OcrResult> PpOcrDecoder::run(const QImage &image) const
{
    QList<OcrResult> results;
    if (image.isNull()) return results;

    QList<QPolygonF> regions = detect(image);

    // If detection found nothing (e.g. a single tight text-line image), fall
    // back to recognising the whole image as one line.
    if (regions.isEmpty()) {
        RecResult r = recognizeCrop(image);
        if (!r.text.isEmpty()) {
            OcrResult o;
            o.text = r.text;
            o.boundingBox = QRectF(0, 0, image.width(), image.height());
            o.confidence = static_cast<int>(std::lround(r.confidence * 100.0));
            results.append(o);
        }
        return results;
    }

    for (const QPolygonF &poly : regions) {
        QRectF br = poly.boundingRect();
        if (br.width() < 3 || br.height() < 3) continue;

        // Warp to a normalized line image at rec height; width by aspect ratio.
        double ar = br.width() / std::max(1.0, br.height());
        int outW = std::max(kRecHeight, static_cast<int>(std::lround(kRecHeight * ar)));
        QImage lineImg = warpPerspective(image, poly, outW, kRecHeight);
        if (lineImg.isNull()) continue;

        // Orientation correction (0 or 180).
        if (classifyOrientation(lineImg) == 180) {
            lineImg = lineImg.mirrored(true, true);   // 180deg rotation
        }

        RecResult r = recognizeCrop(lineImg);
        if (r.text.trimmed().isEmpty()) continue;

        OcrResult o;
        o.text = r.text;
        o.boundingBox = br;
        o.confidence = static_cast<int>(std::lround(r.confidence * 100.0));
        results.append(o);
    }
    return results;
}

#endif // HAS_RAPIDOCR

