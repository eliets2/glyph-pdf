// SPDX-License-Identifier: Apache-2.0
// U05 — Make redaction output and partial failure explicit.
//
// One transactional redaction operation (gp::RedactOperation) behind BOTH entry
// paths, built on the R01 safe-save primitives (gp::SafeSave, extracted from
// FormManager.cpp). This file pins the transaction contract:
//
//   * The source PDF's bytes (SHA-256) are identical after EVERY terminal state
//     that is not a successful commit — candidate-save failure, validation
//     failure, commit failure (via the fault seam, exercising QSaveFile's
//     cancel path), cancel between pages, engine failure after one page, and
//     sanitize failure.
//   * Success is proven against the COMMITTED artifact with an independent
//     extractor (PdfiumBackend): the secret is gone from the output while
//     non-marked text survives (guards against a vacuous empty-extraction pass).
//   * A pre-existing destination is replaced atomically on success and left
//     byte-identical on a failed commit.
//   * The ER-2 signed-file refusal fires at Preflight, before any write.
//   * Sanitize failure yields PartialRedactedOnly (labeled partial state, never
//     a generic success banner), with a working Retry-sanitize seam.
//   * The pre-mutation dialog carries the summary ("N marks on M pages"),
//     destination defaults, and refuses a destination equal to the source.
//   * The shared result-presenter text is explicit for every outcome — the
//     partial-failure wording never claims success.
#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>
#include <QApplication>
#include <QAbstractButton>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QLabel>
#include <QThread>
#include <atomic>
#include <cctype>
#include <cmath>
#include <podofo/podofo.h>

#include "engines/PdfEditorEngine.h"
#include "engines/RedactOperation.h"
#include "engines/SafeSave.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "engines/SignatureManager.h"
#include "modes/RedactApplyDialog.h"
#include "core/AnnotationTypes.h"

// Windows headers (pulled in transitively by the pdfium/OpenSSL headers) define
// `#define DrawText DrawTextW`, which would rewrite the PoDoFo painter calls below.
#ifdef DrawText
#undef DrawText
#endif

#ifdef SOURCE_DIR
static const QString kFixtureDir = QStringLiteral(SOURCE_DIR "/tests/fixtures/signing");
#else
static const QString kFixtureDir = QStringLiteral("tests/fixtures/signing");
#endif
static const QString kP12Path  = kFixtureDir + "/test_signer.p12";
static const QString kInputPdf = kFixtureDir + "/test_input.pdf";
static const QString kCaPath   = kFixtureDir + "/test_ca.pem";
static const QString kP12Pass  = QStringLiteral("test");

#define REQUIRE_FIXTURES() \
    do { \
        if (!QFileInfo::exists(kP12Path) || !QFileInfo::exists(kInputPdf) || !QFileInfo::exists(kCaPath)) { \
            QSKIP("Signing fixtures missing — skipping signed-redaction guard test. " \
                  "Run cmake -P tests/fixtures/signing/generate_fixtures.cmake to create them."); \
        } \
    } while(0)

using namespace gp;

namespace {

QByteArray sha256(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&f);
    return hash.result();
}

QString pageText(const QString& pdfPath, int page) {
    PdfiumBackend backend;
    if (!backend.loadDocument(pdfPath)) return QString();
    return backend.extractText(page);
}

QString errText(const RedactResult& r) {
    return QStringLiteral("outcome=%1 stage=%2 error=%3")
        .arg(int(r.outcome)).arg(r.failedStage, r.error);
}

// ── N08: overlay-fit oracle and parsers ──────────────────────────────────────

// The production overlay size (RedactOperation.cpp kOverlayFontSize, 7pt).
constexpr double kTestOverlayFontSize = 7.0;

// The ACTUAL glyph extents of the overlay font at the overlay size, read
// straight from PoDoFo's PdfFont API. This is the test's independent oracle:
// the production derivation must meet these numbers, not its own guesses.
struct OverlayExtents { double ascent; double descent; double extent; };
OverlayExtents overlayFontExtents(double fontSize) {
    PoDoFo::PdfMemDocument doc;
    doc.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    auto& font = doc.GetFonts().GetStandard14Font(
        PoDoFo::PdfStandard14FontType::Helvetica);
    PoDoFo::PdfTextState state;
    state.Font = &font;
    state.FontSize = fontSize;
    const double ascent = font.GetAscent(state);        // positive, PDF units
    const double descent = -font.GetDescent(state);     // API returns negative
    return { ascent, descent, ascent + descent };
}

// Scans backwards from `pos - 1` over whitespace and parses one decimal
// number. Returns the index of the char BEFORE the number (or -1).
int parsePrevNumber(const QByteArray& s, int pos, double* value) {
    int i = pos - 1;
    while (i >= 0 && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t'))
        --i;
    const int end = i;
    while (i >= 0 && (isdigit(static_cast<unsigned char>(s[i]))
                      || s[i] == '.' || s[i] == '-'))
        --i;
    if (end <= i) return -1;
    bool ok = false;
    const double v = QByteArray(s.mid(i + 1, end - i)).toDouble(&ok);
    if (!ok) return -1;
    if (value) *value = v;
    return i;
}

// The overlay label is the only text painted with a non-stroking white color;
// the burn-in painter emits:  q  1 1 1 rg  q  BT  /Fx <size> Tf  <x> <y> Td
// <glyphs> Tj  ET. Extracts the drawn size and the Td baseline coordinates.
struct OverlayTextOp { double size = 0.0; double x = 0.0; double y = 0.0; };
bool findWhiteOverlayOp(const QByteArray& s, OverlayTextOp* out) {
    const int rg = s.indexOf("1 1 1 rg");
    if (rg < 0) return false;
    const int tf = s.indexOf(" Tf", rg);
    if (tf < 0) return false;
    // The size operand sits immediately before "Tf" (the font name precedes
    // it: "/Ft1 7 Tf").
    if (parsePrevNumber(s, tf, &out->size) < 0) return false;
    const int td = s.indexOf(" Td", tf);
    if (td < 0) return false;
    const int beforeY = parsePrevNumber(s, td, &out->y);
    if (beforeY < 0) return false;
    return parsePrevNumber(s, beforeY + 1, &out->x) >= 0;
}

// Vertical bounding box (rows) of "white-ish" glyph pixels inside `rect`,
// plus how much of the rect is box-black. Glyph pixels outside the black box
// are white-on-white and invisible — which is exactly why the N08 overflow
// must also be pinned by the exact Td/Tf math above.
struct GlyphRows {
    bool anyWhite = false;
    int whiteTop = 0;
    int whiteBottom = 0;
    int blackPixels = 0;
    long totalPixels = 0;
};
GlyphRows scanBoxRows(const QImage& img, const QRect& rect) {
    GlyphRows g;
    for (int y = rect.top(); y <= rect.bottom(); ++y) {
        for (int x = rect.left(); x <= rect.right(); ++x) {
            const QRgb c = img.pixel(x, y);
            ++g.totalPixels;
            const bool white = qRed(c) >= 200 && qGreen(c) >= 200 && qBlue(c) >= 200;
            const bool black = qRed(c) <= 60 && qGreen(c) <= 60 && qBlue(c) <= 60;
            if (white) {
                if (!g.anyWhite) { g.anyWhite = true; g.whiteTop = g.whiteBottom = y; }
                if (y < g.whiteTop) g.whiteTop = y;
                if (y > g.whiteBottom) g.whiteBottom = y;
            }
            if (black) ++g.blackPixels;
        }
    }
    return g;
}

// ── Modal driver: auto-answers RedactResultPresenter::present() ─────────────
// present() runs nested QMessageBox::exec() loops. A repeating 1ms timer fires
// inside those loops, clicks the requested button by text, and records every
// box text it saw. A stall fallback default-clicks any unexpected box so a
// contract change can never hang the run — the test fails on its assertions
// instead. stop() after each present() keeps consecutive drivers independent.
class ModalDriver {
public:
    ModalDriver(QObject* owner, QStringList buttons) : m_remaining(std::move(buttons)) {
        m_timer.setParent(owner);
        m_timer.setInterval(1);
        QObject::connect(&m_timer, &QTimer::timeout, owner, [this]() {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!box) return;
            if (m_remaining.isEmpty() || ++m_stall > 3000) {
                m_boxTexts << box->text() + QStringLiteral(" [default-clicked]");
                m_stall = 0;
                if (auto* d = box->defaultButton()) { d->click(); return; }
                if (!box->buttons().isEmpty()) box->buttons().first()->click();
                return;
            }
            m_boxTexts << box->text();
            for (QAbstractButton* b : box->buttons()) {
                const QString t = b->text();
                const int idx = m_remaining.indexOf(t);
                const int idxMnemonic =
                    t.startsWith(QLatin1Char('&')) ? m_remaining.indexOf(t.mid(1)) : -1;
                if (idx >= 0 || idxMnemonic >= 0) {
                    m_remaining.removeAt(idx >= 0 ? idx : idxMnemonic);
                    m_stall = 0;
                    b->click();
                    return;
                }
            }
        });
        m_timer.start();
    }
    void stop() { m_timer.stop(); }
    QStringList boxTexts() const { return m_boxTexts; }

private:
    QStringList m_remaining;
    QStringList m_boxTexts;
    int m_stall = 0;
    QTimer m_timer;
};

} // namespace

class TestRedactTransaction : public QObject {
    Q_OBJECT
private slots:
    void init() {
        // Isolate the static fault seam between tests (mirrors FormManager's seam).
        RedactOperation::setFaultForTesting(RedactOperation::Fault::None);
    }
    void initTestCase() {
        qRegisterMetaType<gp::RedactResult>("gp::RedactResult");
        qRegisterMetaType<gp::RedactStage>("gp::RedactStage");
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
    }

    // ── Success ────────────────────────────────────────────────────────────
    void successCommitsRedactedOutputSourceByteIdentical();

    // ── Failure / cancel keeps the source byte-identical ───────────────────
    void candidateSaveFailureLeavesSourceAndDestinationIntact();
    void validationFailureLeavesSourceAndDestinationIntact();
    void commitFailureLeavesPreExistingDestinationByteIdentical();
    void engineFailureAfterOnePageFailsAtRedacting();
    void cancelBetweenPagesWritesNothing();
    void sanitizeFailureYieldsPartialRedactedOnly();

    // ── ER-2 signed-file refusal at Preflight ──────────────────────────────
    void signedDocumentIsRefusedInPreflight();

    // ── Destination semantics ──────────────────────────────────────────────
    void existingDestinationIsReplacedOnSuccess();

