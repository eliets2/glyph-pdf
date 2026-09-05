// SPDX-License-Identifier: Apache-2.0
// R03+R04 — bound AI request lifetimes and parsed endpoint policy
// (implementation plan 2026-09-05, findings F02/F03).
//
// The original OllamaProvider::chat queued a GUI-thread callback capturing
// worker-stack variables (replyData/replyOk/replyError) and a QSemaphore by
// reference, behind a fixed 20 s semaphore wait. A timeout could therefore
// return while that pending closure could still run against dead stack memory;
// there was no I/O abort, no cancellation path, and malformed JSON was
// indistinguishable from an empty response.
//
// The original endpoint guard prefix-matched `127.` (F03: http://127.audit.invalid
// passed), so this suite also pins the parsed loopback policy:
//
// R03 contract pinned here:
//   * every request is owned end-to-end by one worker thread — the network
//     manager, reply, deadline timer and event loop all live inside it;
//   * a test-configurable short deadline (QSettings "ai/ollamaTimeoutMs");
//   * exactly one terminal result per request and never a late one (drained
//     after every case);
//   * timeout, cancellation, HTTP status error, network error, malformed JSON
//     and empty response are distinguishable;
//   * pending I/O is aborted on timeout/cancel;
//   * provider destruction mid-work is safe;
//   * request-content restrictions are preserved (32 KiB system-prompt cap,
//     stream=false, model round-trip).
//
// R04 contract pinned here:
//   * HTTP: exact localhost spelling or a host that parses as a literal
//     loopback address (QHostAddress::isLoopback) — no prefix matching;
//   * the full acceptance matrix (allowed and rejected spellings, trailing
//     dots, user-info, numeric spellings, IPv4-mapped IPv6), both supplied
//     and stored endpoints;
//   * HTTPS allowlist / trust-any-HTTPS override with isolated settings;
//   * invalid user-supplied endpoints fail intelligently instead of falling
//     back to a different endpoint;
//   * redirects are disabled and reported, never followed.
//
// The server is a local QTcpServer stub on the loopback interface only — no
// external network.
#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include "engines/ai/IAiProvider.h"
#include "engines/ai/OllamaProvider.h"

// ---------------------------------------------------------------------------
// Local Ollama /api/chat stub — QTcpServer on the loopback, no external net.
// ---------------------------------------------------------------------------
namespace {

constexpr const char* kSuccessBody =
    "{\"model\":\"stub\",\"created_at\":\"now\","
    "\"message\":{\"role\":\"assistant\",\"content\":\"hello from local ollama\"},"
    "\"done\":true}";

class StubOllamaServer {
public:
    enum class Mode {
        Success,        // 200 + valid Ollama chat JSON
        EmptyContent,   // 200 + {"message":{"content":""}}
        Malformed,      // 200 + not-JSON payload
        Status500,      // 500 + JSON error body
        Silent,         // accept and never answer
        DelayedSuccess, // answer Success after delayMs (after any deadline)
        Redirect        // 302 to a remote Location — must NOT be followed
    };

    StubOllamaServer()
    {
        QObject::connect(&m_server, &QTcpServer::newConnection, [this] {
            while (QTcpSocket* s = m_server.nextPendingConnection()) {
                ++connections;
                QObject::connect(s, &QTcpSocket::disconnected,
                                 s, &QObject::deleteLater);
                QObject::connect(s, &QTcpSocket::readyRead, [this, s] { consume(s); });
            }
        });
        if (!m_server.listen(QHostAddress::Any)) // dual-stack: IPv4 + IPv6 loopback
            qFatal("StubOllamaServer: cannot bind loopback listener: %s",
                   qPrintable(m_server.errorString()));
    }

    ~StubOllamaServer() { m_server.close(); }

    bool start() { return m_server.isListening(); }
    quint16 port() const { return m_server.serverPort(); }

    int  connections = 0;    // accepted sockets (≈ connection attempts)
    QByteArray lastBody;     // request body of the last completed request
    Mode mode = Mode::Success;
    int  delayMs = 0;

private:
    void consume(QTcpSocket* s)
    {
        QByteArray& buf = m_buf[s];
        buf.append(s->readAll());
        const int hdrEnd = buf.indexOf("\r\n\r\n");
        if (hdrEnd < 0)
            return;
        int contentLength = 0;
        const QList<QByteArray> lines = buf.left(hdrEnd).split('\n');
        for (const QByteArray& l : lines) {
            if (l.toLower().startsWith("content-length:"))
                contentLength = l.mid(15).trimmed().toInt();
        }
        if (buf.size() - (hdrEnd + 4) < contentLength)
            return; // wait for the full POST body
        lastBody = buf.mid(hdrEnd + 4, contentLength);
        respond(s);
    }

