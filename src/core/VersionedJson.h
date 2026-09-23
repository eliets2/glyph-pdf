// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QByteArray>
#include <QString>

namespace gp {

// ── Shared versioned-JSON file mechanics (SWEEP-QUALITY-NEW D1) ──────────────
//
// The 2026-09 waves introduced three versioned-JSON surfaces that each
// hand-rolled the same file-level mechanics (BatchPreset store files,
// SigningRequestModel sidecars, PolicyController policy.json). The fail-closed
// ENVELOPE laws stay per-surface — their diagnostics are deliberate and pinned
// (magic/kind/schemaVersion ordering, unknown-key discipline, resource caps;
// the law is documented in docs/audit/SWEEP-QUALITY-NEW-2026-09-20.md §D6) —
// but the atomic WRITE is one mechanic and lives here exactly once.
//
// atomicWrite() is the canonical "versioned artifact commit": QSaveFile open →
// write → checked commit. No directory is created and no fallback exists (a
// refused write leaves any existing file byte-identical). Callers that need a
// directory (the signing-request sidecar convention) mkpath first, then
// delegate here.
namespace VersionedJson {

// Atomically replace `path` with `bytes` (QSaveFile transaction). Returns
// false with a user-presentable `err` (naming the path and the OS reason)
// when the file cannot be opened, written, or committed; the destination is
// untouched in every failure case.
bool atomicWrite(const QString& path, const QByteArray& bytes, QString* err);

} // namespace VersionedJson

} // namespace gp
