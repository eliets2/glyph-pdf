// SPDX-License-Identifier: Apache-2.0
#include "AccessibilityTagger.h"

#include "AccessibilityChecker.h"   // kA11yMaxImageFindings (the prompt cap)
#include "SafeSave.h"

#include <podofo/podofo.h>

#include <QFile>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// ── T2-4 accessibility P2: the auto-tagging engine ───────────────────────────
//
// Design: docs/research/accessibility-auto-tagging-plan-2026-09-21.md. The
// acceptance pins live in tests/TestAccessibilityTagger.cpp.
//
// Shape of this file:
//   * ONE analysis walk (PdfContentStreamReader, per content stream, form
//     XObjects recursed with a visited set) produces positioned, DECODED
//     text runs — the single source of truth. Clustering consumes it; the
//     rewrite REPLAYS the identical walk (same reader, same flags, same
//     event indices) and re-emits tokens with inserted /Tag <</MCID n>> BDC
//     … EMC pairs; the text-preservation invariant REPLAYS it on the
//     reloaded candidate and compares per-page (text, y-bucket) sequences.
//   * Decoding is ToUnicode-honest (§3.2): /ToUnicode CMaps, the predefined
//     encodings, glyph-name /Differences through the AGLFN table, or the
//     standard-14 text faces' builtin defaults. A font none of these cover
//     (Type0 without /ToUnicode, unknown builtins) makes its PAGE skipped
//     and disclosed — wrong text in a tree is the one output this engine
//     must never produce.
//   * Headings are document-relative size clustering with the 1.08 ratio
//     break, ranked against the modal body cluster (§3.3); the cluster
//     table is returned so the user can see and judge the classification.
//   * The whole thing runs inside the SafeSave transaction discipline: the
//     original is untouched unless the candidate passed BOTH the structural
//     walk and the invariant (§2.2), then commits atomically.
//
// Disclosed simplifications (P2 scope):
//   * Single-column reading order (y desc, then x); pages carrying a
//     column-suspect signature are tagged anyway but flagged (§3.4).
//   * A form XObject drawn on several pages is tagged once, at its first
//     invocation (its stream is shared; /Pg names the first page).
//   * No /ActualText is ever emitted (P3 candidate — wrong-text hazard).

using PoDoFo::PdfArray;
using PoDoFo::PdfCanvas;
using PoDoFo::PdfContent;
using PoDoFo::PdfContentReaderArgs;
using PoDoFo::PdfContentStreamReader;
using PoDoFo::PdfContentReaderFlags;
using PoDoFo::PdfContentType;
using PoDoFo::PdfDictionary;
using PoDoFo::PdfEncodingMap;
using PoDoFo::PdfEncodingMapFactory;
using PoDoFo::PdfObject;
using PoDoFo::PdfName;
using PoDoFo::PdfOperator;
using PoDoFo::PdfPage;
using PoDoFo::PdfString;
using PoDoFo::PdfXObjectForm;
using PoDoFo::PdfReference;
using PoDoFo::PdfVariant;
using PoDoFo::PdfVariantStack;

