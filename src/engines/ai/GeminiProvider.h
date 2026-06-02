// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "IAiProvider.h"

namespace gp {

/// IAiProvider implementation for Google Gemini (gemini-1.5-flash).
class GeminiProvider : public IAiProvider {
public:
    explicit GeminiProvider(const QString& apiKey = QString());
    QString         providerName() const override { return QStringLiteral("Google Gemini"); }
    bool            isReady()      const override;
    bool            isPlausibleKey(const QString& key) const override;
    QFuture<AiResult> chat(const QList<AiMessage>& history, const AiOptions& opts = {}) override;
private:
    QString m_apiKey;
};

} // namespace gp
