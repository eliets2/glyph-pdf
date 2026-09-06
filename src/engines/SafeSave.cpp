// SPDX-License-Identifier: Apache-2.0
#include "engines/SafeSave.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTemporaryFile>

namespace gp {
namespace SafeSave {

namespace {
// Deterministic test seam (see setCommitFaultForTesting). Mirrors the
// FormManager SaveFault injection point: the failure fires AFTER the bounded
// copy, before QSaveFile::commit(), so the cancelWriting path is exercised.
CommitFaultForTesting g_commitFaultForTesting = CommitFaultForTesting::None;
} // namespace

// Reserve a unique candidate path in the system temp dir. The handle is
// released before any writer produces the candidate so the writer owns the
// file exclusively. (Verbatim semantics from FormManager.cpp:79-90.)
bool makeUniqueCandidate(QString* out, QString* err)
{
    // Dedicated subdirectory: keeps the operation's candidates isolated from
    // the shared temp root, so tests can assert "nothing left behind" without
    // cross-process debris (killed runs, concurrent lanes) polluting the scan.
    QDir candidateDir(QDir::tempPath() + QStringLiteral("/glyphpdf-candidates"));
    if (!candidateDir.exists()) QDir().mkpath(candidateDir.absolutePath());
    QTemporaryFile tmp(candidateDir.absoluteFilePath(QStringLiteral("glyphpdf-XXXXXX.pdf")));
    tmp.setAutoRemove(false);
    if (!tmp.open()) {
        if (err) *err = QStringLiteral("could not create unique candidate PDF: %1").arg(tmp.errorString());
        return false;
    }
    *out = tmp.fileName();
    tmp.close();
    return true;
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