namespace gp {
namespace {

constexpr double kHeadingRatioBreak = 1.08;   // §3.3, pinned in tests
constexpr double kParagraphGapFactor = 1.55;  // × modal baseline gap
constexpr double kBoldStemV = 12.0;           // weight hint
constexpr double kXLeftTolerance = 2.0;       // paragraph x-left family (pt)
constexpr double kColumnFamilyFrac = 0.25;    // ≥25% of runs per x family
constexpr double kColumnSpanFrac = 0.15;      // families span >15% page width
constexpr int kMaxFormDepth = 8;              // same cap as the P1 walks

// ── tiny affine matrix (PDF [a b c d e f]) ───────────────────────────────────

struct Mat {
    double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
};

// (m * n) applied to p == m applied to (n applied to p).
Mat operator*(const Mat& m, const Mat& n) {
    return Mat{m.a * n.a + m.b * n.c,
               m.a * n.b + m.b * n.d,
               m.c * n.a + m.d * n.c,
               m.c * n.b + m.d * n.d,
               m.a * n.e + m.b * n.f + m.e,
               m.c * n.e + m.d * n.f + m.f};
}

double numAt(const PdfVariant& v) {
    // GetReal is lenient: integral operands come back as doubles.
    return v.GetReal();
}

// The reader's variant stack is a PostScript stack: index 0 is the LAST
// pushed operand. A matrix is pushed a,b,c,d,e,f - so a sits at index 5.
Mat matFromStack(const PdfVariantStack& stk) {
    Mat m;
    m.a = numAt(stk[5]);
    m.b = numAt(stk[4]);
    m.c = numAt(stk[3]);
    m.d = numAt(stk[2]);
    m.e = numAt(stk[1]);
    m.f = numAt(stk[0]);
    return m;
}

Mat dictMatrix(const PdfDictionary& dict, const char* key) {
    const PdfObject* o = dict.FindKey(PdfName(key));
    if (o == nullptr || !o->IsArray() || o->GetArray().GetSize() < 6)
        return Mat{};
    const PdfArray& arr = o->GetArray();
    auto num = [&](size_t i) {
        return arr[i].IsNumberOrReal() ? arr[i].GetReal() : 0.0;
    };
    return Mat{num(0), num(1), num(2), num(3), num(4), num(5)};
}

// ── small dictionary helpers (the P1 walk idiom) ─────────────────────────────

const PdfObject* resolveObj(PoDoFo::PdfMemDocument& doc, const PdfObject* obj) {
    if (obj == nullptr) return nullptr;
    if (obj->IsReference()) {
        try {
            return &doc.GetObjects().MustGetObject(obj->GetReference());
        } catch (const PoDoFo::PdfError&) {
            return nullptr;
        }
    }
    return obj;
}

PdfObject* resolveObjMutable(PoDoFo::PdfMemDocument& doc, PdfObject* obj) {
    if (obj == nullptr) return nullptr;
    if (obj->IsReference()) {
        try {
            return &doc.GetObjects().MustGetObject(obj->GetReference());
        } catch (const PoDoFo::PdfError&) {
            return nullptr;
        }
    }
    return obj;
}

QString nameAt(const PdfDictionary& dict, const char* key) {
    const PdfObject* o = dict.FindKey(PdfName(key));
    if (o == nullptr || !o->IsName()) return {};
    return QString::fromLatin1(o->GetName().GetString().data(),
                               static_cast<qsizetype>(
                                   o->GetName().GetString().size()));
}

QString stringAt(const PdfDictionary& dict, const char* key) {
    const PdfObject* o = dict.FindKey(PdfName(key));
    if (o == nullptr || !o->IsString()) return {};
    return QString::fromUtf8(o->GetString().GetString().data(),
                             static_cast<qsizetype>(
                                 o->GetString().GetString().size()));
}

// PoDoFo 1.1.0's loader inlines references once the target object is
// loaded: a saved `5 0 R` value can come back as the object ITSELF (a
// dictionary carrying its original reference). Identity checks must go
// through this helper, never through IsReference alone.
bool refOf(const PdfObject* o, PdfReference& out) {
    if (o == nullptr) return false;
    if (o->IsReference()) {
        out = o->GetReference();
        return true;
    }
    if (o->IsDictionary()) {
        out = o->GetIndirectReference();
        return out.ObjectNumber() != 0;
    }
    return false;
}

QString qstringFromName(const PoDoFo::PdfName& name) {
    return QString::fromLatin1(name.GetString().data(),
                               static_cast<qsizetype>(
                                   name.GetString().size()));
}

// ── ToUnicode-honest decoding (§3.2) ─────────────────────────────────────────

// Strip an "ABCDEF+" subset prefix.
QString stripSubset(const QString& name) {
    const int plus = name.indexOf(QLatin1Char('+'));
    return plus >= 0 && plus < 7 ? name.mid(plus + 1) : name;
}

bool isStandard14Text(const QString& baseName) {
    // The text-bearing standard-14 faces: honestly decodable through their
    // builtin defaults. Symbol and ZapfDingbats use non-Unicode builtin
    // encodings — without /ToUnicode they are exactly the wrong-text
    // hazard, so they are NOT covered.
    static const char* faces[] = {
        "Helvetica", "Helvetica-Bold", "Helvetica-Oblique",
        "Helvetica-BoldOblique",
        "Times-Roman", "Times-Bold", "Times-Italic", "Times-BoldItalic",
        "Courier", "Courier-Bold", "Courier-Oblique", "Courier-BoldOblique",
    };
    const QString n = stripSubset(baseName);
    for (const char* f : faces)
        if (n == QLatin1String(f)) return true;
    return false;
}

const PdfEncodingMap* predefinedMap(const QString& name) {
    if (name == QLatin1String("WinAnsiEncoding"))
        return &PdfEncodingMapFactory::GetWinAnsiEncodingInstance();
    if (name == QLatin1String("MacRomanEncoding"))
        return &PdfEncodingMapFactory::GetMacRomanEncodingInstance();
    if (name == QLatin1String("StandardEncoding"))
        return &PdfEncodingMapFactory::GetStandardEncodingInstance();
    if (name == QLatin1String("MacExpertEncoding"))
        return &PdfEncodingMapFactory::GetMacExpertEncodingInstance();
    return nullptr;
}

QString spanToQString(const PoDoFo::CodePointSpan& span) {
    QString out;
    for (const char32_t cp : span.view())
        out.append(QString::fromUcs4(&cp, 1));
    return out;
}

// Parse a /Differences array into a code → unicode table (AGLFN names).
void parseDifferences(PoDoFo::PdfMemDocument& doc, const PdfObject* arrObj,
                      std::map<unsigned, QString>& out) {
    arrObj = resolveObj(doc, arrObj);
    if (arrObj == nullptr || !arrObj->IsArray()) return;
    unsigned code = 0;
    for (const PdfObject& el : arrObj->GetArray()) {
        if (el.IsNumber()) {
            code = static_cast<unsigned>(el.GetNumber());
        } else if (el.IsName()) {
            PoDoFo::CodePointSpan span;
            const std::string glyphName(el.GetName().GetString().data(),
                                        el.GetName().GetString().size());
            if (PoDoFo::PdfDifferenceEncoding::TryGetCodePointsFromCharName(
                    glyphName, span)) {
                out[code] = spanToQString(span);
            }
            ++code;
        }
    }
}

struct FontInfo {
    bool ok = false;          // honestly decodable
    bool bold = false;        // §3.3 weight hint
    QString baseName;
    bool isType0 = false;
    std::shared_ptr<const PdfEncodingMap> map;  // ToUnicode/predefined
    std::map<unsigned, QString> differences;    // code → unicode
    QString disclosure;                         // why not ok
};

struct FontCache {
    std::map<PdfReference, FontInfo> byRef;
    std::vector<std::unique_ptr<FontInfo>> unkeyed;
    FontInfo sentinel;   // the "font object missing" answer (ok=false)
};

const FontInfo& resolveFont(PoDoFo::PdfMemDocument& doc,
                            const PdfObject* fontObj, FontCache& cache) {
    fontObj = resolveObj(doc, fontObj);
    if (fontObj == nullptr || !fontObj->IsDictionary()) {
        if (cache.sentinel.disclosure.isEmpty())
            cache.sentinel.disclosure = QStringLiteral("font object missing");
        return cache.sentinel;
    }
    const PdfReference ref = fontObj->GetIndirectReference();
    if (ref.ObjectNumber() != 0) {
        const auto it = cache.byRef.find(ref);
        if (it != cache.byRef.end()) return it->second;
    }
    const PdfDictionary& dict = fontObj->GetDictionary();
    FontInfo info;
    // /BaseFont is a NAME (a string value would be nonstandard).
    info.baseName = nameAt(dict, "BaseFont");
    if (info.baseName.isEmpty())
        info.baseName = stringAt(dict, "BaseFont");
    info.isType0 = nameAt(dict, "Subtype") == QLatin1String("Type0");

    // Weight hints (§3.3): the BaseFont name, /FontDescriptor /FontWeight,
    // /FontDescriptor /StemV.
    info.bold = info.baseName.contains(QLatin1String("Bold"));
    const PdfObject* desc = resolveObj(
        doc, dict.FindKey(PdfName("FontDescriptor")));
    if (desc != nullptr && desc->IsDictionary()) {
        const PdfObject* fw =
            desc->GetDictionary().FindKey(PdfName("FontWeight"));
        if (fw != nullptr && fw->IsNumberOrReal() && fw->GetReal() >= 600.0)
            info.bold = true;
        const PdfObject* sv = desc->GetDictionary().FindKey(PdfName("StemV"));
        if (sv != nullptr && sv->IsNumberOrReal()
            && sv->GetReal() >= kBoldStemV)
            info.bold = true;
    }

    const bool hasToUnicode = dict.FindKey(PdfName("ToUnicode")) != nullptr;
    auto tryToUnicode = [&]() -> bool {
        if (!hasToUnicode) return false;
        const PdfObject* tu = resolveObj(
            doc, dict.FindKey(PdfName("ToUnicode")));
        if (tu == nullptr) return false;
        std::unique_ptr<PdfEncodingMap> parsed;
        if (!PdfEncodingMapFactory::TryParseCMapEncoding(*tu, parsed)
            || parsed == nullptr)
            return false;
        info.map = std::move(parsed);
        return true;
    };

    if (info.isType0) {
        // CID text without a usable /ToUnicode CMap is the canonical
        // wrong-text hazard — never guessed, always disclosed.
        if (!tryToUnicode())
            info.disclosure = QStringLiteral("no ToUnicode map");
        else
            info.ok = true;
    } else {
        const PdfObject* enc = dict.FindKey(PdfName("Encoding"));
        if (hasToUnicode) {
            // A /ToUnicode map wins over every other ladder rung.
            if (tryToUnicode())
                info.ok = true;
            else
                info.disclosure = QStringLiteral("unusable ToUnicode map");
        } else if (enc != nullptr && enc->IsName()) {
            const PdfEncodingMap* pre =
                predefinedMap(qstringFromName(enc->GetName()));
            if (pre != nullptr) {
                // Non-owning: the factory returns singletons — a no-op
                // deleter keeps shared_ptr from deleting them.
                info.map = std::shared_ptr<const PdfEncodingMap>(
                    pre, [](const PdfEncodingMap*) {});
                info.ok = true;
            } else {
                info.disclosure = QStringLiteral("unknown /Encoding name");
            }
        } else if (enc != nullptr && enc->IsDictionary()) {
            const PdfDictionary& ed = enc->GetDictionary();
            const PdfEncodingMap* base =
                &PdfEncodingMapFactory::GetStandardEncodingInstance();
            const QString baseName = nameAt(ed, "BaseEncoding");
            if (!baseName.isEmpty()) {
                const PdfEncodingMap* pre = predefinedMap(baseName);
                if (pre != nullptr) base = pre;
            }
            // /Differences decode through the AGLFN glyph-name table on
            // top of the base encoding.
            parseDifferences(doc, ed.FindKey(PdfName("Differences")),
                             info.differences);
            info.map = std::shared_ptr<const PdfEncodingMap>(
                base, [](const PdfEncodingMap*) {});   // non-owning
            info.ok = true;
        } else if (isStandard14Text(info.baseName)) {
            // Standard-14 text faces without /Encoding: the builtin default.
            info.map = std::shared_ptr<const PdfEncodingMap>(
                &PdfEncodingMapFactory::GetStandardEncodingInstance(),
                [](const PdfEncodingMap*) {});   // non-owning singleton
            info.ok = true;
        } else {
            info.disclosure = QStringLiteral("no ToUnicode map");
        }
    }

    if (ref.ObjectNumber() != 0)
        return cache.byRef.emplace(ref, std::move(info)).first->second;
    // A direct (non-indirect) font dict gets its own slot.
    cache.unkeyed.push_back(std::make_unique<FontInfo>(std::move(info)));
    return *cache.unkeyed.back();
}

// Decode one show-string's raw bytes through the honest ladder. Undecodable
// codes are replaced (U+FFFD) and counted — the count is disclosed per page.
QString decodeString(const FontInfo& font, std::string_view raw,
                     int* undecodable) {
    QString out;
    if (font.isType0) {
        size_t i = 0;
        while (i < raw.size()) {
            bool done = false;
            for (unsigned char size : {2, 1}) {   // 2-byte CIDs dominate
                if (raw.size() - i < size) continue;
                unsigned code = 0;
                for (int k = 0; k < size; ++k)
                    code = (code << 8)
                           | static_cast<unsigned>(
                               static_cast<unsigned char>(raw[i + k]));
                if (font.map != nullptr) {
                    PoDoFo::CodePointSpan span;
                    if (font.map->TryGetCodePoints(
                            PoDoFo::PdfCharCode(code, size), span)) {
                        out += spanToQString(span);
                        i += size;
                        done = true;
                        break;
                    }
                }
            }
            if (!done) {
                ++*undecodable;
                out.append(QChar(0xFFFD));
                i += 1;
            }
        }
        return out;
    }
    for (const char ch : raw) {
        const unsigned code =
            static_cast<unsigned>(static_cast<unsigned char>(ch));
        const auto diff = font.differences.find(code);
        if (diff != font.differences.end()) {
            out += diff->second;
            continue;
        }
        if (font.map != nullptr) {
            PoDoFo::CodePointSpan span;
            if (font.map->TryGetCodePoints(PoDoFo::PdfCharCode(code, 1),
                                           span)) {
                out += spanToQString(span);
                continue;
            }
        }
        ++*undecodable;
        out.append(QChar(0xFFFD));
    }
    return out;
}

// ── the analysis walk (single source of truth) ───────────────────────────────

struct RunRec {
    int streamIdx = -1;
    int eventIdx = -1;
    QString text;
    double x = 0, y = 0;
    double size = 0;
    bool bold = false;
    int pageIdx = -1;
    int elementIdx = -1;   // filled during element assignment
};

struct ImageDoRec {
    int streamIdx = -1;
    int eventIdx = -1;
    int pageIdx = -1;
    double x = 0, y = 0;
    PdfObject* imageObj = nullptr;   // for the /Alt policy (§4)
    int elementIdx = -1;             // its Figure; -1 = undescribed → excluded
};

struct StreamRec {
    int index = -1;                   // StructParents key
    PdfObject* contentObj = nullptr;  // page object or form XObject
    bool isForm = false;
    int pageIdx = -1;
};

struct PageAnalysis {
    bool honestyOk = true;          // every /Tf'd font honestly decodable
    QString disclosure;             // the font that broke the gate (§3.2)
    bool hasParseErrors = false;    // broken operators → not safely rewritable
    bool hasMarkedContent = false;  // existing MCIDs → never re-numbered
};

struct WalkOutput {
    QList<RunRec> runs;
    QList<ImageDoRec> imageDos;
    QList<StreamRec> streams;
    QList<PageAnalysis> pages;
    FontCache fontCache;
    int nextStreamKey = 0;

