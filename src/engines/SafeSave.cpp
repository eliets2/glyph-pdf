// SPDX-License-Identifier: Apache-2.0
#include "engines/SafeSave.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace gp {
namespace SafeSave {

namespace {
// ── M-4 (AUDIT-SECURITY-2026-09-25, CWE-377): owner-only candidate staging ───
// The candidates directory lives in the SHARED system-temp root (deliberately
// outside TempFileManager's per-instance session dir: candidates are
// cross-process-visible on purpose, e.g. killed-run debris scans). On
// multi-user POSIX systems mkpath's default (0755, umask-dependent) let any
// local user read the in-flight candidate — the full document bytes — while
// the owning operation ran. The dir is hardened to 0700 and every candidate
// file to 0600. Windows needs no bit work: %TEMP% carries per-user ACLs and
// QFile::setPermissions is a read-only-attribute no-op there, so it is
// attempted but never relied on. POSIX verification is exact: a hardening
// that does not stick fails closed (no candidate is staged at all).
constexpr QFile::Permissions kOwnerOnlyFile = QFile::ReadOwner | QFile::WriteOwner;
constexpr QFile::Permissions kOwnerOnlyDir  = QFile::ReadOwner | QFile::WriteOwner
                                            | QFile::ExeOwner;
constexpr QFile::Permissions kGroupOtherFile = QFile::ReadGroup | QFile::WriteGroup
                                              | QFile::ReadOther | QFile::WriteOther;
constexpr QFile::Permissions kGroupOtherDir  = kGroupOtherFile | QFile::ExeGroup
                                              | QFile::ExeOther;

bool hardenDirOwnerOnly(const QString& path)
{
    QDir d(path);
    if (!d.exists() && !QDir().mkpath(path)) return false;
#ifdef Q_OS_UNIX
    if (!QFile::setPermissions(path, kOwnerOnlyDir)) return false;
    const QFile::Permissions now = QFile::permissions(path);
    return (now & kOwnerOnlyDir) == kOwnerOnlyDir && (now & kGroupOtherDir) == 0;
#else
    QFile::setPermissions(path, kOwnerOnlyDir);  // best-effort on Windows ACLs
    return true;
#endif
}

bool hardenFileOwnerOnly(const QString& path)
{
#ifdef Q_OS_UNIX
    if (!QFile::setPermissions(path, kOwnerOnlyFile)) return false;
    const QFile::Permissions now = QFile::permissions(path);
    return (now & kOwnerOnlyFile) == kOwnerOnlyFile && (now & kGroupOtherFile) == 0;
#else
    QFile::setPermissions(path, kOwnerOnlyFile);  // best-effort on Windows ACLs
    return true;
#endif
}

// Deterministic test seam (see setCommitFaultForTesting). Mirrors the
// FormManager SaveFault injection point: the failure fires AFTER the bounded
// copy, before QSaveFile::commit(), so the cancelWriting path is exercised.
CommitFaultForTesting g_commitFaultForTesting = CommitFaultForTesting::None;

// GUI-held-handle coordinator (see SafeSave.h). Set once by the shell at
// startup; reading the pair is otherwise immutable, so no locking is needed.
FileHandleGuard g_releaseFileHandles;
FileHandleGuard g_restoreFileHandles;

// Full content hash of the destination's current bytes; empty when the file
// cannot be read (absent or unreadable — both are "no expectation").
QByteArray destinationSha256(const QString& destPath)
{
    QFile f(destPath);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return hash.result();
}
} // namespace

// ── emergence E-6: what the destination holds when the operation STARTS. ─────
DestinationIdentity captureDestinationIdentity(const QString& destPath)
{
    DestinationIdentity id;
    if (destPath.isEmpty() || !QFileInfo::exists(destPath)) return id;
    const QByteArray sha = destinationSha256(destPath);
    if (sha.isEmpty()) return id;
    id.valid = true;
    id.sha256 = sha;
    return id;
}

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
    // M-4 (CWE-377): owner-only BEFORE anything is staged — a world-
    // traversable staging dir publishes every in-flight document to all
    // local users on POSIX. Fail closed: no private staging, no candidate.
    if (!hardenDirOwnerOnly(candidateDir.absolutePath())) {
        if (err) *err = QStringLiteral("could not secure the candidate staging "
                                       "directory %1 to owner-only access — "
                                       "refusing to stage a document there")
                            .arg(candidateDir.absolutePath());
        return false;
    }
    QTemporaryFile tmp(candidateDir.absoluteFilePath(QStringLiteral("glyphpdf-XXXXXX") + suffix));
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        if (err) *err = QStringLiteral("could not create unique candidate PDF: %1").arg(tmp.errorString());
        return false;
    }
    *out = tmp.fileName();
    tmp.close();
    // M-4: pin the reservation to 0600 explicitly (QTemporaryFile already
    // creates owner-only on POSIX; the pin survives default changes and
    // documents the invariant). Fail closed as well.
    if (!hardenFileOwnerOnly(*out)) {
        QFile::remove(*out);
        if (err) *err = QStringLiteral("could not restrict the candidate %1 to "
                                       "owner-only access — refusing to stage "
                                       "a document there").arg(*out);
        out->clear();
        return false;
    }
    return true;
}

