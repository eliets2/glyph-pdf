// SPDX-License-Identifier: Apache-2.0
// WP-R03 (WHOLE-ARCHITECTURE-REVIEW-2026-09-10 A02; backlog R03) — complete
// mutation and checked-history coverage: the ROTATE and INLINE-TEXT command
// families must route through the SAME checked seams the G08 work built
// (CheckedUndoCommand + CheckedHistory + the atomic restorePageFromBytes
// seam), so a failed initial mutation, undo, or redo leaves the history
// index, the dirty state and the retryability TRUTHFUL.
//
// A02 probes, per family:
//   - initial mutation failure: the command must never enter history
//     (obsolete → push deletes it), the session stays clean, the failure is
//     reported;
//   - undo (restoration) failure: the index must NOT move, the command stays
//     current, and the traversal is retryable;
//   - redo (re-application) failure: same truthfulness, via the production
//     seam (HomeController ToolId::Redo → CheckedHistory::redo);
//   - the inline-text restoration must be the ATOMIC one-transaction seam:
//     a failed restoration never permits the deletion of the original
//     adjacent page (the pre-fix two-step insert+delete path is gone).
//
// Evidence rules (mirroring TestHistoryIntegrity): fault-injected engines
// carry the failure paths; real-engine artifacts (rotation bytes, page
// counts, PDFium text) carry the success round-trips. Mock call counts alone
// never assert PDF correctness.
//
// COMPILE-COMPATIBILITY (revert verification `git checkout <base> -- src/`):
// every symbol referenced here exists on the PRE-fix baseline. The checked
// redo/apply behavior arrives through the production wiring (HomeController
// ToolId::Redo) and through dynamic_cast inside CheckedHistory::undo, so the
// same binary compiles pre-fix and FAILS at runtime there: the legacy
// fallback really moves the index / records the failed command.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QUndoStack>
#include <QSignalSpy>
#include <QPainter>
#include <QPdfWriter>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"
#include "engines/SafeSave.h"
#include "engines/DocumentSession.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "commands/RotatePageCommand.h"
#include "commands/EditTextInlineCommand.h"
#include "commands/CheckedHistory.h"
#include "mocks/MockPdfEditorEngine.h"
#include "core/AppContext.h"
#include "shell/controllers/HomeController.h"

// wingdi.h defines GetObject as an object-like macro (UNICODE builds); it
// collides with PoDoFo::PdfField::GetObject below.
#ifdef GetObject
#undef GetObject
#endif

namespace {

// Two-page A4 fixture with per-page marker text (the EC01 fixture class).
QString makeTwoPageTextPdf(const QString &dir, const QString &name)
{
    const QString path = dir + QLatin1Char('/') + name;
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    w.setResolution(72);
    QPainter p(&w);
    p.drawText(80, 100, QStringLiteral("R03 page one marker"));
    w.newPage();
    p.drawText(80, 100, QStringLiteral("R03 page two marker"));
    p.end();
    return path;
}

unsigned pdfPageCount(const QString &path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        return doc.GetPages().GetCount();
    } catch (const PoDoFo::PdfError &) {
        return 0;
    }
}

int pdfRotation(const QString &path)
{
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        return static_cast<int>(doc.GetPages().GetPageAt(0).GetRotation());
    } catch (const PoDoFo::PdfError &) {
        return -1;
    }
}

QString extractedText(const QString &path, int page)
{
    PdfiumBackend backend;
    if (!backend.loadDocument(path)) return QString();
    return backend.extractText(page);
}

// Fault engine: per-call failure flags + call counters for the two command
// families under test.
class FaultEngine : public MockPdfEditorEngine {
public:
    bool m_rotateOk = true;
    int m_rotateCalls = 0;
    bool m_editOk = true;
    int m_editCalls = 0;
    bool m_extractOk = true;
    int m_extractCalls = 0;
    bool m_restoreOk = true;
    int m_restoreCalls = 0;
    bool m_insertOk = true;
    int m_insertCalls = 0;
    bool m_deleteOk = true;
    int m_deleteCalls = 0;
    QByteArray m_pageBytes = QByteArray("R03-PAGE0-SNAPSHOT");

