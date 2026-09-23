// SPDX-License-Identifier: Apache-2.0
#pragma once

// ── T1-2: Redaction Proof Mode — machine-verifiable redaction evidence ──────
//
// The corpus's #1 user demand (17 r/pdf threads, docs/research/r-pdf-mining.md
// Finding 2.1): people lose lawsuits and jobs over "black boxes" that are not
// really redacted. Every competitor stops at "trust us" — GlyphPDF's combo of
// excision + transaction + sanitize is the moat (synthesis.md M7); the proof
// pack is its honest face.
//
// This module is a POST-COMMIT VERIFIER over saved artifacts. It never writes
// the documents it verifies: given the ORIGINAL source, the committed redacted
// output (and optionally the sanitized copy) plus the exact redaction geometry
// that was applied, it produces a per-excision manifest and a survival sweep,
// and a PASS/FAIL verdict that is allowed — designed — to fail loudly.
//
//   Per-excision manifest  — location (page/region), method, SHA-256 of the
//                            page's decoded content stream before→after, and
//                            text-operator/text-run counts before→after.
//   Removed strings        — attributed per excision by decoding the SOURCE
//                            with PDFium and collecting the text runs whose
//                            geometry intersects the mark.
//   Survival sweep         — full-document search for every removed string
//                            across every surface of the output(s):
//                            raw bytes, string objects, decoded streams,
//                            decode-level text extraction, info dictionary,
//                            XMP metadata, embedded files, and the revision
//                            structure (incremental-update remnants).
//   Proof pack             — JSON + human-readable TXT export for counsel /
//                            FOIA officers, with an explicit no-overclaim
//                            disclaimer: it is EVIDENCE, not an audit stamp.
//
// Honest-failure contract (the point of the feature): a survivor found on ANY
// sweepable surface FAILS the proof and names where; a surface that cannot be
// swept (stream decode failure, unreadable output, extraction failure) FAILS
// the proof — a proof that cannot fail is worthless. Verified-empty marks
// (regions with no extractable text) are recorded as such in the manifest, and
// the pack states the image-text limitation instead of overclaiming.

#include <QString>
#include <QStringList>
#include <QList>
#include <QRectF>
#include <QMap>
#include <QDateTime>
#include <QMetaType>

class QJsonArray;
class QJsonObject;

namespace gp {
namespace RedactionProof {

// Pack format identifier (schema version of the JSON artifact).
inline constexpr const char* kProofFormatVersion = "glyphpdf-redaction-proof/1";

// Removal method recorded per excision. The current transactional redaction
// (RedactOperation → engine applyRedactions) is excision. "rasterize" is
// reserved for the combo rasterization path and is never produced today; the
// manifest enum exists so the pack schema survives that addition unchanged.
enum class Method { Excision };
QString methodName(Method method);

enum class EntryStatus {
    Verified,              // attributed strings swept clean on every surface
    VerifiedNoTextInRegion,// mark covered no extractable text (nothing removed — recorded honestly)
    Failed                 // a survivor matched this entry's strings, or its mechanical evidence is missing
};
QString entryStatusName(EntryStatus status);

// Surfaces the survival sweep covers. Order is presentation order.
enum class Surface {
    RawBytes,           // literal byte scan of the whole output file
    ObjectStrings,      // every string value in every object (annotations,
                        // outlines, form fields, structure alt text, ...)
    DecodedStreams,     // every stream with filters applied (content, XMP,
                        // object streams, attached files, ...)
    ExtractedText,      // PDFium decode-level extraction over every page
    InfoDictionary,     // document info (title/author/subject/keywords/...)
    XmpMetadata,        // catalog /Metadata XMP stream
    EmbeddedFiles,      // embedded file payloads (decoded)
    RevisionStructure   // single-revision check (incremental-update remnants)
};
QString surfaceName(Surface surface);

enum class SurfaceVerdict {
    Clean,    // swept, nothing found
    Survivor, // swept, a removed string was found — the proof FAILS
    Unswept,  // the surface could not be swept — the proof FAILS (honest failure)
    Absent    // nothing of this kind exists in the document (no XMP, no
              // attachments, single revision) — nothing to sweep
};

struct SurfaceReport {
    Surface surface = Surface::RawBytes;
    SurfaceVerdict verdict = SurfaceVerdict::Absent;
    int itemsScanned = 0;   // streams / strings / pages / files actually scanned
    int itemsRawOnly = 0;   // media streams scanned raw-only (image data is not
                            // decodable as text; still byte-scanned)
    QStringList survivors;  // found removed strings
    QStringList locations;  // where each survivor sits (page / object / role)
    QStringList problems;   // why Unswept (empty otherwise)
    QString note;           // honest limitation wording (always present for media)
};

struct ExcisionEntry {
    int pageIndex = -1;        // 0-based (as supplied in the request)
    QRectF region;             // viewer coordinates as supplied
    Method method = Method::Excision;

