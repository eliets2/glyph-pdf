// SPDX-License-Identifier: Apache-2.0
// WP-R04 (P1) regression suite — safe encrypted-package replacement and
// external-helper lifecycle.
//
// WHOLE-ARCHITECTURE-REVIEW-2026-09-10 A03: the Share → Create encrypted
// package flow DELETED the destination before launching 7-Zip and let the
// tool write the FINAL path under waitForFinished(-1) — a failed launch, a
// failed tool run, or a mid-write exit destroyed the previous package, and
// cancellation was impossible.
//
// The repair boundary is gp::SafeSave::runExternalWriterCommit
// (src/engines/SafeSave.h): unique owned candidate → external tool writes the
// CANDIDATE → validate (readability + caller read-back) → checked atomic
// replace through commitFileToDestination. The destination is never removed,
// truncated or written before commit; cancel/timeout KILL the process we own;
// the wait is bounded.
//
// These tests drive the REAL transaction with a re-exec'd fake writer (this
// test binary itself, --r04-fake-writer mode) for deterministic launch-
// failure, tool-failure, partial-output, hang/cancel/timeout and commit-fault
// behavior, plus a REAL 7-Zip end-to-end leg (skipped honestly when 7z is not
// installed) asserting the A03 acceptance gate: the previous archive's
// SHA-256 is unchanged in every failed case, and a successful package opens
// with the chosen password and contains exactly the intended input.
#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <atomic>
#include <memory>
#include <thread>

#include "engines/SafeSave.h"

using gp::SafeSave::ExternalWriteResult;
using gp::SafeSave::runExternalWriterCommit;

// ── fake-writer child mode ───────────────────────────────────────────────────
// argv: <exe> --r04-fake-writer <mode> <candidatePath>
//   ok      — write marker bytes to the candidate, exit 0
//   fail    — exit 3 without producing any output
//   partial — write marker bytes to the candidate, then exit 3
//   hang    — write marker bytes to the candidate, then sleep until killed
static QByteArray fakeWriterBytes() { return QByteArrayLiteral("R04-FAKE-ARCHIVE-BYTES"); }

static int fakeWriterMain(const QString& mode, const QString& candidate)
{
    if (mode == QLatin1String("fail"))
        return 3;
    QFile out(candidate);
    if (!out.open(QIODevice::WriteOnly))
        return 4;
    out.write(fakeWriterBytes());
    out.close();
    if (mode == QLatin1String("partial"))
        return 3;
    if (mode == QLatin1String("hang")) {
        // Sleep until the owning transaction kills us.
        for (;;) {
            QThread::msleep(100);
        }
    }
    return 0;  // "ok"
}

// ── helpers ──────────────────────────────────────────────────────────────────
static QString candidatesDir() { return QDir::tempPath() + QStringLiteral("/glyphpdf-candidates"); }

static int candidateFileCount()
{
    return QDir(candidatesDir()).entryList({"glyphpdf-*"}, QDir::Files).size();
}

static QByteArray sha256OfFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return hash.result();
}

