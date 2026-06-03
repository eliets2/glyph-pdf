#include <QtTest>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QList>
#include <QtConcurrent/QtConcurrent>
#include <memory>

#include "engines/ai/IAiProvider.h"
#include "engines/ai/OllamaProvider.h"

// ---------------------------------------------------------------------------
// Mock provider for CI (no network calls)
// ---------------------------------------------------------------------------

namespace {

class MockAiProvider : public gp::IAiProvider {
public:
    QString providerName() const override { return QStringLiteral("Mock"); }
    bool isReady()         const override { return true; }
    bool isPlausibleKey(const QString&) const override { return true; }

    QFuture<gp::AiResult> chat(const QList<gp::AiMessage>& history,
                                const gp::AiOptions&) override
    {
        const QString last = history.isEmpty() ? QString()
                                                : history.last().content;
        const gp::AiResult res{true, QStringLiteral("Echo: ") + last, {}};
        return QtConcurrent::run([res]() { return res; });
    }
};

} // anonymous namespace

// ---------------------------------------------------------------------------

class TestAiProvider : public QObject {
    Q_OBJECT

private slots:

    // ── Mock provider ─────────────────────────────────────────────────────

    void testMockIsReady() {
        MockAiProvider p;
        QVERIFY(p.isReady());
    }

    void testMockChatEchoes() {
        MockAiProvider p;
        QList<gp::AiMessage> history{{QStringLiteral("user"), QStringLiteral("Hello")}};
        QFutureWatcher<gp::AiResult> w;
        QEventLoop loop;
        connect(&w, &QFutureWatcher<gp::AiResult>::finished, &loop, &QEventLoop::quit);
        w.setFuture(p.chat(history, {}));
        loop.exec();
        const gp::AiResult r = w.result();
        QVERIFY(r.ok);
        QVERIFY2(r.text.contains("Hello"),
                 qPrintable(QStringLiteral("Expected 'Hello' in: ") + r.text));
    }

    void testMockEmptyHistoryNocrash() {
        MockAiProvider p;
        QFutureWatcher<gp::AiResult> w;
        QEventLoop loop;
        connect(&w, &QFutureWatcher<gp::AiResult>::finished, &loop, &QEventLoop::quit);
        w.setFuture(p.chat({}, {}));
        loop.exec();
        QVERIFY(w.result().ok);  // empty content → still succeeds
    }

    // ── Ollama provider (no network, contract checks) ─────────────────────

    void testOllamaNoKeyRequired() {
        gp::OllamaProvider p;
        // Ollama needs no key — isPlausibleKey always returns true
        QVERIFY(p.isPlausibleKey(QStringLiteral("")));
        QVERIFY(p.isPlausibleKey(QStringLiteral("anything")));
    }

    void testOllamaIsReadyWithDefaultEndpoint() {
        // OllamaProvider with default endpoint is considered "ready"
        // (endpoint is non-empty; actual reachability determined at chat() time)
        gp::OllamaProvider p;
        QVERIFY(p.isReady());
    }

    void testOllamaProviderName() {
        gp::OllamaProvider p;
        QVERIFY(!p.providerName().isEmpty());
        QVERIFY(p.providerName().toLower().contains("ollama"));
    }

    // ── Real round-trip (env-gated — QSKIP when Ollama absent) ───────────

    void testOllamaRealPing() {
        const QString endpoint = qEnvironmentVariable("OLLAMA_ENDPOINT",
                                                       "http://localhost:11434");
        gp::OllamaProvider prov(endpoint);
        // If the env variable OLLAMA_SKIP_REAL_PING is set, skip the network test
        if (!qEnvironmentVariable("OLLAMA_REAL_PING").isEmpty()) {
            QList<gp::AiMessage> ping{{QStringLiteral("user"), QStringLiteral("Say only: ok")}};
            gp::AiOptions opts; opts.maxTokens = 5;
            QFutureWatcher<gp::AiResult> w;
            QEventLoop loop;
            connect(&w, &QFutureWatcher<gp::AiResult>::finished, &loop, &QEventLoop::quit);
            w.setFuture(prov.chat(ping, opts));
            loop.exec();
            const gp::AiResult r = w.result();
            QVERIFY2(r.ok, qPrintable(QStringLiteral("Ollama real ping failed: ") + r.errorMsg));
            QVERIFY(!r.text.isEmpty());
        } else {
            QSKIP("OLLAMA_REAL_PING not set — skipping real Ollama network ping");
        }
    }
};

QTEST_MAIN(TestAiProvider)
#include "TestAiProvider.moc"
