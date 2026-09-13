// SPDX-License-Identifier: Apache-2.0
// WP-R10 (P1) regression suite — temporary data ownership and liveness.
//
// WHOLE-ARCHITECTURE-REVIEW-2026-09-10 A06: startup temporary cleanup treated
// filename+age as ownership — it deleted every GlyphPDF-* / glyph_* entry
// older than 24 h from the system temp directory. A SECOND instance could
// therefore delete an OLDER but STILL-ACTIVE session's working files
// (reproduced at runtime pre-fix; see prefix-runtime-repro-output.txt in this
// evidence directory).
//
// The repair boundary is TempFileManager (src/core/TempFileManager.{h,cpp}):
//  - each instance owns a PRIVATE session directory with an ownership record
//    (PID + session token + lease timestamp) written beside the session data;
//  - cleanup only removes session directories with a VALID ownership record
//    whose owner process is DEAD and whose lease is STALE (24 h);
//  - entries without a valid record are never touched;
//  - the manager fails CLOSED when the private root cannot be created.
//
// All scenarios run in an ISOLATED root (setSessionRootOverrideForTesting) —
// the real system temporary directory is never scanned or modified. Two
// simulated instances use REAL pids: the test process plays the live first
// instance; spawned child processes play a live second owner (survives
// cleanup while running) and a dead one (removed once dead + stale).
#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QUuid>

#include "core/TempFileManager.h"

class TestTempOwnership : public QObject {
    Q_OBJECT

    QTemporaryDir m_root;   // isolated session root for every scenario

    // Plants an owned session directory with a working file inside.
    QString makeSession(const QString& name, quint64 pid, qint64 createdMs,
                        const QString& token = QStringLiteral("tok-abc123"))
    {
        const QString dir = m_root.filePath(name);
        if (!QDir().mkpath(dir))
            qFatal("makeSession: cannot create %s", qPrintable(dir));
        QFile f(dir + QStringLiteral("/autosave-candidate.pdf"));
        if (!f.open(QIODevice::WriteOnly))
            qFatal("makeSession: cannot create working file");
        f.write("session working bytes");
        f.close();
        if (pid > 0 || createdMs > 0 || !token.isEmpty()) {
            if (!TempFileManager::writeOwnershipMarker(dir, pid, token, createdMs))
                qFatal("makeSession: cannot write ownership marker");
        }
        return dir;
    }

    static qint64 staleMs()   { return QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() - 25 * 3600 * 1000; }
    static qint64 freshMs()   { return QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(); }

    static bool exists(const QString& p) { return QFileInfo::exists(p); }

private slots:
    void init() {
        TempFileManager::setSessionRootOverrideForTesting(m_root.path());
    }
    void cleanup() {
        TempFileManager::setSessionRootOverrideForTesting(QString());
    }

    void initTestCase() {
        QVERIFY(m_root.isValid());
        // Sanity: the manager considers THIS process alive.
        QVERIFY(TempFileManager::isProcessAlive(
            static_cast<quint64>(QCoreApplication::applicationPid())));
    }

    // THE A06 trigger: a second instance's startup cleanup must not delete an
    // older but STILL-ACTIVE session's working files. The first instance is
    // simulated by this live process's pid; the lease is 25 h old — under the
    // old name+age rule this session was deleted.
    void liveOldSessionSurvivesSecondInstanceCleanup() {
        const quint64 self = static_cast<quint64>(QCoreApplication::applicationPid());
        const QString dir = makeSession(QStringLiteral("GlyphPDF-liveold"), self, staleMs());
        const QString workFile = dir + QStringLiteral("/autosave-candidate.pdf");

        TempFileManager::instance().cleanStaleTempFiles();

        QVERIFY2(exists(dir), "live owner's session directory must survive");
        QVERIFY2(exists(workFile), "live owner's working files must survive");
    }

    // Liveness is REAL: a running child process's session survives cleanup;
    // the same session is removed only after the owner is dead (and stale).
    void livenessFollowsARealProcess() {
        QProcess live;
        live.setProgram(QStringLiteral("cmd"));
        live.setArguments({ QStringLiteral("/c"), QStringLiteral("ping -n 30 127.0.0.1 > NUL") });
        live.start();
        QVERIFY(live.waitForStarted(10000));
        const quint64 childPid = static_cast<quint64>(live.processId());
        QVERIFY(TempFileManager::isProcessAlive(childPid));

        const QString dir = makeSession(QStringLiteral("GlyphPDF-child"), childPid, staleMs());
        TempFileManager::instance().cleanStaleTempFiles();
        QVERIFY2(exists(dir), "a RUNNING owner's session must survive cleanup");

        live.kill();
        QVERIFY(live.waitForFinished(10000));
        QVERIFY2(!TempFileManager::isProcessAlive(childPid),
                 "the child must be dead before the removal leg");
        TempFileManager::instance().cleanStaleTempFiles();
        QVERIFY2(!exists(dir), "a DEAD owner's stale session must be removed");
    }

