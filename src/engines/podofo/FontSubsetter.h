// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QSet>
#include <QVector>
#include <QByteArray>
#include <QtGlobal>
#include <string>

namespace PoDoFo {
class PdfMemDocument;
}

// ── Font subsetting for Compress (font-subsetting-plan-2026-10-01, route A) ──
//
// Keep-CID "blank-glyph" subsetter. The embedded font PROGRAM of eligible
// TrueType fonts (/FontFile2 — the CIDFontType2 CJK bulk, plus provable simple
// /TrueType fonts) is rewritten so glyphs whose glyph-IDs were never shown
// lose their outlines (loca[i] == loca[i+1]) while GID/CID numbering is
// preserved untouched. Because numbering is preserved, content streams,
// /ToUnicode, /W, /CIDToGIDMap and every other consumer-facing structure stay
// byte-identical — the blast radius is confined to the font file itself, which
// is what makes the pass verifiable by render-diff (TestFontSubset).
//
// Scope of THIS lane (owner-approved Option A TrueType core):
//   * Type0 /CIDFontType2 with /CIDToGIDMap /Identity (absent == Identity per
//     spec) or a /CIDToGIDMap stream — the dominant full-embed CJK case;
//   * simple /TrueType fonts whose code→GID mapping can be PROVEN (built-in
//     cmap with no /Encoding, or /Encoding /Differences fully resolvable
//     through the post-table glyph names);
//   * composite-glyph closure: a kept composite glyph keeps its whole
//     component chain (transitive).
// Skipped + disclosed (never guessed): CFF/Type1C/CIDFontType0C/OpenType
// programs (/FontFile3), Type1 (/FontFile), Type3 (no font program), signed
// documents (the incremental-update write path cannot shrink — same guard as
// the Phase-4 sweep), fonts in AcroForm /DR (usage depends on viewer-side
// appearance regeneration), and any font whose glyph usage cannot be proven.
namespace gp { namespace fontsubset {

// ── Layer 1: pure sfnt surgery (no PoDoFo, directly unit-testable) ──────────

// Parse the sfnt table directory. Fills numTables and the tag/offset/length
// triples (logical order preserved). Returns false for anything that is not a
// parseable single TrueType sfnt (TrueType collections 'ttcf', OpenType/CFF
// 'OTTO', truncated directories).
struct SfntTableEntry { uint32_t tag = 0; uint32_t offset = 0; uint32_t length = 0; };
bool parseSfntDirectory(const unsigned char* data, size_t size,
                        QVector<SfntTableEntry>& tablesOut);

// Expand `usedGids` with the transitive closure of composite-glyph components
// found in the glyf table, and always include GID 0 (.notdef). Returns false
// when glyf/loca/maxp/head are missing or malformed relative to each other —
// the caller must then NOT rewrite the program (skip + disclose).
bool expandCompositeClosure(const unsigned char* data, size_t size,
                            QSet<uint32_t>& usedGids);

// Rewrite the program: every glyph NOT in `keepGids` becomes an empty glyph
// (zero-length loca span); kept glyphs keep their outline bytes verbatim.
// All other tables are byte-identical; loca is rebuilt in the same format,
// head.checkSumAdjustment and the touched directory checksums are recomputed.
// Returns false (and writes nothing) when the program cannot be rewritten.
bool blankUnusedGlyphs(const unsigned char* data, size_t size,
                       const QSet<uint32_t>& keepGids,
                       QByteArray& outFont);

// Sum of glyf entry lengths (loca spans) for glyphs outside `keepGids` — the
// exact blankable byte count the rewriter would remove (estimator honesty:
// the estimate reflects what the pass will actually do).
qint64 blankableGlyfBytes(const unsigned char* data, size_t size,
                          const QSet<uint32_t>& keepGids,
                          bool* okOut = nullptr);

// cmap lookup: charcode (interpreted against the best available subtable —
// (3,1)/(0,x) Unicode preferred, then (3,0) symbol, then (1,0) Mac Roman)
// → glyph ID. Formats 0/4/6/12. Returns 0 when no mapping exists.
uint32_t cmapLookup(const unsigned char* data, size_t size, uint32_t code);

// post-table (version 2.0) glyph-name lookup — the /Encoding /Differences
// resolution path for simple TrueType fonts. Covers the 258 Macintosh
// standard-order names and the font's custom Pascal-string names.
bool postNameToGid(const unsigned char* data, size_t size,
                   const std::string& name, uint16_t& gidOut);

// ── Layer 2: document walk (PoDoFo) ─────────────────────────────────────────

// Per-run outcome counters — the honesty surface: what shipped, what was
// skipped and why. Logged at the end of the pass and asserted by the
// skip-disclosure pins in TestFontSubset.
struct FontSubsetStats {
    int fontProgramsEligible = 0;      // FontFile2 sfnt programs found
    int fontProgramsSubsetted = 0;
    int skippedSignedDoc = 0;          // whole-document guard (0/1)
    int skippedCffProgram = 0;         // /FontFile3 (Type1C / CIDFontType0C / OpenType)
    int skippedType1Program = 0;       // /FontFile (Type1)
    int skippedType3 = 0;              // no font program to rewrite
    int skippedUnknownUsage = 0;       // AcroForm /DR fonts, unparseable streams
    int skippedEncodingAmbiguous = 0;  // simple font whose code→GID cannot be proven
    int skippedCorruptProgram = 0;     // undecodable / malformed sfnt
    int skippedUnsupportedStream = 0;  // /DecodeParms or a non-Flate stream filter
    int skippedAlreadySubset = 0;      // subset-prefixed below the rewrite threshold
    int skippedNoGain = 0;             // rewritten program would not shrink the stream
    qint64 bytesSaved = 0;             // measured encoded-stream delta (committed writes)
};

// Rewrite eligible embedded TrueType programs in place. Never throws, never
// fails the caller's run: every per-font error degrades to a counted skip.
// `documentIsSigned` must come from the same signature inspection the other
// optimizeDocument phases use (the signed-doc write path cannot shrink).
FontSubsetStats subsetDocumentFonts(PoDoFo::PdfMemDocument& doc,
                                    bool documentIsSigned);

// Estimator seam: the exact (haircut-applied) savings estimateOptimization
// may claim for options.subsetFonts — nonzero ONLY for FontFile2 programs the
// pass will actually rewrite in this unsigned document. 0 when nothing is
// eligible. Mirrors subsetDocumentFonts' eligibility exactly.
qint64 estimateSubsetSavings(PoDoFo::PdfMemDocument& doc, bool documentIsSigned);

} } // namespace gp::fontsubset
