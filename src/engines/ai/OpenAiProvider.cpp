#include "OpenAiProvider.h"
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

static QString resolveOpenAiKey(const QString& supplied) {
    if (!supplied.isEmpty()) return supplied;
    return QSettings().value("ai/keyOpenAICached").toString();
}

OpenAiProvider::OpenAiProvider(const QString& apiKey) : m_apiKey(apiKey) {}

bool OpenAiProvider::isReady() const {
    const QString k = resolveOpenAiKey(m_apiKey);
    return !k.isEmpty() && k.startsWith(QLatin1String("sk-"));
}

bool OpenAiProvider::isPlausibleKey(const QString& key) const {
    return key.startsWith(QLatin1String("sk-")) && key.length() > 20;
}

QFuture<AiResult> OpenAiProvider::chat(const QList<AiMessage>& history, const AiOptions& opts) {
    const QString key = resolveOpenAiKey(m_apiKey);
    if (key.isEmpty())
        return QtConcurrent::run([]() -> AiResult {
            return {false, {}, QStringLiteral("No OpenAI API key configured.")};
        });

    return QtConcurrent::run([key, history, opts]() -> AiResult {
        QJsonArray msgs;
        if (!opts.systemPrompt.isEmpty())
            msgs.append(QJsonObject{{"role","system"},{"content",opts.systemPrompt}});
        for (const AiMessage& m : history)
            msgs.append(QJsonObject{{"role",m.role},{"content",m.content}});

        QJsonObject body;
        body["model"]      = QStringLiteral("gpt-4o");
        body["max_tokens"] = opts.maxTokens;
        body["messages"]   = msgs;

        QNetworkAccessManager nam;
        QNetworkRequest req(QUrl(QStringLiteral("https://api.openai.com/v1/chat/completions")));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        req.setRawHeader("Authorization", ("Bearer " + key).toUtf8());

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
        const QString text = doc["choices"][0]["message"]["content"].toString();
        return text.isEmpty() ? AiResult{false,{},"Empty response"} : AiResult{true,text,{}};
    });
}

} // namespace gp
