// SPDX-License-Identifier: Apache-2.0
// Keep-CID blank-glyph font subsetter — implementation (route A of
// docs/research/font-subsetting-plan-2026-10-01.md, TrueType core).
//
// Design constraints this file is written against:
//   * GID/CID numbering is NEVER reassigned — content streams, /ToUnicode,
//     /W and /CIDToGIDMap stay byte-identical (the pass rewrites ONLY the
//     embedded font program stream).
//   * One hostile/malformed font degrades to a counted skip, never aborts
//     the run (the §9.13 F5 Phase-1 containment pattern). Whenever glyph
//     usage cannot be PROVEN for a font, the font is skipped — a guessed
//     keep-set could blank a used glyph, the pass's worst failure mode.
//   * The estimator and the pass share one analysis (analyzeFontPrograms),
//     so the dialog can never predict savings the pass will not realize.
#include "FontSubsetter.h"

#include <QDebug>
#include <QSet>

#include <podofo/podofo.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace gp { namespace fontsubset {

namespace {

constexpr uint32_t kSfntTrueType1 = 0x00010000u;
constexpr uint32_t kSfntTrueType2 = 0x74727565u; // 'true'
constexpr uint32_t kTagGlyf       = 0x676c7966u; // 'glyf'
constexpr uint32_t kTagLoca       = 0x6c6f6361u; // 'loca'
constexpr uint32_t kTagHead       = 0x68656164u; // 'head'
constexpr uint32_t kTagMaxp       = 0x6d617870u; // 'maxp'
constexpr uint32_t kTagCmap       = 0x636d6170u; // 'cmap'
constexpr uint32_t kTagPost       = 0x706f7374u; // 'post'

// An already subset-prefixed font (AAAAAA+Name) whose encoded program is
// below this size is left untouched: PoDoFo-written subsets are dense, so a
// rewrite would churn bytes for no real gain (plan §3.2 write-back notes).
constexpr qint64 kPrefixedRewriteThreshold = 1024 * 1024;

uint16_t rd16(const unsigned char* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}
uint32_t rd32(const unsigned char* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
         | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}
void wr16(unsigned char* p, uint16_t v) {
    p[0] = static_cast<unsigned char>((v >> 8) & 0xFF);
    p[1] = static_cast<unsigned char>(v & 0xFF);
}
void wr32(unsigned char* p, uint32_t v) {
    p[0] = static_cast<unsigned char>((v >> 24) & 0xFF);
    p[1] = static_cast<unsigned char>((v >> 16) & 0xFF);
    p[2] = static_cast<unsigned char>((v >> 8) & 0xFF);
    p[3] = static_cast<unsigned char>(v & 0xFF);
}

// ISO/IEC 14496-6 table checksum: uint32 groups, zero-padded tail, sum mod 2^32.
uint32_t tableChecksum(const unsigned char* data, size_t length) {
    uint32_t sum = 0;
    const size_t groups = length / 4;
    for (size_t i = 0; i < groups; ++i)
        sum += rd32(data + i * 4);
    const size_t rem = length % 4;
    if (rem != 0) {
        unsigned char tail[4] = { 0, 0, 0, 0 };
        std::memcpy(tail, data + groups * 4, rem);
        sum += rd32(tail);
    }
    return sum;
}

// ── sfnt layer ────────────────────────────────────────────────────────────────

// Everything the glyf/loca surgery needs, validated up front.
struct GlyfContext {
    QVector<SfntTableEntry> tables;   // the parsed directory
    SfntTableEntry glyf;
    SfntTableEntry loca;
    bool longLoca = false;
    uint32_t numGlyphs = 0;
};

const SfntTableEntry* findTable(const QVector<SfntTableEntry>& tables, uint32_t tag) {
    for (const auto& t : tables)
        if (t.tag == tag) return &t;
    return nullptr;
}

bool glyfContext(const unsigned char* data, size_t size, GlyfContext& ctx) {
    if (!parseSfntDirectory(data, size, ctx.tables)) return false;
    const SfntTableEntry* glyf = findTable(ctx.tables, kTagGlyf);
    const SfntTableEntry* loca = findTable(ctx.tables, kTagLoca);
    const SfntTableEntry* head = findTable(ctx.tables, kTagHead);
    const SfntTableEntry* maxp = findTable(ctx.tables, kTagMaxp);
    if (!glyf || !loca || !head || !maxp) return false;
    if (head->length < 54 || head->offset + head->length > size) return false;
    const uint16_t indexToLoc = rd16(data + head->offset + 50);
    if (indexToLoc > 1) return false;
    if (maxp->length < 6) return false;
    const uint32_t numGlyphs = rd16(data + maxp->offset + 4);
    if (numGlyphs == 0) return false;
    ctx.glyf = *glyf;
    ctx.loca = *loca;
    ctx.longLoca = indexToLoc == 1;
    ctx.numGlyphs = numGlyphs;
    return true;
}

// loca span for `gid`; sets okOut=false when the tables disagree with maxp.
std::pair<uint32_t, uint32_t> locaSpan(const unsigned char* data, size_t size,
                                       const GlyfContext& ctx, uint32_t gid,
                                       bool* okOut) {
    const size_t entrySize = ctx.longLoca ? 4u : 2u;
    const size_t needed = (static_cast<size_t>(gid) + 2) * entrySize;
    if (gid == 0xFFFFFFFFu || ctx.loca.offset + needed > size) {
        *okOut = false;
        return { 0, 0 };
    }
    uint32_t start, end;
    if (ctx.longLoca) {
        start = rd32(data + ctx.loca.offset + static_cast<size_t>(gid) * 4);
        end = rd32(data + ctx.loca.offset + (static_cast<size_t>(gid) + 1) * 4);
    } else {
        start = static_cast<uint32_t>(rd16(data + ctx.loca.offset + static_cast<size_t>(gid) * 2)) * 2u;
        end = static_cast<uint32_t>(rd16(data + ctx.loca.offset + (static_cast<size_t>(gid) + 1) * 2)) * 2u;
    }
    if (start > end || ctx.glyf.offset + end > size) {
        *okOut = false;
        return { 0, 0 };
    }
    return { start, end };
}

// Walk one glyf entry; if composite, append component glyph indices to `out`.
// Returns false on malformed entry (truncated component chain).
bool collectCompositeComponents(const unsigned char* data, size_t size,
                                const GlyfContext& ctx,
                                uint32_t start, uint32_t end,
                                std::vector<uint32_t>& out) {
    constexpr uint16_t kMoreComponents = 0x0020;
    constexpr uint16_t kArgWords = 0x0001;
    constexpr uint16_t kHasScale = 0x0008;
    constexpr uint16_t kHasXYScale = 0x0040;
    constexpr uint16_t kHas2x2 = 0x0080;

    if (start == end) return true;          // empty glyph
    if (end - start < 4) return false;      // too small for even the header
    const unsigned char* p = data + ctx.glyf.offset + start;
    const int16_t numContours = static_cast<int16_t>(rd16(p));
    if (numContours >= 0) return true;      // simple glyph — bytes kept verbatim

    const uint32_t entryLen = end - start;
    uint32_t off = 10;                      // components begin after the 10-byte header
    while (true) {
        if (off + 4 > entryLen) return false;
        const uint16_t flags = rd16(p + off);
        const uint16_t glyphIndex = rd16(p + off + 2);
        out.push_back(glyphIndex);
        off += 4;
        off += (flags & kArgWords) ? 4u : 2u;           // arguments
        if (flags & kHasScale) off += 2;
        else if (flags & kHasXYScale) off += 4;
        else if (flags & kHas2x2) off += 8;
        if (off > entryLen) return false;
        if (!(flags & kMoreComponents)) return true;
    }
}

// ── cmap ──────────────────────────────────────────────────────────────────────

// Score a cmap subtable's (platformID, encodingID); lower is better.
int cmapSubtableScore(uint16_t platform, uint16_t encoding) {
    if (platform == 3 && encoding == 1) return 0;   // Windows BMP Unicode
    if (platform == 3 && encoding == 10) return 1;  // Windows UCS-4
    if (platform == 0) return 2;                    // Unicode (any)
    if (platform == 3 && encoding == 0) return 3;   // Windows symbol
    if (platform == 1 && encoding == 0) return 4;   // Mac Roman
    return 100;                                     // format-2 CJK Mac and exotics
}

std::optional<uint32_t> cmapSubtableLookup(const unsigned char* sub,
                                           size_t subSize, uint32_t code) {
    if (subSize < 4) return std::nullopt;
    const uint16_t format = rd16(sub);
    switch (format) {
    case 0: { // byte encoding table
        if (code > 0xFF || subSize < 262) return std::nullopt;
        return sub[6 + code];
    }
    case 4: { // segment mapping to delta values
        if (subSize < 16) return std::nullopt;
        const uint16_t segCountX2 = rd16(sub + 6);
        const size_t segCount = segCountX2 / 2;
        if (segCount == 0) return std::nullopt;
        const size_t needEnd = 14 + segCountX2 * 3 + 2;
        if (subSize < needEnd) return std::nullopt;
        const unsigned char* endCodes = sub + 14;
        const unsigned char* startCodes = endCodes + segCountX2 + 2; // + reservedPad
        const unsigned char* idDeltas = startCodes + segCountX2;
        const unsigned char* idRangeOffsets = idDeltas + segCountX2;
        for (size_t i = 0; i < segCount; ++i) {
            const uint16_t segEnd = rd16(endCodes + i * 2);
            if (code > segEnd) continue;
            const uint16_t segStart = rd16(startCodes + i * 2);
            if (code < segStart) return 0u;
            const uint16_t idDelta = rd16(idDeltas + i * 2);
            const uint16_t idRangeOffset = rd16(idRangeOffsets + i * 2);
            if (idRangeOffset == 0)
                return static_cast<uint32_t>(static_cast<uint16_t>(code + idDelta));
            // Glyph-index reads are addressed relative to the idRangeOffset
            // word's own location (spec formula); bound-check every read.
            const size_t glyphOff = static_cast<size_t>(idRangeOffsets - sub)
                                  + i * 2 + idRangeOffset
                                  + static_cast<size_t>(code - segStart) * 2;
            if (glyphOff + 2 > subSize) return 0u;
            const uint16_t gid = rd16(sub + glyphOff);
            return gid == 0 ? 0u
                            : static_cast<uint32_t>(static_cast<uint16_t>(gid + idDelta));
        }
        return 0u;
    }
    case 6: { // trimmed table mapping
        if (subSize < 10) return std::nullopt;
        const uint16_t first = rd16(sub + 6);
        const uint16_t count = rd16(sub + 8);
        if (code < first || code >= first + count) return 0u;
        const size_t off = 10 + static_cast<size_t>(code - first) * 2;
        if (off + 2 > subSize) return 0u;
        return rd16(sub + off);
    }
    case 12: { // segmented coverage
        if (subSize < 16) return std::nullopt;
        const uint32_t nGroups = rd32(sub + 12);
        const size_t groupsEnd = 16 + static_cast<size_t>(nGroups) * 12;
        if (nGroups > 0x100000 || subSize < groupsEnd) return std::nullopt;
        size_t lo = 0, hi = nGroups;   // groups are sorted — binary search
        while (lo < hi) {
            const size_t mid = (lo + hi) / 2;
            const unsigned char* g = sub + 16 + mid * 12;
            const uint32_t startChar = rd32(g);
            const uint32_t endChar = rd32(g + 4);
            if (code < startChar) hi = mid;
            else if (code > endChar) lo = mid + 1;
            else {
                const uint32_t startGid = rd32(g + 8);
                return startGid + (code - startChar);
            }
        }
        return 0u;
    }
    default:
        return std::nullopt; // format 2 (Mac CJK) and 13/14: not consulted
    }
}

// ── post (version 2.0) glyph names ────────────────────────────────────────────

// The 258 Macintosh standard-order glyph names (OpenType post v1.0 table,
// verified entry-for-entry against fontTools' standardGlyphOrder — the
// industry reference implementation of the same table).
const std::array<const char*, 258>& macStdGlyphNames() {
    static const std::array<const char*, 258> kNames = {
        ".notdef",
        ".null",
        "nonmarkingreturn",
        "space",
        "exclam",
        "quotedbl",
        "numbersign",
        "dollar",
        "percent",
        "ampersand",
        "quotesingle",
        "parenleft",
        "parenright",
        "asterisk",
        "plus",
        "comma",
        "hyphen",
        "period",
        "slash",
        "zero",
        "one",
        "two",
        "three",
        "four",
        "five",
        "six",
        "seven",
        "eight",
        "nine",
        "colon",
        "semicolon",
        "less",
        "equal",
        "greater",
        "question",
        "at",
        "A",
        "B",
        "C",
        "D",
        "E",
        "F",
        "G",
        "H",
        "I",
        "J",
        "K",
        "L",
        "M",
        "N",
        "O",
        "P",
        "Q",
        "R",
        "S",
        "T",
        "U",
        "V",
        "W",
        "X",
        "Y",
        "Z",
        "bracketleft",
        "backslash",
        "bracketright",
        "asciicircum",
        "underscore",
        "grave",
        "a",
        "b",
        "c",
        "d",
        "e",
        "f",
        "g",
        "h",
        "i",
        "j",
        "k",
        "l",
        "m",
        "n",
        "o",
        "p",
        "q",
        "r",
        "s",
        "t",
        "u",
        "v",
        "w",
        "x",
        "y",
        "z",
        "braceleft",
        "bar",
        "braceright",
        "asciitilde",
        "Adieresis",
        "Aring",
        "Ccedilla",
        "Eacute",
        "Ntilde",
        "Odieresis",
        "Udieresis",
        "aacute",
        "agrave",
        "acircumflex",
        "adieresis",
        "atilde",
        "aring",
        "ccedilla",
        "eacute",
        "egrave",
        "ecircumflex",
        "edieresis",
        "iacute",
        "igrave",
        "icircumflex",
        "idieresis",
        "ntilde",
        "oacute",
        "ograve",
        "ocircumflex",
        "odieresis",
        "otilde",
        "uacute",
        "ugrave",
        "ucircumflex",
        "udieresis",
        "dagger",
        "degree",
        "cent",
        "sterling",
        "section",
        "bullet",
        "paragraph",
        "germandbls",
        "registered",
        "copyright",
        "trademark",
        "acute",
        "dieresis",
        "notequal",
        "AE",
        "Oslash",
        "infinity",
        "plusminus",
        "lessequal",
        "greaterequal",
        "yen",
        "mu",
        "partialdiff",
        "summation",
        "product",
        "pi",
        "integral",
        "ordfeminine",
        "ordmasculine",
        "Omega",
        "ae",
        "oslash",
        "questiondown",
        "exclamdown",
        "logicalnot",
        "radical",
        "florin",
        "approxequal",
        "Delta",
        "guillemotleft",
        "guillemotright",
        "ellipsis",
        "nonbreakingspace",
        "Agrave",
        "Atilde",
        "Otilde",
        "OE",
        "oe",
        "endash",
        "emdash",
        "quotedblleft",
        "quotedblright",
        "quoteleft",
        "quoteright",
        "divide",
        "lozenge",
        "ydieresis",
        "Ydieresis",
        "fraction",
        "currency",
        "guilsinglleft",
        "guilsinglright",
        "fi",
        "fl",
        "daggerdbl",
        "periodcentered",
        "quotesinglbase",
        "quotedblbase",
        "perthousand",
        "Acircumflex",
        "Ecircumflex",
        "Aacute",
        "Edieresis",
        "Egrave",
        "Iacute",
        "Icircumflex",
        "Idieresis",
        "Igrave",
        "Oacute",
        "Ocircumflex",
        "apple",
        "Ograve",
        "Uacute",
        "Ucircumflex",
        "Ugrave",
        "dotlessi",
        "circumflex",
        "tilde",
        "macron",
        "breve",
        "dotaccent",
        "ring",
        "cedilla",
        "hungarumlaut",
        "ogonek",
        "caron",
        "Lslash",
        "lslash",
        "Scaron",
        "scaron",
        "Zcaron",
        "zcaron",
        "brokenbar",
        "Eth",
        "eth",
        "Yacute",
        "yacute",
        "Thorn",
        "thorn",
        "minus",
        "multiply",
        "onesuperior",
        "twosuperior",
        "threesuperior",
        "onehalf",
        "onequarter",
        "threequarters",
        "franc",
        "Gbreve",
        "gbreve",
        "Idotaccent",
        "Scedilla",
        "scedilla",
        "Cacute",
        "cacute",
        "Ccaron",
        "ccaron",
        "dcroat"
    };
    return kNames;
}

} // namespace

