// SPDX-License-Identifier: Apache-2.0
#include "OllamaProvider.h"
#include <QEventLoop>
#include <QStringList>
#include <QUrl>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHostAddress>
#include <QPromise>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <QSettings>
#include <QCoreApplication>

namespace gp {

// N-1 FIX: Only localhost/loopback or HTTPS endpoints are permitted.
// This prevents document content from being exfiltrated over cleartext HTTP to remote hosts.
//
// R04 (2026-09-05 plan, finding F03): the host decision is PARSED, not prefix-
// matched. The previous `host.startsWith("127.")` accepted hosts such as
// `127.audit.invalid`, a domain an attacker can register and point anywhere.
// The rules below are pinned by TestOllamaProvider's endpoint matrix:
//   * HTTP: the exact `localhost` spelling (QUrl case-folds the host), or a
//     host that PARSES as a literal IP address whose isLoopback() is true
//     (QHostAddress — 127.0.0.0/8, ::1, IPv4-mapped ::ffff:127.0.0.1). QUrl's
//     WHATWG host parser normalizes unusual numeric spellings (2130706433,
//     0x7f000001, 0177.0.0.1, 127.1) to their canonical dotted-quad before we
//     see the host, so those arrive as real loopback literals; hosts with a
//     trailing dot or an out-of-range octet do not parse and are rejected.
//   * user-info (http://user@…) is rejected outright: it is not part of the
//     Ollama endpoint contract, and it kills `http://<loopback>@attacker/`
//     display spoofs at the root (the parse would see host=attacker anyway).
//   * HTTPS keeps the SECFIX-5 explicit host allowlist and the opt-in
//     trust-any-HTTPS override.
//   * missing hosts and non-HTTP(S) schemes are rejected.
static bool isAllowedEndpoint(const QUrl& url)
{
    if (!url.isValid()) {
        qWarning() << "R04: Blocked invalid endpoint URL:" << url.toString();
        return false;
    }

    const QString scheme = url.scheme().toLower();

    if (scheme != QLatin1String("http") && scheme != QLatin1String("https")) {
        qWarning() << "R04: Blocked unsupported scheme:" << scheme;
        return false;
    }

    // User-info is never part of an Ollama endpoint — reject before any
    // host-based decision can be confused by it.
    if (!url.userInfo().isEmpty()) {
        qWarning() << "R04: Blocked endpoint with user-info (not part of the"
                      " Ollama endpoint contract):" << url.toString();
        return false;
    }

    const QString host = url.host();
    if (host.isEmpty()) {
        qWarning() << "R04: Blocked endpoint without a host:" << url.toString();
        return false;
    }

    // SECFIX-5: HTTPS must also pass a host allowlist - scheme alone does NOT prove
    // the destination is trusted. A tampered endpoint setting could otherwise
    // exfiltrate document content over TLS to an attacker-controlled host.
    if (scheme == QLatin1String("https")) {
        // Explicit power-user override (default false): trust any HTTPS host.
        if (QSettings().value(QStringLiteral("ai/ollamaTrustAnyHttps"), false).toBool())
            return true;
        const QStringList allowed = QSettings().value(
            QStringLiteral("ai/ollamaAllowedHosts"),
            QStringList{QStringLiteral("localhost"),
                        QStringLiteral("127.0.0.1"),
                        QStringLiteral("::1")}).toStringList();
        if (allowed.contains(host, Qt::CaseInsensitive))
            return true;
        qWarning() << "R04/SECFIX-5: Blocked HTTPS endpoint to non-allowlisted host:" << host;
        return false;
    }

    // HTTP only to loopback: the exact `localhost` spelling, or a parsed
    // literal loopback address. No name resolution, no prefix match.
    if (host == QLatin1String("localhost"))
        return true;

    const QHostAddress address(host);
    if (address.isNull() || !address.isLoopback()) {
        qWarning() << "R04: Blocked cleartext HTTP endpoint to non-loopback host:"
                   << host;
        return false;
    }
    return true;
}

static QString resolveEndpoint(const QString& supplied) {
    if (!supplied.isEmpty()) {
        // R04: an invalid user-supplied endpoint must fail loudly — silently
        // connecting to the stored/default endpoint instead would be a
        // misleading connection to an endpoint the user never chose.
        if (isAllowedEndpoint(QUrl(supplied)))
            return supplied;
        return QString();
    }
    const QString storedEndpoint = QSettings().value("ai/ollamaEndpoint",
                             QStringLiteral("http://localhost:11434")).toString();
    const QUrl endpointUrl(storedEndpoint);
    if (!isAllowedEndpoint(endpointUrl)) {
        qWarning() << "R04: Stored Ollama endpoint is not allowed:" << storedEndpoint
                   << "— falling back to http://localhost:11434";
        return QStringLiteral("http://localhost:11434");
    }
    return storedEndpoint;
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
    const QUrl endpointUrl(endpoint);
    if (endpoint.isEmpty() || !isAllowedEndpoint(endpointUrl)) {
        qWarning() << "R04: Request blocked — endpoint failed validation:"
                   << m_endpoint;
        const QString supplied = m_endpoint;
        return QtConcurrent::run([supplied]() -> AiResult {
            if (supplied.isEmpty())
                return {false, {},
                        QStringLiteral("Ollama endpoint is not configured "
                                       "(HTTP must be loopback — localhost, 127.x.x.x "
                                       "or [::1]; HTTPS must be an allowlisted host)")};
            return {false, {},
                    QStringLiteral("Ollama endpoint is not permitted: %1 "
                                   "(HTTP must be loopback — localhost, 127.x.x.x "
                                   "or [::1]; HTTPS must be an allowlisted host)")
                        .arg(supplied)};
        });
    }

    const QString model    = QSettings().value("ai/ollamaModel",
                                               QStringLiteral("llama3")).toString();

    // N-1 FIX: Truncate document content to prevent unbounded data exfiltration.
    constexpr int kMaxSystemPromptBytes = 32768; // 32 KB
    
    QString sysPrompt = opts.systemPrompt;
    if (sysPrompt.size() > kMaxSystemPromptBytes) {
        qWarning() << "N-1: Document content truncated from" << sysPrompt.size()
                   << "to" << kMaxSystemPromptBytes << "bytes before sending to Ollama";
        sysPrompt = sysPrompt.left(kMaxSystemPromptBytes);
    }

    QList<AiMessage> safeHistory;
    for (const AiMessage& m : history) {
        QString content = m.content;
        if (content.size() > kMaxSystemPromptBytes) {
            qWarning() << "N-1: Document content truncated from" << content.size()
                       << "to" << kMaxSystemPromptBytes << "bytes before sending to Ollama";
            content = content.left(kMaxSystemPromptBytes);
        }
        safeHistory.append({m.role, content});
    }
    // SECFIX-5: aggregate egress cap. The per-message cap above bounds each message,
    // but a long conversation could still send (N+1) x 32 KB. Bound the TOTAL bytes
    // leaving the machine; trim the OLDEST history entries first, never the system
    // prompt or the most recent message.
    constexpr int kMaxTotalBytes = kMaxSystemPromptBytes * 3; // 96 KB aggregate
    auto totalBytes = [&]() {
        int t = sysPrompt.size();
        for (const AiMessage& m : safeHistory) t += m.content.size();
        return t;
    };
    int before = totalBytes();
    while (totalBytes() > kMaxTotalBytes && safeHistory.size() > 1) {
        safeHistory.removeFirst(); // drop oldest, keep latest turn + system prompt
    }
    if (totalBytes() < before) {
        qWarning() << "N-1/SECFIX-5: aggregate content trimmed from" << before
                   << "to" << totalBytes() << "bytes (cap" << kMaxTotalBytes << ") before sending to Ollama";
    }

    // R03: test-configurable deadline (QSettings "ai/ollamaTimeoutMs", default
    // 20 s), bounded so a corrupt setting can neither busy-loop nor hang us.
    const int timeoutMs = qBound(50,
        QSettings().value(QStringLiteral("ai/ollamaTimeoutMs"), 20000).toInt(),
        600000);

    return QtConcurrent::run(
        [endpoint, model, safeHistory, sysPrompt, timeoutMs](QPromise<AiResult>& promise) {
            if (promise.isCanceled())
                return; // canceled while queued: no I/O, no result

            // Build Ollama /api/chat messages array
            QJsonArray msgs;
            if (!sysPrompt.isEmpty())
                msgs.append(QJsonObject{{"role","system"},{"content",sysPrompt}});
            for (const AiMessage& m : safeHistory)
                msgs.append(QJsonObject{{"role",m.role},{"content",m.content}});

            QJsonObject body;
            body["model"]    = model;
            body["messages"] = msgs;
            body["stream"]   = false;  // single-shot response

            // ── R03: bounded request lifetime, fully owned by this worker ──
            // The old implementation queued a GUI-thread callback capturing
            // worker-stack variables and a semaphore by reference behind a
            // fixed 20 s wait: a timeout could return while that pending
            // closure could still run against dead stack memory, and pending
            // I/O was never aborted. The network manager, reply, deadline
            // timer and event loop now live INSIDE the worker that owns the
            // whole request (Qt network objects need thread affinity and an
            // event loop — not the GUI thread). Every connection is loop-
            // local, terminates at most once via the `done` flag, and the
            // reply is aborted and destroyed in this thread before we return,
            // so no queued closure can outlive this stack frame.
            QNetworkAccessManager nam;
            QEventLoop loop;
            QTimer deadline;
            deadline.setSingleShot(true);
            QTimer cancelPoll;

            struct TerminalState {
                bool     done   = false;
                AiResult result;
            } state;

            auto finishOnce = [&state, &loop](AiResult r) {
                if (state.done)
                    return;
                state.done   = true;
                state.result = std::move(r);
                loop.quit();
            };

            QNetworkRequest req(QUrl(endpoint + QStringLiteral("/api/chat")));
            req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            // R04: automatic redirects are DISABLED. Qt 6's default redirect
            // policy would follow an http→http redirect to ANY host, so a
            // malicious/compromised local server could move a document-
            // content request to a remote destination. A 3xx is reported as
            // a terminal error instead (handled in the finished branch below).
            nam.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);
            QNetworkReply* reply = nam.post(req, QJsonDocument(body).toJson(
                                                       QJsonDocument::Compact));

            // Completion: reply finished before the deadline/cancel.
            QObject::connect(reply, &QNetworkReply::finished, &loop, [&]() {
                if (state.done)
                    return; // timeout/cancellation already terminated the request
                reply->deleteLater();
                const QVariant status = reply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute);
                if (reply->error() != QNetworkReply::NoError) {
                    if (status.isValid()) {
                        finishOnce(AiResult{false, {},
                            QStringLiteral("Ollama server error at %1: HTTP %2 — %3")
                                .arg(endpoint).arg(status.toInt())
                                .arg(reply->errorString())});
                    } else {
                        finishOnce(AiResult{false, {},
                            QStringLiteral("Ollama not reachable at %1: %2")
                                .arg(endpoint, reply->errorString())});
                    }
                    return;
                }
                // R04: redirects are disabled (ManualRedirectPolicy above) — the
                // 3xx arrives as a plain response. Report it instead of
                // following it or mis-parsing its body as malformed JSON.
                if (status.isValid() && status.toInt() / 100 == 3) {
                    const QUrl target = reply->attribute(
                        QNetworkRequest::RedirectionTargetAttribute).toUrl();
                    finishOnce(AiResult{false, {},
                        QStringLiteral("Ollama endpoint %1 redirected to %2 — "
                                       "automatic redirects are disabled")
                            .arg(endpoint, target.toString(QUrl::FullyEncoded))});
                    return;
                }
                const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                if (!doc.isObject()) {
                    finishOnce(AiResult{false, {},
                        QStringLiteral("Ollama returned malformed JSON from %1")
                            .arg(endpoint)});
                    return;
                }
                const QString text = doc.object().value(QStringLiteral("message"))
                                           .toObject().value(QStringLiteral("content"))
                                           .toString();
                if (text.isEmpty())
                    finishOnce(AiResult{false, {}, QStringLiteral("Empty response from Ollama")});
                else
                    finishOnce(AiResult{true, text, {}});
            });

            // Deadline: abort pending I/O, then terminate exactly once.
            QObject::connect(&deadline, &QTimer::timeout, &loop, [&]() {
                finishOnce(AiResult{false, {},
                    QStringLiteral("Ollama request timed out after %1 ms — endpoint %2")
                        .arg(timeoutMs).arg(endpoint)});
                reply->abort(); // the finished handler above is a no-op once done
            });

            // Caller-side cancellation (QFuture::cancel): poll and abort I/O.
            QObject::connect(&cancelPoll, &QTimer::timeout, &loop, [&]() {
                if (!promise.isCanceled())
                    return;
                finishOnce(AiResult{false, {}, QStringLiteral("Ollama request canceled")});
                reply->abort();
            });

            deadline.start(timeoutMs);
            cancelPoll.start(qBound(20, timeoutMs / 4, 100));
            loop.exec();

            // ── Teardown in the OWNING thread, while every local is alive ──
            deadline.stop();
            cancelPoll.stop();
            reply->disconnect();  // no reply signal can fire past this point
            reply->deleteLater();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            // `nam` leaves scope here and destroys any remaining reply on this
            // same thread — after all callbacks were disconnected and the loop
            // is no longer running. Nothing queued can reference this stack.
            (void)nam;

            if (promise.isCanceled())
                return; // cancellation wins: the future carries no result
            promise.addResult(state.result);
        });
}

} // namespace gp
