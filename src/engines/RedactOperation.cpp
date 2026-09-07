// SPDX-License-Identifier: Apache-2.0
#include "engines/RedactOperation.h"
#include "engines/SafeSave.h"
#include "engines/PdfEditorEngine.h"

#include <QFile>
#include <QPointer>
#include <QThread>
#include <mutex>
#include <podofo/podofo.h>

// Windows headers pulled in transitively define `#define DrawText DrawTextW`,
// which would rewrite the PoDoFo painter calls below.
#ifdef DrawText
#undef DrawText
#endif

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

// ── D02: worker-durable execution state ─────────────────────────────────────
//
// The request, the atomic cancellation flag, the configuration seams, the
// overlap guard, and the guarded signal delivery live in ONE shared_ptr-held
// object; start() hands its own shared_ptr to the worker thread. Neither
// side's lifetime implies the other's:
//
//   * the worker never dereferences a RedactOperation — there is no
//     weak-pointer gate deciding whether the run happens (a QPointer observes
//     deletion; it cannot keep the object alive DURING a member call, so the
//     old `if (self) self->run()` shape was unsound by construction);
//   * destroying the UI-side operation neither crashes nor cancels an
//     in-flight run — ~RedactOperation only detaches the signal owner under
//     deliveryMutex, and the run continues to completion on this state;
//   * the request/cancel state survives the QObject by construction.
struct RedactOperation::ExecutionState {
    // Immutable after construction; copied/moved in once by the ctor.
    RedactRequest request;

    // Configuration seams. Mutex-guarded so a host setting them from the UI
    // thread never races the worker's reads at the page boundaries.
    std::mutex configMutex;
    EngineFactory engineFactory{&defaultEngineFactory};
    std::function<void(int pagesDone)> pageBoundaryHook;

    // Cooperative cancellation — shared atomically between the UI side and
    // the worker (the pre-existing contract, unchanged).
    std::atomic<bool> cancelRequested{false};

    // Overlap guard: start() cannot create overlapping runs of one mutable
    // operation (a second start() — or a start() after run() — is refused).
    std::atomic<bool> runStarted{false};
    bool tryBeginRun() { return !runStarted.exchange(true); }

    // Signal delivery: the worker emits ONLY through this guard. The owner is
    // detached under the same lock by ~RedactOperation BEFORE the QObject
    // dies, so an emission can never touch a destroyed operation (the mutex
    // is what makes the QPointer check sound — the QPointer alone is not).
    // Recursive because a direct-connected slot may destroy the owner from
    // inside an emission on the same thread; Qt tolerates sender deletion
    // during its own signal emission.
    std::recursive_mutex deliveryMutex;
    QPointer<RedactOperation> owner;

    void detachOwner() {
        std::lock_guard<std::recursive_mutex> lock(deliveryMutex);
        owner.clear();
    }

    // Signals are public member functions; emitting through the guarded owner
    // from the worker thread queues them to receivers on their own threads,
    // exactly like the pre-fix direct emissions did.
    void emitStage(RedactStage stage, int pagesDone, int pagesTotal) {
        std::lock_guard<std::recursive_mutex> lock(deliveryMutex);
        if (owner) owner->stageChanged(stage, pagesDone, pagesTotal);
    }
    void emitFinished(const RedactResult& result) {
        std::lock_guard<std::recursive_mutex> lock(deliveryMutex);
        if (owner) owner->finished(result);
    }

    bool checkCancel(RedactResult* result) {
        if (!cancelRequested.load()) return false;
        result->outcome = RedactOutcome::Canceled;
        result->failedStage.clear();
        result->error = QStringLiteral("Redaction canceled before the output was committed; "
                                       "no output was written.");
        return true;
    }

    // The whole transaction (moved verbatim from RedactOperation::run).
    void execute();
};