// ── Layer 1: pure sfnt surgery (public, unit-testable) ─────────────────────────────

bool parseSfntDirectory(const unsigned char* data, size_t size,
                        QVector<SfntTableEntry>& tablesOut)
{
    tablesOut.clear();
    if (data == nullptr || size < 12) return false;
    const uint32_t version = rd32(data);
    if (version != kSfntTrueType1 && version != kSfntTrueType2)
        return false; // 'ttcf' collections and 'OTTO' (CFF) never enter this lane
    const uint16_t numTables = rd16(data + 4);
    if (numTables == 0 || numTables > 512) return false;
    const size_t dirEnd = 12 + static_cast<size_t>(numTables) * 16;
    if (size < dirEnd) return false;
    for (uint16_t i = 0; i < numTables; ++i) {
        const unsigned char* rec = data + 12 + static_cast<size_t>(i) * 16;
        SfntTableEntry e;
        e.tag = rd32(rec);
        e.offset = rd32(rec + 8);
        e.length = rd32(rec + 12);
        if (e.offset > size || e.length > size - e.offset) return false;
        tablesOut.append(e);
    }
    return !tablesOut.isEmpty();
}

bool expandCompositeClosure(const unsigned char* data, size_t size,
                            QSet<uint32_t>& usedGids)
{
    GlyfContext ctx;
    if (!glyfContext(data, size, ctx)) return false;

    usedGids.insert(0); // .notdef always survives

    std::vector<uint32_t> queue(usedGids.cbegin(), usedGids.cend());
    QSet<uint32_t> visited(queue.cbegin(), queue.cend());
    while (!queue.empty()) {
        const uint32_t gid = queue.back();
        queue.pop_back();
        if (gid >= ctx.numGlyphs) continue; // keep-set member with nothing to walk
        bool ok = true;
        const auto span = locaSpan(data, size, ctx, gid, &ok);
        if (!ok) return false;
        if (span.first == span.second) continue;
        std::vector<uint32_t> components;
        if (!collectCompositeComponents(data, size, ctx, span.first, span.second,
                                        components))
            return false;
        for (uint32_t comp : components) {
            if (!visited.contains(comp)) {
                visited.insert(comp);
                usedGids.insert(comp);
                queue.push_back(comp);
            }
        }
    }
    return true;
}

qint64 blankableGlyfBytes(const unsigned char* data, size_t size,
                          const QSet<uint32_t>& keepGids, bool* okOut)
{
    if (okOut) *okOut = false;
    GlyfContext ctx;
    if (!glyfContext(data, size, ctx)) return 0;

    qint64 blankable = 0;
    for (uint32_t gid = 0; gid < ctx.numGlyphs; ++gid) {
        if (keepGids.contains(gid)) continue;
        bool ok = true;
        const auto span = locaSpan(data, size, ctx, gid, &ok);
        if (!ok) return 0;
        blankable += span.second - span.first;
    }
    if (okOut) *okOut = true;
    return blankable;
}

bool blankUnusedGlyphs(const unsigned char* data, size_t size,
                       const QSet<uint32_t>& keepGids, QByteArray& outFont)
{
    GlyfContext ctx;
    if (!glyfContext(data, size, ctx)) return false;
    const size_t numGlyphs = ctx.numGlyphs;

    // Per-glyph new lengths: kept → original span, else 0. Every span is
    // validated BEFORE any buffer is built so a malformed font fails early.
    std::vector<uint32_t> newLens(numGlyphs, 0);
    for (uint32_t gid = 0; gid < numGlyphs; ++gid) {
        if (!keepGids.contains(gid)) continue;
        bool ok = true;
        const auto span = locaSpan(data, size, ctx, gid, &ok);
        if (!ok) return false;
        newLens[gid] = span.second - span.first;
    }

    // Output layout: tables in directory order, 4-byte aligned offsets.
    struct OutTable { const SfntTableEntry* in; uint32_t offset; uint32_t length; };
    std::vector<OutTable> outTables;
    outTables.reserve(ctx.tables.size());
    uint32_t cursor = 12 + static_cast<uint32_t>(ctx.tables.size()) * 16;
    for (const auto& t : ctx.tables) {
        OutTable ot;
        ot.in = &t;
        ot.length = t.length;
        if (t.tag == kTagGlyf) {
            uint32_t total = 0;
            for (uint32_t gid = 0; gid < numGlyphs; ++gid) total += newLens[gid];
            ot.length = total;
        } else if (t.tag == kTagLoca) {
            ot.length = (numGlyphs + 1u) * (ctx.longLoca ? 4u : 2u);
        }
        ot.offset = cursor;
        cursor += ot.length;
        cursor = (cursor + 3u) & ~3u; // pad to the 4-byte boundary
        outTables.push_back(ot);
    }

    QByteArray out(static_cast<qsizetype>(cursor), '\0');
    unsigned char* op = reinterpret_cast<unsigned char*>(out.data());

    // Header: sfnt version and numTables verbatim, search parameters recomputed.
    std::memcpy(op, data, 6);
    uint32_t sr = 1;
    uint16_t entrySelector = 0;
    while (sr * 2 <= ctx.tables.size()) { sr *= 2; ++entrySelector; }
    const uint16_t searchRange = static_cast<uint16_t>(sr * 16);
    const uint16_t rangeShift = static_cast<uint16_t>(ctx.tables.size() * 16 - searchRange);
    wr16(op + 6, searchRange);
    wr16(op + 8, entrySelector);
    wr16(op + 10, rangeShift);

    // Directory + table bodies.
    ptrdiff_t headSlot = -1;
    for (size_t i = 0; i < ctx.tables.size(); ++i) {
        const OutTable& ot = outTables[i];
        unsigned char* rec = op + 12 + i * 16;
        wr32(rec, ot.in->tag);
        wr32(rec + 8, ot.offset);
        wr32(rec + 12, ot.length);
        if (ot.in->tag == kTagGlyf) {
            // Kept glyph outline bytes are copied back-to-back in GID order;
            // every other glyph contributes a zero-length loca span.
            uint32_t write = ot.offset;
            for (uint32_t gid = 0; gid < numGlyphs; ++gid) {
                if (newLens[gid] == 0) continue;
                bool ok = true;
                const auto span = locaSpan(data, size, ctx, gid, &ok);
                if (!ok) return false; // validated above — cannot happen
                std::memcpy(op + write, data + ctx.glyf.offset + span.first, newLens[gid]);
                write += newLens[gid];
            }
        } else if (ot.in->tag == kTagLoca) {
            uint32_t acc = 0;
            for (uint32_t gid = 0; gid < numGlyphs; ++gid) {
                if (ctx.longLoca)
                    wr32(op + ot.offset + static_cast<size_t>(gid) * 4, acc);
                else
                    wr16(op + ot.offset + static_cast<size_t>(gid) * 2,
                         static_cast<uint16_t>(acc / 2));
                acc += newLens[gid];
            }
            if (ctx.longLoca)
                wr32(op + ot.offset + static_cast<size_t>(numGlyphs) * 4, acc);
            else
                wr16(op + ot.offset + static_cast<size_t>(numGlyphs) * 2,
                     static_cast<uint16_t>(acc / 2));
        } else {
            std::memcpy(op + ot.offset, data + ot.in->offset, ot.length);
            if (ot.in->tag == kTagHead) {
                // checkSumAdjustment is zeroed while its own checksum is taken;
                // the real adjustment is written after the whole-font sum.
                std::memset(op + ot.offset + 8, 0, 4);
                // indexToLocFormat stays as-is: same entry count, same format.
                headSlot = static_cast<ptrdiff_t>(i);
            }
        }
        wr32(rec + 4, tableChecksum(op + ot.offset, ot.length));
    }

    // head.checkSumAdjustment = 0xB1B0AFBA - checksum(whole font, adjustment 0).
    // Spec order (ISO 14496-6): the directory entry for head carries the table
    // checksum taken over the ZEROED adjustment (done in the loop above), the
    // whole-font sum is computed, and ONLY the adjustment is then written —
    // the directory is never touched again.
    if (headSlot >= 0) {
        const uint32_t whole = tableChecksum(op, static_cast<size_t>(cursor));
        const uint32_t adjustment = 0xB1B0AFBAu - whole;
        wr32(op + outTables[static_cast<size_t>(headSlot)].offset + 8, adjustment);
    }

    outFont = out;
    return true;
}

uint32_t cmapLookup(const unsigned char* data, size_t size, uint32_t code,
                    bool allowOtto)
{
    QVector<SfntTableEntry> tables;
    if (!parseSfntDirectoryEx(data, size, tables, allowOtto)) return 0;
    const SfntTableEntry* cmap = findTable(tables, kTagCmap);
    if (!cmap || cmap->length < 4) return 0;
    const unsigned char* c = data + cmap->offset;
    const uint16_t numSub = rd16(c + 2);
    if (numSub == 0 || 4 + static_cast<size_t>(numSub) * 8 > cmap->length) return 0;
    int bestScore = 101;
    const unsigned char* best = nullptr;
    size_t bestSize = 0;
    for (uint16_t i = 0; i < numSub; ++i) {
        const unsigned char* rec = c + 4 + static_cast<size_t>(i) * 8;
        const uint16_t platform = rd16(rec);
        const uint16_t encoding = rd16(rec + 2);
        const uint32_t offset = rd32(rec + 4);
        if (offset >= cmap->length) continue;
        const int score = cmapSubtableScore(platform, encoding);
        if (score >= bestScore) continue;
        const unsigned char* sub = c + offset;
        const size_t subSize = cmap->length - offset;
        if (!cmapSubtableLookup(sub, subSize, code)) continue; // unsupported format
        bestScore = score;
        best = sub;
        bestSize = subSize;
    }
    if (!best) return 0;
    return cmapSubtableLookup(best, bestSize, code).value_or(0);
}

bool postNameToGid(const unsigned char* data, size_t size,
                   const std::string& name, uint16_t& gidOut, bool allowOtto)
{
    QVector<SfntTableEntry> tables;
    if (!parseSfntDirectoryEx(data, size, tables, allowOtto)) return false;
    const SfntTableEntry* post = findTable(tables, kTagPost);
    if (!post || post->length < 34 || post->offset + post->length > size) return false;
    const unsigned char* p = data + post->offset;
    const uint32_t version = rd32(p);
    if (version != 0x00020000u) return false; // v3.0 carries no names — cannot prove
    const uint16_t numGlyphs = rd16(p + 32);
    if (post->length < 34 + static_cast<size_t>(numGlyphs) * 2) return false;

    int stdIndex = -1; // Macintosh standard order, or -1 for a custom name
    const auto& macStd = macStdGlyphNames();
    for (int i = 0; i < 258; ++i) {
        if (name == macStd[static_cast<size_t>(i)]) { stdIndex = i; break; }
    }

    const unsigned char* idx = p + 34;
    const unsigned char* names = idx + static_cast<size_t>(numGlyphs) * 2;
    const size_t namesAvail = post->length - static_cast<size_t>(names - p);
    size_t nameCursor = 0;
    for (uint16_t g = 0; g < numGlyphs; ++g) {
        const uint16_t nameIndex = rd16(idx + static_cast<size_t>(g) * 2);
        if (nameIndex < 258) {
            if (stdIndex >= 0 && nameIndex == static_cast<uint16_t>(stdIndex)) {
                gidOut = g;
                return true;
            }
        } else {
            // Custom Pascal strings appear in sequential order for all
            // nameIndex >= 258 entries.
            if (nameCursor >= namesAvail) return false;
            const uint8_t len = names[nameCursor];
            if (nameCursor + 1u + len > namesAvail) return false;
            if (stdIndex < 0 && len == name.size()
                && std::memcmp(names + nameCursor + 1, name.data(), len) == 0) {
                gidOut = g;
                return true;
            }
            nameCursor += 1u + len;
        }
    }
    return false; // name not present — caller must skip (never guess)
}

// ── Layer 1b: pure CFF surgery (public, unit-testable) ─────────────────────────
//
// Layout rules per Adobe TN 5176, cross-checked against fontTools' cffLib
// reference implementation: INDEX offsets are relative to the byte preceding
// the data (first offset == 1); DICT ints use the b0=v+139 / 247..250 /
// 251..254 / 28 / 29 encodings; Top DICT ops charset 15, Encoding 16,
// CharStrings 17, Private 18 (size, offset), ROS 12 30, FDArray 12 36,
// FDSelect 12 37; charset format 0 lists numGlyphs-1 SIDs (CIDs for CID-keyed
// fonts), formats 1/2 are {first, nLeft} ranges; custom encodings (formats
// 0/1) map codes to glyph IDs directly, with the predefined Standard Encoding
// (offset 0) covered here for ASCII 32..126; Private op 19 Subrs is an offset
// relative to the Private DICT's own start.
namespace {

constexpr uint32_t kTagCff = 0x43464620u; // 'CFF '
constexpr uint32_t kOttoTag = 0x4F54544Fu; // 'OTTO'

struct CffIndex {
    uint32_t start = 0;               // INDEX start (the count field)
    uint32_t end = 0;                 // one past the INDEX's last byte
    uint32_t count = 0;
    uint32_t base = 0;                // absolute position of "offset 0"
    std::vector<uint32_t> offsets;    // count+1 raw (1-based, relative) offsets
    bool valid = false;