    bool rotatePage(const QString &, int, int) override {
        ++m_rotateCalls;
        return m_rotateOk;
    }
    bool editTextInline(int, const QRectF &, const QString &, const QString &,
                        int, const QColor &, bool, bool, int) override {
        ++m_editCalls;
        return m_editOk;
    }
    QByteArray extractPageAsBytes(const QString &, int) override {
        ++m_extractCalls;
        return m_extractOk ? m_pageBytes : QByteArray();
    }
    bool restorePageFromBytes(const QString &, int, const QByteArray &) override {
        ++m_restoreCalls;
        return m_restoreOk;
    }
    bool insertPageFromBytes(const QString &, int, const QByteArray &) override {
        ++m_insertCalls;
        return m_insertOk;
    }
    bool deletePage(const QString &, int) override {
        ++m_deleteCalls;
        return m_deleteOk;
    }
};

} // namespace

class TestCheckedMutationCoverage : public QObject {
    Q_OBJECT

private slots:
    // ── rotate: initial mutation failure never enters history ──────────────
    void rotateInitialFailureIsDroppedAndReported()
    {
        FaultEngine engine;
        engine.m_loaded = true;
        engine.m_file = QStringLiteral("r03-rotate.pdf");
        engine.m_rotateOk = false;
        DocumentSession doc;
        doc.beginDocument(engine.m_file);
        QUndoStack stack;

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
        stack.push(new RotatePageCommand(&engine, &doc, 0, 90));

        // Pre-fix the raw command ignored the failure: the rotation was
        // recorded (count 1) and the session marked dirty — a lie.
        QCOMPARE(stack.count(), 0);
        QVERIFY(!doc.isDirty());
        QCOMPARE(failed.count(), 1);
        QCOMPARE(engine.m_rotateCalls, 1);
    }

    // ── rotate: undo failure keeps index/dirty truthful and retryable ─────
    void rotateUndoFailureKeepsPositionAndIsRetryable()
    {
        FaultEngine engine;
        engine.m_loaded = true;
        engine.m_file = QStringLiteral("r03-rotate-undo.pdf");
        DocumentSession doc;
        doc.beginDocument(engine.m_file);
        QUndoStack stack;

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
        stack.push(new RotatePageCommand(&engine, &doc, 0, 90));
        QCOMPARE(stack.count(), 1);
        QCOMPARE(stack.index(), 1);
        QVERIFY(doc.isDirty());

        engine.m_rotateOk = false;   // the INVERSE rotation will be refused
        // Production seam: HomeController ToolId::Undo → CheckedHistory::undo.
        gp::HomeController ctrl(&m_ctx, nullptr);
        bindSession(stack, doc);
        ctrl.activate(ToolId::Undo);

        // Pre-fix the legacy fallback moved the index to 0 and markReload()ed
        // as if the rotation had been reverted — a lie.
        QCOMPARE(stack.index(), 1);
        QVERIFY(doc.isDirty());
        QVERIFY(stack.canUndo());
        QVERIFY(!stack.canRedo());
        QCOMPARE(failed.count(), 1);
        QCOMPARE(engine.m_rotateCalls, 2);   // exactly one refused attempt

        // Retry after the failure clears succeeds. The session stays dirty:
        // the restoration really changed the document content (markReload),
        // exactly like the G08 crop restore convention.
        engine.m_rotateOk = true;
        ctrl.activate(ToolId::Undo);
        QCOMPARE(stack.index(), 0);
        QCOMPARE(engine.m_rotateCalls, 3);
        QVERIFY(doc.isDirty());
    }

    // ── rotate: redo failure keeps index truthful and retryable ───────────
    void rotateRedoFailureKeepsPositionAndIsRetryable()
    {
        FaultEngine engine;
        engine.m_loaded = true;
        engine.m_file = QStringLiteral("r03-rotate-redo.pdf");
        DocumentSession doc;
        doc.beginDocument(engine.m_file);
        QUndoStack stack;

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
        stack.push(new RotatePageCommand(&engine, &doc, 0, 90));
        QCOMPARE(stack.index(), 1);
        QVERIFY(engine.m_rotateOk);
        stack.undo();                       // plain successful undo (real restore)
        QCOMPARE(stack.index(), 0);
        const int rotateCallsAtClean = engine.m_rotateCalls;

        engine.m_rotateOk = false;   // the RE-application will be refused
        // Production seam: HomeController ToolId::Redo. Post-fix this routes
        // through the checked apply; pre-fix it was a raw stack->redo().
        gp::HomeController ctrl(&m_ctx, nullptr);
        bindSession(stack, doc);
        ctrl.activate(ToolId::Redo);

        // Pre-fix the raw redo moved the index to 1 although nothing happened.
        QCOMPARE(stack.index(), 0);
        QVERIFY(stack.canRedo());
        QCOMPARE(failed.count(), 1);
        QCOMPARE(engine.m_rotateCalls, rotateCallsAtClean + 1);

        // Retry after the failure clears succeeds.
        engine.m_rotateOk = true;
        ctrl.activate(ToolId::Redo);
        QCOMPARE(stack.index(), 1);
        QCOMPARE(engine.m_rotateCalls, rotateCallsAtClean + 2);
    }