    // ── D05: the SANITIZED copy gets the same safe-replacement boundary ─────
    // sanitizeCommittedFile (the transaction's Sanitizing stage AND the
    // presenter's Retry-sanitize recovery) must sanitize into an
    // operation-owned candidate and commit it through the checked QSaveFile
    // boundary — never write/delete the destination directly.
    void sanitizeStageFaultLeavesPreExistingSanitizedDestinationIntact();
    void sanitizeCommitFaultLeavesExistingSanitizedDestinationByteIdentical();
    void sanitizeReplacesExistingSanitizedDestinationOnSuccess();

    // ── D01-family retry recovery, driven through the SHARED presenter ──────
    // The presenter's Retry-sanitize is the real user recovery path; these
    // tests drive the actual modal flow (button clicks) at that boundary.
    void retryThroughPresenterSucceedsFromCommittedRedactedFile();
    void retryThroughPresenterFailsAgainKeepsRedactedArtifactAndMarks();
    // D01: the partial result must preserve the INTENDED sanitize destination
    // (separately from the committed one) so Retry targets the requested path.
    void partialResultCarriesIntendedSanitizeDestination();

    // ── V01/V02 bounded pins: persistence + marks/history of the redaction flow
    // A cancel requested during Sanitizing must NOT discard the already-
    // committed artifacts (they are reported honestly instead), and the marks
    // decision contract must keep marks recoverable on every failure/cancel.
    void cancelDuringSanitizingStageKeepsCommittedArtifacts();
    void presenterMarkDecisionsPinRecoveryContract();

    // ── SafeSave primitives (R01 extraction) ───────────────────────────────
    void safeSaveCandidatePathsAreUnique();
    void safeSaveCommitReplacesDestinationAndFaultLeavesItIntact();

    // ── Pre-mutation dialog + shared result presenter ──────────────────────
    void dialogPresentsSummaryDefaultsAndRefusesSourceDestination();
    void presenterTextIsExplicitForEveryOutcome();

    // ── §9.8 P1: optional overlay text on the burn-in boxes ────────────────
    // Legal/FOIA users expect to see WHY something was redacted printed on the
    // box. Overlay is burn-in paint only — excision semantics stay untouched;
    // an empty overlay must preserve the current behavior exactly.
    void overlayTextIsPrintedOnBurnedInBoxes();
    void emptyOverlayTextPreservesCurrentBehavior();
    void overlaySkippedWhenBoxTooSmall();

    // ── N08 residual: the HORIZONTAL half of the fit contract ─────────────
    // The review acceptance names "short/narrow boxes": a box narrower than
    // the label's actual advance width must skip the label (never squeeze or
    // overflow sideways), and a drawn label must be horizontally centered on
    // the box by the same measured width. Vertical fit is pinned above.
    void overlaySkippedWhenBoxTooNarrow();
    void overlayCenteredHorizontallyInWideBox();

    // ── N08: the label glyphs must stay INSIDE the burn-in box ────────────
    // Pre-fix the baseline was a guess (0.35 * fontSize above the box middle),
    // which at the minimum-height box put the 7pt ascender tops ~3pt ABOVE the
    // box top edge. These pin the metric contract: the minimum box height is
    // derived from the ACTUAL font extents, and the drawn baseline keeps
    // ascenders and descenders inside the box.
    void overlayGlyphsStayInsideMinimumHeightBox();

    // ── D02: the worker owns the state it uses; the UI-side object is ──────
    //    expendable. start() must deliver outcomes to a LIVE owner through
    //    the queued finished connection, refuse overlapping runs of one
    //    operation, and survive owner destruction (before dispatch and
    //    mid-run) without crashing the worker and without any callback past
    //    the owner's death — the durable execution state carries the run.
    void startDeliversFinishedViaQueuedConnectionToLiveOwner();
    void asyncCancelIsHonoredAndDelivered();
    void repeatedStartRunsOnceAndOwnerDestroyedBeforeDispatchIsSafe();
    void ownerDestroyedMidRunWorkerCompletesOnDurableStateWithoutCallbacks();

    // ── NCR-02: the SYNCHRONOUS entry point honors the same one-shot gate ──
    // run() must execute the transaction exactly once and refuse every second
    // entry (run();run(), run();start(), start();run()), and it must keep a
    // strong local reference to the execution state while execute() is active
    // (a direct-connected finished() slot — or a page-boundary hook — may
    // destroy the operation mid-call).
    void syncRunTwiceExecutesOnce();
    void runThenStartExecutesOnce();
    void startThenRunExecutesOnce();
    void syncOwnerDeletedFromDirectFinishedSlotRunStillCompletes();
    void syncOwnerDeletedAtPageBoundaryRunCompletesWithoutCallbacks();

private:
    QTemporaryDir m_tmpDir;

    QString createPdf(const QString& name, int pageCount,
                      const QString& secretPrefix = QStringLiteral("TOPSECRET_DATA"),
                      bool risky = false);
    RedactRequest makeRequest(const QString& src, const QString& dest,
                              const QList<int>& pages, bool sanitize,
                              const QString& sanitizedDest = QString());
    RedactResult runOp(RedactOperation* op);
};

// Draws `pageCount` pages; each page i carries "<secretPrefix>_PAGE<i>" at
// (50,700) and "PUBLIC_KEEP_TEXT" at (50,650). When `risky`, adds OpenAction
// JS + catalog XMP so the sanitize pass has hidden data to strip (the
// TestRedactSanitizeBundle fixture idiom).
QString TestRedactTransaction::createPdf(const QString& name, int pageCount,
                                         const QString& secretPrefix, bool risky) {
    const QString path = m_tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        for (int p = 0; p < pageCount; ++p) {
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            const QByteArray secret = QStringLiteral("%1_PAGE%2").arg(secretPrefix).arg(p).toUtf8();
            // Parenthesized: the Win32 macro `#define DrawText DrawTextW`
            // (transitively included via Qt/Windows headers) must not rewrite
            // the PoDoFo painter call.
            (painter.DrawText)(secret.constData(), 50, 700);
            (painter.DrawText)("PUBLIC_KEEP_TEXT", 50, 650);
            painter.FinishDrawing();
        }
        if (risky) {
            auto& cat = doc.GetCatalog().GetDictionary();
            PoDoFo::PdfDictionary oa;
            oa.AddKey("S", PoDoFo::PdfName("JavaScript"));
            oa.AddKey("JS", PoDoFo::PdfString("app.alert(1);"));
            cat.AddKey("OpenAction", PoDoFo::PdfObject(oa));
            cat.AddKey("Metadata", PoDoFo::PdfObject(PoDoFo::PdfString("<x:xmpmeta/>")));
        }
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning() << "fixture creation failed:" << e.what();
        return QString(); // callers QVERIFY2 on non-empty
    }
    return path;
}

RedactRequest TestRedactTransaction::makeRequest(const QString& src, const QString& dest,
                                                 const QList<int>& pages, bool sanitize,
                                                 const QString& sanitizedDest) {
    RedactRequest req;
    req.sourcePath = src;
    req.destinationPath = dest;
    for (int p : pages)
        // Marks use the viewer/top-down coordinate convention the engine applies
        // (PoDoFoBackend converts with pageHeight - y - height). Text drawn at
        // PDF (50,700) sits at ~142 from the top, so this band covers it.
        req.redactionsByPage[p].append(QRectF(40, 130, 300, 30));
    req.sanitize = sanitize;
    req.sanitizedDestinationPath = sanitizedDest;
    return req;
}

RedactResult TestRedactTransaction::runOp(RedactOperation* op) {
    RedactResult captured;
    connect(op, &RedactOperation::finished, op,
            [&captured](const RedactResult& r) { captured = r; });
    op->run(); // synchronous execution on the test thread (worker entry point)
    return captured;
}