    uint32_t entryStart(uint32_t i) const { return base + offsets[i]; }
    uint32_t entryEnd(uint32_t i) const { return base + offsets[i + 1]; }
    uint32_t entryLength(uint32_t i) const {
        return offsets.empty() ? 0 : offsets[i + 1] - offsets[i];
    }
};

bool parseCffIndex(const unsigned char* data, size_t size, uint32_t off,
                   CffIndex& ixOut)
{
    ixOut = CffIndex{};
    if (off > size || size - off < 2) return false;
    ixOut.start = off;
    ixOut.count = rd16(data + off);
    if (ixOut.count == 0) {
        ixOut.end = off + 2; // empty INDEX: the count field only
        ixOut.base = ixOut.end;
        ixOut.valid = true;
        return true;
    }
    if (off + 3 > size) return false;
    const uint32_t offSize = data[off + 2];
    if (offSize < 1 || offSize > 4) return false;
    const uint64_t offsetsStart = off + 3ull;
    const uint64_t offsetsEnd = offsetsStart + (uint64_t)(ixOut.count + 1) * offSize;
    if (offsetsEnd > size) return false;
    ixOut.base = static_cast<uint32_t>(offsetsEnd) - 1; // byte preceding the data
    ixOut.offsets.resize(ixOut.count + 1);
    for (uint32_t i = 0; i <= ixOut.count; ++i) {
        uint32_t v = 0;
        for (uint32_t b = 0; b < offSize; ++b)
            v = (v << 8) | data[offsetsStart + i * offSize + b];
        if (i == 0 && v < 1) return false;
        if (i > 0 && v < ixOut.offsets[i - 1]) return false; // must not decrease
        ixOut.offsets[i] = v;
    }
    if (ixOut.base + ixOut.offsets[ixOut.count] > size) return false;
    ixOut.end = ixOut.base + ixOut.offsets[ixOut.count];
    ixOut.valid = true;
    return true;
}

// One parsed DICT item: operands followed by one operator.
struct CffOperand {
    bool isReal = false;
    std::string raw;   // raw bytes for reals (preserved verbatim on rebuild)
    int32_t value = 0; // decoded integer value
};
struct CffDictItem {
    bool escaped = false;
    uint8_t op = 0;
    std::string opBytes;                 // operator bytes (verbatim on rebuild)
    std::vector<CffOperand> operands;
    uint32_t opKey() const { return escaped ? (0x1200u + op) : op; }
};

bool parseCffDict(const unsigned char* data, size_t size, uint32_t start,
                  uint32_t end, std::vector<CffDictItem>& itemsOut)
{
    itemsOut.clear();
    if (start > size || end > size || start > end) return false;
    uint32_t p = start;
    CffDictItem cur;
    const auto flush = [&]() {
        itemsOut.push_back(cur);
        cur = CffDictItem{};
    };
    while (p < end) {
        const uint8_t b0 = data[p];
        if (b0 <= 21) { // operator (12 x escape)
            const uint32_t opStart = p;
            ++p;
            if (b0 == 12) {
                if (p >= end) return false;
                cur.escaped = true;
                cur.op = data[p];
                ++p;
            } else {
                cur.op = b0;
            }
            cur.opBytes.assign(reinterpret_cast<const char*>(data) + opStart,
                               p - opStart);
            flush();
        } else if (b0 == 28) { // 3-byte int16
            if (p + 3 > end) return false;
            CffOperand o;
            o.value = static_cast<int16_t>(rd16(data + p + 1));
            o.raw.assign(reinterpret_cast<const char*>(data) + p, 3);
            cur.operands.push_back(o);
            p += 3;
        } else if (b0 == 29) { // 5-byte int32
            if (p + 5 > end) return false;
            CffOperand o;
            o.value = static_cast<int32_t>(rd32(data + p + 1));
            o.raw.assign(reinterpret_cast<const char*>(data) + p, 5);
            cur.operands.push_back(o);
            p += 5;
        } else if (b0 == 30) { // real (BCD nibbles, terminated by an f nibble)
            const uint32_t s = p;
            ++p;
            bool done = false;
            while (p < end && !done) {
                const uint8_t byte = data[p];
                ++p;
                for (int nib = 0; nib < 2 && !done; ++nib) {
                    const uint8_t n = nib == 0 ? (byte >> 4) : (byte & 0xF);
                    if (n == 0xF) done = true;
                    else if (n == 0xD) return false; // reserved nibble
                }
            }
            if (!done) return false;
            CffOperand o;
            o.isReal = true;
            o.raw.assign(reinterpret_cast<const char*>(data) + s, p - s);
            cur.operands.push_back(o);
        } else if (b0 >= 32 && b0 <= 246) {
            CffOperand o;
            o.value = static_cast<int32_t>(b0) - 139;
            o.raw.assign(reinterpret_cast<const char*>(data) + p, 1);
            cur.operands.push_back(o);
            p += 1;
        } else if (b0 >= 247 && b0 <= 250) {
            if (p + 2 > end) return false;
            CffOperand o;
            o.value = (static_cast<int32_t>(b0) - 247) * 256 + data[p + 1] + 108;
            o.raw.assign(reinterpret_cast<const char*>(data) + p, 2);
            cur.operands.push_back(o);
            p += 2;
        } else if (b0 >= 251 && b0 <= 254) {
            if (p + 2 > end) return false;
            CffOperand o;
            o.value = -(static_cast<int32_t>(b0) - 251) * 256 - data[p + 1] - 108;
            o.raw.assign(reinterpret_cast<const char*>(data) + p, 2);
            cur.operands.push_back(o);
            p += 2;
        } else {
            return false; // reserved byte (22..27, 31)
        }
    }
    return cur.operands.empty(); // trailing operands without an operator
}

void appendDictInt(QByteArray& out, int32_t v)
{
    const auto w8 = [&out](int x) { out.append(static_cast<char>(x & 0xFF)); };
    if (v >= -107 && v <= 107) {
        w8(v + 139);
    } else if (v >= 108 && v <= 1131) {
        v -= 108;
        w8(247 + v / 256);
        w8(v % 256);
    } else if (v <= -108 && v >= -1131) {
        v = -v - 108;
        w8(251 + v / 256);
        w8(v % 256);
    } else if (v >= -32768 && v <= 32767) {
        w8(28);
        w8((v >> 8) & 0xFF);
        w8(v & 0xFF);
    } else {
        w8(29);
        for (int s = 24; s >= 0; s -= 8) w8((v >> s) & 0xFF);
    }
}

// Rebuild a DICT from parsed items. `overrides` maps (opKey, operand slot) to
// a replacement integer — the rewritten offset operands are emitted in the
// fixed 5-byte 29-form so the rebuilt DICT's length is value-independent
// (this is what makes the two-pass layout below consistent: the placeholder
// pass learns the exact lengths the final pass will produce).
QByteArray rebuildCffDict(const std::vector<CffDictItem>& items,
                          const std::map<uint64_t, int32_t>& overrides)
{
    QByteArray out;
    const auto appendInt32 = [&out](int32_t v) {
        out.append(static_cast<char>(29));
        for (int s = 24; s >= 0; s -= 8)
            out.append(static_cast<char>((v >> s) & 0xFF));
    };
    for (const auto& item : items) {
        for (size_t slot = 0; slot < item.operands.size(); ++slot) {
            const uint64_t key = (static_cast<uint64_t>(item.opKey()) << 32) | slot;
            const auto it = overrides.find(key);
            if (it != overrides.end()) {
                appendInt32(it->second);
                continue;
            }
            const CffOperand& o = item.operands[slot];
            if (o.isReal)
                out.append(o.raw.data(), static_cast<qsizetype>(o.raw.size()));
            else
                appendDictInt(out, o.value);
        }
        out.append(item.opBytes.data(), static_cast<qsizetype>(item.opBytes.size()));
    }
    return out;
}

// Locate one Top DICT / Font DICT operand override key. Returns false when
// the operator is absent — the caller must refuse the rewrite rather than
// silently leave a stale offset behind.
bool overrideKeyFor(const std::vector<CffDictItem>& items, uint32_t opKey,
                    size_t slot, uint64_t& keyOut)
{
    for (const auto& item : items) {
        if (item.opKey() == opKey && item.operands.size() > slot) {
            keyOut = (static_cast<uint64_t>(item.opKey()) << 32) | slot;
            return true;
        }
    }
    return false;
}

// Override key for a KNOWN item (the caller already matched the operator).
uint64_t overrideKey(const CffDictItem& item, size_t slot)
{
    return (static_cast<uint64_t>(item.opKey()) << 32) | slot;
}

// charset: fills gidToId (size numGlyphs; SIDs, or CIDs when CID-keyed).
bool parseCffCharset(const unsigned char* data, size_t size, uint32_t off,
                     uint32_t numGlyphs, std::vector<uint32_t>& gidToId)
{
    gidToId.assign(numGlyphs, 0);
    if (off == 0) { // predefined ISOAdobe: the identity mapping
        for (uint32_t g = 0; g < numGlyphs; ++g) gidToId[g] = g;
        return true;
    }
    if (off == 1 || off == 2) return false; // Expert/ExpertSubset — not modeled
    if (off >= size) return false;
    const uint8_t fmt = data[off];
    if (fmt == 0) {
        if (numGlyphs == 0) return false;
        const uint64_t need = 1ull + (numGlyphs - 1ull) * 2;
        if (off + need > size) return false;
        for (uint32_t g = 1; g < numGlyphs; ++g)
            gidToId[g] = rd16(data + off + 1 + (g - 1) * 2);
        return true;
    }
    if (fmt == 1 || fmt == 2) {
        uint32_t p = off + 1;
        uint32_t gid = 1;
        while (gid < numGlyphs) {
            const uint32_t recLen = fmt == 1 ? 3u : 4u;
            if (p + recLen > size) return false;
            const uint32_t first = rd16(data + p);
            const uint32_t nLeft = fmt == 1 ? data[p + 2] : rd16(data + p + 2);
            p += recLen;
            for (uint32_t k = 0; k <= nLeft && gid < numGlyphs; ++k, ++gid)
                gidToId[gid] = first + k;
        }
        return true;
    }
    return false;
}

// Custom encoding (formats 0/1, no supplements): code → glyph ID directly.
bool parseCffEncodingCustom(const unsigned char* data, size_t size, uint32_t off,
                            std::map<uint32_t, uint32_t>& codeToGid)
{
    if (off < 3 || off >= size) return false;
    const uint8_t fmt = data[off];
    if (fmt & 0x80) return false; // supplements not modeled
    if ((fmt & 0x7F) == 0) {
        if (off + 2 > size) return false;
        const uint32_t nCodes = data[off + 1];
        if (off + 2 + nCodes > size) return false;
        uint32_t gid = 1; // glyph IDs are assigned from 1 across the code array
        for (uint32_t i = 0; i < nCodes; ++i, ++gid) {
            const uint32_t code = data[off + 2 + i];
            if (code != 0) codeToGid[code] = gid; // code 0 = unencoded
        }
        return true;
    }
    if ((fmt & 0x7F) == 1) {
        if (off + 2 > size) return false;
        const uint32_t nRanges = data[off + 1];
        uint32_t p = off + 2;
        uint32_t gid = 1;
        for (uint32_t r = 0; r < nRanges; ++r) {
            if (p + 2 > size) return false;
            uint32_t code = data[p];
            const uint32_t nLeft = data[p + 1];
            p += 2;
            for (uint32_t k = 0; k <= nLeft; ++k, ++code, ++gid)
                if (code != 0) codeToGid[code] = gid;
        }
        return true;
    }
    return false;
}

// FDSelect length + basic sanity (kept verbatim; validated so the coverage
// tiling below is meaningful).
bool parseCffFdSelect(const unsigned char* data, size_t size, uint32_t off,
                      uint32_t numGlyphs, uint32_t& lenOut)
{
    if (off >= size) return false;
    const uint8_t fmt = data[off];
    if (fmt == 0) {
        lenOut = 1 + numGlyphs;
        return off + lenOut <= size;
    }
    if (fmt == 3) {
        if (off + 4 > size) return false;
        const uint32_t nRanges = rd16(data + off + 1);
        // fmt(1) + nRanges(2) + nRanges*{first(2),fd(1)} + sentinel(2).
        const uint64_t need = 5ull + nRanges * 3ull;
        lenOut = static_cast<uint32_t>(need);
        if (off + need > size) return false;
        if (nRanges == 0) return false;
        uint32_t prev = rd16(data + off + 3); // the first range's start (== 0)
        if (prev != 0) return false;
        for (uint32_t r = 0; r < nRanges; ++r) {
            const uint32_t first = rd16(data + off + 3 + r * 3);
            if (first < prev) return false; // ascending
            prev = first;
        }
        const uint32_t sentinel = rd16(data + off + 3 + nRanges * 3);
        return sentinel == numGlyphs;
    }
    return false; // format 4 is CFF2 territory
}

// Type 2 charstring token scan for the seac form of endchar (4-5 numbers
// immediately before it). Type 2 token space DIFFERS from DICT space: 29 is
// callgsubr, 30/31 are curve operators, 255 is a 5-byte fixed number.
enum class SeacScan { Clean, Seac, Malformed };

SeacScan scanType2ForSeac(const unsigned char* cs, uint32_t len)
{
    uint32_t p = 0;
    int totalHints = 0;
    int trailing = 0; // number tokens since the last operator
    while (p < len) {
        const uint8_t b0 = cs[p];
        if (b0 == 28) {
            if (p + 3 > len) return SeacScan::Malformed;
            p += 3;
            ++trailing;
        } else if (b0 == 255) {
            if (p + 5 > len) return SeacScan::Malformed;
            p += 5;
            ++trailing;
        } else if (b0 >= 32 && b0 <= 246) {
            p += 1;
            ++trailing;
        } else if (b0 >= 247 && b0 <= 254) {
            if (p + 2 > len) return SeacScan::Malformed;
            p += 2;
            ++trailing;
        } else if (b0 <= 27 || b0 == 30 || b0 == 31) { // operators (28/255 are numbers)
            uint32_t op = b0;
            ++p;
            if (b0 == 12) {
                if (p >= len) return SeacScan::Malformed;
                op = 1000 + cs[p];
                ++p;
                if (op != 1000 + 35 && op != 1000 + 36 && op != 1000 + 37)
                    return SeacScan::Malformed; // only the flex operators exist
            } else if (b0 == 0 || b0 == 2 || b0 == 9 || b0 == 13 || b0 == 15
                       || b0 == 16 || b0 == 17 || b0 == 29) {
                return SeacScan::Malformed; // reserved in Type 2
            }
            if (op == 14) { // endchar
                if (trailing >= 4 && trailing <= 5) return SeacScan::Seac;
                if (trailing >= 2) return SeacScan::Malformed;
                return p == len ? SeacScan::Clean : SeacScan::Malformed;
            }
            if (op == 1 || op == 3 || op == 18 || op == 23) {
                totalHints += trailing / 2; // pairs; a leading odd token is width
            } else if (op == 19 || op == 20) { // hintmask / cntrmask
                totalHints += trailing / 2;
                const uint32_t maskLen = (static_cast<uint32_t>(totalHints) + 7) / 8;
                if (p + maskLen > len) return SeacScan::Malformed;
                p += maskLen;
            }
            trailing = 0;
        } else {
            return SeacScan::Malformed; // unreachable byte pattern
        }
    }
    return SeacScan::Clean; // no endchar — no seac in this program
}

// Assemble an INDEX from per-entry lengths; `concatenated` holds the entry
// bytes back-to-back in entry order.
QByteArray buildCffIndex(const std::vector<uint32_t>& lens,
                         const QByteArray& concatenated)
{
    uint32_t total = 1;
    for (uint32_t l : lens) total += l;
    int offSize = 1;
    // 64-bit guard: 256u << 24 wraps to 0 in 32-bit, looping forever on
    // programs over 16 MB (real CJK CFF programs reach that size).
    while (offSize < 4 && total > (256ull << (8 * (offSize - 1)))) ++offSize;
    QByteArray out;
    const uint32_t count = static_cast<uint32_t>(lens.size());
    out.append(static_cast<char>((count >> 8) & 0xFF));
    out.append(static_cast<char>(count & 0xFF));
    out.append(static_cast<char>(offSize));
    uint32_t acc = 1;
    for (size_t i = 0; i <= lens.size(); ++i) {
        for (int s = (offSize - 1) * 8; s >= 0; s -= 8)
            out.append(static_cast<char>((acc >> s) & 0xFF));
        if (i < lens.size()) acc += lens[i];
    }
    out.append(concatenated);
    return out;
}

} // namespace (CFF internals)

bool parseSfntDirectoryEx(const unsigned char* data, size_t size,
                          QVector<SfntTableEntry>& tablesOut, bool allowOtto)
{
    tablesOut.clear();
    if (data == nullptr || size < 12) return false;
    const uint32_t version = rd32(data);
    if (version != kSfntTrueType1 && version != kSfntTrueType2
        && !(allowOtto && version == kOttoTag))
        return false; // 'ttcf' collections (and CFF, unless asked) never enter
    const uint16_t numTables = rd16(data + 4);
    if (numTables == 0 || numTables > 512) return false;
    const size_t dirEnd = 12 + static_cast<size_t>(numTables) * 16;
    if (size < dirEnd) return false;
    for (uint16_t i = 0; i < numTables; ++i) {
        const unsigned char* rec = data + 12 + static_cast<size_t>(i) * 16;
        SfntTableEntry e;
        e.tag = rd32(rec);
        e.offset = rd32(rec + 8);
        e.length = rd32(rec + 12);
        if (e.offset > size || e.length > size - e.offset) return false;
        tablesOut.append(e);
    }
    return !tablesOut.isEmpty();
}