    int pageOfStream(int streamIdx) const {
        for (const StreamRec& s : streams)
            if (s.index == streamIdx) return s.pageIdx;
        return -1;
    }
};

struct GroupEdge {
    bool start = false;
    bool end = false;
    int mcid = -1;
    QString tag;
};

using StreamGroups = std::map<int, GroupEdge>;   // event idx → edge

struct TagEnv {
    PoDoFo::PdfMemDocument& doc;
    WalkOutput& out;
    int pageIdx = -1;
    const PdfDictionary* resources = nullptr;  // own-or-inherited
    // Rewrite-only state (event parity guaranteed by the identical walk):
    bool rewriting = false;
    std::string* emitted = nullptr;
    const StreamGroups* groups = nullptr;
    bool perturbArmed = false;
    std::set<PdfReference>* visitedForms = nullptr;
    int depth = 0;
    // Invariant mode: the candidate carries the BDC/EMC the rewrite just
    // inserted — run recording must not be suppressed by them.
    bool invariantMode = false;
};

void walkOneStream(TagEnv env, PdfCanvas& canvas, StreamRec& stream);

// Descend into one form invocation (analysis only; first invocation wins —
// a form drawn twice is tagged once, at its first position: a disclosed
// simplification, recorded in the ledger row).
void walkFormInvocation(TagEnv& env, PdfObject* formObj, const Mat& ctmAtDo) {
    if (formObj == nullptr || !formObj->IsDictionary()) return;
    if (env.depth >= kMaxFormDepth) return;
    const PdfReference ref = formObj->GetIndirectReference();
    if (!env.visitedForms->insert(ref).second) return;  // recursion guard

    const PdfDictionary& dict = formObj->GetDictionary();
    const PdfObject* ownRes = resolveObj(
        env.doc, dict.FindKey(PdfName("Resources")));
    const PdfDictionary* res = ownRes != nullptr && ownRes->IsDictionary()
                                   ? &ownRes->GetDictionary()
                                   : env.resources;

    StreamRec s;
    s.index = env.out.nextStreamKey++;
    s.contentObj = formObj;
    s.isForm = true;
    s.pageIdx = env.pageIdx;
    env.out.streams.append(s);

    std::unique_ptr<PoDoFo::PdfXObject> xobj;
    if (!PoDoFo::PdfXObject::TryCreateFromObject(*formObj, xobj)
        || xobj == nullptr)
        return;
    auto* form = static_cast<PoDoFo::PdfXObjectForm*>(xobj.get());

    TagEnv sub = env;
    sub.resources = res;
    sub.depth = env.depth + 1;
    walkOneStream(sub, *form, s);
    Q_UNUSED(ctmAtDo);
}

// The ONE walk — the heart of the engine. Analysis mode records runs and
// image Do events; rewrite mode re-emits every token with the group's
// BDC/EMC markers inserted. Both passes iterate the SAME reader with the
// SAME flags, so event indices match by construction.
void walkOneStream(TagEnv env, PdfCanvas& canvas, StreamRec& stream) {
    std::vector<Mat> ctmStack;
    Mat ctm, tm, tlm;
    double leading = 0, tfSize = 0;
    const FontInfo* font = nullptr;
    int eventIdx = 0;

    PdfContentReaderArgs args;
    args.Flags = PdfContentReaderFlags::SkipFollowFormXObjects;
    PdfContentStreamReader reader(canvas, args);
    PdfContent content;

    auto resourceDict = [&](const char* key) -> const PdfObject* {
        if (env.resources == nullptr) return nullptr;
        return resolveObj(env.doc, env.resources->FindKey(PdfName(key)));
    };

    auto disclose = [&](const QString& s) {
        PageAnalysis& pa = env.out.pages[env.pageIdx];
        if (pa.disclosure.isEmpty()) pa.disclosure = s;
    };

    auto recordRun = [&](int idx, const QString& text) {
        RunRec r;
        r.streamIdx = stream.index;
        r.eventIdx = idx;
        r.pageIdx = env.pageIdx;
        r.text = text;
        const Mat m = ctm * tm;
        r.x = m.e;
        r.y = m.f;
        r.size = tfSize * std::hypot(tm.a, tm.b) * std::hypot(ctm.a, ctm.b);
        r.bold = font != nullptr && font->bold;
        env.out.runs.append(r);
    };

    while (reader.TryReadNext(content)) {
        const int idx = eventIdx++;
        if (env.rewriting) {
            const auto gIt = env.groups->find(idx);
            if (gIt != env.groups->end() && gIt->second.start) {
                *env.emitted += "/" + gIt->second.tag.toStdString()
                                + " <</MCID "
                                + std::to_string(gIt->second.mcid) + ">> BDC\n";
            }
        }
        if (content.HasErrors()) {
            if (!env.rewriting)
                env.out.pages[env.pageIdx].hasParseErrors = true;
            continue;
        }
        const PdfContentType type = content.GetType();
        if (type == PdfContentType::Operator
            || type == PdfContentType::UnexpectedKeyword) {
            const PdfOperator op = content.GetOperator();
            const PdfVariantStack& stk = content.GetStack();
            switch (op) {
                case PdfOperator::q:
                    ctmStack.push_back(ctm);
                    break;
                case PdfOperator::Q:
                    if (!ctmStack.empty()) {
                        ctm = ctmStack.back();
                        ctmStack.pop_back();
                    }
                    break;
                case PdfOperator::cm:
                    if (stk.GetSize() >= 6) ctm = matFromStack(stk) * ctm;
                    break;
                case PdfOperator::BT:
                    tm = Mat{};
                    tlm = Mat{};
                    break;
                case PdfOperator::TL:
                    if (stk.GetSize() >= 1) leading = numAt(stk[0]);
                    break;
                case PdfOperator::Td:
                    if (stk.GetSize() >= 2) {
                        tlm = Mat{1, 0, 0, 1, numAt(stk[1]),
                                  numAt(stk[0])} * tlm;
                        tm = tlm;
                    }
                    break;
                case PdfOperator::TD:
                    if (stk.GetSize() >= 2) {
                        leading = -numAt(stk[0]);
                        tlm = Mat{1, 0, 0, 1, numAt(stk[1]),
                                  numAt(stk[0])} * tlm;
                        tm = tlm;
                    }
                    break;
                case PdfOperator::Tm:
                    if (stk.GetSize() >= 6) {
                        tlm = matFromStack(stk);
                        tm = tlm;
                    }
                    break;
                case PdfOperator::T_Star:
                    tlm = Mat{1, 0, 0, 1, 0, -leading} * tlm;
                    tm = tlm;
                    break;
                case PdfOperator::Tf:
                    if (stk.GetSize() >= 2 && stk[1].IsName()) {
                        tfSize = numAt(stk[0]);
                        const PdfObject* fonts = resourceDict("Font");
                        if (fonts != nullptr && fonts->IsDictionary()) {
                            const PdfObject* fobj = resolveObj(
                                env.doc,
                                fonts->GetDictionary().FindKey(
                                    stk[1].GetName()));
                            font = &resolveFont(env.doc, fobj,
                                                env.out.fontCache);
                            PageAnalysis& pa = env.out.pages[env.pageIdx];
                            if (font != nullptr && !font->ok && pa.honestyOk) {
                                pa.honestyOk = false;
                                pa.disclosure = font->baseName.isEmpty()
                                                    ? font->disclosure
                                                    : font->baseName;
                            }
                        }
                    }
                    break;
                case PdfOperator::Tj:
                case PdfOperator::Quote:
                case PdfOperator::DoubleQuote: {
                    if (op != PdfOperator::Tj) {
                        // ' and " imply a line break first.
                        tlm = Mat{1, 0, 0, 1, 0, -leading} * tlm;
                        tm = tlm;
                    }
                    // The show string is the operand pushed LAST - for
                    // all three show operators it is stk[0].
                    const PageAnalysis& pa = env.out.pages[env.pageIdx];
                    if (!env.rewriting && stk.GetSize() >= 1
                        && font != nullptr && font->ok && pa.honestyOk
                        && !pa.hasParseErrors && !pa.hasMarkedContent
                        && stk[0].IsString()) {
                        int undecodable = 0;
                        const QString text = decodeString(
                            *font, stk[0].GetString().GetString(),
                            &undecodable);
                        recordRun(idx, text);
                        if (undecodable > 0)
                            disclose(QStringLiteral(
                                         "%1 character code(s) did not decode")
                                         .arg(undecodable));
                    }
                    break;
                }
                case PdfOperator::TJ: {
                    const PageAnalysis& pa = env.out.pages[env.pageIdx];
                    if (!env.rewriting && stk.GetSize() >= 1 && stk[0].IsArray()
                        && font != nullptr && font->ok && pa.honestyOk
                        && !pa.hasParseErrors && !pa.hasMarkedContent) {
                        QString text;
                        int undecodable = 0;
                        for (const PdfObject& el : stk[0].GetArray()) {
                            if (el.IsString()) {
                                text += decodeString(
                                    *font, el.GetString().GetString(),
                                    &undecodable);
                            }
                            // Numeric kerning adjustments are rendering-only.
                        }
                        if (!text.isEmpty()) {
                            recordRun(idx, text);
                            if (undecodable > 0)
                                disclose(QStringLiteral(
                                             "%1 character code(s) did not "
                                             "decode")
                                             .arg(undecodable));
                        }
                    }
                    break;
                }
                case PdfOperator::BDC: {
                    // Existing marked content: the engine never re-numbers
                    // another writer's MCIDs — the page is disclosed+skipped.
                    // (Invariant mode ignores them: the candidate's markers
                    // are our own output, not a foreign structure.)
                    if (!env.rewriting && !env.invariantMode
                        && stk.GetSize() >= 2
                        && stk[0].IsDictionary()
                        && stk[0].GetDictionary().FindKey(PdfName("MCID"))
                               != nullptr)
                        env.out.pages[env.pageIdx].hasMarkedContent = true;
                    break;
                }
                default:
                    break;
            }
        } else if (type == PdfContentType::DoXObject) {
            if (!env.rewriting && content->Name != nullptr) {
                const PdfObject* xobjs = resourceDict("XObject");
                if (xobjs != nullptr && xobjs->IsDictionary()) {
                    PdfObject* xo = resolveObjMutable(
                        env.doc,
                        const_cast<PdfObject*>(xobjs->GetDictionary().FindKey(
                            *content->Name)));
                    if (xo != nullptr && xo->IsDictionary()) {
                        const QString sub = nameAt(xo->GetDictionary(),
                                                   "Subtype");
                        if (sub == QLatin1String("Image")) {
                            ImageDoRec d;
                            d.streamIdx = stream.index;
                            d.eventIdx = idx;
                            d.pageIdx = env.pageIdx;
                            d.x = ctm.e;
                            d.y = ctm.f;
                            d.imageObj = xo;
                            env.out.imageDos.append(d);
                        } else if (sub == QLatin1String("Form")) {
                            // Depth-first: the form's runs join the page's
                            // reading order at exactly this point.
                            walkFormInvocation(env, xo, ctm);
                        }
                    }
                }
            }
        }

        if (env.rewriting) {
            // ── token re-emission (the analysis pass skips all of this) ──
            switch (type) {
                case PdfContentType::Operator:
                case PdfContentType::UnexpectedKeyword: {
                    const PdfVariantStack& stk = content.GetStack();
                    std::string line;
                    bool perturbedHere = false;
                    if (env.perturbArmed
                        && content.GetOperator() == PdfOperator::Tj) {
                        // TaggerFaultForTesting::PerturbOneShowOperator:
                        // corrupt exactly one show operator — the invariant
                        // MUST catch this and fail the candidate.
                        line = "() Tj\n";
                        perturbedHere = true;
                    } else {
                        // Reverse iteration: index 0 is the operand pushed last, so
                        // this re-emits the original operand order.
                        for (unsigned i = stk.GetSize(); i-- > 0;)
                            line += stk[i].ToString() + " ";
                        line += std::string(content.GetKeyword()) + "\n";
                    }
                    *env.emitted += line;
                    if (perturbedHere) env.perturbArmed = false;
                    break;
                }
                case PdfContentType::DoXObject: {
                    if (content->Name != nullptr)
                        *env.emitted += PdfVariant(*content->Name).ToString()
                                        + " Do\n";
                    break;
                }
                case PdfContentType::ImageDictionary: {
                    std::string line = "BI\n<<";
                    const PdfDictionary& d =
                        content.GetInlineImageDictionary();
                    for (const auto& entry : d) {
                        line += PdfVariant(entry.first).ToString() + " ";
                        line += entry.second.ToString() + " ";
                    }
                    line += ">>\nID\n";
                    *env.emitted += line;
                    break;
                }
                case PdfContentType::ImageData: {
                    const PoDoFo::charbuff& data =
                        content.GetInlineImageData();
                    env.emitted->append(data.data(), data.size());
                    *env.emitted += "\nEI\n";
                    break;
                }
                default:
                    break;
            }
            const auto gIt = env.groups->find(idx);
            if (gIt != env.groups->end() && gIt->second.end)
                *env.emitted += "EMC\n";
        }
    }
}

// Analysis pass over every page (streams discovered; runs + image Dos).
// `invariantMode` records runs even on pages carrying marked content (the
// candidate re-extraction), without touching the honesty gates.
void analyzeDocument(PoDoFo::PdfMemDocument& doc, WalkOutput& out,
                     bool invariantMode = false) {
    const unsigned pageCount = doc.GetPages().GetCount();
    out.pages.resize(pageCount);
    std::set<PdfReference> visitedForms;
    for (unsigned pi = 0; pi < pageCount; ++pi) {
        PdfPage& page = doc.GetPages().GetPageAt(pi);
        StreamRec s;
        s.index = out.nextStreamKey++;
        s.contentObj = &doc.GetObjects().MustGetObject(
            page.GetObject().GetIndirectReference());
        s.isForm = false;
        s.pageIdx = static_cast<int>(pi);
        out.streams.append(s);

        const PdfDictionary& resDict =
            page.GetResources().GetObject().GetDictionary();

        TagEnv env{doc,   out,      static_cast<int>(pi), &resDict,
                   false, nullptr,  nullptr,              false,
                   &visitedForms, 0, invariantMode};
        walkOneStream(env, page, s);
    }
}

// Rewrite ONE stream (replays the identical walk; groups carry the markers).
bool rewriteStream(PoDoFo::PdfMemDocument& doc, const StreamRec& stream,
                   const StreamGroups& groups, std::string& emitted,
                   bool perturb) {
    WalkOutput scratch;   // analysis outputs of the replay are discarded
    scratch.pages.resize(doc.GetPages().GetCount());
    std::set<PdfReference> visitedForms;

    const PdfDictionary* resDict = nullptr;
    PdfPage* page = nullptr;
    std::unique_ptr<PoDoFo::PdfXObject> xobj;
    PdfXObjectForm* form = nullptr;

    if (stream.isForm) {
        if (stream.contentObj == nullptr) return false;
        if (!PoDoFo::PdfXObject::TryCreateFromObject(*stream.contentObj, xobj)
            || xobj == nullptr)
            return false;
        form = static_cast<PoDoFo::PdfXObjectForm*>(xobj.get());
        // Own /Resources (or none — inherited resources are irrelevant to
        // the rewrite pass, which never resolves fonts).
        PdfObject* resObj = resolveObjMutable(
            doc, const_cast<PdfObject*>(
                     stream.contentObj->GetDictionary().FindKey(
                         PdfName("Resources"))));
        if (resObj != nullptr && resObj->IsDictionary())
            resDict = &resObj->GetDictionary();
    } else {
        if (stream.pageIdx < 0
            || stream.pageIdx >= static_cast<int>(doc.GetPages().GetCount()))
            return false;
        page = &doc.GetPages().GetPageAt(
            static_cast<unsigned>(stream.pageIdx));
        resDict = &page->GetResources().GetObject().GetDictionary();
    }

    TagEnv env{doc,      scratch, stream.pageIdx, resDict, true,
               &emitted, &groups, perturb,        &visitedForms, 0};

    if (form != nullptr) {
        walkOneStream(env, *form, const_cast<StreamRec&>(stream));
    } else if (page != nullptr) {
        walkOneStream(env, *page, const_cast<StreamRec&>(stream));
    } else {
        return false;
    }
    return true;
}

// ── clustering (§3.1–§3.4) ───────────────────────────────────────────────────

struct LineRec {
    int pageIdx = -1;
    QList<int> runIdxs;
    double y = 0, xLeft = 0, size = 0;
    bool bold = false;
    int clusterIdx = -1;
};

struct SizeCluster {
    double size = 0;
    int lineCount = 0;
    bool bold = false;
    QString level;   // "H1".."H6" or "body"
};

struct ElementRec {
    int pageIdx = -1;
    QString tag;
    QList<int> runIdxs;
    QList<int> imageDoIdxs;
    double y = 0, x = 0;
    QString alt;           // figures only
    QList<int> mcids;      // filled at group assignment
    PdfReference ref{};    // filled at assembly
};

// Most frequent value rounded to 0.1; ties → smallest. Empty list → false.
bool modalValue(const QList<double>& values, double* out) {
    if (values.isEmpty()) return false;
    std::map<long long, int> counts;
    for (double v : values)
        ++counts[std::llround(v * 10.0)];
    long long best = counts.begin()->first;
    int bestN = 0;
    for (const auto& entry : counts) {
        if (entry.second > bestN
            || (entry.second == bestN && entry.first < best)) {
            best = entry.first;
            bestN = entry.second;
        }
    }
    *out = static_cast<double>(best) / 10.0;
    return true;
}

// Document-relative size clusters over LINES (§3.3); assigns line.clusterIdx.
QList<SizeCluster> clusterSizes(QList<LineRec>& lines) {
    QList<SizeCluster> clusters;
    if (lines.isEmpty()) return clusters;

    // 1-D clustering: sort sizes descending, break when the ratio between
    // the cluster's representative and the next size exceeds 1.08 (the
    // "just noticeably larger" heuristic — pinned by tests, never tuned on
    // live documents).
    QList<int> order;
    order.reserve(lines.size());
    for (int i = 0; i < lines.size(); ++i) order.append(i);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return lines[a].size > lines[b].size;
    });

