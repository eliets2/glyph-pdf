// SPDX-License-Identifier: Apache-2.0
#pragma once

// ── Shared read-only PoDoFo dictionary helpers (SWEEP-QUALITY-NEW D2) ────────
//
// Header-only, INTERNAL to src/engines readers (AccessibilityChecker,
// AccessibilityFixes, …). Deliberately read-only: mutation stays with the
// owning engine module. `resolve` semantics are the repo idiom verbatim
// (as in PoDoFoBackend.cpp): an indirect reference is dereferenced through
// MustGetObject, a failed lookup resolves to nullptr instead of throwing.

#include <podofo/podofo.h>

#include <QString>

namespace gp::PoFoRead {

inline const PoDoFo::PdfObject* resolve(const PoDoFo::PdfObject* obj,
                                        PoDoFo::PdfMemDocument& doc) {
    if (obj == nullptr) return nullptr;
    if (obj->IsReference()) {
        try {
            return &doc.GetObjects().MustGetObject(obj->GetReference());
        } catch (const PoDoFo::PdfError&) {
            return nullptr;
        }
    }
    return obj;
}

// First non-empty string from a dictionary text key; empty when absent or
// not a string.
inline QString stringAt(const PoDoFo::PdfDictionary& dict, const char* key) {
    const PoDoFo::PdfObject* o = dict.FindKey(PoDoFo::PdfName(key));
    if (o == nullptr || !o->IsString()) return {};
    return QString::fromUtf8(o->GetString().GetString().data(),
                             static_cast<qsizetype>(o->GetString().GetString().size()));
}

} // namespace gp::PoFoRead
