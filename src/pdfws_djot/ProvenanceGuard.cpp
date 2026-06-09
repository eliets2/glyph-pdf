// SPDX-License-Identifier: Apache-2.0
#include "ProvenanceGuard.h"

namespace pdfws {

// CHAIN-1 gate (R2-1 D1):
//
// A DjotThenSave round-trip serialises the entire document from scratch and
// passes it through PoDoFo's full Save(), which re-lays out all object offsets.
// That destroys every /ByteRange window that was computed over the ORIGINAL byte
// sequence — silently invalidating any existing signature.  Therefore:
//
//   signed + DjotThenSave (any origin) → REFUSE, redirect to "Save as copy"
//
// DirectStructural edits on a signed document are safe because the caller
// (HomeController::onSave, D2) routes them through writeUpdate(), which performs
// an incremental append and leaves the original byte range intact.
//
// The DjotThenSaveAsCopy path is always safe: it writes a NEW file, so no
// existing /ByteRange is affected.

ProvenanceGuardResult ProvenanceGuard::checkEditVia(docmodel::ProvenanceTag origin, bool isSigned, EditPath path) const {
    ProvenanceGuardResult result;

    if (path == EditPath::DjotThenSave && isSigned) {
        // Hard constraint regardless of origin: a Djot round-trip full-rewrites
        // the document, which invalidates all /ByteRange signatures.
        throw ProvenanceViolation(
            "Cannot save a signed document via a full rewrite (Djot round-trip). "
            "Use 'Save as copy' to create an unsigned copy, or make structural "
            "edits only (which are routed through incremental update).");
    }

    if (path == EditPath::DjotThenSave &&
        origin == docmodel::ProvenanceTag::BornPDF) {
        // Unsigned BornPDF + Djot round-trip: warn but allow.
        result.allowed = true;
        result.requiresWarning = true;
        return result;
    }

    // All other combinations (DirectStructural ±signed, DjotThenSaveAsCopy,
    // BornDjot/BornOCR + DjotThenSave unsigned) are unconditionally safe.
    result.allowed = true;
    result.requiresWarning = false;
    return result;
}

} // namespace pdfws