bool parseCffLayout(const unsigned char* data, size_t size, CffLayout& layOut)
{
    layOut = CffLayout{};
    if (data == nullptr || size < 4) return false;
    if (data[0] != 1) return false; // major version 1 only (CFF2 is not modeled)
    const uint32_t hdrSize = data[2];
    if (hdrSize < 4 || hdrSize > size) return false;
    layOut.header = { 0, hdrSize };

    // The four fixed sections follow the header in spec order.
    CffIndex name, topDict, strings, gsubrs;
    if (!parseCffIndex(data, size, hdrSize, name)) return false;
    if (!parseCffIndex(data, size, name.end, topDict)) return false;
    if (!parseCffIndex(data, size, topDict.end, strings)) return false;
    if (!parseCffIndex(data, size, strings.end, gsubrs)) return false;
    layOut.nameIndex = { name.start, name.end - name.start };
    layOut.topDictIndex = { topDict.start, topDict.end - topDict.start };
    layOut.stringIndex = { strings.start, strings.end - strings.start };
    layOut.gsubrIndex = { gsubrs.start, gsubrs.end - gsubrs.start };

    // Exactly one Top DICT entry.
    if (topDict.count != 1) return false;
    std::vector<CffDictItem> topItems;
    if (!parseCffDict(data, size, topDict.entryStart(0), topDict.entryEnd(0),
                      topItems))
        return false;

    uint32_t charsetOff = 0, encodingOff = 0, charStringsOff = 0;
    uint32_t privateSize = 0, privateOff = 0;
    bool hasPrivate = false;
    uint32_t fdArrayOff = 0, fdSelectOff = 0;
    int topPrivateCount = 0;
    for (const auto& item : topItems) {
        const auto key = item.opKey();
        if (key == 15 && item.operands.size() == 1) charsetOff = item.operands[0].value;
        else if (key == 16 && item.operands.size() == 1) encodingOff = item.operands[0].value;
        else if (key == 17 && item.operands.size() == 1) charStringsOff = item.operands[0].value;
        else if (key == 18 && item.operands.size() == 2) {
            privateSize = item.operands[0].value;
            privateOff = item.operands[1].value;
            hasPrivate = true;
            ++topPrivateCount;
        }
        else if (key == 0x1200 + 36 && item.operands.size() == 1) fdArrayOff = item.operands[0].value;
        else if (key == 0x1200 + 37 && item.operands.size() == 1) fdSelectOff = item.operands[0].value;
        else if (key == 0x1200 + 30) layOut.cidKeyed = true; // ROS
        else if (key == 0x1200 + 7) return false; // FontMatrix — the transformed
        // glyph space is not modeled (sub-FontMatrix programs are never rewritten)
    }
    if (topPrivateCount > 1) return false; // one Private per DICT (the rebuild pairs them 1:1)
    layOut.charsetOffset = charsetOff;
    layOut.encodingOffset = encodingOff;
    if (charStringsOff == 0) return false; // CharStrings is required

    // CharStrings INDEX (its count IS numGlyphs).
    CffIndex csIx;
    if (!parseCffIndex(data, size, charStringsOff, csIx)) return false;
    if (csIx.count == 0) return false;
    layOut.numGlyphs = csIx.count;
    layOut.charStrings = { csIx.start, csIx.end - csIx.start };

    // FDArray: INDEX of Font DICTs, each carrying its own Private reference.
    std::vector<CffPrivateSection>& privates = layOut.privates;
    if (fdArrayOff != 0) {
        CffIndex fdIx;
        if (!parseCffIndex(data, size, fdArrayOff, fdIx)) return false;
        layOut.hasFdArray = true;
        layOut.fdArray = { fdIx.start, fdIx.end - fdIx.start };
        for (uint32_t i = 0; i < fdIx.count; ++i) {
            std::vector<CffDictItem> fdItems;
            if (!parseCffDict(data, size, fdIx.entryStart(i), fdIx.entryEnd(i),
                              fdItems))
                return false;
            for (const auto& item : fdItems) {
                if (item.opKey() == 0x1200 + 7) return false; // FontMatrix in a
                    // Font DICT — not modeled (and not allowed for CIDFonts)
                if (item.opKey() == 18 && item.operands.size() == 2) {
                    CffPrivateSection priv;
                    priv.dict = { static_cast<uint32_t>(item.operands[1].value),
                                  static_cast<uint32_t>(item.operands[0].value) };
                    if (priv.dict.offset > size
                        || priv.dict.length > size - priv.dict.offset)
                        return false;
                    privates.push_back(priv);
                }
            }
        }
    } else if (hasPrivate) {
        CffPrivateSection priv;
        priv.dict = { privateOff, privateSize };
        if (priv.dict.offset > size
            || priv.dict.length > size - priv.dict.offset)
            return false;
        privates.push_back(priv);
    }
    if (fdArrayOff != 0 && hasPrivate) return false; // unmodeled combination
    if (privates.empty() && !layOut.cidKeyed) {
        // A Private DICT is optional in principle, but a CFF without any is
        // exotic — refuse rather than guess.
        return false;
    }

    // Private DICTs: resolve Subrs (offset relative to the DICT's own start);
    // the Subrs INDEX must sit DIRECTLY after its Private DICT so the pair
    // can be moved verbatim as one block.
    for (auto& priv : privates) {
        std::vector<CffDictItem> privItems;
        if (!parseCffDict(data, size, priv.dict.offset,
                          priv.dict.offset + priv.dict.length, privItems))
            return false;
        for (const auto& item : privItems) {
            if (item.opKey() == 19 && item.operands.size() == 1) {
                const uint32_t subrsOff =
                    priv.dict.offset + static_cast<uint32_t>(item.operands[0].value);
                if (subrsOff != priv.dict.offset + priv.dict.length) return false;
                CffIndex subIx;
                if (!parseCffIndex(data, size, subrsOff, subIx)) return false;
                priv.hasSubrs = true;
                priv.subrs = { subIx.start, subIx.end - subIx.start };
            }
        }
    }

    // Charset (defaults to the predefined ISOAdobe identity when absent).
    if (charsetOff != 0) {
        std::vector<uint32_t> gidToId;
        if (!parseCffCharset(data, size, charsetOff, layOut.numGlyphs, gidToId))
            return false;
        layOut.hasCharset = charsetOff > 2;
    } else if (layOut.cidKeyed) {
        // A CID-keyed CFF without any charset cannot map CIDs → glyphs.
        return false;
    }
    if (charsetOff > 2) {
        if (charsetOff >= size) return false;
        layOut.charset = { charsetOff, 0 };
        std::vector<uint32_t> gidToId;
        if (!parseCffCharset(data, size, charsetOff, layOut.numGlyphs, gidToId))
            return false;
        // Recompute the section length from the parsed format.
        const uint8_t fmt = data[charsetOff];
        if (fmt == 0)
            layOut.charset.length = 1 + (layOut.numGlyphs - 1) * 2;
        else {
            // Walk ranges again to find the end offset.
            uint32_t p = charsetOff + 1;
            uint32_t gid = 1;
            const uint8_t f = fmt;
            while (gid < layOut.numGlyphs) {
                const uint32_t recLen = f == 1 ? 3u : 4u;
                if (p + recLen > size) return false;
                const uint32_t nLeft = f == 1 ? data[p + 2] : rd16(data + p + 2);
                p += recLen;
                gid += nLeft + 1;
            }
            layOut.charset.length = p - charsetOff;
        }
    }

    // Encoding.
    if (encodingOff > 2) {
        if (encodingOff >= size) return false;
        std::map<uint32_t, uint32_t> codeToGid;
        if (!parseCffEncodingCustom(data, size, encodingOff, codeToGid))
            return false;
        layOut.hasEncoding = true;
        // Section length: re-walk the format.
        const uint8_t fmt = data[encodingOff] & 0x7F;
        if (fmt == 0) {
            layOut.encoding = { encodingOff, 2 + data[encodingOff + 1] };
        } else {
            uint32_t p = encodingOff + 2;
            const uint32_t nRanges = data[encodingOff + 1];
            p += static_cast<uint32_t>(nRanges) * 2;
            layOut.encoding = { encodingOff, p - encodingOff };
        }
    } else if (encodingOff == 1) {
        return false; // predefined Expert Encoding — not modeled
    }

    // FDSelect.
    if (fdSelectOff != 0) {
        uint32_t fdSelectLen = 0;
        if (!parseCffFdSelect(data, size, fdSelectOff, layOut.numGlyphs, fdSelectLen))
            return false;
        layOut.hasFdSelect = true;
        layOut.fdSelect = { fdSelectOff, fdSelectLen };
    }

    // Exact-coverage tiling: every byte must belong to exactly one known
    // section — unknown bytes mean the transform cannot prove the rebuild.
    std::vector<std::pair<uint32_t, uint32_t>> spans;
    spans.push_back({ 0, hdrSize });
    const auto add = [&spans](const CffSection& s) {
        if (s.length > 0) spans.push_back({ s.offset, s.offset + s.length });
    };
    add(layOut.nameIndex);
    add(layOut.topDictIndex);
    add(layOut.stringIndex);
    add(layOut.gsubrIndex);
    add(layOut.charset);
    add(layOut.encoding);
    add(layOut.charStrings);
    add(layOut.fdArray);
    add(layOut.fdSelect);
    for (const auto& priv : privates) {
        add(priv.dict);
        if (priv.hasSubrs) add(priv.subrs);
    }
    std::sort(spans.begin(), spans.end());
    uint32_t cursor = 0;
    for (const auto& [s, e] : spans) {
        if (s != cursor) return false; // gap or overlap
        cursor = e;
    }
    if (cursor != size) return false;

    layOut.ok = true;
    return true;
}

qint64 blankableCffCharstringBytes(const unsigned char* data, size_t size,
                                   const QSet<uint32_t>& keepGids, bool* okOut)
{
    if (okOut) *okOut = false;
    CffLayout lay;
    if (!parseCffLayout(data, size, lay)) return 0;
    CffIndex csIx;
    if (!parseCffIndex(data, size, lay.charStrings.offset, csIx)) return 0;
    qint64 blankable = 0;
    for (uint32_t gid = 0; gid < lay.numGlyphs; ++gid) {
        if (keepGids.contains(gid)) continue;
        blankable += static_cast<qint64>(csIx.entryLength(gid)) - 1;
    }
    if (okOut) *okOut = true;
    return blankable > 0 ? blankable : 0;
}

bool blankUnusedCffCharstrings(const unsigned char* data, size_t size,
                               const QSet<uint32_t>& keepGids, QByteArray& outCff)
{
    outCff.clear();
    CffLayout lay;
    if (!parseCffLayout(data, size, lay)) return false;

    // Seac discipline: a kept charstring in the 4/5-argument endchar form
    // references other glyphs by standard-encoding code — those references
    // cannot be proven into the keep set, so the caller must skip the font.
    // Subrs are scanned too: a kept charstring can end inside one.
    CffIndex csIx;
    if (!parseCffIndex(data, size, lay.charStrings.offset, csIx)) return false;
    for (uint32_t gid = 0; gid < lay.numGlyphs; ++gid) {
        if (!keepGids.contains(gid) || csIx.entryLength(gid) == 0) continue;
        const auto r = scanType2ForSeac(data + csIx.entryStart(gid),
                                        csIx.entryLength(gid));
        if (r != SeacScan::Clean) return false;
    }
    CffIndex gsubrIx;
    if (!parseCffIndex(data, size, lay.gsubrIndex.offset, gsubrIx)) return false;
    for (uint32_t i = 0; i < gsubrIx.count; ++i) {
        const auto r = scanType2ForSeac(data + gsubrIx.entryStart(i),
                                        gsubrIx.entryLength(i));
        if (r != SeacScan::Clean) return false;
    }
    for (const auto& priv : lay.privates) {
        if (!priv.hasSubrs) continue;
        CffIndex subIx;
        if (!parseCffIndex(data, size, priv.subrs.offset, subIx)) return false;
        for (uint32_t i = 0; i < subIx.count; ++i) {
            const auto r = scanType2ForSeac(data + subIx.entryStart(i),
                                            subIx.entryLength(i));
            if (r != SeacScan::Clean) return false;
        }
    }

    // New CharStrings INDEX: kept bytes verbatim in GID order, everyone else
    // a single endchar. Blanked entries contribute their endchar BYTE to the
    // data stream — buildCffIndex derives the offsets from newLens, so
    // sum(newLens) must equal kept.size() or the INDEX overruns its data.
    static const char kEndchar = static_cast<char>(0x0E);
    QByteArray kept;
    kept.reserve(static_cast<qsizetype>(lay.charStrings.length));
    std::vector<uint32_t> newLens(lay.numGlyphs, 1);
    for (uint32_t gid = 0; gid < lay.numGlyphs; ++gid) {
        if (!keepGids.contains(gid)) {
            kept.append(kEndchar); // bare endchar — the blanked charstring
            continue;
        }
        const uint32_t len = csIx.entryLength(gid);
        newLens[gid] = len;
        kept.append(reinterpret_cast<const char*>(data) + csIx.entryStart(gid),
                    static_cast<qsizetype>(len));
    }
    const QByteArray newCharStrings = buildCffIndex(newLens, kept);

    // Parse the DICTs that get rebuilt (fixed-width re-encoding keeps their
    // rebuilt lengths independent of the final offset values).
    CffIndex topDictIx;
    if (!parseCffIndex(data, size, lay.topDictIndex.offset, topDictIx)) return false;
    std::vector<CffDictItem> topItems;
    if (!parseCffDict(data, size, topDictIx.entryStart(0), topDictIx.entryEnd(0),
                      topItems))
        return false;

    // Rebuilt FDArray: same Font DICT tokens, Private offsets overridden.
    QByteArray newFdArray;
    std::vector<std::vector<CffDictItem>> fdItems;
    if (lay.hasFdArray) {
        CffIndex fdIx;
        if (!parseCffIndex(data, size, lay.fdArray.offset, fdIx)) return false;
        QByteArray fdConcatenated;
        std::vector<uint32_t> fdLens;
        for (uint32_t i = 0; i < fdIx.count; ++i) {
            std::vector<CffDictItem> items;
            if (!parseCffDict(data, size, fdIx.entryStart(i), fdIx.entryEnd(i),
                              items))
                return false;
            fdItems.push_back(std::move(items));
        }
        // Placeholder pass to learn the fixed length; real values in pass C.
        // Overridden operands re-encode in the fixed 29-form, so the length
        // is independent of the final offset values.
        for (auto& items : fdItems) {
            std::map<uint64_t, int32_t> zeroed;
            for (const auto& item : items)
                if (item.opKey() == 18 && item.operands.size() == 2)
                    zeroed[overrideKey(item, 1)] = 0; // offset slot
            const QByteArray body = rebuildCffDict(items, zeroed);
            fdConcatenated.append(body);
            fdLens.push_back(static_cast<uint32_t>(body.size()));
        }
        newFdArray = buildCffIndex(fdLens, fdConcatenated); // length only
    }

    // Pass A: section lengths. Pass B: offsets. Pass C: content.
    struct Section {
        uint32_t sortKey;      // original offset (emit in original order)
        QByteArray content;    // final bytes (verbatim copy or rebuilt)
        uint32_t newOffset = 0;
        bool isCharset = false, isEncoding = false, isCharStrings = false,
             isFdArray = false, isFdSelect = false, isPrivateDict = false;
        size_t privateIdx = 0;
    };
    const auto verbatim = [&](const CffSection& s) {
        QByteArray b;
        if (s.length > 0)
            b.append(reinterpret_cast<const char*>(data) + s.offset,
                     static_cast<qsizetype>(s.length));
        return b;
    };
    std::vector<Section> sections;
    sections.push_back({ 0, verbatim(lay.header), 0 });
    sections.push_back({ lay.nameIndex.offset, verbatim(lay.nameIndex), 0 });
    sections.push_back({ lay.topDictIndex.offset, QByteArray(), 0 }); // rebuilt below
    sections.push_back({ lay.stringIndex.offset, verbatim(lay.stringIndex), 0 });
    sections.push_back({ lay.gsubrIndex.offset, verbatim(lay.gsubrIndex), 0 });
    if (lay.hasFdSelect)
        sections.push_back({ lay.fdSelect.offset, verbatim(lay.fdSelect), 0, false, false, false, false, true });
    if (lay.hasCharset)
        sections.push_back({ lay.charset.offset, verbatim(lay.charset), 0, true });
    if (lay.hasEncoding)
        sections.push_back({ lay.encoding.offset, verbatim(lay.encoding), 0, false, true });
    if (lay.hasFdArray)
        sections.push_back({ lay.fdArray.offset, QByteArray(), 0, false, false, false, true });
    sections.push_back({ lay.charStrings.offset, newCharStrings, 0, false, false, true });
    for (size_t i = 0; i < lay.privates.size(); ++i) {
        QByteArray block = verbatim(lay.privates[i].dict);
        if (lay.privates[i].hasSubrs) block += verbatim(lay.privates[i].subrs);
        sections.push_back({ lay.privates[i].dict.offset, std::move(block), 0,
                             false, false, false, false, false, true, i });
    }
    std::stable_sort(sections.begin(), sections.end(),
                     [](const Section& a, const Section& b) {
                         return a.sortKey < b.sortKey;
                     });

    // Pass A/B: total layout with placeholder topdict/fdarray content (their
    // rebuilt lengths are value-independent thanks to the 29-form overrides).
    {
        std::map<uint64_t, int32_t> zeroed;
        for (const auto& item : topItems) {
            const auto key = item.opKey();
            if ((key == 15 || key == 16 || key == 17 || key == 0x1200 + 36
                 || key == 0x1200 + 37) && item.operands.size() == 1)
                zeroed[overrideKey(item, 0)] = 0;
            else if (key == 18 && item.operands.size() == 2)
                zeroed[overrideKey(item, 1)] = 0; // offset slot; size stays
        }
        const QByteArray placeholderTop = rebuildCffDict(topItems, zeroed);
        QByteArray topIndex;
        {
            std::vector<uint32_t> lens{ static_cast<uint32_t>(placeholderTop.size()) };
            topIndex = buildCffIndex(lens, placeholderTop);
        }
        uint32_t cursor = 0;
        for (auto& sec : sections) {
            uint32_t len;
            if (sec.sortKey == lay.topDictIndex.offset) {
                len = static_cast<uint32_t>(topIndex.size());
                sec.content = topIndex; // placeholder; replaced in pass C
            } else if (lay.hasFdArray && sec.sortKey == lay.fdArray.offset) {
                len = static_cast<uint32_t>(newFdArray.size());
                sec.content = newFdArray;
            } else {
                len = static_cast<uint32_t>(sec.content.size());
            }
            sec.newOffset = cursor;
            cursor += len;
        }
    }

    // Pass C: real topdict / fdarray content with the final offsets.
    std::map<uint64_t, int32_t> topOverrides;
    {
        const auto setTop = [&](uint32_t opKey, size_t slot, int32_t off) {
            uint64_t key = 0;
            if (!overrideKeyFor(topItems, opKey, slot, key)) return false;
            topOverrides[key] = off;
            return true;
        };
        for (const auto& sec : sections) {
            const int32_t off = static_cast<int32_t>(sec.newOffset);
            if (sec.isCharset) {
                if (!setTop(15, 0, off)) return false;
            } else if (sec.isEncoding) {
                if (!setTop(16, 0, off)) return false;
            } else if (sec.isCharStrings) {
                if (!setTop(17, 0, off)) return false;
            } else if (sec.isFdSelect) {
                if (!setTop(0x1200 + 37, 0, off)) return false;
            } else if (sec.isFdArray) {
                if (!setTop(0x1200 + 36, 0, off)) return false;
            } else if (sec.isPrivateDict && !lay.hasFdArray) {
                // Top-level Private (non-CID): the DICT's offset slot moves.
                if (!setTop(18, 1, off)) return false;
            }
            // FDArray-held Private DICTs are patched in the FDArray rebuild
            // below — NOT in the Top DICT (their owner is a Font DICT).
        }
    }

    // Final FDArray: same Font DICT tokens, Private offsets overridden with
    // the private sections' new offsets (parse pushed privates in Font DICT
    // order, so a running index pairs each op-18 item with its section).
    QByteArray finalFdArray;
    if (lay.hasFdArray) {
        std::map<size_t, int32_t> privOffsets;
        for (const auto& sec : sections)
            if (sec.isPrivateDict)
                privOffsets.emplace(sec.privateIdx,
                                    static_cast<int32_t>(sec.newOffset));
        size_t privIdx = 0;
        QByteArray fdConcatenated;
        std::vector<uint32_t> fdLens;
        for (auto& items : fdItems) {
            std::map<uint64_t, int32_t> real;
            for (const auto& item : items) {
                if (item.opKey() != 18 || item.operands.size() != 2) continue;
                const auto pit = privOffsets.find(privIdx++);
                if (pit == privOffsets.end()) return false; // parse/rebuild skew
                real[overrideKey(item, 1)] = pit->second;
            }
            const QByteArray body = rebuildCffDict(items, real);
            fdConcatenated.append(body);
            fdLens.push_back(static_cast<uint32_t>(body.size()));
        }
        if (privIdx != lay.privates.size()) return false; // parse/rebuild skew
        finalFdArray = buildCffIndex(fdLens, fdConcatenated);
    }

    // Final Top DICT INDEX and the FDArray replace their placeholder
    // contents; the newOffsets stay valid because every overridden operand
    // re-encodes at the same fixed width.
    QByteArray finalTopIndex;
    {
        const QByteArray topBody = rebuildCffDict(topItems, topOverrides);
        finalTopIndex = buildCffIndex({ static_cast<uint32_t>(topBody.size()) },
                                      topBody);
    }
    for (auto& sec : sections) {
        if (sec.sortKey == lay.topDictIndex.offset)
            sec.content = finalTopIndex;
        else if (lay.hasFdArray && sec.sortKey == lay.fdArray.offset)
            sec.content = finalFdArray;
    }

    // Assemble: sections in original layout order tile the output exactly.
    QByteArray out;
    for (const auto& sec : sections) out.append(sec.content);
    if (static_cast<size_t>(out.size()) != sections.back().newOffset
                                       + sections.back().content.size())
        return false; // internal layout skew — refuse rather than corrupt
    outCff = out;
    return true;
}

