// src/engines/ocr/SuryaDetector.cpp
// SuryaDetector implementation.
//
// When HAS_SURYA is NOT defined (default): all methods return empty / false.
// LayoutEnsemble falls back to single-detector mode.
//
// When HAS_SURYA IS defined: detect() invokes `surya-detect` as a QProcess
// subprocess, passing the page image as a temp file and parsing JSON output.
// This subprocess pattern mirrors veraPDF (AGPL-3.0 accepted as subprocess):
// the Surya weights never link into the binary, so Open Rail-M restrictions
// on the weights do not propagate to the GlyphPDF binary's license.
//
// NOTE: The subprocess approach adds ~200–500 ms per page on modern hardware.
// For production workloads, PpDocLayoutDetector (Apache-2.0 weights) is the
// preferred single detector. Surya is an optional enhancement for use cases
// where accuracy outweighs latency and the Open Rail-M terms are acceptable.

#include "engines/ocr/SuryaDetector.h"

#include <QDebug>

#ifdef HAS_SURYA
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryFile>
#endif

class SuryaDetector::Private {
public:
#ifdef HAS_SURYA
    // Path to the surya-detect Python entry-point or script.
    // Resolved at first call; cached thereafter.
    QString suryaExe;
    bool    resolved = false;

    bool resolveExe() {
        if (resolved) return !suryaExe.isEmpty();
        resolved = true;
        // Check surya-detect in PATH
        for (const auto &candidate : { "surya-detect", "surya_detect" }) {
            QProcess probe;
            probe.start(candidate, {"--help"});
            if (probe.waitForStarted(2000)) {
                probe.kill(); probe.waitForFinished(1000);
                suryaExe = candidate;
                return true;
            }
        }
        // Check python -m surya.detect
        {
            QProcess probe;
            probe.start("python", {"-c", "import surya"});
            if (probe.waitForStarted(2000) && probe.waitForFinished(3000)
                && probe.exitCode() == 0) {
                suryaExe = "__python_module__";
                return true;
            }
        }
        qWarning() << "SuryaDetector: surya-detect not found in PATH; "
                      "enable HAS_SURYA and install surya-ocr (pip install surya-ocr)";
        return false;
    }
#endif
};

SuryaDetector::SuryaDetector() : d(std::make_unique<Private>()) {}
SuryaDetector::~SuryaDetector() = default;

bool SuryaDetector::isAvailable() const
{
#ifdef HAS_SURYA
    return const_cast<Private*>(d.get())->resolveExe();
#else
    return false;
#endif
}

QString SuryaDetector::name() const
{
    return QStringLiteral("Surya");
}

QList<LayoutRegion> SuryaDetector::detect(const QImage &page, gp::Lane /*lane*/)
{
#ifdef HAS_SURYA
    if (page.isNull()) return {};
    if (!d->resolveExe()) return {};

    // Write page to a temp PNG for the subprocess.
    QTemporaryFile tmp;
    tmp.setFileTemplate(QDir::tempPath() + "/gp_surya_XXXXXX.png");
    if (!tmp.open() || !page.save(tmp.fileName(), "PNG")) {
        qWarning() << "SuryaDetector: failed to write temp image";
        return {};
    }

    // Build command line
    QStringList args;
    QString prog;
    if (d->suryaExe == "__python_module__") {
        prog = "python";
        args << "-m" << "surya.scripts.detect_text" << tmp.fileName() << "--json";
    } else {
        prog = d->suryaExe;
        args << tmp.fileName() << "--json";
    }

    QProcess proc;
    proc.start(prog, args);
    if (!proc.waitForFinished(30000)) {
        qWarning() << "SuryaDetector: subprocess timed out";
        proc.kill();
        return {};
    }
    if (proc.exitCode() != 0) {
        qWarning() << "SuryaDetector: subprocess error:" << proc.readAllStandardError();
        return {};
    }

    // Parse JSON output — Surya detect outputs:
    //   [{"bbox": [x1, y1, x2, y2], "text_line_score": 0.97}, ...]
    const QByteArray raw = proc.readAllStandardOutput();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray()) {
        qWarning() << "SuryaDetector: unexpected JSON output";
        return {};
    }

    QList<LayoutRegion> regions;
    const QJsonArray arr = doc.array();
    for (const auto &elem : arr) {
        const QJsonObject obj = elem.toObject();
        const QJsonArray bbox = obj["bbox"].toArray();
        if (bbox.size() < 4) continue;

        LayoutRegion r;
        double x1 = bbox[0].toDouble(), y1 = bbox[1].toDouble();
        double x2 = bbox[2].toDouble(), y2 = bbox[3].toDouble();
        r.bbox       = QRectF(x1, y1, x2 - x1, y2 - y1);
        r.type       = RegionType::Paragraph;   // Surya is a text-line detector; all are text
        r.confidence = obj["text_line_score"].toDouble(0.8);
        if (!r.bbox.isEmpty())
            regions.append(r);
    }

    // Assign reading order (top-to-bottom, left-to-right)
    std::sort(regions.begin(), regions.end(),
              [](const LayoutRegion &a, const LayoutRegion &b) {
                  if (std::abs(a.bbox.top() - b.bbox.top()) > 20.0)
                      return a.bbox.top() < b.bbox.top();
                  return a.bbox.left() < b.bbox.left();
              });
    for (int i = 0; i < regions.size(); ++i)
        regions[i].readingOrderIndex = i;

    return regions;
#else
    // HAS_SURYA not defined — LayoutEnsemble operates in single-detector mode.
    Q_UNUSED(page)
    return {};
#endif
}
