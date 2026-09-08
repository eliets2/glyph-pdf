#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QThread>
#include <QSemaphore>
#include <QTemporaryDir>
#include "engines/AutosaveManager.h"
#include "engines/DocumentSession.h"
#include "mocks/MockPdfEditorEngine.h"

class TestAutosave : public QObject {
    Q_OBJECT
    static QByteArray readAllOpened(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        return f.readAll();
    }
private slots:
    void testIntervalClamping() {
        auto mockEditor = std::make_shared<MockPdfEditorEngine>();
        auto doc = std::make_shared<DocumentSession>();
        AutosaveManager manager(mockEditor, doc);

        manager.start(10); // Too low
        QCOMPARE(manager.interval(), 60);

        manager.start(2000); // Too high
        QCOMPARE(manager.interval(), 1800);

        manager.start(300); // Valid
        QCOMPARE(manager.interval(), 300);
    }

    void testNoSaveWhenClean() {
        auto mockEditor = std::make_shared<MockPdfEditorEngine>();
        mockEditor->m_loaded = true;
        mockEditor->m_file = "test_clean.pdf";

        auto doc = std::make_shared<DocumentSession>();
        doc->setPath("test_clean.pdf");
        QVERIFY(!doc->isDirty());

        AutosaveManager manager(mockEditor, doc);
        QSignalSpy spy(&manager, &AutosaveManager::autosaveStarted);

        QMetaObject::invokeMethod(&manager, "onTick", Qt::DirectConnection);

        QCOMPARE(spy.count(), 0);
    }

    void testSaveWhenDirty() {
        QString originalFile = "test_dirty.pdf";
        QString autosaveFile = originalFile + ".autosave.pdf";
        QString tmpAutosaveFile = originalFile + ".autosave.pdf.tmp";

        QFile::remove(originalFile);
        QFile::remove(autosaveFile);
        QFile::remove(tmpAutosaveFile);

        auto mockEditor = std::make_shared<MockPdfEditorEngine>();
        mockEditor->m_loaded = true;
        mockEditor->m_file = originalFile;

        auto doc = std::make_shared<DocumentSession>();
        doc->setPath(originalFile);
        doc->markDirty();
        QVERIFY(doc->isDirty());

        AutosaveManager manager(mockEditor, doc);
        QSignalSpy startSpy(&manager, &AutosaveManager::autosaveStarted);
        QSignalSpy completeSpy(&manager, &AutosaveManager::autosaveCompleted);

        QMetaObject::invokeMethod(&manager, "onTick", Qt::DirectConnection);

        QVERIFY(QTest::qWaitFor([&]() { return completeSpy.count() > 0; }, 2000));

        QCOMPARE(startSpy.count(), 1);
        QVERIFY(QFile::exists(autosaveFile));

        QFile::remove(autosaveFile);
    }

    // ── EC02 (TEAM-ENGINE-CODE-REVIEW-2026-09-07) ───────────────────────────
    // Autosave must never write document B's bytes into the recovery path
    // captured while document A was open, and must not timestamp the
    // switched-to session. Deterministic barrier: the worker parks inside the
    // save decision (two semaphores in the mock), the test switches documents
    // mid-flight, then releases. No sleeps.
    //
    // Saved bytes are identity-tagged (m_saveWritesIdentity): the recovery
    // artifact itself proves WHICH document was serialized.
    void testAutosaveNeverWritesSwitchedDocumentIntoCapturedPath()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath("a.pdf");
        const QString b = dir.filePath("b.pdf");

        auto mockEditor = std::make_shared<MockPdfEditorEngine>();
        mockEditor->m_loaded = true;
        mockEditor->m_file = a;
        mockEditor->m_saveWritesIdentity = true;

        auto doc = std::make_shared<DocumentSession>();
        doc->setPath(a);
        doc->markDirty();

        AutosaveManager manager(mockEditor, doc);
        QSignalSpy completeSpy(&manager, &AutosaveManager::autosaveCompleted);
        QSignalSpy failedSpy(&manager, &AutosaveManager::autosaveFailed);
        // Runtime string connect: the stale outcome signal only exists post-fix;
        // the test still compiles (spy simply stays empty) on pre-fix baselines.
        QSignalSpy staleSpy(&manager, SIGNAL(autosaveStale(QString)));