// ── Adobe Standard Encoding, ASCII span ───────────────────────────────────────
// Codes 32..126 of the predefined Adobe Standard Encoding; CFF standard-string
// SIDs 1..95 correspond 1:1 to these codes (SID = code - 31). The names were
// cross-checked against fontTools' cffStandardStrings (SIDs 1..95) and the
// StandardEncoding table of PDF 32000 Annex D.
const std::array<const char*, 95>& stdEncAsciiNames() {
    static const std::array<const char*, 95> kNames = {
        "space", "exclam", "quotedbl", "numbersign", "dollar", "percent",
        "ampersand", "quoteright", "parenleft", "parenright", "asterisk",
        "plus", "comma", "hyphen", "period", "slash", "zero", "one", "two",
        "three", "four", "five", "six", "seven", "eight", "nine", "colon",
        "semicolon", "less", "equal", "greater", "question", "at",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N",
        "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
        "bracketleft", "backslash", "bracketright", "asciicircum", "underscore",
        "quoteleft",
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n",
        "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z",
        "braceleft", "bar", "braceright", "asciitilde"
    };
    return kNames;
}

// Inverse charset: SID/CID → first glyph carrying it.
bool cffCharsetInverse(const unsigned char* data, size_t size, uint32_t charsetOff,
                       uint32_t numGlyphs,
                       std::map<uint32_t, uint32_t>& idToGidOut)
{
    if (charsetOff == 1 || charsetOff == 2) return false; // Expert tables — not modeled
    std::vector<uint32_t> gidToId;
    if (!parseCffCharset(data, size, charsetOff, numGlyphs, gidToId)) return false;
    for (uint32_t g = 0; g < numGlyphs; ++g)
        idToGidOut.emplace(gidToId[g], g); // first glyph wins (deterministic)
    return true;
}

bool cffCidsToGids(const unsigned char* data, size_t size,
                   const QSet<uint32_t>& cids, QSet<uint32_t>& gidsOut)
{
    gidsOut.clear();
    CffLayout lay;
    if (!parseCffLayout(data, size, lay)) return false;
    if (!lay.cidKeyed)
        return false; // a name-keyed CFF proves nothing about CID usage
    std::map<uint32_t, uint32_t> cidToGid;
    if (!cffCharsetInverse(data, size, lay.charsetOffset, lay.numGlyphs, cidToGid))
        return false;
    gidsOut.insert(0); // .notdef always survives
    for (uint32_t cid : cids) {
        const auto it = cidToGid.find(cid);
        if (it == cidToGid.end())
            return false; // a shown CID with no glyph — never guess
        gidsOut.insert(it->second);
    }
    return true;
}

bool cffCodeToGidBuiltIn(const unsigned char* data, size_t size,
                         uint32_t code, uint32_t& gidOut)
{
    CffLayout lay;
    if (!parseCffLayout(data, size, lay)) return false;
    if (lay.cidKeyed || lay.encodingOffset == 1)
        return false; // encodings are not consulted for CID-keyed; Expert not modeled
    std::map<uint32_t, uint32_t> sidToGid;
    if (!cffCharsetInverse(data, size, lay.charsetOffset, lay.numGlyphs, sidToGid))
        return false;
    if (lay.encodingOffset > 2) { // custom encoding section: codes → GIDs directly
        std::map<uint32_t, uint32_t> codeToGid;
        if (!parseCffEncodingCustom(data, size, lay.encodingOffset, codeToGid))
            return false;
        const auto it = codeToGid.find(code);
        gidOut = it != codeToGid.end() ? it->second : 0u; // absent → .notdef (kept)
        return true;
    }
    // Predefined Adobe Standard Encoding (operand 0), proven for ASCII 32..126:
    // SID = code - 31 (standard strings 1..95). Codes outside the span are
    // proven-unencoded — a conforming reader draws .notdef, which survives.
    if (code < 32 || code > 126) {
        gidOut = 0;
        return true;
    }
    const auto it = sidToGid.find(code - 31);
    gidOut = it != sidToGid.end() ? it->second : 0u; // SID absent → .notdef
    return true;
}

bool cffNameToGid(const unsigned char* data, size_t size,
                  const std::string& name, uint16_t& gidOut)
{
    CffLayout lay;
    if (!parseCffLayout(data, size, lay)) return false;
    if (lay.cidKeyed) return false; // glyph names are not resolvable here
    std::map<uint32_t, uint32_t> sidToGid;
    if (!cffCharsetInverse(data, size, lay.charsetOffset, lay.numGlyphs, sidToGid))
        return false;

    // name → SID: (a) a standard string of the ASCII StandardEncoding span,
    // (b) a String INDEX custom string (SID = 391 + entry index). Anything
    // else cannot be proven — the caller skips the font.
    int64_t sid = -1;
    if (name.size() == 1) { // fast path: single-letter names dominate Differences
        const uint32_t code = static_cast<unsigned char>(name[0]);
        if (code >= 32 && code <= 126) sid = static_cast<int64_t>(code) - 31;
    }
    if (sid < 0) {
        const auto& names = stdEncAsciiNames();
        for (size_t i = 0; i < names.size(); ++i) {
            if (name == names[i]) {
                sid = static_cast<int64_t>(i) + 1; // SIDs 1..95
                break;
            }
        }
    }
    if (sid < 0) {
        CffIndex strIx;
        if (!parseCffIndex(data, size, lay.stringIndex.offset, strIx)) return false;
        for (uint32_t i = 0; i < strIx.count; ++i) {
            const uint32_t s = strIx.entryStart(i);
            const uint32_t l = strIx.entryLength(i);
            if (l == name.size()
                && std::memcmp(data + s, name.data(), l) == 0) {
                sid = 391 + static_cast<int64_t>(i);
                break;
            }
        }
    }
    if (sid < 0) return false; // name not provably resolvable — caller skips
    const auto it = sidToGid.find(static_cast<uint32_t>(sid));
    // A proven-absent SID draws .notdef (always kept) — gid 0 is honest here.
    gidOut = static_cast<uint16_t>(it != sidToGid.end() ? it->second : 0u);
    return true;
}

bool rebuildSfntReplacingTable(const unsigned char* data, size_t size, uint32_t tag,
                               const QByteArray& replacement, QByteArray& outSfnt)
{
    QVector<SfntTableEntry> tables;
    if (!parseSfntDirectoryEx(data, size, tables, /*allowOtto=*/true)) return false;

    // Output layout: tables in directory order, 4-byte aligned offsets.
    struct OutTable { const SfntTableEntry* in; uint32_t offset; uint32_t length; };
    std::vector<OutTable> outTables;
    outTables.reserve(tables.size());
    uint32_t cursor = 12 + static_cast<uint32_t>(tables.size()) * 16;
    bool replaced = false;
    for (const auto& t : tables) {
        OutTable ot;
        ot.in = &t;
        ot.length = t.length;
        if (t.tag == tag) {
            ot.length = static_cast<uint32_t>(replacement.size());
            replaced = true;
        }
        ot.offset = cursor;
        cursor += ot.length;
        cursor = (cursor + 3u) & ~3u;
        outTables.push_back(ot);
    }
    if (!replaced) return false; // the caller asked to replace an absent table

    QByteArray out(static_cast<qsizetype>(cursor), '\0');
    unsigned char* op = reinterpret_cast<unsigned char*>(out.data());

    // Header: sfnt type and numTables verbatim, search parameters recomputed.
    std::memcpy(op, data, 6);
    uint32_t sr = 1;
    uint16_t entrySelector = 0;
    while (sr * 2 <= tables.size()) { sr *= 2; ++entrySelector; }
    const uint16_t searchRange = static_cast<uint16_t>(sr * 16);
    const uint16_t rangeShift = static_cast<uint16_t>(tables.size() * 16 - searchRange);
    wr16(op + 6, searchRange);
    wr16(op + 8, entrySelector);
    wr16(op + 10, rangeShift);

    ptrdiff_t headSlot = -1;
    for (size_t i = 0; i < tables.size(); ++i) {
        const OutTable& ot = outTables[i];
        unsigned char* rec = op + 12 + i * 16;
        wr32(rec, ot.in->tag);
        wr32(rec + 8, ot.offset);
        wr32(rec + 12, ot.length);
        if (ot.in->tag == tag) {
            std::memcpy(op + ot.offset, replacement.constData(),
                        static_cast<size_t>(replacement.size()));
        } else {
            std::memcpy(op + ot.offset, data + ot.in->offset, ot.length);
            if (ot.in->tag == kTagHead) {
                // checkSumAdjustment zeroed while its own checksum is taken
                // (same ISO 14496-6 discipline as blankUnusedGlyphs).
                std::memset(op + ot.offset + 8, 0, 4);
                headSlot = static_cast<ptrdiff_t>(i);
            }
        }
        wr32(rec + 4, tableChecksum(op + ot.offset, ot.length));
    }
    if (headSlot >= 0) {
        const uint32_t whole = tableChecksum(op, static_cast<size_t>(cursor));
        const uint32_t adjustment = 0xB1B0AFBAu - whole;
        wr32(op + outTables[static_cast<size_t>(headSlot)].offset + 8, adjustment);
    }
    outSfnt = out;
    return true;
}

// The CFF feature gate over a parsed layout: a KEPT charstring must not use
// the seac form of endchar (it references base glyphs by standard-encoding
// code that cannot be proven into the keep set), and every global/local
// subroutine must scan clean too (a kept charstring can end inside one).
enum class CffGate { Clean, Seac, Malformed };

CffGate cffSeacGate(const unsigned char* data, size_t size, const CffLayout& lay,
                    const QSet<uint32_t>& keepGids)
{
    CffIndex csIx;
    if (!parseCffIndex(data, size, lay.charStrings.offset, csIx))
        return CffGate::Malformed;
    for (uint32_t gid = 0; gid < lay.numGlyphs; ++gid) {
        if (!keepGids.contains(gid) || csIx.entryLength(gid) == 0) continue;
        const auto r = scanType2ForSeac(data + csIx.entryStart(gid),
                                        csIx.entryLength(gid));
        if (r == SeacScan::Seac) return CffGate::Seac;
        if (r == SeacScan::Malformed) return CffGate::Malformed;
    }
    CffIndex gsubrIx;
    if (!parseCffIndex(data, size, lay.gsubrIndex.offset, gsubrIx))
        return CffGate::Malformed;
    for (uint32_t i = 0; i < gsubrIx.count; ++i) {
        const auto r = scanType2ForSeac(data + gsubrIx.entryStart(i),
                                        gsubrIx.entryLength(i));
        if (r == SeacScan::Seac) return CffGate::Seac;
        if (r == SeacScan::Malformed) return CffGate::Malformed;
    }
    for (const auto& priv : lay.privates) {
        if (!priv.hasSubrs) continue;
        CffIndex subIx;
        if (!parseCffIndex(data, size, priv.subrs.offset, subIx))
            return CffGate::Malformed;
        for (uint32_t i = 0; i < subIx.count; ++i) {
            const auto r = scanType2ForSeac(data + subIx.entryStart(i),
                                            subIx.entryLength(i));
            if (r == SeacScan::Seac) return CffGate::Seac;
            if (r == SeacScan::Malformed) return CffGate::Malformed;
        }
    }
    return CffGate::Clean;
}