    // Dead owner + stale lease → removed recursively (contents included).
    void deadStaleSessionIsRemoved() {
        const QString dir = makeSession(QStringLiteral("GlyphPDF-dead"), 4194304000ull, staleMs());
        TempFileManager::instance().cleanStaleTempFiles();
        QVERIFY2(!exists(dir), "orphaned owned session must be removed");
    }

    // Dead owner but FRESH lease → kept (owner dead AND lease stale required).
    void deadButFreshSessionIsKept() {
        const QString dir = makeSession(QStringLiteral("GlyphPDF-fresh"), 4194304001ull, freshMs());
        TempFileManager::instance().cleanStaleTempFiles();
        QVERIFY2(exists(dir), "a freshly-owned session must not be removed");
    }

    // Entries without a valid ownership record are NEVER touched — unrelated
    // content that merely matches the historic name patterns stays.
    void unownedMatchingEntriesAreNeverTouched() {
        const QString unowned = m_root.filePath(QStringLiteral("GlyphPDF-not-ours"));
        QVERIFY(QDir().mkpath(unowned));
        QFile foreign(unowned + QStringLiteral("/user-data.txt"));
        QVERIFY(foreign.open(QIODevice::WriteOnly));
        foreign.write("not a glyphpdf session");
        foreign.close();

        const QString legacy = m_root.filePath(QStringLiteral("glyph_legacy-file.tmp"));
        QFile legacyFile(legacy);
        QVERIFY(legacyFile.open(QIODevice::WriteOnly));
        legacyFile.write("old loose temp file");
        legacyFile.close();

        TempFileManager::instance().cleanStaleTempFiles();

        QVERIFY2(exists(unowned), "a name-matching but unowned directory must never be deleted");
        QVERIFY2(exists(legacy), "a loose unowned glyph_* file must never be deleted");
        QVERIFY2(exists(unowned + QStringLiteral("/user-data.txt")),
                 "contents of unowned directories must never be deleted");
    }

    // A corrupted/garbage ownership record does NOT prove ownership.
    void corruptMarkerIsNotOwnership() {
        const QString dir = m_root.filePath(QStringLiteral("GlyphPDF-corrupt"));
        QVERIFY(QDir().mkpath(dir));
        QFile marker(dir + QStringLiteral("/") + TempFileManager::ownershipMarkerName());
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("garbage\nnot=a\nvalid=record\n");
        marker.close();

        TempFileManager::instance().cleanStaleTempFiles();
        QVERIFY2(exists(dir), "a corrupt record must be treated as NOT owned");
    }

    // This instance's own session directory is never removed by its own scan.
    void ownSessionIsNeverSelfRemoved() {
        const QString session = TempFileManager::appTempDir();
        QVERIFY(!session.isEmpty());
        TempFileManager::instance().cleanStaleTempFiles();
        QVERIFY2(exists(session), "the running instance's own session must survive");
    }

    // Every created session carries a valid ownership record (PID + token +
    // lease) beside the session data, and creation lands INSIDE the session.
    void createdSessionCarriesOwnershipRecord() {
        const QString file = TempFileManager::instance().createTempFile(QStringLiteral(".tmp"));
        QVERIFY(!file.isEmpty());
        QVERIFY(file.startsWith(TempFileManager::appTempDir()));

        const auto rec = TempFileManager::readOwnershipMarker(TempFileManager::appTempDir());
        QVERIFY(rec.valid);
        QCOMPARE(rec.pid, static_cast<quint64>(QCoreApplication::applicationPid()));
        QVERIFY(!rec.token.isEmpty());
        QVERIFY(rec.createdMs > 0);
    }

    // Fail closed: when the private root cannot be created, the manager must
    // NOT fall back to a shared directory — it reports failure instead.
    void failClosedWhenPrivateRootUnavailable() {
        const QString blocker = m_root.filePath(QStringLiteral("blocker"));
        { QFile f(blocker); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("x"); }
        // A path UNDER a regular file can never be created.
        TempFileManager::setSessionRootOverrideForTesting(blocker + QStringLiteral("/child"));

        QVERIFY2(TempFileManager::appTempDir().isEmpty(),
                 "appTempDir must fail closed (no shared fallback)");
        QVERIFY2(TempFileManager::instance().createTempFile(QStringLiteral(".tmp")).isEmpty(),
                 "createTempFile must report failure");
        QVERIFY2(TempFileManager::instance().createTempDir(QStringLiteral("x")).isEmpty(),
                 "createTempDir must report failure");
    }
};

QTEST_GUILESS_MAIN(TestTempOwnership)
#include "TestTempOwnership.moc"
