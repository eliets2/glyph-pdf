// SPDX-License-Identifier: Apache-2.0
// Repair-order step 2 (2026-09-08): document identity across the open boundary.
//
// Regression tests for TEAM-ARCHITECTURE-REVIEW-2026-09-07 findings ARC05, ARC01
// and ARC02:
//   ARC05 — a successful Open must establish the editing backend: the engine
//           must serve the freshly opened document (identity, real reads,
//           persistence to a distinct output) with NO stale/no-backend state,
//           on first open AND after a switch to B.
//   ARC01 — undo history must never cross documents: A→B, A→B→A reopen and
//           same-path A→A reload all reset history; a failed open destroys
//           nothing. Assertions check saved/reopened artifacts (page count,
//           rendered orientation), not just the undo index.
//   ARC02 — annotation/comment sidecar state belongs to one document identity:
//           A's pending note is flushed to A's sidecar (synchronously, so the
//           test needs no sleeps), B's in-memory list and sidecar start empty
//           (missing sidecar = explicit empty default), and each document
//           reloads its own records.
//
// KNOWN PRE-EXISTING RESIDUAL (EC01 engine-lane follow-up, NOT owned here):
// a same-path engine write fails with "commit to destination failed: Access is
// denied" while the viewer's QPdfDocument holds the file open. The reviewed
// EC01 acceptance flagged exactly this ("a viewer lock can change a destructive
// failure into an access-denied failure"). At HEAD the app cannot rotate/save
// in place while the document is displayed, INDEPENDENT of backend readiness —
// even a primed engine fails the same way (pinned by
// freshOpenEstablishesEditingBackend below, which asserts the mutation REACHES
// the resident document instead of a null backend).
//
// The tests instantiate the REAL MainWindow over the REAL Bootstrapper context
// and REAL generated PDFs (QPdfWriter fixtures) offscreen. They intentionally
// avoid referencing any post-fix-only API so the same binaries can be built
// against the pre-fix baseline for revert verification (git stash push -- src/).
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QUndoStack>
#include <QPdfWriter>
#include <QPainter>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/AnnotationSerializer.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "engines/DocumentSession.h"
#include "ui/PdfViewerWidget.h"
#include "commands/RotatePageCommand.h"

using gp::MainWindow;

namespace {

// Portrait A4 one-page fixture with a marker text.
void makePdf(const QString &path, const QString &marker)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&w);
    p.drawText(100, 100, marker);
    p.end();
    QVERIFY(QFileInfo::exists(path));
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

// Deterministically dismiss the failed-open error dialog: openDocument shows a
// modal ErrorDialog; the zero-timeout timer fires inside the dialog's nested
// event loop and closes it (Rejected ⇒ no Retry recursion).
void scheduleModalDismiss()
{
    QTimer::singleShot(0, [] {
        if (QWidget *w = QApplication::activeModalWidget())
            w->close();
    });
}

} // namespace

class TestDocumentIdentity : public QObject {
    Q_OBJECT

    std::unique_ptr<MainWindow> m_win;

private slots:
    void initTestCase()
    {
        // Isolate QSettings (AutosaveManager interval, recent files, updater):
        // never read the user's real prefs, never clobber them.
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestDocumentIdentity"));
    }

    void init()
    {
        // Fresh window + fresh real context per test (openDocument is the
        // boundary under test; no state may leak between methods).
        m_win = std::make_unique<MainWindow>(Bootstrapper::createContext());
        QVERIFY(m_win->pdfViewer());
        QVERIFY(m_win->appContext()->pdfEditor);
        QVERIFY(m_win->appContext()->document);
        QVERIFY(m_win->appContext()->undoStack);
    }

    void cleanup()
    {
        m_win.reset();
    }

    // ── ARC05 ────────────────────────────────────────────────────────────────
    // A fresh Open must produce a live editing backend for the opened
    // document: the engine's identity follows the open, real document reads
    // succeed with no priming tool, persistence to a distinct output works
    // immediately, and a mutation attempt reaches the RESIDENT document
    // instead of failing with the reviewed ARC05 no-backend state. Repeated
    // after switching to B.
    void freshOpenEstablishesEditingBackend()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        const QString b = dir.filePath("b.pdf");
        makePdf(a, "AAA");
        makePdf(b, "BBB");
        auto *ctx = m_win->appContext();

        m_win->openDocument(a);
        QCOMPARE(ctx->document->path(), a);

        // THE ARC05 contract: with no priming tool, the backend already
        // describes and serves the opened document.
        QCOMPARE(ctx->pdfEditor->currentFile(), a);
        QVERIFY2(!ctx->pdfEditor->extractPageAsBytes(a, 0).isEmpty(),
                 "ARC05: the engine must read the freshly opened document");
        const QString aCopy = dir.filePath("a_copy.pdf");
        QVERIFY2(ctx->pdfEditor->saveDocument(aCopy),
                 "ARC05: persistence must work right after open");
        {
            PdfViewerWidget probe;
            QVERIFY(probe.loadDocument(aCopy));
            QCOMPARE(probe.pageCount(), 1);
        }