// ── §9.8 P1: optional overlay text on the burn-in boxes ─────────────────────
namespace {
// 6–8pt band (Acrobat uses ~6pt for its reason codes); 7pt reads cleanly.
constexpr double kOverlayFontSize = 7.0;
// Auto-fit precedent (signature appearance): a box shorter than the text's
// point size plus leading cannot carry the label honestly — skip it rather
// than draw outside or clip.
constexpr double kOverlayMinBoxHeight = 9.0;

// Burn-in paint ONLY: runs on the saved CANDIDATE after the engine's content
// surgery is complete, so excision semantics are untouched. Each redaction
// rect (still in viewer top-down coordinates) is flipped the same way
// PoDoFoBackend::applyRedactions flips it, and the overlay text is drawn
// centered in white. Boxes too small to fit the text are skipped.
bool drawOverlayTextOnCandidate(const QString& candidatePath,
                                const QMap<int, QList<QRectF>>& redactionsByPage,
                                const QString& rawOverlayText,
                                QString* err)
{
    QString overlayText = rawOverlayText;
    overlayText.replace(QLatin1Char('\r'), QLatin1Char(' '));
    overlayText.replace(QLatin1Char('\n'), QLatin1Char(' '));
    overlayText = overlayText.simplified();
    if (overlayText.isEmpty()) return true; // empty = current behavior

    QString overlayOut;
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(candidatePath.toUtf8().constData());
        auto& pages = doc.GetPages();

        const QByteArray utf8 = overlayText.toUtf8();
        for (auto it = redactionsByPage.constBegin();
             it != redactionsByPage.constEnd(); ++it) {
            if (it.key() < 0 || it.key() >= static_cast<int>(pages.GetCount()))
                continue; // preflight already rejected out-of-range pages
            PoDoFo::PdfPage& page = pages.GetPageAt(it.key());
            const double pageHeight = page.GetMediaBox().Height;

            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, kOverlayFontSize);
            // Text paints with the NON-stroking color (SignatureManager
            // appearance precedent) — white on the black box.
            painter.GraphicsState.SetNonStrokingColor(PoDoFo::PdfColor(1.0, 1.0, 1.0));

            PoDoFo::PdfTextState measure;
            measure.Font = &font;
            measure.FontSize = kOverlayFontSize;
            const double textWidth = font.GetStringLength(utf8.constData(), measure);

            for (const QRectF& r : it.value()) {
                if (r.height() < kOverlayMinBoxHeight) continue; // too small
                if (r.width() < textWidth) continue;             // no horizontal fit
                const double pdfY = pageHeight - r.y() - r.height();
                const double x = r.x() + (r.width() - textWidth) / 2.0;
                const double baselineY = pdfY + r.height() / 2.0 + kOverlayFontSize * 0.35;
                (painter.DrawText)(utf8.constData(), x, baselineY);
            }
            painter.FinishDrawing();
        }
        // Save to a SIBLING file, never over the candidate in place: the
        // loaded PdfMemDocument keeps its input device open for lazy object
        // reads, and overwriting that same file mid-save corrupts the reads
        // (PoDoFo throws "Object and generation number cannot be read").
        overlayOut = candidatePath + QStringLiteral(".ovl");
        doc.Save(overlayOut.toUtf8().constData());
    } catch (const std::exception& e) {
        if (!overlayOut.isEmpty()) QFile::remove(overlayOut);
        if (err) *err = QString::fromLatin1(e.what());
        return false;
    }
    // The document (and its input device) is closed above — now swap the
    // overlaid file in as the candidate through the checked commit boundary
    // (D05: this operation contains no delete-before-rename replacement — the
    // candidate is only ever replaced by an atomic QSaveFile rename, and a
    // failed swap leaves the pre-swap candidate intact for the guard).
    QString swapErr;
    if (!SafeSave::commitFileToDestination(overlayOut, candidatePath, &swapErr)) {
        if (err) *err = QStringLiteral("could not replace the candidate with the overlaid "
                                       "copy: %1").arg(swapErr);
        QFile::remove(overlayOut);
        return false;
    }
    QFile::remove(overlayOut);
    return true;
}
} // namespace

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
    : QObject(parent)
{
    auto state = std::make_shared<ExecutionState>();
    state->request = std::move(request);
    state->owner = this;
    m_exec = std::move(state);
}

RedactOperation::~RedactOperation()
{
    // D02: no worker coordination is attempted — none is needed. The
    // in-flight run keeps its own shared_ptr to the execution state and never
    // dereferences this object, so destruction neither crashes nor cancels
    // it; the run completes on the durable state. The only duty here is to
    // detach the signal owner under the delivery lock, so the worker's
    // remaining stage/finished emissions are skipped instead of touching a
    // destroyed QObject. This runs BEFORE the base ~QObject, so the owner is
    // necessarily cleared while `this` is still a valid QObject.
    if (m_exec) m_exec->detachOwner();
}

void RedactOperation::setEngineFactory(EngineFactory factory)
{
    std::lock_guard<std::mutex> lock(m_exec->configMutex);
    m_exec->engineFactory = std::move(factory);
}

void RedactOperation::setPageBoundaryHook(std::function<void(int pagesDone)> hook)
{
    std::lock_guard<std::mutex> lock(m_exec->configMutex);
    m_exec->pageBoundaryHook = std::move(hook);
}

void RedactOperation::cancel()
{
    m_exec->cancelRequested.store(true);
}