// ── Success: the committed artifact is redacted, the source is untouched ────
void TestRedactTransaction::successCommitsRedactedOutputSourceByteIdentical() {
    const QString src = createPdf("success.pdf", 2);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    QVERIFY(!srcSha.isEmpty());
    const QString dest = m_tmpDir.filePath("success_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("success_redacted_sanitized.pdf");

    RedactOperation op(makeRequest(src, dest, {0, 1}, /*sanitize=*/true, sanitized));
    QSignalSpy stages(&op, &RedactOperation::stageChanged);

    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));
    QCOMPARE(r.destination, dest);
    QCOMPARE(r.sanitizedDestination, sanitized);
    QCOMPARE(r.pagesTotal, 2);
    QCOMPARE(r.pagesProcessed, 2);
    QVERIFY(r.error.isEmpty());

    // Stage machine walked every stage in order, ending at Done. Redacting
    // legitimately emits per-page progress (stageChanged is the progress
    // contract), so collapse consecutive same-stage emissions first.
    QList<int> seen;
    for (const auto& call : stages) {
        const int s = static_cast<int>(call.at(0).value<RedactStage>());
        if (seen.isEmpty() || seen.last() != s) seen << s;
    }
    const QList<int> expected{int(RedactStage::Preflight), int(RedactStage::Redacting),
                              int(RedactStage::SavingCandidate), int(RedactStage::Validating),
                              int(RedactStage::Committing), int(RedactStage::Sanitizing),
                              int(RedactStage::Done)};
    QStringList seenStr;
    for (int s : seen) seenStr << QString::number(s);
    QVERIFY2(seen == expected,
             qPrintable(QStringLiteral("stage order mismatch, seen=[%1] expected=[0,1,2,3,4,5,6]")
                            .arg(seenStr.join(QLatin1Char(',')))));

    // Independent extractor: the secret is GONE from both committed pages,
    // the non-marked text SURVIVES (proves extraction is not vacuously empty).
    for (int p = 0; p < 2; ++p) {
        const QString text = pageText(dest, p);
        QVERIFY2(!text.isEmpty(), "committed output must carry extractable text");
        QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")),
                 qPrintable(QStringLiteral("secret survived on page %1: %2").arg(p).arg(text)));
        QVERIFY2(text.contains(QLatin1String("PUBLIC_KEEP_TEXT")),
                 qPrintable(QStringLiteral("public text lost on page %1").arg(p)));
    }

    // The sanitized artifact exists and is loadable.
    QVERIFY(QFileInfo::exists(sanitized));
    PoDoFo::PdfMemDocument sanitizedDoc;
    sanitizedDoc.Load(sanitized.toUtf8().constData()); // throws on failure -> test aborts

    // The source is byte-identical.
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::candidateSaveFailureLeavesSourceAndDestinationIntact() {
    const QString src = createPdf("cand.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("cand_redacted.pdf");
    const QByteArray preExisting = "PRE-EXISTING-DESTINATION";
    {
        QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write(preExisting);
    }

    RedactOperation::setFaultForTesting(RedactOperation::Fault::CandidateSave);
    RedactOperation op(makeRequest(src, dest, {0}, false));
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::Failed);
    QCOMPARE(r.failedStage, QStringLiteral("SavingCandidate"));
    QVERIFY2(r.error.contains(QLatin1String("injected")), qPrintable(r.error));
    QVERIFY(!r.destination.isEmpty() == false);
    // Destination byte-identical, source byte-identical.
    QCOMPARE(sha256(dest), QCryptographicHash::hash(preExisting, QCryptographicHash::Sha256));
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::validationFailureLeavesSourceAndDestinationIntact() {
    const QString src = createPdf("val.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("val_redacted.pdf");

    RedactOperation::setFaultForTesting(RedactOperation::Fault::Validation);
    RedactOperation op(makeRequest(src, dest, {0}, false));
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::Failed);
    QCOMPARE(r.failedStage, QStringLiteral("Validating"));
    QVERIFY2(r.error.contains(QLatin1String("injected")), qPrintable(r.error));
    QVERIFY(!QFileInfo::exists(dest)); // no output was committed
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::commitFailureLeavesPreExistingDestinationByteIdentical() {
    const QString src = createPdf("commit.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("commit_redacted.pdf");
    const QByteArray preExisting = "PRE-EXISTING-DESTINATION-BYTES";
    {
        QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write(preExisting);
    }

    // The Commit fault fires inside SafeSave::commitFileToDestination AFTER the
    // bounded copy, exercising QSaveFile::cancelWriting — the exact path a real
    // commit failure (open handle, full disk) takes.
    RedactOperation::setFaultForTesting(RedactOperation::Fault::Commit);
    RedactOperation op(makeRequest(src, dest, {0}, false));
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::Failed);
    QCOMPARE(r.failedStage, QStringLiteral("Committing"));
    QVERIFY2(r.error.contains(QLatin1String("commit")), qPrintable(r.error));
    QCOMPARE(sha256(dest), QCryptographicHash::hash(preExisting, QCryptographicHash::Sha256));
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::engineFailureAfterOnePageFailsAtRedacting() {
    const QString src = createPdf("engfail.pdf", 3);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("engfail_redacted.pdf");

    // Fault::Redact fails the SECOND page's applyRedactions — the plan's
    // "engine failure after one page" acceptance case. The live session must
    // show nothing (the operation mutates only its disposable private engine),
    // the source must be byte-identical, and no destination may appear.
    RedactOperation::setFaultForTesting(RedactOperation::Fault::Redact);
    RedactOperation op(makeRequest(src, dest, {0, 1, 2}, false));
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::Failed);
    QCOMPARE(r.failedStage, QStringLiteral("Redacting"));
    QCOMPARE(r.pagesProcessed, 1); // exactly one page applied before the failure
    QCOMPARE(r.pagesTotal, 3);
    QVERIFY2(r.error.contains(QLatin1String("injected")), qPrintable(r.error));
    QVERIFY(!QFileInfo::exists(dest));
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::cancelBetweenPagesWritesNothing() {
    const QString src = createPdf("cancel.pdf", 3);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("cancel_redacted.pdf");

    RedactOperation op(makeRequest(src, dest, {0, 1, 2}, false));
    // Cancel at the first page boundary (after 1 page applied in memory).
    op.setPageBoundaryHook([&op](int pagesDone) {
        if (pagesDone == 1) op.cancel();
    });
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::Canceled);
    QCOMPARE(r.pagesProcessed, 1);
    QVERIFY(r.destination.isEmpty());
    QVERIFY(!QFileInfo::exists(dest)); // no output written
    QCOMPARE(sha256(src), srcSha);
}

void TestRedactTransaction::sanitizeFailureYieldsPartialRedactedOnly() {
    // The source carries hidden data (OpenAction JS + XMP) so the sanitize
    // stage has real work to fail.
    const QString src = createPdf("partial.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("partial_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("partial_redacted_sanitized.pdf");

    RedactOperation::setFaultForTesting(RedactOperation::Fault::Sanitize);
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult r = runOp(&op);

    // The labeled partial state — NOT Failed, NOT Completed.
    QCOMPARE(r.outcome, RedactOutcome::PartialRedactedOnly);
    QCOMPARE(r.failedStage, QStringLiteral("Sanitizing"));
    QVERIFY2(r.error.contains(QLatin1String("sanitiz"), Qt::CaseInsensitive), qPrintable(r.error));

    // The redacted artifact IS committed and valid; the sanitized copy is not.
    QVERIFY(QFileInfo::exists(r.destination));
    QCOMPARE(r.destination, dest);
    QVERIFY(!QFileInfo::exists(sanitized));
    const QString text = pageText(dest, 0);
    QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("PUBLIC_KEEP_TEXT")), qPrintable(text));

    // Source still byte-identical.
    QCOMPARE(sha256(src), srcSha);

    // Retry-sanitize seam: re-runs ONLY the Sanitizing stage over the COMMITTED
    // redacted file (D05: through candidate + validation + checked commit).
    // The injected fault was a one-shot condition of the first attempt — the
    // real Retry runs without it, so clear the seam first.
    RedactOperation::setFaultForTesting(RedactOperation::Fault::None);
    QString retryErr;
    QVERIFY2(RedactOperation::sanitizeCommittedFile(r.destination, sanitized, &retryErr),
             qPrintable(retryErr));
    PoDoFo::PdfMemDocument out;
    out.Load(sanitized.toUtf8().constData());
    auto& cat = out.GetCatalog().GetDictionary();
    QVERIFY(!cat.HasKey("OpenAction"));
    QVERIFY(!cat.HasKey("Metadata"));
}

// ── D05: safe replacement at the sanitize boundary ──────────────────────────
// The sanitize pass is a SECOND output of the same transaction: replacing an
// existing sanitized destination must keep the original bytes intact on every
// failure and atomically replace them on success. (The redacted artifact and
// the source document must survive every one of these failures untouched.)

// Candidate-stage failure (Fault::Sanitize fires before any engine write):
// the pre-existing sanitized destination is never touched.
void TestRedactTransaction::sanitizeStageFaultLeavesPreExistingSanitizedDestinationIntact() {
    const QString src = createPdf("sfadeault.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("sfault_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("sfault_redacted_sanitized.pdf");
    const QByteArray preExisting = "PRE-EXISTING-SANITIZED-DESTINATION";
    {
        QFile d(sanitized); QVERIFY(d.open(QIODevice::WriteOnly)); d.write(preExisting);
    }

    RedactOperation::setFaultForTesting(RedactOperation::Fault::Sanitize);
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult r = runOp(&op);

    QCOMPARE(r.outcome, RedactOutcome::PartialRedactedOnly);
    // Every artifact boundary holds: redacted committed, sanitized destination
    // byte-identical, source byte-identical.
    QVERIFY(QFileInfo::exists(r.destination));
    QCOMPARE(sha256(sanitized), QCryptographicHash::hash(preExisting, QCryptographicHash::Sha256));
    QCOMPARE(sha256(src), srcSha);
}

// Commit-stage failure (injected at the exact QSaveFile::cancelWriting seam):
// the pre-existing sanitized destination stays byte-identical (SHA-256).
void TestRedactTransaction::sanitizeCommitFaultLeavesExistingSanitizedDestinationByteIdentical() {
    const QString src = createPdf("sfcommit.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("sfcommit_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("sfcommit_redacted_sanitized.pdf");

    // First run completes and produces the real sanitized copy.
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    // Re-seed the destination with known bytes: it is now a PRE-EXISTING file
    // that a second sanitize pass must replace atomically or not at all.
    const QByteArray preExisting = "PRE-EXISTING-SANITIZED-DESTINATION-BYTES";
    {
        QFile d(sanitized); QVERIFY(d.open(QIODevice::WriteOnly)); d.resize(0); d.write(preExisting);
    }
    const QByteArray preSha = sha256(sanitized);
    QVERIFY(!preSha.isEmpty());

    // The commit fault fires inside SafeSave::commitFileToDestination AFTER
    // the bounded copy — the exact path a real commit failure takes.
    SafeSave::setCommitFaultForTesting(SafeSave::CommitFaultForTesting::FailBeforeCommit);
    QString err;
    const bool ok = RedactOperation::sanitizeCommittedFile(r.destination, sanitized, &err);
    SafeSave::setCommitFaultForTesting(SafeSave::CommitFaultForTesting::None);
    QVERIFY2(!ok, "injected sanitize-commit fault must fail the sanitize step");
    QVERIFY2(err.contains(QLatin1String("commit")), qPrintable(err));

    // The failed replacement left the existing destination byte-identical,
    // and the committed redacted artifact (the sanitize input) is intact.
    QCOMPARE(sha256(sanitized), preSha);
    QVERIFY(QFileInfo::exists(r.destination));
    QCOMPARE(sha256(src), srcSha);
}

// Success: an existing sanitized destination is replaced by the new sanitized
// content — a valid PDF with the hidden data stripped, not a corrupt or stale file.
void TestRedactTransaction::sanitizeReplacesExistingSanitizedDestinationOnSuccess() {
    const QString src = createPdf("sfreplace.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("sfreplace_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("sfreplace_redacted_sanitized.pdf");

    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    // Stale content in the destination, then a full re-sanitize over it.
    {
        QFile d(sanitized); QVERIFY(d.open(QIODevice::WriteOnly)); d.resize(0);
        d.write("STALE-SANITIZED-CONTENT");
    }
    QString err;
    QVERIFY2(RedactOperation::sanitizeCommittedFile(r.destination, sanitized, &err),
             qPrintable(err));

    PoDoFo::PdfMemDocument out;
    out.Load(sanitized.toUtf8().constData()); // throws on failure -> test aborts
    QVERIFY2(QFile(sanitized).size() > 0, "replaced sanitized output must not be empty");
    auto& cat = out.GetCatalog().GetDictionary();
    QVERIFY(!cat.HasKey("OpenAction"));  // hidden data stripped by the new pass
    QVERIFY(!cat.HasKey("Metadata"));
    QByteArray replacedBytes;
    {
        QFile d(sanitized);
        QVERIFY(d.open(QIODevice::ReadOnly));
        replacedBytes = d.readAll();
    }
    QVERIFY2(!replacedBytes.contains("STALE-SANITIZED-CONTENT"),
             "stale destination bytes must be gone after the atomic replacement");
}

// ── D01-family retry: the presenter's Retry-sanitize at the real boundary ───
// Drives the actual modal flow: a genuine PartialRedactedOnly result from a
// full operation run, the presenter's "Retry Sanitize" button, then the
// artifacts on disk. The retry must re-run ONLY the Sanitizing stage from the
// COMMITTED redacted file (D05 boundary) into the intended destination.
void TestRedactTransaction::retryThroughPresenterSucceedsFromCommittedRedactedFile() {
    const QString src = createPdf("retryok.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("retryok_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("retryok_redacted_sanitized.pdf");
    // Pre-existing stale content at the sanitized destination: the successful
    // retry must atomically REPLACE it (D05), never merge or corrupt.
    {
        QFile d(sanitized); QVERIFY(d.open(QIODevice::WriteOnly)); d.write("STALE-BYTES");
    }

    RedactOperation::setFaultForTesting(RedactOperation::Fault::Sanitize);
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult partial = runOp(&op);
    QCOMPARE(partial.outcome, RedactOutcome::PartialRedactedOnly);
    QByteArray staleBytes;
    {
        QFile d(sanitized);
        QVERIFY(d.open(QIODevice::ReadOnly));
        staleBytes = d.readAll();
    }
    QVERIFY2(!staleBytes.isEmpty(), "precondition: stale bytes present before the retry");
    RedactOperation::setFaultForTesting(RedactOperation::Fault::None);

    ModalDriver driver(this, {QStringLiteral("Retry Sanitize"), QStringLiteral("OK")});
    RedactResult recovered;
    const auto decision = RedactResultPresenter::present(nullptr, partial, &recovered);
    driver.stop();

    // A completed retry means the marks' effect is fully saved: ClearMarks.
    QCOMPARE(decision, RedactResultPresenter::MarkDecision::ClearMarks);

    // D01: the recovery must be reflected in the effective result — the flow
    // is Completed, and the banner built from it names BOTH artifacts and
    // never repeats the failed-sanitization wording.
    QCOMPARE(recovered.outcome, RedactOutcome::Completed);
    QCOMPARE(recovered.sanitizedDestination, sanitized);
    QCOMPARE(recovered.destination, partial.destination);
    const QString banner = RedactResultPresenter::bannerText(recovered);
    QVERIFY2(banner.contains(QFileInfo(dest).fileName()), qPrintable(banner));
    QVERIFY2(banner.contains(QFileInfo(sanitized).fileName()), qPrintable(banner));
    QVERIFY2(!banner.contains(QLatin1String("FAILED")),
             qPrintable(QStringLiteral("banner after a successful retry must not keep the "
                                       "failure wording: %1").arg(banner)));

    // D05 (review 2026-09-07): the Security entry path (SecurityController's
    // RedactOperation::finished handler) used to banner from the ORIGINAL
    // partial result, so a flow whose Retry-sanitize SUCCEEDED still announced
    // "sanitization FAILED". SecurityController::applyRedactions is not
    // headlessly drivable (private; needs a live MainWindow, a loaded viewer
    // and the modal RedactApplyDialog/progress/presenter dialogs — TestControllers
    // constructs SecurityController with a nullptr window and only exercises
    // its static builders), so the recovered-result plumbing is pinned HERE at
    // the shared presenter seam: the two candidate banners must really differ,
    // which is what makes the controller's result choice load-bearing — it must
    // banner from present()'s effective out-param (Completed), never from the
    // original partial result.
    const QString staleBanner = RedactResultPresenter::bannerText(partial);
    QVERIFY2(staleBanner.contains(QLatin1String("FAILED")),
             qPrintable(QStringLiteral("precondition: the ORIGINAL partial result still "
                                      "banners failure — the recovered flow must not: %1")
                            .arg(staleBanner)));
    QVERIFY2(staleBanner != banner,
             "recovered and original banners must differ — the controller's "
             "banner-result choice is load-bearing");

    // The sanitized copy EXISTS at the intended destination and is the NEW
    // sanitized content (pre-fix: the presenter passed an empty path, the
    // retry failed, and this file still held the stale bytes).
    QVERIFY2(QFileInfo::exists(sanitized),
             "retry through the presenter must produce the sanitized copy");
    QByteArray newBytes;
    {
        QFile d(sanitized);
        QVERIFY(d.open(QIODevice::ReadOnly));
        newBytes = d.readAll();
    }
    QVERIFY2(!newBytes.contains("STALE-BYTES") && newBytes != staleBytes,
             "the retry must replace the stale destination bytes with new content");
    PoDoFo::PdfMemDocument out;
    out.Load(sanitized.toUtf8().constData()); // throws on failure -> test aborts
    auto& cat = out.GetCatalog().GetDictionary();
    QVERIFY(!cat.HasKey("OpenAction"));   // hidden data stripped
    QVERIFY(!cat.HasKey("Metadata"));

    // The presenter announced the committed path ("Sanitization Complete").
    const QString boxes = driver.boxTexts().join(QLatin1Char('\n'));
    QVERIFY2(boxes.contains(sanitized), qPrintable(boxes));

    // Every other artifact boundary survives the recovery.
    QVERIFY(QFileInfo::exists(partial.destination));       // redacted artifact intact
    QCOMPARE(sha256(src), srcSha);                          // source untouched
}

// Retry that fails AGAIN must fail honestly and keep the state recoverable:
// the redacted artifact stays, the pre-existing sanitized destination is not
// damaged, and the marks decision RETAINS the marks (a clean retry remains
// possible) — the pre-fix flow cleared them.
void TestRedactTransaction::retryThroughPresenterFailsAgainKeepsRedactedArtifactAndMarks() {
    const QString src = createPdf("retryfail.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("retryfail_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("retryfail_redacted_sanitized.pdf");
    const QByteArray preExisting = "PRE-EXISTING-SANITIZED-DESTINATION";
    {
        QFile d(sanitized); QVERIFY(d.open(QIODevice::WriteOnly)); d.write(preExisting);
    }
    const QByteArray preSha = sha256(sanitized);

    // The fault stays armed: the retry fails AGAIN, deterministically.
    RedactOperation::setFaultForTesting(RedactOperation::Fault::Sanitize);
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult partial = runOp(&op);
    QCOMPARE(partial.outcome, RedactOutcome::PartialRedactedOnly);

    ModalDriver driver(this, {QStringLiteral("Retry Sanitize"), QStringLiteral("OK")});
    const auto decision = RedactResultPresenter::present(nullptr, partial);
    driver.stop();

    // Recoverable state: marks retained for a clean retry.
    QCOMPARE(decision, RedactResultPresenter::MarkDecision::RetainMarks);

    // The honest failure names the redacted file (the user's recoverable copy).
    const QString boxes = driver.boxTexts().join(QLatin1Char('\n'));
    QVERIFY2(boxes.contains(partial.destination), qPrintable(boxes));

    // No artifact was damaged by the failed recovery.
    QVERIFY(QFileInfo::exists(partial.destination));  // redacted artifact kept
    QCOMPARE(sha256(sanitized), preSha);               // pre-existing dest untouched
    QCOMPARE(sha256(src), srcSha);                    // source untouched
}

// ── V01/V02 bounded pins: persistence + marks/history of the redaction flow ─
// The redaction flow keeps marks as viewer annotations (no undo-stack
// commands), so its history contract is the marks decision: marks survive
// every cancel/failure and are cleared exactly once, when the output is
// committed AND kept. Pinned through the shared presenter's real modal flow.
void TestRedactTransaction::cancelDuringSanitizingStageKeepsCommittedArtifacts() {
    const QString src = createPdf("cancelsan.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QByteArray srcSha = sha256(src);
    const QString dest = m_tmpDir.filePath("cancelsan_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("cancelsan_redacted_sanitized.pdf");

    // Cancel requested exactly when the Sanitizing stage begins — AFTER the
    // redacted file is committed. The contract: once committed, cancellation
    // is no longer honored; the artifacts exist and are honestly reported
    // (Completed), never silently discarded.
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    QObject::connect(&op, &RedactOperation::stageChanged, &op,
                     [&op](RedactStage stage, int, int) {
                         if (stage == RedactStage::Sanitizing) op.cancel();
                     });
    const RedactResult r = runOp(&op);

    QVERIFY2(r.outcome == RedactOutcome::Completed,
             qPrintable(QStringLiteral("cancel-after-commit must not discard committed "
                                       "artifacts: %1").arg(errText(r))));
    QCOMPARE(r.destination, dest);
    QCOMPARE(r.sanitizedDestination, sanitized);
    QVERIFY(QFileInfo::exists(dest));
    QVERIFY(QFileInfo::exists(sanitized));
    QCOMPARE(sha256(src), srcSha);  // the original is still never written
}

void TestRedactTransaction::presenterMarkDecisionsPinRecoveryContract() {
    // Failed / Canceled: the marks are kept (RetainMarks) so the user can
    // retry; the wording never claims an output was written.
    {
        RedactResult failed;
        failed.outcome = RedactOutcome::Failed;
        failed.failedStage = QStringLiteral("Committing");
        failed.error = QStringLiteral("injected commit failure");
        failed.destination = m_tmpDir.filePath("dec_failed_redacted.pdf");
        ModalDriver driver(this, {QStringLiteral("OK")});
        const auto decision = RedactResultPresenter::present(nullptr, failed);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::RetainMarks);
        const QString boxes = driver.boxTexts().join(QLatin1Char('\n'));
        QVERIFY2(boxes.contains(QLatin1String("not modified"), Qt::CaseInsensitive),
                 qPrintable(boxes));
    }
    {
        RedactResult canceled;
        canceled.outcome = RedactOutcome::Canceled;
        ModalDriver driver(this, {QStringLiteral("OK")});
        const auto decision = RedactResultPresenter::present(nullptr, canceled);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::RetainMarks);
        const QString boxes = driver.boxTexts().join(QLatin1Char('\n'));
        QVERIFY2(boxes.contains(QLatin1String("No output")), qPrintable(boxes));
        QVERIFY2(boxes.contains(QLatin1String("preserved")), qPrintable(boxes));
    }
    // Completed: both artifacts committed — marks may be cleared.
    {
        RedactResult done;
        done.outcome = RedactOutcome::Completed;
        done.destination = m_tmpDir.filePath("dec_done_redacted.pdf");
        done.sanitizedDestination = m_tmpDir.filePath("dec_done_sanitized.pdf");
        ModalDriver driver(this, {QStringLiteral("OK")});
        const auto decision = RedactResultPresenter::present(nullptr, done);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::ClearMarks);
    }
    // Partial + "Keep Redacted File": artifact committed and kept — ClearMarks,
    // and the redacted file survives.
    {
        const QString dest = m_tmpDir.filePath("dec_keep_redacted.pdf");
        { QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write("REDACTED"); }
        RedactResult partial;
        partial.outcome = RedactOutcome::PartialRedactedOnly;
        partial.destination = dest;
        partial.failedStage = QStringLiteral("Sanitizing");
        partial.error = QStringLiteral("injected sanitize failure (test seam)");
        ModalDriver driver(this, {QStringLiteral("Keep Redacted File")});
        const auto decision = RedactResultPresenter::present(nullptr, partial);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::ClearMarks);
        QVERIFY(QFileInfo::exists(dest));
    }
    // Partial + "Discard Output" with the confirmation DECLINED: marks kept,
    // the redacted copy is NOT deleted.
    {
        const QString dest = m_tmpDir.filePath("dec_discno_redacted.pdf");
        { QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write("REDACTED"); }
        RedactResult partial;
        partial.outcome = RedactOutcome::PartialRedactedOnly;
        partial.destination = dest;
        partial.failedStage = QStringLiteral("Sanitizing");
        partial.error = QStringLiteral("injected sanitize failure (test seam)");
        ModalDriver driver(this, {QStringLiteral("Discard Output"), QStringLiteral("No")});
        const auto decision = RedactResultPresenter::present(nullptr, partial);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::RetainMarks);
        QVERIFY2(QFileInfo::exists(dest), "declined discard must not delete the redacted copy");
    }
    // Partial + "Discard Output" CONFIRMED: the redacted copy is deleted (only
    // after the explicit confirmation; the source is never touched), marks kept.
    {
        const QString dest = m_tmpDir.filePath("dec_discyes_redacted.pdf");
        { QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write("REDACTED"); }
        RedactResult partial;
        partial.outcome = RedactOutcome::PartialRedactedOnly;
        partial.destination = dest;
        partial.failedStage = QStringLiteral("Sanitizing");
        partial.error = QStringLiteral("injected sanitize failure (test seam)");
        ModalDriver driver(this, {QStringLiteral("Discard Output"), QStringLiteral("Yes")});
        const auto decision = RedactResultPresenter::present(nullptr, partial);
        driver.stop();
        QCOMPARE(decision, RedactResultPresenter::MarkDecision::RetainMarks);
        QVERIFY2(!QFileInfo::exists(dest), "confirmed discard deletes only the redacted copy");
    }
}

// D01: PartialRedactedOnly must carry the intended sanitize destination
// separately from the committed one — Retry needs the requested path (the
// pre-fix result lost it, so Retry-sanitize through the presenter always
// failed). A Completed result records the same intended path it committed.
void TestRedactTransaction::partialResultCarriesIntendedSanitizeDestination() {
    const QString src = createPdf("intended.pdf", 1, QStringLiteral("TOPSECRET_DATA"), /*risky=*/true);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("intended_redacted.pdf");
    const QString sanitized = m_tmpDir.filePath("intended_redacted_sanitized.pdf");

    RedactOperation::setFaultForTesting(RedactOperation::Fault::Sanitize);
    RedactOperation op(makeRequest(src, dest, {0}, /*sanitize=*/true, sanitized));
    const RedactResult partial = runOp(&op);

    QCOMPARE(partial.outcome, RedactOutcome::PartialRedactedOnly);
    QCOMPARE(partial.intendedSanitizedDestination, sanitized); // the retry target survives
    QVERIFY2(partial.sanitizedDestination.isEmpty(),
             "a partial result must not claim a committed sanitized copy");

    // The completed run records the same intended path it committed.
    RedactOperation::setFaultForTesting(RedactOperation::Fault::None);
    const QString dest2 = m_tmpDir.filePath("intended2_redacted.pdf");
    const QString sanitized2 = m_tmpDir.filePath("intended2_redacted_sanitized.pdf");
    RedactOperation op2(makeRequest(src, dest2, {0}, /*sanitize=*/true, sanitized2));
    const RedactResult done = runOp(&op2);
    QCOMPARE(done.outcome, RedactOutcome::Completed);
    QCOMPARE(done.intendedSanitizedDestination, sanitized2);
    QCOMPARE(done.sanitizedDestination, sanitized2);
}

void TestRedactTransaction::signedDocumentIsRefusedInPreflight() {
    REQUIRE_FIXTURES();

    QString signedPdf = m_tmpDir.filePath("signed_for_redact.pdf");
    SignatureManager mgr;
    QVERIFY2(mgr.signDocument(kInputPdf, signedPdf, kP12Path, kP12Pass,
                              "RedactTransactionTest", "") == SignOutcome::Success,
             "signDocument should succeed with valid P12");
    const QByteArray srcSha = sha256(signedPdf);

    const QString dest = m_tmpDir.filePath("signed_redacted.pdf");
    RedactOperation op(makeRequest(signedPdf, dest, {0}, false));
    const RedactResult r = runOp(&op);

    // ER-2: refuse at Preflight, before any write.
    QCOMPARE(r.outcome, RedactOutcome::Failed);
    QCOMPARE(r.failedStage, QStringLiteral("Preflight"));
    QVERIFY2(r.error.contains(QLatin1String("signed"), Qt::CaseInsensitive),
             qPrintable(r.error));
    QVERIFY(!QFileInfo::exists(dest));
    QCOMPARE(sha256(signedPdf), srcSha);
}

void TestRedactTransaction::existingDestinationIsReplacedOnSuccess() {
    const QString src = createPdf("replace.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("replace_redacted.pdf");
    {
        QFile d(dest); QVERIFY(d.open(QIODevice::WriteOnly)); d.write("STALE-OLD-CONTENT");
    }

    RedactOperation op(makeRequest(src, dest, {0}, false));
    const RedactResult r = runOp(&op);

    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));
    const QString text = pageText(dest, 0);
    QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")), qPrintable(text));
    QVERIFY2(!text.contains(QLatin1String("STALE-OLD-CONTENT")), "old bytes must be gone");
    QVERIFY2(text.contains(QLatin1String("PUBLIC_KEEP_TEXT")), qPrintable(text));
}

// ── SafeSave primitives (extracted R01) ─────────────────────────────────────
void TestRedactTransaction::safeSaveCandidatePathsAreUnique() {
    QString a, b, err;
    QVERIFY(SafeSave::makeUniqueCandidate(&a, &err));
    QVERIFY(SafeSave::makeUniqueCandidate(&b, &err));
    QVERIFY(!a.isEmpty() && !b.isEmpty());
    QVERIFY2(a != b, "candidate paths must be unique");
    QVERIFY2(a.endsWith(QLatin1String(".pdf")), qPrintable(a));
}

void TestRedactTransaction::safeSaveCommitReplacesDestinationAndFaultLeavesItIntact() {
    const QString candidate = m_tmpDir.filePath("ss_candidate.pdf");
    const QByteArray contentA = "CANDIDATE-BYTES-A";
    {
        QFile f(candidate); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(contentA);
    }
    const QString dest = m_tmpDir.filePath("ss_dest.pdf");

    QString err;
    QVERIFY2(SafeSave::commitFileToDestination(candidate, dest, &err), qPrintable(err));
    QCOMPARE(QFile(dest).size(), qint64(contentA.size()));

    // Replace an existing destination (overwrite semantics).
    const QByteArray contentB = "CANDIDATE-BYTES-B-VERY-DIFFERENT";
    {
        QFile f(candidate); QVERIFY(f.open(QIODevice::WriteOnly)); f.resize(0); f.write(contentB);
    }
    QVERIFY2(SafeSave::commitFileToDestination(candidate, dest, &err), qPrintable(err));
    QCOMPARE(QFile(dest).size(), qint64(contentB.size()));

    // Injected commit failure: the destination stays byte-identical.
    SafeSave::setCommitFaultForTesting(SafeSave::CommitFaultForTesting::FailBeforeCommit);
    const bool ok = SafeSave::commitFileToDestination(candidate, dest, &err);
    SafeSave::setCommitFaultForTesting(SafeSave::CommitFaultForTesting::None);
    QVERIFY2(!ok, "injected commit fault must fail the commit");
    QVERIFY2(!err.isEmpty(), "injected commit fault must report an error");
    QCOMPARE(QFile(dest).size(), qint64(contentB.size()));
}

// ── Pre-mutation dialog ─────────────────────────────────────────────────────
void TestRedactTransaction::dialogPresentsSummaryDefaultsAndRefusesSourceDestination() {
    RedactApplyPlan plan;
    plan.sourcePath = m_tmpDir.filePath("doc.pdf");
    plan.destinationPath = m_tmpDir.filePath("doc_redacted.pdf");
    plan.sanitizedDestinationPath = m_tmpDir.filePath("doc_redacted_sanitized.pdf");
    plan.markCount = 3;
    plan.marksPerPage = {{0, 2}, {2, 1}};
    plan.sourcePageCount = 4;
    plan.sanitize = false;

    RedactApplyDialog dlg(plan);
    dlg.show(); // offscreen platform

    // Summary: "N marks on M pages", the source page count, and the
    // "marked for removal" wording (distinct from "applied to saved output").
    const QString summary = dlg.summaryText();
    QVERIFY2(summary.contains(QLatin1String("3 marks on 2 pages")), qPrintable(summary));
    QVERIFY2(summary.contains(QLatin1String("4 pages")), qPrintable(summary));
    QVERIFY2(summary.contains(QLatin1String("marked for removal")), qPrintable(summary));

    // Defaults preserved: <base>_redacted.pdf / <base>_redacted_sanitized.pdf.
    QCOMPARE(dlg.plan().destinationPath, plan.destinationPath);
    QCOMPARE(dlg.plan().sanitizedDestinationPath, plan.sanitizedDestinationPath);
    QCOMPARE(dlg.plan().sanitize, false);
    QCOMPARE(dlg.plan().markCount, 3);

    // The sanitize choice and destination are user-adjustable.
    dlg.setSanitizeChecked(true);
    QCOMPARE(dlg.plan().sanitize, true);
    dlg.setDestinationPath(m_tmpDir.filePath("elsewhere.pdf"));
    QCOMPARE(dlg.plan().destinationPath, m_tmpDir.filePath("elsewhere.pdf"));

    // The plan's optional overlay text rides the plan struct (§9.8 P1) — empty
    // default, user-adjustable, carried back out of plan().
    QCOMPARE(dlg.plan().overlayText, QString());
    dlg.setOverlayText(QStringLiteral("FOIA-2026-114"));
    QCOMPARE(dlg.plan().overlayText, QStringLiteral("FOIA-2026-114"));

    // A destination equal to the SOURCE must be refused (the old controller
    // path saved over the original — U05 forbids that).
    dlg.setDestinationPath(plan.sourcePath);
    auto* ok = dlg.findChild<QPushButton*>(QStringLiteral("redactApplyOkButton"));
    auto* warn = dlg.findChild<QLabel*>(QStringLiteral("redactApplyWarningLabel"));
    QVERIFY(ok);
    QVERIFY2(!ok->isEnabled(), "OK must be disabled when the destination equals the source");
    QVERIFY2(warn && !warn->text().isEmpty(), "a reason must be shown");
}

// ── Shared result presenter text (never a generic banner) ───────────────────
void TestRedactTransaction::presenterTextIsExplicitForEveryOutcome() {
    const QString dest = QStringLiteral("/out/secret_redacted.pdf");
    const QString sanitized = QStringLiteral("/out/secret_redacted_sanitized.pdf");

    // Completed: names BOTH committed artifacts.
    RedactResult done;
    done.outcome = RedactOutcome::Completed;
    done.destination = dest;
    done.sanitizedDestination = sanitized;
    const QString doneText = RedactResultPresenter::bannerText(done);
    QVERIFY2(doneText.contains(QLatin1String("secret_redacted.pdf")), qPrintable(doneText));
    QVERIFY2(doneText.contains(QLatin1String("secret_redacted_sanitized.pdf")), qPrintable(doneText));
    QVERIFY2(!doneText.contains(QLatin1String("failed"), Qt::CaseInsensitive),
             qPrintable(doneText));

    // PartialRedactedOnly: labeled partial state — names the redacted file,
    // states the sanitization failure, and NEVER claims success.
    RedactResult partial;
    partial.outcome = RedactOutcome::PartialRedactedOnly;
    partial.destination = dest;
    partial.failedStage = QStringLiteral("Sanitizing");
    partial.error = QStringLiteral("injected sanitize failure (test seam)");
    const QString partialText = RedactResultPresenter::detailText(partial);
    QVERIFY2(partialText.contains(QLatin1String("secret_redacted.pdf")), qPrintable(partialText));
    QVERIFY2(partialText.contains(QLatin1String("sanitiz"), Qt::CaseInsensitive),
             qPrintable(partialText));
    QVERIFY2(!partialText.contains(QLatin1String("successfully")),
             qPrintable(QStringLiteral("partial text must never claim success: %1")
                            .arg(partialText)));

    // Failed: states the original was not modified.
    RedactResult failed;
    failed.outcome = RedactOutcome::Failed;
    failed.failedStage = QStringLiteral("SavingCandidate");
    failed.error = QStringLiteral("disk full");
    const QString failedText = RedactResultPresenter::detailText(failed);
    QVERIFY2(failedText.contains(QLatin1String("not modified"), Qt::CaseInsensitive),
             qPrintable(failedText));
    QVERIFY2(failedText.contains(QLatin1String("disk full")), qPrintable(failedText));

    // Canceled: no output written, marks preserved.
    RedactResult canceled;
    canceled.outcome = RedactOutcome::Canceled;
    const QString canceledText = RedactResultPresenter::detailText(canceled);
    QVERIFY2(canceledText.contains(QLatin1String("No output")), qPrintable(canceledText));
}

// ── §9.8 P1: optional overlay text printed on the burn-in boxes ──────────────
void TestRedactTransaction::overlayTextIsPrintedOnBurnedInBoxes() {
    const QString src = createPdf("overlay.pdf", 2);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("overlay_redacted.pdf");

    RedactRequest req = makeRequest(src, dest, {0, 1}, false);
    req.overlayText = QStringLiteral("CLASSIFIED");
    RedactOperation op(req);
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    // The reason code is PRINTED ON the black boxes — independently
    // extractable on every redacted page — while the excision contract is
    // untouched: the secret is gone, the public line survives.
    for (int p = 0; p < 2; ++p) {
        const QString text = pageText(dest, p);
        QVERIFY2(text.contains(QLatin1String("CLASSIFIED")),
                 qPrintable(QStringLiteral("overlay text missing on page %1: %2").arg(p).arg(text)));
        QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")),
                 qPrintable(QStringLiteral("secret survived on page %1: %2").arg(p).arg(text)));
        QVERIFY2(text.contains(QLatin1String("PUBLIC_KEEP_TEXT")),
                 qPrintable(QStringLiteral("public text lost on page %1").arg(p)));
    }
}

void TestRedactTransaction::emptyOverlayTextPreservesCurrentBehavior() {
    const QString src = createPdf("nooverlay.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("nooverlay_redacted.pdf");

    RedactRequest req = makeRequest(src, dest, {0}, false);
    QVERIFY2(req.overlayText.isEmpty(), "overlayText must default to empty");
    RedactOperation op(req);
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    // Empty overlay = exactly the current behavior: excision happens, no
    // overlay text is added to the page.
    const QString text = pageText(dest, 0);
    QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("PUBLIC_KEEP_TEXT")), qPrintable(text));
}

