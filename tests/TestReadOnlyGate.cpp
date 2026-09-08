// SPDX-License-Identifier: Apache-2.0
// Wave 4A (2026-09-09): ARC07 — read-only is enforced at the shared mutation
// boundary, not only for tool selection.
//
// TEAM-ARCHITECTURE-REVIEW-2026-09-07 ARC07: setReadOnly() blocked SELECTING
// editing tool modes (cursor/tool-mode gate) while controller actions that
// directly push commands or call engines still mutated the PDF — the app's
// "opened in read-only mode" promise was false for page actions and
// save-in-place.
//
// Contract pinned here (all on real saved/reopened artifacts):
//   - read-only is ARMED through the REAL production entry (the §9.11 expired-
//     document open), so the same test drives pre-fix and post-fix builds;
//   - registry-dispatched page mutations (rotate/delete/insert) are refused,
//     push nothing on the undo stack and change nothing on disk;
//   - save-in-place is refused with an explanation (Save As stays available
//     by policy);
//   - re-enabling editing restores exactly the actions that are otherwise
//     eligible — the next rotate genuinely lands on disk (the re-enable call
//     is runtime-resolved off DocumentSession so this suite still COMPILES
//     against the pre-fix baseline for revert verification);
//   - viewing/selection stays available in read-only, and the viewer's own
//     tool gate keeps working (defense in depth, now not the ONLY gate).
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QTimer>
#include <QPdfWriter>
#include <QPainter>
#include <QUndoStack>

#include "GpMainWindow.h"
#include "shell/StatusBar.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/PdfEnums.h"
#include "engines/DocumentSession.h"
#include "engines/PdfEditorEngine.h"
#include "ui/PdfViewerWidget.h"

using gp::MainWindow;

namespace {

// N-page portrait A4 fixture, one marker text per page.
void makeMultiPagePdf(const QString &path, const QStringList &markers)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&w);
    for (int i = 0; i < markers.size(); ++i) {
        if (i > 0) w.newPage();
        p.drawText(100, 100, markers.at(i));
    }
    p.end();
    QVERIFY(QFileInfo::exists(path));
}

// Arm the REAL production read-only state: write an expired §9.11 expiry date
// into a fresh copy via a throw-away engine (never the resident editor), so
// the subsequent openDocument() takes the app's only pre-existing read-only
// entry — the expired-document branch at the open boundary.
void makeExpiredCopy(const QString &src, const QString &dest)
{
    // Normalize the fixture through the real engine first (the same state any
    // in-app mutated document has): the expiry round-trip is reliable on
    // engine-written files, where the raw QPdfWriter output's pre-existing
    // XMP is not (observed: setExpiryDate succeeds but the date does not read
    // back — engine writer nuance, noted for the engine lane).
    PdfEditorEngine writer;
    const QString normalized = dest + QStringLiteral(".norm.pdf");
    QVERIFY(writer.loadDocumentForEditing(src));
    QVERIFY(writer.saveDocument(normalized));
    QVERIFY2(writer.setExpiryDate(normalized, QDate::currentDate().addDays(-1), dest),
             "setExpiryDate must produce the expired fixture copy");
    QVERIFY(QFileInfo::exists(dest));
    QVERIFY(PdfEditorEngine::readExpiryDate(dest).isValid());
}

// Deterministically dismiss the "Document Expired" modal that the open
// boundary shows for an expired document (zero-timeout timer fires inside the
// modal's nested event loop; standard-button boxes close cleanly offscreen).
void scheduleModalDismiss()
{
    QTimer::singleShot(0, [] {
        if (QWidget *w = QApplication::activeModalWidget())
            w->close();
    });
}

struct PageGeometry { int count = 0; bool landscape0 = false; };

// Re-open the SAVED artifact in a fresh probe viewer — on-disk truth, not the
// live widget's belief.
PageGeometry diskGeometry(const QString &path)
{
    PdfViewerWidget probe;
    PageGeometry g;
    if (!probe.loadDocument(path)) return g;
    g.count = probe.pageCount();
    const QImage page0 = probe.renderPage(0, 1.0);
    if (!page0.isNull())
        g.landscape0 = page0.width() > page0.height();
    return g;
}

} // namespace

class TestReadOnlyGate : public QObject {
    Q_OBJECT

    std::unique_ptr<MainWindow> m_win;

private slots:
    void initTestCase()
    {
        // Isolate QSettings (autosave interval, recents, updater).
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestReadOnlyGate"));
    }

