// SPDX-License-Identifier: Apache-2.0
// Font-subsetting plan (docs/research/font-subsetting-plan-2026-10-01.md §5.2)
// regression suite for the route-A keep-CID blank-glyph subsetter — the
// TrueType core (/FontFile2) plus the CFF lane (/FontFile3 bare CFF and
// OpenType-wrapped 'CFF ' tables).
//
// Pin families:
//   1. sfnt surgery (hermetic, synthetic program): blanking preserves GID
//      numbering and kept-glyph bytes, rebuilds loca and head checksums,
//      composite-glyph closure pulls component chains;
//   1b. CFF surgery (hermetic, synthetic programs — no host CFF/OTTO font
//      exists on any probed Windows image, so both the CID-keyed and the
//      name-keyed fixtures are hand-built to the CFF spec, layout verified
//      against fontTools' cffLib reference): layout parse, blanking preserves
//      GID numbering and kept charstring bytes, charset/subrs/TopDICT
//      regions consistent, blanked charstrings collapse to bare endchar;
//   2. round-trip CIDFontType2 (hand-built Type0 fixture over a real system
//      TTF, DontSubset-style full program): output smaller, /FontFile2 shrunk,
//      content streams //ToUnicode /W /CIDToGIDMap identical, used glyphs
//      intact, unused glyphs blank;
//   2b. round-trip CFF: CIDFontType0C (Identity-H, CID→GID via the inverse
//      charset), bare Type1C (built-in encoding path), OpenType-wrapped
//      'CFF ' (sfnt cmap path, non-CFF tables byte-identical) — estimator
//      honesty both ways, program shrink, render-diff, extraction identity;
//   3. render-diff (pdfium): the page renders identically before/after — the
//      "blanked a used glyph" bug class;
//   4. scope disclosure: unparseable CFF (/FontFile3), Type1 (/FontFile),
//      non-CID-keyed CFF under a Type0 wrapper, named-/Encoding simple CFF,
//      seac-using CFFs, sub-FontMatrix (non-default /FontMatrix) fonts and
//      corrupt programs are left untouched / skipped safely; signed documents
//      claim no estimate savings;
//   5. estimator honesty: subset savings are claimed ONLY when the pass will
//      actually rewrite a program, never more than the program occupies.
//
// Document fixtures embed a real host TTF (QSKIP when absent — the
// TestFindReplace.cpp:109 precedent); the sfnt and CFF pins are fully
// hermetic.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>

#include <cstring>
#include <map>
#include <utility>
#include <vector>

#include <podofo/podofo.h>

#include "engines/PdfEditorEngine.h"
#include "engines/podofo/FontSubsetter.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "engines/SignatureManager.h"

// windows.h (pulled in by the engine headers) defines GetObject as a GDI
// macro — it would rewrite PoDoFo's PdfIndirectObjectList::GetObject calls.
#ifdef GetObject
#undef GetObject
#endif

using namespace gp::fontsubset;

namespace {

// ── little sfnt helpers over the public parse API ─────────────────────────────

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

const SfntTableEntry* tableOf(const QVector<SfntTableEntry>& t, uint32_t tag) {
    for (const auto& e : t)
        if (e.tag == tag) return &e;
    return nullptr;
}
constexpr uint32_t kGlyf = 0x676c7966u, kLoca = 0x6c6f6361u, kHead = 0x68656164u,
                   kMaxp = 0x6d617870u;

bool longLocaOf(const QByteArray& font) {
    QVector<SfntTableEntry> t;
    if (!parseSfntDirectory(reinterpret_cast<const unsigned char*>(font.constData()),
                            static_cast<size_t>(font.size()), t))
        return false;
    const SfntTableEntry* head = tableOf(t, kHead);
    return head && rd16(reinterpret_cast<const unsigned char*>(font.constData())
                        + head->offset + 50) == 1;
}

uint32_t numGlyphsOf(const QByteArray& font) {
    QVector<SfntTableEntry> t;
    if (!parseSfntDirectory(reinterpret_cast<const unsigned char*>(font.constData()),
                            static_cast<size_t>(font.size()), t))
        return 0;
    const SfntTableEntry* maxp = tableOf(t, kMaxp);
    return maxp ? rd16(reinterpret_cast<const unsigned char*>(font.constData())
                       + maxp->offset + 4) : 0;
}

// glyf byte span [start,end) of `gid` per the program's own loca.
std::pair<uint32_t, uint32_t> spanOf(const QByteArray& font, uint32_t gid) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(font.constData());
    QVector<SfntTableEntry> t;
    if (!parseSfntDirectory(p, static_cast<size_t>(font.size()), t))
        return { 1, 0 }; // poison: "empty" is (n,n)
    const SfntTableEntry* glyf = tableOf(t, kGlyf);
    const SfntTableEntry* loca = tableOf(t, kLoca);
    if (!glyf || !loca || gid >= numGlyphsOf(font)) return { 1, 0 };
    const bool lng = longLocaOf(font);
    uint32_t s, e;
    if (lng) {
        s = rd32(p + loca->offset + static_cast<size_t>(gid) * 4);
        e = rd32(p + loca->offset + (static_cast<size_t>(gid) + 1) * 4);
    } else {
        s = static_cast<uint32_t>(rd16(p + loca->offset + static_cast<size_t>(gid) * 2)) * 2u;
        e = static_cast<uint32_t>(rd16(p + loca->offset + (static_cast<size_t>(gid) + 1) * 2)) * 2u;
    }
    return { s + glyf->offset, e + glyf->offset };
}

// ── hermetic synthetic sfnt: 5 glyphs, short loca ─────────────────────────────
//   g0 empty · g1 simple 12B · g2 simple 8B · g3 composite→g1 · g4 simple 20B
QByteArray makeSyntheticSfnt()
{
    const int n4 = 4;
    const uint32_t tags4[4] = { kGlyf, kLoca, kHead, kMaxp }; // tag-sorted order

    QByteArray glyfB(56, '\0');
    unsigned char* glyf = reinterpret_cast<unsigned char*>(glyfB.data());
    // g3 (span [20,36)) is the composite referencing g1:
    //   header -1 + bbox + flags(byte args, no MORE_COMPONENTS) + index + args.
    wr16(glyf + 20 + 0, 0xFFFF);
    wr16(glyf + 20 + 2, 0); wr16(glyf + 20 + 4, 0);
    wr16(glyf + 20 + 6, 100); wr16(glyf + 20 + 8, 100);
    wr16(glyf + 20 + 10, 0x0000);
    wr16(glyf + 20 + 12, 1);
    wr16(glyf + 20 + 14, 0x0010);

    QByteArray locaB(12, '\x00');
    {
        const uint16_t locaVals[6] = { 0, 0, 6, 10, 18, 28 }; // short loca: offsets/2
        unsigned char* lp = reinterpret_cast<unsigned char*>(locaB.data());
        for (int i = 0; i < 6; ++i) wr16(lp + i * 2, locaVals[i]); // sfnt is big-endian
    }

    QByteArray headB(54, '\0');
    unsigned char* head = reinterpret_cast<unsigned char*>(headB.data());
    wr32(head + 0, 0x00010000u);
    wr32(head + 8, 0);              // checkSumAdjustment
    wr32(head + 12, 0x5F0F3CF5u);   // magic
    wr16(head + 18, 1000);          // unitsPerEm
    wr16(head + 50, 0);             // indexToLocFormat: short

    QByteArray maxpB(6, '\0');
    unsigned char* maxp = reinterpret_cast<unsigned char*>(maxpB.data());
    wr32(maxp + 0, 0x00010000u);
    wr16(maxp + 4, 5);

    const QByteArray bodies4[4] = { glyfB, locaB, headB, maxpB };
    uint32_t cursor = 12 + static_cast<uint32_t>(n4) * 16;
    uint32_t offs4[4], lens4[4];
    for (int i = 0; i < n4; ++i) {
        offs4[i] = cursor;
        lens4[i] = static_cast<uint32_t>(bodies4[i].size());
        cursor = (cursor + lens4[i] + 3u) & ~3u;
    }
    QByteArray out(cursor, '\0');
    unsigned char* o = reinterpret_cast<unsigned char*>(out.data());
    wr32(o + 0, 0x00010000u);
    wr16(o + 4, n4);
    wr16(o + 6, 64);  // searchRange (4 tables × 16)
    wr16(o + 8, 2);   // entrySelector
    wr16(o + 10, 0);  // rangeShift
    for (int i = 0; i < n4; ++i) {
        unsigned char* rec = o + 12 + i * 16;
        wr32(rec + 0, tags4[i]);
        wr32(rec + 4, 0); // checksums irrelevant to the parser
        wr32(rec + 8, offs4[i]);
        wr32(rec + 12, lens4[i]);
        std::memcpy(o + offs4[i], bodies4[i].constData(), bodies4[i].size());
    }
    return out;
}

// ── hermetic synthetic CFF programs (no host CFF/OTTO font exists) ────────────
// Hand-built to the CFF spec (Adobe TN 5176); the layout/encoding rules were
// cross-checked against fontTools' cffLib reference implementation:
//   * INDEX: count(2B); count>0 → offSize(1B) + (count+1) big-endian offsets
//     relative to the byte preceding the data (first offset == 1);
//   * DICT ints: -107..107 → b0=v+139; 108..1131 → 247+…; -1131..-108 →
//     251+…; 28 → 3-byte int16; 29 → 5-byte int32 (fixed width, patchable);
//   * Top DICT ops: charset 15, Encoding 16, CharStrings 17, Private 18
//     (size, offset), ROS 12 30, FDArray 12 36, FDSelect 12 37;
//   * charset format 0: (numGlyphs-1) SIDs/CIDs; FDSelect format 0: 1 byte
//     per glyph; Private op 19 Subrs: offset relative to the Private DICT.

QByteArray t2num(int v) // Type 2 charstring integer
{
    QByteArray b;
    const auto w8 = [&b](int x) { b.append(static_cast<char>(x & 0xFF)); };
    if (v >= -107 && v <= 107) { w8(v + 139); }
    else if (v >= 108 && v <= 1131) { v -= 108; w8(247 + v / 256); w8(v % 256); }
    else if (v <= -108 && v >= -1131) { v = -v - 108; w8(251 + v / 256); w8(v % 256); }
    else { w8(28); w8((v >> 8) & 0xFF); w8(v & 0xFF); }
    return b;
}

QByteArray dictNum(int v) // DICT integer, minimal form
{
    QByteArray b;
    const auto w8 = [&b](int x) { b.append(static_cast<char>(x & 0xFF)); };
    if (v >= -107 && v <= 107) { w8(v + 139); return b; }
    if (v >= 108 && v <= 1131) { v -= 108; w8(247 + v / 256); w8(v % 256); return b; }
    if (v <= -108 && v >= -1131) { v = -v - 108; w8(251 + v / 256); w8(v % 256); return b; }
    if (v >= -32768 && v <= 32767) { w8(28); w8((v >> 8) & 0xFF); w8(v & 0xFF); return b; }
    w8(29);
    for (int s = 24; s >= 0; s -= 8) w8((v >> s) & 0xFF);
    return b;
}

QByteArray dictNum32(int v) // DICT integer, fixed 5-byte 29-form (patchable)
{
    QByteArray b;
    b.append(static_cast<char>(29));
    for (int s = 24; s >= 0; s -= 8) b.append(static_cast<char>((v >> s) & 0xFF));
    return b;
}

QByteArray cffIndex(const QVector<QByteArray>& items, int offSize)
{
    QByteArray b;
    const int count = items.size();
    b.append(static_cast<char>((count >> 8) & 0xFF));
    b.append(static_cast<char>(count & 0xFF));
    if (count == 0) return b; // empty INDEX: count field only
    b.append(static_cast<char>(offSize));
    QVector<uint32_t> offs;
    uint32_t acc = 1;
    offs.append(acc);
    for (const auto& it : items) { acc += static_cast<uint32_t>(it.size()); offs.append(acc); }
    for (uint32_t o : offs)
        for (int s = (offSize - 1) * 8; s >= 0; s -= 8)
            b.append(static_cast<char>((o >> s) & 0xFF));
    for (const auto& it : items) b.append(it);
    return b;
}

// A filled-square Type 2 charstring (real ink, so render-diffs are meaningful).
QByteArray squareCharstring(int x0, int y0, int w, int h, bool withWidth)
{
    QByteArray cs;
    if (withWidth) cs += t2num(120);
    cs += t2num(x0); cs += t2num(y0); cs.append(char(21)); // rmoveto
    cs += t2num(w);  cs += t2num(0);  cs.append(char(5));  // rlineto
    cs += t2num(0);  cs += t2num(h);  cs.append(char(5));  // rlineto
    cs += t2num(-w); cs += t2num(0);  cs.append(char(5));  // rlineto
    cs.append(char(14));                                   // endchar
    return cs;
}

