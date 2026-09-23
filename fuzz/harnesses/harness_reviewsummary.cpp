// SPDX-License-Identifier: Apache-2.0
// harness_reviewsummary.cpp — W1 sweep S4: the printable summary renderer,
// driven through the REAL annotation-extraction chain.
//
// Surfaces under test:
//   PoDoFoBackend::extractAnnotations(path)   (src/engines/podofo/PoDoFoBackend.cpp)
//   ReviewSummaryWriter::renderEntries(items) (src/engines/ReviewSummaryWriter.cpp)
//   ReviewSummaryWriter::writePrintable(...)  — PoDoFo render + SafeSave commit
//   ReviewSummaryWriter::proofSummaryLine(...)— redaction-proof pack intake
//
// Adversarial input classes (fixture corpus + byte mutations): empty/absent/
// mismatched /Rect, /Contents as hex-string vs literal, huge author strings,
// nested /Popup cycles, /Measure with extreme doubles, odd /InkList and
// /Vertices arrays, /GlyphSigMode weirdness.
//
// Per-input flow: bytes → scratch/rs-in.pdf → extractAnnotations →
// renderEntries → writePrintable to scratch/rs-out.pdf with a proof pack
// (the same bytes when they parse as JSON, else a fixed valid pack).
// All writes stay in the driver's own scratch dir (SWEEP_SCRATCH; TMP/TEMP
// are also pointed there by the runner so SafeSave candidates land in it).
#include <cstdio>
#include <string>

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QString>

#include "engines/podofo/PoDoFoBackend.h"
#include "engines/ReviewSummaryWriter.h"
#include "sweep_common.h"

// PoDoFoBackend / AnnotationItem / ReviewSummaryWriter live at global scope
// (src/engines/ReviewSummaryWriter.h, src/core/AnnotationTypes.h carry no
// namespace).
namespace {

std::string scratchPath() {
    const char* env = getenv("SWEEP_SCRATCH");
    return env ? env : "sweep-scratch";
}

const char* runOne(const std::vector<uint8_t>& data) {
    const std::string in = scratchPath() + "\\rs-in.pdf";
    const std::string out = scratchPath() + "\\rs-out.pdf";
    const std::string pack = scratchPath() + "\\rs-proofpack.json";
    QFile::remove(QString::fromStdString(out));
    if (!sweep::writeFileBytes(in, data)) return "ERR scratch-unwritable";

    // ── the real chain: extract → render → write printable ──────────────
    PoDoFoBackend backend;
    QList<AnnotationItem> items;
    try {
        items = backend.extractAnnotations(QString::fromStdString(in));
    } catch (const std::exception& e) {
        static std::string v;
        v = std::string("FINDING S4_EXTRACT_THREW: ") + e.what();
        return v.c_str();
    }

    // Extraction invariants: pageIndex always sane; points finite.
    for (const AnnotationItem& it : items) {
        if (it.pageIndex < 0)
            return "FINDING S4_NEGATIVE_PAGE_INDEX";
        for (const QPointF& p : it.points) {
            if (!qIsFinite(p.x()) || !qIsFinite(p.y()))
                return "FINDING S4_NONFINITE_GEOMETRY";
        }
        if (!qIsFinite(it.rect.width()) || !qIsFinite(it.rect.height()))
            return "FINDING S4_NONFINITE_RECT";
    }

    // renderEntries is pure — group/sort/wrap over hostile strings.
    const QList<ReviewSummaryWriter::RenderedEntry> entries =
        ReviewSummaryWriter::renderEntries(items);
    if (entries.size() != items.size())
        return "FINDING S4_RENDERED_ENTRY_COUNT_MISMATCH";

    // Proof-pack intake: use the input bytes when they happen to be JSON,
    // else a fixed valid pack (exercises both branches of proofSummaryLine).
    const QByteArray raw(reinterpret_cast<const char*>(data.data()),
                         (qsizetype)data.size());
    const QJsonDocument probe = QJsonDocument::fromJson(raw);
    if (probe.isObject()) {
        sweep::writeFileBytes(pack, data);
    } else {
        const QByteArray fixed =
            "{\"verdict\":\"clean\",\"generated_at_utc\":\"2026-09-20T00:00:00Z\","
            "\"excisions\":[]}";
        sweep::writeFileBytes(
            pack, std::vector<uint8_t>(fixed.cbegin(), fixed.cend()));
    }

    ReviewSummaryWriter::PrintOptions opts;
    opts.proofPackPath = QString::fromStdString(pack);
    QString err;
    const bool wrote = ReviewSummaryWriter::writePrintable(
        QString::fromStdString(out), QStringLiteral("Fuzz Subject — \xF0\x9F\x98\x80"),
        items, opts, &err);
    if (wrote) {
        const QFileInfo fi(QString::fromStdString(out));
        if (!fi.exists() || fi.size() <= 0)
            return "FINDING S4_WRITE_OK_BUT_NO_ARTIFACT";
    }
    // ok:false with an error message is an honest refusal — fine.
    return "OK";
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    return sweep::driverMain(argc, argv, runOne);
}
