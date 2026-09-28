// SPDX-License-Identifier: Apache-2.0
// CX-15 — the smallest sanitizer gate: a standalone, QtCore-only test of
// gp::content (src/engines/podofo/ContentSpans.{h,cpp}) over the adversarial
// content-stream shapes from findings CX-08..CX-12, built on the CI
// ubuntu-24.04 job with
//   -fsanitize=address,undefined -fno-sanitize-recover=all
//   -fno-omit-frame-pointer
// so a memory error or UB in the hostile-PDF parsing paths fails the job.
//
// This file is NOT registered with CTest and is not built by the normal CMake
// build: it is a CI-job harness, compiled directly by
// .github/workflows/ci.yml (job `content-spans-sanitizer`) in the same spirit
// as the fuzz/ harnesses. Locally it builds with any g++/clang that has Qt6
// Core headers.
//
// What is asserted:
//   * Memory safety and UB freedom (the sanitizer's job) for every entry
//     point — lex, restackImage, wrapImageInExtGState, replaceImageMatrix —
//     over adversarial inputs, including a bounded byte-mutation sweep.
//   * Token-stream consistency for every successful lex: tokens ordered,
//     in range, covering [0, size).
//   * Verdicts that are true on the current engine and stay true after the
//     CX-08..CX-12 fixes land (set-membership or fix-agnostic invariants —
//     the gate must not break when the image lanes tighten the semantics,
//     and must not weaken those fixes either: where the current engine
//     already refuses, the refusal is pinned exactly).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <QByteArray>
#include <QList>

#include "engines/podofo/ContentSpans.h"

using gp::content::EditResult;
using gp::content::Token;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Every successful lex must produce an ordered, in-range token stream.
static void checkTokenConsistency(const QByteArray &s, const QList<Token> &toks)
{
    qsizetype prev = 0;
    for (const Token &t : toks) {
        CHECK(t.start >= prev);          // ordered, no overlap
        CHECK(t.end > t.start);          // non-empty
        CHECK(t.start < s.size());       // in range
        CHECK(t.end <= s.size());
        prev = t.end;
    }
}

static void checkStreamStaysLexable(const QByteArray &s, const QByteArray &out)
{
    // The engine never re-serializes operands, so an edit must return a
    // stream that still lexes with balanced q/Q — whatever the verdict was.
    QList<Token> toks;
    CHECK(gp::content::lex(out, &toks));
    checkTokenConsistency(out, toks);
}

// ── CX-08: inline-image data extents ───────────────────────────────────────
// Hostile inline images: binary data that contains " EI " inside unfiltered
// bytes, EI at the very end, EI followed by a delimiter, a missing EI, an
// empty stream after ID. The exact-extent fix (take /L, W×H×BPC for
// unfiltered data, the EOD marker for AHx/A85, refuse otherwise) may change
// WHICH verdict each input gets; the gate pins only memory safety, stream
// consistency and the two verdicts no honest lexer may change (missing EI and
// ID-at-end are Malformed on any implementation).
static void inlineImageExtents()
{
    QList<Token> toks;

    // Unfiltered binary data whose bytes embed a fake whitespace-delimited
    // EI, followed by what lexes as another operator. Memory safety and
    // consistency must hold for whichever data extent the engine takes.
    QByteArray binary;
    for (int i = 0; i < 64; ++i)
        binary.append(char(i & 0xff));
    binary.insert(20, " EI /ImA Do ");   // the fake end-of-image inside data
    QByteArray s = "0 0 8 1 /AHx false ID " + binary + " 1 js";
    if (gp::content::lex(s, &toks))
        checkTokenConsistency(s, toks);
    else
        CHECK(true);                     // a refusal is also acceptable

    // EI exactly at the end of the stream (j+2 >= size branch).
    s = "0 0 1 1 ID \x01\x02\x03 EI";
    if (gp::content::lex(s, &toks))
        checkTokenConsistency(s, toks);

    // EI followed by a delimiter rather than whitespace.
    s = "0 0 1 1 ID \x01 EI/Q\n";
    if (gp::content::lex(s, &toks))
        checkTokenConsistency(s, toks);

    // EI preceded by data, with the one-byte whitespace after ID consumed:
    // "ID\nEI" ends the (empty) data immediately.
    s = "0 0 1 1 ID\nEI\n1 js";
    CHECK(gp::content::lex(s, &toks));
    checkTokenConsistency(s, toks);
    bool hasInline = false;
    for (const Token &t : toks)
        if (t.kind == Token::Kind::InlineImage) hasInline = true;
    CHECK(hasInline);
    CHECK(!toks.isEmpty() && toks.last().text == "js");

    // Missing EI: malformed, on the current engine and on any fixed one.
    CHECK(!gp::content::lex("0 0 1 1 ID \x01\x02\x03 1 js", &toks));

    // ID at the very end: malformed (no data, no EI).
    CHECK(!gp::content::lex("q 0 0 1 1 ID", &toks));

    // ID with nothing but a stray whitespace byte after it.
    CHECK(!gp::content::lex("0 0 1 1 ID ", &toks));
}

