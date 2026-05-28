#include "GlyphAdvanceCalculator.h"
#include <podofo/podofo.h>

PoDoFo::PdfTextState GlyphAdvanceCalculator::makePdfTextState(
        const PoDoFo::PdfFont& font, const TextState& state)
{
    PoDoFo::PdfTextState ps;
    ps.Font        = &font;
    ps.FontSize    = state.fontSize;
    ps.FontScale   = state.fontScale;
    ps.CharSpacing = state.charSpacing;
    ps.WordSpacing = state.wordSpacing;
    return ps;
}

double GlyphAdvanceCalculator::sumAdvances(
        const PoDoFo::PdfFont& font,
        const PoDoFo::PdfString& encodedStr,
        const TextState& state)
{
    auto ps = makePdfTextState(font, state);

    // Path 1: PoDoFo's encoded-string length (handles Simple + CID Identity-H).
    double len = 0.0;
    try {
        if (font.TryGetEncodedStringLength(encodedStr, ps, len)) {
            return len;
        }
    } catch (...) {}

    // Path 2: decoded UTF-8 string fallback (simple fonts after IsStringEvaluated).
    if (encodedStr.IsStringEvaluated()) {
        try {
            return font.GetStringLength(encodedStr.GetString(), ps);
        } catch (...) {}
    }

    // Path 3: CID Type0/CIDTrueType per-glyph fallback for Identity-H (2-byte CID codes).
    auto fontType = font.GetType();
    if (fontType == PoDoFo::PdfFontType::CIDCFF || fontType == PoDoFo::PdfFontType::CIDTrueType) {
        auto rawData = encodedStr.GetRawData();
        if (rawData.size() >= 2 && (rawData.size() % 2) == 0) {
            const auto& metrics = font.GetMetrics();
            double total = 0.0;
            for (size_t i = 0; i + 1 < rawData.size(); i += 2) {
                auto hi = static_cast<unsigned char>(rawData[i]);
                auto lo = static_cast<unsigned char>(rawData[i + 1]);
                unsigned gid = (static_cast<unsigned>(hi) << 8) | lo;
                double glyphWidth = 0.0;
                if (metrics.TryGetGlyphWidth(gid, glyphWidth)) {
                    // glyphWidth is in glyph units (0-1000 scale); scale to text space.
                    total += glyphWidth * state.fontSize * state.fontScale / 1000.0;
                }
                total += state.charSpacing * state.fontScale;
                if (gid == 0x0020) {
                    total += state.wordSpacing * state.fontScale;
                }
            }
            return total;
        }
    }

    throw std::runtime_error("GlyphAdvanceCalculator: unsupported font encoding — cannot compute glyph advances");
}

double GlyphAdvanceCalculator::sumAdvances(
        const PoDoFo::PdfFont& font,
        std::string_view utf8Text,
        const TextState& state)
{
    auto ps = makePdfTextState(font, state);
    try {
        return font.GetStringLength(utf8Text, ps);
    } catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("GlyphAdvanceCalculator: GetStringLength failed: ") + e.what());
    }
}
