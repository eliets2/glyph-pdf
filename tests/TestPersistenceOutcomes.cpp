// SPDX-License-Identifier: Apache-2.0
// Repair-order step 3 (2026-09-08): persistence truthfulness.
//
// Regression tests for TEAM-ARCHITECTURE-REVIEW-2026-09-07 findings ARC03 and
// ARC04, the engine-lane residual handed over from repair-order step 2 (the
// GUI-held handle: same-path engine writes failed "Access is denied" at the
// SafeSave commit while the viewer's QPdfDocument held the file open), and
// residual V01 (PARITY-BRANCH-REVIEW): the form-import flow deleted the
// temporary output and renamed it onto itself, so the imported result never
// landed in the real document.
//
//   ARC03 — choosing Save while closing used to close the window even when
//           the save failed ("save initiated" was treated as persistence).
//           The close path now proceeds only on a checked Saved outcome;
//           failed/canceled saves keep the window open with all unsaved work
//           and the undo history available for retry.
//   ARC04 — annotation edits used to start the sidecar debounce WITHOUT
//           marking the session dirty. They now flow through the same
//           session-dirty pipeline as command mutations, and a successful
//           save clears that flag (title/status/prompt/autosave agree).
//   GUI handle — the shared SafeSave commit boundary now coordinates the
//           viewer handle (release before the atomic replacement, restore
//           after), so in-place mutation/save works while the document is
//           displayed.
//   V01   — form data import swaps the REAL path on disk (parked handle,
//           checked SafeSave commit) and leaves the viewer on the real path.
//
// The tests instantiate the REAL MainWindow over the REAL Bootstrapper
// context and REAL generated PDFs offscreen. Deterministic barriers only
// (queued modal drivers, no sleeps). They intentionally avoid referencing
// post-fix-only API so the same binaries build against the pre-fix baseline
// for revert verification (`git stash push -- src/`).
//
// Platform note: run with QT_QPA_PLATFORM=offscreen (as CTest pins). The
// suite forces AA_DontUseNativeDialogs, so the file-dialog driver sees the
// Qt widget dialog on every platform plugin; without it the windows plugin's
// native IFileDialog shim would ignore a post-show selectFile() entirely.
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QUndoStack>
#include <QMessageBox>
#include <QFileDialog>
#include <QAbstractButton>
#include <QPdfWriter>
#include <QPainter>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/ToolId.h"
#include "engines/DocumentSession.h"
#include "engines/FormManager.h"
#include "shell/controllers/FormsController.h"
#include "ui/PdfViewerWidget.h"
#include "ui/AnnotationLayer.h"
#include "commands/RotatePageCommand.h"

using gp::MainWindow;

namespace {

// Portrait A4 one-page fixture with a marker text.
void makePdf(const QString &path)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&w);
    p.drawText(100, 100, QStringLiteral("PERSIST"));
    p.end();
    QVERIFY2(QFileInfo::exists(path), "fixture must exist");
}

void setWritable(const QString &path, bool writable)
{
    QFile::Permissions perms = QFile::ReadOwner | QFile::ReadUser
        | QFile::ReadGroup | QFile::ReadOther;
    if (writable)
        perms |= QFile::WriteOwner | QFile::WriteUser | QFile::WriteGroup | QFile::WriteOther;
    QFile::setPermissions(path, perms);
}

// Deterministic modal drivers: queued zero-timeout turns INSIDE the modal
// dialogs' nested event loops (no sleeps; each retry is one event-loop turn).
// A modal registers as QApplication::activeModalWidget only after the nested
// loop's first turn — and the exact turn DEPENDS ON THE PLATFORM PLUGIN (the
// windows plugin registers the box before the first nested turn; the
// offscreen plugin measurably does not: the original one-shot drivers wedged
// this suite into a 300 s hang under QT_QPA_PLATFORM=offscreen while passing
// on windows). Drivers therefore RETRY until the target state is OBSERVED,
// with a bounded turn budget so a regression wedges into a qWarning instead
// of a hang.

