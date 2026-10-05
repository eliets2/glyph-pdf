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
// fonts is rewritten so glyphs whose glyph-IDs were never shown lose their
// outlines (TrueType: zero-length loca spans; CFF: bare `endchar`
// charstrings) while GID/CID numbering is preserved untouched. Because
// numbering is preserved, content streams, /ToUnicode, /W, /CIDToGIDMap and
// every other consumer-facing structure stay byte-identical — the blast
// radius is confined to the font file itself, which is what makes the pass
// verifiable by render-diff (TestFontSubset).
//
// Scope (owner-approved Option A: TrueType core + CFF follow-up lane):
//   * Type0 /CIDFontType2 with /CIDToGIDMap /Identity (absent == Identity per
//     spec) or a /CIDToGIDMap stream — the dominant full-embed CJK case;
//   * simple /TrueType fonts whose code→GID mapping can be PROVEN (built-in
//     cmap with no /Encoding, or /Encoding /Differences fully resolvable
//     through the post-table glyph names);
//   * Type0 /CIDFontType0 with Identity-H/-V over a CID-keyed /FontFile3
//     /CIDFontType0C — CIDs resolve through the CFF charset (inverse lookup);
//   * simple /Type1 over a name-keyed /FontFile3 /Type1C — built-in CFF
//     encoding or /Encoding /Differences resolved through charset names;
//   * simple /OpenType over an /FontFile3 /OpenType (OTTO-wrapped CFF) —
//     the sfnt cmap/post prove GIDs, the 'CFF ' table is rewritten in place;
//   * TrueType composite-glyph closure: a kept composite glyph keeps its
//     whole component chain (transitive).
// Skipped + disclosed (never guessed): Type1 (/FontFile), Type3 (no font
// program), signed documents (the incremental-update write path cannot
// shrink — same guard as the Phase-4 sweep), fonts in AcroForm /DR (usage
// depends on viewer-side appearance regeneration), CFF shapes whose CID→GID
// or code→GID mapping cannot be proven (non-CID-keyed CFF under a Type0
// wrapper, named /Encodings, seac-using charstrings, non-standard charset/
// encoding tables, subrs not contiguous with their Private DICT, any
// unmodeled bytes in the CFF), sub-FontMatrix transforms (a non-default
// /FontMatrix on the referencing font dict, or a FontMatrix operator in the
// CFF Top DICT — the transformed glyph space is not modeled), and any font
// whose glyph usage cannot be proven.
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
// → glyph ID. Formats 0/4/6/12. `allowOtto` admits OpenType wrappers (the
// OpenType/CFF path consults the wrapper's cmap). Returns 0 when no mapping
// exists.
uint32_t cmapLookup(const unsigned char* data, size_t size, uint32_t code,
                    bool allowOtto = false);

// post-table (version 2.0) glyph-name lookup — the /Encoding /Differences
// resolution path for simple TrueType fonts. Covers the 258 Macintosh
// standard-order names and the font's custom Pascal-string names.
// `allowOtto` admits OpenType wrappers (post lookup on /OpenType fonts).
bool postNameToGid(const unsigned char* data, size_t size,
                   const std::string& name, uint16_t& gidOut,
                   bool allowOtto = false);

// sfnt directory parse that ALSO accepts OpenType 'OTTO' wrappers (the
// OpenType/CFF path needs the table list of the wrapper; the TrueType glyf
// lane keeps the strict parser above, whose OTTO rejection is pinned).
bool parseSfntDirectoryEx(const unsigned char* data, size_t size,
                          QVector<SfntTableEntry>& tablesOut, bool allowOtto);

// ── Layer 1b: pure CFF surgery (no PoDoFo, directly unit-testable) ──────────

// One located CFF section (offset/length within the CFF data).
struct CffSection { uint32_t offset = 0; uint32_t length = 0; };

// A Private DICT plus its (required-contiguous, else the font is skipped)
// local Subrs INDEX.
struct CffPrivateSection {
    CffSection dict;
    CffSection subrs;
    bool hasSubrs = false;
};

// Everything the blanking transform needs about a CFF's physical layout.
// `ok == true` guarantees the parsed sections tile the whole input exactly —
// no unmodeled bytes, no overlaps — which is what makes the full rebuild
// below provably structure-preserving.
struct CffLayout {
    bool ok = false;
    bool cidKeyed = false;      // Top DICT carries ROS
    bool hasCharset = false;    // custom charset section (not a predefined one)
    bool hasEncoding = false;   // custom encoding section
    bool hasFdArray = false;
    bool hasFdSelect = false;
    uint32_t numGlyphs = 0;     // CharStrings INDEX count
    // Raw Top DICT operands, kept for the resolution helpers below:
    // charsetOffset 0 = predefined ISOAdobe (identity); encodingOffset
    // 0 = predefined Adobe Standard Encoding, 1 = predefined Expert
    // (refused), >2 = a custom encoding section.
    uint32_t charsetOffset = 0;
    uint32_t encodingOffset = 0;
    CffSection header, nameIndex, topDictIndex, stringIndex, gsubrIndex;
    CffSection charset, encoding, charStrings, fdArray, fdSelect;
    std::vector<CffPrivateSection> privates;
};

