// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QList>

namespace PoDoFo {
class PdfMemDocument;
class PdfField;
}

namespace gp::formjs {

// ── Phase 1: run-side AcroForm Calculate (/AA /C) + Format (/AA /F) ──────────
// Integration contract (design doc §4): the calculate cascade runs INSIDE the
// FormManager R01 transaction (candidate → mutate → validate → commit), so the
// user's field value and every recalculated /V persist as ONE atomic commit.
// Format is display-only and NEVER writes /V (Acrobat semantics: formatting
// changes presentation, not value — writing formatted strings into /V would
// corrupt round-tripping).
//
// Honesty contract (tasking): a script failure never produces a wrong value.
// The affected field keeps its committed value; the failure is reported with
// the field name, an error kind and the engine's reason.

struct FieldJsFailure {
    QString fieldName;
    QString kind;    // "timeout" | "memory" | "syntax" | "exception" | "rejected" | "engine"
    QString reason;  // engine/classifier reason, user-presentable
};

struct CascadeReport {
    int calculated = 0;                    // fields whose /V was rewritten
    int attempted = 0;                     // /CO entries with a runnable /AA /C
    bool engineAborted = false;            // timeout/memory — remaining entries skipped
    QList<FieldJsFailure> failures;        // per-field, field-attributed
    QStringList blockedActions;            // egress verbs attempted (audit)
    QStringList logs;                      // console/app.alert sink output
};

class FormJsRunner {
public:
    // Global kill-switch (default: enabled when the engine is linked).
    // The DISABLED state IS the pre-fix disclosure behavior; the revert-verify
    // test proves the cascade tests detect it. Not persisted anywhere.
    static void setExecutionGloballyEnabled(bool on);
    static bool executionGloballyEnabled();

    // Runs the AcroForm /CO calculate cascade over the in-memory document:
    // for each /CO entry carrying /AA /C (S=JavaScript), builds the event
    // object (value = the field's current /V), evaluates the script and, on
    // success with rc=true, writes event.value back to the field's /V and
    // refreshes the value snapshot later entries read. Cyclic/malformed /CO
    // terminates (each field is calculated at most once per cascade — the
    // pdf.js `_isCalculating` pattern adapted to the single-pass Acrobat
    // order; depth cap = /CO length).
    //
    // Failure policy:
    //   syntax/exception/rejected  → skip that field (keep its /V), CONTINUE
    //                                the cascade so other fields still compute;
    //   timeout/memory             → skip that field and ABORT the remaining
    //                                cascade (engine state is not trusted);
    //   the document is left consistent either way; failures are returned.
    static CascadeReport runCalculateCascade(PoDoFo::PdfMemDocument& doc,
                                             int eventDeadlineMs = 250,
                                             int cascadeDeadlineMs = 1000);

    // Runs the field's /AA /F (Format) script as a DISPLAY-ONLY pass and
    // returns the formatted presentation value. Never writes /V. Returns a
    // null QString when the field has no format script; on failure returns
    // the unformatted value and fills `failure` (honest attribution).
    static QString formatForDisplay(PoDoFo::PdfMemDocument& doc,
                                    const PoDoFo::PdfField& field,
                                    FieldJsFailure* failure = nullptr,
                                    int eventDeadlineMs = 250);

    // Inspection helpers (no execution):
    static bool fieldHasActionScript(const PoDoFo::PdfField& field, char actionKey);
    static bool hasCalculateEntries(PoDoFo::PdfMemDocument& doc); // non-empty /CO

    // Convenience value helpers shared with the cascade (mirrors
    // FormManager's captureFieldSnapshot semantics).
    static QVariantMap collectFieldValues(PoDoFo::PdfMemDocument& doc);
    static bool writeFieldValue(PoDoFo::PdfMemDocument& doc, const QString& name,
                                const QString& value);

    // ── P2/P3 hooks (present, deliberately unimplemented — no stubs) ────────
    // Phase 2: runValidateEvent     — /AA /V on the commit path (event.rc=false
    //                                 blocks the commit with the field's error).
    // Phase 2: runKeystrokeEvent    — /AA /K in the Qt line-edit layer
    //                                 (AFMergeChange semantics).
    // Phase 3: runDocumentOpenAction/NamedScripts — consent-gated, per-document
    //                                 session runtime with an audit surface.
};

} // namespace gp::formjs