void clickPromptButton(const QString &text, int turnsLeft = 100)
{
    QTimer::singleShot(0, [text, turnsLeft] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            const auto buttons = box->findChildren<QAbstractButton *>();
            for (QAbstractButton *b : buttons) {
                if (b->text() == text) { b->click(); return; }
            }
        }
        if (turnsLeft > 0)
            clickPromptButton(text, turnsLeft - 1);
        else
            qWarning() << "clickPromptButton: no box with a" << text << "button became the active modal";
    });
}

void closeNextModalSoon(int turnsLeft)
{
    QTimer::singleShot(0, [turnsLeft] {
        if (QWidget *w = QApplication::activeModalWidget()) {
            w->close();
            return;
        }
        if (turnsLeft > 0)
            closeNextModalSoon(turnsLeft - 1);
    });
}

// Closes the next modal (the driven flow's own failure box) whenever it shows
// up; the retry budget only bounds how long it waits, so a flow that answers
// later than expected degrades into a qWarning rather than a stale closer.
void dismissNextModal(int turnsLeft = 100)
{
    QTimer::singleShot(0, [turnsLeft] {
        if (QWidget *w = QApplication::activeModalWidget()) {
            w->close();
            return;
        }
        if (turnsLeft > 0)
            dismissNextModal(turnsLeft - 1);
        else
            qWarning() << "dismissNextModal: no modal appeared within the turn budget";
    });
}

void pickFileInNextDialog(const QString &path, int turnsLeft = 100)
{
    QTimer::singleShot(0, [path, turnsLeft] {
        if (auto *dlg = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            // Accept only after the selection is OBSERVED in the dialog —
            // QFileSystemModel populates its directory asynchronously, and an
            // accept fired before selectFile() has actually landed delivers an
            // empty selection (the silent early-return that left the disk
            // stale in the pre-fix flow on the windows platform, whose native
            // dialog shim ignores a post-show selectFile altogether —
            // initTestCase disables native dialogs so every platform uses the
            // widget implementation here).
            const QStringList selected = dlg->selectedFiles();
            const bool delivered = selected.contains(path)
                || (!selected.isEmpty()
                    && QFileInfo(selected.last()).canonicalFilePath() == QFileInfo(path).canonicalFilePath());
            if (!delivered) {
                dlg->selectFile(path);
                if (turnsLeft > 0) { pickFileInNextDialog(path, turnsLeft - 1); return; }
                qWarning() << "pickFileInNextDialog: the selection never appeared for" << path;
                return;
            }
            // QFileDialog::accept() is protected in Qt 6; the metaobject still
            // exposes the QDialog accept() slot.
            QMetaObject::invokeMethod(dlg, "accept");
            // Safety net: if the driven flow answers with a modal failure box,
            // close it (a synchronous box is up within a turn of the accept;
            // the small budget cannot leak stray closes into later modals).
            closeNextModalSoon(3);
            return;
        }
        if (turnsLeft > 0)
            pickFileInNextDialog(path, turnsLeft - 1);
        else
            qWarning() << "pickFileInNextDialog: no QFileDialog became the active modal";
    });
}

} // namespace

class TestPersistenceOutcomes : public QObject {
    Q_OBJECT

    std::unique_ptr<MainWindow> m_win;
    QString m_maybeReadOnly;   // restored in cleanup() so the temp dir can go

    void queueReadonlyRestore()
    {
        QTimer::singleShot(0, [this] { if (!m_maybeReadOnly.isEmpty()) setWritable(m_maybeReadOnly, true); });
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestPersistenceOutcomes"));
        // The file-dialog driver must behave identically on every platform
        // plugin: the windows plugin's native IFileDialog shim ignores a
        // post-show selectFile() (the options are inert once the dialog is
        // up), which turned every driven pick into an empty accept. Forcing
        // the widget implementation makes the dialog a plain QFileDialog —
        // qobject_cast-able, observable through selectedFiles() — everywhere.
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    }