    void init()
    {
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

    // ── ARC07 core: page mutations + save-in-place obey the read-only ──
    // policy; re-enabling restores them. Every state assertion is checked
    // against the saved file, not just widget state.
    void readOnlyBlocksPageMutationsAndSaveInPlaceAndReenablingRestoresThem()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makeMultiPagePdf(a, { "P1", "P2" });
        auto *ctx = m_win->appContext();
        auto *viewer = m_win->pdfViewer();

        // Baseline (no read-only): the registry-driven rotate genuinely lands
        // in the displayed file on disk (step-3 handle coordination).
        m_win->openDocument(a);
        QCOMPARE(viewer->pageCount(), 2);
        m_win->onToolActivated(QStringLiteral("rotate"));
        {
            const PageGeometry g = diskGeometry(a);
            QCOMPARE(g.count, 2);
            QVERIFY2(g.landscape0, "baseline: the first rotate must persist 90° (landscape)");
        }
        QCOMPARE(ctx->undoStack->count(), 1);

        // ── arm read-only through the production expiry entry ──────────────
        const QString expired = dir.filePath("expired.pdf");
        makeExpiredCopy(a, expired);
        scheduleModalDismiss();
        m_win->openDocument(expired);
        QVERIFY2(viewer->isReadOnly(),
                 "ARC07: the expired open must arm read-only on the viewer");
        // A new document identity: history was reset at the open boundary.
        QCOMPARE(ctx->undoStack->count(), 0);

        // Rotate: refused at the shared dispatch boundary — no command pushed,
        // no disk change (pre-fix this ran and persisted 180° portrait).
        m_win->onToolActivated(QStringLiteral("rotate"));
        QCOMPARE(ctx->undoStack->count(), 0);
        {
            const PageGeometry g = diskGeometry(expired);
            QCOMPARE(g.count, 2);
            QVERIFY2(g.landscape0,
                     "ARC07: read-only must refuse the second rotate (still 90°, not 180°)");
        }

        // Delete page: refused — the document keeps both pages on disk
        // (pre-fix this deleted the current page).
        m_win->onToolActivated(QStringLiteral("deletePage"));
        {
            const PageGeometry g = diskGeometry(expired);
            QCOMPARE(g.count, 2);
        }

        // Save-in-place: refused with a status explanation (pre-fix this ran
        // the in-place save and reported "Document saved").
        m_win->onToolActivated(QStringLiteral("save"));
        QVERIFY2(m_win->statusBar()->currentMessage().contains(QStringLiteral("read-only")),
                 qPrintable(QStringLiteral("ARC07: save-in-place refusal must be explained; got: ")
                            + m_win->statusBar()->currentMessage()));

        // ── re-enable: the same actions are eligible again ──────────────────
        // (Runtime-resolved so the suite compiles against the pre-fix
        // baseline; on pre-fix builds the invoke fails right here.)
        QVERIFY2(QMetaObject::invokeMethod(ctx->document.get(), "setReadOnly", Q_ARG(bool, false)),
                 "DocumentSession::setReadOnly(bool) must be invokable");
        QTRY_VERIFY_WITH_TIMEOUT(!viewer->isReadOnly(), 5000);
        m_win->onToolActivated(QStringLiteral("rotate"));
        {
            const PageGeometry g = diskGeometry(expired);
            QCOMPARE(g.count, 2);
            QVERIFY2(!g.landscape0,
                     "ARC07: after re-enabling, rotate must persist again (90°→180° portrait)");
        }
        QCOMPARE(ctx->undoStack->count(), 1);
    }

    // Insertion and annotation tooling are refused while viewing/selection
    // stay available (the reviewed "keep viewing, selection and permitted
    // copy/export actions available" half of the contract).
    void readOnlyBlocksInsertAndAnnotationToolsWhileViewingStaysAvailable()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makeMultiPagePdf(a, { "P1", "P2" });
        auto *viewer = m_win->pdfViewer();

        const QString expired = dir.filePath("expired.pdf");
        makeExpiredCopy(a, expired);
        scheduleModalDismiss();
        m_win->openDocument(expired);
        QVERIFY(viewer->isReadOnly());
        QCOMPARE(viewer->pageCount(), 2);

        // Insert page: refused on disk (pre-fix: a blank page was inserted).
        m_win->onToolActivated(QStringLiteral("insertPage"));
        const PageGeometry g = diskGeometry(expired);
        QCOMPARE(g.count, 2);

        // Annotation placement: refused at the registry AND still disarmed at
        // the viewer's own tool gate (defense in depth — the reviewed finding
        // is that the viewer gate alone was the ONLY gate).
        m_win->onToolActivated(QStringLiteral("highlight"));
        QCOMPARE(viewer->toolMode(), ToolMode::HandTool);
        viewer->setToolMode(ToolMode::Highlight);
        QCOMPARE(viewer->toolMode(), ToolMode::HandTool);

        // Viewing/selection stays available in read-only (display state only,
        // nothing persisted).
        m_win->onToolActivated(QStringLiteral("select"));
        QCOMPARE(viewer->toolMode(), ToolMode::SelectText);
        viewer->goToPage(1);
    }

    // Control: opening a NON-expired document resets read-only (the open
    // boundary publishes a fresh, editable session). Passes pre- and post-fix
    // through different mechanisms (viewer branch vs session authority) — it
    // pins that the session-based wiring preserves the old observable outcome.
    void openBoundaryResetsReadOnlyForNonExpiredDocument()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        const QString b = dir.filePath("b.pdf");
        makeMultiPagePdf(a, { "A1" });
        makeMultiPagePdf(b, { "B1" });

        m_win->openDocument(a);
        m_win->pdfViewer()->setReadOnly(true);
        QVERIFY(m_win->pdfViewer()->isReadOnly());

        m_win->openDocument(b);
        QVERIFY2(!m_win->pdfViewer()->isReadOnly(),
                 "ARC07: a fresh non-expired open must not stay read-only");
    }
};

QTEST_MAIN(TestReadOnlyGate)
#include "TestReadOnlyGate.moc"
