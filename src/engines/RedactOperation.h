// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QRectF>
#include <atomic>
#include <functional>
#include <memory>

class IPdfEditorEngine;

namespace gp {

// ── U05: ONE transactional redaction operation behind BOTH entry paths ──────
//
// RedactMode ("Apply All Redactions") and SecurityController (ToolId::ApplyRedact)
// previously ran two divergent flows: the mode mutated the LIVE document and
// saved to a fixed `_redacted.pdf`, while the controller saved IN PLACE over
// the original and reported "Sanitized copy: …" without checking sanitize
// success. Both routed through PdfEditorEngine::saveDocument, which is a direct
// backend save (its linearize variant even removes the destination before
// copying) — not a transaction.
//
// RedactOperation mirrors the landed R01 form-save transaction instead:
//   Preflight  — load a DISPOSABLE private engine from sourcePath, re-check the
//                ER-2 signed-file refusal and page ranges (defense in depth
//                behind the UI guards and PdfEditorEngine.cpp:1383-1390 /
//                1424-1431, which stay untouched).
//   Redacting  — per page applyRedactions(page, rects); progress + cancel are
//                honored BETWEEN pages only (a page's content-stream excision
//                is atomic inside the engine; there is no mid-page cancellation).
//   SavingCandidate — saveDocument(candidate) into a unique temp file — NEVER
//                the destination (the engine's save is not transactional).
//   Validating — reopen the candidate, require a readable PDF with an unchanged
//                page count (FormManager.cpp:203-214 pattern).
//   Committing — gp::SafeSave::commitFileToDestination: bounded copy into
//                QSaveFile + checked commit(); on failure the destination is
//                byte-identical. The source is NEVER written.
//   Sanitizing — load the COMMITTED redacted file and sanitizeDocument() into
//                the sanitized destination, so a sanitize failure can never
//                corrupt the committed redacted artifact.
//
// Outcomes are explicit; partial failure is never masked by a success banner:
//   Completed           — every requested step committed.
//   PartialRedactedOnly — the redacted file IS committed; only sanitization
//                         failed (retry via sanitizeCommittedFile()).
//   Failed              — nothing written; the original is byte-identical.
//   Canceled            — honored at stage/page boundaries BEFORE the commit;
//                         once the redacted file is committed, cancellation is
//                         no longer honored (the artifact exists and is
//                         honestly reported instead).

struct RedactRequest {
    QString sourcePath;                          // original — NEVER written
    QString destinationPath;                     // redacted output (committed atomically)
    QMap<int, QList<QRectF>> redactionsByPage;   // 0-based page -> rects
    bool sanitize = false;
    QString sanitizedDestinationPath;            // required when sanitize
};

enum class RedactStage  { Preflight, Redacting, SavingCandidate, Validating, Committing, Sanitizing, Done };
enum class RedactOutcome { Completed, PartialRedactedOnly, Failed, Canceled };

// User-presentable stage name (used for failedStage and logs).
QString redactStageName(RedactStage stage);

struct RedactResult {
    RedactOutcome outcome = RedactOutcome::Failed;
    QString destination;              // committed redacted file ("" if none)
    QString sanitizedDestination;     // committed sanitized file ("" if none)
    int pagesProcessed = 0;           // pages successfully redacted
    int pagesTotal = 0;               // pages with marks (the Redacting scope)
    QString failedStage;              // redactStageName of the failing stage
    QString error;                    // user-presentable reason
};

class RedactOperation : public QObject {
    Q_OBJECT
public:
    // Deterministic test seam, mirroring FormManager::setSaveFaultForTesting.
    enum class Fault { None = 0, Redact, CandidateSave, Validation, Commit, Sanitize };
    static void setFaultForTesting(Fault fault);
    static Fault faultForTesting();

    // Disposable session: the operation NEVER uses the live viewer's engine
    // (a failed or canceled run must not leave the live document half-redacted).
    // It builds a private engine through this factory; the default creates a
    // fresh PdfEditorEngine. Tests may inject a different factory.
    using EngineFactory = std::function<std::shared_ptr<IPdfEditorEngine>()>;
    static std::shared_ptr<IPdfEditorEngine> defaultEngineFactory();
    void setEngineFactory(EngineFactory factory);

    explicit RedactOperation(RedactRequest request, QObject* parent = nullptr);
    ~RedactOperation() override;

    // Test/progress seam: invoked on the worker thread at each page boundary
    // with the number of pages applied so far. Tests use it to cancel() at an
    // exact boundary; hosts may use it for fine-grained progress.
    void setPageBoundaryHook(std::function<void(int pagesDone)> hook);

    void start();    // run() on a worker QThread (SecurityController lifetime
                     // pattern: weak engine, QPointer self, queued finished).
    void cancel();   // cooperative — honored at stage/page boundaries only.
    void run();      // synchronous execution of the whole state machine.

    // Partial-result recovery (the "Retry sanitize" action): sanitize an
    // ALREADY-COMMITTED redacted file into `sanitizedDestination` using a
    // disposable engine. Never touches the original source document.
    static bool sanitizeCommittedFile(const QString& committedRedactedPath,
                                      const QString& sanitizedDestination,
                                      QString* err);

signals:
    void stageChanged(gp::RedactStage stage, int pagesDone, int pagesTotal);
    void finished(const gp::RedactResult& result);

private:
    bool checkCancel(RedactResult* result) const;

    RedactRequest m_request;
    EngineFactory m_engineFactory;
    std::function<void(int pagesDone)> m_pageBoundaryHook;
    std::atomic<bool> m_cancelRequested{false};
    static std::atomic<Fault> s_faultForTesting;
};

} // namespace gp

Q_DECLARE_METATYPE(gp::RedactResult)
Q_DECLARE_METATYPE(gp::RedactStage)
