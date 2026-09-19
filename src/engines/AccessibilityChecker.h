// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QList>

class QTemporaryDir;

// ── T2-4 accessibility P1: DETECTION engine ──────────────────────────────────
//
// gp::scanAccessibility() scans a PDF and reports what a TAGGED document
// needs. Scope discipline (P1, disclosed in the panel):
//
//   * DETECTION + DISCLOSURE only. The checker never certifies PDF/UA
//     conformance — a report with zero findings is NOT a PDF/UA claim.
//   * Content tagging (constructing a /StructTreeRoot from page-content
//     heuristics) is explicitly OUT of scope for P1. The checker detects an
//     untagged document; it does not tag it.
//   * Per-object checks (image /Alt, field /TU) sample a bounded number of
//     findings (kA11yMax*Findings) and the report DISCLOSES truncation via
//     the totals — large documents are never silently under-reported.
//
// Every finding carries an honest whyNot (why the gap matters to assistive
// technology) and names its target (page / object / field).
namespace gp {

enum class A11ySeverity { High, Medium, Low };

struct A11yFinding {
    // Stable check ids: "struct-tree", "doc-language", "doc-title",
    // "display-doc-title", "image-alt", "field-tu".
    QString checkId;
    A11ySeverity severity = A11ySeverity::Medium;
    QString where;   // human-readable target: "page 3 · image /Im0"
    QString whyNot;  // honest explanation of the gap
    int page = -1;   // 0-based page, -1 = document-level / n/a
};

struct A11yReport {
    QString path;
    bool loadOk = false;   // false ⇒ findings are meaningless (NOT "clean")
    QString loadError;
    bool tagged = false;   // document carries /StructTreeRoot

    // Truncation disclosure for the bounded per-object samples.
    int imagesReported = 0;
    int imagesTotal = 0;
    int fieldsReported = 0;
    int fieldsTotal = 0;

    QList<A11yFinding> findings;

    bool truncated() const {
        return imagesReported < imagesTotal || fieldsReported < fieldsTotal;
    }
};

// Bounded per-object sample caps. NAMED constants (pinned by
// tests/TestAccessibilityChecker.cpp) so truncation can never silently
// drift. Triage bounds, not conformance rules.
inline constexpr int kA11yMaxImageFindings = 50;
inline constexpr int kA11yMaxFieldFindings = 50;

// Scan the document at `path`. Never throws: load failures come back as
// loadOk=false with `loadError` set.
A11yReport scanAccessibility(const QString& path);

} // namespace gp
