// src/engines/ocr/SuryaDetector.h
// SuryaDetector — Layout detector backed by VikParuchuri/surya (M5-P2 D3).
//
// ── License Decision ────────────────────────────────────────────────────────
// Surya CODE is Apache-2.0 (compatible with GlyphPDF).
// Surya MODEL WEIGHTS use the modified AI Pubs Open Rail-M license:
//   "free for research, personal use, and startups under $5M funding/revenue."
//   Commercial use above that threshold requires a separate agreement.
//   Redistribution of the weights with a binary is NOT covered under Apache-2.0.
//
// Decision: GlyphPDF ships under Apache-2.0 and targets broad commercial use.
// Bundling or redistributing Surya weights would impose Open Rail-M restrictions
// on downstream users, violating the project's licensing commitment.
//
// We ship SuryaDetector as a clearly-named stub gated by HAS_SURYA.
// To enable Surya at build time (research / personal builds only):
//   - Install Surya Python package (pip install surya-ocr)
//   - Define HAS_SURYA=1 in CMake
//   - The subprocess implementation will call surya-detect via QProcess
//
// LayoutEnsemble::setDetectors() runs in single-detector mode (PpDocLayout only)
// when Surya is not enabled — see the single-detector-mode contract in D4.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once

#include "engines/ocr/ILayoutDetector.h"
#include <memory>

class SuryaDetector final : public ILayoutDetector {
public:
    SuryaDetector();
    ~SuryaDetector() override;

    // ILayoutDetector interface
    QList<LayoutRegion> detect(const QImage &page,
                               gp::Lane lane = gp::Lane::GPU) override;
    QString name() const override;

    /// Returns true when HAS_SURYA is defined and the subprocess is available.
    bool isAvailable() const;

private:
    class Private;
    std::unique_ptr<Private> d;
};
