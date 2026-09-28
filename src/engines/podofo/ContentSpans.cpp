// SPDX-License-Identifier: Apache-2.0
#include "engines/podofo/ContentSpans.h"
#include <QHash>
#include <QSet>
#include <QVector>
#include <cmath>

namespace gp::content {
namespace {

bool isWhite(char c)
{
    return c == '\0' || c == '\t' || c == '\n' || c == '\f' || c == '\r' || c == ' ';
}

bool isDelimiter(char c)
{
    return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']'
        || c == '{' || c == '}' || c == '/' || c == '%';
}

bool isRegular(char c) { return !isWhite(c) && !isDelimiter(c); }

int hexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// "/A#20B" → "A B" (PDF 1.2+ name escapes).
QByteArray decodeName(const QByteArray &raw)
{
    QByteArray out;
    out.reserve(raw.size());
    for (qsizetype i = 0; i < raw.size(); ++i) {
        if (raw[i] == '#' && i + 2 < raw.size()) {
            const int hi = hexValue(raw[i + 1]);
            const int lo = hexValue(raw[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.append(char(hi * 16 + lo));
                i += 2;
                continue;
            }
        }
        out.append(raw[i]);
    }
    return out;
}

bool looksNumeric(const QByteArray &w)
{
    qsizetype i = 0;
    if (i < w.size() && (w[i] == '+' || w[i] == '-')) ++i;
    bool digits = false, dot = false;
    for (; i < w.size(); ++i) {
        if (w[i] >= '0' && w[i] <= '9') digits = true;
        else if (w[i] == '.' && !dot) dot = true;
        else return false;
    }
    return digits;
}

// ── inline image data extent (CX-08) ────────────────────────────────────────
// The exact end of the data after "ID". The old rule — the first
// whitespace-delimited "EI" — ended the data at a false EI inside raw pixel
// bytes, and the operator-looking bytes after it were lexed and EDITED as if
// they were real (an opacity edit could wrap a "Do" that only existed inside
// an image's bytes). The extent is now taken, in order:
//   1. /L (or /Length) when present — the data is exactly that many bytes;
//   2. the EOD marker of the outermost filter — '>' for ASCIIHexDecode,
//      '~>' for ASCII85Decode;
//   3. for unfiltered data, the exact byte count W×H×components×BPC
//      (rows packed, so ceil(W*comps*BPC/8) per row);
// and anything else — a binary filter without /L, an unknown colour space,
// missing dimensions — refuses the edit (-1): never guess. A zero-length
// data section ("ID EI") is accepted as the one unambiguous short read: no
// fake operator can hide in zero bytes.

// "EI" (the end-of-image marker) at `at`, with whitespace/delimiter/end
// after it. Returns the offset one past it, or -1.
qsizetype requireEi(const QByteArray &s, qsizetype at)
{
    while (at < s.size() && isWhite(s[at])) ++at;
    if (at + 1 < s.size() && s[at] == 'E' && s[at + 1] == 'I'
        && (at + 2 >= s.size() || isWhite(s[at + 2]) || isDelimiter(s[at + 2])))
        return at + 2;
    return -1;
}

// The inline image dict between the BI at `biIdx` and the ID at `idIdx`,
// keyed by decoded key name; array elements keep their ArrayOpen/Close
// tokens so a /F [/AHx /Fl] stays one operand list. A bare Name right after
// a key is its VALUE ("/F /Fl"), not the next key; Names inside << >> (a
// /DP dictionary) belong to that dict, not to this one.
QHash<QByteArray, QList<Token>> inlineImageDict(const QList<Token> &toks, int biIdx, int idIdx)
{
    QHash<QByteArray, QList<Token>> dict;
    QByteArray key;
    int arrayDepth = 0, dictDepth = 0;
    for (int i = biIdx + 1; i < idIdx; ++i) {
        const Token &t = toks[i];
        if (t.kind == Token::Kind::ArrayOpen) {
            ++arrayDepth;
        } else if (t.kind == Token::Kind::ArrayClose) {
            --arrayDepth;
            if (arrayDepth < 0) return {};          // malformed dict
        } else if (t.kind == Token::Kind::DictOpen) {
            ++dictDepth;
        } else if (t.kind == Token::Kind::DictClose) {
            --dictDepth;
            if (dictDepth < 0) return {};
        } else if (arrayDepth == 0 && dictDepth == 0 && t.kind == Token::Kind::Name) {
            if (!key.isEmpty() && dict[key].isEmpty())
                dict[key].append(t);                // the pending key's value
            else
                key = t.text;                       // a new key
            continue;
        }
        if (!key.isEmpty()) dict[key].append(t);
    }
    if (arrayDepth != 0 || dictDepth != 0) return {};
    return dict;
}

// -1: the key is present but its first operand is not a readable number;
//  0: absent; 1: read into *out.
int numberOperand(const QHash<QByteArray, QList<Token>> &dict, const QByteArray &s,
                  const char *abbrev, const char *longName, double *out)
{
    QList<Token> v = dict.value(QByteArray(abbrev));
    if (v.isEmpty()) v = dict.value(QByteArray(longName));
    if (v.isEmpty()) return 0;
    if (v.first().kind != Token::Kind::Number) return -1;
    bool ok = false;
    const double d = s.mid(v.first().start, v.first().end - v.first().start).toDouble(&ok);
    if (!ok) return -1;
    *out = d;
    return 1;
}

// The filter names of /F (or /Filter): a Name, or an Array of Names, in
// stream order (the first is the outermost decoder). *present is true when
// the key is there at all; an unrecognized shape yields an empty list.
QList<QByteArray> filterNames(const QHash<QByteArray, QList<Token>> &dict, bool *present)
{
    *present = false;
    QList<QByteArray> out;
    QList<Token> v = dict.value(QByteArray("F"));
    if (v.isEmpty()) v = dict.value(QByteArray("Filter"));
    if (v.isEmpty()) return out;
    *present = true;
    if (v.first().kind == Token::Kind::Name) {
        out.append(v.first().text);
    } else if (v.first().kind == Token::Kind::ArrayOpen) {
        for (const Token &t : v)
            if (t.kind == Token::Kind::Name) out.append(t.text);
    }
    return out;
}

// Components per sample for the /CS colour-space name (abbreviations first,
// per ISO 32000-1 §8.9.6.4). 0: unknown — the caller refuses.
int componentsFor(const QByteArray &cs)
{
    if (cs == "G" || cs == "Gray" || cs == "DeviceGray" || cs == "CalGray"
        || cs == "I" || cs == "Indexed")
        return 1;
    if (cs == "RGB" || cs == "DeviceRGB" || cs == "CalRGB" || cs == "Lab")
        return 3;
    if (cs == "CMYK" || cs == "DeviceCMYK" || cs == "CalCMYK")
        return 4;
    return 0;
}

qsizetype inlineImageDataEnd(const QByteArray &s, qsizetype afterId,
                             const QList<Token> &toks, int idIdx)
{
    // The BI that opened this inline image: walking back from the ID, the
    // first operator must be it (a legal inline dict holds no operators).
    int biIdx = -1;
    for (int i = idIdx - 1; i >= 0; --i) {
        if (toks[i].kind != Token::Kind::Operator) continue;
        if (toks[i].text == "BI") biIdx = i;
        break;
    }
    if (biIdx < 0) {
        // Not a real inline image: an "ID" without a BI is undefined PDF, and
        // the historical first-EI read stays for it (the CX-15 sanitizer gate
        // pins that verdict; the exact extent governs real inline images,
        // which always carry BI).
        qsizetype dataStart = afterId;
        if (dataStart < s.size() && isWhite(s[dataStart])) ++dataStart;
        for (qsizetype j = dataStart; j + 1 < s.size(); ++j) {
            if (s[j] == 'E' && s[j + 1] == 'I' && isWhite(s[j - 1])
                && (j + 2 >= s.size() || isWhite(s[j + 2]) || isDelimiter(s[j + 2])))
                return j + 2;
        }
        return -1;
    }
    const auto dict = inlineImageDict(toks, biIdx, idIdx);

    // The single whitespace byte after the ID, then the data.
    qsizetype dataStart = afterId;
    if (dataStart < s.size() && isWhite(s[dataStart])) ++dataStart;

    // 1. /L (or /Length): the data is exactly that many bytes.
    double lval = 0;
    const int lOk = numberOperand(dict, s, "L", "Length", &lval);
    if (lOk == -1) return -1;
    if (lOk == 1) {
        if (lval < 0 || lval != std::floor(lval) || lval > s.size()) return -1;
        return requireEi(s, dataStart + qsizetype(lval));
    }

    // 2. The EOD marker of the outermost filter.
    bool hasFilter = false;
    const QList<QByteArray> filters = filterNames(dict, &hasFilter);
    if (hasFilter) {
        if (filters.isEmpty()) return -1;
        const QByteArray outer = filters.first();
        const bool ahx = outer == "AHx" || outer == "ASCIIHexDecode";
        const bool a85 = outer == "A85" || outer == "ASCII85Decode";
        const qsizetype eodLen = a85 ? 2 : 1;
        if (!ahx && !a85) return -1;               // binary filter without /L
        const QByteArray eod = a85 ? QByteArray("~>") : QByteArray(">");
        for (qsizetype j = dataStart; j + eodLen <= s.size(); ++j) {
            if (s.mid(j, eodLen) == eod) return requireEi(s, j + eodLen);
        }
        return -1;                                 // no EOD before the stream ends
    }

    // 3. Unfiltered: the exact byte count. W and H are required; the
    //    colour space defaults to gray; /IM true is one 1-bit component.
    double w = 0, h = 0, bpc = 8;
    if (numberOperand(dict, s, "W", "Width", &w) != 1) return -1;
    if (numberOperand(dict, s, "H", "Height", &h) != 1) return -1;
    const int bpcOk = numberOperand(dict, s, "BPC", "BitsPerComponent", &bpc);
    if (bpcOk == -1) return -1;

    bool imageMask = false;
    QList<Token> im = dict.value(QByteArray("IM"));
    if (im.isEmpty()) im = dict.value(QByteArray("ImageMask"));
    if (!im.isEmpty())
        imageMask = im.first().kind == Token::Kind::Other
            && s.mid(im.first().start, im.first().end - im.first().start) == "true";

    int comps = 1;
    QList<Token> cs = dict.value(QByteArray("CS"));
    if (cs.isEmpty()) cs = dict.value(QByteArray("ColorSpace"));
    if (!cs.isEmpty()) {
        if (cs.first().kind != Token::Kind::Name) return -1;   // e.g. [/ICCBased ...]
        comps = componentsFor(cs.first().text);
        if (comps == 0) return -1;
    }
    if (imageMask) comps = 1;
    if (bpcOk == 0 && imageMask) bpc = 1;   // an image mask defaults to 1 bit

    if (w <= 0 || h <= 0 || w > 1e9 || h > 1e9) return -1;
    if (bpc != 1 && bpc != 2 && bpc != 4 && bpc != 8 && bpc != 16) return -1;
    const qint64 bits = qint64(w) * comps * qint64(bpc);
    const qint64 bytes = ((bits + 7) / 8) * qint64(h);
    if (bytes < 0 || bytes > s.size()) return -1;

    const qsizetype exact = requireEi(s, dataStart + qsizetype(bytes));
    if (exact >= 0) return exact;
    // The declared extent does not land at an EI. If even a zero-length data
    // section would end the image here, take that unambiguous short read;
    // anything else is refused rather than guessed.
    return requireEi(s, dataStart);
}

bool isPainting(const Token &t)
{
    static const QSet<QByteArray> ops = {
        "Tj", "TJ", "'", "\"",                               // text
        "f", "F", "f*", "B", "B*", "b", "b*", "S", "s",      // paths
        "sh", "Do",                                          // shadings, XObjects
    };
    return t.kind == Token::Kind::InlineImage
        || (t.kind == Token::Kind::Operator && ops.contains(t.text));
}

// Would this operator, at the parent level, change how the moved image draws?
// cm (unless identity), gs and clipping always; the fill colour only for a
// stencil mask, which paints with it.
bool changesImageState(const QList<Token> &toks, int idx, const QByteArray &src,
                       bool colorSensitive)
{
    const Token &t = toks[idx];
    if (t.kind != Token::Kind::Operator) return false;
    if (t.text == "gs" || t.text == "W" || t.text == "W*") return true;
    if (colorSensitive) {
        static const QSet<QByteArray> fill = { "g", "rg", "k", "sc", "scn", "cs" };
        if (fill.contains(t.text)) return true;
    }
    if (t.text != "cm") return false;
    if (idx < 6) return true;
    static const double identity[6] = { 1, 0, 0, 1, 0, 0 };
    for (int k = 0; k < 6; ++k) {
        const Token &a = toks[idx - 6 + k];
        if (a.kind != Token::Kind::Number) return true;
        bool ok = false;
        const double v = src.mid(a.start, a.end - a.start).toDouble(&ok);
        if (!ok || v != identity[k]) return true;
    }
    return false;
}

// The `occurrence`-th (0-based, stream order) "/<name> Do" placement. N1:
// this is the ONLY Do selector — a name-only lookup ("first Do wins") made
// every edit of a reused XObject land on its first placement.
int findImageDoNth(const QList<Token> &toks, const QByteArray &name, int occurrence)
{
    if (occurrence < 0) occurrence = 0;
    int seen = 0;
    for (int i = 1; i < toks.size(); ++i) {
        if (toks[i].kind == Token::Kind::Operator && toks[i].text == "Do"
            && toks[i - 1].kind == Token::Kind::Name && toks[i - 1].text == name) {
            if (seen == occurrence) return i;
            ++seen;
        }
    }
    return -1;
}

// match[i]: for the q token at i, the index of its Q (lex() guaranteed
// balance); unmatched entries stay -1.
QVector<int> qMatch(const QList<Token> &toks)
{
    QVector<int> match(toks.size(), -1);
    QVector<int> open;
    for (int i = 0; i < toks.size(); ++i) {
        if (toks[i].kind != Token::Kind::Operator) continue;
        if (toks[i].text == "q") open.push_back(i);
        else if (toks[i].text == "Q" && !open.isEmpty()) match[open.takeLast()] = i;
    }
    return match;
}

// The innermost q..Q block containing the token at `at` (-1 when the token
// sits outside every block). `match` comes from qMatch().
int innermostBlockOpen(const QList<Token> &toks, int at)
{
    QVector<int> open;
    for (int i = 0; i < at; ++i) {
        if (toks[i].kind != Token::Kind::Operator) continue;
        if (toks[i].text == "q") open.push_back(i);
        else if (toks[i].text == "Q" && !open.isEmpty()) open.pop_back();
    }
    return open.isEmpty() ? -1 : open.back();
}

// The q..Q blocks around the token at `at`, outermost first (the last entry
// is the innermost block).
QVector<int> enclosingOpens(const QList<Token> &toks, int at)
{
    QVector<int> open;
    for (int i = 0; i < at; ++i) {
        if (toks[i].kind != Token::Kind::Operator) continue;
        if (toks[i].text == "q") open.push_back(i);
        else if (toks[i].text == "Q" && !open.isEmpty()) open.pop_back();
    }
    return open;
}

// CX-10: the image's OWN block. After wrapImageInExtGState the Do sits in a
// gs-only wrapper ("q /GSop… gs /ImA Do Q") nested inside the placement
// block ("q cm … Q"). A level counts as a wrapper — and is stepped out of —
// only when it contains a gs operator and nothing else but the Do: a plain
// "q /ImA Do Q" inside a placement block keeps the innermost-block semantics
// (restacking within the parent must not escape it). The innermost
// non-wrapper block wins; when every enclosing level is a wrapper, their
// outermost is the image's own block. Returns the block's open index and its
// parent block's open index (-1 when the block is at stream level).
struct OwnBlock { int open; int parentOpen; };
OwnBlock ownBlockOf(const QList<Token> &toks, int doIdx, const QVector<int> &match,
                    const QVector<int> &enclosing)
{
    OwnBlock outermost{-1, -1};
    for (int k = int(enclosing.size()) - 1; k >= 0; --k) {
        const int open = enclosing[k];
        const int close = match[open];
        const int parentOpen = k > 0 ? enclosing[k - 1] : -1;
        if (close < 0) return { open, parentOpen };
        bool hasGs = false, onlyGs = true;
        for (int i = open + 1; i < close; ++i) {
            if (i == doIdx) continue;
            if (toks[i].kind != Token::Kind::Operator) continue;
            if (toks[i].text == "gs") { hasGs = true; continue; }
            onlyGs = false;
            break;
        }
        if (!hasGs || !onlyGs)
            return { open, parentOpen };
        outermost = { open, parentOpen };
    }
    return outermost;
}

} // namespace

bool lex(const QByteArray &s, QList<Token> *tokens)
{
    tokens->clear();
    int depth = 0;
    const qsizetype n = s.size();
    qsizetype i = 0;
    while (i < n) {
        const char c = s[i];
        if (isWhite(c)) { ++i; continue; }
        if (c == '%') {
            while (i < n && s[i] != '\n' && s[i] != '\r') ++i;
            continue;
        }
        Token t;
        t.start = i;
        t.depth = depth;
        if (c == '(') {
            int nest = 0;
            for (; i < n; ++i) {
                if (s[i] == '\\') { ++i; continue; }
                if (s[i] == '(') ++nest;
                else if (s[i] == ')' && --nest == 0) { ++i; break; }
            }
            if (nest != 0) return false;             // unterminated string
            t.kind = Token::Kind::String;
        } else if (c == '<') {
            if (i + 1 < n && s[i + 1] == '<') {
                t.kind = Token::Kind::DictOpen;
                i += 2;
            } else {
                const qsizetype close = s.indexOf('>', i + 1);
                if (close < 0) return false;          // unterminated hex string
                t.kind = Token::Kind::String;
                i = close + 1;
            }
        } else if (c == '>') {
            if (i + 1 >= n || s[i + 1] != '>') return false;
            t.kind = Token::Kind::DictClose;
            i += 2;
        } else if (c == '[' || c == ']') {
            t.kind = c == '[' ? Token::Kind::ArrayOpen : Token::Kind::ArrayClose;
            ++i;
        } else if (c == '{' || c == '}') {
            t.kind = Token::Kind::Other;
            ++i;
        } else if (c == ')') {
            return false;                             // stray ')'
        } else if (c == '/') {
            qsizetype j = i + 1;
            while (j < n && isRegular(s[j])) ++j;
            t.kind = Token::Kind::Name;
            t.text = decodeName(s.mid(i + 1, j - i - 1));
            i = j;
        } else {
            qsizetype j = i;
            while (j < n && isRegular(s[j])) ++j;
            const QByteArray word = s.mid(i, j - i);
            i = j;
            if (looksNumeric(word)) {
                t.kind = Token::Kind::Number;
            } else if (word == "true" || word == "false" || word == "null") {
                t.kind = Token::Kind::Other;
            } else if (word == "ID") {
                const qsizetype after = inlineImageDataEnd(s, i, *tokens, tokens->size());
                if (after < 0) return false;      // no EI, or the extent cannot be known
                t.kind = Token::Kind::InlineImage;
                t.text = word;
                i = after;
            } else {
                t.kind = Token::Kind::Operator;
                t.text = word;
                if (word == "q") {
                    ++depth;
                } else if (word == "Q") {
                    if (depth == 0) return false;     // Q with no open q
                    --depth;
                }
            }
        }
        t.end = i;
        tokens->append(t);
    }
    return depth == 0;                                // a q left open is malformed too
}

EditResult restackImage(const QByteArray &s, const QByteArray &name, bool toFront,
                        QByteArray *out, bool colorSensitive, int occurrence)
{
    QList<Token> toks;
    if (!lex(s, &toks)) return EditResult::Malformed;

    const int doIdx = findImageDoNth(toks, name, occurrence);
    if (doIdx < 0) return EditResult::NotFound;

    // The image's own block (through any gs-only wrapper — CX-10) and the
    // block around that (its parent; none = the whole stream).
    const QVector<int> match = qMatch(toks);
    const QVector<int> enclosing = enclosingOpens(toks, doIdx);
    const OwnBlock own = ownBlockOf(toks, doIdx, match, enclosing);
    const int blockOpen = own.open, parentOpen = own.parentOpen;
    if (blockOpen < 0) return EditResult::NotIsolated;
    const int blockClose = match[blockOpen];
    for (int i = blockOpen + 1; i < blockClose; ++i) {
        if (i != doIdx && isPainting(toks[i])) return EditResult::SharedBlock;
    }

    // CX-09: the span's own marked content must be balanced — a BDC/BMC
    // opened inside the block must close inside it, and no EMC inside may
    // close a region opened before it. Moving an unbalanced span tears a
    // hidden-layer or tagged owner apart.
    {
        int marked = 0;
        bool balanced = true;
        for (int i = blockOpen + 1; i < blockClose && balanced; ++i) {
            if (toks[i].kind != Token::Kind::Operator) continue;
            if (toks[i].text == "BDC" || toks[i].text == "BMC") {
                ++marked;
            } else if (toks[i].text == "EMC") {
                if (marked == 0) balanced = false;
                else --marked;
            }
        }
        if (!balanced || marked != 0) return EditResult::StateInTheWay;
    }

    const int level = toks[blockOpen].depth;
    const int parentClose = parentOpen < 0 ? -1 : match[parentOpen];
    const int first = toFront ? blockClose + 1 : (parentOpen < 0 ? 0 : parentOpen + 1);
    const int last = toFront ? (parentOpen < 0 ? int(toks.size()) - 1 : parentClose - 1)
                             : blockOpen - 1;
    bool paintsBetween = false;
    for (int i = first; i <= last; ++i) {
        if (toks[i].depth == level && changesImageState(toks, i, s, colorSensitive))
            return EditResult::StateInTheWay;
        if (isPainting(toks[i])) paintsBetween = true;
        // CX-09: a marked-content operator between the span and its
        // destination means the move would repaint a hidden optional-content
        // layer or drag the placement out of its tagged owner.
        if (toks[i].kind == Token::Kind::Operator
            && (toks[i].text == "BDC" || toks[i].text == "BMC"
                || toks[i].text == "EMC"))
            return EditResult::StateInTheWay;
    }
    if (!paintsBetween) return EditResult::Unchanged;    // already front-/backmost

    const qsizetype spanStart = toks[blockOpen].start;
    const qsizetype spanEnd = toks[blockClose].end;
    const QByteArray span = '\n' + s.mid(spanStart, spanEnd - spanStart) + '\n';
    QByteArray result = s;
    if (toFront) {
        const qsizetype at = parentOpen < 0 ? s.size() : toks[parentClose].start;
        result.insert(at, span);                         // after the span: offsets hold
        result.remove(spanStart, spanEnd - spanStart);
    } else {
        const qsizetype at = parentOpen < 0 ? 0 : toks[parentOpen].end;
        result.remove(spanStart, spanEnd - spanStart);   // after `at`: offsets hold
        result.insert(at, span);
    }
    *out = result;
    return EditResult::Changed;
}

EditResult wrapImageInExtGState(const QByteArray &s, const QByteArray &name,
                                const QByteArray &gsName, QByteArray *out, int occurrence)
{
    QList<Token> toks;
    if (!lex(s, &toks)) return EditResult::Malformed;
    const int doIdx = findImageDoNth(toks, name, occurrence);
    if (doIdx < 0) return EditResult::NotFound;

    // Our own earlier wrap: q /<gsName> gs /<name> Do Q — looked up around
    // THIS placement's Do only (N1): another placement's wrap never masks it.
    const auto op = [&](int i, const char *kw) {
        return i >= 0 && i < toks.size() && toks[i].kind == Token::Kind::Operator
            && toks[i].text == kw;
    };
    if (op(doIdx - 4, "q") && toks[doIdx - 3].kind == Token::Kind::Name
        && toks[doIdx - 3].text == gsName && op(doIdx - 2, "gs") && op(doIdx + 1, "Q"))
        return EditResult::Unchanged;

    QByteArray result = s;
    result.insert(toks[doIdx].end, "\nQ\n");
    // The leading newline is load-bearing: a legal stream may put the name
    // directly after a regular character ("5 5 cm/ImA Do Q" — '/' is a
    // delimiter, so no whitespace is required). Without it the inserted "q"
    // glues onto that character ("cmq" + "q" lexes as one foreign operator
    // "cmqq"), the wrapper never opens, and the inserted Q breaks the q/Q
    // balance — the wrapped output was corrupt and unlexable (found by the
    // CX-15 sanitizer-gate mutation sweep; evidence in
    // docs/audit/evidence-cx15/).
    result.insert(toks[doIdx - 1].start, "\nq\n/" + gsName + " gs\n");
    *out = result;
    return EditResult::Changed;
}

EditResult replaceImageMatrix(const QByteArray &s, const QByteArray &name,
                              const QByteArray &matrix, QByteArray *out, int occurrence)
{
    QList<Token> toks;
    if (!lex(s, &toks)) return EditResult::Malformed;
    const int doIdx = findImageDoNth(toks, name, occurrence);
    if (doIdx < 0) return EditResult::NotFound;

    const QVector<int> match = qMatch(toks);
    const QVector<int> enclosing = enclosingOpens(toks, doIdx);
    const OwnBlock own = ownBlockOf(toks, doIdx, match, enclosing);
    const int blockOpen = own.open;
    if (blockOpen < 0) return EditResult::NotIsolated;

    // CX-11: restack's isolation rule — the block must paint nothing but
    // this placement. The cm inside a shared block carries the neighbours'
    // placement too (q 100 0 0 100 20 20 cm /ImA Do /ImB Do Q moved BOTH
    // images), so rewriting it is refused, never applied.
    const int blockClose = match[blockOpen];
    if (blockClose < 0) return EditResult::Malformed;
    for (int i = blockOpen + 1; i < blockClose; ++i)
        if (i != doIdx && isPainting(toks[i])) return EditResult::SharedBlock;

    // The last cm between the block's q and the Do, at the block's own
    // nesting level (direct children of the q): a cm inside a nested,
    // already-closed q..Q does not apply to the Do, and through a gs-only
    // wrapper (CX-10) the owning cm sits one level above the Do.
    for (int i = doIdx - 1; i > blockOpen; --i) {
        const Token &t = toks[i];
        if (t.depth != toks[blockOpen].depth + 1
            || t.kind != Token::Kind::Operator || t.text != "cm")
            continue;
        if (i - 6 <= blockOpen) return EditResult::Malformed;
        for (int k = i - 6; k < i; ++k)
            if (toks[k].kind != Token::Kind::Number) return EditResult::Malformed;
        QByteArray result = s;
        result.replace(toks[i - 6].start, toks[i].end - toks[i - 6].start, matrix + " cm");
        *out = result;
        return EditResult::Changed;
    }
    return EditResult::NotIsolated;
}

EditResult removeImagePlacement(const QByteArray &s, const QByteArray &name,
                                int occurrence, QByteArray *out)
{
    QList<Token> toks;
    if (!lex(s, &toks)) return EditResult::Malformed;

    const int doIdx = findImageDoNth(toks, name, occurrence);
    if (doIdx < 0) return EditResult::NotFound;

    // The image's own block (through any gs-only wrapper — CX-10, so an
    // opacity-wrapped image's delete takes the wrapper with it). A placement
    // outside every block is refused — erasing the bare "name Do" would leave
    // its cm/gs/clip in force for whatever content follows.
    const QVector<int> match = qMatch(toks);
    const QVector<int> enclosing = enclosingOpens(toks, doIdx);
    const OwnBlock own = ownBlockOf(toks, doIdx, match, enclosing);
    const int blockOpen = own.open;
    if (blockOpen < 0) return EditResult::NotIsolated;

    const int blockClose = match[blockOpen];
    // restack's isolation rule: the block must paint nothing but this
    // placement, or the removal would take the neighbours with it.
    for (int i = blockOpen + 1; i < blockClose; ++i)
        if (i != doIdx && isPainting(toks[i])) return EditResult::SharedBlock;

    // A marked-content region opened inside the block must also close inside
    // it: removal may not orphan a BDC/BMC or an EMC any more than restacking
    // may cross one (CX-09's ownership rule, which deletion honours too).
    int markedDepth = 0;
    for (int i = blockOpen + 1; i < blockClose; ++i) {
        if (toks[i].kind != Token::Kind::Operator) continue;
        if (toks[i].text == "BDC" || toks[i].text == "BMC") {
            ++markedDepth;
        } else if (toks[i].text == "EMC") {
            if (markedDepth == 0) return EditResult::StateInTheWay;
            --markedDepth;
        }
    }
    if (markedDepth != 0) return EditResult::StateInTheWay;

    QByteArray result = s;
    result.remove(toks[blockOpen].start, toks[blockClose].end - toks[blockOpen].start);
    *out = result;
    return EditResult::Changed;
}

} // namespace gp::content