    EntryStatus status = EntryStatus::Failed;
    QString detail;            // per-entry honest wording (status reason)

    // Source text attributed to THIS mark: PDFium text runs of the SOURCE
    // whose geometry intersects the region, decoded Unicode.
    QStringList removedStrings;

    // Page-level mechanical evidence (identical for every entry of a page;
    // the excision operates on whole-page content streams).
    QString pageStreamSha256Before;
    QString pageStreamSha256After;
    bool pageStreamDigestable = false; // false = stream could not be read on
                                       // either side (entry Failed: the
                                       // digests are the load-bearing claim)
    int textOpsBefore = -1;    // glyph-carrying text-showing operators in the
    int textOpsAfter = -1;     // page's decoded content stream, before → after
                               // (the excision's numeric-only [ N ] TJ gap
                               // substitutes do not count — see
                               // countTextOperators)
    int textRunsBefore = -1;   // PDFium text runs on the page, before → after
    int textRunsAfter = -1;
};

struct Request {
    QString sourcePath;    // ORIGINAL — re-read read-only, never written
    QString outputPath;    // committed redacted output
    QString sanitizedPath; // optional sanitized copy (swept too when non-empty)
    // The exact redaction geometry that was applied (0-based page → rects).
    QMap<int, QList<QRectF>> redactionsByPage;
    // Survivor strings known by the caller that geometry attribution cannot
    // derive (e.g. pattern-redaction matches). Swept like derived strings.
    QStringList extraSurvivorStrings;
};

struct Result {
    bool proofRan = false;     // false only when verify() could not run at all
    bool proofPassed = false;  // the verdict (meaningful only when proofRan)
    QString error;             // why the proof could not run (proofRan == false)

    QStringList failureReasons; // loud, located: surface + where + what
    QString sourceSha256;
    QString outputSha256;
    QString sanitizedSha256;
    qint64 outputBytes = 0;
    int pagesBefore = 0;
    int pagesAfter = 0;

    QList<ExcisionEntry> entries;
    QList<SurfaceReport> surfaces;
    QStringList extraStringsSwept; // the request's extra survivor strings

    QString application;      // tool identity line for the pack
    QDateTime generatedAtUtc;

    int survivorCount() const;
    bool hasUnsweptSurfaces() const;

    // ── Proof pack export ───────────────────────────────────────────────────
    // JSON artifact (machine-checkable) + TXT report (counsel-readable).
    QByteArray toJson() const;
    QString toTextReport() const;
    // Writes both files. Either path may be empty to skip that file.
    bool exportPack(const QString& jsonPath, const QString& textPath, QString* err) const;
};

// Runs the whole verification against COMMITTED files. Deterministic, pure
// reads; never writes the inputs. Never throws.
Result verify(const Request& request);

// ── Unit-testable primitives (also used by the sweep itself) ────────────────

// All byte encodings a survivor string can legitimately take inside a PDF:
// raw UTF-8/ASCII, UTF-16BE with BOM (PDF text strings), UTF-16LE, hex-string
// ASCII forms, and the backslash-escaped literal-string form.
QList<QByteArray> survivorEncodings(const QString& text);

// Counts GLYPH-CARRYING text-showing operators (Tj, TJ-with-strings, ', ") in
// a decoded content stream. String literals are skipped with full escape
// handling so "(Tj) Tj" counts exactly one operator. A numeric-only `[ N ] TJ`
// (the excision engine's advance-gap substitute for a removed Tj) is NOT
// counted — it shifts the cursor but carries no glyphs, so the count decreases
// exactly when real glyph runs were removed.
int countTextOperators(const QByteArray& decodedStream);

} // namespace RedactionProof
} // namespace gp

Q_DECLARE_METATYPE(gp::RedactionProof::Result)