// ── WP-R04 external-writer transaction ──────────────────────────────────────

// PARITY-SCORECARD-2026-09-30 §4 row 14 — see the header note. App-owned
// (vendored) 7-Zip ONLY, empty on true absence (the caller discloses
// honestly). The PATH and Program-Files fallback legs were removed at the
// wave-2b security audit (F-02, CWE-427): a planted 7z.exe on PATH or in a
// conventional install dir would receive the document bytes AND the package
// password with no hash verification — silently reintroducing the
// external-binary dependency the vendoring removed. CMake stages the pinned
// bundle beside the app and every test executable, so the bundled branch is
// the only legitimate resolution. Requires BOTH 7z.exe and 7z.dll: 7z.exe is
// only a launcher.
QString locateSevenZip(const QString& appDirOverride) {
    const QString appDir = appDirOverride.isEmpty()
        ? QCoreApplication::applicationDirPath() : appDirOverride;
    if (!appDir.isEmpty()) {
        const QString bundled = appDir + QStringLiteral("/7z.exe");
        if (QFileInfo::exists(bundled)
            && QFileInfo::exists(appDir + QStringLiteral("/7z.dll")))
            return QDir::toNativeSeparators(bundled);
    }
    return {};
}

// One bounded, cancellable process run owned by the caller. Returns true when
// the process exited normally with exitCode set; false with `error` (and
// `canceled`) when it could not start, was canceled, or hit the deadline. A
// cancel/timeout always KILLS the process we own before returning.
bool runBoundedProcess(const QString& program, const QStringList& args,
                       qint64 timeoutMs, const std::function<bool()>& isCanceled,
                       bool* canceled, int* exitCode, QString* error,
                       const QByteArray& stdinData)
{
    *canceled = false;
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForStarted(10000)) {
        if (error) *error = QStringLiteral("could not start the external tool: %1").arg(proc.errorString());
        return false;
    }
    // M-1 (CWE-214): deliver a secret (e.g. the 7-Zip `-p` prompt reply) via
    // the stdin pipe and close the channel — the child reads its prompt reply
    // from the pipe and sees EOF, so nothing waits on the channel. The secret
    // never appears in argv.
    if (!stdinData.isEmpty()) {
        proc.write(stdinData);
        proc.closeWriteChannel();
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
    const ExternalWriteValidateFn& validateCandidate,
    const QByteArray& procStdin)
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
    if (!runBoundedProcess(program, args, timeoutMs, isCanceled, &canceled, &exitCode, &err, procStdin)) {
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

    // M-4 (CWE-377): the tool recreated the candidate after we dropped our
    // 0600 reservation, so on POSIX its own umask (typically 022) left the
    // document bytes 0644 in the staging dir for the whole validate+commit
    // window. Re-harden BEFORE anything reads it.
    if (!hardenFileOwnerOnly(candidate)) {
        r.stage = ExternalWriteResult::Stage::ValidateCandidate;
        r.error = QStringLiteral("could not restrict the external tool's "
                                 "candidate to owner-only access");
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
                             CommitFaultForTesting fault, const DestinationIdentity& expected)
{
    QFile src(candidate);
    if (!src.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("validated candidate became unreadable: %1").arg(src.errorString());
        return false;
    }
    const qint64 expectedSize = src.size();

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
    while (copied < expectedSize) {
        const qint64 want = qMin<qint64>(static_cast<qint64>(sizeof(buf)), expectedSize - copied);
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

    // ── emergence E-6: the destination-identity precondition, checked at the
    // LAST possible moment before the atomic rename (the copy window above can
    // span seconds for large documents — exactly the window a second writer's
    // commit lands in). A divergence here means the bytes this operation
    // started from no longer exist on disk; committing would silently replace
    // them with a serialization of stale state. Nothing is overwritten: the
    // refusal leaves the OTHER writer's bytes in place and says so.
    if (expected.valid) {
        const QByteArray current = destinationSha256(destPath);
        if (current.isEmpty()) {
            out.cancelWriting();
            if (err) *err = QStringLiteral("the document changed on disk while this operation "
                                           "was running (it is no longer readable where it "
                                           "was) — nothing was overwritten; reload before "
                                           "saving.");
            return false;
        }
        if (current != expected.sha256) {
            out.cancelWriting();
            if (err) *err = QStringLiteral("the document changed on disk while this operation "
                                           "was running (another window or process saved it) — "
                                           "nothing was overwritten; review the current file, "
                                           "then reload or use Save As.");
            return false;
        }
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