        QSemaphore entered;
        QSemaphore hold;
        mockEditor->m_saveEntered = &entered;
        mockEditor->m_saveHold = &hold;

        QMetaObject::invokeMethod(&manager, "onTick", Qt::DirectConnection);
        // Barrier 1: the worker reached the save decision for captured A.
        QVERIFY(QTest::qWaitFor([&] { return entered.available() > 0; }, 5000));

        // Switch documents while A's autosave is stalled.
        mockEditor->m_file = b;
        doc->setPath(b);
        doc->markDirty();

        hold.release();   // let the save decision proceed against the switched editor
        QVERIFY(QTest::qWaitFor([&] {
            return completeSpy.count() + failedSpy.count() + staleSpy.count() > 0;
        }, 5000));

        // A's recovery path must never come into existence with B's bytes.
        const QString aAutosave = a + ".autosave.pdf";
        const QString aTmp = a + ".autosave.pdf.tmp";
        QVERIFY2(!QFileInfo::exists(aAutosave),
                 "EC02: A's recovery file must not be created after a document switch");
        QVERIFY2(!QFileInfo::exists(aTmp),
                 "EC02: no temporary autosave may be left for the stale capture");
        // …and B's session must not be told that A's autosave happened.
        QVERIFY2(doc->lastAutosave().isNull(),
                 "EC02: completion must not timestamp the switched-to session");
        QCOMPARE(completeSpy.count(), 0);
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(staleSpy.count(), 1);
    }

    // The engine-resident document did NOT change, but the SESSION was switched
    // while the save was in flight: A's own bytes may be promoted into A's
    // recovery file, but the new session must not receive the timestamp.
    void testAutosaveCompletionAfterSessionSwitchDoesNotTimestampNewSession()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath("a.pdf");
        const QString b = dir.filePath("b.pdf");

        auto mockEditor = std::make_shared<MockPdfEditorEngine>();
        mockEditor->m_loaded = true;
        mockEditor->m_file = a;
        mockEditor->m_saveWritesIdentity = true;

        auto doc = std::make_shared<DocumentSession>();
        doc->setPath(a);
        doc->markDirty();

        AutosaveManager manager(mockEditor, doc);
        QSignalSpy completeSpy(&manager, &AutosaveManager::autosaveCompleted);
        QSignalSpy staleSpy(&manager, SIGNAL(autosaveStale(QString)));

        QSemaphore entered;
        QSemaphore hold;
        mockEditor->m_saveEntered = &entered;
        mockEditor->m_saveHold = &hold;

        QMetaObject::invokeMethod(&manager, "onTick", Qt::DirectConnection);
        QVERIFY(QTest::qWaitFor([&] { return entered.available() > 0; }, 5000));

        doc->setPath(b);   // session switches; the engine still holds A
        doc->markDirty();

        hold.release();
        QVERIFY(QTest::qWaitFor([&] {
            return completeSpy.count() + staleSpy.count() > 0;
        }, 5000));

        // A's own bytes are A's legitimate recovery content…
        const QString aAutosave = a + ".autosave.pdf";
        QVERIFY(QFileInfo::exists(aAutosave));
        QCOMPARE(readAllOpened(aAutosave), QByteArray("resident=") + a.toUtf8());
        // …but the timestamp belongs to A's identity, not to whichever session
        // happens to be current at completion time.
        QVERIFY2(doc->lastAutosave().isNull(),
                 "EC02: a session switched away must not be timestamped by a late completion");
        QCOMPARE(completeSpy.count(), 1);
        QCOMPARE(staleSpy.count(), 0);
    }

    // After a switch, a NEW autosave tick must target the CURRENT document's
    // own recovery path with the CURRENT document's bytes and timestamp.
    void testAutosaveAfterSwitchTargetsCurrentDocument()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath("a.pdf");
        const QString b = dir.filePath("b.pdf");

        auto mockEditor = std::make_shared<MockPdfEditorEngine>();
        mockEditor->m_loaded = true;
        mockEditor->m_file = a;
        mockEditor->m_saveWritesIdentity = true;

        auto doc = std::make_shared<DocumentSession>();
        doc->setPath(a);
        doc->markDirty();

        AutosaveManager manager(mockEditor, doc);
        QSignalSpy completeSpy(&manager, &AutosaveManager::autosaveCompleted);
        QSignalSpy staleSpy(&manager, SIGNAL(autosaveStale(QString)));

        QSemaphore entered;
        QSemaphore hold;
        mockEditor->m_saveEntered = &entered;
        mockEditor->m_saveHold = &hold;

        // Tick 1 while A is current — stalled, then switched away (stale).
        QMetaObject::invokeMethod(&manager, "onTick", Qt::DirectConnection);
        QVERIFY(QTest::qWaitFor([&] { return entered.available() > 0; }, 5000));
        mockEditor->m_file = b;
        doc->setPath(b);
        doc->markDirty();
        hold.release();
        // Tick 1 must terminate with the stale outcome (which also releases
        // the m_saving gate) before the next tick is meaningful.
        QVERIFY(QTest::qWaitFor([&] { return staleSpy.count() >= 1; }, 5000));

        // Tick 2 while B is current and dirty: must save B into B's recovery
        // path and timestamp B's session.
        QMetaObject::invokeMethod(&manager, "onTick", Qt::DirectConnection);
        QVERIFY(QTest::qWaitFor([&] { return completeSpy.count() >= 1; }, 5000));

        const QString bAutosave = b + ".autosave.pdf";
        QVERIFY(QFileInfo::exists(bAutosave));
        QCOMPARE(readAllOpened(bAutosave), QByteArray("resident=") + b.toUtf8());
        QVERIFY(!doc->lastAutosave().isNull());
        QVERIFY(!QFileInfo::exists(a + ".autosave.pdf"));
    }

    void testFindOrphanedAutosaves() {
        QString originalFile = "test_orphan.pdf";
        QString autosaveFile = originalFile + ".autosave.pdf";

        QFile::remove(originalFile);
        QFile::remove(autosaveFile);

        QFile orig(originalFile);
        QVERIFY(orig.open(QIODevice::WriteOnly));
        orig.write("original");
        orig.close();

        QThread::msleep(100);

        QFile autosave(autosaveFile);
        QVERIFY(autosave.open(QIODevice::WriteOnly));
        autosave.write("autosave content");
        autosave.close();

        QStringList recent = { originalFile };
        QStringList orphans = DocumentSession::findOrphanedAutosaves(recent);

        QCOMPARE(orphans.size(), 1);
        QCOMPARE(orphans.first(), originalFile);

        QFile::remove(originalFile);
        QFile::remove(autosaveFile);
    }
    void testRetryUAF() {
        QString originalFile = "test_uaf.pdf";
        QString autosaveFile = originalFile + ".autosave.pdf";
        QString tmpAutosaveFile = originalFile + ".autosave.pdf.tmp";

        QFile::remove(originalFile);
        QFile::remove(autosaveFile);
        QFile::remove(tmpAutosaveFile);

        auto mockEditor = std::make_shared<MockPdfEditorEngine>();
        mockEditor->m_loaded = true;
        mockEditor->m_file = originalFile;

        auto doc = std::make_shared<DocumentSession>();
        doc->setPath(originalFile);
        doc->markDirty();

        // Create a directory at autosaveFile to force atomicRename to fail
        QDir().mkdir(autosaveFile);

        {
            AutosaveManager manager(mockEditor, doc);
            QMetaObject::invokeMethod(&manager, "onTick", Qt::DirectConnection);
            
            // Wait for the future to finish, which will trigger the lambda and fail the rename.
            // The rename failure queues the 250ms retry timer.
            QTest::qWait(50);
            
            // AutosaveManager goes out of scope here and is destroyed
        }

        // Wait past the 250ms window to see if the timer callback crashes (UAF)
        QTest::qWait(300);
        
        QDir().rmdir(autosaveFile);
        QFile::remove(tmpAutosaveFile);
    }
};

QTEST_GUILESS_MAIN(TestAutosave)
#include "TestAutosave.moc"