    QList<QList<int>> members;
    for (int idx : order) {
        const bool newCluster =
            members.isEmpty() || members.last().isEmpty()
            || lines[members.last().first()].size / lines[idx].size
                   > kHeadingRatioBreak;
        if (newCluster)
            members.append(QList<int>{idx});
        else
            members.last().append(idx);
    }

    // Representative = the largest member size; the modal cluster = body.
    int bodyIdx = -1;
    for (int ci = 0; ci < members.size(); ++ci) {
        SizeCluster c;
        c.size = lines[members[ci].first()].size;
        c.lineCount = members[ci].size();
        for (int li : members[ci])
            if (lines[li].bold) c.bold = true;
        clusters.append(c);
        if (bodyIdx < 0 || c.lineCount > clusters[bodyIdx].lineCount
            || (c.lineCount == clusters[bodyIdx].lineCount
                && c.size > clusters[bodyIdx].size)) {
            bodyIdx = ci;
        }
    }
    const double bodySize = clusters[bodyIdx].size;

    // Heading ranks: clusters LARGER than the body, size descending (bold
    // breaks exact size ties toward the better level), mapped to H1..H6.
    QList<int> headingOrder;
    for (int ci = 0; ci < clusters.size(); ++ci)
        if (ci != bodyIdx && clusters[ci].size > bodySize + 0.05)
            headingOrder.append(ci);
    std::stable_sort(headingOrder.begin(), headingOrder.end(),
                     [&](int a, int b) {
                         if (clusters[a].size != clusters[b].size)
                             return clusters[a].size > clusters[b].size;
                         return clusters[a].bold && !clusters[b].bold;
                     });
    for (int rank = 0; rank < headingOrder.size(); ++rank) {
        const int level = std::min(rank + 1, 6);
        clusters[headingOrder[rank]].level = QStringLiteral("H%1").arg(level);
    }
    for (int ci = 0; ci < clusters.size(); ++ci)
        if (clusters[ci].level.isEmpty())
            clusters[ci].level = QStringLiteral("body");

    for (int ci = 0; ci < members.size(); ++ci)
        for (int li : members[ci])
            lines[li].clusterIdx = ci;
    return clusters;
}

// Build visual lines from runs (walk order; baseline-y proximity tolerance).
QList<LineRec> buildLines(const WalkOutput& out, int pageIdx,
                          double lineTolerance) {
    QList<LineRec> lines;
    for (int ri = 0; ri < out.runs.size(); ++ri) {
        const RunRec& r = out.runs[ri];
        if (r.pageIdx != pageIdx) continue;
        if (!lines.isEmpty()
            && std::abs(r.y - lines.last().y) <= lineTolerance) {
            lines.last().runIdxs.append(ri);
            lines.last().xLeft = std::min(lines.last().xLeft, r.x);
            lines.last().size = std::max(lines.last().size, r.size);
            if (r.bold) lines.last().bold = true;
        } else {
            LineRec l;
            l.pageIdx = pageIdx;
            l.runIdxs = QList<int>{ri};
            l.y = r.y;
            l.xLeft = r.x;
            l.size = r.size;
            l.bold = r.bold;
            lines.append(l);
        }
    }
    // Reading order: lines by (y desc, xLeft asc); runs within a line by x.
    for (LineRec& l : lines) {
        std::stable_sort(l.runIdxs.begin(), l.runIdxs.end(),
                         [&](int a, int b) {
                             return out.runs[a].x < out.runs[b].x;
                         });
    }
    std::stable_sort(lines.begin(), lines.end(), [](const LineRec& a,
                                                    const LineRec& b) {
        if (a.y != b.y) return a.y > b.y;   // PDF y-up: top first
        return a.xLeft < b.xLeft;
    });
    return lines;
}

// Group a page's ordered lines into P / H* elements (§3.1 step 3–4).
QList<ElementRec> elementsFromLines(const WalkOutput& out,
                                    const QList<LineRec>& ordered,
                                    const QList<SizeCluster>& clusters,
                                    int pageIdx, double modalGap) {
    QList<ElementRec> elements;
    QList<int> current;
    double anchorX = 0;
    auto lineAt = [&](int li) -> const LineRec& { return ordered[li]; };

    auto flush = [&]() {
        if (current.isEmpty()) return;
        ElementRec el;
        el.pageIdx = pageIdx;
        const LineRec& first = lineAt(current.first());
        const QString& level = clusters[first.clusterIdx].level;
        el.tag = level == QLatin1String("body") ? QStringLiteral("P")
                                                : level;
        for (int li : current) {
            el.runIdxs.append(ordered[li].runIdxs);
            el.y = std::max(el.y, ordered[li].y);
        }
        el.x = first.xLeft;
        elements.append(el);
        current.clear();
    };

    for (int li = 0; li < ordered.size(); ++li) {
        const LineRec& line = lineAt(li);
        bool brk = current.isEmpty();
        if (!brk) {
            const LineRec& prev = lineAt(current.last());
            if (prev.clusterIdx != line.clusterIdx) {
                brk = true;   // a size-cluster change ends the element
            } else if (std::abs(line.xLeft - anchorX) > kXLeftTolerance) {
                brk = true;   // a different x-left family starts its own
            } else {
                const double gap = prev.y - line.y;   // y-up: top first
                const double limit =
                    modalGap > 0
                        ? kParagraphGapFactor * modalGap
                        : kParagraphGapFactor * prev.size * 1.2;
                if (gap > limit) brk = true;
            }
        }
        if (brk) {
            flush();
            anchorX = line.xLeft;
        }
        current.append(li);
    }
    flush();
    Q_UNUSED(out);
    return elements;
}

