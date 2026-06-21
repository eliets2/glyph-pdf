#include <QTest>
#include <QSignalSpy>
#include <QCoreApplication>
#include "core/UpdateChecker.h"
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QTimer>

class MockReply : public QNetworkReply {
    Q_OBJECT
public:
    explicit MockReply(const QByteArray& data, QObject* parent = nullptr)
        : QNetworkReply(parent), m_data(data)
    {
        setError(NoError, {});
        open(ReadOnly);
        QTimer::singleShot(0, this, [this](){ emit finished(); });
    }
    void abort() override {}
    qint64 bytesAvailable() const override { return m_data.size() - m_pos; }
    bool isSequential() const override { return true; }
protected:
    qint64 readData(char* dst, qint64 maxlen) override {
        qint64 n = qMin(maxlen, static_cast<qint64>(m_data.size() - m_pos));
        if (n <= 0) return -1;
        memcpy(dst, m_data.constData() + m_pos, static_cast<size_t>(n));
        m_pos += n;
        return n;
    }
private:
    QByteArray m_data;
    qint64 m_pos = 0;
};

class MockNam : public QNetworkAccessManager {
    Q_OBJECT
public:
    explicit MockNam(const QByteArray& responseBody, QObject* parent = nullptr)
        : QNetworkAccessManager(parent), m_body(responseBody) {}
protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& req,
                                 QIODevice* outgoingData = nullptr) override
    {
        Q_UNUSED(op); Q_UNUSED(req); Q_UNUSED(outgoingData);
        return new MockReply(m_body, this);
    }
private:
    QByteArray m_body;
};

static void driveChecker(gp::UpdateChecker& checker, const QByteArray& manifestJson)
{
    auto* mockNam = new MockNam(manifestJson, &checker);
    checker.setNetworkAccessManager(mockNam);
    checker.setManifestUrl(QUrl("https://glyphpdf.com/updates/latest.json"));
    checker.checkForUpdates();
}

class TestUpdateChecker : public QObject {
    Q_OBJECT

private slots:
    void testHttpUrlRejected()
    {
        QCoreApplication::setApplicationVersion("1.0.0");
        gp::UpdateChecker checker;

        QSignalSpy failed(&checker, &gp::UpdateChecker::checkFailed);
        QSignalSpy available(&checker, &gp::UpdateChecker::updateAvailable);

        const QByteArray json = "{"
            "\"version\": \"9.9.9\","
            "\"releaseDate\": \"2026-01-01\","
            "\"downloadUrl\": \"http://evil.example.com/glyphpdf.msi\","
            "\"sha256\": \"abc123\","
            "\"releaseNotes\": \"pwn\","
            "\"minVersion\": \"1.0.0\""
        "}";

        driveChecker(checker, json);

        QVERIFY(failed.wait(2000));
        QCOMPARE(available.count(), 0);
        QVERIFY2(!failed.first().first().toString().isEmpty(),
                 "checkFailed reason must not be empty");
    }

    void testFtpUrlRejected()
    {
        QCoreApplication::setApplicationVersion("1.0.0");
        gp::UpdateChecker checker;

        QSignalSpy failed(&checker, &gp::UpdateChecker::checkFailed);
        QSignalSpy available(&checker, &gp::UpdateChecker::updateAvailable);

        const QByteArray json = "{"
            "\"version\": \"9.9.9\","
            "\"releaseDate\": \"2026-01-01\","
            "\"downloadUrl\": \"ftp://evil.example.com/glyphpdf.msi\","
            "\"sha256\": \"abc123\","
            "\"releaseNotes\": \"pwn\","
            "\"minVersion\": \"1.0.0\""
        "}";

        driveChecker(checker, json);

        QVERIFY(failed.wait(2000));
        QCOMPARE(available.count(), 0);
        QVERIFY2(!failed.first().first().toString().isEmpty(),
                 "checkFailed reason must not be empty");
    }

    void testEmptySha256Rejected()
    {
        QCoreApplication::setApplicationVersion("1.0.0");
        gp::UpdateChecker checker;

        QSignalSpy failed(&checker, &gp::UpdateChecker::checkFailed);
        QSignalSpy available(&checker, &gp::UpdateChecker::updateAvailable);

        const QByteArray json = "{"
            "\"version\": \"9.9.9\","
            "\"releaseDate\": \"2026-01-01\","
            "\"downloadUrl\": \"https://ok.example.com/glyphpdf.msi\","
            "\"sha256\": \"\","
            "\"releaseNotes\": \"pwn\","
            "\"minVersion\": \"1.0.0\""
        "}";

        driveChecker(checker, json);

        QVERIFY(failed.wait(2000));
        QCOMPARE(available.count(), 0);
        QVERIFY2(!failed.first().first().toString().isEmpty(),
                 "checkFailed reason must not be empty");
    }

    void testValidManifestNoUpdate()
    {
        QCoreApplication::setApplicationVersion("1.0.0");
        gp::UpdateChecker checker;

        QSignalSpy failed(&checker, &gp::UpdateChecker::checkFailed);
        QSignalSpy available(&checker, &gp::UpdateChecker::updateAvailable);
        QSignalSpy noUpdate(&checker, &gp::UpdateChecker::noUpdateAvailable);

        const QByteArray json = "{"
            "\"version\": \"0.0.1\","
            "\"releaseDate\": \"2026-01-01\","
            "\"downloadUrl\": \"https://ok.example.com/glyphpdf.msi\","
            "\"sha256\": \"abc123xyz\","
            "\"releaseNotes\": \"fixes\","
            "\"minVersion\": \"1.0.0\""
        "}";

        driveChecker(checker, json);

        QVERIFY(noUpdate.wait(2000));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(available.count(), 0);
    }

    void testValidManifestUpdateAvailable()
    {
        QCoreApplication::setApplicationVersion("1.0.0");
        gp::UpdateChecker checker;

        QSignalSpy failed(&checker, &gp::UpdateChecker::checkFailed);
        QSignalSpy available(&checker, &gp::UpdateChecker::updateAvailable);
        QSignalSpy noUpdate(&checker, &gp::UpdateChecker::noUpdateAvailable);

        const QByteArray json = "{"
            "\"version\": \"9.9.9\","
            "\"releaseDate\": \"2026-01-01\","
            "\"downloadUrl\": \"https://ok.example.com/glyphpdf.msi\","
            "\"sha256\": \"abc123xyz\","
            "\"releaseNotes\": \"fixes\","
            "\"minVersion\": \"1.0.0\""
        "}";

        driveChecker(checker, json);

        QVERIFY(available.wait(2000));
        QCOMPARE(failed.count(), 0);
        QCOMPARE(noUpdate.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TestUpdateChecker)
#include "TestUpdateChecker.moc"
