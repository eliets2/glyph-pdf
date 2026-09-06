// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>

namespace gp {

// ── R01 safe-save primitives (extracted from FormManager.cpp) ────────────────
//
// Audit F01 (P1) established the transaction shape every mutation boundary now
// follows: serialize the COMPLETE mutation to a unique temp candidate PDF
// (never the destination), validate the candidate, then commit the validated
// bytes to the destination with a bounded copy into QSaveFile + a checked
// commit(). The original is never deleted before the atomic rename, and if the
// replacement is blocked (open handle, full disk) the operation fails with the
// original byte-identical. There is deliberately NO direct-write fallback.
//
// These two functions are the reusable file-level primitives of that shape.
// FormManager consumes them for the form-save transaction (rung 2 — no
// duplication); RedactOperation (U05) builds the redaction transaction on them.
// The semantics are verbatim from FormManager.cpp:79-141.
namespace SafeSave {

// Reserve a unique candidate path in the system temp dir. The handle is
// released before any writer produces the candidate so the writer owns the
// file exclusively. Returns false with a user-presentable `err` on failure.
bool makeUniqueCandidate(QString* out, QString* err);

// Deterministic test seam for the commit step (mirrors FormManager's
// SaveFault::Commit injection point: after the bounded copy, before
// QSaveFile::commit()). RedactOperation::Fault::Commit maps onto this.
enum class CommitFaultForTesting { None = 0, FailBeforeCommit };
void setCommitFaultForTesting(CommitFaultForTesting fault);
CommitFaultForTesting commitFaultForTesting();

// Bounded copy of the validated candidate into QSaveFile + checked commit().
// QSaveFile writes a hidden temp in the destination's directory and atomically
// renames over the destination at commit(); a failed or canceled commit never
// touches the original. Returns false with `err` on any failure; on failure the
// destination is byte-identical (it may pre-exist and is only replaced by the
// atomic rename).
bool commitFileToDestination(const QString& candidate, const QString& destPath, QString* err,
                             CommitFaultForTesting fault = CommitFaultForTesting::None);

} // namespace SafeSave
} // namespace gp
