// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QVector>
#include <QRectF>

// ── R26 send-for-signing P1 — ADDITIVE signature-field creation seam ─────────
//
// WHY THIS FILE EXISTS (the additive-exposure note promised to the ledger):
// GlyphPDF had NO seam that creates a real PDF signature field. FormManager
// creates Text/Date/Numeric/Checkbox/Radio/Dropdown/ListBox/Button/Calculated
// fields only, and its Form Builder "signature" mode deliberately places a
// TEXT box ("Sig uses text box", FormBuilderMode::toolModeToFieldType) — not a
// /FT /Sig field. SignatureManager (off-limits internals) only creates
// signature fields IMPLICITLY at sign time, at a fixed engine-chosen rect on
// page 1. The send-for-signing workflow needs signer fields placed at
// PREPARED locations, so this sibling helper performs exactly that one
// mutation. It touches NO existing engine internals: it drives PoDoFo's
// public `PdfPage::CreateField<PdfSignature>` and persists through the shared
// gp::SafeSave R01 transaction (same shape as FormManager's
// runFormSaveTransaction, rebuilt here on the public SafeSave primitives
// because FormManager's private helper is not reusable without editing the
// off-limits FormManager core).
//
// Placement obeys the ONE shared page-space law (SEP13 L5/L8 /
// PageSpaceTransform.h — "Do NOT re-derive this flip locally"): caller rect is
// VIEWER space (top-left origin, Y down, displayed size); the field /Rect is
// written in RAW USER space via gp::PageSpace::viewerToUser.

namespace gp {
class SignatureFieldCreator {
public:
    // One signature field to create. `pageIndex` is 0-based; `viewerRect` is
    // the widget rect in VIEWER convention (top-left origin, Y down) on the
    // DISPLAYED page — the same convention ISignatureManager's
    // signatureFieldAnchors() reports and the on-page badges overlay.
    struct Spec {
        QString fieldName;   // fully-qualified AcroForm name; must be non-empty
        int pageIndex = 0;
        QRectF viewerRect;   // viewer convention (see above)
    };

    // Create ALL requested signature fields as ONE transaction: unique temp
    // candidate → mutate → save → reopen + validate (every name present and
    // of type Signature; page count unchanged) → checked atomic commit to
    // destPath. srcPath and destPath may be the same file; the source is only
    // ever read and the destination is only ever replaced by the atomic
    // commit (byte-identical on any failure — no direct-write fallback).
    // Refuses (fail-loud, nothing written): empty src/dest, empty or
    // duplicate field names, out-of-range pageIndex, invalid (non-positive)
    // rects. Returns false with a user-presentable `err` on every refusal.
    static bool createSignatureFields(const QString &srcPath,
                                      const QVector<Spec> &specs,
                                      const QString &destPath,
                                      QString *err);
};
} // namespace gp
