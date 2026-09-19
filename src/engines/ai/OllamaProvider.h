// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "IAiProvider.h"

namespace gp {

/// IAiProvider implementation for Ollama local inference server.
/// Default endpoint: http://localhost:11434 (configurable via QSettings "ai/ollamaEndpoint").
/// R24 wiring closure: the stored user endpoint meets the machine policy
/// (PolicyController, key ai/ollamaEndpoint) in ONE place — resolveEndpoint().
/// A policy that manages the key with an EMPTY value disables AI chat for the
/// whole machine: the endpoint resolves to empty (no silent fallback to the
/// default), isReady() reports honestly unavailable, and chat() refuses with
/// a whyNot naming the policy.
class OllamaProvider : public IAiProvider {
public:
    explicit OllamaProvider(const QString& endpoint = QString());
    QString         providerName() const override { return QStringLiteral("Ollama (local)"); }
    bool            isReady()      const override;
    bool            isPlausibleKey(const QString& key) const override { Q_UNUSED(key); return true; }
    QFuture<AiResult> chat(const QList<AiMessage>& history, const AiOptions& opts = {}) override;

    // The endpoint gate: `supplied` (constructor/dialog endpoint) wins when
    // non-empty and allowed; otherwise the stored user pref is resolved
    // through the machine policy (ai/ollamaEndpoint). Empty result = no
    // endpoint may be used (policy-disabled or unusable configuration).
    // Static + public so tests pin the EXACT endpoint the provider will use.
    static QString resolveEndpoint(const QString& supplied);
private:
    QString m_endpoint;
};

} // namespace gp