// The column-suspect signature (§3.4): a bimodal x-left distribution across
// a page's runs — ≥2 families, each holding ≥25% of runs, separated by more
// than 15% of the page width.
bool columnSuspect(const WalkOutput& out, int pageIdx, double pageWidth) {
    QList<double> xs;
    for (const RunRec& r : out.runs)
        if (r.pageIdx == pageIdx) xs.append(r.x);
    if (xs.size() < 2 || pageWidth <= 0) return false;
    std::sort(xs.begin(), xs.end());
    QList<double> anchors;   // family representatives
    double anchor = xs[0];
    int count = 0;
    for (double x : xs) {
        if (x - anchor > kXLeftTolerance) {
            anchors.append(anchor);
            anchor = x;
            count = 0;
        }
        ++count;
        Q_UNUSED(count);
    }
    anchors.append(anchor);
    if (anchors.size() < 2) return false;
    // Count runs per family.
    int big = 0;
    for (double a : anchors) {
        int n = 0;
        for (double x : xs)
            if (std::abs(x - a) <= kXLeftTolerance) ++n;
        if (n >= kColumnFamilyFrac * xs.size()) ++big;
    }
    const double span = anchors.last() - anchors.first();
    return big >= 2 && span > kColumnSpanFrac * pageWidth;
}

// Group the marked events (shows + figure Dos) of each stream into BDC/EMC
// groups with per-stream MCIDs in event (= reading) order. Splits of one
// element get several MCIDs (element /K becomes an array — §3.5).
std::map<int, StreamGroups> assignGroups(WalkOutput& out,
                                         QList<ElementRec>& elements) {
    struct MarkedEv {
        int eventIdx;
        int elementIdx;
    };
    std::map<int, QList<MarkedEv>> byStream;
    for (int ei = 0; ei < elements.size(); ++ei) {
        const ElementRec& el = elements[ei];
        for (int ri : el.runIdxs) {
            RunRec& r = out.runs[ri];
            r.elementIdx = ei;
            byStream[r.streamIdx].append({r.eventIdx, ei});
        }
        for (int di : el.imageDoIdxs) {
            byStream[out.imageDos[di].streamIdx].append(
                {out.imageDos[di].eventIdx, ei});
        }
    }

    std::map<int, StreamGroups> result;
    for (auto& [streamIdx, evs] : byStream) {
        std::sort(evs.begin(), evs.end(),
                  [](const MarkedEv& a, const MarkedEv& b) {
                      return a.eventIdx < b.eventIdx;
                  });
        StreamGroups& groups = result[streamIdx];
        // Cut maximal same-element runs of adjacent marked events first,
        // then number the MCIDs in ELEMENT READING order (§3.1 step 5) —
        // content order within one element, element order across the page.
        struct Group {
            int startEvent;
            int endEvent;
            int elementIdx;
        };
        QList<Group> cut;
        size_t i = 0;
        while (i < evs.size()) {
            size_t j = i;
            const int elementIdx = evs[i].elementIdx;
            while (j + 1 < evs.size()
                   && evs[j + 1].eventIdx == evs[j].eventIdx + 1
                   && evs[j + 1].elementIdx == elementIdx)
                ++j;
            cut.append({evs[i].eventIdx, evs[j].eventIdx, elementIdx});
            i = j + 1;
        }
        std::stable_sort(cut.begin(), cut.end(),
                         [](const Group& a, const Group& b) {
                             if (a.elementIdx != b.elementIdx)
                                 return a.elementIdx < b.elementIdx;
                             return a.startEvent < b.startEvent;
                         });
        int mcid = 0;
        for (const Group& g : cut) {
            GroupEdge& start = groups[g.startEvent];
            start.start = true;
            start.mcid = mcid;
            start.tag = elements[g.elementIdx].tag;
            GroupEdge& end = groups[g.endEvent];
            end.end = true;
            end.mcid = mcid;
            end.tag = elements[g.elementIdx].tag;
            elements[g.elementIdx].mcids.append(mcid);
            ++mcid;
        }
    }
    return result;
}

// Count images lacking a non-empty /Alt (the §4 policy), page resources
// downward incl. form XObjects, deduped — the P1 checker's walk idiom.
int countUndescribedImages(PoDoFo::PdfMemDocument& doc,
                           QList<QPair<int, QString>>* sample,
                           int sampleCap) {
    int total = 0;
    std::set<PdfReference> visited;
    auto walk = [&](auto&& self, const PdfObject* resources, int pageIdx,
                    int depth) -> void {
        if (resources == nullptr || depth > kMaxFormDepth) return;
        resources = resolveObj(doc, resources);
        if (resources == nullptr || !resources->IsDictionary()) return;
        const PdfObject* xobjs = resolveObj(
            doc, resources->GetDictionary().FindKey(PdfName("XObject")));
        if (xobjs == nullptr || !xobjs->IsDictionary()) return;
        for (const auto& entry : xobjs->GetDictionary()) {
            const PdfObject* xo = resolveObj(doc, &entry.second);
            if (xo == nullptr || !xo->IsDictionary()) continue;
            if (xo->GetIndirectReference().ObjectNumber() != 0
                && !visited.insert(xo->GetIndirectReference()).second)
                continue;   // shared resource — count once
            const PdfDictionary& dict = xo->GetDictionary();
            if (nameAt(dict, "Subtype") == QLatin1String("Image")) {
                const QString alt = stringAt(dict, "Alt").trimmed();
                if (!alt.isEmpty()) continue;
                ++total;
                if (sample != nullptr && sample->size() < sampleCap)
                    sample->append({pageIdx, qstringFromName(entry.first)});
            } else if (nameAt(dict, "Subtype") == QLatin1String("Form")) {
                self(self, dict.FindKey(PdfName("Resources")), pageIdx,
                     depth + 1);
            }
        }
    };
    for (unsigned pi = 0; pi < doc.GetPages().GetCount(); ++pi) {
        PdfPage& page = doc.GetPages().GetPageAt(pi);
        walk(walk, &page.GetResources().GetObject(),
             static_cast<int>(pi), 0);
    }
    return total;
}

// ── the invariant's extraction sequence (§2.2) ───────────────────────────────

QString canonicalText(const QString& s) {
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    return QString(s).replace(ws, QStringLiteral(" ")).trimmed();
}

QList<QList<QPair<QString, long long>>> extractionSequences(
    const WalkOutput& out) {
    QList<QList<QPair<QString, long long>>> seq(out.pages.size());
    for (const RunRec& r : out.runs) {
        if (r.pageIdx < 0 || r.pageIdx >= seq.size()) continue;
        seq[r.pageIdx].append({canonicalText(r.text),
                               std::llround(r.y * 10.0)});
    }
    return seq;
}

} // namespace

// ── the public pre-flight ────────────────────────────────────────────────────

TaggerPreflight preflightTagging(const QString& path) {
    TaggerPreflight p;
    if (path.isEmpty()) {
        p.loadError = QStringLiteral("no document");
        return p;
    }
    PoDoFo::PdfMemDocument doc;
    try {
        doc.Load(path.toUtf8().constData());
    } catch (const PoDoFo::PdfError& e) {
        p.loadError = QString::fromUtf8(e.what());
        return p;
    } catch (const std::exception& e) {
        p.loadError = QString::fromUtf8(e.what());
        return p;
    }

    p.loadOk = true;

    const PdfObject* root = resolveObj(
        doc, doc.GetCatalog().GetDictionary().FindKey(
                 PdfName("StructTreeRoot")));
    if (root != nullptr && root->IsDictionary()) {
        p.alreadyTagged = true;
        return p;
    }

    WalkOutput out;
    try {
        analyzeDocument(doc, out);
    } catch (const PoDoFo::PdfError& e) {
        p.loadError = QStringLiteral("content analysis failed: %1")
                          .arg(QString::fromUtf8(e.what()));
        return p;
    } catch (const std::exception& e) {
        p.loadError = QStringLiteral("content analysis failed: %1")
                          .arg(QString::fromUtf8(e.what()));
        return p;
    }

    // Build the document's lines (honest pages only — the same shape the
    // tagging pass will produce, so the preview IS what would be applied).
    QList<double> runSizes;
    for (const RunRec& r : out.runs)
        if (out.pages[r.pageIdx].honestyOk
            && !out.pages[r.pageIdx].hasParseErrors
            && !out.pages[r.pageIdx].hasMarkedContent)
            runSizes.append(r.size);
    double modalSize = 0;
    modalValue(runSizes, &modalSize);
    const double tol = std::max(1.0, 0.5 * modalSize * 1.2);

    QList<LineRec> allLines;
    p.anyText = !runSizes.isEmpty();
    for (int pi = 0; pi < out.pages.size(); ++pi) {
        const PageAnalysis& pa = out.pages[pi];
        if (!pa.honestyOk || pa.hasParseErrors || pa.hasMarkedContent)
            continue;
        allLines.append(buildLines(out, pi, tol));
    }
    const QList<SizeCluster> clusters = clusterSizes(allLines);
    for (const SizeCluster& c : clusters) {
        TaggerSizeCluster row;
        row.size = c.size;
        row.runCount = c.lineCount;
        row.bold = c.bold;
        row.level = c.level;
        p.sizeClusters.append(row);
    }

    // The image prompt list (bounded sample + disclosed total).
    QList<QPair<int, QString>> sample;
    try {
        p.imagesTotal = countUndescribedImages(doc, &sample,
                                               kA11yMaxImageFindings);
    } catch (const PoDoFo::PdfError& e) {
        p.loadError = QStringLiteral("image walk failed: %1")
                          .arg(QString::fromUtf8(e.what()));
        return p;
    } catch (const std::exception& e) {
        p.loadError = QStringLiteral("image walk failed: %1")
                          .arg(QString::fromUtf8(e.what()));
        return p;
    }
    for (const auto& s : sample) {
        TaggerImageGap gap;
        gap.page = s.first;
        gap.resourceName = s.second;
        p.imageGaps.append(gap);
    }
    return p;
}