void TestRedactTransaction::overlaySkippedWhenBoxTooSmall() {
    const QString src = createPdf("smalloverlay.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("smalloverlay_redacted.pdf");

    // N08: "too small" is now a METRIC verdict — a box shorter than the font's
    // ascent+descent extent at the overlay size cannot carry the glyphs. Half
    // that extent (~3.3pt at 7pt Helvetica, extent ≈ 6.57pt) is provably below
    // the minimum, and the band still crosses the secret's baseline (pdf
    // y 698.6..701.9 contains y=700 — the engine excises on baseline-span
    // intersection): the box IS burned in, but the overlay must be skipped
    // (appearance auto-fit precedent), never squeezed in or drawn outside.
    const OverlayExtents ext = overlayFontExtents(7.0);
    const double boxH = ext.extent * 0.5;
    RedactRequest req = makeRequest(src, dest, {0}, false);
    req.redactionsByPage.clear();
    req.redactionsByPage[0].append(QRectF(40, 140, 300, boxH));
    req.overlayText = QStringLiteral("CLASSIFIED");
    RedactOperation op(req);
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    const QString text = pageText(dest, 0);
    QVERIFY2(!text.contains(QLatin1String("CLASSIFIED")),
             qPrintable(QStringLiteral("overlay must be skipped on a %1pt-tall box "
                                      "(below the %2pt font extent): %3")
                            .arg(boxH).arg(ext.extent).arg(text)));
    QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")),
             qPrintable(QStringLiteral("excision must not depend on overlay fit: %1").arg(text)));
}

