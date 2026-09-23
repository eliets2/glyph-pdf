// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>

// ── T2-4 accessibility P1: the CHEAP STRUCTURAL FIXES ────────────────────────
//
// Scope discipline (see AccessibilityChecker.h):
//   * Each fix writes ONE thing the checker reported missing: /Lang,
//     /ViewerPreferences /DisplayDocTitle (title from /Info), /Alt on a
//     selected image XObject, /TU on a selected field. No auto-tagging —
//     no structure tree is ever constructed here.
//   * Every fix is a SafeSave transaction (unique candidate → independent
//     PoDoFo reopen validation → checked atomic commit; original untouched
//     on any failure) — the same shape as every other mutation.
//   * /TU goes through the EXISTING FormManager::setFieldMetadata seam (the
//     field-mutation transaction the properties panel already uses); this
//     module only supplies readFieldRequiredFlag() so the call can preserve
//     the field's /Ff Required bit exactly.
//   * Fixes never claim conformance: a fixed document simply loses one
//     finding on the next scan.
namespace gp {

enum class A11yFixKind {
    SetLanguage,
    EnableDisplayDocTitle,
    SetImageAltText,
    SetFieldTu,
};

struct A11yFixRequest {
    A11yFixKind kind = A11yFixKind::SetLanguage;
    QString language;      // SetLanguage: BCP-47 tag, chosen by the user
    int page = -1;         // SetImageAltText: 0-based page
    QString resourceName;  // SetImageAltText: key under the page /XObject dict
    QString text;          // SetImageAltText / SetFieldTu: user's description
    QString fieldName;     // SetFieldTu: fully-qualified field name
};

struct A11yFixOutcome {
    bool ok = false;
    // User-presentable. On success this is what changed; on refusal WHY
    // (e.g. "no /Info /Title — set a title in Document Properties first").
    QString message;
};

// Apply one fix to the document at `path` IN PLACE (candidate → validate →
// atomic commit; viewer-handle coordination via the installed SafeSave
// coordinator). Never throws.
A11yFixOutcome applyAccessibilityFix(const QString& path,
                                     const A11yFixRequest& request);

// Read-only: the /Ff Required bit (bit 2) of the named field, so the TU fix
// through FormManager::setFieldMetadata can preserve it. Returns false with
// *err set when the field cannot be found.
bool readFieldRequiredFlag(const QString& path, const QString& fieldName,
                           bool* required, QString* err);

} // namespace gp