namespace {

using PoDoFo::PdfMemDocument;
using PoDoFo::PdfObject;

// Resolve an indirect reference; nullptr when absent/unresolvable.
PdfObject* resolveRef(PdfMemDocument& doc, PdfObject* obj) {
    if (obj && obj->IsReference())
        obj = doc.GetObjects().GetObject(obj->GetReference());
    return obj;
}

// A font's classification for the keep/blank decision.
enum class FontKind {
    CidTrueType,        // Type0 → CIDFontType2 with /FontFile2 — rewritable
    SimpleTrueType,     // /TrueType with /FontFile2 — rewritable when provable
    CidCffProgram,      // Type0 → CIDFontType0 (Identity-H/-V/-UCS2) over a bare
                        // /FontFile3 /CIDFontType0C — CIDs resolve via the charset
    CffNameProgram,     // simple /Type1 over a bare /FontFile3 /Type1C
    CffOpenTypeProgram, // simple /Type1 or /OpenType over /FontFile3 /OpenType
                        // (OTTO-wrapped CFF; sfnt cmap/post prove GIDs)
    CffProgram,         // /FontFile3 with an unhandled subtype/shape
    Type1Program,       // /FontFile involved (Type1: out of scope)
    Type3Font,          // no font program
    UnknownOrForeign    // non-embedded fonts, exotic dicts
};

struct FontInfo {
    PdfObject* fontDict = nullptr;
    FontKind kind = FontKind::UnknownOrForeign;
    PdfObject* fontFile = nullptr;    // /FontFile2 stream object (rewritable kind)
    bool cidIdentity = true;          // CID: /CIDToGIDMap Identity or absent
    PdfObject* cidToGidStream = nullptr;
    bool encodingUnprovable = false;  // simple: named /Encoding or /BaseEncoding only
    bool requiresDifferences = false; // simple: /Encoding /Differences path
    bool nonDefaultFontMatrix = false; // CFF kinds: a non-default /FontMatrix is
                                      // present on the font dict (sub-FontMatrix
                                      // transforms are never rewritten)
    bool subsetPrefixed = false;      // BaseFont carries an AAAAAA+ tag
    // Populated during the content walk:
    bool seen = false;                // referenced by a Tf in any walked stream
    bool ragged = false;              // CID string with odd byte length
    QSet<uint32_t> codes;             // codes shown with this font
};

// Classify one /Type /Font dictionary.
// /FontMatrix provability for the CFF kinds: absent (or the exact default
// [0.001 0 0 0.001 0 0]) is fine; anything else is a sub-FontMatrix transform
// the lane refuses rather than models. Compared with a small epsilon — PDF
// reals round-trip through double, so a same-literal matrix is exact anyway.
bool fontMatrixIsDefault(const PdfObject* fontDictObj)
{
    const auto* fm = fontDictObj->GetDictionary().FindKey("FontMatrix");
    if (!fm) return true; // absent == the default matrix
    if (!fm->IsArray()) return false; // exotic shape — refuse
    static const double kDefault[6] = { 0.001, 0.0, 0.0, 0.001, 0.0, 0.0 };
    int i = 0;
    for (const auto& el : fm->GetArray()) {
        if (i >= 6 || !el.IsNumberOrReal()) return false;
        if (std::abs(el.GetReal() - kDefault[i]) > 1e-9) return false;
        ++i;
    }
    return i == 6;
}

FontInfo classifyFont(PdfMemDocument& doc, PdfObject* fontDictObj) {
    FontInfo info;
    info.fontDict = fontDictObj;
    if (!fontDictObj || !fontDictObj->IsDictionary()) return info;
    auto& d = fontDictObj->GetDictionary();
    const auto subtype = d.FindKey("Subtype");
    if (!subtype || !subtype->IsName()) return info;
    const std::string_view st = subtype->GetName().GetString();

    if (st == "Type3") { info.kind = FontKind::Type3Font; return info; }

    PdfObject* descriptor = nullptr;
    bool decided = false; // kind + fontFile decided by the branch itself
    if (st == "Type0") {
        PdfObject* descArr = resolveRef(doc, d.FindKey("DescendantFonts"));
        if (!descArr || !descArr->IsArray() || descArr->GetArray().IsEmpty()) return info;
        PdfObject* desc = resolveRef(doc, &descArr->GetArray()[0]);
        if (!desc || !desc->IsDictionary()) return info;
        const auto descSub = desc->GetDictionary().FindKey("Subtype");
        if (!descSub || !descSub->IsName()) return info;
        const std::string_view dst = descSub->GetName().GetString();

        // Code width (both CID kinds): the /Encoding CMap name decides;
        // Identity-H/-V/-UCS2 are 2-byte. A CMap-stream encoding is not
        // consulted — such fonts skip.
        const auto enc = d.FindKey("Encoding");
        if (!enc || !enc->IsName()) return info; // absent or a CMap stream
        const std::string_view e = enc->GetName().GetString();
        if (e != "Identity-H" && e != "Identity-V" && e != "Identity-UCS2")
            return info;

        descriptor = resolveRef(doc, desc->GetDictionary().FindKey("FontDescriptor"));
        if (dst == "CIDFontType2") {
            // CID→GID: Identity (explicit or absent per 32000 9.7.4.2) or a stream.
            const auto cidMap = desc->GetDictionary().FindKey("CIDToGIDMap");
            if (!cidMap || (cidMap->IsName() && cidMap->GetName().GetString() == "Identity")) {
                info.cidIdentity = true;
            } else if (cidMap->HasStream()) {
                info.cidIdentity = false;
                info.cidToGidStream = cidMap;
            } else {
                return info; // neither Identity nor a stream — unknown mapping
            }
            info.kind = FontKind::CidTrueType;
        } else if (dst == "CIDFontType0") {
            // CFF lane: the 2-byte codes ARE CIDs; CID→glyph resolves through
            // the CFF charset's inverse. Only a BARE /CIDFontType0C program is
            // modeled — an OpenType-wrapped CIDFontType0C stays a counted skip.
            PdfObject* ff3 = descriptor && descriptor->IsDictionary()
                ? resolveRef(doc, descriptor->GetDictionary().FindKey("FontFile3"))
                : nullptr;
            if (ff3 && ff3->HasStream()) {
                const auto f3sub = ff3->GetDictionary().FindKey("Subtype");
                if (f3sub && f3sub->IsName()
                    && f3sub->GetName().GetString() == "CIDFontType0C") {
                    info.kind = FontKind::CidCffProgram;
                    info.fontFile = ff3;
                    // A sub-FontMatrix transform on the Type0 wrapper or the
                    // descendant CIDFont is refused before any rewrite.
                    info.nonDefaultFontMatrix =
                        !fontMatrixIsDefault(fontDictObj)
                        || !fontMatrixIsDefault(desc);
                } else {
                    info.kind = FontKind::CffProgram; // wrapped / other subtype
                }
                decided = true;
            } else {
                return info; // non-embedded CIDFontType0 — nothing to rewrite
            }
        } else {
            return info;
        }
    } else if (st == "TrueType") {
        descriptor = resolveRef(doc, d.FindKey("FontDescriptor"));
        info.kind = FontKind::SimpleTrueType;
        // Provable code→GID: no /Encoding at all (the font's built-in cmap,
        // which is exactly what a conforming reader consults), or /Encoding
        // /Differences where every shown code is listed and its glyph NAME
        // resolves through the post table. A bare named /Encoding (or a
        // /BaseEncoding without Differences) shifts bytes 0x80+ — unprovable.
        const auto enc = d.FindKey("Encoding");
        if (!enc) {
            // built-in cmap path
        } else if (enc->IsDictionary() && enc->GetDictionary().FindKey("Differences")) {
            info.requiresDifferences = true;
        } else {
            info.encodingUnprovable = true;
        }
    } else if (st == "Type1" || st == "OpenType") {
        // CFF lane: simple fonts over /FontFile3 — a bare Type1C (built-in CFF
        // encoding or /Differences-through-charset-names) or an OpenType
        // wrapper (sfnt cmap/post prove GIDs; only the 'CFF ' table is
        // rewritten). Same /Encoding provability rule as simple TrueType.
        descriptor = resolveRef(doc, d.FindKey("FontDescriptor"));
        const auto enc = d.FindKey("Encoding");
        if (!enc) {
            // built-in encoding path
        } else if (enc->IsDictionary() && enc->GetDictionary().FindKey("Differences")) {
            info.requiresDifferences = true;
        } else {
            info.encodingUnprovable = true;
        }
        PdfObject* ff3 = descriptor && descriptor->IsDictionary()
            ? resolveRef(doc, descriptor->GetDictionary().FindKey("FontFile3"))
            : nullptr;
        if (ff3 && ff3->HasStream()) {
            const auto f3sub = ff3->GetDictionary().FindKey("Subtype");
            const std::string_view f3st = f3sub && f3sub->IsName()
                ? f3sub->GetName().GetString() : std::string_view();
            if (f3st == "Type1C" && st == "Type1") {
                info.kind = FontKind::CffNameProgram;
                info.fontFile = ff3;
                info.nonDefaultFontMatrix = !fontMatrixIsDefault(fontDictObj);
            } else if (f3st == "OpenType") {
                info.kind = FontKind::CffOpenTypeProgram;
                info.fontFile = ff3;
                info.nonDefaultFontMatrix = !fontMatrixIsDefault(fontDictObj);
            } else {
                info.kind = FontKind::CffProgram; // unhandled subtype pairing
            }
            decided = true;
        } else if (descriptor && descriptor->IsDictionary()
                   && descriptor->GetDictionary().FindKey("FontFile")) {
            info.kind = FontKind::Type1Program; // classic Type1 — out of scope
            decided = true;
        } else {
            return info; // non-embedded — nothing to rewrite
        }
    } else {
        return info; // MMType1 …
    }

    // Subset-prefix detection runs for every classified font dict (the
    // already-subset skip check reads it for any rewritable kind).
    if (const auto baseFont = d.FindKey("BaseFont"); baseFont && baseFont->IsName()) {
        std::string_view name = baseFont->GetName().GetString();
        if (!name.empty() && name.front() == '/') name.remove_prefix(1);
        if (name.size() > 7) {
            bool tagOk = true;
            for (size_t i = 0; i < 6; ++i)
                if (name[i] < 'A' || name[i] > 'Z') { tagOk = false; break; }
            info.subsetPrefixed = tagOk && name[6] == '+';
        }
    }

    if (!decided) {
        // CidTrueType / SimpleTrueType: the embedded program decides.
        if (descriptor && descriptor->IsDictionary()) {
            if (PdfObject* ff2 = resolveRef(doc, descriptor->GetDictionary().FindKey("FontFile2"));
                ff2 && ff2->HasStream()) {
                info.fontFile = ff2;
                return info;
            }
            if (descriptor->GetDictionary().FindKey("FontFile3")) {
                info.kind = FontKind::CffProgram;
                info.fontFile = nullptr;
                return info;
            }
            if (descriptor->GetDictionary().FindKey("FontFile")) {
                info.kind = FontKind::Type1Program;
                info.fontFile = nullptr;
                return info;
            }
        }
        info.kind = FontKind::UnknownOrForeign; // no embedded program to rewrite
        info.fontFile = nullptr;
        return info;
    }
    return info;
}

// One embedded font program and everything known about its usage.
struct ProgramWork {
    enum class ProgKind { Unknown, TrueTypeSfnt, BareCff, OpenTypeCff };
    PdfObject* fontFile = nullptr;   // the /FontFile2 or /FontFile3 stream object
    ProgKind progKind = ProgKind::Unknown;
    QSet<uint32_t> usedGids;         // accumulated across referencing fonts
    bool usable = true;              // false → never rewrite (reason recorded)
    QString skipReason;
    QByteArray cidToGidBytes;        // /CIDToGIDMap stream bytes (when mapped)
    QByteArray decoded;              // decoded (raw) program bytes
    QByteArray encoded;              // current encoded stream bytes
    qint64 encodedSize = 0;
    bool filterOk = false;           // old stream filter ∈ {none, FlateDecode}
    bool oldHadFilter = false;
    QSet<uint32_t> keepGids;         // used ∪ {0} ∪ composite closure
    qint64 blankableBytes = 0;
    uint32_t cffOffset = 0;          // OpenTypeCff: 'CFF ' table slice in decoded
    uint32_t cffLength = 0;
};

// Enumerate the /Font entries of a resource dictionary: name → font dict object.
void fontsOfResources(PdfMemDocument& doc, PdfObject* resources,
                      std::map<std::string, PdfObject*, std::less<>>& out) {
    out.clear();
    if (!resources || !resources->IsDictionary()) return;
    PdfObject* fontDict = resolveRef(doc, resources->GetDictionary().FindKey("Font"));
    if (!fontDict || !fontDict->IsDictionary()) return;
    for (auto& entry : fontDict->GetDictionary()) {
        PdfObject* val = resolveRef(doc, &entry.second);
        if (val && val->IsDictionary())
            out.emplace(std::string(entry.first.GetString()), val);
    }
}

// The resources dictionary of a page, honoring /Pages-tree inheritance.
PdfObject* pageResources(PdfMemDocument& doc, PoDoFo::PdfPage& page) {
    PdfObject* node = &page.GetObject();
    int depth = 0;
    while (node && depth < 64) {
        if (PdfObject* res = resolveRef(doc, node->GetDictionary().FindKey("Resources")))
            return res;
        node = resolveRef(doc, node->GetDictionary().FindKey("Parent"));
        ++depth;
    }
    return nullptr;
}

// One canvas that can carry text operators: its decoded content bytes plus
// the resource dictionary that scopes its /Font names.
struct Canvas {
    PdfObject* resources = nullptr;
    std::string bytes;     // decoded content stream
    bool readable = true;  // false → usage unknown for this canvas's fonts
};

// Collect canvases reachable from one resource dictionary: Form XObjects,
// tiling patterns, and transparency /SMask /G forms (recursively).
void collectResourceCanvases(PdfMemDocument& doc, PdfObject* resources,
                             std::vector<Canvas>& out,
                             QSet<PdfObject*>& visited, int depth) {
    if (!resources || depth > 16) return;
    auto addCanvas = [&](PdfObject* streamObj) {
        if (!streamObj || !streamObj->IsDictionary() || !streamObj->HasStream()
            || visited.contains(streamObj))
            return;
        visited.insert(streamObj);
        Canvas c;
        c.resources = resolveRef(doc, streamObj->GetDictionary().FindKey("Resources"));
        try {
            PoDoFo::charbuff buf;
            streamObj->GetOrCreateStream().CopyTo(buf);
            c.bytes.assign(buf.data(), buf.size());
        } catch (const PoDoFo::PdfError&) {
            c.readable = false;
        }
        out.push_back(c);
        collectResourceCanvases(doc, c.resources, out, visited, depth + 1);
    };
    for (const char* key : { "XObject", "Pattern" }) {
        PdfObject* group = resolveRef(doc, resources->GetDictionary().FindKey(key));
        if (!group || !group->IsDictionary()) continue;
        for (auto& entry : group->GetDictionary()) {
            PdfObject* val = resolveRef(doc, &entry.second);
            if (!val || !val->IsDictionary()) continue;
            if (std::string_view(key) == "XObject") {
                const auto subtype = val->GetDictionary().FindKey("Subtype");
                if (!subtype || !subtype->IsName()
                    || subtype->GetName().GetString() != "Form")
                    continue;
            } else {
                // Tiling patterns (/PatternType 1) carry content streams.
                const auto ptype = val->GetDictionary().FindKey("PatternType");
                if (!ptype || !ptype->IsNumberOrReal()
                    || static_cast<int>(ptype->GetReal()) != 1)
                    continue;
            }
            addCanvas(val);
        }
    }
    // Transparency: /ExtGState << /SMask << /G form-xobject >>> — a canvas.
    PdfObject* extg = resolveRef(doc, resources->GetDictionary().FindKey("ExtGState"));
    if (extg && extg->IsDictionary()) {
        for (auto& entry : extg->GetDictionary()) {
            PdfObject* gs = resolveRef(doc, &entry.second);
            if (!gs || !gs->IsDictionary()) continue;
            PdfObject* smask = resolveRef(doc, gs->GetDictionary().FindKey("SMask"));
            if (!smask || !smask->IsDictionary()) continue;
            addCanvas(resolveRef(doc, smask->GetDictionary().FindKey("G")));
        }
    }
}

// Every canvas in the document: page contents, Form XObjects, tiling
// patterns, transparency groups, annotation appearance streams, and Type3
// CharProcs.
std::vector<Canvas> documentCanvases(PdfMemDocument& doc) {
    std::vector<Canvas> canvases;
    QSet<PdfObject*> visited;
    const unsigned int pc = doc.GetPages().GetCount();
    for (unsigned int pi = 0; pi < pc; ++pi) {
        auto& page = doc.GetPages().GetPageAt(pi);
        PdfObject* res = pageResources(doc, page);

        // Page contents (PdfContents::CopyTo merges a /Contents array).
        Canvas pageCanvas;
        pageCanvas.resources = res;
        PdfObject* contentsKey = page.GetDictionary().FindKey("Contents");
        try {
            if (auto* contents = page.GetContents()) {
                PoDoFo::charbuff buf;
                contents->CopyTo(buf);
                pageCanvas.bytes.assign(buf.data(), buf.size());
            } else if (contentsKey) {
                // Present but unloadable — usage unknown for this page's fonts.
                pageCanvas.readable = false;
            }
        } catch (const PoDoFo::PdfError&) {
            pageCanvas.readable = false; // present but unreadable — usage unknown
        }
        canvases.push_back(pageCanvas);
        collectResourceCanvases(doc, res, canvases, visited, 0);

        // Annotation appearance streams.
        try {
            auto& annos = page.GetAnnotations();
            for (unsigned i = 0; i < annos.GetCount(); ++i) {
                auto& anno = annos.GetAnnotAt(i);
                PdfObject* ap = resolveRef(doc, anno.GetDictionary().FindKey("AP"));
                if (!ap || !ap->IsDictionary()) continue;
                for (const char* key : { "N", "R", "D" }) {
                    PdfObject* stream = resolveRef(doc, ap->GetDictionary().FindKey(key));
                    if (!stream || !stream->IsDictionary() || !stream->HasStream())
                        continue;
                    if (visited.contains(stream)) continue;
                    visited.insert(stream);
                    Canvas c;
                    c.resources = resolveRef(doc,
                        stream->GetDictionary().FindKey("Resources"));
                    try {
                        PoDoFo::charbuff buf;
                        stream->GetOrCreateStream().CopyTo(buf);
                        c.bytes.assign(buf.data(), buf.size());
                    } catch (const PoDoFo::PdfError&) {
                        c.readable = false;
                    }
                    canvases.push_back(c);
                    collectResourceCanvases(doc, c.resources, canvases, visited, 1);
                }
            }
        } catch (const PoDoFo::PdfError&) {
            // Annotation walk failed — the per-canvas readable flags stay the
            // single source of the skip decision; nothing more to mark here.
        }
    }
    return canvases;
}

// Type3 CharProcs canvases (their fonts come from the Type3's /Resources;
// a Type3 without /Resources yields an unreadable canvas — conservative).
void collectType3Canvases(PdfMemDocument& doc,
                          const std::map<PdfObject*, FontInfo>& fontInfos,
                          std::vector<Canvas>& out) {
    for (const auto& [obj, info] : fontInfos) {
        (void)obj;
        if (info.kind != FontKind::Type3Font || !info.fontDict) continue;
        PdfObject* charProcs = resolveRef(doc,
            info.fontDict->GetDictionary().FindKey("CharProcs"));
        if (!charProcs || !charProcs->IsDictionary()) continue;
        PdfObject* res = resolveRef(doc,
            info.fontDict->GetDictionary().FindKey("Resources"));
        for (auto& entry : charProcs->GetDictionary()) {
            PdfObject* stream = resolveRef(doc, &entry.second);
            if (!stream || !stream->IsDictionary() || !stream->HasStream()) continue;
            Canvas c;
            c.resources = res;
            if (res == nullptr) {
                c.readable = false; // containing-page resources uncorrelatable
            } else {
                try {
                    PoDoFo::charbuff buf;
                    stream->GetOrCreateStream().CopyTo(buf);
                    c.bytes.assign(buf.data(), buf.size());
                } catch (const PoDoFo::PdfError&) {
                    c.readable = false;
                }
            }
            out.push_back(c);
        }
    }
}

// Walk one canvas's decoded content stream, feeding every shown string to
// `onText`. Returns false when the stream cannot be parsed fully — the caller
// must then treat the canvas's fonts as usage-unknown.
bool walkCanvasStream(const Canvas& canvas,
                      const std::function<void(const std::string_view& fontName,
                                               const std::string_view& rawString)>& onText)
{
    if (canvas.bytes.empty()) return canvas.readable;
    auto device = std::make_shared<PoDoFo::SpanStreamDevice>(canvas.bytes);
    PoDoFo::PdfContentStreamReader reader(device);
    PoDoFo::PdfContent content;
    std::string currentFont; // empty = no Tf seen yet
    while (reader.TryReadNext(content)) {
        // Fail fast on ANY unreadable item: PoDoFo's reader can merge a
        // mangled operand into an UnexpectedKeyword and then clear the error
        // flag on the final (failed) read — checking only after the loop
        // would silently treat a half-parsed stream as fully walked.
        if (content.HasErrors()
            || content.GetType() == PoDoFo::PdfContentType::UnexpectedKeyword)
            return false;
        if (content.GetType() != PoDoFo::PdfContentType::Operator) continue;
        const auto& stack = content.GetStack();
        const auto kw = content.GetKeyword();
        if (kw == "Tf") {
            if (stack.size() >= 2 && stack[1].IsName())
                currentFont.assign(stack[1].GetName().GetString());
        } else if (!currentFont.empty()
                   && (kw == "Tj" || kw == "'" || kw == "\"")) {
            if (stack.size() >= 1 && stack[0].IsString())
                onText(currentFont, stack[0].GetString().GetRawData());
        } else if (!currentFont.empty() && kw == "TJ") {
            if (stack.size() >= 1 && stack[0].IsArray())
                for (const auto& item : stack[0].GetArray())
                    if (item.IsString())
                        onText(currentFont, item.GetString().GetRawData());
        }
    }
    return true;
}

// The shared analysis behind both the pass and the estimator. Builds one
// ProgramWork per distinct /FontFile2 stream object with its provable used
// GIDs, closure, and stream state.
void analyzeFontPrograms(PdfMemDocument& doc, FontSubsetStats* stats,
                         std::map<PdfObject*, ProgramWork>& programs,
                         std::map<PdfObject*, FontInfo>& fontInfos)
{
    // 1. Classify every /Type /Font dictionary in the document.
    for (auto obj : doc.GetObjects()) {
        if (!obj->IsDictionary()) continue;
        auto* typeObj = obj->GetDictionary().FindKey("Type");
        if (!typeObj || !typeObj->IsName()
            || typeObj->GetName().GetString() != "Font")
            continue;
        FontInfo info = classifyFont(doc, obj);
        switch (info.kind) {
        case FontKind::CffProgram: if (stats) stats->skippedCffProgram++; break;
        case FontKind::Type1Program: if (stats) stats->skippedType1Program++; break;
        case FontKind::Type3Font: if (stats) stats->skippedType3++; break;
        default: break;
        }
        fontInfos.emplace(obj, info);
    }

    // 2. AcroForm /DR fonts: their glyphs are needed whenever a viewer
    //    regenerates a field appearance from /DA — usage cannot be proven
    //    from the file. Never rewritten (disclosed as skipped-unknown).
    try {
        if (auto* acro = doc.GetAcroForm()) {
            PdfObject* dr = resolveRef(doc, acro->GetDictionary().FindKey("DR"));
            if (dr && dr->IsDictionary()) {
                PdfObject* drFonts = resolveRef(doc, dr->GetDictionary().FindKey("Font"));
                if (drFonts && drFonts->IsDictionary()) {
                    for (auto& entry : drFonts->GetDictionary()) {
                        PdfObject* val = resolveRef(doc, &entry.second);
                        auto it = fontInfos.find(val);
                        if (it == fontInfos.end() || !it->second.fontFile) continue;
                        auto& w = programs[it->second.fontFile];
                        w.fontFile = it->second.fontFile;
                        if (w.usable) {
                            w.usable = false;
                            w.skipReason = QStringLiteral(
                                "AcroForm /DR font — usage depends on viewer-side "
                                "appearance regeneration");
                            if (stats) stats->skippedUnknownUsage++;
                        }
                    }
                }
            }
        }
    } catch (const PoDoFo::PdfError&) {
        // Unreadable AcroForm — the conservative marks below cover it.
    }

    // 3. Walk every canvas, accumulating shown codes per font.
    std::vector<Canvas> canvases = documentCanvases(doc);
    collectType3Canvases(doc, fontInfos, canvases);

    bool sawUnresolvedFontName = false;
    std::vector<const Canvas*> failedCanvases;
    for (const auto& canvas : canvases) {
        std::map<std::string, PdfObject*, std::less<>> fonts;
        fontsOfResources(doc, canvas.resources, fonts);
        const bool ok = walkCanvasStream(canvas,
            [&](const std::string_view& fontName, const std::string_view& raw) {
                const auto it = fonts.find(fontName);
                if (it == fonts.end()) {
                    sawUnresolvedFontName = true;
                    return;
                }
                auto fit = fontInfos.find(it->second);
                if (fit == fontInfos.end()) return;
                FontInfo& info = fit->second;
                info.seen = true;
                if (info.kind == FontKind::CidTrueType
                    || info.kind == FontKind::CidCffProgram) {
                    if (raw.size() % 2 != 0) { info.ragged = true; return; }
                    for (size_t i = 0; i < raw.size(); i += 2)
                        info.codes.insert(
                            (static_cast<uint32_t>(
                                 static_cast<unsigned char>(raw[i])) << 8)
                          | static_cast<unsigned char>(raw[i + 1]));
                } else if (info.kind == FontKind::SimpleTrueType
                           || info.kind == FontKind::CffNameProgram
                           || info.kind == FontKind::CffOpenTypeProgram) {
                    for (size_t i = 0; i < raw.size(); ++i)
                        info.codes.insert(static_cast<unsigned char>(raw[i]));
                }
            });
        if (!ok || !canvas.readable)
            failedCanvases.push_back(&canvas);
    }

    // 4. Any unreadable canvas or unresolved font name makes glyph usage
    //    unprovable — text could hide anywhere we could not see.
    auto markProgramUnknown = [&](PdfObject* fontFile, const QString& reason) {
        auto& w = programs[fontFile];
        w.fontFile = fontFile;
        if (w.usable) {
            w.usable = false;
            w.skipReason = reason;
            if (stats) stats->skippedUnknownUsage++;
        }
    };
    for (const Canvas* canvas : failedCanvases) {
        std::map<std::string, PdfObject*, std::less<>> fonts;
        fontsOfResources(doc, canvas->resources, fonts);
        for (auto& [name, fontObj] : fonts) {
            (void)name;
            auto it = fontInfos.find(fontObj);
            if (it == fontInfos.end() || !it->second.fontFile) continue;
            markProgramUnknown(it->second.fontFile, QStringLiteral(
                "its canvas stream could not be parsed — glyph usage cannot "
                "be proven"));
        }
    }
    if (sawUnresolvedFontName) {
        for (auto& [obj, info] : fontInfos) {
            (void)obj;
            if (info.fontFile)
                markProgramUnknown(info.fontFile, QStringLiteral(
                    "a text operator referenced a font outside its resource "
                    "scope — glyph usage cannot be proven"));
        }
    }

    // 5. Resolve shown codes → GIDs per font; accumulate per program.
    for (auto& [obj, info] : fontInfos) {
        (void)obj;
        if (!info.fontFile) continue;
        auto& w = programs[info.fontFile];
        w.fontFile = info.fontFile;
        // One program can back several font dicts — but only of one program
        // shape; a disagreement is an unmodeled structure → counted skip.
        const ProgramWork::ProgKind kindProg =
              info.kind == FontKind::CidTrueType
           || info.kind == FontKind::SimpleTrueType
                ? ProgramWork::ProgKind::TrueTypeSfnt
            : info.kind == FontKind::CidCffProgram
           || info.kind == FontKind::CffNameProgram
                ? ProgramWork::ProgKind::BareCff
            : info.kind == FontKind::CffOpenTypeProgram
                ? ProgramWork::ProgKind::OpenTypeCff
                : ProgramWork::ProgKind::Unknown;
        if (w.progKind == ProgramWork::ProgKind::Unknown) {
            w.progKind = kindProg;
        } else if (w.progKind != kindProg && w.usable) {
            w.usable = false;
            w.skipReason = QStringLiteral(
                "one font program is shared by fonts of different program kinds");
            if (stats) stats->skippedCffProgram++;
        }
        // Sub-FontMatrix disclosure (CFF kinds): a non-default /FontMatrix on
        // the referencing font dict (or, CID-keyed, the descendant) means a
        // transformed glyph space the lane does not model — counted skip.
        if (info.nonDefaultFontMatrix && w.usable) {
            w.usable = false;
            w.skipReason = QStringLiteral(
                "the font dict carries a non-default /FontMatrix — a "
                "sub-FontMatrix transform is not modeled");
            if (stats) stats->skippedFontMatrix++;
        }
        if (!w.usable) continue;
        if (info.kind == FontKind::CidTrueType || info.kind == FontKind::CidCffProgram) {
            if (info.ragged) {
                w.usable = false;
                w.skipReason = QStringLiteral(
                    "a shown string's byte length is not a multiple of the "
                    "2-byte code width");
                if (stats) stats->skippedEncodingAmbiguous++;
                continue;
            }
            if (info.kind == FontKind::CidTrueType) {
                if (info.cidIdentity) {
                    for (uint32_t cid : info.codes) w.usedGids.insert(cid);
                } else if (!info.codes.isEmpty()) {
                    if (w.cidToGidBytes.isEmpty()) {
                        try {
                            PoDoFo::charbuff buf;
                            info.cidToGidStream->GetOrCreateStream().CopyTo(buf);
                            w.cidToGidBytes = QByteArray(buf.data(),
                                                         static_cast<qsizetype>(buf.size()));
                        } catch (const PoDoFo::PdfError&) {
                            w.usable = false;
                            w.skipReason = QStringLiteral(
                                "/CIDToGIDMap stream could not be read");
                            if (stats) stats->skippedCorruptProgram++;
                            continue;
                        }
                    }
                    bool resolvable = true;
                    const unsigned char* mapBytes = reinterpret_cast<const unsigned char*>(
                        w.cidToGidBytes.constData());
                    const size_t mapSize = static_cast<size_t>(w.cidToGidBytes.size());
                    for (uint32_t cid : info.codes) {
                        const size_t off = static_cast<size_t>(cid) * 2;
                        if (off + 2 > mapSize) {
                            resolvable = false; // CID beyond the map — usage unknown
                            break;
                        }
                        w.usedGids.insert(rd16(mapBytes + off));
                    }
                    if (!resolvable) {
                        w.usable = false;
                        w.skipReason = QStringLiteral(
                            "a shown CID falls outside its /CIDToGIDMap stream");
                        if (stats) stats->skippedEncodingAmbiguous++;
                        continue;
                    }
                }
            } else if (!info.codes.isEmpty()) {
                // CidCffProgram: 2-byte codes ARE CIDs; resolve through the
                // CFF charset's inverse lookup.
                if (w.decoded.isEmpty()) {
                    try {
                        PoDoFo::charbuff buf;
                        w.fontFile->GetOrCreateStream().CopyTo(buf);
                        w.decoded = QByteArray(buf.data(),
                                               static_cast<qsizetype>(buf.size()));
                    } catch (const PoDoFo::PdfError&) {
                        w.usable = false;
                        w.skipReason = QStringLiteral(
                            "font program could not be decoded");
                        if (stats) stats->skippedCorruptProgram++;
                        continue;
                    }
                }
                const unsigned char* prog =
                    reinterpret_cast<const unsigned char*>(w.decoded.constData());
                const size_t progSize = static_cast<size_t>(w.decoded.size());
                QSet<uint32_t> gids;
                if (!cffCidsToGids(prog, progSize, info.codes, gids)) {
                    w.usable = false;
                    w.skipReason = QStringLiteral(
                        "a shown CID cannot be proven through the CFF charset "
                        "(the program is not CID-keyed, or the CID has no glyph)");
                    if (stats) stats->skippedEncodingAmbiguous++;
                    continue;
                }
                w.usedGids.unite(gids);
            }
        } else if ((info.kind == FontKind::SimpleTrueType
                    || info.kind == FontKind::CffNameProgram
                    || info.kind == FontKind::CffOpenTypeProgram)
                   && !info.codes.isEmpty()) {
            if (info.encodingUnprovable) {
                // A named /Encoding (or /BaseEncoding without Differences)
                // shifts bytes 0x80+ — the code-to-glyph path cannot be
                // proven, so the font is disclosed as skipped, never guessed.
                w.usable = false;
                w.skipReason = QStringLiteral(
                    "simple font with a named /Encoding — code-to-glyph "
                    "mapping cannot be proven");
                if (stats) stats->skippedEncodingAmbiguous++;
                continue;
            }
            // Decode the program once for cmap/post/charset resolution.
            if (w.decoded.isEmpty()) {
                try {
                    PoDoFo::charbuff buf;
                    w.fontFile->GetOrCreateStream().CopyTo(buf);
                    w.decoded = QByteArray(buf.data(), static_cast<qsizetype>(buf.size()));
                } catch (const PoDoFo::PdfError&) {
                    w.usable = false;
                    w.skipReason = QStringLiteral("font program could not be decoded");
                    if (stats) stats->skippedCorruptProgram++;
                    continue;
                }
            }
            const unsigned char* prog =
                reinterpret_cast<const unsigned char*>(w.decoded.constData());
            const size_t progSize = static_cast<size_t>(w.decoded.size());
            const bool ottoWrapper = info.kind == FontKind::CffOpenTypeProgram;
            // /Encoding /Differences path: code → glyph NAME → post table
            // (TrueType/OpenType) or the CFF charset (bare Type1C).
            std::map<uint32_t, std::string> differences;
            if (info.requiresDifferences) {
                PdfObject* encDict = resolveRef(doc,
                    info.fontDict->GetDictionary().FindKey("Encoding"));
                PdfObject* diff = encDict
                    ? resolveRef(doc, encDict->GetDictionary().FindKey("Differences"))
                    : nullptr;
                if (diff && diff->IsArray()) {
                    uint32_t code = 0;
                    for (const auto& item : diff->GetArray()) {
                        if (item.IsNumberOrReal()) {
                            code = static_cast<uint32_t>(item.GetReal());
                        } else if (item.IsName()) {
                            differences.emplace(code++,
                                std::string(item.GetName().GetString()));
                        }
                    }
                }
            }
            bool resolvable = true;
            for (uint32_t code : info.codes) {
                if (info.requiresDifferences) {
                    const auto dit = differences.find(code);
                    if (dit == differences.end()) { resolvable = false; break; }
                    uint16_t gid = 0;
                    bool ok = false;
                    if (info.kind == FontKind::CffNameProgram)
                        ok = cffNameToGid(prog, progSize, dit->second, gid);
                    else
                        ok = postNameToGid(prog, progSize, dit->second, gid,
                                           ottoWrapper);
                    if (!ok) {
                        resolvable = false; // name unresolvable — never guess
                        break;
                    }
                    w.usedGids.insert(gid);
                } else if (info.kind == FontKind::CffNameProgram) {
                    uint32_t gid = 0;
                    if (!cffCodeToGidBuiltIn(prog, progSize, code, gid)) {
                        resolvable = false; // built-in encoding unprovable — skip
                        break;
                    }
                    w.usedGids.insert(gid);
                } else {
                    const uint32_t g = cmapLookup(prog, progSize, code, ottoWrapper);
                    w.usedGids.insert(g);
                }
            }
            if (!resolvable) {
                w.usable = false;
                w.skipReason = QStringLiteral(
                    "a shown code's glyph cannot be proven through its "
                    "/Encoding, the font's post table or the CFF charset");
                if (stats) stats->skippedEncodingAmbiguous++;
                continue;
            }
        }
    }

    // 6. Finalize each program: decode, closure, blankable bytes, filters.
    for (auto& [obj, w] : programs) {
        (void)obj;
        try {
            if (w.decoded.isEmpty()) {
                PoDoFo::charbuff raw;
                w.fontFile->GetOrCreateStream().CopyTo(raw);
                w.decoded = QByteArray(raw.data(), static_cast<qsizetype>(raw.size()));
            }
            if (w.encoded.isEmpty()) {
                PoDoFo::charbuff enc;
                w.fontFile->GetOrCreateStream().CopyTo(enc, /*raw=*/true);
                w.encoded = QByteArray(enc.data(), static_cast<qsizetype>(enc.size()));
            }
        } catch (const PoDoFo::PdfError&) {
            w.usable = false;
            w.skipReason = QStringLiteral("font program could not be decoded");
            if (stats) stats->skippedCorruptProgram++;
            continue;
        }
        w.encodedSize = static_cast<qint64>(w.encoded.size());
        if (w.encoded.isEmpty()) {
            // An empty encoded stream must never be the restore source.
            w.usable = false;
            w.skipReason = QStringLiteral("font program stream is empty");
            if (stats) stats->skippedCorruptProgram++;
            continue;
        }
        if (!w.usable) continue;

        // Old-stream filter must be {absent} or {FlateDecode} — anything else
        // (LZW, /DecodeParms predictors) is skipped, never half-restored.
        PdfObject* filter = w.fontFile->GetDictionary().FindKey("Filter");
        PdfObject* parms = w.fontFile->GetDictionary().FindKey("DecodeParms");
        auto filterNameOk = [](const PdfObject* f) {
            return f && f->IsName() && f->GetName().GetString() == "FlateDecode";
        };
        if (parms != nullptr) {
            w.usable = false;
            w.skipReason = QStringLiteral("/DecodeParms on the font stream");
            if (stats) stats->skippedUnsupportedStream++;
            continue;
        }
        if (filter == nullptr) {
            w.filterOk = true;
            w.oldHadFilter = false;
        } else if (filterNameOk(filter)) {
            w.filterOk = true;
            w.oldHadFilter = true;
        } else if (filter->IsArray()) {
            w.filterOk = true;
            w.oldHadFilter = true;
            for (const auto& f : filter->GetArray())
                if (!filterNameOk(&f)) { w.filterOk = false; break; }
        }
        if (!w.filterOk) {
            w.usable = false;
            w.skipReason = QStringLiteral(
                "font stream filter other than /FlateDecode");
            if (stats) stats->skippedUnsupportedStream++;
            continue;
        }
        const unsigned char* prog =
            reinterpret_cast<const unsigned char*>(w.decoded.constData());
        const size_t progSize = static_cast<size_t>(w.decoded.size());
        w.keepGids = w.usedGids;
        bool ok = false;
        switch (w.progKind) {
        case ProgramWork::ProgKind::TrueTypeSfnt: {
            QVector<SfntTableEntry> directory;
            if (!parseSfntDirectory(prog, progSize, directory)) {
                w.usable = false;
                w.skipReason = QStringLiteral(
                    "not a parseable single TrueType sfnt (collection or CFF)");
                if (stats) stats->skippedCorruptProgram++;
                continue;
            }
            if (!expandCompositeClosure(prog, progSize, w.keepGids)) {
                w.usable = false;
                w.skipReason = QStringLiteral(
                    "malformed glyf/loca/maxp tables — glyph closure failed");
                if (stats) stats->skippedCorruptProgram++;
                continue;
            }
            w.blankableBytes = blankableGlyfBytes(prog, progSize, w.keepGids, &ok);
            if (!ok) {
                w.usable = false;
                w.skipReason = QStringLiteral("glyf tables inconsistent");
                if (stats) stats->skippedCorruptProgram++;
                continue;
            }
            break;
        }
        case ProgramWork::ProgKind::BareCff:
        case ProgramWork::ProgKind::OpenTypeCff: {
            // The CFF slice: the whole program (bare CFF) or its 'CFF '
            // table (OpenType wrapper; all other tables stay byte-identical).
            const unsigned char* cff = prog;
            size_t cffSize = progSize;
            if (w.progKind == ProgramWork::ProgKind::OpenTypeCff) {
                QVector<SfntTableEntry> ottoTables;
                if (!parseSfntDirectoryEx(prog, progSize, ottoTables,
                                          /*allowOtto=*/true)) {
                    w.usable = false;
                    w.skipReason = QStringLiteral(
                        "not a parseable OpenType sfnt wrapper");
                    if (stats) stats->skippedCorruptProgram++;
                    continue;
                }
                const SfntTableEntry* cffTable = findTable(ottoTables, kTagCff);
                if (!cffTable || cffTable->length < 4) {
                    w.usable = false;
                    w.skipReason = QStringLiteral(
                        "OpenType wrapper without a usable 'CFF ' table");
                    if (stats) stats->skippedCorruptProgram++;
                    continue;
                }
                w.cffOffset = cffTable->offset;
                w.cffLength = cffTable->length;
                cff = prog + cffTable->offset;
                cffSize = cffTable->length;
            }
            CffLayout lay;
            if (!parseCffLayout(cff, cffSize, lay)) {
                w.usable = false;
                w.skipReason = QStringLiteral(
                    "not a parseable CFF (malformed, or an unmodeled feature: "
                    "Expert charset/encoding, encoding supplements, CFF2, "
                    "non-contiguous subrs, a FontMatrix operator, "
                    "non-standard table order)");
                if (stats) stats->skippedCorruptProgram++;
                continue;
            }
            // Feature gate: a kept charstring (or any subroutine it can end
            // in) using the seac form of endchar references base glyphs by
            // standard-encoding code that cannot be proven into the keep set.
            const CffGate gate = cffSeacGate(cff, cffSize, lay, w.keepGids);
            if (gate == CffGate::Seac) {
                w.usable = false;
                w.skipReason = QStringLiteral(
                    "a kept charstring or subroutine uses the seac form of "
                    "endchar — the referenced base glyphs cannot be proven");
                if (stats) stats->skippedCffUnsupported++;
                continue;
            }
            if (gate == CffGate::Malformed) {
                w.usable = false;
                w.skipReason = QStringLiteral("malformed Type 2 charstring");
                if (stats) stats->skippedCorruptProgram++;
                continue;
            }
            w.blankableBytes = blankableCffCharstringBytes(cff, cffSize,
                                                           w.keepGids, &ok);
            if (!ok) {
                w.usable = false;
                w.skipReason = QStringLiteral("CFF layout inconsistent");
                if (stats) stats->skippedCorruptProgram++;
                continue;
            }
            break;
        }
        case ProgramWork::ProgKind::Unknown:
        default:
            w.usable = false;
            w.skipReason = QStringLiteral("font program kind unresolved");
            if (stats) stats->skippedCorruptProgram++;
            continue;
        }
        if (w.blankableBytes == 0) {
            // Every glyph is used — a rewrite would gain nothing.
            w.usable = false;
            w.skipReason = QStringLiteral("no unused glyphs");
            if (stats) stats->skippedNoGain++;
            continue;
        }
        // Already subset-prefixed fonts below the rewrite threshold are left
        // untouched (PoDoFo-written subsets are dense; churn without gain).
        bool prefixedBelowThreshold = false;
        for (const auto& [fobj, finfo] : fontInfos) {
            (void)fobj;
            if (finfo.fontFile == w.fontFile && finfo.subsetPrefixed
                && w.encodedSize < kPrefixedRewriteThreshold) {
                prefixedBelowThreshold = true;
                break;
            }
        }
        if (prefixedBelowThreshold) {
            w.usable = false;
            w.skipReason = QStringLiteral(
                "already subset-prefixed and below the rewrite threshold");
            if (stats) stats->skippedAlreadySubset++;
            continue;
        }
        if (stats) stats->fontProgramsEligible++;
    }
}

} // namespace