        // Mutations must REACH the resident document. (The same-path write
        // commit itself is blocked by the viewer-held file handle — the
        // pre-existing EC01 GUI-handle residual documented in the header —
        // which is exactly NOT the ARC05 defect. The reviewed ARC05 failure
        // was "No document is open for editing": a NULL backend.)
        ctx->pdfEditor->rotatePage(a, 0, 90);
        QVERIFY2(!ctx->pdfEditor->lastError().userMessage.contains(
                     QStringLiteral("No document is open")),
                 "ARC05: a mutation must reach the resident document, not a null backend");

        // Repeat after switching to B — the backend follows the open.
        m_win->openDocument(b);
        QCOMPARE(ctx->document->path(), b);
        QCOMPARE(ctx->pdfEditor->currentFile(), b);
        QVERIFY2(!ctx->pdfEditor->extractPageAsBytes(b, 0).isEmpty(),
                 "ARC05: the engine must serve B right after the switch");
        const QString bCopy = dir.filePath("b_copy.pdf");
        QVERIFY2(ctx->pdfEditor->saveDocument(bCopy),
                 "ARC05: persistence must work right after a switch");
        {
            PdfViewerWidget probe;
            QVERIFY(probe.loadDocument(bCopy));
            QCOMPARE(probe.pageCount(), 1);
            const QImage page0 = probe.renderPage(0, 1.0);
            QVERIFY(!page0.isNull());
            QVERIFY2(page0.height() > page0.width(),
                     "ARC05: the engine must serve B (portrait), not a stale rotated A");
        }
    }

    // ── ARC01 ────────────────────────────────────────────────────────────────
    // Undo history must never cross documents. A's rotation command must not
    // mutate B after the switch; A→B→A reopen and same-path A→A reload reset
    // history; a failed open destroys nothing. Checks saved artifacts.
    void undoHistoryDoesNotCrossDocuments()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        const QString b = dir.filePath("b.pdf");
        makePdf(a, "AAA");
        makePdf(b, "BBB");
        auto *ctx = m_win->appContext();

        // Edit A: one real rotation command in the stack.
        m_win->openDocument(a);
        ctx->undoStack->push(new RotatePageCommand(
            ctx->pdfEditor.get(), ctx->document.get(), 0, 90));
        QCOMPARE(ctx->undoStack->count(), 1);

        // Switch to B. (The explicit engine init models the existing
        // text/metadata flows that load the backend ad hoc; after the ARC05
        // fix openDocument has already done this and the call is idempotent.)
        m_win->openDocument(b);
        ctx->pdfEditor->loadDocumentForEditing(b);

        // History must be EMPTY for B: no command created for A survives.
        QCOMPARE(ctx->undoStack->count(), 0);
        QVERIFY2(!ctx->undoStack->canUndo(), "ARC01: undo must be unavailable for B");

        // Undo must not mutate B — proven on saved bytes, not just the index.
        ctx->undoStack->undo();
        const QString bCopy = dir.filePath("b_copy.pdf");
        QVERIFY(ctx->pdfEditor->saveDocument(bCopy));
        {
            PdfViewerWidget probe;
            QVERIFY(probe.loadDocument(bCopy));
            QCOMPARE(probe.pageCount(), 1);
            const QImage page0 = probe.renderPage(0, 1.0);
            QVERIFY(!page0.isNull());
            QVERIFY2(page0.height() > page0.width(),
                     "ARC01: undoing A's command must not rotate B (portrait expected)");
        }

        // A→B→A reopen: history reset again, engine live for the reopened A.
        m_win->openDocument(a);
        QCOMPARE(ctx->undoStack->count(), 0);
        QVERIFY(!ctx->pdfEditor->extractPageAsBytes(a, 0).isEmpty());
        ctx->undoStack->undo();
        QCOMPARE(ctx->undoStack->count(), 0);
        const QString aCopy = dir.filePath("a_copy2.pdf");
        QVERIFY(ctx->pdfEditor->saveDocument(aCopy));
        {
            PdfViewerWidget probe;
            QVERIFY(probe.loadDocument(aCopy));
            QCOMPARE(probe.pageCount(), 1);
            const QImage page0 = probe.renderPage(0, 1.0);
            QVERIFY2(page0.height() > page0.width(),
                     "ARC01: undo after reopen must not act on the reloaded A's stale history");
        }

        // Same-path reload A→A: opening the open document is a NEW revision —
        // history resets to the reloaded revision.
        m_win->openDocument(a);
        ctx->undoStack->push(new RotatePageCommand(
            ctx->pdfEditor.get(), ctx->document.get(), 0, 90));
        QCOMPARE(ctx->undoStack->count(), 1);
        m_win->openDocument(a);
        QCOMPARE(ctx->undoStack->count(), 0);
        QVERIFY2(!ctx->undoStack->canUndo(),
                 "ARC01: same-path reload must reset history to the reloaded revision");
        ctx->undoStack->undo();
        const QString aCopy3 = dir.filePath("a_copy3.pdf");
        QVERIFY(ctx->pdfEditor->saveDocument(aCopy3));
        {
            PdfViewerWidget probe;
            QVERIFY(probe.loadDocument(aCopy3));
            const QImage page0 = probe.renderPage(0, 1.0);
            QVERIFY2(page0.height() > page0.width(),
                     "ARC01: pre-reload history must not act on the reloaded revision");
        }
    }

    // ARC01 acceptance: a failed/canceled open must not destroy the current
    // state (session, engine, history, viewer all keep describing A).
    void failedOpenDestroysNothing()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        const QString junk = dir.filePath("junk.pdf");
        makePdf(a, "AAA");
        {
            QFile f(junk);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("this is not a pdf");
        }
        auto *ctx = m_win->appContext();

        m_win->openDocument(a);
        ctx->undoStack->push(new RotatePageCommand(
            ctx->pdfEditor.get(), ctx->document.get(), 0, 90));
        QCOMPARE(ctx->undoStack->count(), 1);

        scheduleModalDismiss();
        m_win->openDocument(junk);

        QCOMPARE(ctx->document->path(), a);
        QCOMPARE(ctx->undoStack->count(), 1);
        QCOMPARE(m_win->pdfViewer()->filePath(), a);
        QCOMPARE(ctx->pdfEditor->currentFile(), a);

        // …and the app still persists A's resident document afterwards.
        const QString aCopy = dir.filePath("a_copy.pdf");
        QVERIFY(ctx->pdfEditor->saveDocument(aCopy));
        {
            PdfViewerWidget probe;
            QVERIFY(probe.loadDocument(aCopy));
            QCOMPARE(probe.pageCount(), 1);
        }
    }

    // ── ARC02 ────────────────────────────────────────────────────────────────
    // Comment/annotation sidecar state belongs to ONE document identity:
    // A's pending note is flushed into A's sidecar (never into B's), B starts
    // empty when it has no sidecar, and B with its own sidecar shows only B's
    // records. A→B→A round-trips each document's own records.
    void annotationsDoNotLeakAcrossDocuments()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        const QString b = dir.filePath("b.pdf");
        makePdf(a, "AAA");
        makePdf(b, "BBB");
        auto *ctx = m_win->appContext();
        auto *viewer = m_win->pdfViewer();

        AnnotationItem noteOfA;
        noteOfA.pageIndex = 0;
        noteOfA.mode = ToolMode::AddComment;
        noteOfA.rect = QRectF(10, 10, 60, 20);
        noteOfA.text = QStringLiteral("ONLY_A_SECRET_NOTE");

        m_win->openDocument(a);
        viewer->setAnnotations({ noteOfA });   // arms the 2 s debounce
        QCOMPARE(viewer->annotations().size(), 1);

        // Switch INSIDE the debounce window to B, which has NO sidecar.
        m_win->openDocument(b);
        QCOMPARE(ctx->document->path(), b);

        // B's in-memory overlay must be EMPTY (missing sidecar = empty default),
        // never A's retained list.
        QCOMPARE(viewer->annotations().size(), 0);

        // A's pending work was captured against A's identity: A's sidecar holds
        // the note; B's sidecar must never contain it.
        const QString aAnn = a + ".ann";
        const QString bAnn = b + ".ann";
        QVERIFY2(QFileInfo::exists(aAnn), "ARC02: A's pending note must be flushed to A's sidecar");
        const QByteArray aBytes = readFile(aAnn);
        QVERIFY2(aBytes.contains("ONLY_A_SECRET_NOTE"),
                 "ARC02: A's own sidecar must contain A's note");

        // Whatever B's sidecar ends up containing, it must not contain A's note.
        viewer->saveAnnotations();
        QVERIFY(QTest::qWaitFor([&] { return QFileInfo::exists(bAnn); }, 5000));
        const QByteArray bBytes = readFile(bAnn);
        QVERIFY2(!bBytes.contains("ONLY_A_SECRET_NOTE"),
                 "ARC02: B's sidecar must never receive A's comment");

        // B WITH its own sidecar: only B's records are shown/kept.
        AnnotationItem noteOfB;
        noteOfB.pageIndex = 0;
        noteOfB.mode = ToolMode::AddComment;
        noteOfB.rect = QRectF(10, 10, 60, 20);
        noteOfB.text = QStringLiteral("B_OWN_RECORD");
        const QString c = dir.filePath("c.pdf");
        makePdf(c, "CCC");
        {
            QFile f(c + ".ann");
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(AnnotationSerializer::toJson({ noteOfB }).toJson());
        }
        m_win->openDocument(c);
        QCOMPARE(viewer->annotations().size(), 1);
        QCOMPARE(viewer->annotations().first().text, QStringLiteral("B_OWN_RECORD"));

        // A→B→A: reopening A restores A's own records from A's sidecar.
        m_win->openDocument(a);
        QCOMPARE(viewer->annotations().size(), 1);
        QCOMPARE(viewer->annotations().first().text, QStringLiteral("ONLY_A_SECRET_NOTE"));
    }
};

QTEST_MAIN(TestDocumentIdentity)
#include "TestDocumentIdentity.moc"
