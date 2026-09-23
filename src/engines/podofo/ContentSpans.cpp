// SPDX-License-Identifier: Apache-2.0
#include "engines/podofo/ContentSpans.h"
#include <QSet>
#include <QVector>

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

// Inline image data after "ID": one whitespace byte, then binary data up to
// an "EI" with whitespace before it and whitespace/delimiter/end after it.
// Returns the offset one past "EI", or -1.
qsizetype skipInlineImageData(const QByteArray &s, qsizetype i)
{
    if (i < s.size() && isWhite(s[i])) ++i;
    for (qsizetype j = i; j + 1 < s.size(); ++j) {
        if (s[j] == 'E' && s[j + 1] == 'I' && j > 0 && isWhite(s[j - 1])
            && (j + 2 >= s.size() || isWhite(s[j + 2]) || isDelimiter(s[j + 2])))
            return j + 2;
    }
    return -1;
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

int findImageDo(const QList<Token> &toks, const QByteArray &name)
{
    for (int i = 1; i < toks.size(); ++i) {
        if (toks[i].kind == Token::Kind::Operator && toks[i].text == "Do"
            && toks[i - 1].kind == Token::Kind::Name && toks[i - 1].text == name)
            return i;
    }
    return -1;
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
                const qsizetype after = skipInlineImageData(s, i);
                if (after < 0) return false;          // inline image without EI
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
                        QByteArray *out, bool colorSensitive)
{
    QList<Token> toks;
    if (!lex(s, &toks)) return EditResult::Malformed;

    // match[i]: for a q token, the index of its Q (lex() guaranteed balance).
    QVector<int> match(toks.size(), -1);
    {
        QVector<int> open;
        for (int i = 0; i < toks.size(); ++i) {
            if (toks[i].kind != Token::Kind::Operator) continue;
            if (toks[i].text == "q") open.push_back(i);
            else if (toks[i].text == "Q") match[open.takeLast()] = i;
        }
    }

    const int doIdx = findImageDo(toks, name);
    if (doIdx < 0) return EditResult::NotFound;

    // The innermost q..Q around the Do (the image's own block) and the block
    // around that (its parent; none = the whole stream).
    int blockOpen = -1, parentOpen = -1;
    {
        QVector<int> open;
        for (int i = 0; i < doIdx; ++i) {
            if (toks[i].kind != Token::Kind::Operator) continue;
            if (toks[i].text == "q") open.push_back(i);
            else if (toks[i].text == "Q") open.pop_back();
        }
        if (!open.isEmpty()) blockOpen = open.back();
        if (open.size() >= 2) parentOpen = open[open.size() - 2];
    }
    if (blockOpen < 0) return EditResult::NotIsolated;
    const int blockClose = match[blockOpen];
    for (int i = blockOpen + 1; i < blockClose; ++i) {
        if (i != doIdx && isPainting(toks[i])) return EditResult::SharedBlock;
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
                                const QByteArray &gsName, QByteArray *out)
{
    QList<Token> toks;
    if (!lex(s, &toks)) return EditResult::Malformed;
    const int doIdx = findImageDo(toks, name);
    if (doIdx < 0) return EditResult::NotFound;

    // Our own earlier wrap: q /<gsName> gs /<name> Do Q
    const auto op = [&](int i, const char *kw) {
        return i >= 0 && i < toks.size() && toks[i].kind == Token::Kind::Operator
            && toks[i].text == kw;
    };
    if (op(doIdx - 4, "q") && toks[doIdx - 3].kind == Token::Kind::Name
        && toks[doIdx - 3].text == gsName && op(doIdx - 2, "gs") && op(doIdx + 1, "Q"))
        return EditResult::Unchanged;

    QByteArray result = s;
    result.insert(toks[doIdx].end, "\nQ\n");
    result.insert(toks[doIdx - 1].start, "q\n/" + gsName + " gs\n");
    *out = result;
    return EditResult::Changed;
}

EditResult replaceImageMatrix(const QByteArray &s, const QByteArray &name,
                              const QByteArray &matrix, QByteArray *out)
{
    QList<Token> toks;
    if (!lex(s, &toks)) return EditResult::Malformed;
    const int doIdx = findImageDo(toks, name);
    if (doIdx < 0) return EditResult::NotFound;

    int blockOpen = -1;
    {
        QVector<int> open;
        for (int i = 0; i < doIdx; ++i) {
            if (toks[i].kind != Token::Kind::Operator) continue;
            if (toks[i].text == "q") open.push_back(i);
            else if (toks[i].text == "Q") open.pop_back();
        }
        if (open.isEmpty()) return EditResult::NotIsolated;
        blockOpen = open.back();
    }
    // The last cm between the block's q and the Do, at the Do's own depth
    // (a cm inside a nested, already-closed q..Q does not apply to the Do).
    for (int i = doIdx - 1; i > blockOpen; --i) {
        const Token &t = toks[i];
        if (t.depth != toks[doIdx].depth || t.kind != Token::Kind::Operator || t.text != "cm")
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

} // namespace gp::content