    void init()
    {
        m_win = std::make_unique<MainWindow>(Bootstrapper::createContext());
        m_win->show();   // close()/isVisible() semantics require a shown window
        QVERIFY(m_win->pdfViewer());
        QVERIFY(m_win->appContext()->pdfEditor);
        QVERIFY(m_win->appContext()->document);
        QVERIFY(m_win->appContext()->undoStack);
    }

    void cleanup()
    {
        if (!m_maybeReadOnly.isEmpty()) {
            setWritable(m_maybeReadOnly, true);
            m_maybeReadOnly.clear();
        }
        m_win.reset();
    }

    // ── Engine-lane residual (GUI-held handle) ───────────────────────────────
    // A same-path engine mutation must succeed while the viewer displays the
    // file, and the saved bytes must actually carry the mutation. Pre-fix the
    // commit failed with "Access is denied" and the artifact stayed unchanged.
    void inPlaceMutationSucceedsWhileViewerDisplaysFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a);
        auto *ctx = m_win->appContext();

        m_win->openDocument(a);
        QCOMPARE(ctx->document->path(), a);

        QVERIFY2(ctx->pdfEditor->rotatePage(a, 0, 90),
                 "the in-place commit must coordinate the viewer-held handle");

        // Saved-artifact truth: the displayed file itself carries the rotation.
        PdfViewerWidget probe;
        QVERIFY(probe.loadDocument(a));
        QCOMPARE(probe.pageCount(), 1);
        const QImage page0 = probe.renderPage(0, 1.0);
        QVERIFY(!page0.isNull());
        QVERIFY2(page0.width() > page0.height(),
                 "the on-disk page must render landscape after the in-place rotate");
    }

    // ── ARC03 ────────────────────────────────────────────────────────────────
    // Choosing Save in the close prompt with a FAILING save (read-only
    // destination → the checked commit cannot replace) must NOT close the
    // window: dirty state and history stay available for retry, and the
    // retry (writable again) completes the close only after real persistence.
    void closeAfterFailedSaveKeepsDocumentOpen()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a);
        auto *ctx = m_win->appContext();

        m_win->openDocument(a);
        ctx->undoStack->push(new RotatePageCommand(
            ctx->pdfEditor.get(), ctx->document.get(), 0, 90));
        QVERIFY(ctx->document->isDirty());

        setWritable(a, false);
        m_maybeReadOnly = a;

        clickPromptButton(QStringLiteral("Save"));   // close prompt → Save
        dismissNextModal();                          // the failed-save error box
        const bool closed = m_win->close();

        QVERIFY2(!closed && m_win->isVisible(),
                 "ARC03: a failed save must keep the window and document open");
        QVERIFY2(ctx->document->isDirty(), "ARC03: the unsaved work must stay dirty-truthful");
        QVERIFY2(ctx->undoStack->canUndo(), "ARC03: history must remain available for retry");

        // Retry with a writable destination: the save now really persists and
        // only then does the close proceed.
        setWritable(a, true);
        m_maybeReadOnly.clear();
        clickPromptButton(QStringLiteral("Save"));
        const bool closedAfterRetry = m_win->close();

        QVERIFY2(closedAfterRetry && !m_win->isVisible(),
                 "ARC03: a checked successful save completes the close");
        QVERIFY2(!ctx->document->isDirty(),
                 "ARC04/ARC03: a checked successful save clears the session dirty flag");
    }

    // ── ARC04 ────────────────────────────────────────────────────────────────
    // An annotation edit must mark the session dirty through the same
    // pipeline as command mutations (title, prompts, autosave read that flag).
    void annotationEditMarksSessionDirty()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a);
        auto *ctx = m_win->appContext();

        m_win->openDocument(a);
        QVERIFY2(!ctx->document->isDirty(), "a freshly opened document is clean");

        AnnotationItem note;
        note.pageIndex = 0;
        note.mode = ToolMode::AddComment;
        note.rect = QRectF(10, 10, 60, 20);
        note.text = QStringLiteral("USER_NOTE");
        m_win->pdfViewer()->annotationLayer()->setAnnotations({ note });

        QVERIFY2(ctx->document->isDirty(),
                 "ARC04: an annotation edit must dirty the session like any mutation");
        QVERIFY2(m_win->windowTitle().endsWith(QLatin1String(" *")),
                 "ARC04: the title's unsaved marker must agree with the session flag");
    }

    // Control for the ARC04 relay: (re)loads are NOT user edits — a document
    // opened fresh (with or without a sidecar) must never appear dirty.
    void documentReloadDoesNotDirtyFreshSession()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a);
        {
            QFile ann(a + QStringLiteral(".ann"));
            QVERIFY(ann.open(QIODevice::WriteOnly));
            ann.write("{\"annotations\":[],\"version\":1}");
        }
        auto *ctx = m_win->appContext();

        m_win->openDocument(a);
        QVERIFY2(!ctx->document->isDirty(),
                 "ARC04: opening a document (sidecar load) must not dirty it");

        // Same-path reload after an edit: the fresh revision starts clean.
        ctx->undoStack->push(new RotatePageCommand(
            ctx->pdfEditor.get(), ctx->document.get(), 0, 90));
        QVERIFY(ctx->document->isDirty());
        m_win->openDocument(a);
        QVERIFY2(!ctx->document->isDirty(),
                 "ARC04: the reloaded revision is clean — loads never dirty");
    }

    // ── V01 ──────────────────────────────────────────────────────────────────
    // Form-data import must make the DISK truthful: the real path receives the
    // imported values, the temporary output is gone, and the viewer stays on
    // the real path. Pre-fix the flow deleted the temp and renamed it onto
    // itself, so the import never landed in the real document.
    void formImportSwapsDiskTruthfully()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a);
        auto *ctx = m_win->appContext();

        QVERIFY(ctx->forms->addTextField(a, 0, QRectF(72, 100, 144, 24),
                                         QStringLiteral("Field1"), a));
        QVariantMap seed;
        seed[QStringLiteral("Field1")] = QStringLiteral("ORIGINAL");
        QVERIFY(ctx->forms->fillForm(a, seed, a, /*lockFields=*/false));

        m_win->openDocument(a);

        const QString csv = dir.filePath("data.csv");
        {
            QFile f(csv);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
            f.write("FieldName,FieldValue\n\"Field1\",\"IMPORTED_VALUE\"\n");
        }

        gp::FormsController forms(ctx, m_win.get());
        // The pick driver retries until the file dialog is the active modal and
        // closes any modal failure box afterwards (a pre-queued one-shot
        // dismiss would close the freshly opened dialog with an empty
        // selection — the silent early-return that left the disk stale in
        // earlier runs of this suite).
        pickFileInNextDialog(csv);
        forms.activate(ToolId::ImportData);

        // Disk truth at the REAL path.
        const FormFieldSnapshot snap = ctx->forms->captureFieldSnapshot(a, QStringLiteral("Field1"));
        QVERIFY2(snap.found && snap.value == QLatin1String("IMPORTED_VALUE"),
                 "V01: the imported value must land in the real document on disk");
        QVERIFY2(!QFileInfo::exists(a + QStringLiteral(".tmp")),
                 "V01: the temporary import output must not survive the swap");
        QVERIFY2(m_win->pdfViewer()->filePath() == a,
                 "V01: the viewer must keep the real path (not the temp output)");
        QCOMPARE(m_win->pdfViewer()->pageCount(), 1);
    }
};

QTEST_MAIN(TestPersistenceOutcomes)
#include "TestPersistenceOutcomes.moc"
