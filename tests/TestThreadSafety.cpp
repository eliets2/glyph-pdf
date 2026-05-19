#include <QtTest>
#include <QTemporaryDir>
#include <future>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"

class TestThreadSafety : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    QString tmpPath(const QString &name) const {
        return m_tmpDir.filePath(name);
    }

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Failed to create temp directory");
    }

    void testConcurrentEngineAccess() {
        QString pdf = tmpPath("concurrent.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        auto future1 = std::async(std::launch::async, [&]() {
            PdfMetadata meta;
            return engine.getMetadata(meta);
        });

        auto future2 = std::async(std::launch::async, [&]() {
            PdfMetadata meta;
            meta.title = "Concurrent Title";
            return engine.setMetadata(meta);
        });

        future1.wait();
        future2.wait();

        QVERIFY(future1.get() || future2.get());
    }

    void testAsyncEncryptionDataRace() {
        QString pdf = tmpPath("encrypt_race.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        auto future = std::async(std::launch::async, [&]() {
            return engine.encryptDocument("secret", "secret", true, true, true);
        });

        engine.currentFile();

        future.wait();
        QVERIFY(future.get());
    }

    void testAsyncRedactionDataRace() {
        QString pdf = tmpPath("redact_race.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        auto future = std::async(std::launch::async, [&]() {
            return engine.applyRedactions(0, {QRectF(0, 0, 10, 10)});
        });

        engine.currentFile();

        future.wait();
        QVERIFY(future.get());
    }
};

QTEST_GUILESS_MAIN(TestThreadSafety)
#include "TestThreadSafety.moc"
