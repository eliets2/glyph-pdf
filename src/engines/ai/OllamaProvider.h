// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "IAiProvider.h"

namespace gp {

/// IAiProvider implementation for Ollama local inference server.
/// Default endpoint: http://localhost:11434 (configurable via QSettings "ai/ollamaEndpoint").
class OllamaProvider : public IAiProvider {
public:
    explicit OllamaProvider(const QString& endpoint = QString());
    QString         providerName() const override { return QStringLiteral("Ollama (local)"); }
    bool            isReady()      const override;
    bool            isPlausibleKey(const QString& key) const override { Q_UNUSED(key); return true; }
    QFuture<AiResult> chat(const QList<AiMessage>& history, const AiOptions& opts = {}) override;
private:
    QString m_endpoint;
};

} // namespace gp
