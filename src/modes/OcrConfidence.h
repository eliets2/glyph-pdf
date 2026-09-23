// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QColor>
#include <QString>

namespace gp {

/// U03: THE one OCR confidence classifier. The legend, the per-word overlay
/// colors, the low-confidence count and the uncertain-word navigation all
/// classify through bandFor(), so they can never disagree again (the legend
/// used to claim 80/50 while highlighting used 90/70).
///
/// Thresholds follow the plan's first repair: align every consumer on the
/// overlay's long-standing 90/70 bands. BatchMode::lowConfidenceNote (the
/// batch review rule, parameterized by batch settings) and the OcrPipeline
/// ConfidenceWeighted strategy switch (an engine concern) are deliberately
/// separate consumers with documented different semantics and do NOT read
/// this classifier.
struct OcrConfidence {
    enum class Band { High, Medium, Low };

    static constexpr int kHighThreshold   = 90;  // conf >= 90 → High
    static constexpr int kMediumThreshold = 70;  // 70..89 → Medium; < 70 → Low

    /// Classify one engine confidence estimate. Clamps out-of-range input:
    /// negative → Low, above 100 → High.
    static Band bandFor(int confidence);

    /// The band's overlay color (also the legend swatch color):
    /// green #22c55e / yellow #eab308 / red #ef4444 — the palette the scan
    /// pane has always drawn for the 90/70 bands.
    static QColor bandColor(Band band);

    /// Human-readable range text for legend/tooltip:
    /// "≥ 90%" / "70–89%" / "< 70%" (meaning never rests on color alone).
    static QString bandRangeText(Band band);
};

} // namespace gp
