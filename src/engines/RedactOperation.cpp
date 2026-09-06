// SPDX-License-Identifier: Apache-2.0
#include "engines/RedactOperation.h"
#include "engines/SafeSave.h"
#include "engines/PdfEditorEngine.h"

#include <QFile>
#include <QPointer>
#include <QThread>
#include <podofo/podofo.h>

namespace gp {

std::atomic<RedactOperation::Fault> RedactOperation::s_faultForTesting{RedactOperation::Fault::None};

// finished(RedactResult) crosses threads as a queued connection (the worker
// thread emits; the host consumes on the caller's thread).
namespace {
[[maybe_unused]] const bool kRedactMetaTypesRegistered = [] {
    qRegisterMetaType<gp::RedactResult>("gp::RedactResult");
    qRegisterMetaType<gp::RedactStage>("gp::RedactStage");
    return true;
}();

// ER-2 user-presentable refusal (same contract as the engine guards,
// PdfEditorEngine.cpp applyRedactions/applyMarkRedactions, which stay in place
// as the hard engine-level stop).
const char* kSignedRefusal =
    "This document is digitally signed. Redacting it would leave the original "
    "content recoverable from the PDF revision history. Save an unsigned copy "
    "first (File > Save As), then redact the copy.";

// RAII removal of this operation's candidate file on every exit
// (CandidateFileGuard pattern, FormManager.cpp:67-75).
class CandidateFileGuard {
public:
    CandidateFileGuard() = default;
    explicit CandidateFileGuard(const QString& path) : m_path(path) {}
    ~CandidateFileGuard() { if (!m_path.isEmpty()) QFile::remove(m_path); }
    CandidateFileGuard(const CandidateFileGuard&) = delete;
    CandidateFileGuard& operator=(const CandidateFileGuard&) = delete;
    CandidateFileGuard(CandidateFileGuard&& other) noexcept : m_path(other.m_path) { other.m_path.clear(); }
    CandidateFileGuard& operator=(CandidateFileGuard&& other) noexcept {
        if (this != &other) {
            if (!m_path.isEmpty()) QFile::remove(m_path);
            m_path = other.m_path;
            other.m_path.clear();
        }
        return *this;
    }
private:
    QString m_path;
};
} // namespace

QString redactStageName(RedactStage stage)
{
    switch (stage) {
    case RedactStage::Preflight:       return QStringLiteral("Preflight");
    case RedactStage::Redacting:       return QStringLiteral("Redacting");
    case RedactStage::SavingCandidate: return QStringLiteral("SavingCandidate");
    case RedactStage::Validating:      return QStringLiteral("Validating");
    case RedactStage::Committing:      return QStringLiteral("Committing");
    case RedactStage::Sanitizing:      return QStringLiteral("Sanitizing");
    case RedactStage::Done:            return QStringLiteral("Done");
    }
    return QStringLiteral("Unknown");
}

void RedactOperation::setFaultForTesting(Fault fault)
{
    s_faultForTesting.store(fault);
}

RedactOperation::Fault RedactOperation::faultForTesting()
{
    return s_faultForTesting.load();
}

std::shared_ptr<IPdfEditorEngine> RedactOperation::defaultEngineFactory()
{
    return std::make_shared<PdfEditorEngine>();
}

RedactOperation::RedactOperation(RedactRequest request, QObject* parent)
    : QObject(parent), m_request(std::move(request)), m_engineFactory(&defaultEngineFactory)
{
}

RedactOperation::~RedactOperation() = default;

void RedactOperation::setEngineFactory(EngineFactory factory)
{
    m_engineFactory = std::move(factory);
}

void RedactOperation::setPageBoundaryHook(std::function<void(int pagesDone)> hook)
{
    m_pageBoundaryHook = std::move(hook);
}

void RedactOperation::cancel()
{
    m_cancelRequested.store(true);
}

// Partial-result recovery: load the COMMITTED redacted file in a disposable
// engine and sanitize it. A failure here cannot corrupt the redacted artifact
// (the engine re-saves to the sanitized destination only).
bool RedactOperation::sanitizeCommittedFile(const QString& committedRedactedPath,
                                            const QString& sanitizedDestination,
                                            QString* err)
{
    auto engine = defaultEngineFactory();
    if (!engine) {
        if (err) *err = QStringLiteral("could not create an engine for sanitization");
        return false;
    }
    if (!engine->loadDocumentForEditing(committedRedactedPath)) {
        if (err) *err = QStringLiteral("could not reopen the committed redacted file: %1")
                            .arg(engine->lastError().userMessage);
        return false;
    }
    if (!engine->sanitizeDocument(sanitizedDestination)) {
        if (err) *err = QStringLiteral("sanitization failed: %1").arg(engine->lastError().userMessage);
        return false;
    }
    return true;
}

bool RedactOperation::checkCancel(RedactResult* result) const
{
    if (!m_cancelRequested.load()) return false;
    result->outcome = RedactOutcome::Canceled;
    result->failedStage.clear();
    result->error = QStringLiteral("Redaction canceled before the output was committed; "
                                   "no output was written.");
    return true;
}

void RedactOperation::start()
{
    QPointer<RedactOperation> self(this);
    QThread* worker = QThread::create([self]() {
        if (self) self->run();
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void RedactOperation::run()
{
    const Fault fault = s_faultForTesting.load();
    RedactResult result;
    result.pagesTotal = m_request.redactionsByPage.size();

    // `ok` is false once a stage failed or a cancel was honored; `result`
    // carries the truthful terminal state either way.
    bool ok = true;

    auto fail = [&result, &ok](RedactStage stage, const QString& message) {
        result.outcome = RedactOutcome::Failed;
        result.failedStage = redactStageName(stage);
        result.error = message;
        ok = false;
    };

    // Disposable session: a private engine, loaded from sourcePath. The live
    // viewer's engine is never touched, so cancel/failure cannot leave the
    // live document half-redacted (the mutated state is simply discarded).
    std::shared_ptr<IPdfEditorEngine> engine = m_engineFactory ? m_engineFactory() : nullptr;

    emit stageChanged(RedactStage::Preflight, 0, result.pagesTotal);

    // ── Preflight: ER-2 re-check + page ranges (no writes have happened) ────
    int sourcePageCount = 0;
    if (!engine) {
        fail(RedactStage::Preflight, QStringLiteral("Could not create an editing engine."));
    } else {
        if (m_request.redactionsByPage.isEmpty()) {
            fail(RedactStage::Preflight, QStringLiteral("No redaction marks were supplied."));
        } else if (!engine->loadDocumentForEditing(m_request.sourcePath)) {
            fail(RedactStage::Preflight,
                 QStringLiteral("Could not open the document: %1").arg(engine->lastError().userMessage));
        } else if (engine->hasPdfSignatures()) {
            fail(RedactStage::Preflight, QString::fromLatin1(kSignedRefusal));
        } else {
            // Source page count via PoDoFo (independent of any live session),
            // and every requested page must exist.
            try {
                PoDoFo::PdfMemDocument doc;
                doc.Load(m_request.sourcePath.toUtf8().constData());
                sourcePageCount = static_cast<int>(doc.GetPages().GetCount());
            } catch (const PoDoFo::PdfError& e) {
                fail(RedactStage::Preflight,
                     QStringLiteral("Could not read the source document: %1")
                         .arg(QString::fromLatin1(e.what())));
            }
            if (ok) {
                for (auto it = m_request.redactionsByPage.constBegin();
                     it != m_request.redactionsByPage.constEnd(); ++it) {
                    if (it.key() < 0 || it.key() >= sourcePageCount) {
                        fail(RedactStage::Preflight,
                             QStringLiteral("A redaction mark targets page %1, but the document "
                                            "has %2 page(s).").arg(it.key() + 1).arg(sourcePageCount));
                        break;
                    }
                }
            }
        }
    }

    // ── Redacting: per page; cancel/faults honored BETWEEN pages only ───────
    QString candidate;
    CandidateFileGuard candidateGuard;
    if (ok) {
        emit stageChanged(RedactStage::Redacting, 0, result.pagesTotal);
        int pagesDone = 0;
        for (auto it = m_request.redactionsByPage.constBegin();
             it != m_request.redactionsByPage.constEnd(); ++it) {
            if (checkCancel(&result)) { ok = false; break; }
            if (m_pageBoundaryHook) m_pageBoundaryHook(pagesDone);
            if (checkCancel(&result)) { ok = false; break; }
            if (fault == Fault::Redact && pagesDone == 1) {
                // Plan acceptance case: "engine failure after one page".
                fail(RedactStage::Redacting,
                     QStringLiteral("Redaction failed on page %1: injected redaction failure "
                                    "(test seam). The original document was not modified.")
                         .arg(it.key() + 1));
                result.pagesProcessed = pagesDone;
                break;
            }
            if (!engine->applyRedactions(it.key(), it.value())) {
                fail(RedactStage::Redacting,
                     QStringLiteral("Redaction failed on page %1: %2. The original document "
                                    "was not modified.").arg(it.key() + 1)
                         .arg(engine->lastError().userMessage));
                result.pagesProcessed = pagesDone;
                break;
            }
            ++pagesDone;
            result.pagesProcessed = pagesDone;
            emit stageChanged(RedactStage::Redacting, pagesDone, result.pagesTotal);
        }
    }

    // ── SavingCandidate: serialize to a unique temp file, never the dest ────
    if (ok) {
        if (checkCancel(&result)) {
            ok = false;
        } else {
            emit stageChanged(RedactStage::SavingCandidate, result.pagesProcessed, result.pagesTotal);
            if (fault == Fault::CandidateSave) {
                fail(RedactStage::SavingCandidate,
                     QStringLiteral("Saving the redacted candidate failed: injected candidate-save "
                                    "failure (test seam). The original document was not modified."));
            } else if (!SafeSave::makeUniqueCandidate(&candidate, &result.error)) {
                fail(RedactStage::SavingCandidate, result.error);
            } else {
                candidateGuard = CandidateFileGuard(candidate);
                if (!engine->saveDocument(candidate)) {
                    fail(RedactStage::SavingCandidate,
                         QStringLiteral("Saving the redacted candidate failed: %1. The original "
                                        "document was not modified.")
                             .arg(engine->lastError().userMessage));
                }
            }
        }
    }

    // ── Validating: reopen the candidate; page count must be unchanged ──────
    if (ok) {
        if (checkCancel(&result)) {
            ok = false;
        } else {
            emit stageChanged(RedactStage::Validating, result.pagesProcessed, result.pagesTotal);
            if (fault == Fault::Validation) {
                fail(RedactStage::Validating,
                     QStringLiteral("The redacted output failed validation: injected validation "
                                    "failure (test seam)."));
            } else {
                try {
                    PoDoFo::PdfMemDocument reopened;
                    reopened.Load(candidate.toUtf8().constData());
                    const int reopenedPages = static_cast<int>(reopened.GetPages().GetCount());
                    if (reopenedPages != sourcePageCount) {
                        fail(RedactStage::Validating,
                             QStringLiteral("The redacted output failed validation: page count "
                                            "changed (%1 -> %2).").arg(sourcePageCount).arg(reopenedPages));
                    }
                } catch (const PoDoFo::PdfError& e) {
                    fail(RedactStage::Validating,
                         QStringLiteral("The redacted output is not a valid PDF: %1")
                             .arg(QString::fromLatin1(e.what())));
                }
            }
        }
    }

    // ── Committing: bounded copy into QSaveFile + checked commit() ──────────
    if (ok) {
        if (checkCancel(&result)) {
            ok = false;
        } else {
            emit stageChanged(RedactStage::Committing, result.pagesProcessed, result.pagesTotal);
            QString commitErr;
            const SafeSave::CommitFaultForTesting commitFault =
                (fault == Fault::Commit) ? SafeSave::CommitFaultForTesting::FailBeforeCommit
                                         : SafeSave::CommitFaultForTesting::None;
            if (!SafeSave::commitFileToDestination(candidate, m_request.destinationPath,
                                                   &commitErr, commitFault)) {
                fail(RedactStage::Committing,
                     QStringLiteral("Committing the redacted file failed: %1. The destination "
                                    "was left unchanged.").arg(commitErr));
            } else {
                result.destination = m_request.destinationPath; // RedactionCommitted
            }
        }
    }

    // ── Sanitizing: load the COMMITTED redacted file, sanitize into the copy ─
    if (ok) {
        if (!m_request.sanitize) {
            result.outcome = RedactOutcome::Completed;
        } else {
            emit stageChanged(RedactStage::Sanitizing, result.pagesProcessed, result.pagesTotal);
            // Cancellation is not honored past the commit: the redacted
            // artifact already exists and is honestly reported below.
            QString sanitizeErr;
            bool sanitized;
            if (fault == Fault::Sanitize) {
                sanitized = false;
                sanitizeErr = QStringLiteral("injected sanitize failure (test seam)");
            } else {
                // The sanitize pass reads the committed destination (fresh
                // disposable session) — never the in-memory redaction state.
                sanitized = sanitizeCommittedFile(result.destination,
                                                  m_request.sanitizedDestinationPath, &sanitizeErr);
            }
            if (sanitized) {
                result.outcome = RedactOutcome::Completed;
                result.sanitizedDestination = m_request.sanitizedDestinationPath;
            } else {
                // Labeled partial state: the redacted file IS committed; only
                // the sanitize step failed. Never masked as plain success.
                result.outcome = RedactOutcome::PartialRedactedOnly;
                result.failedStage = redactStageName(RedactStage::Sanitizing);
                result.error = QStringLiteral("The redacted copy was saved, but sanitization "
                                              "failed: %1").arg(sanitizeErr);
            }
        }
    } else if (result.outcome != RedactOutcome::Canceled) {
        result.outcome = RedactOutcome::Failed; // safety net: truthful failure
    }

    // ── Done ────────────────────────────────────────────────────────────────
    emit stageChanged(RedactStage::Done, result.pagesProcessed, result.pagesTotal);
    emit finished(result);
}

} // namespace gp
