// SPDX-License-Identifier: Apache-2.0
#include "ProvenanceGuard.h"

namespace pdfws {

ProvenanceGuardResult ProvenanceGuard::checkEditVia(docmodel::ProvenanceTag origin, bool isSigned, EditPath path) const {
    ProvenanceGuardResult result;
    
    if (origin == docmodel::ProvenanceTag::BornPDF) {
        if (path == EditPath::DjotThenSave) {
            if (isSigned) {
                // Hard constraint: signed PDF cannot be structurally saved back
                throw ProvenanceViolation("Cannot perform semantic edit on signed BornPDF document directly. Save as a copy.");
            } else {
                // Not signed, but still BornPDF. Warn the user.
                result.allowed = true;
                result.requiresWarning = true;
            }
        } else {
            // DirectStructural or DjotThenSaveAsCopy
            result.allowed = true;
            result.requiresWarning = false;
        }
    } else {
        // BornDjot, BornOCR
        result.allowed = true;
        result.requiresWarning = false;
    }

    return result;
}

} // namespace pdfws