// Partial-result recovery: load the COMMITTED redacted file in a disposable
// engine and sanitize it. A failure here cannot corrupt the redacted artifact
// (the engine re-saves to the sanitized destination only).
//
// D05: the sanitized copy is a SECOND output of the same transaction, so it
// gets the same safe-replacement boundary as the redacted one. The backend's
// sanitizeDocument() writes (and on its qpdf-fallback path delete-before-
// renames) whatever path it is handed — handing it the destination directly
// would damage a pre-existing sanitized output on a mid-write or rename
// failure. Instead the pass writes an operation-owned CANDIDATE, the candidate
// is validated (loadable PDF, unchanged page count), and only then is it
// committed through SafeSave::commitFileToDestination (bounded copy into
// QSaveFile + checked commit(): a failed commit leaves the destination
// byte-identical). There is deliberately NO direct-write fallback.
bool RedactOperation::sanitizeCommittedFile(const QString& committedRedactedPath,
                                            const QString& sanitizedDestination,
                                            QString* err)
{
    if (sanitizedDestination.isEmpty()) {
        if (err) *err = QStringLiteral("no destination path was provided for the sanitized copy");
        return false;
    }
    if (s_faultForTesting.load() == Fault::Sanitize) {
        // Deterministic seam honored on the recovery path too (the presenter's
        // Retry-sanitize): a repeated failure stays reproducible in tests.
        if (err) *err = QStringLiteral("injected sanitize failure (test seam)");
        return false;
    }

    // Sanitize into an operation-owned candidate — never the destination.
    QString candidate;
    if (!SafeSave::makeUniqueCandidate(&candidate, err)) return false;
    CandidateFileGuard candidateGuard(candidate);

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
    if (!engine->sanitizeDocument(candidate)) {
        if (err) *err = QStringLiteral("sanitization failed: %1").arg(engine->lastError().userMessage);
        return false;
    }

    // Validate the candidate before it may replace anything: a loadable PDF
    // with the redacted file's page count (FormManager/U05 validation shape).
    try {
        PoDoFo::PdfMemDocument sourceDoc;
        sourceDoc.Load(committedRedactedPath.toUtf8().constData());
        PoDoFo::PdfMemDocument candidateDoc;
        candidateDoc.Load(candidate.toUtf8().constData());
        const int sourcePages = static_cast<int>(sourceDoc.GetPages().GetCount());
        const int candidatePages = static_cast<int>(candidateDoc.GetPages().GetCount());
        if (candidatePages != sourcePages) {
            if (err) *err = QStringLiteral("the sanitized output failed validation: page count "
                                           "changed (%1 -> %2)").arg(sourcePages).arg(candidatePages);
            return false;
        }
    } catch (const PoDoFo::PdfError& e) {
        if (err) *err = QStringLiteral("the sanitized output is not a valid PDF: %1")
                            .arg(QString::fromLatin1(e.what()));
        return false;
    }

    // Commit: bounded copy into QSaveFile + checked commit(). The destination
    // (even a pre-existing one) is only ever replaced by this atomic rename.
    return SafeSave::commitFileToDestination(candidate, sanitizedDestination, err);
}

