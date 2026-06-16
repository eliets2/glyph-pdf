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

    // ── AR-1 D5: AIChatPanel void* UAF guard ─────────────────────────────
    // Pre-fix: cursor item stored as void* in a QVariant property; list clear
    //   or document switch between onSend and onAiFinished dangled the ptr.
    // Post-fix: row index stored in m_cursorRow; looked up safely via
    //   m_msgs->item(row) in onAiFinished. Input disabled while in-flight.
    // This guard test reads the AIChatPanel source and asserts the forbidden
    // pattern is absent and the safe pattern is present.
    void testAR1D5_noCursorItemVoidPtrProperty()
    {
#ifndef SOURCE_DIR
#define SOURCE_DIR "."
#endif
        const QString srcPath = QStringLiteral(SOURCE_DIR)
            + "/src/modes/AIChatPanel.cpp";

        QFile src(srcPath);
        if (!src.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QSKIP("Cannot open AIChatPanel.cpp for source inspection — skipping");
        }
        const QString content = QString::fromUtf8(src.readAll());
        src.close();

        // Banned: raw void* property round-trip
        const bool hasVoidPtrPattern =
            content.contains(QStringLiteral("static_cast<void*>")) ||
            content.contains(QStringLiteral("value<void*>"));

        QVERIFY2(!hasVoidPtrPattern,
            "AIChatPanel must not store cursor item as void* (AR-1 D5)");

        // Required: row-based safe lookup
        QVERIFY2(content.contains(QStringLiteral("m_cursorRow")),
            "AIChatPanel must use m_cursorRow for safe cursor tracking (AR-1 D5)");

        // Required: input disabled while in-flight
        QVERIFY2(content.contains(QStringLiteral("setInputEnabled")),
            "AIChatPanel must disable input while request is in-flight (AR-1 D5)");
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