// CID-keyed CFF: 6 glyphs (g0 .notdef + 5 squares, g5 via a local subr),
// charset maps GID1..5 → CIDs 7..11 (NON-identity — exercises the inverse
// lookup), FDSelect format 0 (single FD), Private + local Subrs.
// Used CIDs in the fixture below: 7 (g1), 9 (g3), 11 (g5) → blankable g2+g4.
struct CidCffSpec {
    bool seacGlyph = false; // make USED glyph g1 a seac charstring (skip gate)
};
QByteArray makeCidCff(const CidCffSpec& spec = {})
{
    // Top DICT: ROS(SIDs 391/392, supplement 0), charset, CharStrings,
    // FDArray, FDSelect — every offset operand in fixed 5-byte 29-form, so
    // the entry length is deterministic and offsets can be precomputed.
    const int topDictEntryLen =
        2 + 2 + 1 + 2   // ROS: 391(2B) 392(2B) 0(1B) + op(2B)
        + 5 + 1 + 5 + 1 // charset, CharStrings
        + 5 + 2 + 5 + 2;// FDArray, FDSelect
    const auto topDictBuilder = [](int charsetOff, int csOff, int fdArrayOff,
                                    int fdSelectOff) {
        QByteArray td;
        td += dictNum(391); td += dictNum(392); td += dictNum(0);
        td.append(char(12)); td.append(char(30));                       // ROS
        td += dictNum32(charsetOff); td.append(char(15));               // charset
        td += dictNum32(csOff); td.append(char(17));                    // CharStrings
        td += dictNum32(fdArrayOff); td.append(char(12)); td.append(char(36)); // FDArray
        td += dictNum32(fdSelectOff); td.append(char(12)); td.append(char(37)); // FDSelect
        return td;
    };

    QByteArray header;
    header.append(char(1)); header.append(char(0)); // major 1, minor 0
    header.append(char(4)); header.append(char(2)); // hdrSize 4, offSize 2
    const QByteArray nameIndex = cffIndex({ "Fix" }, 1);
    const QByteArray stringIndex = cffIndex({ "Adobe", "Identity" }, 1); // SIDs 391, 392
    const QByteArray gsubrIndex = cffIndex({}, 1); // empty global subrs

    QByteArray fdSelect;
    fdSelect.append(char(0)); // format 0
    for (int i = 0; i < 6; ++i) fdSelect.append(char(0));

    QByteArray charset;
    charset.append(char(0)); // format 0
    const uint16_t cids[5] = { 7, 8, 9, 10, 11 };
    for (uint16_t c : cids) {
        charset.append(static_cast<char>((c >> 8) & 0xFF));
        charset.append(static_cast<char>(c & 0xFF));
    }

    QByteArray subr;
    subr += t2num(400); subr += t2num(0); subr.append(char(5)); // 400 0 rlineto
    subr.append(char(11));                                      // return
    const QByteArray subrs = cffIndex({ subr }, 1);
    // Private DICT: defaultWidthX 0 (op 20), nominalWidthX 0 (op 21),
    // Subrs <privateLen> (op 19) — the subrs follow the private dict directly.
    QByteArray priv;
    priv.append(char(139)); priv.append(char(20)); // defaultWidthX 0
    priv.append(char(139)); priv.append(char(21)); // nominalWidthX 0
    priv.append(char(139 + static_cast<int>(priv.size()) + 1 + 1)); // Subrs = 6 (1B form)
    priv.append(char(19));
    // FDArray: one Font DICT holding Private <size> <offset> (op 18).
    QByteArray fontDict;
    fontDict += dictNum32(static_cast<int>(priv.size()));
    fontDict += dictNum32(0); // offset patched below (payload pos recorded)
    fontDict.append(char(18));
    const int privOffPayloadPos = static_cast<int>(fontDict.size()) - 5;
    const QByteArray fdArray = cffIndex({ fontDict }, 1);

    QByteArray g1;
    if (spec.seacGlyph) {
        // seac form: width asb adx ady bchar achar endchar (standard codes
        // 65='A', 66='B' — both exist in this font's charset).
        g1 += t2num(100);
        g1 += t2num(0); g1 += t2num(0); g1 += t2num(0);
        g1 += t2num(65); g1 += t2num(66);
        g1.append(char(14));
    } else {
        g1 = squareCharstring(50, 50, 400, 400, false);
    }
    const QByteArray g0 = squareCharstring(30, 30, 300, 300, false); // .notdef
    const QByteArray g2 = squareCharstring(0, 0, 700, 700, false);   // unused victim
    const QByteArray g3 = squareCharstring(50, 50, 500, 500, true);  // width variant
    const QByteArray g4 = squareCharstring(0, 0, 650, 650, false);   // unused victim
    QByteArray g5;
    g5 += t2num(50); g5 += t2num(50); g5.append(char(21)); // 50 50 rmoveto
    g5.append(char(139)); g5.append(char(10));             // 0 callsubr (bias 0)
    g5 += t2num(0);  g5 += t2num(500); g5.append(char(5));
    g5 += t2num(-500); g5 += t2num(0); g5.append(char(5));
    g5.append(char(14));
    const QByteArray charStrings = cffIndex({ g0, g1, g2, g3, g4, g5 }, 1);

    // Layout (original order, exactly tiled):
    //   header | name | topdict | string | gsubr | fdSelect | charset
    //   | fdArray | charStrings | private | subrs
    uint32_t charsetOff = static_cast<uint32_t>(header.size() + nameIndex.size()
        + 2 + 1 + 2 + topDictEntryLen + stringIndex.size() + gsubrIndex.size()
        + fdSelect.size());
    uint32_t csOff = charsetOff + static_cast<uint32_t>(charset.size())
                   + static_cast<uint32_t>(fdArray.size());
    uint32_t fdArrayOff = charsetOff + static_cast<uint32_t>(charset.size());
    uint32_t fdSelectOff = static_cast<uint32_t>(header.size() + nameIndex.size()
        + 2 + 1 + 2 + topDictEntryLen + stringIndex.size() + gsubrIndex.size());

    QByteArray topDict = topDictBuilder(static_cast<int>(charsetOff),
                                        static_cast<int>(csOff),
                                        static_cast<int>(fdArrayOff),
                                        static_cast<int>(fdSelectOff));
    // The Top DICT travels inside its own INDEX (count + offSize + offsets);
    // the offset arithmetic above already accounts for its 2+1+2 bytes.
    const QByteArray topDictIndex = cffIndex({ topDict }, 1);
    // Patch the FDArray Font DICT's Private offset (29-form payload).
    const uint32_t privOff = csOff + static_cast<uint32_t>(charStrings.size());
    const int patchPos = static_cast<int>(fdArray.size()) - static_cast<int>(fontDict.size())
                       + privOffPayloadPos;
    QByteArray fdArrayPatched = fdArray;
    for (int s = 24; s >= 0; s -= 8)
        fdArrayPatched[patchPos + (24 - s) / 8] =
            static_cast<char>((privOff >> s) & 0xFF);

    QByteArray out;
    out += header; out += nameIndex; out += cffIndex({ topDict }, 1);
    out += stringIndex;
    out += gsubrIndex; out += fdSelect; out += charset; out += fdArrayPatched;
    out += charStrings; out += priv; out += subrs;
    return out;
}

// Name-keyed CFF (bare Type1C shape): 4 glyphs, charset format 0 with the
// STANDARD-STRING SIDs for A(34) B(35) C(36) — no Encoding operator, so the
// built-in Adobe Standard Encoding path resolves codes 'A'/'C' → g1/g3.
// Top-level Private, no local subrs. `bulkUnused` pads the unused glyph with
// redundant path work so blanking frees enough raw bytes for the Flate-encoded
// stream to shrink even inside an OpenType wrapper (whose rebuilt directory
// alignment absorbs a small delta).
QByteArray makeNameCff(bool seacGlyph = false, bool bulkUnused = false,
                       bool topFontMatrix = false)
{
    QByteArray header;
    header.append(char(1)); header.append(char(0));
    header.append(char(4)); header.append(char(2));
    const QByteArray nameIndex = cffIndex({ "Fix" }, 1);
    const QByteArray stringIndex = cffIndex({}, 1); // empty String INDEX
    const QByteArray gsubrIndex = cffIndex({}, 1);  // empty global subrs

    QByteArray charset;
    charset.append(char(0)); // format 0
    const uint16_t sids[3] = { 34, 35, 36 }; // A, B, C
    for (uint16_t s : sids) {
        charset.append(static_cast<char>((s >> 8) & 0xFF));
        charset.append(static_cast<char>(s & 0xFF));
    }

    QByteArray g1;
    if (seacGlyph) {
        g1 += t2num(100);
        g1 += t2num(0); g1 += t2num(0); g1 += t2num(0);
        g1 += t2num(65); g1 += t2num(66);
        g1.append(char(14));
    } else {
        g1 = squareCharstring(50, 50, 400, 400, false);
    }
    const QByteArray g0 = squareCharstring(30, 30, 300, 300, false); // .notdef
    QByteArray g2 = squareCharstring(0, 0, 700, 700, false);         // unused victim
    if (bulkUnused) {
        // 60 × (100 0 rlineto): +180 raw bytes that vanish when g2 is blanked,
        // so the Flate-encoded rewrite shrinks even inside the OTTO wrapper.
        for (int i = 0; i < 60; ++i) {
            g2 += t2num(100); g2 += t2num(0); g2.append(char(5));
        }
    }
    const QByteArray g3 = squareCharstring(50, 50, 500, 500, true);
    const QByteArray charStrings = cffIndex({ g0, g1, g2, g3 }, 1);

    QByteArray priv;
    priv.append(char(139)); priv.append(char(20)); // defaultWidthX 0
    priv.append(char(139)); priv.append(char(21)); // nominalWidthX 0

    // Top DICT: charset [off] op15, CharStrings [off] op17, Private <size off> op18.
    // op 18 is a single byte (only 12-escape ops take a second byte).
    // `topFontMatrix` adds a FontMatrix operator (12 7) whose operand is the
    // BCD real 0.001 (nibbles 0 '.' 0 0 1 end → 30 0A 00 1F) — the layout
    // arithmetic must account for the 4 operand bytes + 2 operator bytes.
    const int fontMatrixLen = topFontMatrix ? 4 + 2 : 0;
    const int topDictEntryLen = 5 + 1 + 5 + 1 + 5 + 5 + 1 + fontMatrixLen;
    // Layout: header | name | topdict | string | gsubr | charset | charstrings | private
    const uint32_t charsetOff = static_cast<uint32_t>(
        header.size() + nameIndex.size() + 2 + 1 + 2 + topDictEntryLen
        + stringIndex.size() + gsubrIndex.size());
    const uint32_t csOff = charsetOff + static_cast<uint32_t>(charset.size());
    const uint32_t privOff = csOff + static_cast<uint32_t>(charStrings.size());

    QByteArray topDict;
    if (topFontMatrix) {
        topDict.append(char(30)); topDict.append(char(0x0A));
        topDict.append(char(0x00)); topDict.append(char(0x1F)); // real 0.001
        topDict.append(char(12)); topDict.append(char(7));      // FontMatrix
    }
    topDict += dictNum32(static_cast<int>(charsetOff)); topDict.append(char(15));
    topDict += dictNum32(static_cast<int>(csOff)); topDict.append(char(17));
    topDict += dictNum32(static_cast<int>(priv.size()));
    topDict += dictNum32(static_cast<int>(privOff)); topDict.append(char(18));

    QByteArray out;
    out += header; out += nameIndex; out += cffIndex({ topDict }, 1);
    out += stringIndex;
    out += gsubrIndex; out += charset; out += charStrings; out += priv;
    return out;
}

// Minimal OpenType (OTTO) wrapper around a CFF program: cmap (3,10) format 12
// mapping 'A'→GID1 and 'C'→GID3, head/hhea/hmtx/maxp(0.5) — enough for
// FreeType/pdfium to load and render the CFF glyphs.
QByteArray makeOttWrapper(const QByteArray& cff)
{
    const uint32_t kCffTag = 0x43464620u; // 'CFF '
    const uint32_t kCmapTag = 0x636d6170u;
    const uint32_t kHeadTag = 0x68656164u;
    const uint32_t kHheaTag = 0x68686561u;
    const uint32_t kHmtxTag = 0x686d7478u;
    const uint32_t kMaxpTag = 0x6d617870u;

    QByteArray headB(54, '\0');
    {
        unsigned char* h = reinterpret_cast<unsigned char*>(headB.data());
        wr32(h + 0, 0x00010000u);
        wr32(h + 4, 0x00010000u);       // fontRevision
        wr32(h + 8, 0);                 // checkSumAdjustment
        wr32(h + 12, 0x5F0F3CF5u);      // magic
        wr16(h + 16, 0x0003u);          // flags
        wr16(h + 18, 1000);             // unitsPerEm
        wr16(h + 36, 0); wr16(h + 38, 0);   // xMin yMin
        wr16(h + 40, 700); wr16(h + 42, 700); // xMax yMax
        wr16(h + 46, 8);                // lowestRecPPEM
        wr16(h + 48, 2);                // fontDirectionHint
        wr16(h + 50, 0);                // indexToLocFormat (no glyf)
        wr16(h + 52, 0);                // glyphDataFormat
    }
    QByteArray maxpB(6, '\0');
    {
        unsigned char* m = reinterpret_cast<unsigned char*>(maxpB.data());
        wr32(m + 0, 0x00005000u);       // version 0.5 (CFF)
        wr16(m + 4, 4);                 // numGlyphs
    }
    QByteArray hheaB(36, '\0');
    {
        unsigned char* h = reinterpret_cast<unsigned char*>(hheaB.data());
        wr32(h + 0, 0x00010000u);
        wr16(h + 4, 800); wr16(h + 6, 0xFF38u); // ascender, descender (-200)
        wr16(h + 8, 0);                          // lineGap
        wr16(h + 10, 600);                       // advanceWidthMax
        wr16(h + 12, 0); wr16(h + 14, 0); wr16(h + 16, 600);
        wr16(h + 18, 1); wr16(h + 20, 0); wr16(h + 22, 0); // caret
        wr16(h + 24, 0); wr16(h + 26, 0); wr16(h + 28, 0); wr16(h + 30, 0);
        wr16(h + 32, 0);                         // metricDataFormat
        wr16(h + 34, 4);                         // numberOfHMetrics
    }
    QByteArray hmtxB(16, '\0');
    {
        unsigned char* m = reinterpret_cast<unsigned char*>(hmtxB.data());
        for (int i = 0; i < 4; ++i) { wr16(m + i * 4, 600); wr16(m + i * 4 + 2, 0); }
    }
    QByteArray cmapSub(40, '\0');
    {
        // format 12: groups 'A'(65)→1, 'C'(67)→3
        unsigned char* s = reinterpret_cast<unsigned char*>(cmapSub.data());
        wr16(s + 0, 12); wr16(s + 2, 0); wr32(s + 4, 40); // format, reserved, length
        wr32(s + 8, 0);  wr32(s + 12, 2);                 // language, nGroups
        wr32(s + 16, 65); wr32(s + 20, 65); wr32(s + 24, 1);
        wr32(s + 28, 67); wr32(s + 32, 67); wr32(s + 36, 3);
    }
    QByteArray cmapB;
    {
        unsigned char* c = reinterpret_cast<unsigned char*>(cmapB.data());
        QByteArray headerPart(12, '\0');
        unsigned char* h = reinterpret_cast<unsigned char*>(headerPart.data());
        wr16(h + 0, 0); wr16(h + 2, 1);            // version, numTables
        wr16(h + 4, 3); wr16(h + 6, 10);           // platformID 3, encodingID 10
        wr32(h + 8, 12);                           // subtable offset
        cmapB = headerPart + cmapSub;
        Q_UNUSED(c);
    }

    struct T { uint32_t tag; QByteArray body; };
    const T tables[6] = {
        { kCffTag, cff }, { kCmapTag, cmapB }, { kHeadTag, headB },
        { kHheaTag, hheaB }, { kHmtxTag, hmtxB }, { kMaxpTag, maxpB },
    };
    uint32_t cursor = 12 + 6u * 16u;
    uint32_t offs[6], lens[6];
    for (int i = 0; i < 6; ++i) {
        offs[i] = cursor;
        lens[i] = static_cast<uint32_t>(tables[i].body.size());
        cursor = (cursor + lens[i] + 3u) & ~3u;
    }
    QByteArray out(cursor, '\0');
    unsigned char* o = reinterpret_cast<unsigned char*>(out.data());
    wr32(o + 0, 0x4F54544Fu); // 'OTTO'
    wr16(o + 4, 6);
    wr16(o + 6, 96);  // searchRange (6 × 16)
    wr16(o + 8, 2);   // entrySelector
    wr16(o + 10, 0);  // rangeShift
    for (int i = 0; i < 6; ++i) {
        unsigned char* rec = o + 12 + i * 16;
        wr32(rec + 0, tables[i].tag);
        wr32(rec + 4, 0);
        wr32(rec + 8, offs[i]);
        wr32(rec + 12, lens[i]);
        std::memcpy(o + offs[i], tables[i].body.constData(), tables[i].body.size());
    }
    return out;
}

