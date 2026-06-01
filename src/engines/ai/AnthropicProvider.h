#pragma once
#include "IAiProvider.h"
#include <QString>

namespace gp {

/// IAiProvider implementation for Anthropic Claude (claude-3-5-sonnet-20241022).
///
/// Authentication: API key read from QSettings "ai/keyAnthropicCached" or
/// CredentialManager service "Anthropic" at call time.
class AnthropicProvider : public IAiProvider {
public:
    explicit AnthropicProvider(const QString& apiKey = QString());

    QString         providerName() const override { return QStringLiteral("Anthropic Claude"); }
    bool            isReady()      const override;
    bool            isPlausibleKey(const QString& key) const override;
    QFuture<AiResult> chat(const QList<AiMessage>& history,
                           const AiOptions& opts = {}) override;

private:
    QString m_apiKey;
};

} // namespace gp