    void respond(QTcpSocket* s)
    {
        auto send = [s](int code, const QByteArray& body) {
            const char* reason = (code == 200) ? "OK" : "Internal Server Error";
            QByteArray head;
            head += "HTTP/1.1 " + QByteArray::number(code) + ' ' + reason + "\r\n";
            head += "Content-Type: application/json\r\n";
            head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
            head += "Connection: close\r\n\r\n";
            s->write(head + body);
        };
        switch (mode) {
        case Mode::Success:
            send(200, kSuccessBody);
            break;
        case Mode::EmptyContent:
            send(200, "{\"message\":{\"role\":\"assistant\",\"content\":\"\"}}");
            break;
        case Mode::Malformed:
            send(200, "this is definitely not json{");
            break;
        case Mode::Status500:
            send(500, "{\"error\":\"stub exploded\"}");
            break;
        case Mode::Silent:
            break;
        case Mode::Redirect:
        {
            // A remote-looking Location on a host that must never be dialed:
            // the request must terminate here, not follow the redirect.
            QByteArray head;
            head += "HTTP/1.1 302 Found\r\n";
            head += "Location: http://audit.invalid:11434/\r\n";
            head += "Content-Length: 0\r\n";
            head += "Connection: close\r\n\r\n";
            s->write(head);
            break;
        }
        case Mode::DelayedSuccess:
            QTimer::singleShot(delayMs, s, [s] {
                if (s->state() == QAbstractSocket::ConnectedState) {
                    const QByteArray body = kSuccessBody;
                    QByteArray head;
                    head += "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n";
                    head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
                    head += "Connection: close\r\n\r\n";
                    s->write(head + body);
                }
            });
            break;
        }
    }

