#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QThread>
#include "engines/AutosaveManager.h"
#include "engines/DocumentSession.h"
#include "mocks/MockPdfEditorEngine.h"

class TestAutosave : public QObject {
    Q_OBJECT
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
};

QTEST_GUILESS_MAIN(TestAutosave)
#include "TestAutosave.moc"