// ── N08: at the minimum box height the glyphs are fully INSIDE the box ───────
// The contract: minimum box height = the font's ascent+descent at the overlay
// size; the drawn baseline vertically centers that extent, so at the minimum
// the glyph extent exactly touches both edges and never crosses them. A box
// above the minimum must show clear symmetric margins (the pre-fix
// 0.35*fontSize baseline pushed the ascender tops ~3pt out of a 9pt box).
void TestRedactTransaction::overlayGlyphsStayInsideMinimumHeightBox() {
    const OverlayExtents ext = overlayFontExtents(7.0);
    QVERIFY2(ext.ascent > 0.0 && ext.descent > 0.0,
             qPrintable(QStringLiteral("font metrics unusable: ascent=%1 descent=%2")
                            .arg(ext.ascent).arg(ext.descent)));

    const QString src = createPdf("n08fit.pdf", 2);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("n08fit_redacted.pdf");

    RedactRequest req = makeRequest(src, dest, {0, 1}, false);
    req.redactionsByPage.clear();
    // Page 0: 9pt box — the pre-fix hardcoded minimum and the worst N08 case
    // (7pt glyphs DO fit its 6.57pt extent, but the guessed baseline pushed
    // the ascender tops ~3pt out of the box). Page 1: box at EXACTLY the
    // metric minimum (ascent+descent ≈ 6.57pt). Both bands cross the secret's
    // baseline (pdf y=700) so the box burns in.
    req.redactionsByPage[0].append(QRectF(40, 138.5, 300, 9.0));
    req.redactionsByPage[1].append(QRectF(40, 138.5, 300, ext.extent));
    req.overlayText = QStringLiteral("CLASSIFIED");
    RedactOperation op(req);
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    // The excision contract is untouched on both pages.
    for (int p = 0; p < 2; ++p) {
        const QString text = pageText(dest, p);
        QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")),
                 qPrintable(QStringLiteral("secret survived on page %1: %2").arg(p).arg(text)));
    }

    // Output geometry + content streams, read back from the artifact.
    PoDoFo::PdfMemDocument out;
    out.Load(dest.toUtf8().constData()); // throws on failure -> test aborts
    PdfiumBackend renderer;
    QVERIFY2(renderer.loadDocument(dest), "could not load the output for rasterization");

    constexpr double kDpi = 288.0;
    const double scale = kDpi / 72.0;
    for (int p = 0; p < 2; ++p) {
        const double pageH = out.GetPages().GetPageAt(p).GetMediaBox().Height;
        const double boxH = (p == 0) ? 9.0 : ext.extent;
        const double boxTopPdf = pageH - 138.5;
        const double boxBottomPdf = boxTopPdf - boxH;

        // (a) Exact math on the DRAWN text op: the white label's Tf size and
        // Td baseline must keep the full glyph extent inside the box.
        const PoDoFo::charbuff content =
            out.GetPages().GetPageAt(p).GetContents()->GetCopy();
        const QByteArray stream(content.data(), static_cast<int>(content.size()));
        OverlayTextOp drawn;
        QVERIFY2(findWhiteOverlayOp(stream, &drawn),
                 qPrintable(QStringLiteral("page %1: no white overlay text op — a %2pt box "
                                          "at the %3pt font-extent minimum must carry the "
                                          "label").arg(p).arg(boxH).arg(ext.extent)));
        QVERIFY2(qAbs(drawn.size - 7.0) < 1e-9,
                 qPrintable(QStringLiteral("overlay drawn at %1pt, expected 7pt").arg(drawn.size)));
        const double ascenderTop = drawn.y + ext.ascent;
        const double descenderBottom = drawn.y - ext.descent;
        QVERIFY2(descenderBottom >= boxBottomPdf - 1e-6,
                 qPrintable(QStringLiteral("page %1: descenders reach %2, below the box "
                                          "bottom %3").arg(p).arg(descenderBottom)
                                .arg(boxBottomPdf)));
        QVERIFY2(ascenderTop <= boxTopPdf + 1e-6,
                 qPrintable(QStringLiteral("N08 page %1: ascender top %2 > box top %3 — "
                                          "glyphs extend %4pt outside the box "
                                          "(baseline %5, ascent %6)")
                                .arg(p).arg(ascenderTop).arg(boxTopPdf)
                                .arg(ascenderTop - boxTopPdf).arg(drawn.y).arg(ext.ascent)));

        // (b) Pixel truth: render the page; the label must be visible INSIDE
        // the black box (white-on-white overflow is invisible to text
        // extraction, so the raster pins visibility and position).
        const QImage img = renderer.renderPage(p, static_cast<int>(kDpi));
        QVERIFY2(!img.isNull(), "rasterization failed");
        QRect boxPx(lround(40 * scale), lround(138.5 * scale),
                    lround(300 * scale), lround(boxH * scale));
        boxPx = boxPx.intersected(img.rect());
        const QRect interior = boxPx.adjusted(2, 2, -2, -2); // skip AA edge rows
        const GlyphRows rows = scanBoxRows(img, interior);
        QVERIFY2(rows.blackPixels > rows.totalPixels / 2,
                 qPrintable(QStringLiteral("page %1: box interior is not burned-in black "
                                          "(%2/%3 black)").arg(p).arg(rows.blackPixels)
                                .arg(rows.totalPixels)));
        QVERIFY2(rows.anyWhite,
                 qPrintable(QStringLiteral("page %1: no visible label glyphs inside the box")
                                .arg(p)));
        QVERIFY2(rows.whiteTop >= boxPx.top() - 1,
                 qPrintable(QStringLiteral("page %1: glyph pixels above the box (row %2 < %3)")
                                .arg(p).arg(rows.whiteTop).arg(boxPx.top())));
        QVERIFY2(rows.whiteBottom <= boxPx.bottom() + 1,
                 qPrintable(QStringLiteral("page %1: glyph pixels below the box (row %2 > %3)")
                                .arg(p).arg(rows.whiteBottom).arg(boxPx.bottom())));
        if (boxH > ext.extent + 1e-9) {
            // Above the minimum the extent must be CENTERED: symmetric margins
            // between the glyph extremes and both box edges (≈1.22pt ≈ 5px at
            // 288dpi for the 9pt box). The pre-fix baseline put glyph ink on
            // the box's top edge rows instead.
            const double marginPx = (boxH - ext.extent) / 2.0 * scale;
            const int minGap = qMax(1.0, marginPx - 2.0);
            QVERIFY2(rows.whiteTop >= boxPx.top() + minGap,
                     qPrintable(QStringLiteral("N08 page %1: glyphs touch the box top "
                                              "(white row %2, box top %3, expected gap "
                                              "≥%4px from the %5pt margin) — extent not "
                                              "centered / overflowing")
                                    .arg(p).arg(rows.whiteTop).arg(boxPx.top())
                                    .arg(minGap).arg(marginPx / scale)));
            QVERIFY2(rows.whiteBottom <= boxPx.bottom() - minGap,
                     qPrintable(QStringLiteral("page %1: glyphs touch the box bottom "
                                              "(white row %2, box bottom %3, expected gap "
                                              "≥%4px)").arg(p).arg(rows.whiteBottom)
                                    .arg(boxPx.bottom()).arg(minGap)));
        }

        // (c) The label is independently extractable from the committed page.
        const QString text = pageText(dest, p);
        QVERIFY2(text.contains(QLatin1String("CLASSIFIED")),
                 qPrintable(QStringLiteral("overlay text missing on page %1: %2").arg(p).arg(text)));
    }
}

