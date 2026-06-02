// SPDX-License-Identifier: Apache-2.0
#include "GeminiProvider.h"
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent/QtConcurrent>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

namespace gp {

static QString resolveGeminiKey(const QString& supplied) {
    if (!supplied.isEmpty()) return supplied;
    return QSettings().value("ai/keyGeminiCached").toString();
}

GeminiProvider::GeminiProvider(const QString& apiKey) : m_apiKey(apiKey) {}

bool GeminiProvider::isReady() const {
    return !resolveGeminiKey(m_apiKey).isEmpty();
}

bool GeminiProvider::isPlausibleKey(const QString& key) const {
    return key.length() > 20;  // Gemini keys are variable-format
}

QFuture<AiResult> GeminiProvider::chat(const QList<AiMessage>& history, const AiOptions& opts) {
    const QString key = resolveGeminiKey(m_apiKey);
    if (key.isEmpty())
        return QtConcurrent::run([]() -> AiResult {
            return {false, {}, QStringLiteral("No Gemini API key configured.")};
        });

    return QtConcurrent::run([key, history, opts]() -> AiResult {
        // Gemini generateContent endpoint
        QUrl url(QStringLiteral(
            "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent"));
        QUrlQuery q; q.addQueryItem("key", key);
        url.setQuery(q);

        // Build contents array (convert role names: "assistant" → "model")
        QJsonArray contents;
        for (const AiMessage& m : history) {
            const QString role = (m.role == QLatin1String("assistant"))
                                 ? QStringLiteral("model") : m.role;
            contents.append(QJsonObject{
                {"role", role},
                {"parts", QJsonArray{QJsonObject{{"text", m.content}}}}
            });
        }

        QJsonObject body;
        body["contents"] = contents;
        if (!opts.systemPrompt.isEmpty())
            body["systemInstruction"] = QJsonObject{
                {"parts", QJsonArray{QJsonObject{{"text", opts.systemPrompt}}}}
            };

        QNetworkAccessManager nam;
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QNetworkReply* reply = nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            return {false, {}, QStringLiteral("Network error: %1").arg(err)};
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();
        const QString text = doc["candidates"][0]["content"]["parts"][0]["text"].toString();
        return text.isEmpty() ? AiResult{false,{},"Empty response"} : AiResult{true,text,{}};
    });
}

} // namespace gp