// Parse and validate a CFF: header (major version 1), the four fixed INDEXes
// in order, every Top DICT / FDArray Font DICT reference resolved, charset
// format 0/1/2 bounds-checked against numGlyphs, FDSelect format 0/3, and
// the exact-coverage tiling above. Any unsupported shape (predefined Expert
// charset/encoding, encoding supplements, CFF2, multiple Top DICTs, gaps,
// non-contiguous subrs) returns ok == false — the caller must NOT rewrite.
bool parseCffLayout(const unsigned char* data, size_t size, CffLayout& layoutOut);

// Sum over glyphs OUTSIDE `keepGids` of (charstring length - 1) — the exact
// raw byte delta the rewriter will produce (each blanked charstring becomes
// one `endchar` byte). 0 with okOut=false on an unparseable program.
qint64 blankableCffCharstringBytes(const unsigned char* data, size_t size,
                                   const QSet<uint32_t>& keepGids,
                                   bool* okOut = nullptr);

// Rewrite the program: every charstring NOT in `keepGids` becomes a bare
// `endchar`; kept charstrings keep their bytes verbatim. The charset,
// Encoding, FDSelect, global subrs and every Private DICT/Subrs pair are
// byte-identical; the Top DICT / FDArray are rebuilt with re-encoded
// (structure-preserving) offsets. Returns false (and writes nothing) when
// the program cannot be rewritten.
bool blankUnusedCffCharstrings(const unsigned char* data, size_t size,
                               const QSet<uint32_t>& keepGids, QByteArray& outCff);

// CID→GID resolution for a CID-keyed CFF: builds the inverse charset (formats
// 0/1/2 carry CIDs for CID-keyed fonts) and maps every shown CID. Returns
// false when any shown CID has no glyph, or the charset is a predefined
// non-identity table — the caller must skip, never guess.
bool cffCidsToGids(const unsigned char* data, size_t size,
                   const QSet<uint32_t>& cids, QSet<uint32_t>& gidsOut);

// code→GID through the font's built-in encoding: a custom encoding (formats
// 0/1, no supplements) maps codes to glyph IDs directly; the predefined
// Adobe Standard Encoding is proven for ASCII 32..126 only. Returns false
// for any code it cannot prove.
bool cffCodeToGidBuiltIn(const unsigned char* data, size_t size,
                         uint32_t code, uint32_t& gidOut);

// glyph-NAME→GID through the charset: standard-string SIDs (< 391, the 258
// Macintosh standard-order names) and String INDEX entries (SID ≥ 391).
// Returns false when the name cannot be proven absent or present.
bool cffNameToGid(const unsigned char* data, size_t size,
                  const std::string& name, uint16_t& gidOut);

// sfnt rebuild for the OpenType/CFF wrapper: every table except `tag` is
// byte-identical, `replacement` takes `tag`'s slot, directory checksums and
// head.checkSumAdjustment are recomputed (ISO 14496-6, same discipline as
// blankUnusedGlyphs).
bool rebuildSfntReplacingTable(const unsigned char* data, size_t size, uint32_t tag,
                               const QByteArray& replacement, QByteArray& outSfnt);

// ── Layer 2: document walk (PoDoFo) ─────────────────────────────────────────

// Per-run outcome counters — the honesty surface: what shipped, what was
// skipped and why. Logged at the end of the pass and asserted by the
// skip-disclosure pins in TestFontSubset.
struct FontSubsetStats {
    int fontProgramsEligible = 0;      // FontFile2/FontFile3 programs found rewritable
    int fontProgramsSubsetted = 0;
    int skippedSignedDoc = 0;          // whole-document guard (0/1)
    int skippedCffProgram = 0;         // /FontFile3 with an unhandled subtype/shape
    int skippedCffUnsupported = 0;     // CFF parseable but a feature gate refuses it
    int skippedFontMatrix = 0;         // non-default /FontMatrix on the font dict
    int skippedType1Program = 0;       // /FontFile (Type1)
    int skippedType3 = 0;              // no font program to rewrite
    int skippedUnknownUsage = 0;       // AcroForm /DR fonts, unparseable streams
    int skippedEncodingAmbiguous = 0;  // font whose code/CID→GID cannot be proven
    int skippedCorruptProgram = 0;     // undecodable / malformed sfnt or CFF
    int skippedUnsupportedStream = 0;  // /DecodeParms or a non-Flate stream filter
    int skippedAlreadySubset = 0;      // subset-prefixed below the rewrite threshold
    int skippedNoGain = 0;             // rewritten program would not shrink the stream
    qint64 bytesSaved = 0;             // measured encoded-stream delta (committed writes)
};

// Rewrite eligible embedded font programs (TrueType sfnt and CFF) in place.
// Never throws, never fails the caller's run: every per-font error degrades
// to a counted skip. `documentIsSigned` must come from the same signature
// inspection the other optimizeDocument phases use (the signed-doc write
// path cannot shrink).
FontSubsetStats subsetDocumentFonts(PoDoFo::PdfMemDocument& doc,
                                    bool documentIsSigned);

// Estimator seam: the exact (haircut-applied) savings estimateOptimization
// may claim for options.subsetFonts — nonzero ONLY for programs (FontFile2 /
// FontFile3) the pass will actually rewrite in this unsigned document. 0 when
// nothing is eligible. Mirrors subsetDocumentFonts' eligibility exactly.
qint64 estimateSubsetSavings(PoDoFo::PdfMemDocument& doc, bool documentIsSigned);

} } // namespace gp::fontsubset