// ── D02: worker-durable execution state ─────────────────────────────────────
// The pre-fix start() captured a QPointer, checked it ONCE, and called
// self->run() on a worker: the whole state machine then kept reading members
// of a QObject that its UI parent could destroy at any moment (QPointer
// observes deletion — it does not keep the object alive DURING the call).
// These tests pin the demanded shape: the worker owns its request/cancel/
// execution state, destruction of the UI-side object never crashes an
// in-flight run, no callback reaches a destroyed owner, repeated start()
// cannot create overlapping runs, and cancel still works across threads.

// Normal completion: start() is asynchronous and the outcome reaches a live
// owner's receiver through the queued finished connection (exactly how the
// real hosts in RedactMode.cpp / SecurityController.cpp connect).
void TestRedactTransaction::startDeliversFinishedViaQueuedConnectionToLiveOwner() {
    const QString src = createPdf("d02live.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("d02live_redacted.pdf");

    QObject host; // the UI-owned parent, exactly like the real hosts
    auto* op = new RedactOperation(makeRequest(src, dest, {0}, false), &host);

    RedactResult captured;
    std::atomic<int> finishedCalls{0};
    connect(op, &RedactOperation::finished, this,
            [&captured, &finishedCalls](const RedactResult& r) {
                captured = r;
                ++finishedCalls;
            });

    op->start();
    // No event loop has run since start(): a queued delivery cannot have
    // happened yet. A result here would mean start() executed the
    // transaction synchronously on the caller's thread.
    QVERIFY2(captured.destination.isEmpty(),
             "start() must be asynchronous: no result before any event drain");

    // 30s budget: the first async test in the process pays cold-start costs.
    QTRY_VERIFY_WITH_TIMEOUT(captured.outcome == RedactOutcome::Completed, 30000);
    QCOMPARE(captured.destination, dest);
    QCOMPARE(finishedCalls.load(), 1);
    QVERIFY(QFileInfo::exists(dest));
    QTest::qWait(30); // worker thread + its QThread object tear down
}

// Cancellation is honored on the async path too: the shared atomic is set
// from the worker-side hook (the same cooperative seam the progress dialog's
// Cancel button drives from the UI thread) and the Canceled outcome is
// delivered to the live owner.
void TestRedactTransaction::asyncCancelIsHonoredAndDelivered() {
    const QString src = createPdf("d02cancel.pdf", 3);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("d02cancel_redacted.pdf");

    QObject host;
    auto* op = new RedactOperation(makeRequest(src, dest, {0, 1, 2}, false), &host);
    op->setPageBoundaryHook([op](int pagesDone) {
        if (pagesDone == 1) op->cancel();
    });
    RedactResult captured;
    connect(op, &RedactOperation::finished, this,
            [&captured](const RedactResult& r) { captured = r; });

    op->start();
    QTRY_VERIFY_WITH_TIMEOUT(captured.outcome == RedactOutcome::Canceled, 30000);
    QCOMPARE(captured.pagesProcessed, 1);
    QVERIFY(captured.destination.isEmpty());
    QVERIFY(!QFileInfo::exists(dest)); // cancellation still writes nothing
    QTest::qWait(30);
}

// Owner destruction BEFORE dispatch + repeated start: the second start() of
// one mutable operation must be refused (no overlapping runs), and deleting
// the owner immediately after start() — possibly before the worker has even
// dispatched — must neither crash nor cancel the run: the durable state
// carries it to completion.
void TestRedactTransaction::repeatedStartRunsOnceAndOwnerDestroyedBeforeDispatchIsSafe() {
    const QString src = createPdf("d02dispatch.pdf", 2);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("d02dispatch_redacted.pdf");

    std::atomic<int> hookCalls{0};
    auto* op = new RedactOperation(makeRequest(src, dest, {0, 1}, false));
    op->setPageBoundaryHook([&hookCalls](int) { ++hookCalls; });

    op->start();
    op->start(); // must be REFUSED — one operation, one execution

    // Destroy the owner while the worker may be running (explicit delete,
    // the harshest variant — no deleteLater deferral).
    delete op;

    // The durable state must carry the run to completion although the owner
    // died before the worker even dispatched.
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(dest), 30000);
    QTest::qWait(30);
    // Exactly ONE execution crossed the 2 page boundaries. Pre-fix code
    // either launched two overlapping runs (4 hook calls, both racing the
    // freed members) or skipped the run entirely when the QPointer died
    // before dispatch (0 hook calls, no output).
    QCOMPARE(int(hookCalls.load()), 2);
    QVERIFY(QFileInfo::exists(dest));
}