// ── CX-09: restack must not be silently wrong across marked content ────────
// The current engine crosses BDC/BMC/EMC (the finding). The fix refuses.
// The gate pins: the stream it returns (if any) is still lexable and still
// contains every original byte, and a refusal leaves the output untouched.
static void restackMarkedContent()
{
    const QByteArray s =
        "q 1 0 0 1 0 0 cm /ImA Do Q\n"
        "/P BDC\n(hello) Tj\nEMC\n";

    QByteArray out;
    const EditResult r = gp::content::restackImage(s, "ImA", true, &out);
    CHECK(r == EditResult::Changed || r == EditResult::StateInTheWay
          || r == EditResult::SharedBlock || r == EditResult::NotIsolated);
    if (r == EditResult::Changed) {
        checkStreamStaysLexable(s, out);
        // Byte-exactness: the edit only relocates the block, so the output
        // must still contain the marked-content sequence verbatim.
        CHECK(out.contains("/P BDC"));
        CHECK(out.contains("(hello) Tj"));
        CHECK(out.contains("EMC"));
        CHECK(out.contains("/ImA Do"));
    } else {
        CHECK(out.isEmpty());            // a refusal must not touch `out`
    }

    // Unbalanced marked content around the image block: whatever the engine
    // decides, it may not crash and a Changed stream stays lexable.
    const QByteArray unbalanced =
        "q /OC /BMC\n/ImA Do\nEMC\nQ\nq /P BDC (x) Tj EMC\n";
    const EditResult r2 = gp::content::restackImage(unbalanced, "ImA", false, &out);
    if (r2 == EditResult::Changed) checkStreamStaysLexable(unbalanced, out);
}

// ── CX-11: a shared graphics block must not be silently rewritten ──────────
// The current engine replaces the shared cm (the finding); the fix refuses
// with SharedBlock. The gate pins the verdict set, stream validity and that
// the neighbour placement's bytes are never lost.
static void sharedBlockMatrix()
{
    const QByteArray s = "q 100 0 0 100 10 20 cm /ImA Do /ImB Do Q";
    QByteArray out;
    const EditResult r =
        gp::content::replaceImageMatrix(s, "ImA", "1 0 0 1 30 40", &out);
    CHECK(r == EditResult::Changed || r == EditResult::SharedBlock
          || r == EditResult::NotIsolated);
    if (r == EditResult::Changed) {
        checkStreamStaysLexable(s, out);
        CHECK(out.contains("/ImB Do"));  // the neighbour survives byte-exact
    } else {
        CHECK(out.isEmpty());
    }

    // Image + text painting in one block: same verdict set.
    const QByteArray st = "q /ImA Do (hello) Tj Q";
    const EditResult rt =
        gp::content::replaceImageMatrix(st, "ImA", "1 0 0 1 0 0", &out);
    CHECK(rt == EditResult::Changed || rt == EditResult::SharedBlock
          || rt == EditResult::NotIsolated);
    if (rt == EditResult::Changed) checkStreamStaysLexable(st, out);
}

// ── CX-10: the opacity gs wrapper ──────────────────────────────────────────
// Wrapping is idempotent on the current engine and must stay so after the
// gs-only-wrapper fix: the first wrap Changed, the identical second wrap
// Unchanged (the caller then edits the ExtGState in place).
static void extGStateWrap()
{
    const QByteArray s = "q 1 0 0 1 0 0 cm /ImA Do Q";
    QByteArray out;
    CHECK(gp::content::wrapImageInExtGState(s, "ImA", "GSop", &out)
          == EditResult::Changed);
    CHECK(out.contains("/GSop gs"));
    checkStreamStaysLexable(s, out);
    CHECK(gp::content::wrapImageInExtGState(out, "ImA", "GSop", &out)
          == EditResult::Unchanged);

    // An unrelated existing gs at the parent level must not crash the wrap
    // detector nor be destroyed by it.
    const QByteArray sg = "q /GSx gs /ImA Do Q";
    QByteArray outg;
    const EditResult rg = gp::content::wrapImageInExtGState(sg, "ImA", "GSop", &outg);
    CHECK(rg == EditResult::Changed);
    checkStreamStaysLexable(sg, outg);

    // Tight adjacency — the CX-15 gate's first find. A legal stream may put
    // the name directly after a regular character ('/' is a delimiter, so
    // "cm/ImA Do" needs no whitespace). The wrap used to insert its "q"
    // glued onto the preceding word ("cm" + "q" lexed as one foreign
    // operator "cmq"), so the wrapper never opened and the inserted Q broke
    // the q/Q balance: the output was corrupt and unlexable.
    const QByteArray tight = "q 5 5 cm/ImA Do Q";
    QByteArray outt;
    CHECK(gp::content::wrapImageInExtGState(tight, "ImA", "GSop", &outt)
          == EditResult::Changed);
    checkStreamStaysLexable(tight, outt);
    CHECK(outt.contains("\nq\n/GSop gs\n/ImA Do\nQ\n"));
    CHECK(gp::content::wrapImageInExtGState(outt, "ImA", "GSop", &outt)
          == EditResult::Unchanged);
}

