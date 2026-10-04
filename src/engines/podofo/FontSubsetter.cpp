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

uint32_t cmapLookup(const unsigned char* data, size_t size, uint32_t code)
{
    QVector<SfntTableEntry> tables;
    if (!parseSfntDirectory(data, size, tables)) return 0;
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
                   const std::string& name, uint16_t& gidOut)
{
    QVector<SfntTableEntry> tables;
    if (!parseSfntDirectory(data, size, tables)) return false;
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

// ── Layer 2: document walk (PoDoFo) ───────────────────────────────────────────

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
    CffProgram,         // /FontFile3 involved (CFF lane: deferred)
    Type1Program,       // /FontFile involved (Type1: out of scope)
    Type3Font,          // no font program
    UnknownOrForeign    // CIDFontType0, non-embedded fonts, exotic dicts
};

struct FontInfo {
    PdfObject* fontDict = nullptr;
    FontKind kind = FontKind::UnknownOrForeign;
    PdfObject* fontFile = nullptr;    // /FontFile2 stream object (rewritable kind)
    bool cidIdentity = true;          // CID: /CIDToGIDMap Identity or absent
    PdfObject* cidToGidStream = nullptr;
    bool encodingUnprovable = false;  // simple: named /Encoding or /BaseEncoding only
    bool requiresDifferences = false; // simple: /Encoding /Differences path
    bool subsetPrefixed = false;      // BaseFont carries an AAAAAA+ tag
    // Populated during the content walk:
    bool seen = false;                // referenced by a Tf in any walked stream
    bool ragged = false;              // CID string with odd byte length
    QSet<uint32_t> codes;             // codes shown with this font
};

// Classify one /Type /Font dictionary.
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
    if (st == "Type0") {
        PdfObject* descArr = resolveRef(doc, d.FindKey("DescendantFonts"));
        if (!descArr || !descArr->IsArray() || descArr->GetArray().IsEmpty()) return info;
        PdfObject* desc = resolveRef(doc, &descArr->GetArray()[0]);
        if (!desc || !desc->IsDictionary()) return info;
        const auto descSub = desc->GetDictionary().FindKey("Subtype");
        if (!descSub || !descSub->IsName()
            || descSub->GetName().GetString() != "CIDFontType2")
            return info; // CIDFontType0 → CFF lane
        descriptor = resolveRef(doc, desc->GetDictionary().FindKey("FontDescriptor"));

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

        // Code width: the /Encoding CMap name decides; Identity-H/-V/-UCS2 are
        // 2-byte. A CMap-stream encoding is not consulted — such fonts skip.
        const auto enc = d.FindKey("Encoding");
        if (enc && enc->IsName()) {
            const std::string_view e = enc->GetName().GetString();
            if (e != "Identity-H" && e != "Identity-V" && e != "Identity-UCS2")
                return info;
        } else {
            return info; // absent or a CMap stream — code width unprovable
        }
        info.kind = FontKind::CidTrueType;
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
    } else {
        return info; // Type1, Type0/CIDFontType0, MMType1 …
    }

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

// One embedded font program and everything known about its usage.
struct ProgramWork {
    PdfObject* fontFile = nullptr;   // the /FontFile2 stream object
    QSet<uint32_t> usedGids;         // accumulated across referencing fonts
    bool usable = true;              // false → never rewrite (reason recorded)
    QString skipReason;
    QByteArray cidToGidBytes;        // /CIDToGIDMap stream bytes (when mapped)
    QByteArray decoded;              // decoded (raw sfnt) program bytes
    QByteArray encoded;              // current encoded stream bytes
    qint64 encodedSize = 0;
    bool filterOk = false;           // old stream filter ∈ {none, FlateDecode}
    bool oldHadFilter = false;
    QSet<uint32_t> keepGids;         // used ∪ {0} ∪ composite closure
    qint64 blankableBytes = 0;
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
                if (info.kind == FontKind::CidTrueType) {
                    if (raw.size() % 2 != 0) { info.ragged = true; return; }
                    for (size_t i = 0; i < raw.size(); i += 2)
                        info.codes.insert(
                            (static_cast<uint32_t>(
                                 static_cast<unsigned char>(raw[i])) << 8)
                          | static_cast<unsigned char>(raw[i + 1]));
                } else if (info.kind == FontKind::SimpleTrueType) {
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
        if (!w.usable) continue;
        if (info.kind == FontKind::CidTrueType) {
            if (info.ragged) {
                w.usable = false;
                w.skipReason = QStringLiteral(
                    "a shown string's byte length is not a multiple of the "
                    "2-byte code width");
                if (stats) stats->skippedEncodingAmbiguous++;
                continue;
            }
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
        } else if (info.kind == FontKind::SimpleTrueType && !info.codes.isEmpty()) {
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
            // Decode the program once for cmap/post resolution.
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
            // /Encoding /Differences path: code → glyph NAME → post table.
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
                    if (!postNameToGid(prog, progSize, dit->second, gid)) {
                        resolvable = false; // name unresolvable — never guess
                        break;
                    }
                    w.usedGids.insert(gid);
                } else {
                    const uint32_t g = cmapLookup(prog, progSize, code);
                    w.usedGids.insert(g);
                }
            }
            if (!resolvable) {
                w.usable = false;
                w.skipReason = QStringLiteral(
                    "a shown code's glyph cannot be proven through its "
                    "/Encoding or the font's post table");
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
        QVector<SfntTableEntry> directory;
        if (!parseSfntDirectory(prog, progSize, directory)) {
            w.usable = false;
            w.skipReason = QStringLiteral(
                "not a parseable single TrueType sfnt (collection or CFF)");
            if (stats) stats->skippedCorruptProgram++;
            continue;
        }
        w.keepGids = w.usedGids;
        if (!expandCompositeClosure(prog, progSize, w.keepGids)) {
            w.usable = false;
            w.skipReason = QStringLiteral(
                "malformed glyf/loca/maxp tables — glyph closure failed");
            if (stats) stats->skippedCorruptProgram++;
            continue;
        }
        bool ok = false;
        w.blankableBytes = blankableGlyfBytes(prog, progSize, w.keepGids, &ok);
        if (!ok) {
            w.usable = false;
            w.skipReason = QStringLiteral("glyf tables inconsistent");
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
            QByteArray newSfnt;
            if (!blankUnusedGlyphs(
                    reinterpret_cast<const unsigned char*>(w.decoded.constData()),
                    static_cast<size_t>(w.decoded.size()), w.keepGids, newSfnt)) {
                stats.skippedCorruptProgram++;
                qDebug() << "subsetDocumentFonts: font program skipped — rewrite failed";
                continue;
            }
            // Commit-or-restore: the new program replaces the stream only when
            // the re-encoded (Flate) stream is smaller than the old one.
            QByteArray oldEncoded = w.encoded;
            PoDoFo::charbuff newBuf(
                std::string_view(newSfnt.constData(), static_cast<size_t>(newSfnt.size())));
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
                         << "rawOld" << w.decoded.size() << "rawNew" << newSfnt.size() << ")";
                continue;
            }
            auto& dict = w.fontFile->GetDictionary();
            dict.RemoveKey("DecodeParms");
            // /Length1 must equal the UNCOMPRESSED font program length (32000 9.8).
            dict.AddKey("Length1", static_cast<int64_t>(newSfnt.size()));
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