// ── host TTF lookup (QSKIP precedent, TestFindReplace.cpp) ────────────────────

QString pickHostTtf()
{
    for (const QString& path : {
             QStringLiteral("C:/Windows/Fonts/simhei.ttf"),
             QStringLiteral("C:/Windows/Fonts/arial.ttf"),
             QStringLiteral("C:/Windows/Fonts/tahoma.ttf"),
             QStringLiteral("C:/Windows/Fonts/segoeui.ttf") }) {
        if (QFileInfo::exists(path)) return path;
    }
    return QString();
}

QByteArray hostTtfBytes(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

// ── hand-built document fixtures ───────────────────────────────────────────────

PoDoFo::PdfObject& makeFontFile2Stream(PoDoFo::PdfMemDocument& doc,
                                       const QByteArray& program)
{
    auto& obj = doc.GetObjects().CreateDictionaryObject();
    PoDoFo::charbuff buf(
        std::string_view(program.constData(), static_cast<size_t>(program.size())));
    obj.GetOrCreateStream().SetData(buf,
        PoDoFo::PdfFilterList{ PoDoFo::PdfFilterType::FlateDecode }, /*raw=*/false);
    obj.GetDictionary().AddKey("Length1", static_cast<int64_t>(program.size()));
    return obj;
}

PoDoFo::PdfObject& makeFontDescriptor(PoDoFo::PdfMemDocument& doc,
                                      PoDoFo::PdfObject& fontFile,
                                      const char* fileKey)
{
    auto& fd = doc.GetObjects().CreateDictionaryObject();
    fd.GetDictionary().AddKey("Type", PoDoFo::PdfName("FontDescriptor"));
    fd.GetDictionary().AddKey("FontName", PoDoFo::PdfName("Fixture"));
    fd.GetDictionary().AddKey("Flags", static_cast<int64_t>(4));
    PoDoFo::PdfArray bbox;
    bbox.Add(PoDoFo::PdfVariant(0.0));
    bbox.Add(PoDoFo::PdfVariant(0.0));
    bbox.Add(PoDoFo::PdfVariant(1000.0));
    bbox.Add(PoDoFo::PdfVariant(1000.0));
    fd.GetDictionary().AddKey("FontBBox", PoDoFo::PdfVariant(bbox));
    fd.GetDictionary().AddKey("ItalicAngle", 0.0);
    fd.GetDictionary().AddKey("Ascent", 800.0);
    fd.GetDictionary().AddKey("Descent", -200.0);
    fd.GetDictionary().AddKey("CapHeight", 700.0);
    fd.GetDictionary().AddKey("StemV", 80.0);
    fd.GetDictionary().AddKey(fileKey, fontFile.GetIndirectReference());
    return fd;
}

PoDoFo::PdfObject& addContentStream(PoDoFo::PdfMemDocument& doc,
                                    PoDoFo::PdfPage& page,
                                    const std::string& content,
                                    PoDoFo::PdfObject* fontObj,
                                    const char* fontName)
{
    auto& res = doc.GetObjects().CreateDictionaryObject();
    auto& fonts = doc.GetObjects().CreateDictionaryObject();
    fonts.GetDictionary().AddKey(fontName, fontObj->GetIndirectReference());
    res.GetDictionary().AddKey("Font", fonts.GetIndirectReference());
    page.GetDictionary().AddKey("Resources", res.GetIndirectReference());
    auto& contents = doc.GetObjects().CreateDictionaryObject();
    PoDoFo::charbuff buf(std::string_view(content.data(), content.size()));
    contents.GetOrCreateStream().SetData(buf, PoDoFo::PdfFilterList{}, /*raw=*/true);
    page.GetDictionary().AddKey("Contents", contents.GetIndirectReference());
    return contents;
}

QString toHexCodes(const QVector<uint32_t>& codes) // 2-byte big-endian hex string body
{
    QString hex;
    for (uint32_t c : codes)
        hex += QStringLiteral("%1").arg(c, 4, 16, QLatin1Char('0'));
    return hex.toUpper();
}

struct CidFixture {
    QString path;
    uint32_t gA = 0, gB = 0, gC = 0;
    QByteArray program;
};

// Type0 /CIDFontType2 with /CIDToGIDMap /Identity over a real host TTF;
// codes in the content stream ARE glyph IDs (the dominant full-embed shape).
// `requireGlyphs=false` builds with fixed codes for tests that never render
// (the corrupt-program pin) and must not depend on the cmap.
CidFixture buildCidFixture(const QTemporaryDir& tmp, const QString& name,
                           const QByteArray& ttf, bool streamCidMap = false,
                           bool requireGlyphs = true)
{
    CidFixture fx;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(ttf.constData());
    fx.gA = cmapLookup(p, static_cast<size_t>(ttf.size()), 'A');
    fx.gB = cmapLookup(p, static_cast<size_t>(ttf.size()), 'B');
    fx.gC = cmapLookup(p, static_cast<size_t>(ttf.size()), 'C');
    if (!requireGlyphs) {
        fx.gA = 1; fx.gB = 2; fx.gC = 3;
    } else if (fx.gA == 0 || fx.gB == 0 || fx.gC == 0) {
        return fx; // caller QSKIPs
    }

    PoDoFo::PdfMemDocument doc;
    doc.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    auto& page = doc.GetPages().GetPageAt(0);

    auto& ff2 = makeFontFile2Stream(doc, ttf);
    auto& fd = makeFontDescriptor(doc, ff2, "FontFile2");

    auto& cidFont = doc.GetObjects().CreateDictionaryObject();
    cidFont.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
    cidFont.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("CIDFontType2"));
    cidFont.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("Fixture"));
    auto& csi = doc.GetObjects().CreateDictionaryObject();
    csi.GetDictionary().AddKey("Registry", PoDoFo::PdfString("Adobe"));
    csi.GetDictionary().AddKey("Ordering", PoDoFo::PdfString("Identity"));
    csi.GetDictionary().AddKey("Supplement", static_cast<int64_t>(0));
    cidFont.GetDictionary().AddKey("CIDSystemInfo", csi.GetIndirectReference());
    cidFont.GetDictionary().AddKey("FontDescriptor", fd.GetIndirectReference());
    cidFont.GetDictionary().AddKey("DW", static_cast<int64_t>(1000));
    if (streamCidMap) {
        // CID n → GID (n == 1,2,3) → (gA,gB,gC); a 2-bytes-per-CID table.
        QByteArray map(8, '\0');
        unsigned char* m = reinterpret_cast<unsigned char*>(map.data());
        wr16(m + 2, static_cast<uint16_t>(fx.gA));
        wr16(m + 4, static_cast<uint16_t>(fx.gB));
        wr16(m + 6, static_cast<uint16_t>(fx.gC));
        auto& mapObj = doc.GetObjects().CreateDictionaryObject();
        PoDoFo::charbuff buf(std::string_view(map.constData(),
                                              static_cast<size_t>(map.size())));
        mapObj.GetOrCreateStream().SetData(buf, PoDoFo::PdfFilterList{}, /*raw=*/true);
        cidFont.GetDictionary().AddKey("CIDToGIDMap", mapObj.GetIndirectReference());
    } else {
        cidFont.GetDictionary().AddKey("CIDToGIDMap", PoDoFo::PdfName("Identity"));
    }

    auto& type0 = doc.GetObjects().CreateDictionaryObject();
    type0.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
    type0.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Type0"));
    type0.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("Fixture"));
    type0.GetDictionary().AddKey("Encoding", PoDoFo::PdfName("Identity-H"));
    PoDoFo::PdfArray descendants;
    descendants.Add(PoDoFo::PdfObject(cidFont.GetIndirectReference()));
    type0.GetDictionary().AddKey("DescendantFonts", PoDoFo::PdfVariant(descendants));

    // ToUnicode mapping the shown CIDs to A/B/C (pinned byte-identical later).
    QString toUni = QStringLiteral(
        "/CIDInit /ProcSet findresource begin\n"
        "12 dict begin\nbegincmap\n"
        "1 begincodespacerange <0000> <FFFF> endcodespacerange\n"
        "3 beginbfchar\n"
        "<%1> <0041>\n<%2> <0042>\n<%3> <0043>\n"
        "endbfchar\nendcmap\nend\nend\n")
        .arg(toHexCodes({ streamCidMap ? 1u : fx.gA }),
             toHexCodes({ streamCidMap ? 2u : fx.gB }),
             toHexCodes({ streamCidMap ? 3u : fx.gC }));
    const QByteArray toUniB = toUni.toLatin1();
    auto& toUniObj = doc.GetObjects().CreateDictionaryObject();
    PoDoFo::charbuff tuBuf(std::string_view(toUniB.constData(),
                                            static_cast<size_t>(toUniB.size())));
    toUniObj.GetOrCreateStream().SetData(tuBuf, PoDoFo::PdfFilterList{}, /*raw=*/true);
    type0.GetDictionary().AddKey("ToUnicode", toUniObj.GetIndirectReference());

    // W array (pinned structurally identical later).
    PoDoFo::PdfArray w;
    PoDoFo::PdfArray widths;
    widths.Add(PoDoFo::PdfVariant(static_cast<int64_t>(600)));
    w.Add(PoDoFo::PdfVariant(static_cast<int64_t>(fx.gA)));
    w.Add(PoDoFo::PdfVariant(widths));
    type0.GetDictionary().AddKey("W", PoDoFo::PdfVariant(w));

    // Draw A B A C as 2-byte codes: GIDs when CIDToGIDMap is Identity (CID ==
    // GID), raw CIDs 1..3 when the stream map carries the CID-to-GID binding.
    const QVector<uint32_t> codes = streamCidMap
        ? QVector<uint32_t>{ 1, 2, 3 } : QVector<uint32_t>{ fx.gA, fx.gB, fx.gC };
    const std::string content =
        "BT /F1 24 Tf 72 700 Td <" + toHexCodes({ codes[0], codes[1] }).toStdString()
        + "> Tj 0 -30 Td <" + toHexCodes({ codes[0], codes[2] }).toStdString() + "> Tj ET\n";
    addContentStream(doc, page, content, &type0, "F1");

    fx.path = tmp.filePath(name);
    doc.Save(fx.path.toUtf8().constData());
    fx.program = ttf;
    return fx;
}

// Simple /TrueType with NO /Encoding (built-in cmap is the provable path);
// 1-byte codes drawn straight into the content stream.
struct SimpleFixture {
    QString path;
    uint32_t gA = 0, gComposite = 0;
    unsigned char compositeByte = 0; // the raw 1-byte code drawn
    bool ok = false;
};

SimpleFixture buildSimpleFixture(const QTemporaryDir& tmp, const QString& name,
                                 const QByteArray& ttf, bool drawComposite,
                                 unsigned char compositeByte = 0)
{
    SimpleFixture fx;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(ttf.constData());
    fx.gA = cmapLookup(p, static_cast<size_t>(ttf.size()), 'A');
    fx.gComposite = cmapLookup(p, static_cast<size_t>(ttf.size()),
                               compositeByte ? compositeByte : 0xE7);
    if (compositeByte != 0) fx.compositeByte = compositeByte;
    if (fx.gA == 0) return fx;

    PoDoFo::PdfMemDocument doc;
    doc.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    auto& page = doc.GetPages().GetPageAt(0);
    auto& ff2 = makeFontFile2Stream(doc, ttf);
    auto& fd = makeFontDescriptor(doc, ff2, "FontFile2");
    auto& font = doc.GetObjects().CreateDictionaryObject();
    font.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
    font.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("TrueType"));
    font.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("Fixture"));
    font.GetDictionary().AddKey("FirstChar", static_cast<int64_t>(65));
    font.GetDictionary().AddKey("LastChar", static_cast<int64_t>(67));
    PoDoFo::PdfArray widths;
    widths.Add(PoDoFo::PdfVariant(static_cast<int64_t>(600)));
    widths.Add(PoDoFo::PdfVariant(static_cast<int64_t>(600)));
    widths.Add(PoDoFo::PdfVariant(static_cast<int64_t>(600)));
    font.GetDictionary().AddKey("Widths", PoDoFo::PdfVariant(widths));
    font.GetDictionary().AddKey("FontDescriptor", fd.GetIndirectReference());
    // Deliberately NO /Encoding key: the built-in cmap is what a conforming
    // reader consults, and what the subsetter can prove.

    std::string content = "BT /F1 24 Tf 72 700 Td (A) Tj ";
    if (drawComposite) {
        if (fx.gComposite == 0) return fx; // caller falls back
        // The RAW single byte (ç in the font's built-in (3,1) cmap).
        // Never write UTF-8 here — the code is one byte wide.
        // Hex string: PoDoFo's content tokenizer mangles high-byte LITERAL
        // strings into an UnexpectedKeyword, while hex strings parse cleanly.
        content += QString("<%1> Tj").arg(static_cast<uint>(fx.compositeByte), 2, 16,
                                          QLatin1Char('0')).toUpper().toStdString();
    }
    content += "ET\n";
    addContentStream(doc, page, content, &font, "F1");

    fx.path = tmp.filePath(name);
    doc.Save(fx.path.toUtf8().constData());
    fx.ok = true;
    return fx;
}

