// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QByteArray>
#include <QList>

// Byte-exact surgery on a page content stream for one image placement
// ("/<name> Do"): restacking it (bring to front / send to back) and wrapping it
// in its own ExtGState for opacity. Everything outside the edited bytes is
// kept verbatim — the stream is never re-serialized (re-serializing operands
// is how E-1 corrupted glyph strings). A lexer tracks q/Q nesting with byte
// offsets, so an edit either provably keeps the image's graphics state or is
// refused with a reason.
namespace gp::content {

struct Token {
    enum class Kind { Operator, Name, Number, String, ArrayOpen, ArrayClose,
                      DictOpen, DictClose, InlineImage, Other };
    Kind kind = Kind::Other;
    qsizetype start = 0;   // first byte
    qsizetype end = 0;     // one past the last byte
    QByteArray text;       // operator keyword, or the decoded name (no '/')
    int depth = 0;         // q-nesting depth before this token
};

// Lexes `content`. Returns false on malformed input — an unterminated string,
// an inline image without EI, a Q with no open q, or a q left open at the end
// (`tokens` then holds the tokens read so far).
bool lex(const QByteArray &content, QList<Token> *tokens);

enum class EditResult {
    Changed,            // `out` holds the edited stream
    Unchanged,          // nothing to do (already in place / already wrapped)
    NotFound,           // no "/<name> Do" in the stream
    Malformed,          // unparseable, or q/Q unbalanced
    NotIsolated,        // the Do is not inside its own q..Q block
    SharedBlock,        // its q..Q block paints other content too
    StateInTheWay,      // a cm/gs/clip between the old and new position
};

// Moves the image's q..Q block to the end (front) or start (back) of its
// parent block — the whole stream when the block is top level. Refuses
// (without touching `out`) whenever the move could change what the image
// looks like: a Do outside any q..Q, a block that also paints other content,
// or a non-identity cm / gs / clip at the parent level between the two
// positions. `colorSensitive` (a stencil /ImageMask, which paints with the
// current fill colour) also refuses a fill-colour change in between.
EditResult restackImage(const QByteArray &content, const QByteArray &xobjectName,
                        bool toFront, QByteArray *out, bool colorSensitive = false);

// Wraps the image's first Do in "q /<gsName> gs … Q". Unchanged when that
// exact wrap is already present (the caller then updates the ExtGState's
// values in place instead of nesting another wrap).
EditResult wrapImageInExtGState(const QByteArray &content, const QByteArray &xobjectName,
                                const QByteArray &gsName, QByteArray *out);

// Replaces the six operands of the image's placement matrix — the last "cm"
// inside the image's own q..Q block, before its first Do — with `matrix`
// (the six numbers, already formatted). Refuses when the image has no block
// of its own or the block sets no cm: rewriting an outer cm would move other
// content too.
EditResult replaceImageMatrix(const QByteArray &content, const QByteArray &xobjectName,
                              const QByteArray &matrix, QByteArray *out);

} // namespace gp::content