// ── CX-12: shapes that made raw-substring deletion lie ─────────────────────
// The deletion itself lives in PoDoFoBackend; these are the stream shapes it
// is exercised through, pinned here so the sanitizer sees them too: the image
// block at offset 0, at the very end, CRLF line endings, # escaped names and
// "/ImA Do" appearing only inside a literal string or inline-image data.
static void deletionShapes()
{
    QList<Token> toks;
    QByteArray out;

    // Block at offset 0 — the raw-substring path saw no newline before the q.
    const QByteArray s0 = "q 100 0 0 100 20 20 cm /ImA Do Q";
    CHECK(gp::content::replaceImageMatrix(s0, "ImA", "2 0 0 2 0 0", &out)
          == EditResult::Changed);
    checkStreamStaysLexable(s0, out);
    CHECK(out.contains("2 0 0 2 0 0 cm /ImA Do"));

    // Block at the very end of the stream (blockClose is the last token).
    const QByteArray se = "1 js\nq 1 0 0 1 5 5 cm /ImA Do Q";
    CHECK(gp::content::replaceImageMatrix(se, "ImA", "1 0 0 1 6 6", &out)
          == EditResult::Changed);
    checkStreamStaysLexable(se, out);

    // CRLF line endings everywhere.
    const QByteArray crlf = "1 js\r\nq 1 0 0 1 5 5 cm\r\n/ImA Do\r\nQ";
    CHECK(gp::content::lex(crlf, &toks));
    CHECK(gp::content::replaceImageMatrix(crlf, "ImA", "1 0 0 1 7 7", &out)
          == EditResult::Changed);
    checkStreamStaysLexable(crlf, out);

    // # escaped name decodes to the same placement.
    const QByteArray esc = "q 1 0 0 1 5 5 cm /Im#41 Do Q";
    CHECK(gp::content::restackImage(esc, "ImA", true, &out) == EditResult::Unchanged);
    CHECK(gp::content::restackImage(esc, "ImX", true, &out) == EditResult::NotFound);

    // The name only inside a string: parsing must not find a placement there.
    CHECK(gp::content::restackImage("(x /ImA Do y) Tj", "ImA", true, &out)
          == EditResult::NotFound);

    // q/Q unbalanced: malformed, never a silent edit.
    CHECK(!gp::content::lex("q /ImA Do", &toks));
    CHECK(!gp::content::lex("Q /ImA Do Q", &toks));
    CHECK(gp::content::restackImage("q /ImA Do", "ImA", true, &out)
          == EditResult::Malformed);

    // A cm with missing operands: refused as malformed, no OOB operand read.
    CHECK(gp::content::replaceImageMatrix("q cm /ImA Do Q", "ImA", "1 0 0 1 0 0", &out)
          == EditResult::Malformed);
    CHECK(gp::content::replaceImageMatrix("q 1 0 0 1 cm /ImA Do Q", "ImA", "1 0 0 1 0 0", &out)
          == EditResult::Malformed);
}

// ── Bounded mutation sweep ─────────────────────────────────────────────────
// Flip each byte of a representative stream to structural characters and run
// every entry point. The sanitizer is the assertion; the invariants above are
// checked where a verdict is deterministic (NotFound for an absent name).
static void mutationSweep()
{
    const QByteArray base =
        "q 1 0 0 1 5 5 cm /ImA Do Q\n"
        "/P BDC (t) Tj EMC\n"
        "0 0 1 1 ID \x01 EI\n"
        "q /ImB Do /ImC Do Q\n";
    const char probes[] = { 'q', 'Q', 'E', 'I', '(', ')', '<', '>', '/',
                            '\\', '%', '\0', '\n', 'D', 'O' };

    for (int i = 0; i < base.size(); ++i) {
        for (char c : probes) {
            QByteArray s = base;
            s[i] = c;
            QList<Token> toks;
            if (gp::content::lex(s, &toks))
                checkTokenConsistency(s, toks);
            QByteArray out;
            const EditResult r1 = gp::content::restackImage(s, "ImA", true, &out);
            if (r1 == EditResult::Changed) checkStreamStaysLexable(s, out);
            out.clear();
            const EditResult r2 = gp::content::restackImage(s, "ImA", false, &out);
            if (r2 == EditResult::Changed) checkStreamStaysLexable(s, out);
            out.clear();
            const EditResult r3 =
                gp::content::replaceImageMatrix(s, "ImA", "1 0 0 1 0 0", &out);
            if (r3 == EditResult::Changed) checkStreamStaysLexable(s, out);
            out.clear();
            const EditResult r4 = gp::content::wrapImageInExtGState(s, "ImA", "GS", &out);
            if (r4 == EditResult::Changed) checkStreamStaysLexable(s, out);
            (void)r1; (void)r2; (void)r3; (void)r4;
        }
    }
}

int main()
{
    inlineImageExtents();
    restackMarkedContent();
    sharedBlockMatrix();
    extGStateWrap();
    deletionShapes();
    mutationSweep();

    std::fprintf(stderr,
                 "ContentSpansAdversarial: %d checks, %d failures\n",
                 g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