// ── CFF document fixtures (hermetic: the synthetic programs above) ────────────

// Type0 /CIDFontType0 with /Identity-H over a bare CID-keyed CFF
// (FontFile3 /CIDFontType0C): 2-byte codes ARE CIDs; the charset maps
// GID1..5 → CIDs 7..11, so shown CIDs 7/9/11 select GIDs 1/3/5.
struct CffCidFixture {
    QString path;
    QByteArray cff;
};

CffCidFixture buildCidCffFixture(const QTemporaryDir& tmp, const QString& name,
                                 const QByteArray& cff)
{
    CffCidFixture fx;
    fx.cff = cff;

    PoDoFo::PdfMemDocument doc;
    doc.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    auto& page = doc.GetPages().GetPageAt(0);

    auto& ff3 = doc.GetObjects().CreateDictionaryObject();
    ff3.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("CIDFontType0C"));
    {
        PoDoFo::charbuff buf(
            std::string_view(cff.constData(), static_cast<size_t>(cff.size())));
        ff3.GetOrCreateStream().SetData(buf,
            PoDoFo::PdfFilterList{ PoDoFo::PdfFilterType::FlateDecode }, /*raw=*/false);
    }
    auto& fd = makeFontDescriptor(doc, ff3, "FontFile3");

    auto& cidFont = doc.GetObjects().CreateDictionaryObject();
    cidFont.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
    cidFont.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("CIDFontType0"));
    cidFont.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("FixtureCFF"));
    auto& csi = doc.GetObjects().CreateDictionaryObject();
    csi.GetDictionary().AddKey("Registry", PoDoFo::PdfString("Adobe"));
    csi.GetDictionary().AddKey("Ordering", PoDoFo::PdfString("Identity"));
    csi.GetDictionary().AddKey("Supplement", static_cast<int64_t>(0));
    cidFont.GetDictionary().AddKey("CIDSystemInfo", csi.GetIndirectReference());
    cidFont.GetDictionary().AddKey("FontDescriptor", fd.GetIndirectReference());
    cidFont.GetDictionary().AddKey("DW", static_cast<int64_t>(600));
    PoDoFo::PdfArray w;
    PoDoFo::PdfArray widths;
    widths.Add(PoDoFo::PdfVariant(static_cast<int64_t>(600)));
    w.Add(PoDoFo::PdfVariant(static_cast<int64_t>(7)));
    w.Add(PoDoFo::PdfVariant(widths));
    cidFont.GetDictionary().AddKey("W", PoDoFo::PdfVariant(w));

    auto& type0 = doc.GetObjects().CreateDictionaryObject();
    type0.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
    type0.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Type0"));
    type0.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("FixtureCFF"));
    type0.GetDictionary().AddKey("Encoding", PoDoFo::PdfName("Identity-H"));
    PoDoFo::PdfArray descendants;
    descendants.Add(PoDoFo::PdfObject(cidFont.GetIndirectReference()));
    type0.GetDictionary().AddKey("DescendantFonts", PoDoFo::PdfVariant(descendants));

    QString toUni = QStringLiteral(
        "/CIDInit /ProcSet findresource begin\n"
        "12 dict begin\nbegincmap\n"
        "1 begincodespacerange <0000> <FFFF> endcodespacerange\n"
        "3 beginbfchar\n"
        "<0007> <0041>\n<0009> <0042>\n<000B> <0043>\n"
        "endbfchar\nendcmap\nend\nend\n");
    const QByteArray toUniB = toUni.toLatin1();
    auto& toUniObj = doc.GetObjects().CreateDictionaryObject();
    PoDoFo::charbuff tuBuf(std::string_view(toUniB.constData(),
                                            static_cast<size_t>(toUniB.size())));
    toUniObj.GetOrCreateStream().SetData(tuBuf, PoDoFo::PdfFilterList{}, /*raw=*/true);
    type0.GetDictionary().AddKey("ToUnicode", toUniObj.GetIndirectReference());

    const std::string content =
        "BT /F1 24 Tf 72 700 Td <0007> Tj 0 -30 Td <0009> Tj 0 -30 Td <000B> Tj ET\n";
    addContentStream(doc, page, content, &type0, "F1");

    fx.path = tmp.filePath(name);
    doc.Save(fx.path.toUtf8().constData());
    return fx;
}

// Simple /Type1 over a bare name-keyed CFF (FontFile3 /Type1C); 1-byte codes
// resolve through the CFF's built-in encoding (no PDF /Encoding) unless
// `namedEncoding` adds one (the unprovable-disclosure case). `subFontMatrix`
// adds a non-default /FontMatrix (the sub-FontMatrix-disclosure case).
QString buildSimpleCffFixture(const QTemporaryDir& tmp, const QString& name,
                              const QByteArray& cff, bool namedEncoding,
                              bool subFontMatrix = false)
{
    PoDoFo::PdfMemDocument doc;
    doc.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    auto& page = doc.GetPages().GetPageAt(0);

    auto& ff3 = doc.GetObjects().CreateDictionaryObject();
    ff3.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Type1C"));
    {
        PoDoFo::charbuff buf(
            std::string_view(cff.constData(), static_cast<size_t>(cff.size())));
        ff3.GetOrCreateStream().SetData(buf,
            PoDoFo::PdfFilterList{ PoDoFo::PdfFilterType::FlateDecode }, /*raw=*/false);
    }
    auto& fd = makeFontDescriptor(doc, ff3, "FontFile3");
    auto& font = doc.GetObjects().CreateDictionaryObject();
    font.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
    font.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Type1"));
    font.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("FixtureCFF"));
    font.GetDictionary().AddKey("FirstChar", static_cast<int64_t>(65));
    font.GetDictionary().AddKey("LastChar", static_cast<int64_t>(67));
    PoDoFo::PdfArray widths;
    for (int i = 0; i < 3; ++i)
        widths.Add(PoDoFo::PdfVariant(static_cast<int64_t>(600)));
    font.GetDictionary().AddKey("Widths", PoDoFo::PdfVariant(widths));
    if (namedEncoding)
        font.GetDictionary().AddKey("Encoding", PoDoFo::PdfName("WinAnsiEncoding"));
    if (subFontMatrix) {
        // [0.0005 0 0 0.0005 0 0] — half the default glyph scale.
        PoDoFo::PdfArray fm;
        fm.Add(PoDoFo::PdfVariant(0.0005));
        fm.Add(PoDoFo::PdfVariant(0.0));
        fm.Add(PoDoFo::PdfVariant(0.0));
        fm.Add(PoDoFo::PdfVariant(0.0005));
        fm.Add(PoDoFo::PdfVariant(0.0));
        fm.Add(PoDoFo::PdfVariant(0.0));
        font.GetDictionary().AddKey("FontMatrix", PoDoFo::PdfVariant(fm));
    }
    font.GetDictionary().AddKey("FontDescriptor", fd.GetIndirectReference());

    addContentStream(doc, page, "BT /F1 24 Tf 72 700 Td (AC) Tj ET\n", &font, "F1");

    const QString path = tmp.filePath(name);
    doc.Save(path.toUtf8().constData());
    return path;
}

// Simple /OpenType over an OTTO-wrapped CFF (FontFile3 /OpenType): 1-byte
// codes resolve through the sfnt cmap (no PDF /Encoding).
QString buildOpenTypeCffFixture(const QTemporaryDir& tmp, const QString& name,
                                const QByteArray& otto)
{
    PoDoFo::PdfMemDocument doc;
    doc.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    auto& page = doc.GetPages().GetPageAt(0);

    auto& ff3 = doc.GetObjects().CreateDictionaryObject();
    ff3.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("OpenType"));
    {
        PoDoFo::charbuff buf(
            std::string_view(otto.constData(), static_cast<size_t>(otto.size())));
        ff3.GetOrCreateStream().SetData(buf,
            PoDoFo::PdfFilterList{ PoDoFo::PdfFilterType::FlateDecode }, /*raw=*/false);
    }
    auto& fd = makeFontDescriptor(doc, ff3, "FontFile3");
    auto& font = doc.GetObjects().CreateDictionaryObject();
    font.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
    font.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("OpenType"));
    font.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("FixtureOTF"));
    font.GetDictionary().AddKey("FirstChar", static_cast<int64_t>(65));
    font.GetDictionary().AddKey("LastChar", static_cast<int64_t>(67));
    PoDoFo::PdfArray widths;
    for (int i = 0; i < 3; ++i)
        widths.Add(PoDoFo::PdfVariant(static_cast<int64_t>(600)));
    font.GetDictionary().AddKey("Widths", PoDoFo::PdfVariant(widths));
    font.GetDictionary().AddKey("FontDescriptor", fd.GetIndirectReference());

    addContentStream(doc, page, "BT /F1 24 Tf 72 700 Td (AC) Tj ET\n", &font, "F1");

    const QString path = tmp.filePath(name);
    doc.Save(path.toUtf8().constData());
    return path;
}

// ── test-side CFF/sfnt readers for byte-level after-state assertions ──────────

struct TestIndex {
    uint32_t count = 0;
    std::vector<std::pair<uint32_t, uint32_t>> spans; // absolute [start,end) per entry
};

TestIndex readCffIndexAt(const QByteArray& cff, uint32_t indexOffset)
{
    TestIndex ix;
    const unsigned char* p =
        reinterpret_cast<const unsigned char*>(cff.constData()) + indexOffset;
    ix.count = rd16(p);
    if (ix.count == 0) return ix;
    const int offSize = p[2];
    const uint32_t base = indexOffset + 3u + (ix.count + 1u) * offSize - 1u;
    // INDEX offsets are 1-based relative to `base` and the first one is
    // spec-mandated to be 1 — so entry 0 starts at base+1, not base.
    uint32_t prev = 1;
    for (uint32_t i = 1; i <= ix.count; ++i) {
        uint32_t o = 0;
        for (int s = 0; s < offSize; ++s)
            o = (o << 8) | p[3 + i * offSize + s];
        ix.spans.push_back({ base + prev, base + o });
        prev = o;
    }
    return ix;
}

std::map<uint32_t, std::pair<uint32_t, uint32_t>> readSfntTables(const QByteArray& sfnt)
{
    // Version-agnostic directory read (accepts OTTO — parseSfntDirectory
    // deliberately does not; the TrueType lane's pin keeps that contract).
    // Values are [start, end) spans — same convention as readCffIndexAt.
    std::map<uint32_t, std::pair<uint32_t, uint32_t>> out;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(sfnt.constData());
    const uint16_t numTables = rd16(p + 4);
    for (uint16_t i = 0; i < numTables; ++i) {
        const unsigned char* rec = p + 12 + static_cast<size_t>(i) * 16;
        const uint32_t off = rd32(rec + 8);
        out[rd32(rec)] = { off, off + rd32(rec + 12) };
    }
    return out;
}

PoDoFo::PdfObject* resolveObject(PoDoFo::PdfMemDocument& doc, PoDoFo::PdfObject* obj)
{
    if (obj && obj->IsReference())
        obj = doc.GetObjects().GetObject(obj->GetReference());
    return obj;
}

QVector<PoDoFo::PdfObject*> findFontFileStreams(PoDoFo::PdfMemDocument& doc,
                                                const char* key)
{
    QVector<PoDoFo::PdfObject*> out;
    for (auto obj : doc.GetObjects()) {
        if (!obj->IsDictionary()) continue;
        auto* type = obj->GetDictionary().FindKey("Type");
        if (!type || !type->IsName() || type->GetName().GetString() != "FontDescriptor")
            continue;
        auto* ff = resolveObject(doc, obj->GetDictionary().FindKey(key));
        if (ff && ff->HasStream()) out.append(ff);
    }
    return out;
}

QByteArray decodedStream(PoDoFo::PdfObject* obj)
{
    if (!obj || !obj->HasStream()) return {};
    PoDoFo::charbuff buf;
    obj->GetOrCreateStream().CopyTo(buf);
    return QByteArray(buf.data(), static_cast<qsizetype>(buf.size()));
}

qint64 encodedStreamSize(PoDoFo::PdfObject* obj)
{
    if (!obj || !obj->HasStream()) return -1;
    PoDoFo::charbuff buf;
    obj->GetOrCreateStream().CopyTo(buf, /*raw=*/true);
    return static_cast<qint64>(buf.size());
}

// Fraction of pixels differing by more than `threshold` per channel.
double imageDiffFraction(const QImage& a, const QImage& b, int threshold)
{
    if (a.size() != b.size()) return 1.0;
    const QImage x = a.convertToFormat(QImage::Format_RGB32);
    const QImage y = b.convertToFormat(QImage::Format_RGB32);
    qint64 diff = 0;
    const qint64 total = static_cast<qint64>(x.width()) * x.height();
    for (int r = 0; r < x.height(); ++r) {
        const QRgb* lx = reinterpret_cast<const QRgb*>(x.scanLine(r));
        const QRgb* ly = reinterpret_cast<const QRgb*>(y.scanLine(r));
        for (int c = 0; c < x.width(); ++c) {
            if (qAbs(qRed(lx[c]) - qRed(ly[c])) > threshold
                || qAbs(qGreen(lx[c]) - qGreen(ly[c])) > threshold
                || qAbs(qBlue(lx[c]) - qBlue(ly[c])) > threshold)
                ++diff;
        }
    }
    return total == 0 ? 0.0 : static_cast<double>(diff) / total;
}