QString validateTaggedStructureTree(const QString& path) {
    if (path.isEmpty())
        return QStringLiteral("no document to validate");
    PoDoFo::PdfMemDocument doc;
    try {
        doc.Load(path.toUtf8().constData());
    } catch (const PoDoFo::PdfError& e) {
        return QString::fromUtf8(e.what());
    } catch (const std::exception& e) {
        return QString::fromUtf8(e.what());
    }

    try {
        const PdfObject* rootObj = resolveObj(
            doc, doc.GetCatalog().GetDictionary().FindKey(
                     PdfName("StructTreeRoot")));
        if (rootObj == nullptr || !rootObj->IsDictionary())
            return QStringLiteral(
                "no /StructTreeRoot in this document — nothing to validate");
        const PdfDictionary& root = rootObj->GetDictionary();
        if (nameAt(root, "Type") != QLatin1String("StructTreeRoot"))
            return QStringLiteral(
                "/StructTreeRoot lacks /Type /StructTreeRoot");

        const PdfObject* markInfo = resolveObj(
            doc, doc.GetCatalog().GetDictionary().FindKey(
                     PdfName("MarkInfo")));
        const PdfObject* marked =
            markInfo != nullptr && markInfo->IsDictionary()
                ? markInfo->GetDictionary().FindKey(PdfName("Marked"))
                : nullptr;
        if (marked == nullptr || !marked->IsBool() || !marked->GetBool())
            return QStringLiteral("/MarkInfo /Marked is not true");

        // ── the marked content actually present, per stream ─────────────
        // streamKey → {mcids}; pageIdx → stream keys (ownership, for /Pg).
        std::map<int, std::set<int>> streamMcids;   // StructParents → MCIDs
        std::map<int, std::set<int>> pageStreams;   // pageIdx → stream keys
        std::set<PdfReference> visitedForms;
        const unsigned pageCount = doc.GetPages().GetCount();

        auto walkMarked = [&](auto&& self, PdfCanvas& canvas,
                              const PdfObject* contentObj,
                              const PdfDictionary* resources, int pageIdx,
                              int depth) -> void {
            if (depth > kMaxFormDepth) return;
            const int key = contentObj != nullptr
                                ? [&] {
                                      const PdfObject* sp =
                                          contentObj->GetDictionary().FindKey(
                                              PdfName("StructParents"));
                                      return sp != nullptr && sp->IsNumber()
                                             ? static_cast<int>(
                                                   sp->GetNumber())
                                             : -1;
                                  }()
                                : -1;
            if (key >= 0) pageStreams[pageIdx].insert(key);

            PdfContentReaderArgs args;
            args.Flags = PdfContentReaderFlags::SkipFollowFormXObjects;
            PdfContentStreamReader reader(canvas, args);
            PdfContent content;
            while (reader.TryReadNext(content)) {
                const PdfContentType type = content.GetType();
                if (type == PdfContentType::DoXObject
                    && content->Name != nullptr && resources != nullptr) {
                    const PdfObject* xobjs = resolveObj(
                        doc, resources->FindKey(PdfName("XObject")));
                    if (xobjs != nullptr && xobjs->IsDictionary()) {
                        PdfObject* xo = resolveObjMutable(
                            doc,
                            const_cast<PdfObject*>(
                                xobjs->GetDictionary().FindKey(
                                    *content->Name)));
                        if (xo != nullptr && xo->IsDictionary()
                            && nameAt(xo->GetDictionary(), "Subtype")
                                   == QLatin1String("Form")) {
                            const PdfReference ref =
                                xo->GetIndirectReference();
                            if (visitedForms.insert(ref).second) {
                                const PdfObject* fres = resolveObj(
                                    doc, xo->GetDictionary().FindKey(
                                             PdfName("Resources")));
                                std::unique_ptr<PoDoFo::PdfXObject> fxobj;
                                if (PoDoFo::PdfXObject::TryCreateFromObject(
                                        *xo, fxobj)
                                    && fxobj != nullptr) {
                                    self(self,
                                         *static_cast<PoDoFo::PdfXObjectForm*>(
                                             fxobj.get()),
                                         xo,
                                         fres != nullptr
                                             && fres->IsDictionary()
                                             ? &fres->GetDictionary()
                                             : resources,
                                         pageIdx, depth + 1);
                                }
                            }
                        }
                    }
                    continue;
                }
                if (type != PdfContentType::Operator) continue;
                if (content.GetOperator() == PdfOperator::BDC
                    && content.GetStack().GetSize() >= 2
                    && content.GetStack()[0].IsDictionary()) {
                    const PdfObject* mcid =
                        content.GetStack()[0].GetDictionary().FindKey(
                            PdfName("MCID"));
                    if (mcid != nullptr && mcid->IsNumber() && key >= 0)
                        streamMcids[key].insert(
                            static_cast<int>(mcid->GetNumber()));
                }
            }
        };

        for (unsigned pi = 0; pi < pageCount; ++pi) {
            PdfPage& page = doc.GetPages().GetPageAt(pi);
            const PdfDictionary& resDict =
                page.GetResources().GetObject().GetDictionary();
            walkMarked(walkMarked, page,
                       &doc.GetObjects().MustGetObject(
                           page.GetObject().GetIndirectReference()),
                       &resDict, static_cast<int>(pi), 0);
        }

        // ── the tree ────────────────────────────────────────────────────
        const PdfReference rootRef = rootObj->GetIndirectReference();
        const PdfObject* kidsObj = resolveObj(
            doc, root.FindKey(PdfName("K")));
        if (kidsObj == nullptr || !kidsObj->IsArray())
            return QStringLiteral("root /K is missing or not an array");

        // ParentTree, keyed by StructParents.
        std::map<int, std::map<int, PdfReference>> parentTree;
        {
            const PdfObject* pt = resolveObj(
                doc, root.FindKey(PdfName("ParentTree")));
            if (pt == nullptr || !pt->IsDictionary())
                return QStringLiteral("/ParentTree is missing");
            const PdfObject* nums = resolveObj(
                doc, pt->GetDictionary().FindKey(PdfName("Nums")));
            if (nums == nullptr || !nums->IsArray())
                return QStringLiteral("/ParentTree /Nums is missing");
            const PdfArray& arr = nums->GetArray();
            if (arr.GetSize() % 2 != 0)
                return QStringLiteral("/ParentTree /Nums is unpaired");
            for (size_t i = 0; i + 1 < arr.GetSize(); i += 2) {
                if (!arr[i].IsNumber())
                    return QStringLiteral("/ParentTree key is not a number");
                const int key = static_cast<int>(arr[i].GetNumber());
                const PdfObject* val = resolveObj(doc, &arr[i + 1]);
                if (val == nullptr || !val->IsArray())
                    return QStringLiteral(
                        "/ParentTree value is not an array");
                std::map<int, PdfReference>& perStream = parentTree[key];
                for (size_t m = 0; m < val->GetArray().GetSize(); ++m) {
                    const PdfObject& refObj = val->GetArray()[m];
                    PdfReference r;
                    if (!refOf(&refObj, r))
                        return QStringLiteral(
                            "/ParentTree entry is not a reference");
                    perStream[static_cast<int>(m)] = r;
                }
            }
        }

        // Walk elements: shape, /P back-links, /Pg, /K ↔ ParentTree.
        struct Frame {
            const PdfDictionary* el;
            PdfReference ref;
            PdfReference parent;
        };
        QList<Frame> stack;
        for (const PdfObject& k : kidsObj->GetArray()) {
            const PdfObject* el = resolveObj(doc, &k);
            PdfReference elRef;
            if (el == nullptr || !el->IsDictionary() || !refOf(&k, elRef))
                return QStringLiteral("root /K entry is not a reference");
            stack.append({&el->GetDictionary(), elRef, rootRef});
        }
        if (stack.isEmpty())
            return QStringLiteral("the structure tree is empty");

        int guards = 0;
        while (!stack.isEmpty()) {
            if (++guards > 1000000)
                return QStringLiteral("structure tree walk overflowed");
            const Frame f = stack.takeLast();
            const PdfDictionary& el = *f.el;

            const QString tag = nameAt(el, "S");
            static const char* kTags[] = {"P", "H1", "H2", "H3", "H4", "H5",
                                          "H6", "Figure"};
            bool known = false;
            for (const char* t : kTags)
                if (tag == QLatin1String(t)) known = true;
            if (!known)
                return QStringLiteral(
                    "element /S %1 is not a P2 structure type").arg(tag);

            // /P back-link: no orphans (inlined or reference - see refOf).
            const PdfObject* pKey = el.FindKey(PdfName("P"));
            PdfReference parentRef;
            if (pKey == nullptr || !refOf(pKey, parentRef))
                return QStringLiteral("element /S %1 has no /P").arg(tag);
            const PdfObject* parent = resolveObj(doc, pKey);
            if (parent == nullptr || !parent->IsDictionary())
                return QStringLiteral("element /S %1 has a dangling /P")
                           .arg(tag);
            if (!(parentRef == f.parent))
                return QStringLiteral(
                    "element /S %1 has an orphan /P link").arg(tag);

            // /K: int or array of ints; cross-check against ParentTree and
            // the stream's marked content.
            const PdfObject* kObj = el.FindKey(PdfName("K"));
            QList<int> mcids;
            if (kObj == nullptr)
                return QStringLiteral("element /S %1 has no /K").arg(tag);
            if (kObj->IsNumber()) {
                mcids.append(static_cast<int>(kObj->GetNumber()));
            } else if (kObj->IsArray()) {
                for (const PdfObject& m : kObj->GetArray()) {
                    if (!m.IsNumber())
                        return QStringLiteral(
                            "element /S %1 has a non-numeric /K entry")
                               .arg(tag);
                    mcids.append(static_cast<int>(m.GetNumber()));
                }
            } else {
                return QStringLiteral(
                    "element /S %1 has an unsupported /K").arg(tag);
            }
            if (mcids.isEmpty())
                return QStringLiteral("element /S %1 has an empty /K")
                           .arg(tag);

            // /Pg: must resolve to a page, and every MCID's stream must be
            // owned by that page.
            const PdfObject* pg = resolveObj(doc, el.FindKey(PdfName("Pg")));
            if (pg == nullptr || !pg->IsDictionary()
                || nameAt(pg->GetDictionary(), "Type") != QLatin1String("Page"))
                return QStringLiteral(
                    "element /S %1 has a missing or non-page /Pg").arg(tag);
            int pgIdx = -1;
            for (unsigned pi = 0; pi < pageCount; ++pi) {
                const PdfObject& p = doc.GetObjects().MustGetObject(
                    doc.GetPages().GetPageAt(pi).GetObject().GetIndirectReference());
                PdfReference pgRef;
            if (!refOf(pg, pgRef))
                return QStringLiteral(
                    "element /S %1 has an unidentifiable /Pg")
                       .arg(tag);
            if (p.GetIndirectReference() == pgRef)
                    pgIdx = static_cast<int>(pi);
            }
            if (pgIdx < 0)
                return QStringLiteral("element /S %1 /Pg is not in the page tree")
                           .arg(tag);
            const std::set<int>& owned = pageStreams[pgIdx];
            for (int mcid : mcids) {
                bool found = false;
                for (int key : owned) {
                    const auto ptIt = parentTree.find(key);
                    if (ptIt == parentTree.end()) continue;
                    const auto mIt = ptIt->second.find(mcid);
                    if (mIt == ptIt->second.end()) continue;
                    if (!(mIt->second == f.ref))
                        return QStringLiteral(
                            "/ParentTree[%1][%2] does not point at the "
                            "element that claims it").arg(key).arg(mcid);
                    found = true;
                }
                if (!found)
                    return QStringLiteral(
                        "element /S %1 MCID %2 has no /ParentTree entry via "
                        "its page").arg(tag).arg(mcid);
            }

            // Descend (children's /P must point back at this element).
            const PdfObject* elKids = resolveObj(doc,
                                                 el.FindKey(PdfName("K")));
            Q_UNUSED(elKids);
            // P2 trees are flat (root /K only); nested /K dicts would be a
            // P3 concern. Still verify no child arrays lurk inside /K —
            // done above by the numeric check.
        }

        // ── bidirectional MCID consistency ───────────────────────────────
        // Every marked MCID in every stream must exist in its ParentTree
        // entry (the drop-entry fault breaks exactly this).
        for (const auto& [key, mcids] : streamMcids) {
            const auto ptIt = parentTree.find(key);
            if (ptIt == parentTree.end())
                return QStringLiteral(
                    "stream StructParents %1 carries marked content but has "
                    "no /ParentTree entry").arg(key);
            for (int mcid : mcids)
                if (ptIt->second.find(mcid) == ptIt->second.end())
                    return QStringLiteral(
                               "stream %1 MCID %2 is marked content without "
                               "a /ParentTree entry").arg(key).arg(mcid);
        }
    } catch (const PoDoFo::PdfError& e) {
        return QString::fromUtf8(e.what());
    } catch (const std::exception& e) {
        return QString::fromUtf8(e.what());
    }
    return {};
}