    // ── inline text: the restoration is atomic and never deletes the
    //    adjacent original page, even when the restore itself fails ────────
    void inlineTextUndoFailureNeverDeletesAdjacentPage()
    {
        FaultEngine engine;
        engine.m_loaded = true;
        engine.m_file = QStringLiteral("r03-inline.pdf");
        DocumentSession doc;
        doc.beginDocument(engine.m_file);
        QUndoStack stack;

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
        stack.push(new EditTextInlineCommand(&engine, &doc, 0,
                                             QRectF(80, 90, 200, 40),
                                             QStringLiteral("REPLACED"),
                                             QStringLiteral("Helvetica"), 12,
                                             Qt::black, false, false, 0));
        QCOMPARE(stack.count(), 1);
        QCOMPARE(stack.index(), 1);
        QVERIFY(doc.isDirty());

        // The restoration FAILS while the (pre-fix) destructive two-step path
        // would have "succeeded" at deleting the real following page.
        engine.m_restoreOk = false;
        engine.m_insertOk = true;
        engine.m_deleteOk = true;
        gp::HomeController ctrl(&m_ctx, nullptr);
        bindSession(stack, doc);
        ctrl.activate(ToolId::Undo);

        // A02 acceptance: a failed insertion must never permit the deletion
        // of an original adjacent page — post-fix the atomic seam is refused
        // as a whole; pre-fix insert "succeeded" and deletePage destroyed the
        // page at m_page+1 while the index moved to 0.
        QCOMPARE(stack.index(), 1);
        QVERIFY(stack.canUndo());
        QCOMPARE(engine.m_restoreCalls, 1);
        QCOMPARE(engine.m_insertCalls, 0);
        QCOMPARE(engine.m_deleteCalls, 0);
        QCOMPARE(failed.count(), 1);

        // Retryable: the retry succeeds without any two-step writes.
        engine.m_restoreOk = true;
        ctrl.activate(ToolId::Undo);
        QCOMPARE(stack.index(), 0);
        QCOMPARE(engine.m_restoreCalls, 2);
        QCOMPARE(engine.m_insertCalls, 0);
        QCOMPARE(engine.m_deleteCalls, 0);
    }

    // ── inline text: redo failure keeps index truthful and retryable ──────
    void inlineTextRedoFailureKeepsPositionAndIsRetryable()
    {
        FaultEngine engine;
        engine.m_loaded = true;
        engine.m_file = QStringLiteral("r03-inline-redo.pdf");
        DocumentSession doc;
        doc.beginDocument(engine.m_file);
        QUndoStack stack;

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
        stack.push(new EditTextInlineCommand(&engine, &doc, 0,
                                             QRectF(80, 90, 200, 40),
                                             QStringLiteral("REPLACED"),
                                             QStringLiteral("Helvetica"), 12,
                                             Qt::black, false, false, 0));
        QCOMPARE(stack.index(), 1);
        stack.undo();                       // plain successful undo (real restore)
        QCOMPARE(stack.index(), 0);
        const int editCallsAtClean = engine.m_editCalls;

        engine.m_editOk = false;   // the re-application will be refused
        gp::HomeController ctrl(&m_ctx, nullptr);
        bindSession(stack, doc);
        ctrl.activate(ToolId::Redo);

        QCOMPARE(stack.index(), 0);   // pre-fix: raw redo moved the index to 1
        QVERIFY(stack.canRedo());
        QCOMPARE(failed.count(), 1);
        QCOMPARE(engine.m_editCalls, editCallsAtClean + 1);

        engine.m_editOk = true;
        ctrl.activate(ToolId::Redo);
        QCOMPARE(stack.index(), 1);
        QCOMPARE(engine.m_editCalls, editCallsAtClean + 2);
    }

