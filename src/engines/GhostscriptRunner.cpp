// SPDX-License-Identifier: Apache-2.0
#include "engines/GhostscriptRunner.h"

#include "engines/SafeSave.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QVersionNumber>
#include <QtGlobal>

namespace gp {
namespace GhostscriptLocator {

QString consoleBinaryName()
{
#ifdef Q_OS_WIN
    // The 64-bit AGPL installer's console binary (gswin32c.exe is only checked
    // as a fallback leg inside a version directory, never on other roots).
    return QStringLiteral("gswin64c.exe");
#else
    return QStringLiteral("gs");
#endif
}

namespace {

// Console binaries checked inside one gs<version> directory, in order.
QStringList consoleBinaryCandidates()
{
#ifdef Q_OS_WIN
    return { QStringLiteral("gswin64c.exe"), QStringLiteral("gswin32c.exe") };
#else
    return { QStringLiteral("gs") };
#endif
}

// "gs10.02.1" -> QVersionNumber(10,0,2,1)-style parse; invalid for anything
// that does not carry a version tail (such directories sort lowest).
QVersionNumber versionOfDirName(const QString& dirName)
{
    if (!dirName.startsWith(QLatin1Char('g'), Qt::CaseInsensitive)
        || !dirName.startsWith(QLatin1String("gs"), Qt::CaseInsensitive))
        return {};
    return QVersionNumber::fromString(dirName.mid(2));
}

// Highest gs<version>/bin/<console> under one root; empty when absent.
QString highestVersionInRoot(const QString& root)
{
    QDir gsRoot(root + QStringLiteral("/gs"));
    if (!gsRoot.exists()) return {};
    const QFileInfoList versions =
        gsRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QString best;
    QVersionNumber bestVersion;
    for (const QFileInfo& vdir : versions) {
        const QVersionNumber v = versionOfDirName(vdir.fileName());
        if (v.isNull()) continue;  // never launch from an unversioned dir
        for (const QString& exe : consoleBinaryCandidates()) {
            const QString candidate =
                vdir.absoluteFilePath() + QStringLiteral("/bin/") + exe;
            if (QFileInfo::exists(candidate)) {
                if (v > bestVersion) {
                    bestVersion = v;
                    best = candidate;
                }
                break;  // one binary per version directory is enough
            }
        }
    }
    return best;
}

} // namespace

QStringList standardInstallRoots()
{
    QStringList roots;
    const QString pf = qEnvironmentVariable("ProgramFiles");
    const QString pf86 = qEnvironmentVariable("ProgramFiles(x86)");
    if (!pf.isEmpty()) roots << pf;
    else roots << QStringLiteral("C:/Program Files");
    if (!pf86.isEmpty()) roots << pf86;
    else roots << QStringLiteral("C:/Program Files (x86)");
    return roots;
}

QString locateForTesting(const QString& rootDir)
{
    if (rootDir.isEmpty()) return {};
    const QString found = highestVersionInRoot(rootDir);
    if (found.isEmpty()) return {};
    return QDir::toNativeSeparators(found);
}

QString locate()
{
    for (const QString& root : standardInstallRoots()) {
        const QString found = highestVersionInRoot(root);
        if (!found.isEmpty())
            return QDir::toNativeSeparators(found);
    }
    return {};
}

QString absenceDisclosure()
{
    QStringList nativeRoots;
    for (const QString& root : standardInstallRoots())
        nativeRoots << QDir::toNativeSeparators(root + QStringLiteral("/gs"));
    return QObject::tr(
        "Unlock is unavailable: Ghostscript was not found on this computer.\n\n"
        "GlyphPDF looks for the Ghostscript console tool (gswin64c.exe) in the "
        "standard installation locations only — %1 — and does not run a copy "
        "from PATH. Ghostscript is not bundled with GlyphPDF (licensing: AGPL).\n\n"
        "Install the AGPL Ghostscript from https://ghostscript.com (any recent "
        "10.x version into the default location) and try again.").arg(
        nativeRoots.join(QStringLiteral("  or  ")));
}

} // namespace GhostscriptLocator

namespace GhostscriptRunner {

QStringList buildUnlockArgs(const QString& candidate, const QString& sourcePdf)
{
    // -dSAFER is MANDATORY (the interpreted document must not gain PostScript
    // file access); -dBATCH -dNOPAUSE make the run terminate on its own; no
    // -dQUIET: the captured output is what failures quote. No -sPDFPassword=
    // EVER: the secret would be on the command line (M-1, CWE-214).
    return {
        QStringLiteral("-dSAFER"),
        QStringLiteral("-dBATCH"),
        QStringLiteral("-dNOPAUSE"),
        QStringLiteral("-sDEVICE=pdfwrite"),
        QStringLiteral("-sOutputFile=%1").arg(candidate),
        sourcePdf,
    };
}

namespace {

// Read the child's merged output as it runs; keep only the TAIL (Ghostscript
// prints one line per page for large documents — the diagnostics that matter
// are at the end). Draining while the child runs also prevents the classic
// full-pipe deadlock the plain waitForFinished idiom can hit on verbose runs.
constexpr int kMaxKeptOutput = 32768;

void appendTail(QByteArray& tail, const QByteArray& chunk)
{
    tail.append(chunk);
    if (tail.size() > kMaxKeptOutput)
        tail.remove(0, tail.size() - kMaxKeptOutput);
}

// Ghostscript's own diagnostics carry the failure signature that the EXIT
// CODE does not: the C-based PDF interpreter (pdfi, default since 9.55) exits
// 0 even when it could not open an encrypted document — it prints
//   "**** This file requires a password for access." and
//   "No pages will be processed (FirstPage > LastPage)."
// and pdfwrite still emits a BLANK single-page PDF. Committing that would
// silently replace the document with an empty husk, so a run whose output
// reports zero processed pages FAILS here regardless of the exit code
// (verified against AGPL Ghostscript 10.08.0 — see
// docs/audit/LANE-REPORT-gs-unlock-2026-10-04.md §empirics). The same
// signature honestly fails hopeless corruption, while genuinely REPAIRABLE
// documents ("…errors that were repaired or ignored" + pages processed) still
// pass — the repair mission is preserved by NOT running with -dPDFSTOPONERROR
// (verified: STOPONERROR exits 1 on the repairable broken-xref case too).
constexpr const char* kNoPagesProcessedMarker =
    "No pages will be processed";
constexpr const char* kPasswordRequiredMarker =
    "requires a password";

QString lastOutputLines(const QByteArray& tail, int maxLines = 12)
{
    const QList<QByteArray> lines = tail.trimmed().split('\n');
    QStringList kept;
    for (int i = lines.size() - 1; i >= 0 && kept.size() < maxLines; --i)
        kept.prepend(QString::fromLocal8Bit(lines.at(i)).trimmed());
    return kept.join(QStringLiteral("\n"));
}

struct BoundedRunResult {
    bool finished = false;   // exited normally (exitCode set)
    bool canceled = false;
    int exitCode = -1;
    QString error;           // user-presentable when !finished
    QByteArray outputTail;   // merged stdout+stderr tail (tool diagnostics)
};

// One bounded, cancellable, owned process run with merged, drained output and
// an optional stdin payload. Modeled on SafeSave::runBoundedProcess; differs
// ONLY in output capture (needed so Ghostscript failures can quote the tool's
// own words) — the lifecycle is the idiom's: launch failure, cancel and
// timeout each leave NOTHING running that this call owns.
BoundedRunResult runBoundedMerged(const QString& program, const QStringList& args,
                                  qint64 timeoutMs,
                                  const std::function<bool()>& isCanceled,
                                  const QByteArray& stdinData)
{
    BoundedRunResult r;
    QProcess proc;
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start(program, args);
    if (!proc.waitForStarted(10000)) {
        r.error = QObject::tr("could not start the Ghostscript tool '%1': %2")
                      .arg(program, proc.errorString());
        return r;
    }
    // M-1 belt-and-braces: the password (when one is in play) is ALSO written
    // to the child's stdin with the channel closed. The pdfi interpreter does
    // not read a piped prompt reply today (verified against 10.08.0 — the
    // working off-argv channel is the -sPDFPassword response file below), but
    // a future build that prompts on the console would read its reply here.
    // The secret never appears in argv either way.
    if (!stdinData.isEmpty()) {
        proc.write(stdinData);
        proc.closeWriteChannel();
    }
    const qint64 pollMs = 100;
    qint64 waited = 0;
    while (!proc.waitForFinished(static_cast<int>(pollMs))) {
        appendTail(r.outputTail, proc.readAll());
        if (isCanceled && isCanceled()) {
            proc.kill();
            proc.waitForFinished(5000);
            r.canceled = true;
            r.error = QObject::tr("canceled by the user");
            return r;
        }
        waited += pollMs;
        if (waited >= timeoutMs) {
            proc.kill();
            proc.waitForFinished(5000);
            r.error = QObject::tr("Ghostscript did not finish within %1 s")
                          .arg(timeoutMs / 1000);
            return r;
        }
    }
    appendTail(r.outputTail, proc.readAll());
    if (proc.exitStatus() != QProcess::NormalExit) {
        r.error = QObject::tr("the Ghostscript tool crashed");
        return r;
    }
    r.exitCode = proc.exitCode();
    r.finished = true;
    return r;
}

bool candidateLooksLikePdf(const QString& path)
{
    QFileInfo info(path);
    if (!info.exists() || info.size() <= 0) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray head = f.read(5);
    f.close();
    return head == "%PDF-";
}

// The -sPDFPassword response file: the ONE channel that delivers the user
// password to the pdfi interpreter WITHOUT putting the secret on the command
// line (M-1, CWE-214). Ghostscript expands arguments from `@file` — argv
// carries only the @path. The file lives in the owner-only candidates staging
// dir (SafeSave::makeUniqueCandidate, 0600 — the same protection the
// in-flight document bytes get) and is deleted as soon as the run ends.
// Token format verified against 10.08.0: a double-quoted token survives
// spaces AND backslashes in the password; an embedded double quote cannot be
// expressed (such a password simply fails again in pass 2 — honestly).
bool writePasswordResponseFile(const QString& path, const QByteArray& userPassword)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray quoted = QByteArrayLiteral("-sPDFPassword=\"")
        + userPassword + QByteArrayLiteral("\"") + '\n';
    const bool ok = f.write(quoted) == quoted.size();
    f.close();
    return ok;
}

bool outputSaysNoPagesProcessed(const QByteArray& outputTail)
{
    return outputTail.contains(kNoPagesProcessedMarker);
}

bool outputSaysPasswordRequired(const QByteArray& outputTail)
{
    return outputTail.contains(kPasswordRequiredMarker);
}

// One full pass (password-less or response-file), validation included. The
// password response file (when `responseFile` is non-empty) is expanded by the
// tool from `@<path>` — argv never carries the secret.
UnlockResult runOnePass(const QString& program, const QString& candidate,
                        const QString& sourcePdf, const QString& responseFile,
                        const QByteArray& stdinData,
                        qint64 timeoutMs, const std::function<bool()>& isCanceled)
{
    UnlockResult r;
    r.usedPassword = !responseFile.isEmpty();

    QStringList args = buildUnlockArgs(candidate, sourcePdf);
    if (!responseFile.isEmpty())
        args.prepend(QLatin1Char('@') + responseFile);

    const BoundedRunResult run = runBoundedMerged(
        program, args, timeoutMs, isCanceled, stdinData);
    r.canceled = run.canceled;
    r.exitCode = run.exitCode;
    if (!run.finished) {
        r.stage = UnlockResult::Stage::Launch;
        r.error = run.error;
        return r;
    }
    const bool noPages = outputSaysNoPagesProcessed(run.outputTail);
    if (run.exitCode != 0 || noPages) {
        r.stage = UnlockResult::Stage::Tool;
        if (noPages && outputSaysPasswordRequired(run.outputTail)) {
            r.error = QObject::tr("the document requires its user password to "
                                  "be opened — Ghostscript reported:\n%1")
                          .arg(lastOutputLines(run.outputTail));
        } else if (noPages) {
            r.error = QObject::tr("Ghostscript could not process any page of "
                                  "this document (it is damaged beyond "
                                  "re-distillation). Ghostscript reported:\n%1")
                          .arg(lastOutputLines(run.outputTail));
        } else {
            r.error = QObject::tr("Ghostscript failed (exit code %1):\n%2")
                          .arg(run.exitCode)
                          .arg(run.outputTail.trimmed().isEmpty()
                                   ? QObject::tr("(no diagnostic output)")
                                   : lastOutputLines(run.outputTail));
        }
        return r;
    }
    if (!candidateLooksLikePdf(candidate)) {
        r.stage = UnlockResult::Stage::ValidateCandidate;
        r.error = QObject::tr("Ghostscript exited successfully but did not "
                              "produce a readable PDF.");
        return r;
    }
    r.stage = UnlockResult::Stage::None;  // pass succeeded; commit follows
    return r;
}

} // namespace

UnlockResult unlockPdf(const QString& program,
                       const QString& sourcePdf,
                       const QString& destinationPdf,
                       const QByteArray& userPassword,
                       qint64 timeoutMs,
                       const std::function<bool()>& isCanceled)
{
    UnlockResult r;
    if (sourcePdf.isEmpty() || !QFileInfo::exists(sourcePdf)) {
        r.error = QObject::tr("the source document does not exist");
        return r;
    }
    if (destinationPdf.isEmpty()) {
        r.error = QObject::tr("no destination was given");
        return r;
    }
    if (timeoutMs <= 0) timeoutMs = 180000;  // bounded; never unbounded

    // SafeSave external-writer transaction: Ghostscript writes the CANDIDATE
    // it owns; the destination is never a process argument and is only
    // replaced by the checked atomic commit after validation.
    QString err;
    QString candidate;
    if (!SafeSave::makeUniqueCandidate(&candidate, &err)) {
        r.stage = UnlockResult::Stage::Candidate;
        r.error = err;
        return r;
    }
    // The reservation staked out a unique NAME; drop our 0-byte placeholder so
    // the external writer owns the file from its first byte.
    QFile::remove(candidate);

    // Pass 1 — always WITHOUT the password (most protected documents are
    // owner-password-only and open with the empty user password).
    UnlockResult pass = runOnePass(program, candidate, sourcePdf, {}, {}, timeoutMs, isCanceled);
    if (pass.stage == UnlockResult::Stage::Tool
        && !pass.canceled
        && !userPassword.isEmpty()) {
        // Pass 2 — the source needs its user password. The secret travels in
        // the -sPDFPassword RESPONSE FILE (owner-only staging, deleted right
        // after the run; argv carries only `@path`) and is also piped to the
        // child's stdin for prompt-reading builds. NEVER on the command line.
        QString rspErr;
        QString responseFile;
        if (!SafeSave::makeUniqueCandidate(&responseFile, &rspErr,
                                           QStringLiteral(".gspwrsp"))) {
            r.stage = UnlockResult::Stage::Candidate;
            r.error = rspErr;
            QFile::remove(candidate);
            return r;
        }
        if (!writePasswordResponseFile(responseFile, userPassword)) {
            r.stage = UnlockResult::Stage::Candidate;
            r.error = QObject::tr("could not stage the password response file");
            QFile::remove(responseFile);
            QFile::remove(candidate);
            return r;
        }
        QFile::remove(candidate);
        pass = runOnePass(program, candidate, sourcePdf, responseFile,
                          userPassword + '\n', timeoutMs, isCanceled);
        // The response file holds the secret: gone the moment the run ends,
        // on every path.
        QFile::remove(responseFile);
    }
    r.canceled = pass.canceled;
    r.exitCode = pass.exitCode;
    r.usedPassword = pass.usedPassword;
    if (pass.stage != UnlockResult::Stage::None) {
        r.stage = pass.stage;
        r.error = pass.error;
        QFile::remove(candidate);
        return r;
    }

    // Destination-identity precondition (emergence E-6): for an in-place
    // unlock (destination == source) a second writer committing the same
    // document mid-run must never be silently erased by our commit.
    const SafeSave::DestinationIdentity expected =
        SafeSave::captureDestinationIdentity(destinationPdf);

    if (!SafeSave::commitFileToDestination(candidate, destinationPdf, &err,
                                           SafeSave::CommitFaultForTesting::None,
                                           expected)) {
        r.stage = UnlockResult::Stage::Commit;
        r.error = err;
        QFile::remove(candidate);
        return r;
    }
    QFile::remove(candidate);
    r.ok = true;
    r.outputPath = destinationPdf;
    return r;
}

} // namespace GhostscriptRunner
} // namespace gp