// ── the tagging transaction ──────────────────────────────────────────────────

TaggerReport tagDocumentAccessibility(const QString& path,
                                      TaggerFaultForTesting fault) {
    TaggerReport report;
    if (path.isEmpty()) {
        report.message = QStringLiteral("no document");
        return report;
    }

    try {
        // The whole transaction rides the SafeSave handle coordination +
        // commit (the AccessibilityFixes shape; restore on every outcome).
        SafeSave::ScopedFileHandleCoordination scope(path);
        Q_UNUSED(scope);

        // `doc` must be DESTROYED before the commit: its input device holds
        // the destination open, and the atomic rename is denied while any
        // handle (even our own) keeps it (the AccessibilityFixes lesson).
        QString candidate, err;
        bool saved = false;
        qint64 imagesExcluded = 0;
        QList<QList<QPair<QString, long long>>> beforeSeq;
        {
        PoDoFo::PdfMemDocument doc;
        try {
            doc.Load(path.toUtf8().constData());
        } catch (const PoDoFo::PdfError& e) {
            report.message = QString::fromUtf8(e.what());
            return report;
        } catch (const std::exception& e) {
            report.message = QString::fromUtf8(e.what());
            return report;
        }

        // §3.4: an already-tagged document is refused, never merged.
        const PdfObject* existingRoot = resolveObj(
            doc, doc.GetCatalog().GetDictionary().FindKey(
                     PdfName("StructTreeRoot")));
        if (existingRoot != nullptr && existingRoot->IsDictionary()) {
            report.message = QStringLiteral(
                "this document is already tagged; re-tagging would discard "
                "the existing structure");
            return report;
        }

        // ── the analysis (single source of truth) ───────────────────────
        WalkOutput out;
        try {
            analyzeDocument(doc, out);
        } catch (const PoDoFo::PdfError& e) {
            report.message = QStringLiteral("phase analyze: %1")
                                 .arg(QString::fromUtf8(e.what()));
            return report;
        } catch (const std::exception& e) {
            report.message = QStringLiteral("phase analyze: %1")
                                 .arg(QString::fromUtf8(e.what()));
            return report;
        }

        // Per-page tagging decisions + honest notes (§3.2).
        QList<int> taggedPages;
        for (int pi = 0; pi < out.pages.size(); ++pi) {
            const PageAnalysis& pa = out.pages[pi];
            if (pa.honestyOk && !pa.hasParseErrors && !pa.hasMarkedContent) {
                taggedPages.append(pi);
                continue;
            }
            TaggerReport::PageNote note;
            note.page = pi;
            if (!pa.honestyOk) {
                note.note = QStringLiteral(
                    "page %1 not tagged: font '%2' has no ToUnicode map — "
                    "extracted text would be wrong")
                                .arg(pi + 1)
                                .arg(pa.disclosure);
            } else if (pa.hasParseErrors) {
                note.note = QStringLiteral(
                    "page %1 not tagged: the content stream has errors that "
                    "cannot be rewritten safely")
                                .arg(pi + 1);
            } else {
                note.note = QStringLiteral(
                    "page %1 not tagged: it already carries marked content "
                    "(existing /MCID sequences)")
                                .arg(pi + 1);
            }
            report.pageNotes.append(note);
        }

        // ── lines + document-relative clusters (§3.1–§3.3) ──────────────
        QList<double> runSizes;
        for (const RunRec& r : out.runs) runSizes.append(r.size);
        double modalSize = 0;
        modalValue(runSizes, &modalSize);
        const double lineTol = std::max(1.0, 0.5 * modalSize * 1.2);

        QList<LineRec> allLines;
        for (int pi : taggedPages)
            allLines.append(buildLines(out, pi, lineTol));
        const QList<SizeCluster> clusters = clusterSizes(allLines);

        // Document-modal baseline gap (same-cluster consecutive lines).
        QList<double> gaps;
        for (int pi : taggedPages) {
            QList<LineRec> pageLines;
            for (const LineRec& l : allLines)
                if (l.pageIdx == pi) pageLines.append(l);
            // buildLines already ordered y desc; the order survives the copy.
            for (int i = 1; i < pageLines.size(); ++i) {
                if (pageLines[i - 1].clusterIdx == pageLines[i].clusterIdx)
                    gaps.append(pageLines[i - 1].y - pageLines[i].y);
            }
        }
        double modalGap = 0;
        modalValue(gaps, &modalGap);

        // ── elements per page + figures (§4) ────────────────────────────
        QList<ElementRec> elements;
        bool columnSuspectAny = false;
        for (int pi : taggedPages) {
            QList<LineRec> pageLines;
            for (const LineRec& l : allLines)
                if (l.pageIdx == pi) pageLines.append(l);
            QList<ElementRec> pageEls =
                elementsFromLines(out, pageLines, clusters, pi, modalGap);

            // Figures: DRAWN images whose XObject carries a non-empty /Alt
            // (seeded by the user through the P1 fix seam). Undescribed
            // images stay OUT of the tree — counted, never invented.
            for (int di = 0; di < out.imageDos.size(); ++di) {
                ImageDoRec& d = out.imageDos[di];
                if (d.pageIdx != pi) continue;
                const QString alt = d.imageObj != nullptr
                                        ? stringAt(d.imageObj->GetDictionary(),
                                                   "Alt")
                                              .trimmed()
                                        : QString();
                if (alt.isEmpty()) continue;   // excluded + counted, §4
                ElementRec fig;
                fig.pageIdx = pi;
                fig.tag = QStringLiteral("Figure");
                fig.y = d.y;
                fig.x = d.x;
                fig.alt = alt;
                fig.imageDoIdxs = QList<int>{di};
                pageEls.append(fig);
            }

            // Reading order within the page: y desc, then x asc.
            std::stable_sort(pageEls.begin(), pageEls.end(),
                             [](const ElementRec& a, const ElementRec& b) {
                                 if (a.y != b.y) return a.y > b.y;
                                 return a.x < b.x;
                             });
            for (int ei = 0; ei < pageEls.size(); ++ei)
                for (int di : pageEls[ei].imageDoIdxs)
                    out.imageDos[di].elementIdx = ei;

            double pageWidth = 0;
            try {
                pageWidth = doc.GetPages()
                                .GetPageAt(static_cast<unsigned>(pi))
                                .GetMediaBox()
                                .Width;
            } catch (...) {
            }
            if (columnSuspect(out, pi, pageWidth)) {
                columnSuspectAny = true;
                TaggerReport::PageNote note;
                note.page = pi;
                note.note = QStringLiteral(
                    "multi-column layout suspected — reading order in the "
                    "tree may interleave columns; review recommended");
                report.pageNotes.append(note);
            }
            elements.append(pageEls);
        }
        report.columnSuspect = columnSuspectAny;

        // Nothing taggable → honest refusal (the scanned-page case).
        if (elements.isEmpty()) {
            report.message = QStringLiteral(
                "no taggable text content — the document has no honestly "
                "decodable text runs (scanned pages need OCR, which tagging "
                "does not provide)");
            return report;
        }

        // ── group + MCID assignment, then the rewrite ───────────────────
        std::map<int, StreamGroups> groupMap;
        try {
            groupMap = assignGroups(out, elements);
        } catch (const PoDoFo::PdfError& e) {
            report.message = QStringLiteral("phase groups: %1")
                                 .arg(QString::fromUtf8(e.what()));
            return report;
        } catch (const std::exception& e) {
            report.message = QStringLiteral("phase groups: %1")
                                 .arg(QString::fromUtf8(e.what()));
            return report;
        }

        // Snapshot BEFORE any mutation (the invariant's left side).
        beforeSeq = extractionSequences(out);

        const auto streamByIdx = [&](int idx) -> const StreamRec* {
            for (const StreamRec& s : out.streams)
                if (s.index == idx) return &s;
            return nullptr;
        };

        bool perturbPending =
            fault == TaggerFaultForTesting::PerturbOneShowOperator;
        std::map<int, std::string> rewritten;
        for (const auto& sg : groupMap) {
            const StreamRec* sr = streamByIdx(sg.first);
            if (sr == nullptr) continue;
            std::string text;
            const bool perturb = perturbPending;
            if (!rewriteStream(doc, *sr, sg.second, text, perturb)) {
                report.message = QStringLiteral(
                    "the content-stream rewrite failed; the document is "
                    "unchanged");
                return report;
            }
            if (perturb && text.find("() Tj") != std::string::npos)
                perturbPending = false;
            rewritten[sg.first] = std::move(text);
        }

        // Apply the rewritten streams. Pages get a fresh single /Contents
        // stream (the concatenation of what the walk read); forms rewrite
        // their own stream in place.
        for (const auto& rw : rewritten) {
            const StreamRec* sr = streamByIdx(rw.first);
            if (sr == nullptr || sr->contentObj == nullptr) continue;
            const PoDoFo::bufferview data(rw.second.data(), rw.second.size());
            if (sr->isForm) {
                sr->contentObj->GetOrCreateStream().SetData(data);
            } else {
                PdfObject& newStream =
                    doc.GetObjects().CreateDictionaryObject();
                newStream.GetOrCreateStream().SetData(data);
                sr->contentObj->GetDictionary().AddKey(
                    PdfName("Contents"), newStream.GetIndirectReference());
            }
        }

        // ── tree assembly (raw dicts, §3.5) ─────────────────────────────
        PdfObject& rootObj = doc.GetObjects().CreateDictionaryObject();
        rootObj.GetDictionary().AddKey(PdfName("Type"),
                                       PdfObject(PdfName("StructTreeRoot")));
        PdfArray kids;
        for (ElementRec& el : elements) {
            PdfObject& elObj = doc.GetObjects().CreateDictionaryObject();
            PdfDictionary& d = elObj.GetDictionary();
            d.AddKey(PdfName("Type"), PdfObject(PdfName("StructElem")));
            d.AddKey(PdfName("S"), PdfObject(PdfName(el.tag.toStdString())));
            d.AddKey(PdfName("P"), PdfObject(rootObj.GetIndirectReference()));
            // /Pg — the PAGE the element's content renders on (its page
            // stream's owner).
            for (const StreamRec& s : out.streams) {
                if (s.pageIdx != el.pageIdx || s.isForm) continue;
                d.AddKey(PdfName("Pg"),
                         PdfObject(s.contentObj->GetIndirectReference()));
                break;
            }
            if (el.mcids.size() == 1) {
                d.AddKey(PdfName("K"),
                         PdfObject(static_cast<int64_t>(el.mcids.first())));
            } else {
                PdfArray ks;
                for (int m : el.mcids)
                    ks.Add(PdfObject(static_cast<int64_t>(m)));
                d.AddKey(PdfName("K"), PdfObject(ks));
            }
            if (!el.alt.isEmpty())
                d.AddKey(PdfName("Alt"),
                         PdfObject(PdfString(el.alt.toStdString())));
            kids.Add(PdfObject(elObj.GetIndirectReference()));
            el.ref = elObj.GetIndirectReference();
        }
        rootObj.GetDictionary().AddKey(PdfName("K"), PdfObject(kids));

        PdfObject& parentTree = doc.GetObjects().CreateDictionaryObject();
        PdfArray nums;
        for (const auto& sg : groupMap) {
            nums.Add(PdfObject(static_cast<int64_t>(sg.first)));
            // MCID → element ref for this stream.
            std::map<int, PdfReference> mcidToRef;
            int maxMcid = -1;
            for (const ElementRec& el : elements) {
                bool inStream = false;
                for (int ri : el.runIdxs)
                    if (out.runs[ri].streamIdx == sg.first) {
                        inStream = true;
                        break;
                    }
                if (!inStream)
                    for (int di : el.imageDoIdxs)
                        if (out.imageDos[di].streamIdx == sg.first) {
                            inStream = true;
                            break;
                        }
                if (!inStream) continue;
                for (int m : el.mcids) {
                    mcidToRef[m] = el.ref;
                    maxMcid = std::max(maxMcid, m);
                }
            }
            PdfArray arr;
            for (int m = 0; m <= maxMcid; ++m) {
                const auto it = mcidToRef.find(m);
                arr.Add(it != mcidToRef.end() ? PdfObject(it->second)
                                              : PdfObject());
            }
            nums.Add(PdfObject(arr));
        }
        parentTree.GetDictionary().AddKey(PdfName("Nums"), PdfObject(nums));
        parentTree.GetDictionary().AddKey(
            PdfName("ParentTreeNextKey"),
            PdfObject(static_cast<int64_t>(out.nextStreamKey)));
        rootObj.GetDictionary().AddKey(PdfName("ParentTree"),
                                       PdfObject(parentTree.GetIndirectReference()));

        // Corrupted-tree fault (the negative control): drop one /ParentTree
        // entry — the candidate MUST fail the structural walk and never
        // commit.
        if (fault == TaggerFaultForTesting::DropParentTreeEntry) {
            PdfObject* numsDirect =
                parentTree.GetDictionary().FindKey(PdfName("Nums"));
            if (numsDirect != nullptr && numsDirect->IsArray()
                && numsDirect->GetArray().GetSize() >= 2) {
                PdfObject& firstArr = numsDirect->GetArray()[1];
                if (firstArr.IsArray() && firstArr.GetArray().GetSize() > 0)
                    firstArr.GetArray().RemoveAt(
                        static_cast<unsigned>(
                            firstArr.GetArray().GetSize() - 1));
            }
        }

        auto& catalog = doc.GetCatalog().GetDictionary();
        catalog.AddKey(PdfName("StructTreeRoot"),
                       PdfObject(rootObj.GetIndirectReference()));
        {
            PdfObject* markInfoRaw = catalog.FindKey(PdfName("MarkInfo"));
            PdfObject* markInfo =
                markInfoRaw != nullptr
                    ? resolveObjMutable(doc, markInfoRaw)
                    : nullptr;
            if (markInfo == nullptr || !markInfo->IsDictionary()) {
                PdfDictionary mi;
                mi.AddKey(PdfName("Marked"), PdfObject(true));
                catalog.AddKey(PdfName("MarkInfo"), PdfObject(mi));
            } else {
                markInfo->GetDictionary().AddKey(PdfName("Marked"),
                                                 PdfObject(true));
            }
        }
        // /StructParents on every marked content stream's dict.
        for (const auto& sg : groupMap) {
            const StreamRec* sr = streamByIdx(sg.first);
            if (sr == nullptr || sr->contentObj == nullptr) continue;
            sr->contentObj->GetDictionary().AddKey(
                PdfName("StructParents"),
                PdfObject(static_cast<int64_t>(sg.first)));
        }

        // ── the SafeSave transaction ────────────────────────────────────
        // The exclusion count reads the (unmodified) /Alt state in memory.
        imagesExcluded = countUndescribedImages(doc, nullptr, 0);
        if (!SafeSave::makeUniqueCandidate(&candidate, &err)) {
            report.message = err;
            return report;
        }
        {
            // The loaded document keeps its input device until destroyed —
            // it must die BEFORE the commit (destroy-before-rename).
            try {
                doc.Save(candidate.toUtf8().constData());
                saved = true;
            } catch (...) {
                saved = false;
            }
        }
        if (!saved) {
            QFile::remove(candidate);
            report.message = QStringLiteral("candidate write failed");
            return report;
        }
        // ── the honest report ───────────────────────────────────────────
        for (const SizeCluster& c : clusters) {
            TaggerSizeCluster row;
            row.size = c.size;
            row.runCount = c.lineCount;
            row.bold = c.bold;
            row.level = c.level;
            report.sizeClusters.append(row);
        }
        report.elementsTagged = elements.size();
        for (const ElementRec& el : elements) {
            if (el.tag == QLatin1String("Figure"))
                ++report.figuresTagged;
            else if (el.tag.startsWith(QLatin1Char('H')))
                ++report.headingsTagged;
            else
                ++report.paragraphsTagged;
        }
        report.imagesExcluded = static_cast<int>(imagesExcluded);
        report.message = QStringLiteral(
            "tagged %1 element(s) — review the result: the tags come from "
            "layout heuristics, and a tagged document is not a conforming "
            "document")
                             .arg(report.elementsTagged);
        } // doc destroyed here — the destination is renameable again

        // Independent validation 1: the structural walk on the CANDIDATE.
        const QString invalid = validateTaggedStructureTree(candidate);
        if (!invalid.isEmpty()) {
            QFile::remove(candidate);
            report.message =
                QStringLiteral("candidate rejected: %1").arg(invalid);
            return report;
        }

        // Independent validation 2: the text-preservation invariant (§2.2).
        {
            PoDoFo::PdfMemDocument cand;
            bool loaded = true;
            try {
                cand.Load(candidate.toUtf8().constData());
            } catch (...) {
                loaded = false;
            }
            if (!loaded) {
                QFile::remove(candidate);
                report.message = QStringLiteral(
                    "candidate rejected: the candidate does not reload");
                return report;
            }
            WalkOutput after;
            analyzeDocument(cand, after, /*invariantMode=*/true);
            const QList<QList<QPair<QString, long long>>> afterSeq =
                extractionSequences(after);
            if (afterSeq.size() != beforeSeq.size()) {
                QFile::remove(candidate);
                report.message = QStringLiteral(
                    "text-preservation invariant failed: page count "
                    "diverged — the original is unchanged");
                return report;
            }
            bool diverged = false;
            for (int pi = 0; pi < beforeSeq.size() && !diverged; ++pi) {
                const auto& b = beforeSeq[pi];
                const auto& a = afterSeq[pi];
                if (b.size() != a.size()) {
                    diverged = true;
                    report.message = QStringLiteral(
                        "text-preservation invariant failed on page %1 — "
                        "the rewrite diverged; the original is unchanged")
                                         .arg(pi + 1);
                    break;
                }
                for (int j = 0; j < b.size(); ++j) {
                    if (b[j].first != a[j].first
                        || b[j].second != a[j].second) {
                        diverged = true;
                        report.message = QStringLiteral(
                            "text-preservation invariant failed on page %1 "
                            "— the rewrite diverged; the original is "
                            "unchanged")
                                             .arg(pi + 1);
                        break;
                    }
                }
            }
            if (diverged) {
                QFile::remove(candidate);
                return report;
            }
        }

        if (!SafeSave::commitFileToDestination(candidate, path, &err)) {
            QFile::remove(candidate);
            report.message = err;
            return report;
        }

        report.ok = true;

        return report;
    } catch (const PoDoFo::PdfError& e) {
        report.message = QString::fromUtf8(e.what());
        return report;
    } catch (const std::exception& e) {
        report.message = QString::fromUtf8(e.what());
        return report;
    }
}

} // namespace gp
