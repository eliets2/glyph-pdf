// SPDX-License-Identifier: Apache-2.0
#include "AnthropicProvider.h"
#include "core/CredentialManager.h"

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

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

static QString resolveKey(const QString& supplied)
{
    if (!supplied.isEmpty()) return supplied;
    // Try encrypted credential store first
    CredentialManager cm;
    if (cm.hasKey(QStringLiteral("Anthropic")))
        return cm.readKey(QStringLiteral("Anthropic"));
    // Fall back to plain-text QSettings (convenience; key stored by PreferencesDialog)
    return QSettings().value("ai/keyAnthropicCached").toString();
}

// ---------------------------------------------------------------------------
// AnthropicProvider
// ---------------------------------------------------------------------------

AnthropicProvider::AnthropicProvider(const QString& apiKey)
    : m_apiKey(apiKey)
{}

bool AnthropicProvider::isReady() const
{
    const QString key = resolveKey(m_apiKey);
    return !key.isEmpty() && key.startsWith(QLatin1String("sk-ant-"));
}

bool AnthropicProvider::isPlausibleKey(const QString& key) const
{
    return key.startsWith(QLatin1String("sk-ant-")) && key.length() > 20;
}

QFuture<AiResult> AnthropicProvider::chat(const QList<AiMessage>& history,
                                           const AiOptions& opts)
{
    const QString key = resolveKey(m_apiKey);
    if (key.isEmpty()) {
        return QtConcurrent::run([]() -> AiResult {
            return {false, {}, QStringLiteral("No Anthropic API key configured.")};
        });
    }

    // Capture by value so the lambda is safe across thread boundaries
    return QtConcurrent::run([key, history, opts]() -> AiResult {
        // Build messages JSON array
        QJsonArray msgs;
        for (const AiMessage& m : history) {
            msgs.append(QJsonObject{{"role", m.role}, {"content", m.content}});
        }

        QJsonObject body;
        body["model"]      = QStringLiteral("claude-3-5-sonnet-20241022");
        body["max_tokens"] = opts.maxTokens;
        body["messages"]   = msgs;
        if (!opts.systemPrompt.isEmpty())
            body["system"] = opts.systemPrompt;

        const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

        QNetworkAccessManager nam;
        QNetworkRequest req(QUrl(QStringLiteral("https://api.anthropic.com/v1/messages")));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("x-api-key",         key.toUtf8());
        req.setRawHeader("anthropic-version", "2023-06-01");

        QNetworkReply* reply = nam.post(req, payload);

        // Block this worker thread until the reply finishes
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            return {false, {}, QStringLiteral("Network error: %1").arg(err)};
        }

        const QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError)
            return {false, {}, QStringLiteral("JSON parse error: %1").arg(pe.errorString())};

        const QJsonObject root = doc.object();
        // Anthropic response: { "content": [{"type":"text","text":"..."}], ... }
        const QJsonArray content = root["content"].toArray();
        QString text;
        for (const QJsonValue& v : content) {
            const QJsonObject block = v.toObject();
            if (block["type"].toString() == QLatin1String("text"))
                text += block["text"].toString();
        }

        if (text.isEmpty()) {
            // Might be an error response: { "error": { "message": "..." } }
            if (root.contains("error")) {
                return {false, {}, root["error"].toObject()["message"].toString()};
            }
            return {false, {}, QStringLiteral("Empty response from Anthropic API")};
        }

        return {true, text, {}};
    });
}

} // namespace gp
