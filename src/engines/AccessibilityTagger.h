// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QList>

// ── T2-4 accessibility P2: the auto-tagging engine ───────────────────────────
//
// gp::tagDocumentAccessibility() closes the P1 checker's biggest reported gap
// (the untagged document) by CONSTRUCTING a /StructTreeRoot from page-content
// heuristics. Design: docs/research/accessibility-auto-tagging-plan-
// 2026-09-21.md. Scope discipline (disclosed in the panel and the report):
//
//   * P2 tags PARAGRAPHS (/P) and HEADINGS (/H1–/H6) only. Tables, lists,
//     /ActualText, multi-column reading-order recovery and outline merging
//     are P3 — a wrong list is worse than a plain paragraph.
//   * Headings come from document-relative font-size/weight CLUSTERING with
//     the cluster table returned in the report so the user can SEE and judge
//     the classification. It is a heuristic, never a claim: decorative
//     covers classify as headings, color-only headings are invisible to this
//     engine, and dense typographic documents produce noisy levels.
//   * Text is ToUnicode-honest: runs decode through /ToUnicode CMaps or the
//     predefined encodings only. A page whose fonts lack usable Unicode
//     maps is SKIPPED and disclosed — the engine never writes text it cannot
//     decode correctly into structure elements.
//   * /Alt is NEVER auto-generated. Images with an /Alt (seeded by the user
//     through the P1 SetImageAltText fix) become Figure elements; images
//     without one are excluded from the tree and counted in the report — an
//     empty /Alt would be a false statement of review.
//   * Reading order assumes SINGLE COLUMN (disclosed): pages carrying a
//     column-suspect signature are tagged anyway but flagged in the report.
//   * A document that ALREADY carries a /StructTreeRoot is REFUSED —
//     re-tagging would silently discard the author's own structure.
//   * The checker's sentence extends verbatim: THE CHECKER NEVER CERTIFIES
//     PDF/UA — AND A TAGGED DOCUMENT IS NOT A CONFORMING DOCUMENT. The
//     tagging engine produces a best-effort structure tree from layout
//     heuristics; it does not verify reading order against intent and it
//     issues no conformance verdict of any kind.
//
// Transaction discipline (SafeSave, same shape as every mutation): the
// rewrite + tree assembly happen in memory, a unique candidate is written,
// the candidate is validated INDEPENDENTLY (a fresh reopen runs the
// structural walk below AND the text-preservation invariant: every page's
// re-extracted (text, y-bucket) sequence must equal the pre-rewrite
// extraction, whitespace-canonical — any divergence fails the candidate and
// the original is left byte-identical), and only then is the candidate
// committed atomically. A rewriting bug can corrupt pages, so the invariant
// is the load-bearing gate, not a nicety.
namespace gp {

// Deterministic mutation seams for the negative controls (the SaveFault
// house pattern). None is the only production value.
enum class TaggerFaultForTesting {
    None = 0,
    PerturbOneShowOperator,  // corrupt one text operator during the rewrite —
                             // MUST trip the text-preservation invariant and
                             // leave the original byte-identical
    DropParentTreeEntry,     // assemble a tree missing one /ParentTree entry —
                             // the structural walk (and veraPDF) MUST reject it
};

// One row of the size-cluster table shown to the user (§3.3 disclosure):
// the detected clusters, with the level the heuristic assigned each.
struct TaggerSizeCluster {
    double size = 0;     // representative effective font size (pt)
    int runCount = 0;    // text lines carrying this size
    bool bold = false;   // weight hint fires somewhere in the cluster
    QString level;       // "H1".."H6" or "body"
};

// An image XObject lacking /Alt (the pre-flight prompt list; bounded sample
// with a disclosed total — same shape as the P1 checker's image findings).
struct TaggerImageGap {
    int page = -1;            // 0-based page
    QString resourceName;     // key under the page /XObject dict
};

// Read-only pre-flight: what tagging WOULD do, before any mutation. Feeds
// the panel's confirmation surface (cluster table + image list).
struct TaggerPreflight {
    bool loadOk = false;
    QString loadError;
    bool alreadyTagged = false;  // /StructTreeRoot present → tagging refuses
    // PR-review §3.2: a signed signature field (/V with /ByteRange) is
    // present → tagging refuses. The transaction rewrites every content
    // stream and full-saves in place, which would invalidate the signature.
    bool signedDocument = false;
    bool anyText = false;        // any honestly-decodable text runs exist
    QList<TaggerSizeCluster> sizeClusters;
    QList<TaggerImageGap> imageGaps;
    int imagesTotal = 0;         // images lacking /Alt, including truncated
};

// Result of the tagging transaction. On refusal, ok=false and `message`
// says WHY (already tagged / load failure / nothing taggable).
struct TaggerReport {
    bool ok = false;
    QString message;  // what changed, or why not — user-presentable

    QList<TaggerSizeCluster> sizeClusters;  // the table shown to the user

    struct PageNote {
        int page = -1;  // 0-based
        QString note;   // honest per-page disclosure (skip reason, suspicion)
    };
    QList<PageNote> pageNotes;

    bool columnSuspect = false;  // multi-column signature detected somewhere
    int elementsTagged = 0;
    int headingsTagged = 0;
    int paragraphsTagged = 0;
    int figuresTagged = 0;    // Figure elements (user-described images only)
    int imagesExcluded = 0;   // images left OUT of the tree (no /Alt) — both
                              // counts are named in the run report (§4.4)
};

// Pre-flight the document at `path` for tagging. Never throws.
TaggerPreflight preflightTagging(const QString& path);

// Run the tagging engine over the document at `path` IN PLACE (candidate →
// independent validation → atomic commit; viewer-handle coordination via the
// installed SafeSave coordinator). Never throws.
TaggerReport tagDocumentAccessibility(
    const QString& path,
    TaggerFaultForTesting fault = TaggerFaultForTesting::None);

// The engine's own structural verifier (design §6.1) — a public function so
// the tests do not re-implement it. Reopens `path` and walks the tree:
// every /K resolves; every MCID in /ParentTree exists as marked content in
// its stream (and vice versa); every element's /Pg matches the stream chain
// its MCIDs live in; no orphan /Parent links; /MarkInfo /Marked true.
// Returns an empty string when the tree validates, else an honest reason.
QString validateTaggedStructureTree(const QString& path);

} // namespace gp