OptimizeOptions subsetOnlyOptions()
{
    OptimizeOptions o;
    o.downsampleImages = false;
    o.deduplicateImages = false;
    o.subsetFonts = true;
    o.removeUnusedObjects = false;
    o.stripMetadata = false;
    return o;
}

} // namespace

class TestFontSubset : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    QString tmpPath(const QString& name) const { return m_tmpDir.filePath(name); }

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Failed to create temp directory");
    }

    // ── 1. sfnt surgery (hermetic) ────────────────────────────────────────────

    void sfntDirectoryRejectsCollectionsAndCff() {
        QByteArray otto = makeSyntheticSfnt();
        otto[0] = 'O'; otto[1] = 'T'; otto[2] = 'T'; otto[3] = 'O';
        QVector<SfntTableEntry> tables;
        QVERIFY2(!parseSfntDirectory(reinterpret_cast<const unsigned char*>(otto.constData()),
                                     static_cast<size_t>(otto.size()), tables),
                 "OTTO (CFF) must not enter the TrueType lane");
        QByteArray ttcf = makeSyntheticSfnt();
        ttcf[0] = 't'; ttcf[1] = 't'; ttcf[2] = 'c'; ttcf[3] = 'f';
        QVERIFY2(!parseSfntDirectory(reinterpret_cast<const unsigned char*>(ttcf.constData()),
                                     static_cast<size_t>(ttcf.size()), tables),
                 "ttcf collections must not enter the single-sfnt lane");
    }

    void blankingPreservesNumberingAndKeptGlyphs() {
        const QByteArray program = makeSyntheticSfnt();
        QCOMPARE(numGlyphsOf(program), 5u);
        // Baseline spans: g0 empty, g1 12B, g2 8B, g3 16B, g4 20B.
        const auto s1 = spanOf(program, 1);
        QCOMPARE(s1.second - s1.first, 12u);

        QSet<uint32_t> keep{ 0, 2, 3 };
        QByteArray out;
        QVERIFY2(blankUnusedGlyphs(reinterpret_cast<const unsigned char*>(program.constData()),
                                   static_cast<size_t>(program.size()), keep, out),
                 "the rewriter must accept its own synthetic fixture");
        // Numbering preserved: same glyph count.
        QCOMPARE(numGlyphsOf(out), 5u);
        // Kept glyphs keep their exact outline bytes.
        const auto inSpan2 = spanOf(program, 2);
        const auto outSpan2 = spanOf(out, 2);
        QCOMPARE(outSpan2.second - outSpan2.first, inSpan2.second - inSpan2.first);
        QVERIFY(std::memcmp(program.constData() + inSpan2.first,
                            out.constData() + outSpan2.first,
                            inSpan2.second - inSpan2.first) == 0);
        const auto inSpan3 = spanOf(program, 3);
        const auto outSpan3 = spanOf(out, 3);
        QCOMPARE(outSpan3.second - outSpan3.first, inSpan3.second - inSpan3.first);
        QVERIFY(std::memcmp(program.constData() + inSpan3.first,
                            out.constData() + outSpan3.first,
                            inSpan3.second - inSpan3.first) == 0);
        // Blanked glyphs collapse to zero-length spans.
        QCOMPARE(spanOf(out, 1).second - spanOf(out, 1).first, 0u);
        QCOMPARE(spanOf(out, 4).second - spanOf(out, 4).first, 0u);
        QCOMPARE(spanOf(out, 0).second - spanOf(out, 0).first, 0u);
        // Untouched tables stay byte-identical (maxp).
        QVector<SfntTableEntry> inTables, outTables;
        const unsigned char* ip = reinterpret_cast<const unsigned char*>(program.constData());
        const unsigned char* op = reinterpret_cast<const unsigned char*>(out.constData());
        QVERIFY(parseSfntDirectory(ip, static_cast<size_t>(program.size()), inTables));
        QVERIFY(parseSfntDirectory(op, static_cast<size_t>(out.size()), outTables));
        const SfntTableEntry* inMaxp = tableOf(inTables, kMaxp);
        const SfntTableEntry* outMaxp = tableOf(outTables, kMaxp);
        QVERIFY(inMaxp && outMaxp);
        QCOMPARE(static_cast<int>(outMaxp->length), static_cast<int>(inMaxp->length));
        QVERIFY(std::memcmp(ip + inMaxp->offset, op + outMaxp->offset, inMaxp->length) == 0);
        // head survives intact except checkSumAdjustment (recomputed).
        const SfntTableEntry* inHead = tableOf(inTables, kHead);
        const SfntTableEntry* outHead = tableOf(outTables, kHead);
        QVERIFY(inHead && outHead);
        QVERIFY(std::memcmp(ip + inHead->offset + 12, op + outHead->offset + 12,
                            inHead->length - 12) == 0);
        // Whole-font checksum identity: with the adjustment zeroed, the font
        // checksum must equal 0xB1B0AFBA - adjustment (ISO 14496-6).
        uint32_t adjustment = rd32(op + outHead->offset + 8);
        wr32(const_cast<unsigned char*>(op) + outHead->offset + 8, 0);
        // tableChecksum equivalent, computed inline over the whole output.
        uint32_t sum = 0;
        for (int i = 0; i + 4 <= out.size(); i += 4)
            sum += rd32(op + i);
        if (out.size() % 4 != 0) {
            unsigned char tail[4] = { 0, 0, 0, 0 };
            std::memcpy(tail, op + (out.size() / 4) * 4,
                         static_cast<size_t>(out.size() % 4));
            sum += rd32(tail);
        }
        wr32(const_cast<unsigned char*>(op) + outHead->offset + 8, adjustment);
        QCOMPARE(sum, 0xB1B0AFBAu - adjustment);
    }

    void compositeClosurePullsComponentChain() {
        const QByteArray program = makeSyntheticSfnt();
        // g3 is a composite referencing g1.
        QSet<uint32_t> used{ 3 };
        QVERIFY(expandCompositeClosure(
            reinterpret_cast<const unsigned char*>(program.constData()),
            static_cast<size_t>(program.size()), used));
        QVERIFY2(used.contains(1), "the component glyph must join the keep set");
        QVERIFY2(used.contains(0), ".notdef must always survive");
        QVERIFY(used.contains(3));
        // Blankable bytes with closure applied: g2 (8B) + g4 (20B) = 28.
        bool ok = false;
        const qint64 blankable = blankableGlyfBytes(
            reinterpret_cast<const unsigned char*>(program.constData()),
            static_cast<size_t>(program.size()), used, &ok);
        QVERIFY(ok);
        QCOMPARE(blankable, static_cast<qint64>(28));
    }

    // ── 2/3. document round-trip + render-diff (real host TTF) ────────────────

    void roundTripCidFontType2PreservesUsedAndBlanksUnused() {
        const QString ttfPath = pickHostTtf();
        if (ttfPath.isEmpty())
            QSKIP("no host TrueType font available — document fixtures not constructible");
        const QByteArray ttf = hostTtfBytes(ttfPath);
        const CidFixture fx = buildCidFixture(m_tmpDir, QStringLiteral("cid_id.pdf"), ttf);
        if (fx.path.isEmpty())
            QSKIP("host font lacks the fixture glyphs in its cmap");

        // ── estimator honesty, part 1: the estimate claims subset savings only
        // when the pass will run, and never more than the programs occupy.
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(fx.path));
        OptimizeOptions estOpts = subsetOnlyOptions();
        estOpts.subsetFonts = false;
        const OptimizeEstimate off = engine.estimateOptimization(estOpts);
        estOpts.subsetFonts = true;
        const OptimizeEstimate on = engine.estimateOptimization(estOpts);
        QCOMPARE(off.estimatedBytes, off.originalBytes);
        QVERIFY2(on.estimatedBytes < on.originalBytes,
                 "an eligible FontFile2 program must yield a real estimate");
        QVERIFY2(on.originalBytes - on.estimatedBytes <= on.originalBytes,
                 "the claim is bounded by the document size");
        // Never claims more than the FontFile2 streams occupy in the file.
        {
            PoDoFo::PdfMemDocument probe;
            probe.Load(fx.path.toUtf8().constData());
            qint64 encoded = 0;
            for (auto* ff : findFontFileStreams(probe, "FontFile2"))
                encoded += encodedStreamSize(ff);
            QVERIFY2(encoded > 0, "the fixture must carry an embedded FontFile2");
            QVERIFY2(on.originalBytes - on.estimatedBytes <= encoded,
                     "the subset claim must never exceed the programs' encoded size");
        }

        // ── capture the before state.
        PoDoFo::PdfMemDocument before;
        before.Load(fx.path.toUtf8().constData());
        auto* ff2Before = findFontFileStreams(before, "FontFile2").first();
        const QByteArray programBefore = decodedStream(ff2Before);
        QVERIFY(!programBefore.isEmpty());
        qint64 encodedBefore = encodedStreamSize(ff2Before);
        // Content stream + ToUnicode + W + CIDToGIDMap (Identity is a name).
        PoDoFo::PdfObject* font0Before = nullptr;
        for (auto obj : before.GetObjects()) {
            if (!obj->IsDictionary()) continue;
            auto* st = obj->GetDictionary().FindKey("Subtype");
            if (st && st->IsName() && st->GetName().GetString() == "Type0")
                font0Before = obj;
        }
        QVERIFY(font0Before);
        auto contentsBefore = before.GetPages().GetPageAt(0).GetContents();
        QVERIFY(contentsBefore);
        PoDoFo::charbuff cb;
        contentsBefore->CopyTo(cb);
        const QByteArray contentBefore(cb.data(), static_cast<qsizetype>(cb.size()));
        auto* toUniBefore = font0Before->GetDictionary().FindKey("ToUnicode");
        QVERIFY(toUniBefore);
        const QByteArray toUniBeforeBytes =
            decodedStream(resolveObject(before, toUniBefore));
        const QByteArray wBefore = QString::fromUtf8(
            font0Before->GetDictionary().FindKey("W")->ToString().c_str()).toUtf8();

        PdfiumBackend rendererBefore;
        QVERIFY(rendererBefore.loadDocument(fx.path));
        const QImage pageBefore = rendererBefore.renderPage(0, 150);
        QVERIFY(!pageBefore.isNull());
        const QString textBefore = rendererBefore.extractText(0);

        // ── run the pass.
        OptimizeOptions opts = subsetOnlyOptions();
        const QString outPath = tmpPath(QStringLiteral("cid_id_out.pdf"));
        QVERIFY2(engine.optimizeDocument(outPath, opts),
                 "optimizeDocument must succeed with subsetting");
        QVERIFY(QFileInfo(outPath).size() < QFileInfo(fx.path).size());

        // ── the after state.
        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff2After = findFontFileStreams(after, "FontFile2").first();
        const QByteArray programAfter = decodedStream(ff2After);
        QVERIFY(!programAfter.isEmpty());
        QVERIFY2(programAfter.size() < programBefore.size(),
                 "the decoded program must shrink (unused outlines removed)");
        QVERIFY2(encodedStreamSize(ff2After) < encodedBefore,
                 "the encoded FontFile2 stream must shrink");
        // Content streams, /ToUnicode and /W stay byte-identical.
        auto* font0After = static_cast<PoDoFo::PdfObject*>(nullptr);
        for (auto obj : after.GetObjects()) {
            if (!obj->IsDictionary()) continue;
            auto* st = obj->GetDictionary().FindKey("Subtype");
            if (st && st->IsName() && st->GetName().GetString() == "Type0")
                font0After = obj;
        }
        QVERIFY(font0After);
        auto contentsAfter = after.GetPages().GetPageAt(0).GetContents();
        QVERIFY(contentsAfter);
        PoDoFo::charbuff ca;
        contentsAfter->CopyTo(ca);
        const QByteArray contentAfter(ca.data(), static_cast<qsizetype>(ca.size()));
        QCOMPARE(contentAfter, contentBefore);
        auto* toUniAfter = font0After->GetDictionary().FindKey("ToUnicode");
        QVERIFY(toUniAfter);
        QCOMPARE(decodedStream(resolveObject(after, toUniAfter)),
                 toUniBeforeBytes);
        QCOMPARE(QString::fromUtf8(
                     font0After->GetDictionary().FindKey("W")->ToString().c_str()),
                 QString::fromUtf8(
                     font0Before->GetDictionary().FindKey("W")->ToString().c_str()));

        // Used glyphs keep outlines; an unused one (largest span outside the
        // closure) is blanked; glyph count is preserved.
        QCOMPARE(numGlyphsOf(programAfter), numGlyphsOf(programBefore));
        for (uint32_t gid : { fx.gA, fx.gB, fx.gC }) {
            const auto span = spanOf(programAfter, gid);
            QVERIFY2(span.second > span.first,
                     "every shown glyph must keep its outline");
        }
        QSet<uint32_t> keep{ fx.gA, fx.gB, fx.gC };
        QVERIFY(expandCompositeClosure(
            reinterpret_cast<const unsigned char*>(programBefore.constData()),
            static_cast<size_t>(programBefore.size()), keep));
        uint32_t victim = 0;
        qint64 victimSpan = 0;
        for (uint32_t gid = 0; gid < numGlyphsOf(programBefore); ++gid) {
            if (keep.contains(gid)) continue;
            const auto span = spanOf(programBefore, gid);
            const qint64 len = static_cast<qint64>(span.second) - span.first;
            if (len > victimSpan) { victimSpan = len; victim = gid; }
        }
        QVERIFY2(victimSpan > 100,
                 "the fixture font must have a sizable unused glyph");
        const auto victimAfter = spanOf(programAfter, victim);
        QCOMPARE(victimAfter.second - victimAfter.first, 0u);

        // ── render-diff + extraction identity (the blanked-used-glyph class).
        PdfiumBackend rendererAfter;
        QVERIFY(rendererAfter.loadDocument(outPath));
        const QImage pageAfter = rendererAfter.renderPage(0, 150);
        QVERIFY(!pageAfter.isNull());
        const double diff = imageDiffFraction(pageBefore, pageAfter, 16);
        QVERIFY2(diff < 0.001,
                 qPrintable(QStringLiteral(
                     "the page must render identically after subsetting "
                     "(differing pixel fraction %1)").arg(diff)));
        QCOMPARE(rendererAfter.extractText(0), textBefore);
    }

    void cidToGidMapStreamIsHonored() {
        const QString ttfPath = pickHostTtf();
        if (ttfPath.isEmpty())
            QSKIP("no host TrueType font available");
        const QByteArray ttf = hostTtfBytes(ttfPath);
        const CidFixture fx =
            buildCidFixture(m_tmpDir, QStringLiteral("cid_map.pdf"), ttf,
                            /*streamCidMap=*/true);
        if (fx.path.isEmpty())
            QSKIP("host font lacks the fixture glyphs in its cmap");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(fx.path));
        const QString outPath = tmpPath(QStringLiteral("cid_map_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));

        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff2 = findFontFileStreams(after, "FontFile2").first();
        const QByteArray program = decodedStream(ff2);
        // The used CIDs (1,2,3) map to gA,gB,gC through the stream — those
        // glyph outlines must have survived.
        for (uint32_t gid : { fx.gA, fx.gB, fx.gC }) {
            const auto span = spanOf(program, gid);
            QVERIFY2(span.second > span.first, "stream-mapped used glyph blanked");
        }
        QVERIFY(program.size() < static_cast<qint64>(ttf.size()));
    }

    void simpleTrueTypeBuiltinCmapRoundTrip() {
        const QString ttfPath = pickHostTtf();
        if (ttfPath.isEmpty())
            QSKIP("no host TrueType font available");
        const QByteArray ttf = hostTtfBytes(ttfPath);
        const SimpleFixture fx =
            buildSimpleFixture(m_tmpDir, QStringLiteral("simple.pdf"), ttf,
                               /*drawComposite=*/false);
        if (!fx.ok)
            QSKIP("host font lacks the fixture glyphs in its cmap");

        PdfiumBackend rendererBefore;
        QVERIFY(rendererBefore.loadDocument(fx.path));
        const QString textBefore = rendererBefore.extractText(0);
        const QImage pageBefore = rendererBefore.renderPage(0, 150);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(fx.path));
        const QString outPath = tmpPath(QStringLiteral("simple_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));

        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff2 = findFontFileStreams(after, "FontFile2").first();
        const QByteArray program = decodedStream(ff2);
        QVERIFY(program.size() < static_cast<qint64>(ttf.size()));
        // Shown code 'A' keeps its glyph.
        const auto spanA = spanOf(program, fx.gA);
        QVERIFY(spanA.second > spanA.first);

        PdfiumBackend rendererAfter;
        QVERIFY(rendererAfter.loadDocument(outPath));
        QCOMPARE(rendererAfter.extractText(0), textBefore);
        const double diff =
            imageDiffFraction(pageBefore, rendererAfter.renderPage(0, 150), 16);
        QVERIFY2(diff < 0.001,
                 qPrintable(QStringLiteral("render-diff fraction %1").arg(diff)));
    }

    void compositeGlyphComponentSurvivesViaClosure() {
        const QString ttfPath = pickHostTtf();
        if (ttfPath.isEmpty())
            QSKIP("no host TrueType font available");
        const QByteArray ttf = hostTtfBytes(ttfPath);
        const unsigned char* p = reinterpret_cast<const unsigned char*>(ttf.constData());

        // Pick an accented candidate that actually exercises the closure in
        // THIS font: mapped, with a real outline, and composite (its closure
        // adds at least one component glyph).
        unsigned char chosenByte = 0;
        uint32_t chosenGid = 0;
        uint32_t component = 0xFFFFFFFFu;
        for (unsigned code : { 0xE7u, 0xE0u, 0xE1u, 0xE8u, 0xE9u, 0xF2u, 0xF9u,
                               0xC0u, 0xC1u, 0xC8u, 0xC9u, 0xD2u, 0xD9u }) {
            const uint32_t gid = cmapLookup(p, static_cast<size_t>(ttf.size()), code);
            if (gid == 0) continue;
            const auto span = spanOf(ttf, gid);
            if (span.second <= span.first) continue; // outline-less mapping
            QSet<uint32_t> probe{ gid };
            if (!expandCompositeClosure(p, static_cast<size_t>(ttf.size()), probe))
                continue;
            uint32_t comp = 0xFFFFFFFFu;
            for (uint32_t g : probe)
                if (g != gid && g != 0) { comp = g; break; }
            if (comp == 0xFFFFFFFFu) continue; // not composite in this font
            chosenByte = static_cast<unsigned char>(code);
            chosenGid = gid;
            component = comp;
            break;
        }
        if (chosenByte == 0)
            QSKIP("no composite accented glyph with an outline in this font — "
                  "closure not exercisable");

        const SimpleFixture fx =
            buildSimpleFixture(m_tmpDir, QStringLiteral("composite.pdf"), ttf,
                               /*drawComposite=*/true, chosenByte);
        if (!fx.ok)
            QSKIP("host font lacks the fixture glyphs in its cmap");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(fx.path));
        const QString outPath = tmpPath(QStringLiteral("composite_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));

        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff2 = findFontFileStreams(after, "FontFile2").first();
        const QByteArray program = decodedStream(ff2);
        const auto spanDrawn = spanOf(program, chosenGid);
        QVERIFY2(spanDrawn.second > spanDrawn.first,
                 "the drawn glyph must survive");
        const auto spanComponent = spanOf(program, component);
        QVERIFY2(spanComponent.second > spanComponent.first,
                 "the composite's component glyph must survive via closure "
                 "even though it was never drawn");
    }

    // ── 4. scope disclosure ──────────────────────────────────────────────────

    void unparseableCffProgramIsLeftUntouched() {
        // CFF lane shipped: an ELIGIBLE /FontFile3 program is now subsetted
        // (round-trip pins below). This pin keeps the degradation contract
        // for a FontFile3 whose bytes are not a parseable CFF: not one byte
        // of it may be touched, and the estimator must claim nothing.
        PoDoFo::PdfMemDocument doc;
        doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        auto& page = doc.GetPages().GetPageAt(0);
        QByteArray cffBytes(2048, '\x7f');
        auto& ff3 = doc.GetObjects().CreateDictionaryObject();
        ff3.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("CIDFontType0C"));
        {
            PoDoFo::charbuff buf(std::string_view(cffBytes.constData(),
                                                  static_cast<size_t>(cffBytes.size())));
            ff3.GetOrCreateStream().SetData(buf,
                PoDoFo::PdfFilterList{ PoDoFo::PdfFilterType::FlateDecode },
                /*raw=*/false);
        }
        auto& fd = makeFontDescriptor(doc, ff3, "FontFile3");
        auto& cidFont = doc.GetObjects().CreateDictionaryObject();
        cidFont.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
        cidFont.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("CIDFontType0"));
        cidFont.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("FixtureCFF"));
        cidFont.GetDictionary().AddKey("FontDescriptor", fd.GetIndirectReference());
        auto& type0 = doc.GetObjects().CreateDictionaryObject();
        type0.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
        type0.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Type0"));
        type0.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("FixtureCFF"));
        type0.GetDictionary().AddKey("Encoding", PoDoFo::PdfName("Identity-H"));
        PoDoFo::PdfArray descendants;
        descendants.Add(PoDoFo::PdfObject(cidFont.GetIndirectReference()));
        type0.GetDictionary().AddKey("DescendantFonts", PoDoFo::PdfVariant(descendants));
        addContentStream(doc, page, "BT /F1 12 Tf 72 700 Td <00440045> Tj ET\n",
                         &type0, "F1");
        const QString inPath = tmpPath(QStringLiteral("cff.pdf"));
        doc.Save(inPath.toUtf8().constData());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(inPath));
        OptimizeOptions estOpts = subsetOnlyOptions();
        estOpts.subsetFonts = true;
        const OptimizeEstimate est = engine.estimateOptimization(estOpts);
        QCOMPARE(est.estimatedBytes, est.originalBytes);

        const QString outPath = tmpPath(QStringLiteral("cff_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));
        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        const auto streams = findFontFileStreams(after, "FontFile3");
        QCOMPARE(streams.size(), 1);
        QCOMPARE(decodedStream(streams.first()).size(), cffBytes.size());
    }

    // ── 1b. CFF surgery (hermetic) ────────────────────────────────────────────

    void cffLayoutParsesSyntheticFonts() {
        const QByteArray cid = makeCidCff();
        CffLayout lay;
        QVERIFY2(parseCffLayout(reinterpret_cast<const unsigned char*>(cid.constData()),
                                static_cast<size_t>(cid.size()), lay),
                 "the synthetic CID-keyed CFF must parse");
        QVERIFY(lay.cidKeyed);
        QCOMPARE(lay.numGlyphs, 6u);
        QVERIFY(lay.hasFdArray);
        QVERIFY(lay.hasFdSelect);
        QCOMPARE(lay.header.length, 4u);
        QCOMPARE(lay.charset.length, 11u); // format 0: 1 + 5 SIDs
        QVERIFY(lay.charStrings.length > 0);
        QCOMPARE(static_cast<int>(lay.privates.size()), 1);
        QVERIFY(lay.privates[0].hasSubrs);
        QVERIFY(lay.privates[0].subrs.length > 0);

        const QByteArray name = makeNameCff();
        CffLayout lay2;
        QVERIFY2(parseCffLayout(reinterpret_cast<const unsigned char*>(name.constData()),
                                static_cast<size_t>(name.size()), lay2),
                 "the synthetic name-keyed CFF must parse");
        QVERIFY2(!lay2.cidKeyed, "no ROS operator → not CID-keyed");
        QCOMPARE(lay2.numGlyphs, 4u);
        QCOMPARE(static_cast<int>(lay2.privates.size()), 1);
        QVERIFY2(!lay2.privates[0].hasSubrs, "the name-keyed fixture has no Subrs");

        // A Top-DICT FontMatrix operator (12 7) is a sub-FontMatrix transform:
        // the parse must refuse the program (the caller skips + discloses).
        const QByteArray fmCff = makeNameCff(false, false, /*topFontMatrix=*/true);
        CffLayout lay3;
        QVERIFY2(!parseCffLayout(
                     reinterpret_cast<const unsigned char*>(fmCff.constData()),
                     static_cast<size_t>(fmCff.size()), lay3),
                 "a Top-DICT FontMatrix operator must be refused");
    }

    void cffBlankingPreservesNumberingAndKeptBytes() {
        const QByteArray cff = makeCidCff();
        const unsigned char* ip =
            reinterpret_cast<const unsigned char*>(cff.constData());
        CffLayout before;
        QVERIFY(parseCffLayout(ip, static_cast<size_t>(cff.size()), before));

        // Used CIDs 7/9/11 → GIDs 1/3/5; plus .notdef (0). g2+g4 blankable:
        // (16-1) + (16-1) = 30 raw bytes.
        QSet<uint32_t> keep{ 0, 1, 3, 5 };
        bool ok = false;
        const qint64 blankable = blankableCffCharstringBytes(
            ip, static_cast<size_t>(cff.size()), keep, &ok);
        QVERIFY(ok);
        QCOMPARE(blankable, static_cast<qint64>(30));

        QByteArray out;
        QVERIFY2(blankUnusedCffCharstrings(ip, static_cast<size_t>(cff.size()),
                                           keep, out),
                 "the rewriter must accept its own synthetic fixture");
        QVERIFY(out.size() < cff.size());
        const unsigned char* op =
            reinterpret_cast<const unsigned char*>(out.constData());
        CffLayout after;
        QVERIFY(parseCffLayout(op, static_cast<size_t>(out.size()), after));
        QCOMPARE(after.numGlyphs, 6u);

        // CharStrings INDEX: same entry count; kept entries byte-verbatim;
        // blanked entries collapse to a single endchar (0x0E).
        const TestIndex inIx = readCffIndexAt(cff, before.charStrings.offset);
        const TestIndex outIx = readCffIndexAt(out, after.charStrings.offset);
        QCOMPARE(outIx.count, inIx.count);
        for (uint32_t gid = 0; gid < 6; ++gid) {
            const auto inSpan = inIx.spans[gid];
            const auto outSpan = outIx.spans[gid];
            const int inLen = static_cast<int>(inSpan.second - inSpan.first);
            const int outLen = static_cast<int>(outSpan.second - outSpan.first);
            if (keep.contains(gid)) {
                QCOMPARE(outLen, inLen);
                QVERIFY(std::memcmp(ip + inSpan.first, op + outSpan.first, inLen) == 0);
            } else {
                QCOMPARE(outLen, 1);
                QCOMPARE(static_cast<int>(op[outSpan.first]), 0x0E);
            }
        }
        // charset, FDSelect, Name/String/gsubr INDEXes, Private DICT and its
        // Subrs all stay byte-identical (FDArray is rebuilt: Private offsets
        // move — that is its only content change and it is checked via the
        // render-diff round-trip below).
        for (const CffSection section : { before.charset, before.fdSelect,
                                          before.nameIndex, before.stringIndex,
                                          before.gsubrIndex }) {
            QVERIFY(section.length > 0);
        }
        QVERIFY(std::memcmp(ip + before.charset.offset, op + after.charset.offset,
                            before.charset.length) == 0);
        QVERIFY(std::memcmp(ip + before.fdSelect.offset, op + after.fdSelect.offset,
                            before.fdSelect.length) == 0);
        QVERIFY(std::memcmp(ip + before.nameIndex.offset, op + after.nameIndex.offset,
                            before.nameIndex.length) == 0);
        QVERIFY(std::memcmp(ip + before.stringIndex.offset,
                            op + after.stringIndex.offset,
                            before.stringIndex.length) == 0);
        QVERIFY(std::memcmp(ip + before.gsubrIndex.offset, op + after.gsubrIndex.offset,
                            before.gsubrIndex.length) == 0);
        QCOMPARE(after.privates.size(), before.privates.size());
        for (size_t i = 0; i < before.privates.size(); ++i) {
            QVERIFY(std::memcmp(ip + before.privates[i].dict.offset,
                                op + after.privates[i].dict.offset,
                                before.privates[i].dict.length) == 0);
            QVERIFY(before.privates[i].hasSubrs);
            QVERIFY(std::memcmp(ip + before.privates[i].subrs.offset,
                                op + after.privates[i].subrs.offset,
                                before.privates[i].subrs.length) == 0);
        }
    }

    // ── 2b. CFF document round-trips + render-diff ───────────────────────────

    void roundTripCidFontType0CPreservesUsedAndBlanksUnused() {
        const QByteArray cff = makeCidCff();
        const CffCidFixture fx = buildCidCffFixture(m_tmpDir,
                                                    QStringLiteral("cffcid.pdf"), cff);
        QVERIFY(!fx.path.isEmpty());

        // Estimator honesty, both ways.
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(fx.path));
        OptimizeOptions estOpts = subsetOnlyOptions();
        estOpts.subsetFonts = false;
        const OptimizeEstimate off = engine.estimateOptimization(estOpts);
        estOpts.subsetFonts = true;
        const OptimizeEstimate on = engine.estimateOptimization(estOpts);
        QCOMPARE(off.estimatedBytes, off.originalBytes);
        QVERIFY2(on.estimatedBytes < on.originalBytes,
                 "an eligible CIDFontType0C program must yield a real estimate");
        {
            PoDoFo::PdfMemDocument probe;
            probe.Load(fx.path.toUtf8().constData());
            qint64 encoded = 0;
            for (auto* ff : findFontFileStreams(probe, "FontFile3"))
                encoded += encodedStreamSize(ff);
            QVERIFY2(encoded > 0, "the fixture must carry an embedded FontFile3");
            QVERIFY2(on.originalBytes - on.estimatedBytes <= encoded,
                     "the subset claim must never exceed the program's encoded size");
        }

        // Before state.
        PdfiumBackend rendererBefore;
        QVERIFY(rendererBefore.loadDocument(fx.path));
        const QImage pageBefore = rendererBefore.renderPage(0, 150);
        QVERIFY(!pageBefore.isNull());
        const QString textBefore = rendererBefore.extractText(0);
        QVERIFY2(!textBefore.trimmed().isEmpty(),
                 "the fixture must render extractable text (ToUnicode)");
        PoDoFo::PdfMemDocument before;
        before.Load(fx.path.toUtf8().constData());
        auto* ff3Before = findFontFileStreams(before, "FontFile3").first();
        const QByteArray programBefore = decodedStream(ff3Before);
        QCOMPARE(programBefore, cff);
        auto contentsBefore = before.GetPages().GetPageAt(0).GetContents();
        QVERIFY(contentsBefore);
        PoDoFo::charbuff cb;
        contentsBefore->CopyTo(cb);
        const QByteArray contentBefore(cb.data(), static_cast<qsizetype>(cb.size()));
        PoDoFo::PdfObject* font0Before = nullptr;
        for (auto obj : before.GetObjects()) {
            if (!obj->IsDictionary()) continue;
            auto* st = obj->GetDictionary().FindKey("Subtype");
            if (st && st->IsName() && st->GetName().GetString() == "Type0")
                font0Before = obj;
        }
        QVERIFY(font0Before);
        const QByteArray toUniBeforeBytes = decodedStream(resolveObject(
            before, font0Before->GetDictionary().FindKey("ToUnicode")));
        // /W lives on the descendant CIDFont dict, not on the Type0 wrapper.
        const auto descendantOf = [](PoDoFo::PdfMemDocument& doc,
                                     PoDoFo::PdfObject* type0) -> PoDoFo::PdfObject* {
            auto* arr = resolveObject(doc,
                type0->GetDictionary().FindKey("DescendantFonts"));
            if (!arr || !arr->IsArray() || arr->GetArray().IsEmpty()) return nullptr;
            return resolveObject(doc, &arr->GetArray()[0]);
        };
        auto* cidFontBefore = descendantOf(before, font0Before);
        QVERIFY(cidFontBefore);
        const QByteArray wBefore = QString::fromUtf8(
            cidFontBefore->GetDictionary().FindKey("W")->ToString().c_str()).toUtf8();

        // Run the pass.
        const QString outPath = tmpPath(QStringLiteral("cffcid_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));
        QVERIFY(QFileInfo(outPath).size() < QFileInfo(fx.path).size());

        // After state: program shrunk; content/ToUnicode/W byte-identical.
        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff3After = findFontFileStreams(after, "FontFile3").first();
        const QByteArray programAfter = decodedStream(ff3After);
        QVERIFY2(programAfter.size() < programBefore.size(),
                 "the decoded CFF program must shrink (unused charstrings removed)");
        QVERIFY2(encodedStreamSize(ff3After) < encodedStreamSize(ff3Before),
                 "the encoded FontFile3 stream must shrink");
        auto contentsAfter = after.GetPages().GetPageAt(0).GetContents();
        QVERIFY(contentsAfter);
        PoDoFo::charbuff ca;
        contentsAfter->CopyTo(ca);
        QCOMPARE(QByteArray(ca.data(), static_cast<qsizetype>(ca.size())), contentBefore);
        PoDoFo::PdfObject* font0After = nullptr;
        for (auto obj : after.GetObjects()) {
            if (!obj->IsDictionary()) continue;
            auto* st = obj->GetDictionary().FindKey("Subtype");
            if (st && st->IsName() && st->GetName().GetString() == "Type0")
                font0After = obj;
        }
        QVERIFY(font0After);
        QCOMPARE(decodedStream(resolveObject(
                     after, font0After->GetDictionary().FindKey("ToUnicode"))),
                 toUniBeforeBytes);
        auto* cidFontAfter = descendantOf(after, font0After);
        QVERIFY(cidFontAfter);
        QCOMPARE(QString::fromUtf8(
                     cidFontAfter->GetDictionary().FindKey("W")->ToString().c_str()),
                 QString::fromUtf8(wBefore.constData()));

        // Charstrings: used CIDs' glyphs intact, unused blanked to endchar.
        CffLayout layBefore;
        CffLayout layAfter;
        QVERIFY(parseCffLayout(reinterpret_cast<const unsigned char*>(cff.constData()),
                               static_cast<size_t>(cff.size()), layBefore));
        QVERIFY(parseCffLayout(
            reinterpret_cast<const unsigned char*>(programAfter.constData()),
            static_cast<size_t>(programAfter.size()), layAfter));
        const TestIndex inIx = readCffIndexAt(cff, layBefore.charStrings.offset);
        const TestIndex outIx = readCffIndexAt(programAfter,
                                               layAfter.charStrings.offset);
        QCOMPARE(outIx.count, inIx.count);
        QSet<uint32_t> keep{ 0, 1, 3, 5 };
        for (uint32_t gid = 0; gid < 6; ++gid) {
            const auto inSpan = inIx.spans[gid];
            const auto outSpan = outIx.spans[gid];
            if (keep.contains(gid)) {
                QCOMPARE(static_cast<int>(outSpan.second - outSpan.first),
                         static_cast<int>(inSpan.second - inSpan.first));
                QVERIFY(std::memcmp(cff.constData() + inSpan.first,
                                    programAfter.constData() + outSpan.first,
                                    inSpan.second - inSpan.first) == 0);
            } else {
                QCOMPARE(static_cast<int>(outSpan.second - outSpan.first), 1);
                QCOMPARE(static_cast<int>(
                             programAfter.constData()[outSpan.first]), 0x0E);
            }
        }

        // Render-diff + extraction identity.
        PdfiumBackend rendererAfter;
        QVERIFY(rendererAfter.loadDocument(outPath));
        const QImage pageAfter = rendererAfter.renderPage(0, 150);
        QVERIFY(!pageAfter.isNull());
        const double diff = imageDiffFraction(pageBefore, pageAfter, 16);
        QVERIFY2(diff < 0.001,
                 qPrintable(QStringLiteral(
                     "the page must render identically after CFF subsetting "
                     "(differing pixel fraction %1)").arg(diff)));
        QCOMPARE(rendererAfter.extractText(0), textBefore);
    }

    void simpleCffBuiltinEncodingRoundTrip() {
        // Name-keyed CFF, NO PDF /Encoding → the CFF's built-in encoding is
        // what a conforming reader consults; the fixture's charset carries
        // standard-string SIDs (A/B/C), so codes 'A'/'C' resolve provably.
        const QByteArray cff = makeNameCff();
        const QString inPath =
            buildSimpleCffFixture(m_tmpDir, QStringLiteral("cffsimple.pdf"), cff,
                                  /*namedEncoding=*/false);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(inPath));
        OptimizeOptions estOpts = subsetOnlyOptions();
        estOpts.subsetFonts = false;
        QCOMPARE(engine.estimateOptimization(estOpts).estimatedBytes,
                 engine.estimateOptimization(estOpts).originalBytes);
        estOpts.subsetFonts = true;
        QVERIFY2(engine.estimateOptimization(estOpts).estimatedBytes
                     < engine.estimateOptimization(estOpts).originalBytes,
                 "an eligible Type1C program must yield a real estimate");

        PdfiumBackend rendererBefore;
        QVERIFY(rendererBefore.loadDocument(inPath));
        const QImage pageBefore = rendererBefore.renderPage(0, 150);
        QVERIFY(!pageBefore.isNull());
        const QString textBefore = rendererBefore.extractText(0);

        const QString outPath = tmpPath(QStringLiteral("cffsimple_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));

        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff3 = findFontFileStreams(after, "FontFile3").first();
        const QByteArray program = decodedStream(ff3);
        QVERIFY2(program.size() < static_cast<qint64>(cff.size()),
                 "the decoded CFF program must shrink");

        CffLayout layBefore;
        CffLayout layAfter;
        QVERIFY(parseCffLayout(reinterpret_cast<const unsigned char*>(cff.constData()),
                               static_cast<size_t>(cff.size()), layBefore));
        QVERIFY(parseCffLayout(
            reinterpret_cast<const unsigned char*>(program.constData()),
            static_cast<size_t>(program.size()), layAfter));
        const TestIndex inIx = readCffIndexAt(cff, layBefore.charStrings.offset);
        const TestIndex outIx = readCffIndexAt(program, layAfter.charStrings.offset);
        QCOMPARE(outIx.count, inIx.count);
        // Used 'A'→g1, 'C'→g3 stay; unused g2 blanked.
        QVERIFY(outIx.spans[1].second > outIx.spans[1].first);
        QVERIFY(std::memcmp(cff.constData() + inIx.spans[1].first,
                            program.constData() + outIx.spans[1].first,
                            inIx.spans[1].second - inIx.spans[1].first) == 0);
        QVERIFY(outIx.spans[3].second > outIx.spans[3].first);
        QCOMPARE(static_cast<int>(outIx.spans[2].second - outIx.spans[2].first), 1);
        QCOMPARE(static_cast<int>(program.constData()[outIx.spans[2].first]), 0x0E);

        PdfiumBackend rendererAfter;
        QVERIFY(rendererAfter.loadDocument(outPath));
        QCOMPARE(rendererAfter.extractText(0), textBefore);
        const double diff =
            imageDiffFraction(pageBefore, rendererAfter.renderPage(0, 150), 16);
        QVERIFY2(diff < 0.001,
                 qPrintable(QStringLiteral("render-diff fraction %1").arg(diff)));
    }

    void openTypeWrapperCffRoundTrip() {
        // OTTO-wrapped CFF under a simple /OpenType font: the cmap path
        // proves GIDs; the pass must rewrite ONLY the 'CFF ' table and keep
        // every other sfnt table byte-identical. bulkUnused pads the unused
        // glyph (+180 raw bytes) so the Flate-encoded sfnt reliably shrinks —
        // the bare 15-byte delta of the default fixture is inside deflate
        // noise on a wrapper this small.
        const QByteArray cff = makeNameCff(/*seacGlyph=*/false, /*bulkUnused=*/true);
        const QByteArray otto = makeOttWrapper(cff);
        const QString inPath =
            buildOpenTypeCffFixture(m_tmpDir, QStringLiteral("cffotf.pdf"), otto);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(inPath));
        OptimizeOptions estOpts = subsetOnlyOptions();
        estOpts.subsetFonts = true;
        QVERIFY2(engine.estimateOptimization(estOpts).estimatedBytes
                     < engine.estimateOptimization(estOpts).originalBytes,
                 "an eligible OpenType/CFF program must yield a real estimate");

        PdfiumBackend rendererBefore;
        QVERIFY(rendererBefore.loadDocument(inPath));
        const QImage pageBefore = rendererBefore.renderPage(0, 150);
        QVERIFY(!pageBefore.isNull());
        const QString textBefore = rendererBefore.extractText(0);

        const QString outPath = tmpPath(QStringLiteral("cffotf_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));

        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff3 = findFontFileStreams(after, "FontFile3").first();
        const QByteArray program = decodedStream(ff3);
        QVERIFY2(program.size() < static_cast<qint64>(otto.size()),
                 "the wrapped program must shrink");

        // Every non-'CFF ' table stays byte-identical — EXCEPT head's
        // checkSumAdjustment, which the sfnt rebuild recomputes over the new
        // layout (ISO 14496-6, the same discipline blankUnusedGlyphs pins for
        // the TrueType lane). For head we assert the stronger property: every
        // other byte identical AND the adjustment equals 0xB1B0AFBA minus the
        // whole-font checksum of the OUTPUT bytes.
        const uint32_t kCffTag = 0x43464620u;  // 'CFF '
        const uint32_t kHeadTag = 0x68656164u; // 'head'
        auto inTables = readSfntTables(otto);
        auto outTables = readSfntTables(program);
        QCOMPARE(inTables.size(), outTables.size());
        const auto sfntChecksum = [](const QByteArray& b) {
            uint32_t sum = 0;
            for (int i = 0; i < b.size(); i += 4) {
                uint32_t word = 0;
                for (int k = 0; k < 4; ++k) {
                    const int idx = i + k;
                    const uint32_t octet = idx < b.size()
                        ? static_cast<unsigned char>(b.constData()[idx]) : 0;
                    word = (word << 8) | octet;
                }
                sum += word; // wraparound 32-bit accumulate
            }
            return sum;
        };
        for (const auto& [tag, span] : inTables) {
            QVERIFY(outTables.count(tag) == 1);
            const auto outSpan = outTables[tag];
            if (tag == kCffTag) continue; // rewritten by design
            QCOMPARE(static_cast<int>(outSpan.second - outSpan.first),
                     static_cast<int>(span.second - span.first));
            if (tag != kHeadTag) {
                QVERIFY(std::memcmp(otto.constData() + span.first,
                                    program.constData() + outSpan.first,
                                    span.second - span.first) == 0);
                continue;
            }
            const int headLen = static_cast<int>(span.second - span.first);
            for (int i = 0; i < headLen; ++i) {
                if (i >= 8 && i < 12) continue; // checkSumAdjustment slot
                QCOMPARE(static_cast<int>(program.constData()[outSpan.first + i]),
                         static_cast<int>(otto.constData()[span.first + i]));
            }
            // The ISO 14496-6 invariant: a correctly adjusted whole font
            // checksums to 0xB1B0AFBA (adjustment = 0xB1B0AFBA - checksum
            // with the adjustment zeroed; checksumming the final font must
            // therefore yield exactly 0xB1B0AFBA).
            QCOMPARE(sfntChecksum(program), 0xB1B0AFBAu);
        }
        // Inside the rewritten 'CFF ': used glyphs intact, unused blanked.
        CffLayout layBefore;
        CffLayout layAfter;
        QVERIFY(parseCffLayout(reinterpret_cast<const unsigned char*>(cff.constData()),
                               static_cast<size_t>(cff.size()), layBefore));
        const auto cffSpan = outTables[kCffTag];
        QVERIFY(parseCffLayout(
            reinterpret_cast<const unsigned char*>(program.constData()) + cffSpan.first,
            cffSpan.second - cffSpan.first, layAfter));
        const TestIndex outIx = readCffIndexAt(program,
            cffSpan.first + layAfter.charStrings.offset);
        QCOMPARE(outIx.count, 4u);
        QCOMPARE(static_cast<int>(outIx.spans[2].second - outIx.spans[2].first), 1);
        QCOMPARE(static_cast<int>(
                     program.constData()[outIx.spans[2].first]), 0x0E);
        QVERIFY(outIx.spans[1].second > outIx.spans[1].first);
        QVERIFY(outIx.spans[3].second > outIx.spans[3].first);

        PdfiumBackend rendererAfter;
        QVERIFY(rendererAfter.loadDocument(outPath));
        QCOMPARE(rendererAfter.extractText(0), textBefore);
        const double diff =
            imageDiffFraction(pageBefore, rendererAfter.renderPage(0, 150), 16);
        QVERIFY2(diff < 0.001,
                 qPrintable(QStringLiteral("render-diff fraction %1").arg(diff)));
    }

    // ── 4b. CFF scope disclosure ─────────────────────────────────────────────

    void nonCidKeyedCffUnderType0IsUntouched() {
        // A name-keyed CFF cannot prove CID→GID (no charset CID mapping):
        // under Type0/CIDFontType0 + Identity-H the usage is unprovable, so
        // the font is disclosed as skipped — never a guessed keep-set.
        const QByteArray cff = makeNameCff();
        const CffCidFixture fx =
            buildCidCffFixture(m_tmpDir, QStringLiteral("cffname_cid.pdf"), cff);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(fx.path));
        OptimizeOptions estOpts = subsetOnlyOptions();
        estOpts.subsetFonts = true;
        const OptimizeEstimate est = engine.estimateOptimization(estOpts);
        QCOMPARE(est.estimatedBytes, est.originalBytes);

        const QString outPath = tmpPath(QStringLiteral("cffname_cid_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));
        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff3 = findFontFileStreams(after, "FontFile3").first();
        QCOMPARE(decodedStream(ff3), cff); // untouched
    }

    void simpleCffNamedEncodingIsUntouched() {
        // Named /Encoding shifts the code→glyph path out of provability
        // (same rule as simple TrueType): untouched + zero claim.
        const QByteArray cff = makeNameCff();
        const QString inPath =
            buildSimpleCffFixture(m_tmpDir, QStringLiteral("cffambig.pdf"), cff,
                                  /*namedEncoding=*/true);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(inPath));
        OptimizeOptions estOpts = subsetOnlyOptions();
        estOpts.subsetFonts = true;
        const OptimizeEstimate est = engine.estimateOptimization(estOpts);
        QCOMPARE(est.estimatedBytes, est.originalBytes);

        const QString outPath = tmpPath(QStringLiteral("cffambig_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));
        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff3 = findFontFileStreams(after, "FontFile3").first();
        QCOMPARE(decodedStream(ff3), cff); // untouched
    }

    void cffSeacCharstringSkipsFont() {
        // A kept charstring in the seac form (4-5 numbers + endchar)
        // references other glyphs by standard-encoding code — the referenced
        // glyphs cannot be proven into the keep set without the full Adobe
        // Standard Encoding machinery, so the font is skipped + disclosed.
        CidCffSpec spec;
        spec.seacGlyph = true;
        const QByteArray cff = makeCidCff(spec);
        const CffCidFixture fx =
            buildCidCffFixture(m_tmpDir, QStringLiteral("cffseac.pdf"), cff);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(fx.path));
        OptimizeOptions estOpts = subsetOnlyOptions();
        estOpts.subsetFonts = true;
        const OptimizeEstimate est = engine.estimateOptimization(estOpts);
        QCOMPARE(est.estimatedBytes, est.originalBytes);

        const QString outPath = tmpPath(QStringLiteral("cffseac_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));
        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff3 = findFontFileStreams(after, "FontFile3").first();
        QCOMPARE(decodedStream(ff3), cff); // untouched
    }

    void cffSubFontMatrixIsUntouched() {
        // A non-default /FontMatrix on the font dict is a sub-FontMatrix
        // transform: the lane refuses the rewrite and claims nothing —
        // counted skip, never a guessed keep-set.
        const QByteArray cff = makeNameCff();
        const QString inPath =
            buildSimpleCffFixture(m_tmpDir, QStringLiteral("cffmatrix.pdf"), cff,
                                  /*namedEncoding=*/false, /*subFontMatrix=*/true);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(inPath));
        OptimizeOptions estOpts = subsetOnlyOptions();
        estOpts.subsetFonts = true;
        const OptimizeEstimate est = engine.estimateOptimization(estOpts);
        QCOMPARE(est.estimatedBytes, est.originalBytes);

        const QString outPath = tmpPath(QStringLiteral("cffmatrix_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));
        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff3 = findFontFileStreams(after, "FontFile3").first();
        QCOMPARE(decodedStream(ff3), cff); // untouched
    }

    void corruptCffProgramSkipsSafely() {
        QByteArray cff = makeCidCff();
        cff.truncate(40); // header-shaped but truncated
        const CffCidFixture fx =
            buildCidCffFixture(m_tmpDir, QStringLiteral("cffcorrupt.pdf"), cff);

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(fx.path));
        const QString outPath = tmpPath(QStringLiteral("cffcorrupt_out.pdf"));
        QVERIFY2(engine.optimizeDocument(outPath, subsetOnlyOptions()),
                 "one hostile font must degrade to a skip, never abort the run");
        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        const auto streams = findFontFileStreams(after, "FontFile3");
        QCOMPARE(streams.size(), 1);
        QCOMPARE(decodedStream(streams.first()), cff); // left exactly as it was
    }

    void type1ProgramIsLeftUntouched() {
        PoDoFo::PdfMemDocument doc;
        doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        auto& page = doc.GetPages().GetPageAt(0);
        QByteArray type1Bytes(1024, '\x2f');
        auto& ff = doc.GetObjects().CreateDictionaryObject();
        {
            PoDoFo::charbuff buf(std::string_view(type1Bytes.constData(),
                                                  static_cast<size_t>(type1Bytes.size())));
            ff.GetOrCreateStream().SetData(buf, PoDoFo::PdfFilterList{}, /*raw=*/true);
        }
        auto& fd = makeFontDescriptor(doc, ff, "FontFile");
        auto& font = doc.GetObjects().CreateDictionaryObject();
        font.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
        font.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("Type1"));
        font.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("FixtureType1"));
        font.GetDictionary().AddKey("FontDescriptor", fd.GetIndirectReference());
        addContentStream(doc, page, "BT /F1 12 Tf 72 700 Td (Hi) Tj ET\n", &font, "F1");
        const QString inPath = tmpPath(QStringLiteral("type1.pdf"));
        doc.Save(inPath.toUtf8().constData());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(inPath));
        const QString outPath = tmpPath(QStringLiteral("type1_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));
        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        const auto streams = findFontFileStreams(after, "FontFile");
        QCOMPARE(streams.size(), 1);
        QCOMPARE(decodedStream(streams.first()), type1Bytes);
    }

    void corruptFontProgramSkipsSafely() {
        const QString ttfPath = pickHostTtf();
        if (ttfPath.isEmpty())
            QSKIP("no host TrueType font available");
        QByteArray ttf = hostTtfBytes(ttfPath);
        ttf.truncate(64); // a header-shaped but truncated program
        const CidFixture fx =
            buildCidFixture(m_tmpDir, QStringLiteral("corrupt.pdf"), ttf,
                            /*streamCidMap=*/false, /*requireGlyphs=*/false);
        Q_ASSERT(!fx.path.isEmpty());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(fx.path));
        const QString outPath = tmpPath(QStringLiteral("corrupt_out.pdf"));
        QVERIFY2(engine.optimizeDocument(outPath, subsetOnlyOptions()),
                 "one hostile font must degrade to a skip, never abort the run");
        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        const auto streams = findFontFileStreams(after, "FontFile2");
        QCOMPARE(streams.size(), 1);
        // The corrupt program is left exactly as it was.
        QCOMPARE(decodedStream(streams.first()), ttf);
    }

    void signedDocumentClaimsNoSubsetSavings() {
        // The signing fixtures carry the same requirement as
        // TestOptimizeSignedGuard; the estimator must claim nothing for a
        // signed document even when an eligible FontFile2 is present (the
        // incremental-update write path cannot shrink).
#ifdef SOURCE_DIR
        static const QString kFixtureDir = QStringLiteral(SOURCE_DIR "/tests/fixtures/signing");
#else
        static const QString kFixtureDir = QStringLiteral("tests/fixtures/signing");
#endif
        const QString kP12 = kFixtureDir + "/test_signer.p12";
        if (!QFileInfo::exists(kP12))
            QSKIP("Signing fixtures missing — run tests/fixtures/signing/generate.bat");
        const QString ttfPath = pickHostTtf();
        if (ttfPath.isEmpty())
            QSKIP("no host TrueType font available");
        const QByteArray ttf = hostTtfBytes(ttfPath);
        const CidFixture eligible = buildCidFixture(m_tmpDir,
            QStringLiteral("elig_for_signing.pdf"), ttf);
        if (eligible.path.isEmpty())
            QSKIP("host font lacks the fixture glyphs in its cmap");

        SignatureManager mgr;
        const QString signedPdf = tmpPath(QStringLiteral("signed_elig.pdf"));
        QVERIFY2(mgr.signDocument(eligible.path, signedPdf, kP12,
                                  QStringLiteral("test"),
                                  QStringLiteral("FontSubsetTest"), "")
                     == SignOutcome::Success,
                 "signDocument should succeed with the test P12");

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(signedPdf));
        OptimizeOptions estOpts = subsetOnlyOptions();
        estOpts.subsetFonts = true;
        const OptimizeEstimate est = engine.estimateOptimization(estOpts);
        QCOMPARE(est.estimatedBytes, est.originalBytes);
    }

    // ── 5. estimator honesty: unsubsettable simple fonts claim nothing ───────

    void ambiguousEncodingSimpleFontClaimsNothing() {
        const QString ttfPath = pickHostTtf();
        if (ttfPath.isEmpty())
            QSKIP("no host TrueType font available");
        const QByteArray ttf = hostTtfBytes(ttfPath);
        // Same fixture shape but WITH a named /Encoding — the code→GID path
        // becomes unprovable, so the font is disclosed as skipped and the
        // estimator must not claim its savings.
        PoDoFo::PdfMemDocument doc;
        doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        auto& page = doc.GetPages().GetPageAt(0);
        auto& ff2 = makeFontFile2Stream(doc, ttf);
        auto& fd = makeFontDescriptor(doc, ff2, "FontFile2");
        auto& font = doc.GetObjects().CreateDictionaryObject();
        font.GetDictionary().AddKey("Type", PoDoFo::PdfName("Font"));
        font.GetDictionary().AddKey("Subtype", PoDoFo::PdfName("TrueType"));
        font.GetDictionary().AddKey("BaseFont", PoDoFo::PdfName("Fixture"));
        font.GetDictionary().AddKey("Encoding", PoDoFo::PdfName("WinAnsiEncoding"));
        font.GetDictionary().AddKey("FontDescriptor", fd.GetIndirectReference());
        addContentStream(doc, page, "BT /F1 12 Tf 72 700 Td (A) Tj ET\n", &font, "F1");
        const QString inPath = tmpPath(QStringLiteral("ambig.pdf"));
        doc.Save(inPath.toUtf8().constData());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(inPath));
        OptimizeOptions estOpts = subsetOnlyOptions();
        estOpts.subsetFonts = true;
        const OptimizeEstimate est = engine.estimateOptimization(estOpts);
        QCOMPARE(est.estimatedBytes, est.originalBytes);

        const QString outPath = tmpPath(QStringLiteral("ambig_out.pdf"));
        QVERIFY(engine.optimizeDocument(outPath, subsetOnlyOptions()));
        PoDoFo::PdfMemDocument after;
        after.Load(outPath.toUtf8().constData());
        auto* ff2After = findFontFileStreams(after, "FontFile2").first();
        QCOMPARE(decodedStream(ff2After), ttf); // untouched
    }
};

QTEST_MAIN(TestFontSubset)
#include "TestFontSubset.moc"