    // ── inline text: the initial edit refuses to run without a restorable
    //    snapshot (EC03 ownership rule for the sibling family) ─────────────
    void inlineTextWithoutSnapshotIsRefused()
    {
        FaultEngine engine;
        engine.m_loaded = true;
        engine.m_file = QStringLiteral("r03-inline-nosnap.pdf");
        engine.m_extractOk = false;
        DocumentSession doc;
        doc.beginDocument(engine.m_file);
        QUndoStack stack;

        QSignalSpy failed(&doc, SIGNAL(mutationFailed(QString)));
        stack.push(new EditTextInlineCommand(&engine, &doc, 0,
                                             QRectF(80, 90, 200, 40),
                                             QStringLiteral("REPLACED"),
                                             QStringLiteral("Helvetica"), 12,
                                             Qt::black, false, false, 0));
        QCOMPARE(stack.count(), 0);      // never an undoable step
        QVERIFY(!doc.isDirty());
        QCOMPARE(failed.count(), 1);
        QCOMPARE(engine.m_editCalls, 0); // the destructive edit never started
    }

    // ── real-engine round trip: checked rotate undo/redo over real
    //    artifacts through the production seam ─────────────────────────────
    void realRotateCheckedRoundTripKeepsSavedGeometryTruthful()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "r03-real-rotate.pdf");
        QCOMPARE(pdfRotation(f), 0);

        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(f));
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;

        stack.push(new RotatePageCommand(&editor, &doc, 0, 90));
        QCOMPARE(stack.count(), 1);
        QCOMPARE(pdfRotation(f), 90);

        gp::HomeController ctrl(&m_ctx, nullptr);
        bindSession(stack, doc);
        m_ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(&editor, [](IPdfEditorEngine *) {});
        ctrl.activate(ToolId::Undo);
        QCOMPARE(stack.index(), 0);
        QCOMPARE(pdfRotation(f), 0);

        ctrl.activate(ToolId::Redo);
        QCOMPARE(stack.index(), 1);
        QCOMPARE(pdfRotation(f), 90);
        QCOMPARE(pdfPageCount(f), 2u);
    }

    // ── real-engine round trip: the inline-text undo restores the original
    //    page bytes in the saved artifact ──────────────────────────────────
    void realInlineTextUndoRestoresSavedPageContent()
    {
        QTemporaryDir dir;
        const QString f = makeTwoPageTextPdf(dir.path(), "r03-real-inline.pdf");

        PdfEditorEngine editor;
        QVERIFY(editor.loadDocumentForEditing(f));
        DocumentSession doc;
        doc.beginDocument(f);
        QUndoStack stack;

        stack.push(new EditTextInlineCommand(&editor, &doc, 0,
                                             QRectF(80, 90, 200, 40),
                                             QStringLiteral("R03 REPLACED TEXT"),
                                             QStringLiteral("Helvetica"), 12,
                                             Qt::black, false, false, 0));
        QCOMPARE(stack.count(), 1);

        // Commit the resident edit so the saved artifact carries it.
        QVERIFY(editor.writeUpdate(f));
        QCOMPARE(extractedText(f, 0).contains(QStringLiteral("R03 REPLACED TEXT")), true);

        // Checked undo: the saved artifact is the ORIGINAL page again.
        CheckedHistory::undo(&stack);
        QCOMPARE(stack.index(), 0);
        QCOMPARE(pdfPageCount(f), 2u);
        QVERIFY2(!extractedText(f, 0).contains(QStringLiteral("R03 REPLACED TEXT")),
                 "the restored page must not contain the replaced text");
        QVERIFY2(extractedText(f, 0).contains(QStringLiteral("R03 page one marker")),
                 "the restored page must carry the original marker");
    }

private:
    // Bind the per-test session objects to the shared AppContext the
    // HomeController seam reads. No-op deleters: the objects are owned by the
    // test frames and re-bound before every use.
    void bindSession(QUndoStack &stack, DocumentSession &doc)
    {
        m_ctx.undoStack = std::shared_ptr<QUndoStack>(&stack, [](QUndoStack *) {});
        m_ctx.document = std::shared_ptr<DocumentSession>(&doc, [](DocumentSession *) {});
    }

    AppContext m_ctx;
};

QTEST_MAIN(TestCheckedMutationCoverage)
#include "TestCheckedMutationCoverage.moc"