FontSubsetStats subsetDocumentFonts(PdfMemDocument& doc, bool documentIsSigned)
{
    FontSubsetStats stats;
    try {
        if (documentIsSigned) {
            // The signed-doc write path is an incremental update (writeUpdate)
            // that can never shrink — same guard as the Phase-4 sweep.
            stats.skippedSignedDoc = 1;
            qDebug() << "subsetDocumentFonts: signed document — font subsetting skipped";
            return stats;
        }
        std::map<PdfObject*, ProgramWork> programs;
        std::map<PdfObject*, FontInfo> fontInfos;
        analyzeFontPrograms(doc, &stats, programs, fontInfos);

        for (auto& [obj, w] : programs) {
            (void)obj;
            if (!w.usable) {
                qDebug() << "subsetDocumentFonts: font program skipped —" << w.skipReason;
                continue;
            }
            const unsigned char* prog =
                reinterpret_cast<const unsigned char*>(w.decoded.constData());
            const size_t progSize = static_cast<size_t>(w.decoded.size());
            QByteArray newProgram;
            if (w.progKind == ProgramWork::ProgKind::OpenTypeCff) {
                // Blank the 'CFF ' table's unused charstrings, then rebuild
                // the OTTO wrapper around the rewritten table (every other
                // sfnt table stays byte-identical).
                QByteArray newCff;
                if (!blankUnusedCffCharstrings(prog + w.cffOffset, w.cffLength,
                                               w.keepGids, newCff)
                    || !rebuildSfntReplacingTable(prog, progSize, kTagCff,
                                                  newCff, newProgram)) {
                    stats.skippedCorruptProgram++;
                    qDebug() << "subsetDocumentFonts: font program skipped — "
                                "rewrite failed (OpenType/CFF)";
                    continue;
                }
            } else if (w.progKind == ProgramWork::ProgKind::BareCff) {
                if (!blankUnusedCffCharstrings(prog, progSize, w.keepGids,
                                               newProgram)) {
                    stats.skippedCorruptProgram++;
                    qDebug() << "subsetDocumentFonts: font program skipped — "
                                "rewrite failed (CFF)";
                    continue;
                }
            } else if (!blankUnusedGlyphs(prog, progSize, w.keepGids, newProgram)) {
                stats.skippedCorruptProgram++;
                qDebug() << "subsetDocumentFonts: font program skipped — rewrite failed";
                continue;
            }
            // Commit-or-restore: the new program replaces the stream only when
            // the re-encoded (Flate) stream is smaller than the old one.
            QByteArray oldEncoded = w.encoded;
            PoDoFo::charbuff newBuf(
                std::string_view(newProgram.constData(), static_cast<size_t>(newProgram.size())));
            auto& stream = w.fontFile->GetOrCreateStream();
            stream.SetData(newBuf, PoDoFo::PdfFilterList{ PoDoFo::PdfFilterType::FlateDecode },
                           /*raw=*/false);   // PoDoFo encodes; writes /Filter /FlateDecode
            PoDoFo::charbuff newEncodedBuf;
            stream.CopyTo(newEncodedBuf, /*raw=*/true);
            const qint64 newEncodedSize = static_cast<qint64>(newEncodedBuf.size());
            if (newEncodedSize >= w.encodedSize) {
                // No gain — restore the original stream exactly.
                if (w.oldHadFilter) {
                    stream.SetData(PoDoFo::bufferview(oldEncoded.constData(),
                                                      static_cast<size_t>(oldEncoded.size())),
                                   PoDoFo::PdfFilterList{ PoDoFo::PdfFilterType::FlateDecode },
                                   /*raw=*/true);
                } else {
                    stream.SetData(PoDoFo::bufferview(oldEncoded.constData(),
                                                      static_cast<size_t>(oldEncoded.size())),
                                   /*raw=*/true);
                }
                stats.skippedNoGain++;
                qDebug() << "subsetDocumentFonts: font program skipped — "
                            "rewritten stream is not smaller (old"
                         << w.encodedSize << "new" << newEncodedSize
                         << "rawOld" << w.decoded.size() << "rawNew" << newProgram.size() << ")";
                continue;
            }
            auto& dict = w.fontFile->GetDictionary();
            dict.RemoveKey("DecodeParms");
            // /Length1 must equal the UNCOMPRESSED font program length
            // (32000 9.8). It is a FontFile/FontFile2 program key — a
            // FontFile3 (CFF) stream dict carries no Length keys.
            if (w.progKind == ProgramWork::ProgKind::TrueTypeSfnt)
                dict.AddKey("Length1", static_cast<int64_t>(newProgram.size()));
            stats.fontProgramsSubsetted++;
            stats.bytesSaved += w.encodedSize - newEncodedSize;
        }
    } catch (const std::exception& e) {
        // Containment: the pass never fails the caller's run; fonts already
        // rewritten stay rewritten (each is independently valid).
        qWarning() << "subsetDocumentFonts: aborted with" << e.what()
                   << "— remaining font programs left untouched";
    }
    qDebug() << "subsetDocumentFonts: eligible" << stats.fontProgramsEligible
             << "subsetted" << stats.fontProgramsSubsetted
             << "bytesSaved" << stats.bytesSaved
             << "| skips: signed" << stats.skippedSignedDoc
             << "cff" << stats.skippedCffProgram
             << "cffUnsupported" << stats.skippedCffUnsupported
             << "fontMatrix" << stats.skippedFontMatrix
             << "type1" << stats.skippedType1Program
             << "type3" << stats.skippedType3
             << "unknownUsage" << stats.skippedUnknownUsage
             << "encodingAmbiguous" << stats.skippedEncodingAmbiguous
             << "corrupt" << stats.skippedCorruptProgram
             << "unsupportedStream" << stats.skippedUnsupportedStream
             << "alreadySubset" << stats.skippedAlreadySubset
             << "noGain" << stats.skippedNoGain;
    return stats;
}

qint64 estimateSubsetSavings(PdfMemDocument& doc, bool documentIsSigned)
{
    if (documentIsSigned) return 0;
    try {
        FontSubsetStats stats;
        std::map<PdfObject*, ProgramWork> programs;
        std::map<PdfObject*, FontInfo> fontInfos;
        analyzeFontPrograms(doc, &stats, programs, fontInfos);
        qint64 savings = 0;
        for (auto& [obj, w] : programs) {
            (void)obj;
            if (!w.usable || !w.filterOk) continue;
            // The pass commits the rewritten program only when the re-encoded
            // stream is smaller than the old one. Glyph data flate-compresses
            // to roughly half its raw size, so the claim takes a conservative
            // 50% haircut off the raw blankable bytes and never claims more
            // than the stream occupies in the file.
            const qint64 claim = w.blankableBytes / 2;
            savings += std::min(claim, w.encodedSize);
        }
        return savings;
    } catch (const std::exception&) {
        return 0; // an estimate must never fail the caller
    }
}

} } // namespace gp::fontsubset