static QByteArray sha256OfBytes(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

static const QByteArray kSentinel = QByteArrayLiteral("PREVIOUS-SENTINEL-ARCHIVE-v1");

// Writes the sentinel "previous package" to `path` and returns its SHA-256.
static QByteArray plantSentinel(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        qFatal("plantSentinel: cannot write fixture %s", qPrintable(path));
    f.write(kSentinel);
    f.close();
    return sha256OfFile(path);
}

// A buildArgs callback mirroring the production 7z invocation shape, but
// pointing at the re-exec'd fake writer.
static gp::SafeSave::ExternalWriteArgsFn fakeWriterArgs(const QString& mode)
{
    const QString self = QCoreApplication::applicationFilePath();
    return [self, mode](const QString& candidate) {
        return QStringList{ QStringLiteral("--r04-fake-writer"), mode, candidate };
    };
}

class TestEncryptedPackageSafeWrite : public QObject {
    Q_OBJECT

    QTemporaryDir m_work;

    QString destPath() const { return m_work.filePath(QStringLiteral("package.zip")); }

private slots:
    void initTestCase() {
        QVERIFY(m_work.isValid());
    }

    // A tool that cannot start must leave the previous package byte-identical.
    void launchFailurePreservesExistingPackage() {
        const QString dest = destPath();
        const QByteArray before = plantSentinel(dest);
        const int beforeCandidates = candidateFileCount();

        ExternalWriteResult r = runExternalWriterCommit(
            QStringLiteral("Z:/nonexistent/r04/no-such-tool.exe"),
            fakeWriterArgs(QStringLiteral("ok")), dest, QStringLiteral(".zip"),
            30000);
        QVERIFY(!r.ok);
        QCOMPARE(r.stage, ExternalWriteResult::Stage::Launch);
        QCOMPARE(sha256OfFile(dest), before);              // old package byte-identical
        QCOMPARE(candidateFileCount(), beforeCandidates);  // no candidate left behind
    }

    // A tool that starts and fails must leave the previous package intact.
    void toolFailurePreservesExistingPackage() {
        const QString dest = destPath();
        const QByteArray before = plantSentinel(dest);
        const int beforeCandidates = candidateFileCount();

        ExternalWriteResult r = runExternalWriterCommit(
            QCoreApplication::applicationFilePath(),
            fakeWriterArgs(QStringLiteral("fail")), dest, QStringLiteral(".zip"),
            30000);
        QVERIFY(!r.ok);
        QCOMPARE(r.stage, ExternalWriteResult::Stage::Tool);
        QCOMPARE(r.exitCode, 3);
        QCOMPARE(sha256OfFile(dest), before);
        QCOMPARE(candidateFileCount(), beforeCandidates);
    }

    // A tool that produced a candidate BUT exited nonzero must not have that
    // (unvalidated) candidate committed over the destination.
    void partialOutputIsNotCommitted() {
        const QString dest = destPath();
        const QByteArray before = plantSentinel(dest);

        ExternalWriteResult r = runExternalWriterCommit(
            QCoreApplication::applicationFilePath(),
            fakeWriterArgs(QStringLiteral("partial")), dest, QStringLiteral(".zip"),
            30000);
        QVERIFY(!r.ok);
        QCOMPARE(r.stage, ExternalWriteResult::Stage::Tool);
        QCOMPARE(sha256OfFile(dest), before);   // old package survives, candidate refused
    }

    // Candidate validation (the 7z read-back in production) refusing the
    // candidate must preserve the previous package.
    void validatorRefusalPreservesExistingPackage() {
        const QString dest = destPath();
        const QByteArray before = plantSentinel(dest);

        ExternalWriteResult r = runExternalWriterCommit(
            QCoreApplication::applicationFilePath(),
            fakeWriterArgs(QStringLiteral("ok")), dest, QStringLiteral(".zip"),
            30000, {},
            [](const QString&) { return QStringLiteral("read-back refused (test)"); });
        QVERIFY(!r.ok);
        QCOMPARE(r.stage, ExternalWriteResult::Stage::ValidateCandidate);
        QCOMPARE(sha256OfFile(dest), before);
    }

    // The deterministic commit-fault seam (the same one TestEngineSave uses)
    // must leave the destination byte-identical — the "failed destination
    // commit" leg of the A03 acceptance gate.
    void injectedCommitFaultPreservesExistingPackage() {
        const QString dest = destPath();
        const QByteArray before = plantSentinel(dest);
        const int beforeCandidates = candidateFileCount();

        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::FailBeforeCommit);
        ExternalWriteResult r = runExternalWriterCommit(
            QCoreApplication::applicationFilePath(),
            fakeWriterArgs(QStringLiteral("ok")), dest, QStringLiteral(".zip"),
            30000);
        gp::SafeSave::setCommitFaultForTesting(
            gp::SafeSave::CommitFaultForTesting::None);
        QVERIFY(!r.ok);
        QCOMPARE(r.stage, ExternalWriteResult::Stage::Commit);
        QCOMPARE(sha256OfFile(dest), before);          // byte-identical through the fault
        QCOMPARE(candidateFileCount(), beforeCandidates);
    }

    // Success: only a validated candidate replaces the destination.
    void successReplacesDestinationWithCandidateBytes() {
        const QString dest = destPath();
        const QByteArray before = plantSentinel(dest);
        const int beforeCandidates = candidateFileCount();

        ExternalWriteResult r = runExternalWriterCommit(
            QCoreApplication::applicationFilePath(),
            fakeWriterArgs(QStringLiteral("ok")), dest, QStringLiteral(".zip"),
            30000);
        QVERIFY(r.ok);
        QVERIFY(r.error.isEmpty());
        QVERIFY(r.candidatePath.isEmpty());      // candidate cleaned up
        QCOMPARE(sha256OfFile(dest), sha256OfBytes(fakeWriterBytes()));
        QVERIFY(sha256OfFile(dest) != before);   // the new package IS in place
        QCOMPARE(candidateFileCount(), beforeCandidates);
    }

    // Cancel while the (hung) writer runs: the process we own is killed, the
    // wait terminates, and the previous package survives untouched.
    void cancelKillsWriterAndPreservesExistingPackage() {
        const QString dest = destPath();
        const QByteArray before = plantSentinel(dest);
        const int beforeCandidates = candidateFileCount();

        auto cancel = std::make_shared<std::atomic<bool>>(false);
        ExternalWriteResult r;
        QElapsedTimer clock;
        clock.start();
        std::thread worker([&] {
            r = runExternalWriterCommit(
                QCoreApplication::applicationFilePath(),
                fakeWriterArgs(QStringLiteral("hang")), dest, QStringLiteral(".zip"),
                120000, [&] { return cancel->load(); });
        });

        // Deterministic mid-flight cancel: wait (bounded) until the writer has
        // produced the candidate, then cancel. No fixed-timing assumptions.
        bool sawCandidate = false;
        while (clock.elapsed() < 20000) {
            const auto files = QDir(candidatesDir())
                                   .entryList({"glyphpdf-*.zip"}, QDir::Files);
            for (const QString& f : files) {
                QFileInfo fi(candidatesDir() + QLatin1Char('/') + f);
                if (fi.size() > 0) { sawCandidate = true; break; }
            }
            if (sawCandidate) break;
            QThread::msleep(25);
        }
        QVERIFY(sawCandidate);                    // the writer is mid-flight
        cancel->store(true);
        worker.join();                            // bounded — the kill works
        QVERIFY(clock.elapsed() < 60000);         // far short of the 120 s tool deadline

        QVERIFY(!r.ok);
        QVERIFY(r.canceled);
        QCOMPARE(sha256OfFile(dest), before);     // old package byte-identical
        QCOMPARE(candidateFileCount(), beforeCandidates);
    }

    // A hung writer cannot hang the operation: the deadline kills it.
    void hungWriterIsBoundedByDeadline() {
        const QString dest = destPath();
        const QByteArray before = plantSentinel(dest);

        QElapsedTimer clock;
        clock.start();
        ExternalWriteResult r = runExternalWriterCommit(
            QCoreApplication::applicationFilePath(),
            fakeWriterArgs(QStringLiteral("hang")), dest, QStringLiteral(".zip"),
            2000);
        const qint64 elapsed = clock.elapsed();
        QVERIFY(!r.ok);
        QVERIFY(!r.canceled);
        QVERIFY(elapsed < 30000);                 // bounded (2 s deadline + kill)
        QVERIFY(r.error.contains(QStringLiteral("did not finish")));
        QCOMPARE(sha256OfFile(dest), before);
    }

    // ── REAL 7-Zip end-to-end (A03 acceptance gate) ─────────────────────────
    // The sentinel archive survives a failing real 7z run; a successful run
    // replaces it with an archive that opens with the chosen password, never
    // with the wrong one, and contains exactly the intended input.
    void realSevenZipEndToEnd() {
        QString sevenZip = QStandardPaths::findExecutable(QStringLiteral("7z"));
        if (sevenZip.isEmpty()) {
            const QStringList cands{
                QStringLiteral("C:/Program Files/7-Zip/7z.exe"),
                QStringLiteral("C:/Program Files (x86)/7-Zip/7z.exe")};
            for (const QString& c : cands)
                if (QFileInfo::exists(c)) { sevenZip = c; break; }
        }
        if (sevenZip.isEmpty())
            QSKIP("7z not installed on this machine — the lifecycle legs above carry the regression");

        const QString input = m_work.filePath(QStringLiteral("report.pdf"));
        const QByteArray inputBytes =
            QByteArrayLiteral("%PDF-1.6\n%R04 fixture payload bytes\n");
        {
            QFile f(input);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(inputBytes);
        }

        auto readBack = [sevenZip](const QString& candidate,
                                   const QString& password) -> QString {
            bool canceled = false; int exitCode = -1; QString err;
            const bool okRun = gp::SafeSave::runBoundedProcess(
                sevenZip,
                QStringList{ QStringLiteral("t"), QStringLiteral("-p") + password,
                             QDir::toNativeSeparators(candidate) },
                60000, {}, &canceled, &exitCode, &err);
            if (!okRun || exitCode != 0)
                return QStringLiteral("read-back failed (exit %1)").arg(exitCode);
            return {};
        };

        // Failure leg: a nonexistent input makes real 7z fail after it would
        // have been invoked — the sentinel must be untouched (the pre-fix
        // flow had already DELETED it at exactly this point).
        const QString dest = m_work.filePath(QStringLiteral("existing.zip"));
        const QByteArray before = plantSentinel(dest);
        ExternalWriteResult failRun = runExternalWriterCommit(
            sevenZip,
            [input](const QString& candidate) {
                return QStringList{ QStringLiteral("a"), QStringLiteral("-tzip"),
                                    QStringLiteral("-mem=AES256"), QStringLiteral("-pR04pass"),
                                    QDir::toNativeSeparators(candidate),
                                    QDir::toNativeSeparators(input + QStringLiteral(".missing")) };
            },
            dest, QStringLiteral(".zip"), 60000, {},
            [&readBack](const QString& c) { return readBack(c, QStringLiteral("R04pass")); });
        QVERIFY(!failRun.ok);
        QCOMPARE(sha256OfFile(dest), before);     // THE A03 gate: sentinel survives

        // Validation-refusal leg: 7z succeeds but encrypted with a DIFFERENT
        // password than the read-back checks — the candidate must be refused
        // and the sentinel preserved.
        ExternalWriteResult mismatchRun = runExternalWriterCommit(
            sevenZip,
            [input](const QString& candidate) {
                return QStringList{ QStringLiteral("a"), QStringLiteral("-tzip"),
                                    QStringLiteral("-mem=AES256"), QStringLiteral("-pOTHER"),
                                    QDir::toNativeSeparators(candidate),
                                    QDir::toNativeSeparators(input) };
            },
            dest, QStringLiteral(".zip"), 60000, {},
            [&readBack](const QString& c) { return readBack(c, QStringLiteral("R04pass")); });
        QVERIFY(!mismatchRun.ok);
        QCOMPARE(mismatchRun.stage, ExternalWriteResult::Stage::ValidateCandidate);
        QCOMPARE(sha256OfFile(dest), before);

        // Success leg: correct package replaces the sentinel; opens with the
        // chosen password; rejects the wrong one; content == input exactly.
        ExternalWriteResult okRun = runExternalWriterCommit(
            sevenZip,
            [input](const QString& candidate) {
                return QStringList{ QStringLiteral("a"), QStringLiteral("-tzip"),
                                    QStringLiteral("-mem=AES256"), QStringLiteral("-pR04pass"),
                                    QDir::toNativeSeparators(candidate),
                                    QDir::toNativeSeparators(input) };
            },
            dest, QStringLiteral(".zip"), 60000, {},
            [&readBack](const QString& c) { return readBack(c, QStringLiteral("R04pass")); });
        QVERIFY(okRun.ok);
        QVERIFY(sha256OfFile(dest) != before);

        int exitCode = -1; bool canceled = false; QString err;
        QVERIFY(gp::SafeSave::runBoundedProcess(sevenZip,
            QStringList{ QStringLiteral("t"), QStringLiteral("-pR04pass"),
                         QDir::toNativeSeparators(dest) },
            60000, {}, &canceled, &exitCode, &err));
        QCOMPARE(exitCode, 0);                    // opens with the chosen password
        QVERIFY(gp::SafeSave::runBoundedProcess(sevenZip,
            QStringList{ QStringLiteral("t"), QStringLiteral("-pWRONG"),
                         QDir::toNativeSeparators(dest) },
            60000, {}, &canceled, &exitCode, &err));
        QCOMPARE(exitCode, 2);                    // wrong password is REJECTED

        QProcess extract;
        extract.start(sevenZip, QStringList{ QStringLiteral("e"), QStringLiteral("-so"),
                                             QStringLiteral("-pR04pass"),
                                             QDir::toNativeSeparators(dest),
                                             QFileInfo(input).fileName() });
        QVERIFY(extract.waitForStarted(10000));
        QVERIFY(extract.waitForFinished(30000));
        QCOMPARE(extract.exitCode(), 0);
        QCOMPARE(extract.readAllStandardOutput(), inputBytes);  // exactly the intended input
    }
};

// Custom main mirroring QTEST_GUILESS_MAIN plus the --r04-fake-writer re-exec
// branch (same shape as TestSecretStore's --ec04-child).
int main(int argc, char** argv)
{
    for (int i = 1; i + 2 < argc; ++i) {
        if (QByteArray(argv[i]) == QByteArrayLiteral("--r04-fake-writer"))
            return fakeWriterMain(QString::fromLocal8Bit(argv[i + 1]),
                                  QString::fromLocal8Bit(argv[i + 2]));
    }
    QCoreApplication app(argc, argv);
    TestEncryptedPackageSafeWrite tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "TestEncryptedPackageSafeWrite.moc"
