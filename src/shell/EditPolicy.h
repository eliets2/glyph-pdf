// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QObject>
#include <QString>
#include "core/ToolId.h"
#include "engines/DocumentSession.h"

namespace gp {
namespace EditPolicy {

// ── ARC07 (TEAM-ARCHITECTURE-REVIEW-2026-09-07): ONE read-only policy ────────
//
// The review: "enforce editability at the shared command/mutation dispatch
// boundary and reflect it in action enablement. Keep viewing, selection and
// permitted copy/export actions available. Do not rely solely on
// cursor/tool-mode gating, and do not scatter a separate policy
// implementation through every dialog."
//
// This header IS that single implementation. The state lives on
// DocumentSession (the shared session every controller holds); this file only
// classifies tools and answers the gate question. Consumers:
//   - ToolRegistry::activate (the shared tool-dispatch boundary) refuses a
//     mutating tool whose controller reports it disabled — enablement and
//     dispatch are the same predicate;
//   - each controller's isEnabled() override (one line, below) so lazily
//     created QActions and the registry agree;
//   - direct mutation entries that bypass the registry (viewer-signal slots
//     in PagesController, FindBar replace/redact-all in EditController,
//     HomeController::saveNow, ALL THREE PagesMode reorder routes —
//     grid drag/keyboard moves, the reorder-panel list Apply button, and
//     page labels — FormBuilderMode's tab-order Apply button, and Form
//     Builder's field properties panel — Apply entry and keystroke entry,
//     emergence E-1) call mutationBlocked() at their top.
//     (S2-1, SWEEP-BACKEND-2026-09-21: this comment previously claimed
//     "PagesMode reorder/labels" without exception, but the reorder-panel
//     list Apply route was ungated until the audit caught it; S2-2 caught
//     the same class in FormBuilderMode::onTabOrderApplyClicked — the
//     comment now names every direct route because the code gates them.)

// The mutation tool set, in one place. Viewing/selection (Hand, Select,
// SelectObject/EditObject arming, Search), navigation, and copy/export-shaped
// actions (Extract, Split, SaveAs, Print, Share, ExportData, ExportAnno,
// ValidateSig, page-management dialogs) deliberately stay available.
inline bool isMutatingTool(ToolId id)
{
    switch (id) {
    // Pages: in-place page geometry/structure and stamping.
    case ToolId::RotateCW:
    case ToolId::RotateCCW:
    case ToolId::DeletePage:
    case ToolId::InsertPage:
    case ToolId::Crop:
    case ToolId::Resize:
    case ToolId::AddHeader:
    case ToolId::AddFooter:
    case ToolId::AddPageNumbers:
    case ToolId::BatesNumber:
    // Edit: content and annotation mutation (arming a placement tool writes
    // on the next click; refusing here gives honest feedback instead of the
    // viewer's silent fall-back to the hand tool).
    case ToolId::EditText:
    case ToolId::Image:
    case ToolId::EditImage:
    case ToolId::Erase:
    case ToolId::Cut:
    case ToolId::DeleteSelection:
    case ToolId::Highlight:
    case ToolId::Underline:
    case ToolId::Strikeout:
    case ToolId::Squiggly:
    case ToolId::Pencil:
    case ToolId::Freehand:
    case ToolId::TextBox:
    case ToolId::AddText:
    case ToolId::Note:
    case ToolId::Comment:
    case ToolId::Stamp:
    case ToolId::Callout:
    case ToolId::MarkRedact:
    case ToolId::Signature:
    case ToolId::Rectangle:
    case ToolId::Oval:
    case ToolId::Line:
    case ToolId::Arrow:
    // Forms: field creation/edition, auto-detect placement, data import.
    case ToolId::CreateForm:
    case ToolId::TextField:
    case ToolId::Checkbox:
    case ToolId::Radio:
    case ToolId::Dropdown:
    case ToolId::ListBox:
    case ToolId::DateField:
    case ToolId::NumField:
    case ToolId::Button:
    case ToolId::SigField:
    case ToolId::CalcField:
    case ToolId::AutoDetect:
    case ToolId::Tabs:
    case ToolId::ImportData:
    // Security: encryption, signing, redaction burn-in, sanitization,
    // permissions, expiry, annotation import.
    case ToolId::Encrypt:
    case ToolId::Password:
    case ToolId::Sign:
    case ToolId::Sanitize:
    case ToolId::ApplyRedact:
    case ToolId::ImportAnno:
    case ToolId::Permissions:
    case ToolId::RemoveSecurity:
    case ToolId::Certify:
    case ToolId::Timestamp:
    case ToolId::PatternRedact:
    case ToolId::RegexRedact:
    case ToolId::ExpiryDate:
    case ToolId::CertEncrypt:   // N17: re-encrypts the session document
    case ToolId::PrepareSigningRequest: // R26: creates signature fields in place
    // Home: save-in-place (Save As stays available).
    case ToolId::Save:
        return true;
    default:
        return false;
    }
}

// The shared gate for TOOL dispatch: the session is read-only and the tool
// mutates. (Viewing, selection and permitted copy/export tools stay enabled —
// isMutatingTool() decides, not the gate.)
inline bool toolRefusedByReadOnly(const DocumentSession* session, ToolId id)
{
    return session && session->isReadOnly() && isMutatingTool(id);
}

// The shared gate for DIRECT mutation entries that carry no ToolId (viewer
// signal slots, dialog-driven writes). The caller shows readOnlyMessage().
inline bool mutationBlocked(const DocumentSession* session)
{
    return session && session->isReadOnly();
}

// One user-facing refusal wording for every read-only refusal.
inline QString readOnlyMessage()
{
    return QObject::tr("Document is read-only — editing is disabled.");
}

} // namespace EditPolicy
} // namespace gp
