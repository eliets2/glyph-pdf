// SPDX-License-Identifier: Apache-2.0
#include "OllamaProvider.h"
#include <QEventLoop>
#include <QStringList>
#include <QUrl>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent/QtConcurrent>
#include <QSettings>
#include <QSemaphore>
#include <QCoreApplication>

namespace gp {

// N-1 FIX: Only localhost/loopback or HTTPS endpoints are permitted.
// This prevents document content from being exfiltrated over cleartext HTTP to remote hosts.
static bool isAllowedEndpoint(const QUrl& url)
{
    if (!url.isValid()) return false;

    const QString scheme = url.scheme().toLower();
    const QString host   = url.host().toLower();

    // SECFIX-5: HTTPS must also pass a host allowlist - scheme alone does NOT prove
    // the destination is trusted. A tampered endpoint setting could otherwise
    // exfiltrate document content over TLS to an attacker-controlled host.
    if (scheme == QLatin1String("https")) {
        // Explicit power-user override (default false): trust any HTTPS host.
        if (QSettings().value(QStringLiteral("ai/ollamaTrustAnyHttps"), false).toBool())
            return true;
        const QStringList allowed = QSettings().value(
            QStringLiteral("ai/ollamaAllowedHosts"),
            QStringList{QStringLiteral("localhost"),
                        QStringLiteral("127.0.0.1"),
                        QStringLiteral("::1")}).toStringList();
        if (allowed.contains(host, Qt::CaseInsensitive))
            return true;
        qWarning() << "N-1/SECFIX-5: Blocked HTTPS endpoint to non-allowlisted host:" << host;
        return false;
    }

    // HTTP only to loopback addresses
    if (scheme == QLatin1String("http")) {
        if (host == QLatin1String("localhost")  ||
            host == QLatin1String("127.0.0.1")  ||
            host == QLatin1String("::1")         ||
            host.startsWith(QLatin1String("127."))) { // 127.x.x.x range
            return true;
        }
        qWarning() << "N-1: Blocked cleartext HTTP endpoint to non-loopback host:" << host;
        return false;
    }

    qWarning() << "N-1: Blocked unsupported scheme:" << scheme;
    return false;
}

static QString resolveEndpoint(const QString& supplied) {
    if (!supplied.isEmpty()) {
        QUrl u(supplied);
        if (isAllowedEndpoint(u)) return supplied;
    }
    const QString storedEndpoint = QSettings().value("ai/ollamaEndpoint",
                             QStringLiteral("http://localhost:11434")).toString();
    const QUrl endpointUrl(storedEndpoint);
    if (!isAllowedEndpoint(endpointUrl)) {
        qWarning() << "N-1: Stored Ollama endpoint is not allowed:" << storedEndpoint
                   << "— falling back to http://localhost:11434";
        return QStringLiteral("http://localhost:11434");
    }
    return storedEndpoint;
}

OllamaProvider::OllamaProvider(const QString& endpoint) : m_endpoint(endpoint) {}

bool OllamaProvider::isReady() const {
    // Ollama needs no API key; assume ready if endpoint is non-empty
    return !resolveEndpoint(m_endpoint).isEmpty();
}

QFuture<AiResult> OllamaProvider::chat(const QList<AiMessage>& history,
                                        const AiOptions& opts)
{
    const QString endpoint = resolveEndpoint(m_endpoint);
    const QUrl endpointUrl(endpoint);
    if (!isAllowedEndpoint(endpointUrl)) {
        qWarning() << "N-1: Request blocked — endpoint failed runtime validation:" << endpoint;
        return QtConcurrent::run([]() -> AiResult {
            return {false, {}, "Ollama endpoint is not permitted (must be localhost or HTTPS)"};
        });
    }

    const QString model    = QSettings().value("ai/ollamaModel",
                                               QStringLiteral("llama3")).toString();

    // N-1 FIX: Truncate document content to prevent unbounded data exfiltration.
    constexpr int kMaxSystemPromptBytes = 32768; // 32 KB
    
    QString sysPrompt = opts.systemPrompt;
    if (sysPrompt.size() > kMaxSystemPromptBytes) {
        qWarning() << "N-1: Document content truncated from" << sysPrompt.size()
                   << "to" << kMaxSystemPromptBytes << "bytes before sending to Ollama";
        sysPrompt = sysPrompt.left(kMaxSystemPromptBytes);
    }

    QList<AiMessage> safeHistory;
    for (const AiMessage& m : history) {
        QString content = m.content;
        if (content.size() > kMaxSystemPromptBytes) {
            qWarning() << "N-1: Document content truncated from" << content.size()
                       << "to" << kMaxSystemPromptBytes << "bytes before sending to Ollama";
            content = content.left(kMaxSystemPromptBytes);
        }
        safeHistory.append({m.role, content});
    }
    // SECFIX-5: aggregate egress cap. The per-message cap above bounds each message,
    // but a long conversation could still send (N+1) x 32 KB. Bound the TOTAL bytes
    // leaving the machine; trim the OLDEST history entries first, never the system
    // prompt or the most recent message.
    constexpr int kMaxTotalBytes = kMaxSystemPromptBytes * 3; // 96 KB aggregate
    auto totalBytes = [&]() {
        int t = sysPrompt.size();
        for (const AiMessage& m : safeHistory) t += m.content.size();
        return t;
    };
    int before = totalBytes();
    while (totalBytes() > kMaxTotalBytes && safeHistory.size() > 1) {
        safeHistory.removeFirst(); // drop oldest, keep latest turn + system prompt
    }
    if (totalBytes() < before) {
        qWarning() << "N-1/SECFIX-5: aggregate content trimmed from" << before
                   << "to" << totalBytes() << "bytes (cap" << kMaxTotalBytes << ") before sending to Ollama";
    }

    return QtConcurrent::run([endpoint, model, safeHistory, sysPrompt]() -> AiResult {
        // Build Ollama /api/chat messages array
        QJsonArray msgs;
        if (!sysPrompt.isEmpty())
            msgs.append(QJsonObject{{"role","system"},{"content",sysPrompt}});
        for (const AiMessage& m : safeHistory)
            msgs.append(QJsonObject{{"role",m.role},{"content",m.content}});

        QJsonObject body;
        body["model"]    = model;
        body["messages"] = msgs;
        body["stream"]   = false;  // single-shot response

        // Run the network call on the main thread (QNAM is not thread-safe)
        QByteArray replyData;
        bool replyOk = false;
        QString replyError;
        QSemaphore sem(0);
        QMetaObject::invokeMethod(qApp, [&]() {
            static QNetworkAccessManager s_nam;
            QNetworkRequest req(QUrl(endpoint + QStringLiteral("/api/chat")));
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            QNetworkReply *reply = s_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
            QObject::connect(reply, &QNetworkReply::finished, qApp, [&, reply]() {
                replyOk = (reply->error() == QNetworkReply::NoError);
                if (replyOk)
                    replyData = reply->readAll();
                else
                    replyError = reply->errorString();
                reply->deleteLater();
                sem.release();
            });
        }, Qt::QueuedConnection);
        if (!sem.tryAcquire(1, 20000)) {
            return AiResult{false, {}, QStringLiteral("Ollama request timed out (20s) — main event loop may be blocked")};
        }

        if (!replyOk) {
            // Likely no local Ollama server running
            return {false, {}, QStringLiteral("Ollama not reachable at %1: %2").arg(endpoint, replyError)};
        }

        const QJsonDocument doc = QJsonDocument::fromJson(replyData);
        const QString text = doc["message"]["content"].toString();
        return text.isEmpty() ? AiResult{false,{},"Empty response from Ollama"} : AiResult{true,text,{}};
    });
}

} // namespace gp