    QTcpServer m_server;
    QHash<QTcpSocket*, QByteArray> m_buf;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Pump the owning (test) thread's event queue, including deferred deletes, so
// any late/queued callback that still referenced dead state would run HERE and
// be observable as a crash, an extra result, or a state change.
void drainEvents(int rounds = 6)
{
    for (int i = 0; i < rounds; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QTest::qWait(20);
    }
}

void setDeadlineMs(int ms)
{
    QSettings().setValue(QStringLiteral("ai/ollamaTimeoutMs"), ms);
}

// Wait for one terminal result on this thread's event loop (pumps events while
// waiting, so pre-fix GUI-thread dispatch can also deliver).
gp::AiResult waitResult(QFuture<gp::AiResult> future, int timeoutMs)
{
    QFutureWatcher<gp::AiResult> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<gp::AiResult>::finished,
                     &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    watcher.setFuture(future);
    loop.exec();
    drainEvents(3);
    if (future.resultCount() > 0)
        return future.result();
    return gp::AiResult{false, {}, QStringLiteral("<no result delivered>")};
}

// Find a port the local TCP stack actively REFUSES (RST) right now, verified
// with a real connect. Returns 0 when the stack never refuses: some Windows
// firewall/WFP configurations silently DROP loopback SYNs to any listener-less
// port (every probe times out), where a request would hang until the deadline
// instead of failing fast.
quint16 refusedLoopbackPort()
{
    const quint16 candidates[] = { 1, 4, 9, 10000, 12345, 18000, 24000,
                                   30000, 36000, 41000, 46000 };
    for (const quint16 candidate : candidates) {
        QTcpSocket probe;
        probe.connectToHost(QHostAddress::LocalHost, candidate);
        probe.waitForConnected(150);
        if (probe.error() == QAbstractSocket::ConnectionRefusedError)
            return candidate;
        probe.abort();
    }
    // Also try the freshest possible free port (bind a listener, close it).
    QTcpServer srv;
    if (srv.listen(QHostAddress::LocalHost)) {
        const quint16 p = srv.serverPort();
        srv.close();
        QTcpSocket probe;
        probe.connectToHost(QHostAddress::LocalHost, p);
        probe.waitForConnected(150);
        if (probe.error() == QAbstractSocket::ConnectionRefusedError)
            return p;
    }
    return 0; // this stack drops instead of refusing — use the fallback below
}

// Transport-failure fallback: accepts the TCP connection and closes it
// immediately, before any HTTP byte is exchanged. The provider outcome is the
// branch the connection-refused case targets — reply finished with an error
// and NO HTTP status attribute (nothing was ever received) — the reachability
// error path. Used only when the OS cannot produce a genuine RST refusal.
class CloseImmediatelyServer {
public:
    CloseImmediatelyServer()
    {
        QObject::connect(&m_server, &QTcpServer::newConnection, [this] {
            while (QTcpSocket* s = m_server.nextPendingConnection()) {
                ++accepted;
                s->disconnectFromHost();
                QObject::connect(s, &QTcpSocket::disconnected,
                                 s, &QObject::deleteLater);
            }
        });
        if (!m_server.listen(QHostAddress::LocalHost))
            qFatal("CloseImmediatelyServer: cannot bind: %s",
                   qPrintable(m_server.errorString()));
    }
    ~CloseImmediatelyServer() { m_server.close(); }
    quint16 port() const { return m_server.serverPort(); }
    int accepted = 0;
private:
    QTcpServer m_server;
};

QString endpointFor(const StubOllamaServer& server)
{
    return QStringLiteral("http://127.0.0.1:%1").arg(server.port());
}

} // anonymous namespace

// ---------------------------------------------------------------------------

class TestOllamaProvider : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Isolate QSettings — never read the user's real prefs (same idiom as
        // TestOcrPreprocessPrefs / TestBatchOcrLanguage).
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestOllamaProvider"));
    }

    void cleanup()
    {
        for (const char* key : { "ai/ollamaEndpoint", "ai/ollamaModel",
                                 "ai/ollamaTimeoutMs", "ai/ollamaTrustAnyHttps",
                                 "ai/ollamaAllowedHosts" })
            QSettings().remove(QString::fromLatin1(key));
        drainEvents(2);
    }

    // ── R03: request lifetime ─────────────────────────────────────────────

    // Success over the loopback with body/contract pinning.
    void successOverLoopbackDeliversExactlyOneResult()
    {
        StubOllamaServer server;
        QVERIFY(server.start());
        setDeadlineMs(3000);

        gp::OllamaProvider provider(endpointFor(server));
        gp::AiOptions opts;
        opts.systemPrompt = QStringLiteral("You are a local stub.");
        QFuture<gp::AiResult> future =
            provider.chat({ { QStringLiteral("user"), QStringLiteral("ping") } }, opts);

        const gp::AiResult r = waitResult(future, 10000);
        QVERIFY2(r.ok, qPrintable(QStringLiteral("expected success, got: ") + r.errorMsg));
        QCOMPARE(r.text, QStringLiteral("hello from local ollama"));
        QCOMPARE(future.resultCount(), 1);
        QVERIFY(!future.isCanceled());

        // The request must still carry the Ollama chat contract (N-1 caps and
        // content restrictions preserved — see oversizedSystemPrompt... below).
        const QJsonDocument req = QJsonDocument::fromJson(server.lastBody);
        QVERIFY(req.isObject());
        QCOMPARE(req.object().value("stream").toBool(false), false);
        QCOMPARE(req.object().value("model").toString(), QStringLiteral("llama3"));
        const QJsonArray msgs = req.object().value("messages").toArray();
        QVERIFY(msgs.size() >= 2);
        QCOMPARE(msgs.at(0).toObject().value("role").toString(), QStringLiteral("system"));
        QCOMPARE(msgs.at(0).toObject().value("content").toString(),
                 QStringLiteral("You are a local stub."));
        QCOMPARE(msgs.last().toObject().value("content").toString(), QStringLiteral("ping"));

        // Late delivery would be a second, stale result — drain and re-check.
        drainEvents();
        QCOMPARE(future.resultCount(), 1);
    }

    // The server answers only AFTER the deadline has fired: the request must
    // time out, abort its pending I/O, deliver exactly one timeout result and
    // never a later, stale success.
    void responseAfterDeadlineIsTimeoutWithAbortedIo()
    {
        StubOllamaServer server;
        QVERIFY(server.start());
        server.mode = StubOllamaServer::Mode::DelayedSuccess;
        server.delayMs = 1200;
        setDeadlineMs(250);

        gp::OllamaProvider provider(endpointFor(server));
        QFuture<gp::AiResult> future =
            provider.chat({ { QStringLiteral("user"), QStringLiteral("ping") } });

        const gp::AiResult r = waitResult(future, 10000);
        QVERIFY2(!r.ok, "a response arriving after the deadline must not succeed");
        QVERIFY2(r.errorMsg.contains(QStringLiteral("timed out"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("expected a timeout error, got: ") + r.errorMsg));
        QCOMPARE(future.resultCount(), 1);

        // Let the stub answer into the aborted request; drain everything. A
        // second (late) result or a crash here means dead state was touched.
        QTest::qWait(1600);
        drainEvents();
        QCOMPARE(future.resultCount(), 1);
        QVERIFY(!future.isCanceled());
    }

    void connectionRefusedReportsReachabilityError()
    {
        setDeadlineMs(3000);
        // Prefer a genuinely refused (RST) port. When this OS never refuses —
        // some Windows firewall configs DROP loopback SYNs to any port without
        // a listener — use the accept-then-close fallback, which fails the
        // request at the same transport level (error, no HTTP status) and so
        // exercises the same provider reachability branch.
        const quint16 refused = refusedLoopbackPort();
        QString bad;
        int expectAccepted = 0;
        CloseImmediatelyServer closer;
        if (refused != 0) {
            bad = QStringLiteral("http://127.0.0.1:%1").arg(refused);
            qWarning() << "[R03] connection-refused case via actively refused port"
                       << refused;
        } else {
            bad = QStringLiteral("http://127.0.0.1:%1").arg(closer.port());
            expectAccepted = 1; // the fallback server must have been reached
            qWarning() << "[R03] this OS drops (never refuses) listener-less "
                          "loopback SYNs — connection-refused case via "
                          "accept-then-close transport failure";
        }

        gp::OllamaProvider provider(bad);
        QFuture<gp::AiResult> future =
            provider.chat({ { QStringLiteral("user"), QStringLiteral("ping") } });

        const gp::AiResult r = waitResult(future, 10000);
        QVERIFY(!r.ok);
        QVERIFY2(r.errorMsg.contains(QStringLiteral("not reachable"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("expected a reachability error, got: ") + r.errorMsg));
        QVERIFY2(r.errorMsg.contains(bad),
                 "the error must name the endpoint that was not reachable");
        QCOMPARE(future.resultCount(), 1);
        if (expectAccepted)
            // QNAM may transparently retry once when the transport closes
            // before any response byte — the point here is that the endpoint
            // WAS dispatched (not policy-rejected), then failed to reach.
            QVERIFY2(closer.accepted >= 1,
                     "the fallback endpoint must have been dispatched to");
        drainEvents();
        QCOMPARE(future.resultCount(), 1);
    }

    void httpErrorStatusIsDistinguishedFromNetworkError()
    {
        StubOllamaServer server;
        QVERIFY(server.start());
        server.mode = StubOllamaServer::Mode::Status500;
        setDeadlineMs(3000);

        gp::OllamaProvider provider(endpointFor(server));
        QFuture<gp::AiResult> future = provider.chat({ { "user", "ping" } });

        const gp::AiResult r = waitResult(future, 10000);
        QVERIFY(!r.ok);
        QVERIFY2(r.errorMsg.contains(QStringLiteral("HTTP 500")),
                 qPrintable(QStringLiteral("expected the HTTP status to be reported, got: ")
                            + r.errorMsg));
        QCOMPARE(future.resultCount(), 1);
        drainEvents();
        QCOMPARE(future.resultCount(), 1);
    }

    void malformedJsonIsDistinguishedFromEmptyResponse()
    {
        StubOllamaServer server;
        QVERIFY(server.start());
        server.mode = StubOllamaServer::Mode::Malformed;
        setDeadlineMs(3000);

        gp::OllamaProvider provider(endpointFor(server));
        QFuture<gp::AiResult> future = provider.chat({ { "user", "ping" } });

        const gp::AiResult r = waitResult(future, 10000);
        QVERIFY(!r.ok);
        QVERIFY2(r.errorMsg.contains(QStringLiteral("malformed"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("expected a malformed-JSON error, got: ") + r.errorMsg));
        QVERIFY2(!r.errorMsg.contains(QStringLiteral("Empty response")),
                 "malformed JSON must not be reported as an empty response");
        QCOMPARE(future.resultCount(), 1);
        drainEvents();
        QCOMPARE(future.resultCount(), 1);
    }

    void emptyResponseIsDistinguishedFromMalformedJson()
    {
        StubOllamaServer server;
        QVERIFY(server.start());
        server.mode = StubOllamaServer::Mode::EmptyContent;
        setDeadlineMs(3000);

        gp::OllamaProvider provider(endpointFor(server));
        QFuture<gp::AiResult> future = provider.chat({ { "user", "ping" } });

        const gp::AiResult r = waitResult(future, 10000);
        QVERIFY(!r.ok);
        QVERIFY2(r.errorMsg.contains(QStringLiteral("Empty response")),
                 qPrintable(QStringLiteral("expected an empty-response error, got: ") + r.errorMsg));
        QVERIFY2(!r.errorMsg.contains(QStringLiteral("malformed"), Qt::CaseInsensitive),
                 "an empty completion must not be reported as malformed JSON");
        QCOMPARE(future.resultCount(), 1);
        drainEvents();
        QCOMPARE(future.resultCount(), 1);
    }

    void requestCancellationAbortsPendingIo()
    {
        StubOllamaServer server;
        QVERIFY(server.start());
        server.mode = StubOllamaServer::Mode::Silent;
        setDeadlineMs(15000); // long enough that only the cancel can finish us

        gp::OllamaProvider provider(endpointFor(server));
        QFuture<gp::AiResult> future =
            provider.chat({ { QStringLiteral("user"), QStringLiteral("ping") } });

        QTest::qWait(150);           // let the worker dispatch and connect
        const qint64 startedMs = QDateTime::currentMSecsSinceEpoch();
        future.cancel();             // caller-side cancellation …
        future.waitForFinished();    // … must abort the pending I/O promptly
        const qint64 cancelToFinishMs = QDateTime::currentMSecsSinceEpoch() - startedMs;
        drainEvents();

        // The deadline is 15 s — only an abort can finish a canceled request
        // quickly; a worker that keeps grinding must fail this bound.
        QVERIFY2(cancelToFinishMs < 5000,
                 qPrintable(QStringLiteral("cancellation must abort pending I/O promptly "
                                          "(cancel→finished = %1 ms, deadline 15000 ms)")
                                .arg(cancelToFinishMs)));
        QVERIFY2(future.isCanceled(), "the future must report cancellation");
        QVERIFY2(future.resultCount() == 0,
                 "a canceled request must deliver no result at all");

        // No late result may surface after everything drained.
        QTest::qWait(100);
        drainEvents();
        QCOMPARE(future.resultCount(), 0);
        QVERIFY(future.isFinished());
    }

    void providerDestructionMidWorkStillDeliversOneResult()
    {
        StubOllamaServer server;
        QVERIFY(server.start());
        setDeadlineMs(5000);

        auto* provider = new gp::OllamaProvider(endpointFor(server));
        QFuture<gp::AiResult> future = provider->chat({ { "user", "ping" } });
        delete provider; // owner destroyed while the request is in flight

        const gp::AiResult r = waitResult(future, 10000);
        QVERIFY2(r.ok, qPrintable(QStringLiteral("expected success, got: ") + r.errorMsg));
        QCOMPARE(future.resultCount(), 1);
        drainEvents();
        QCOMPARE(future.resultCount(), 1);
    }

    void providerDestructionBeforeDeadlineTimesOutCleanly()
    {
        StubOllamaServer server;
        QVERIFY(server.start());
        server.mode = StubOllamaServer::Mode::Silent;
        setDeadlineMs(300);

        auto* provider = new gp::OllamaProvider(endpointFor(server));
        QFuture<gp::AiResult> future = provider->chat({ { "user", "ping" } });
        delete provider; // owner destroyed before the deadline fires

        const gp::AiResult r = waitResult(future, 10000);
        QVERIFY(!r.ok);
        QVERIFY2(r.errorMsg.contains(QStringLiteral("timed out"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("expected a timeout error, got: ") + r.errorMsg));
        QCOMPARE(future.resultCount(), 1);
        drainEvents();
        QCOMPARE(future.resultCount(), 1);
    }

    // N-1 / SECFIX-5 restrictions must survive the rework: the system prompt
    // is capped at 32 KiB and stream stays false (checked in the success test).
    void oversizedSystemPromptIsTruncatedTo32k()
    {
        StubOllamaServer server;
        QVERIFY(server.start());
        setDeadlineMs(3000);

        gp::AiOptions opts;
        opts.systemPrompt = QString(40000, QLatin1Char('A'));

        gp::OllamaProvider provider(endpointFor(server));
        QFuture<gp::AiResult> future = provider.chat({ { "user", "ping" } }, opts);

        const gp::AiResult r = waitResult(future, 10000);
        QVERIFY2(r.ok, qPrintable(QStringLiteral("expected success, got: ") + r.errorMsg));

        const QJsonDocument req = QJsonDocument::fromJson(server.lastBody);
        QVERIFY(req.isObject());
        const QJsonArray msgs = req.object().value("messages").toArray();
        QVERIFY(msgs.size() >= 2);
        QCOMPARE(msgs.at(0).toObject().value("role").toString(), QStringLiteral("system"));
        const int sent = msgs.at(0).toObject().value("content").toString().size();
        QVERIFY2(sent <= 32768,
                 qPrintable(QStringLiteral("system prompt must be capped at 32768 bytes, got %1")
                                .arg(sent)));
        QCOMPARE(sent, 32768);
        drainEvents();
        QCOMPARE(future.resultCount(), 1);
    }

    // ── R04: endpoint policy — parsed loopback validation ──────────────────
    //
    // Acceptance matrix (plan R04): permit canonical localhost, 127.0.0.1,
    // other valid 127.x.x.x literals and [::1]; reject 127.audit.invalid,
    // localhost.audit.invalid, 0.0.0.0, private LAN, public HTTP, missing
    // hosts and non-HTTP(S) schemes; pin trailing dots, user-info, unusual
    // numeric spellings and IPv4-mapped IPv6; exercise stored AND directly
    // supplied endpoints. Every rejected row must be blocked BEFORE any
    // network I/O (stub connection count stays 0) with an intelligible error
    // naming the endpoint.

    void endpointPolicyMatrixHttp_data()
    {
        QTest::addColumn<QString>("endpointTemplate"); // %1 = stub port
        QTest::addColumn<bool>("allowed");
        QTest::addColumn<bool>("viaStored");           // exercise stored setting

        // ── allowed: canonical loopback spellings, through the stub ───────
        const char* allowedTemplates[] = {
            "http://localhost:%1",
            "http://LOCALHOST:%1",             // QUrl case-folds the host
            "http://127.0.0.1:%1",
            "http://127.42.0.7:%1",            // another valid 127.x.x.x literal
            "http://[::1]:%1",
            "http://[::ffff:127.0.0.1]:%1",    // IPv4-mapped loopback: pinned ALLOW
            "http://127.1:%1",                 // numeric spelling → QUrl normalizes to 127.0.0.1
            "http://2130706433:%1",            // decimal spelling → QUrl normalizes to 127.0.0.1
        };
        for (const char* t : allowedTemplates) {
            QTest::newRow(qPrintable(QStringLiteral("supplied:%1").arg(t)))
                << QString::fromLatin1(t) << true << false;
            QTest::newRow(qPrintable(QStringLiteral("stored:%1").arg(t)))
                << QString::fromLatin1(t) << true << true;
        }

        // ── rejected: never dispatched, intelligible error ─────────────────
        const char* rejectedTemplates[] = {
            "http://127.audit.invalid:%1",     // the F03 probe — prefix match passed this
            "http://localhost.audit.invalid:%1",
            "http://0.0.0.0:%1",
            "http://192.168.1.10:%1",          // private LAN
            "http://10.0.0.5:%1",              // private LAN
            "http://172.16.4.9:%1",            // private LAN
            "http://example.com:%1",           // public HTTP
            "http://[::ffff:192.168.1.10]:%1", // IPv4-mapped private: pinned REJECT
            "http://:%1",                      // missing host
            "http:///api",                     // missing host
            "localhost:%1",                    // no scheme → parses as scheme, no host
            "ftp://127.0.0.1:%1",              // non-HTTP(S) scheme
            "file:///tmp/x",                   // non-HTTP(S) scheme
            "http://127.0.0.1.:%1",            // trailing dot on a literal
            "http://localhost.:%1",            // trailing dot on a name
            "http://127.0.0.256:%1",           // out-of-range octet — not a parseable literal
            "http://user@127.0.0.1:%1",        // user-info pinned REJECT
            "http://user:pw@localhost:%1",     // user-info pinned REJECT
            "http://127.0.0.1@evil.example:%1" // classic spoof: host is evil.example
        };
        for (const char* t : rejectedTemplates)
            QTest::newRow(qPrintable(QStringLiteral("supplied:%1").arg(t)))
                << QString::fromLatin1(t) << false << false;
    }

    void endpointPolicyMatrixHttp()
    {
        QFETCH(QString, endpointTemplate);
        QFETCH(bool, allowed);
        QFETCH(bool, viaStored);

        StubOllamaServer server;
        QVERIFY(server.start());
        setDeadlineMs(3000);
        const QString endpoint = endpointTemplate.arg(server.port());

        gp::OllamaProvider provider(viaStored ? QString() : endpoint);
        if (viaStored)
            QSettings().setValue(QStringLiteral("ai/ollamaEndpoint"), endpoint);

        QFuture<gp::AiResult> future =
            provider.chat({ { QStringLiteral("user"), QStringLiteral("ping") } });
        const gp::AiResult r = waitResult(future, 10000);
        drainEvents();

        if (allowed) {
            QVERIFY2(r.ok, qPrintable(QStringLiteral("endpoint %1 must be allowed, got: ")
                                      .arg(endpoint) + r.errorMsg));
            QCOMPARE(r.text, QStringLiteral("hello from local ollama"));
            QVERIFY2(server.connections >= 1,
                     "an allowed endpoint must have been dispatched to the stub");
            QCOMPARE(future.resultCount(), 1);
            QVERIFY(!future.isCanceled());
            drainEvents();
            QCOMPARE(future.resultCount(), 1);
        } else {
            QVERIFY2(!r.ok, qPrintable(QStringLiteral("endpoint %1 must be rejected")
                                       .arg(endpoint)));
            QVERIFY2(r.errorMsg.contains(QStringLiteral("not permitted")),
                     qPrintable(QStringLiteral("expected an intelligible policy "
                                              "rejection, got: ") + r.errorMsg));
            QVERIFY2(r.errorMsg.contains(endpoint),
                     "the rejection must name the offending endpoint");
            QCOMPARE(server.connections, 0); // blocked BEFORE any network I/O
            QCOMPARE(future.resultCount(), 1);
            drainEvents();
            QCOMPARE(future.resultCount(), 1);
        }
    }

    // HTTPS: the SECFIX-5 allowlist and the opt-in trust-any override must
    // survive the R04 rework, with isolated settings. Dispatched rows prove
    // the policy let the request through (TLS to a plain stub stalls into the
    // deadline — the point is that it was NOT policy-rejected and DID dial);
    // denied rows must be blocked before any I/O.
    void endpointPolicyHttps_data()
    {
        QTest::addColumn<QString>("endpointTemplate");
        QTest::addColumn<QString>("allowlist");  // "@default" → remove key; else the list
        QTest::addColumn<bool>("trustAny");
        QTest::addColumn<bool>("expectDispatch");

        QTest::newRow("https:default-allowlist:localhost")
            << QString::fromLatin1("https://localhost:%1") << QStringLiteral("@default") << false << true;
        QTest::newRow("https:default-allowlist:loopback-v6")
            << QString::fromLatin1("https://[::1]:%1") << QStringLiteral("@default") << false << true;
        QTest::newRow("https:restricted-allowlist:denies-127.0.0.1")
            << QString::fromLatin1("https://127.0.0.1:%1")
            << QStringLiteral("my-ollama.internal") << false << false;
        QTest::newRow("https:empty-allowlist:denies-everything")
            << QString::fromLatin1("https://localhost:%1") << QString() << false << false;
        QTest::newRow("https:restricted-allowlist:override-dispatches")
            << QString::fromLatin1("https://127.0.0.1:%1")
            << QStringLiteral("my-ollama.internal") << true << true;
        QTest::newRow("https:non-allowlisted-host:denied")
            << QString::fromLatin1("https://my-ollama.example:%1") << QStringLiteral("@default") << false << false;
        QTest::newRow("https:user-info:denied-even-with-override")
            << QString::fromLatin1("https://user@127.0.0.1:%1") << QStringLiteral("@default") << true << false;
    }

    void endpointPolicyHttps()
    {
        QFETCH(QString, endpointTemplate);
        QFETCH(QString, allowlist);
        QFETCH(bool, trustAny);
        QFETCH(bool, expectDispatch);

        StubOllamaServer server;
        QVERIFY(server.start());
        server.mode = StubOllamaServer::Mode::Silent;
        setDeadlineMs(800);
        if (allowlist == QLatin1String("@default"))
            QSettings().remove(QStringLiteral("ai/ollamaAllowedHosts"));
        else
            QSettings().setValue(QStringLiteral("ai/ollamaAllowedHosts"),
                                 allowlist.split(QLatin1Char(';'), Qt::SkipEmptyParts));
        QSettings().setValue(QStringLiteral("ai/ollamaTrustAnyHttps"), trustAny);

        const QString endpoint = endpointTemplate.arg(server.port());
        gp::OllamaProvider provider(endpoint);
        QFuture<gp::AiResult> future =
            provider.chat({ { QStringLiteral("user"), QStringLiteral("ping") } });
        const gp::AiResult r = waitResult(future, 10000);
        drainEvents();

        QCOMPARE(future.resultCount(), 1);
        if (expectDispatch) {
            QVERIFY2(server.connections >= 1,
                     "an HTTPS-allowed endpoint must have been dispatched");
            QVERIFY2(!r.errorMsg.contains(QStringLiteral("not permitted")),
                     qPrintable(QStringLiteral("HTTPS-allowed endpoint must not be "
                                              "policy-rejected, got: ") + r.errorMsg));
        } else {
            QVERIFY2(!r.ok, qPrintable(QStringLiteral("endpoint %1 must be rejected")
                                       .arg(endpoint)));
            QVERIFY2(r.errorMsg.contains(QStringLiteral("not permitted")),
                     qPrintable(QStringLiteral("expected a policy rejection, got: ")
                                + r.errorMsg));
            QCOMPARE(server.connections, 0);
        }
    }

    // An invalid DIRECTLY-SUPPLIED endpoint must fail intelligently — never
    // fall back to a connection with some other (stored/default) endpoint —
    // and isReady() must reflect it.
    void invalidSuppliedEndpointFailsInsteadOfFallback()
    {
        StubOllamaServer server;
        QVERIFY(server.start());
        setDeadlineMs(3000);

        const QStringList invalid = {
            QStringLiteral("http://0.0.0.0:9"),
            QStringLiteral("http://127.audit.invalid:11434"),
            QStringLiteral("localhost:11434"),           // no scheme → no host
            QStringLiteral("http://user@127.0.0.1:%1").arg(server.port()),
        };
        for (const QString& bad : invalid) {
            gp::OllamaProvider provider(bad);
            QVERIFY2(!provider.isReady(),
                     qPrintable(QStringLiteral("isReady must be false for %1").arg(bad)));
            QFuture<gp::AiResult> future =
                provider.chat({ { QStringLiteral("user"), QStringLiteral("ping") } });
            const gp::AiResult r = waitResult(future, 10000);
            QVERIFY2(!r.ok, qPrintable(QStringLiteral("%1 must fail").arg(bad)));
            QVERIFY2(r.errorMsg.contains(QStringLiteral("not permitted")),
                     qPrintable(QStringLiteral("expected intelligible rejection for %1, got: ")
                                .arg(bad) + r.errorMsg));
            QVERIFY2(r.errorMsg.contains(bad),
                     "the rejection must name the supplied endpoint");
            QCOMPARE(future.resultCount(), 1);
        }
        // Nothing may have been dialed, including the default localhost:11434.
        QCOMPARE(server.connections, 0);

        // Contrast: a valid supplied endpoint is ready and dispatches.
        gp::OllamaProvider good(endpointFor(server));
        QVERIFY(good.isReady());
        const gp::AiResult r = waitResult(
            good.chat({ { QStringLiteral("user"), QStringLiteral("ping") } }), 10000);
        QVERIFY2(r.ok, qPrintable(r.errorMsg));
    }

    // An invalid STORED endpoint keeps the documented fallback to the app
    // default (http://localhost:11434) — it must NOT be reported as a policy
    // rejection and must never contact the invalid endpoint.
    void storedInvalidEndpointFallsBackToDefaultEndpoint()
    {
        StubOllamaServer server; // on a random port — NOT the fallback target
        QVERIFY(server.start());
        setDeadlineMs(800);
        QSettings().setValue(QStringLiteral("ai/ollamaEndpoint"),
                             QStringLiteral("http://0.0.0.0:9"));

        gp::OllamaProvider provider; // no supplied endpoint → stored path
        QFuture<gp::AiResult> future =
            provider.chat({ { QStringLiteral("user"), QStringLiteral("ping") } });
        const gp::AiResult r = waitResult(future, 10000);
        drainEvents();

        QCOMPARE(future.resultCount(), 1);
        QVERIFY2(!r.errorMsg.contains(QStringLiteral("not permitted")),
                 "stored-invalid fallback must not be a policy rejection");
        QVERIFY2(!r.errorMsg.contains(QStringLiteral("0.0.0.0")),
                 "the invalid stored endpoint must never be contacted or named");
        QVERIFY2(r.ok || r.errorMsg.contains(QStringLiteral("localhost:11434"))
                     || r.errorMsg.contains(QStringLiteral("Empty response")),
                 qPrintable(QStringLiteral("failure must implicate the default fallback "
                                          "endpoint (or a live default-endpoint server), "
                                          "got: ") + r.errorMsg));
        QCOMPARE(server.connections, 0);
    }

    // Redirects are DISABLED: a 302 must terminate the request with an
    // intelligible error, never follow the Location (nothing may dial the
    // redirect target) and never leak a second result.
    void redirectsAreDisabledAndReported()
    {
        StubOllamaServer server;
        QVERIFY(server.start());
        server.mode = StubOllamaServer::Mode::Redirect;
        setDeadlineMs(3000);

        gp::OllamaProvider provider(endpointFor(server));
        QFuture<gp::AiResult> future =
            provider.chat({ { QStringLiteral("user"), QStringLiteral("ping") } });

        const gp::AiResult r = waitResult(future, 10000);
        QVERIFY2(!r.ok, "a redirect must not be reported as success");
        QVERIFY2(r.errorMsg.contains(QStringLiteral("redirect"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("expected the redirect to be reported, got: ")
                            + r.errorMsg));
        QVERIFY2(r.errorMsg.contains(QStringLiteral("audit.invalid")),
                 "the redirect target must be named in the error");
        QCOMPARE(future.resultCount(), 1);

        // The Location target must never be dialed: exactly one connection to
        // the stub, and the target host is non-loopback so a follow attempt
        // would also surface as a second stub-visible or DNS event.
        QTest::qWait(300);
        drainEvents();
        QCOMPARE(server.connections, 1);
        QCOMPARE(future.resultCount(), 1);
    }
};

QTEST_GUILESS_MAIN(TestOllamaProvider)
#include "TestOllamaProvider.moc"
