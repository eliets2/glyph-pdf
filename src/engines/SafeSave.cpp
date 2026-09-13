// SPDX-License-Identifier: Apache-2.0
#include "engines/SafeSave.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryFile>

namespace gp {
namespace SafeSave {

namespace {
// Deterministic test seam (see setCommitFaultForTesting). Mirrors the
// FormManager SaveFault injection point: the failure fires AFTER the bounded
// copy, before QSaveFile::commit(), so the cancelWriting path is exercised.
CommitFaultForTesting g_commitFaultForTesting = CommitFaultForTesting::None;

// GUI-held-handle coordinator (see SafeSave.h). Set once by the shell at
// startup; reading the pair is otherwise immutable, so no locking is needed.
FileHandleGuard g_releaseFileHandles;
FileHandleGuard g_restoreFileHandles;
} // namespace

void setFileHandleCoordinator(FileHandleGuard releaseFileHandles,
                              FileHandleGuard restoreFileHandles)
{
    g_releaseFileHandles = std::move(releaseFileHandles);
    g_restoreFileHandles = std::move(restoreFileHandles);
}

ScopedFileHandleCoordination::ScopedFileHandleCoordination(const QString &destPath)
    : m_dest(destPath)
{
    m_armed = static_cast<bool>(g_releaseFileHandles);
    if (m_armed) g_releaseFileHandles(m_dest);
}

ScopedFileHandleCoordination::~ScopedFileHandleCoordination()
{
    if (m_armed && g_restoreFileHandles)
        g_restoreFileHandles(m_dest);
}

// Reserve a unique candidate path in the system temp dir. The handle is
// released before any writer produces the candidate so the writer owns the
// file exclusively. (Verbatim semantics from FormManager.cpp:79-90.)
bool makeUniqueCandidate(QString* out, QString* err, const QString& suffix)
{
    // Dedicated subdirectory: keeps the operation's candidates isolated from
    // the shared temp root, so tests can assert "nothing left behind" without
    // cross-process debris (killed runs, concurrent lanes) polluting the scan.
    QDir candidateDir(QDir::tempPath() + QStringLiteral("/glyphpdf-candidates"));
    if (!candidateDir.exists()) QDir().mkpath(candidateDir.absolutePath());
    QTemporaryFile tmp(candidateDir.absoluteFilePath(QStringLiteral("glyphpdf-XXXXXX") + suffix));
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        if (err) *err = QStringLiteral("could not create unique candidate PDF: %1").arg(tmp.errorString());
        return false;
    }
    *out = tmp.fileName();
    tmp.close();
    return true;
}

// ── WP-R04 external-writer transaction ──────────────────────────────────────

// One bounded, cancellable process run owned by the caller. Returns true when
// the process exited normally with exitCode set; false with `error` (and
// `canceled`) when it could not start, was canceled, or hit the deadline. A
// cancel/timeout always KILLS the process we own before returning.
bool runBoundedProcess(const QString& program, const QStringList& args,
                       qint64 timeoutMs, const std::function<bool()>& isCanceled,
                       bool* canceled, int* exitCode, QString* error)
{
    *canceled = false;
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForStarted(10000)) {
        if (error) *error = QStringLiteral("could not start the external tool: %1").arg(proc.errorString());
        return false;
    }
    // Bounded wait with cooperative cancellation: the process belongs to this
    // call, so cancel/timeout must stop it (kill) rather than leave it running.
    const qint64 pollMs = 100;
    qint64 waited = 0;
    while (!proc.waitForFinished(static_cast<int>(pollMs))) {
        if (isCanceled && isCanceled()) {
            proc.kill();
            proc.waitForFinished(5000);
            *canceled = true;
            if (error) *error = QStringLiteral("canceled by the user");
            return false;
        }
        waited += pollMs;
        if (waited >= timeoutMs) {
            proc.kill();
            proc.waitForFinished(5000);
            if (error) *error = QStringLiteral("the external tool did not finish within %1 s").arg(timeoutMs / 1000);
            return false;
        }
    }
    if (proc.exitStatus() != QProcess::NormalExit) {
        if (error) *error = QStringLiteral("the external tool crashed");
        return false;
    }
    *exitCode = proc.exitCode();
    return true;
}

