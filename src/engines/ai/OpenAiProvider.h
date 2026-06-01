#pragma once
#include "IAiProvider.h"

namespace gp {

/// IAiProvider implementation for OpenAI ChatGPT (gpt-4o).
class OpenAiProvider : public IAiProvider {
public:
    explicit OpenAiProvider(const QString& apiKey = QString());
    QString         providerName() const override { return QStringLiteral("OpenAI ChatGPT"); }
    bool            isReady()      const override;
    bool            isPlausibleKey(const QString& key) const override;
    QFuture<AiResult> chat(const QList<AiMessage>& history, const AiOptions& opts = {}) override;
private:
    QString m_apiKey;
};

} // namespace gp
