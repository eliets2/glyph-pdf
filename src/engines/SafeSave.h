// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QString>
#include <QStringList>
#include <functional>

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
// `suffix` selects the candidate extension (default ".pdf"; the encrypted
// package writer reserves ".zip" — WP-R04).
bool makeUniqueCandidate(QString* out, QString* err, const QString& suffix = QStringLiteral(".pdf"));

// ── WP-R04: external-writer transaction (unique candidate → tool → validate →
// checked atomic replace) ────────────────────────────────────────────────────
//
// WHOLE-ARCHITECTURE-REVIEW-2026-09-10 A03: the encrypted-package flow deleted
// the destination BEFORE launching 7-Zip and let the tool write the FINAL path
// with an unbounded waitForFinished(-1) — a failed launch, a failed tool run,
// or a mid-write exit destroyed the previous package. The SafeSave transaction
// shape closes that: the external tool writes a unique OWNED candidate, the
// candidate is validated, and only then is the destination replaced through
// commitFileToDestination (atomic; destination untouched on any failure). The
// caller's destination is NEVER removed, truncated or written before commit.
//
// Process lifecycle is owned: the launched process belongs to this call, the
// wait is bounded by `timeoutMs`, and `isCanceled` is polled while the tool
// runs — a cancellation or timeout KILLS the process we own before returning.
// The reserved candidate file starts empty (size 0), so appending writers such
// as `7z a` still produce a fresh archive.
struct ExternalWriteResult {
    enum class Stage { None, Launch, Tool, ValidateCandidate, Commit };
    bool ok = false;            // true exactly when the destination was replaced
    bool canceled = false;      // isCanceled fired (process was killed)
    Stage stage = Stage::None;  // where a !ok run stopped
    int exitCode = -1;          // tool exit code when one was observed
    QString error;              // user-presentable when !ok
    QString candidatePath;      // the operation's own candidate (always removed)
};

// `buildArgs` receives the candidate path and returns the FULL argument list
// for `program` (the tool must WRITE the candidate; it never sees the
// destination). `validateCandidate` (optional) runs after the tool exited
// successfully: it receives the candidate path and returns an empty string on
// success or a user-presentable reason to refuse the commit.
using ExternalWriteArgsFn = std::function<QStringList(const QString& candidate)>;
using ExternalWriteValidateFn = std::function<QString(const QString& candidate)>;

// One bounded, cancellable process run owned by the caller (the same lifecycle
// discipline the transaction applies, exposed for tool-side validation steps
// such as an encrypted-archive read-back check). Returns true when the process
// exited normally (exitCode set); false with `error` when it could not start,
// was canceled (`*canceled`), crashed, or hit `timeoutMs` — in the cancel and
// timeout cases the process we own is KILLED before returning.
bool runBoundedProcess(const QString& program, const QStringList& args,
                       qint64 timeoutMs, const std::function<bool()>& isCanceled,
                       bool* canceled, int* exitCode, QString* error);

ExternalWriteResult runExternalWriterCommit(
    const QString& program,
    const ExternalWriteArgsFn& buildArgs,
    const QString& destination,
    const QString& candidateSuffix,
    qint64 timeoutMs,
    const std::function<bool()>& isCanceled = {},
    const ExternalWriteValidateFn& validateCandidate = {});

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

// ── GUI-held-handle coordination (engine-lane residual, step-2 ledger) ──────
//
// The viewer displaying the destination file holds an OS handle without
// delete access, so QSaveFile's atomic rename fails with "Access is denied"
// for every in-place write (rotate/save/crop/redaction commit, form import).
// The shell installs a coordinator pair ONCE (MainWindow startup):
//   release(destPath) — invoked before the destination is opened for the
//                       atomic replacement (the viewer parks its document,
//                       releasing the handle),
//   restore(destPath) — invoked after the commit attempt, on EVERY outcome
//                       (the viewer reloads the replaced or preserved file).
// NULL functions are a no-op (engine-only callers, tests without a viewer).
// Implementations must be cheap, must no-op for paths their UI does not
// display, and must only touch UI objects from the GUI thread.
using FileHandleGuard = std::function<void(const QString &destPath)>;
void setFileHandleCoordinator(FileHandleGuard releaseFileHandles,
                              FileHandleGuard restoreFileHandles);

// RAII scope for writers that bypass commitFileToDestination (e.g. the signed
// SaveUpdate incremental append in PoDoFoBackend::writeUpdate): releases on
// construction, restores on destruction — covering every early return.
class ScopedFileHandleCoordination {
public:
    explicit ScopedFileHandleCoordination(const QString &destPath);
    ~ScopedFileHandleCoordination();
    ScopedFileHandleCoordination(const ScopedFileHandleCoordination&) = delete;
    ScopedFileHandleCoordination& operator=(const ScopedFileHandleCoordination&) = delete;
private:
    QString m_dest;
    bool m_armed = false;
};

} // namespace SafeSave
} // namespace gp