ExternalWriteResult runExternalWriterCommit(
    const QString& program,
    const ExternalWriteArgsFn& buildArgs,
    const QString& destination,
    const QString& candidateSuffix,
    qint64 timeoutMs,
    const std::function<bool()>& isCanceled,
    const ExternalWriteValidateFn& validateCandidate)
{
    ExternalWriteResult r;
    QString candidate;
    QString err;
    if (!makeUniqueCandidate(&candidate, &err, candidateSuffix)) {
        r.error = err;
        return r;  // nothing was started; the destination was never touched
    }
    r.candidatePath = candidate;

    if (timeoutMs <= 0) timeoutMs = 120000;  // sane default; the wait is never unbounded

    // The reservation only staked out a unique NAME; appending writers (7z a)
    // refuse a pre-existing EMPTY archive, so drop the 0-byte placeholder we
    // own before the tool starts. This is the operation's own candidate — the
    // DESTINATION is never removed here (that was the WP-R04 defect).
    QFile::remove(candidate);

    bool canceled = false;
    int exitCode = -1;
    // The tool writes the CANDIDATE it owns — the destination is not an
    // argument at all, so a misbehaving tool cannot touch it before commit.
    const QStringList args = buildArgs(candidate);
    if (!runBoundedProcess(program, args, timeoutMs, isCanceled, &canceled, &exitCode, &err)) {
        r.canceled = canceled;
        r.stage = canceled ? ExternalWriteResult::Stage::Tool : ExternalWriteResult::Stage::Launch;
        r.exitCode = exitCode;
        r.error = err;
        QFile::remove(candidate);
        return r;
    }
    r.exitCode = exitCode;

    if (exitCode != 0) {
        r.stage = ExternalWriteResult::Stage::Tool;
        r.error = QStringLiteral("the external tool failed (exit code %1)").arg(exitCode);
        QFile::remove(candidate);
        return r;
    }

    QFileInfo candidateInfo(candidate);
    if (!candidateInfo.exists() || candidateInfo.size() <= 0) {
        r.stage = ExternalWriteResult::Stage::ValidateCandidate;
        r.error = QStringLiteral("the external tool did not produce a readable output file");
        QFile::remove(candidate);
        return r;
    }

    if (validateCandidate) {
        err = validateCandidate(candidate);
        if (!err.isEmpty()) {
            r.stage = ExternalWriteResult::Stage::ValidateCandidate;
            r.error = err;
            QFile::remove(candidate);
            return r;
        }
    }

    // Checked atomic replace through the existing SafeSave commit (QSaveFile).
    // On ANY failure here the destination is byte-identical; the candidate is
    // this operation's own temp file and is always cleaned up.
    if (!commitFileToDestination(candidate, destination, &err)) {
        r.stage = ExternalWriteResult::Stage::Commit;
        r.error = err;
        QFile::remove(candidate);
        return r;
    }

    QFile::remove(candidate);
    r.candidatePath.clear();
    r.ok = true;
    return r;
}

void setCommitFaultForTesting(CommitFaultForTesting fault)
{
    g_commitFaultForTesting = fault;
}

CommitFaultForTesting commitFaultForTesting()
{
    return g_commitFaultForTesting;
}

// Bounded copy of the validated candidate into QSaveFile + checked commit().
// Note PoDoFo is never handed QSaveFile::fileName(). (Verbatim semantics from
// FormManager.cpp:96-141.)
bool commitFileToDestination(const QString& candidate, const QString& destPath, QString* err,
                             CommitFaultForTesting fault)
{
    QFile src(candidate);
    if (!src.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("validated candidate became unreadable: %1").arg(src.errorString());
        return false;
    }
    const qint64 expected = src.size();

    // GUI-held-handle coordination: the release runs before the destination is
    // opened for the atomic replacement; the restore runs on EVERY outcome, so
    // a failed commit also leaves the (preserved) file displayed again.
    ScopedFileHandleCoordination coordinationScope(destPath);

    QSaveFile out(destPath);
    if (!out.open(QIODevice::WriteOnly)) {
        if (err) *err = QStringLiteral("cannot open destination for safe write: %1").arg(out.errorString());
        return false;
    }

    qint64 copied = 0;
    char buf[65536];
    while (copied < expected) {
        const qint64 want = qMin<qint64>(static_cast<qint64>(sizeof(buf)), expected - copied);
        const qint64 got = src.read(buf, want);
        if (got <= 0) {
            out.cancelWriting();
            if (err) *err = QStringLiteral("short read from validated candidate");
            return false;
        }
        if (out.write(buf, got) != got) {
            out.cancelWriting();
            if (err) *err = QStringLiteral("cannot write destination bytes: %1").arg(out.errorString());
            return false;
        }
        copied += got;
    }

    if (fault == CommitFaultForTesting::FailBeforeCommit
        || g_commitFaultForTesting == CommitFaultForTesting::FailBeforeCommit) {
        out.cancelWriting();
        if (err) *err = QStringLiteral("injected commit failure (test seam)");
        return false;
    }
    if (!out.commit()) {
        // Open-handle replacement failure lands here: reported as failure, the
        // original destination is byte-identical.
        if (err) *err = QStringLiteral("commit to destination failed: %1").arg(out.errorString());
        return false;
    }
    return true;
}

} // namespace SafeSave
} // namespace gp
