#pragma once
#include <podofo/podofo.h>
#include <string>
#include <stdexcept>

// Computes total glyph-advance widths for redaction gap normalization and inline editing.
// Used by PoDoFoBackend (content-stream surgery) and future M3 inline-edit features.
// Throws std::runtime_error when all encoding paths fail — caller aborts redaction safely.
class GlyphAdvanceCalculator {
public:
    // Mirrors PdfTextState but decoupled from PoDoFo headers in callers outside engines/podofo/.
    struct TextState {
        double fontSize    = 12.0;
        double fontScale   = 1.0;   // Tz operator value already divided by 100
        double charSpacing = 0.0;   // Tc
        double wordSpacing = 0.0;   // Tw
    };

    // Compute total advance for an encoded PDF string from a content-stream operator.
    // Uses TryGetEncodedStringLength → decoded-string GetStringLength → CID per-glyph fallback.
    // Throws std::runtime_error if all paths fail (e.g., truly unsupported font encoding).
    static double sumAdvances(const PoDoFo::PdfFont& font,
                              const PoDoFo::PdfString& encodedStr,
                              const TextState& state);

    // Compute total advance for a UTF-8 decoded string (M3 inline-edit reuse path).
    // Throws std::runtime_error if GetStringLength fails.
    static double sumAdvances(const PoDoFo::PdfFont& font,
                              std::string_view utf8Text,
                              const TextState& state);

private:
    static PoDoFo::PdfTextState makePdfTextState(const PoDoFo::PdfFont& font, const TextState& state);
};
