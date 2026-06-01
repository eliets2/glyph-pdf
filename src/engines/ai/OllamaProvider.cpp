#include "OllamaProvider.h"
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent/QtConcurrent>
#include <QSettings>

namespace gp {

static QString resolveEndpoint(const QString& supplied) {
    if (!supplied.isEmpty()) return supplied;
    return QSettings().value("ai/ollamaEndpoint",
                             QStringLiteral("http://localhost:11434")).toString();
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
    const QString model    = QSettings().value("ai/ollamaModel",
                                               QStringLiteral("llama3")).toString();

    return QtConcurrent::run([endpoint, model, history, opts]() -> AiResult {
        // Build Ollama /api/chat messages array
        QJsonArray msgs;
        if (!opts.systemPrompt.isEmpty())
            msgs.append(QJsonObject{{"role","system"},{"content",opts.systemPrompt}});
        for (const AiMessage& m : history)
            msgs.append(QJsonObject{{"role",m.role},{"content",m.content}});

        QJsonObject body;
        body["model"]    = model;
        body["messages"] = msgs;
        body["stream"]   = false;  // single-shot response

        QNetworkAccessManager nam;
        QNetworkRequest req(QUrl(endpoint + QStringLiteral("/api/chat")));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QNetworkReply* reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            // Likely no local Ollama server running
            const QString err = reply->errorString();
            reply->deleteLater();
            return {false, {}, QStringLiteral("Ollama not reachable at %1: %2").arg(endpoint, err)};
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();
        const QString text = doc["message"]["content"].toString();
        return text.isEmpty() ? AiResult{false,{},"Empty response from Ollama"} : AiResult{true,text,{}};
    });
}

} // namespace gp
