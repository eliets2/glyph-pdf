// SPDX-License-Identifier: Apache-2.0
#include "OcrConfidence.h"

namespace gp {

OcrConfidence::Band OcrConfidence::bandFor(int confidence)
{
    // THE one classification rule (U03): the legend, the per-word overlay
    // colors, the low-confidence count and the uncertain-word navigation all
    // classify here. bandFor clamps out-of-range estimates: negative values
    // fall through to Low, anything above 100 to High.
    if (confidence >= kHighThreshold)   return Band::High;   // >= 90
    if (confidence >= kMediumThreshold) return Band::Medium; // 70..89
    return Band::Low;                                        // < 70
}

QColor OcrConfidence::bandColor(Band band)
{
    // The palette the scan pane overlay has always drawn for the 90/70 bands
    // (M5-P2 D6 spec); the legend swatches now show the same colors.
    switch (band) {
    case Band::High:   return QColor(QStringLiteral("#22c55e"));
    case Band::Medium: return QColor(QStringLiteral("#eab308"));
    case Band::Low:    return QColor(QStringLiteral("#ef4444"));
    }
    return QColor(QStringLiteral("#ef4444"));
}

QString OcrConfidence::bandRangeText(Band band)
{
    // Meaning must not depend on red/green alone — the legend rows carry the
    // exact range each band covers.
    switch (band) {
    case Band::High:   return QStringLiteral("≥ 90%");
    case Band::Medium: return QStringLiteral("70–89%");
    case Band::Low:    return QStringLiteral("< 70%");
    }
    return QStringLiteral("< 70%");
}

} // namespace gp
