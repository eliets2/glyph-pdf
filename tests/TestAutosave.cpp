#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QThread>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>
#include <QPdfWriter>
#include <QPainter>
#include "engines/AutosaveManager.h"
#include "engines/DocumentSession.h"
#include "engines/PdfEditorEngine.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "mocks/MockPdfEditorEngine.h"

class TestAutosave : public QObject {
    Q_OBJECT
    static QByteArray readAllOpened(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return {};
        return f.readAll();
    }
    // Real one-page PDF with an extractable marker — hand-built byte-exact
    // xref (TestPagesMode idiom): no QPainter/QFontDatabase, so it works in a
    // QCoreApplication test while still being a real, PDFium-readable file.
    static bool makeMarkerPdf(const QString &path, const QString &marker)
    {
        QByteArray lit = marker.toLatin1();
        lit.replace('\\', "\\\\").replace('(', "\\(").replace(')', "\\)");
        QByteArray content = "BT /F1 12 Tf 72 720 Td (" + lit + ") Tj ET\n";
        QByteArray pdf = "%PDF-1.4\n";
        QList<qint64> offsets;
        offsets.append(pdf.size());
        pdf += "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n";
        offsets.append(pdf.size());
        pdf += "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n";
        offsets.append(pdf.size());
        pdf += "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
               "/Resources<</Font<</F1 5 0 R>>>>>>endobj\n";
        offsets.append(pdf.size());
        pdf += "4 0 obj<</Length " + QByteArray::number(content.size())
             + ">>stream\n" + content + "endstream endobj\n";
        offsets.append(pdf.size());
        pdf += "5 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica/Encoding/WinAnsiEncoding>>endobj\n";
        const qint64 xref = pdf.size();
        pdf += "xref\n0 6\n0000000000 65535 f \n";
        for (qint64 off : offsets)
            pdf += QByteArray::number(static_cast<qulonglong>(off))
                       .rightJustified(10, '0') + " 00000 n \n";
        pdf += "trailer<</Size 6/Root 1 0 R>>\nstartxref\n"
             + QByteArray::number(static_cast<qulonglong>(xref)) + "\n%%EOF\n";
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return false;
        return f.write(pdf) == pdf.size();
    }
    static QString extractedText(const QString &path)
    {
        PdfiumBackend backend;
        if (!backend.loadDocument(path)) return {};
        return backend.extractText(0);
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

    // ── G04 (P1, QUALITY-GATE-2026-09-09) ───────────────────────────────────
    // The gate's EC02 residual, against the REAL engine: the EC02 path-string
    // check alone accepts a REPLACED document. Queue a dirty A's autosave
    // behind a deterministic worker barrier; switch to B; replace A on disk
    // and re-open it (A→B→A, same path, new incarnation); release the worker.
    // The old recovery file must NEVER be overwritten: byte-identical
    // OLD_RECOVERY_A survives, no temp is left, the run is reported stale.
    // (Pre-fix the string matched, the new incarnation's bytes were saved into
    // A's recovery file and autosaveCompleted was emitted.)
    void testAutosaveRejectsReplacedSamePathDocument()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath("a.pdf");
        const QString b = dir.filePath("b.pdf");
        QVERIFY(makeMarkerPdf(a, QStringLiteral("OLD_A")));
        QVERIFY(makeMarkerPdf(b, QStringLiteral("B_DOC")));
        QVERIFY(makeMarkerPdf(a + ".autosave.pdf", QStringLiteral("OLD_RECOVERY_A")));
        const QByteArray recoveryBefore = readAllOpened(a + ".autosave.pdf");

        auto editor = std::make_shared<PdfEditorEngine>();
        auto doc = std::make_shared<DocumentSession>();
        AutosaveManager manager(editor, doc);

        // Deterministic barrier: occupy the single worker thread BEFORE the
        // autosave future is queued, so the whole A→B→A(replace) sequence
        // lands while the autosave is still pending. No sleeps.
        QThreadPool* pool = QThreadPool::globalInstance();
        const int capacity = pool->maxThreadCount();
        pool->setMaxThreadCount(1);
        QSemaphore barrierReady, barrierGo;
        (void)QtConcurrent::run([&] { barrierReady.release(); barrierGo.acquire(); });
        barrierReady.acquire();

        QVERIFY(editor->loadDocumentForEditing(a));
        doc->beginDocument(a);
        doc->markDirty();

        QSignalSpy completeSpy(&manager, &AutosaveManager::autosaveCompleted);
        QSignalSpy failedSpy(&manager, &AutosaveManager::autosaveFailed);
        // Runtime string connect: compiles on baselines without the signal.
        QSignalSpy staleSpy(&manager, SIGNAL(autosaveStale(QString)));

        QMetaObject::invokeMethod(&manager, "onTick", Qt::DirectConnection);

        // A→B→A with A REPLACED on disk: same path, new incarnation.
        QVERIFY(editor->loadDocumentForEditing(b));
        doc->beginDocument(b);
        QVERIFY(makeMarkerPdf(a, QStringLiteral("NEW_INCARNATION_A")));
        QVERIFY(editor->loadDocumentForEditing(a));
        doc->beginDocument(a);

        barrierGo.release();
        QVERIFY(QTest::qWaitFor([&] {
            return completeSpy.count() + failedSpy.count() + staleSpy.count() > 0;
        }, 10000));
        pool->waitForDone();
        pool->setMaxThreadCount(capacity);

        // The captured recovery file is untouched — byte-identical, still the
        // OLD recovery content.
        QCOMPARE(readAllOpened(a + ".autosave.pdf"), recoveryBefore);
        QVERIFY2(extractedText(a + ".autosave.pdf").contains(QStringLiteral("OLD_RECOVERY_A")),
                 "the recovery file must still hold the old recovery content");
        QVERIFY(!QFileInfo::exists(a + ".autosave.pdf.tmp"));
        QCOMPARE(completeSpy.count(), 0);
        QCOMPARE(staleSpy.count(), 1);
        QCOMPARE(failedSpy.count(), 0);
        QVERIFY(doc->lastAutosave().isNull());
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