// Owner destruction DURING processing, at a deterministic boundary: from the
// worker's first page boundary the hook asks the UI thread to delete the
// owner and pins the worker until that deletion has actually happened.
// Everything the run touches after that point must be the durable state —
// the transaction completes, the boundary hook keeps firing (the state
// outlived the QObject), and NO stage/finished callback is delivered after
// the owner's death.
void TestRedactTransaction::ownerDestroyedMidRunWorkerCompletesOnDurableStateWithoutCallbacks() {
    const QString src = createPdf("d02midrun.pdf", 3);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("d02midrun_redacted.pdf");

    std::atomic<int> hookCalls{0};
    std::atomic<bool> opDestroyed{false};
    std::atomic<int> stageCalls{0};
    std::atomic<int> finishedCalls{0};
    auto* op = new RedactOperation(makeRequest(src, dest, {0, 1, 2}, false));
    op->setPageBoundaryHook([&](int) {
        ++hookCalls;
        if (hookCalls.load() != 1) return;
        // Deterministic mid-run destruction: ask the UI thread (the test's
        // thread) to delete the owner, then hold the worker inside this hook
        // until the destructor has actually run.
        QMetaObject::invokeMethod(qApp, [&]() {
            delete op; // synchronous: ~RedactOperation (and its owner detach) ran
            opDestroyed.store(true);
        }, Qt::QueuedConnection);
        while (!opDestroyed.load())
            QThread::msleep(1);
    });
    // Surviving UI-side receivers with queued delivery, like the real hosts.
    connect(op, &RedactOperation::stageChanged, this,
            [&stageCalls](RedactStage, int, int) { ++stageCalls; },
            Qt::QueuedConnection);
    connect(op, &RedactOperation::finished, this,
            [&finishedCalls](const RedactResult&) { ++finishedCalls; },
            Qt::QueuedConnection);

    op->start();
    QTRY_VERIFY_WITH_TIMEOUT(opDestroyed.load(), 30000);
    // The run continues on the durable state and completes its transaction.
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(dest), 30000);
    QTest::qWait(30);
    // The state outlived the QObject: all 3 page boundaries were crossed —
    // the last two strictly AFTER `delete op`.
    QCOMPARE(int(hookCalls.load()), 3);
    // Preflight + the Redacting(0,3) progress emission happened before the
    // destruction; nothing may be delivered afterwards.
    QCOMPARE(int(stageCalls.load()), 2);
    QCOMPARE(int(finishedCalls.load()), 0); // no callback past the owner's death
}

// ── NCR-02: the synchronous entry point must obey the one-shot contract ─────
// Pre-fix run() called m_exec->execute() directly — no tryBeginRun() gate and
// no local strong reference — so run();run() executed twice, run()/start()
// pairings crossed the gate three times, and a direct-connected finished()
// slot that destroyed the operation freed the execution state out from under
// the running execute(). A failing fixture keeps executions cheap and honest:
// every refused entry emits NOTHING, every accepted one emits exactly one
// finished, and the page-boundary hook fires once per accepted execution.

