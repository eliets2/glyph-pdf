#pragma once
#include <QFuture>
#include <QList>
#include <QString>

namespace gp {

/// A single message in a chat history (role = "user" | "assistant" | "system").
struct AiMessage {
    QString role;
    QString content;
};

/// Tuning options for a single chat call.
struct AiOptions {
    int     maxTokens   = 1024;
    double  temperature = 0.7;
    QString systemPrompt;    ///< Prepended before chat history when non-empty
};

/// Result of an AI chat call.
struct AiResult {
    bool    ok      = false;
    QString text;            ///< Response text on success
    QString errorMsg;        ///< Error description on failure
};

/// Abstract AI provider.  Each implementation wraps one backend
/// (Anthropic Claude, OpenAI, Google Gemini, Ollama local).
class IAiProvider {
public:
    virtual ~IAiProvider() = default;

    /// Provider display name (e.g. "Anthropic Claude").
    virtual QString providerName() const = 0;

    /// True if the provider has a configured API key / endpoint.
    virtual bool isReady() const = 0;

    /// Submit a chat turn asynchronously.
    /// Returns a QFuture<AiResult> — connect via QFutureWatcher.
    virtual QFuture<AiResult> chat(const QList<AiMessage>& history,
                                   const AiOptions& opts = {}) = 0;

    /// Format-check an API key string (no network call).
    /// Returns true if the key looks plausibly valid for this provider.
    virtual bool isPlausibleKey(const QString& key) const = 0;

protected:
    IAiProvider() = default;
};

} // namespace gp