void RedactOperation::start()
{
    // D02: start cannot create overlapping runs of one mutable operation — a
    // second start() (or a start() after a run()) is refused.
    if (!m_exec->tryBeginRun()) return;
    // The worker captures ONLY the durable execution state. There is
    // deliberately NO weak-pointer gate here: the execution must not depend
    // on the UI-side object's lifetime, and a weak-pointer check cannot make
    // a whole member-function call safe anyway. Results reach live receivers
    // through emitFinished/emitStage's guarded owner (queued connections from
    // this worker thread); if the owner dies mid-run, the remaining
    // emissions are simply skipped.
    auto state = m_exec;
    QThread* worker = QThread::create([state]() {
        state->execute();
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void RedactOperation::run()
{
    // Synchronous seam (tests / scripted hosts): the same durable state
    // machine, executed on the calling thread. start() is the asynchronous
    // entry point; both share this one ExecutionState and its atomic cancel.
    m_exec->execute();
}

// The whole transaction. Everything read here belongs to the ExecutionState —
// NOT to the RedactOperation — so the run is valid for as long as the worker
// holds its shared_ptr, regardless of the UI-side object's lifetime.
void RedactOperation::ExecutionState::execute()
{
    const Fault fault = s_faultForTesting.load();
    RedactResult result;
    result.pagesTotal = request.redactionsByPage.size();

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
    EngineFactory factorySnapshot;
    {
        std::lock_guard<std::mutex> lock(configMutex);
        factorySnapshot = engineFactory;
    }
    std::shared_ptr<IPdfEditorEngine> engine = factorySnapshot ? factorySnapshot() : nullptr;

    emitStage(RedactStage::Preflight, 0, result.pagesTotal);

    // ── Preflight: ER-2 re-check + page ranges (no writes have happened) ────
    int sourcePageCount = 0;
    if (!engine) {
        fail(RedactStage::Preflight, QStringLiteral("Could not create an editing engine."));
    } else {
        if (request.redactionsByPage.isEmpty()) {
            fail(RedactStage::Preflight, QStringLiteral("No redaction marks were supplied."));
        } else if (!engine->loadDocumentForEditing(request.sourcePath)) {
            fail(RedactStage::Preflight,
                 QStringLiteral("Could not open the document: %1").arg(engine->lastError().userMessage));
        } else if (engine->hasPdfSignatures()) {
            fail(RedactStage::Preflight, QString::fromLatin1(kSignedRefusal));
        } else {
            // Source page count via PoDoFo (independent of any live session),
            // and every requested page must exist.
            try {
                PoDoFo::PdfMemDocument doc;
                doc.Load(request.sourcePath.toUtf8().constData());
                sourcePageCount = static_cast<int>(doc.GetPages().GetCount());
            } catch (const PoDoFo::PdfError& e) {
                fail(RedactStage::Preflight,
                     QStringLiteral("Could not read the source document: %1")
                         .arg(QString::fromLatin1(e.what())));
            }
            if (ok) {
                for (auto it = request.redactionsByPage.constBegin();
                     it != request.redactionsByPage.constEnd(); ++it) {
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
        emitStage(RedactStage::Redacting, 0, result.pagesTotal);
        int pagesDone = 0;
        for (auto it = request.redactionsByPage.constBegin();
             it != request.redactionsByPage.constEnd(); ++it) {
            if (checkCancel(&result)) { ok = false; break; }
            // The hook is read under the config mutex at each boundary, so it
            // may be (re)set from the UI thread and remains callable even if
            // the UI-side operation is destroyed mid-run.
            std::function<void(int)> hook;
            {
                std::lock_guard<std::mutex> lock(configMutex);
                hook = pageBoundaryHook;
            }
            if (hook) hook(pagesDone);
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
            emitStage(RedactStage::Redacting, pagesDone, result.pagesTotal);
        }
    }

    // ── SavingCandidate: serialize to a unique temp file, never the dest ────
    if (ok) {
        if (checkCancel(&result)) {
            ok = false;
        } else {
            emitStage(RedactStage::SavingCandidate, result.pagesProcessed, result.pagesTotal);
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
                } else if (!request.overlayText.isEmpty()) {
                    // §9.8 P1: part of producing the candidate — burn-in paint
                    // on the already-excised boxes, before validation/commit.
                    // Deliberately NOT a new pipeline stage: the stage order
                    // contract (and its progress reporting) is unchanged.
                    QString overlayErr;
                    if (!drawOverlayTextOnCandidate(candidate, request.redactionsByPage,
                                                    request.overlayText, &overlayErr)) {
                        fail(RedactStage::SavingCandidate,
                             QStringLiteral("Drawing the overlay text failed: %1. The original "
                                            "document was not modified.").arg(overlayErr));
                    }
                }
            }
        }
    }

    // ── Validating: reopen the candidate; page count must be unchanged ──────
    if (ok) {
        if (checkCancel(&result)) {
            ok = false;
        } else {
            emitStage(RedactStage::Validating, result.pagesProcessed, result.pagesTotal);
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
            emitStage(RedactStage::Committing, result.pagesProcessed, result.pagesTotal);
            QString commitErr;
            const SafeSave::CommitFaultForTesting commitFault =
                (fault == Fault::Commit) ? SafeSave::CommitFaultForTesting::FailBeforeCommit
                                         : SafeSave::CommitFaultForTesting::None;
            if (!SafeSave::commitFileToDestination(candidate, request.destinationPath,
                                                   &commitErr, commitFault)) {
                fail(RedactStage::Committing,
                     QStringLiteral("Committing the redacted file failed: %1. The destination "
                                    "was left unchanged.").arg(commitErr));
            } else {
                result.destination = request.destinationPath; // RedactionCommitted
            }
        }
    }

    // ── Sanitizing: load the COMMITTED redacted file, sanitize into the copy ─
    if (ok) {
        if (!request.sanitize) {
            result.outcome = RedactOutcome::Completed;
        } else {
            emitStage(RedactStage::Sanitizing, result.pagesProcessed, result.pagesTotal);
            // D01: preserve the intended destination on EVERY sanitize outcome.
            // The committed field below stays empty when sanitization fails,
            // but Retry must still know where the copy was supposed to go.
            result.intendedSanitizedDestination = request.sanitizedDestinationPath;
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
                                                  request.sanitizedDestinationPath, &sanitizeErr);
            }
            if (sanitized) {
                result.outcome = RedactOutcome::Completed;
                result.sanitizedDestination = request.sanitizedDestinationPath;
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
    emitStage(RedactStage::Done, result.pagesProcessed, result.pagesTotal);
    emitFinished(result);
}

} // namespace gp