// run(); run() — the second synchronous call must be refused.
void TestRedactTransaction::syncRunTwiceExecutesOnce() {
    const QString src = createPdf("ncr02rr.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("ncr02rr_redacted.pdf");

    std::atomic<int> execCount{0};
    std::atomic<int> finishedCount{0};
    RedactOperation op(makeRequest(src, dest, {0}, false));
    op.setPageBoundaryHook([&execCount](int) { ++execCount; });
    connect(&op, &RedactOperation::finished, this,
            [&finishedCount](const RedactResult&) { ++finishedCount; });

    op.run();
    op.run(); // must be REFUSED — one operation, one execution

    QCOMPARE(int(finishedCount.load()), 1);
    QCOMPARE(int(execCount.load()), 1);
    QVERIFY(QFileInfo::exists(dest));
}

// run(); start() — start() after a completed synchronous run must be refused.
// Bounded QTRY window: if the (would-be) second execution were dispatched, it
// would deterministically raise both counters within the budget.
void TestRedactTransaction::runThenStartExecutesOnce() {
    const QString src = createPdf("ncr02rs.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("ncr02rs_redacted.pdf");

    std::atomic<int> execCount{0};
    std::atomic<int> finishedCount{0};
    RedactOperation op(makeRequest(src, dest, {0}, false));
    op.setPageBoundaryHook([&execCount](int) { ++execCount; });
    connect(&op, &RedactOperation::finished, this,
            [&finishedCount](const RedactResult&) { ++finishedCount; });

    op.run();
    QCOMPARE(int(finishedCount.load()), 1);
    op.start(); // must be REFUSED

    QTRY_VERIFY_WITH_TIMEOUT(int(finishedCount.load()) == 1
                             && int(execCount.load()) == 1, 5000);
    QTest::qWait(30); // worker teardown would land here if one had started
    QCOMPARE(int(finishedCount.load()), 1);
    QCOMPARE(int(execCount.load()), 1);
}

// start(); run() — run() after a dispatched asynchronous run must be refused
// (pre-fix it entered the same mutable execution state concurrently with the
// worker).
void TestRedactTransaction::startThenRunExecutesOnce() {
    const QString src = createPdf("ncr02sr.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("ncr02sr_redacted.pdf");

    std::atomic<int> execCount{0};
    std::atomic<int> finishedCount{0};
    RedactOperation op(makeRequest(src, dest, {0}, false));
    op.setPageBoundaryHook([&execCount](int) { ++execCount; });
    connect(&op, &RedactOperation::finished, this,
            [&finishedCount](const RedactResult&) { ++finishedCount; });

    op.start();
    QTRY_VERIFY_WITH_TIMEOUT(int(finishedCount.load()) == 1, 30000);
    op.run(); // must be REFUSED — the one-shot was consumed by start()

    QTRY_VERIFY_WITH_TIMEOUT(int(execCount.load()) == 1, 5000);
    QTest::qWait(30);
    QCOMPARE(int(finishedCount.load()), 1);
    QCOMPARE(int(execCount.load()), 1);
    QVERIFY(QFileInfo::exists(dest));
}

// The literal NCR-02 ownership probe: a DIRECT-connected finished() slot that
// explicitly deletes the operation — the harshest variant of the review's
// "synchronous direct-callback owner-destruction check". Post-fix run() holds
// a strong local reference, so the state outlives the member call: no crash,
// run() returns, the transaction completed. (No sanitizer claim — the
// pre-fix shape freed the state under the running execute(); the deterministic
// mid-execute variant below is the anchor that fails on the old code.)
void TestRedactTransaction::syncOwnerDeletedFromDirectFinishedSlotRunStillCompletes() {
    const QString src = createPdf("ncr02own.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("ncr02own_redacted.pdf");

    auto* op = new RedactOperation(makeRequest(src, dest, {0}, false));
    connect(op, &RedactOperation::finished, op,
            [op](const RedactResult&) { delete op; });

    op->run(); // must return without touching freed state

    QVERIFY2(QFileInfo::exists(dest),
             "the run deleted from its own finished() slot must still complete "
             "its transaction");
}

// The deterministic pre-fix anchor: the page-boundary hook (called from INSIDE
// ExecutionState::execute(), outside any signal emission) deletes the owner.
// Pre-fix, run() held no local reference: ~RedactOperation freed the
// execution state while execute() was still running on it — every subsequent
// member access (cancel flag, request, config mutex) was use-after-free, and
// the transaction could not complete truthfully. Post-fix the local strong
// reference carries the run to completion; the destroyed owner receives no
// finished() callback.
void TestRedactTransaction::syncOwnerDeletedAtPageBoundaryRunCompletesWithoutCallbacks() {
    const QString src = createPdf("ncr02hook.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("ncr02hook_redacted.pdf");

    std::atomic<bool> opDestroyed{false};
    std::atomic<int> finishedCount{0};
    auto* op = new RedactOperation(makeRequest(src, dest, {0}, false));
    op->setPageBoundaryHook([op, &opDestroyed](int) {
        if (opDestroyed.exchange(true)) return;
        delete op; // free the owner mid-execute — the state must survive
    });
    // Receiver outlives the operation (the test object), direct connection.
    connect(op, &RedactOperation::finished, this,
            [&finishedCount](const RedactResult&) { ++finishedCount; });

    op->run();
    QVERIFY(opDestroyed.load());
    QVERIFY2(QFileInfo::exists(dest),
             "the run whose owner died mid-execute must complete on the "
             "durable state");
    QCOMPARE(int(finishedCount.load()), 0); // no callback past the owner's death
}

// ── N08 residual: horizontal fit, measured on the saved artifact ─────────────
// The overlay label's advance width comes from the test's own PoDoFo oracle
// (Helvetica at 7pt) — the production skip/center decisions must agree with
// that measurement, not with a private guess.

// A box NARROWER than the label's advance width (but tall enough vertically)
// must SKIP the label: no squeeze, no sideways overflow, excision untouched.
void TestRedactTransaction::overlaySkippedWhenBoxTooNarrow() {
    const QString src = createPdf("n08narrow.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("n08narrow_redacted.pdf");

    // The label the production path would draw, measured independently.
    const QByteArray label = QByteArrayLiteral("CLASSIFIED");
    PoDoFo::PdfMemDocument oracle;
    oracle.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    auto& font = oracle.GetFonts().GetStandard14Font(
        PoDoFo::PdfStandard14FontType::Helvetica);
    PoDoFo::PdfTextState state;
    state.Font = &font;
    state.FontSize = kTestOverlayFontSize;
    const double textWidth = font.GetStringLength(label.constData(), state);
    QVERIFY2(textWidth > 10.0, "oracle must produce a plausible advance width");

    const OverlayExtents ext = overlayFontExtents(kTestOverlayFontSize);
    // Height: exactly the metric minimum (vertically legal). Width: 60% of the
    // label's advance width — provably too narrow. The band still crosses the
    // secret's baseline (pdf y=700), so the box IS burned in.
    RedactRequest req = makeRequest(src, dest, {0}, false);
    req.redactionsByPage.clear();
    req.redactionsByPage[0].append(QRectF(40, 138.5, textWidth * 0.6, ext.extent));
    req.overlayText = QString::fromLatin1(label);
    RedactOperation op(req);
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    // The label must be ABSENT (skipped) while the excision still happened and
    // the surviving public text is untouched.
    const QString text = pageText(dest, 0);
    QVERIFY2(!text.contains(QString::fromLatin1(label)),
             qPrintable(QStringLiteral("label must be skipped on a box narrower "
                                      "than its %1pt advance width: %2")
                            .arg(textWidth).arg(text)));
    QVERIFY2(!text.contains(QLatin1String("TOPSECRET_DATA")), qPrintable(text));
    QVERIFY2(text.contains(QLatin1String("PUBLIC_KEEP_TEXT")), qPrintable(text));
}

// A drawn label must be horizontally CENTERED: the Td x operand equals
// boxLeft + (boxWidth - measuredAdvance)/2, on the committed artifact.
void TestRedactTransaction::overlayCenteredHorizontallyInWideBox() {
    const QString src = createPdf("n08center.pdf", 1);
    QVERIFY2(!src.isEmpty(), "fixture creation failed");
    const QString dest = m_tmpDir.filePath("n08center_redacted.pdf");

    const QByteArray label = QByteArrayLiteral("CLASSIFIED");
    PoDoFo::PdfMemDocument oracle;
    oracle.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    auto& font = oracle.GetFonts().GetStandard14Font(
        PoDoFo::PdfStandard14FontType::Helvetica);
    PoDoFo::PdfTextState state;
    state.Font = &font;
    state.FontSize = kTestOverlayFontSize;
    const double textWidth = font.GetStringLength(label.constData(), state);

    const double boxX = 40.0, boxW = 300.0, boxY = 138.5;
    RedactRequest req = makeRequest(src, dest, {0}, false);
    req.redactionsByPage.clear();
    req.redactionsByPage[0].append(QRectF(boxX, boxY, boxW, 20.0));
    req.overlayText = QString::fromLatin1(label);
    RedactOperation op(req);
    const RedactResult r = runOp(&op);
    QVERIFY2(r.outcome == RedactOutcome::Completed, qPrintable(errText(r)));

    // Read the drawn text op back from the saved artifact.
    PoDoFo::PdfMemDocument out;
    out.Load(dest.toUtf8().constData()); // throws on failure -> test aborts
    const PoDoFo::charbuff content =
        out.GetPages().GetPageAt(0).GetContents()->GetCopy();
    const QByteArray stream(content.data(), static_cast<int>(content.size()));
    OverlayTextOp drawn;
    QVERIFY2(findWhiteOverlayOp(stream, &drawn),
             "the wide box must carry the overlay label");
    const double expectedX = boxX + (boxW - textWidth) / 2.0;
    QVERIFY2(qAbs(drawn.x - expectedX) < 1e-6,
             qPrintable(QStringLiteral("label drawn at x=%1, expected centered "
                                      "x=%2 (advance %3pt)")
                            .arg(drawn.x).arg(expectedX).arg(textWidth)));
    // And the label is extractable from the committed page (it was drawn).
    QVERIFY2(pageText(dest, 0).contains(QString::fromLatin1(label)),
             "centered label must be present on the committed page");
}

QTEST_MAIN(TestRedactTransaction)
#include "TestRedactTransaction.moc"
