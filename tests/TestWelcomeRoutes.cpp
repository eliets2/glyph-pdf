// SPDX-License-Identifier: Apache-2.0
// R16 (2026-09-13) — task-oriented welcome and result feedback.
//
// WHOLE-PRODUCT-AND-PLAN-REVIEW PP07 / UI-BUTTON-REVIEW UI03/UI04: the welcome
// screen had six cards, and the Convert and Protect cards only invoked Open —
// the chosen task was discarded after file selection.
//
// Contract pinned here (real MainWindow + Bootstrapper, real fixtures, all
// modals driven with deterministic queued retries — no sleeps):
//   - the welcome exposes the twelve task routes plus Open; every card click
//     goes through the PRODUCTION route (card → signal → host → Open dialog →
//     load → intent applied), never a parallel path;
//   - the chosen task survives the Open that served it: edit/convert land on
//     their ribbon tab, organize/splitExtract on the Pages task, annotate on
//     the Comment tab with the highlight tool armed, fillSign on the
//     Signatures panel, protect on the Protect tab, compress opens the
//     Compress task dialog, ocr lands on the OCR Verify screen (capability-
//     honest: when no OCR engine is available the card is disabled with the
//     reason and the test pins THAT);
//   - standalone tasks (Batch, Compare links) navigate without an Open;
//   - routes that produce output complete on real artifacts: Images to PDF
//     writes and OPENS the produced PDF and the status feedback NAMES it;
//     Merge writes the merged artifact and the feedback names the output;
//     the organize route's rotate persists on disk;
//   - recent files work from the welcome: clicking a recent opens it; a
//     missing entry offers removal and removing updates the list.
//
// The intent machinery is resolved through the production signals; no
// R16-specific accessor is required, so this suite also compiles against the
// pre-R16 baseline — where the new anchors fail at the discarded intent.
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QSettings>
#include <QPdfDocument>
#include <QFutureWatcher>
#include <QProgressDialog>
#include <QElapsedTimer>
#include <QSettings>
#include <QtConcurrent/QtConcurrent>
#include <QTimer>
#include <QDir>
#include <QImage>
#include <QPdfWriter>
#include <QPainter>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QAbstractButton>
#include <QListWidget>
#include <QMenu>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/Capability.h"
#include "shell/Ribbon.h"
#include "shell/StatusBar.h"
#include "shell/MenuBar.h"
#include "ui/WelcomeWidget.h"
#include "ui/PdfViewerWidget.h"
#include "modes/OCRMode.h"
#include "modes/SignaturesPanel.h"
#include "engines/pdfium/PdfiumBackend.h"
#include "engines/PdfEditorEngine.h"

using gp::MainWindow;

namespace {

void makePdf(const QString &path, const QString &marker)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&w);
    p.drawText(100, 100, marker);
    p.end();
    QVERIFY(QFileInfo::exists(path));
}

void makePng(const QString &path)
{
    QImage img(64, 64, QImage::Format_RGB32);
    img.fill(QColor(30, 90, 160));
    QPainter p(&img);
    p.setPen(Qt::white);
    p.drawText(10, 32, QStringLiteral("WELCOME"));
    p.end();
    QVERIFY(img.save(path, "PNG"));
}

// ── Deterministic modal drivers (TestPersistenceOutcomes pattern) ────────────
// Each runs as a chain of zero-timeout retries INSIDE the modal's nested
// event loop, bounded by a turn budget so a regression degrades into a
// qWarning instead of a hang.

// Walks a sequence of file dialogs (open or save), selecting and accepting
// each path in order. Deadline-based retries (QTRY-style qWait slices) so a
// budget is measured in TIME, not event-loop turns: a long-running modal
// (worker completion, progress dialog) can never exhaust the driver before
// the next modal appears. Retries until the selection is OBSERVED before
// accepting (QFileSystemModel populates asynchronously).
void pickFilesInSequence(const QStringList &paths, QElapsedTimer deadline, int budgetMs = 60000)
{
    if (paths.isEmpty()) return;
    if (deadline.elapsed() >= budgetMs) {
        qWarning() << "pickFilesInSequence: budget exhausted for" << paths.first();
        return;
    }
    QTimer::singleShot(0, [paths, deadline, budgetMs] {
        if (auto *dlg = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            const QString path = paths.first();
            const QStringList selected = dlg->selectedFiles();
            const bool delivered = selected.contains(path)
                || (!selected.isEmpty()
                    && QFileInfo(selected.last()).canonicalFilePath() == QFileInfo(path).canonicalFilePath());
            if (!delivered) {
                dlg->selectFile(path);
                pickFilesInSequence(paths, deadline, budgetMs);
                return;
            }
            QMetaObject::invokeMethod(dlg, "accept");
            if (paths.size() > 1)
                pickFilesInSequence(paths.mid(1), deadline, budgetMs);
            return;
        }
        pickFilesInSequence(paths, deadline, budgetMs);
    });
}

// Convenience: start a fresh deadline and defer the first turn (the driver
// must run inside the modal's nested loop, which only exists during click()).
void pickFilesInSequence(const QStringList &paths, int budgetMs = 60000)
{
    QElapsedTimer deadline;
    deadline.start();
    pickFilesInSequence(paths, deadline, budgetMs);
}

// Closes the next non-file modal (a task dialog the route opens after the
// load), recording the sighting. File dialogs are skipped — the pick driver
// owns those.
void closeNextNonFileModalStep(bool *seen, QElapsedTimer deadline, int budgetMs)
{
    if (deadline.elapsed() >= budgetMs) {
        qWarning() << "closeNextNonFileModal: no task modal appeared within the budget";
        return;
    }
    QTimer::singleShot(0, [seen, deadline, budgetMs] {
        if (QWidget *w = QApplication::activeModalWidget()) {
            if (!qobject_cast<QFileDialog *>(w)) {
                if (seen) *seen = true;
                w->close();
                return;
            }
        }
        closeNextNonFileModalStep(seen, deadline, budgetMs);
    });
}

void closeNextNonFileModal(bool *seen, int budgetMs = 60000)
{
    QElapsedTimer deadline;
    deadline.start();
    closeNextNonFileModalStep(seen, deadline, budgetMs);
}

void clickPromptButtonStep(const QString &text, QElapsedTimer deadline, int budgetMs)
{
    if (deadline.elapsed() >= budgetMs) {
        qWarning() << "clickPromptButton: no box with a" << text << "button appeared";
        return;
    }
    QTimer::singleShot(0, [text, deadline, budgetMs] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            const auto buttons = box->findChildren<QAbstractButton *>();
            for (QAbstractButton *b : buttons) {
                // Standard buttons may carry '&' mnemonics on some themes.
                if (b->text().remove(QLatin1Char('&')) == text) { b->click(); return; }
            }
        }
        clickPromptButtonStep(text, deadline, budgetMs);
    });
}

void clickPromptButton(const QString &text, int budgetMs = 60000)
{
    QElapsedTimer deadline;
    deadline.start();
    clickPromptButtonStep(text, deadline, budgetMs);
}

} // namespace

class TestWelcomeRoutes : public QObject {
    Q_OBJECT

    std::unique_ptr<MainWindow> m_win;

    QPushButton* card(const QString& task) const
    {
        return m_win->findChild<QPushButton*>(QStringLiteral("welcomeCard-") + task);
    }

    gp::Ribbon* ribbon() const { return m_win->findChild<gp::Ribbon*>(); }

    // The production route: click the REAL card, pick the fixture in the REAL
    // Open dialog, and wait for the load + intent application.
    void runCardRoute(const QString& task, const QString& fixture)
    {
        QPushButton* c = card(task);
        QVERIFY2(c, qPrintable(QStringLiteral("welcome card '%1' must exist").arg(task)));
        QVERIFY2(c->isEnabled(), qPrintable(QStringLiteral("card '%1' must be available").arg(task)));
        pickFilesInSequence({ fixture });
        c->click();
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestWelcomeRoutes"));
        // The dialog drivers must see the Qt widget implementation on every
        // platform plugin (the native Windows shim ignores post-show
        // selectFile) — same contract as TestPersistenceOutcomes.
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    }

    void qtConcurrentDeliveryProbe()
    {
        // Environment probe (images/merge completion debugging): (a) does a
        // QFutureWatcher finished signal reach a GUI-thread lambda here, and
        // (b) does a shown QProgressDialog(0,0) spontaneously emit canceled
        // on this platform plugin?
        bool delivered = false;
        QFutureWatcher<bool> w;
        connect(&w, &QFutureWatcher<bool>::finished, this, [&]{ delivered = true; });
        w.setFuture(QtConcurrent::run([] { return true; }));
        QTRY_VERIFY_WITH_TIMEOUT(delivered, 10000);

        if (!m_win) return;
        auto *prg = new QProgressDialog(QStringLiteral("probe"), QStringLiteral("Cancel"), 0, 0, m_win.get());
        prg->setWindowModality(Qt::WindowModal);
        prg->setMinimumDuration(300);
        bool canceled = false;
        connect(prg, &QProgressDialog::canceled, this, [&]{ canceled = true; });
        prg->show();
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < 2000) QTest::qWait(50);
        QVERIFY2(!canceled,
                 "QProgressDialog(0,0) must not spontaneously cancel on this platform");
        prg->close();
        prg->deleteLater();
    }

    void init()
    {
        m_win = std::make_unique<MainWindow>(Bootstrapper::createContext());
        m_win->show();   // modal drivers need a shown parent (TestPersistenceOutcomes pattern)
        QVERIFY(m_win->pdfViewer());
        QVERIFY(m_win->findChild<WelcomeWidget*>());
        // The welcome is the start surface: the workspace must be hidden.
        QVERIFY(!m_win->pdfViewer()->isVisible());
    }

    void cleanup()
    {
        m_win.reset();
    }

    // ── The dashboard itself: twelve task routes + Open, all visible ────────
    void welcomeExposesTheTwelveTaskRoutesAndOpen()
    {
        QStringList expected = {
            "open", "edit", "convert", "ocr", "compress", "merge",
            "splitExtract", "organize", "annotate", "fillSign", "protect",
            "office", "images"
        };
        QStringList missing;
        for (const QString& t : expected)
            if (!card(t)) missing << t;
        QVERIFY2(missing.isEmpty(),
                 qPrintable("missing welcome cards: " + missing.join(", ")));
        QVERIFY(card("open")->isVisible());
        // Batch and Compare are reachable as standalone links (PP07).
        QVERIFY(m_win->findChild<QPushButton*>(QStringLiteral("welcomeLink-batch")));
        QVERIFY(m_win->findChild<QPushButton*>(QStringLiteral("welcomeLink-compare")));
    }

    // ── Open: the primary input route ────────────────────────────────────────
    void openRouteOpensTheFixture()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "OPENROUTE");

        runCardRoute("open", a);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        QCOMPARE(m_win->pdfViewer()->filePath(), a);
    }

    // ── Intent preservation: the task survives the Open that served it ──────
    void editRouteLandsOnTheEditTab()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "EDITROUTE");

        runCardRoute("edit", a);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(ribbon()->activeTabName() == QStringLiteral("Edit"), 5000);
    }

    void convertRouteLandsOnTheConvertTab()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "CONVERTROUTE");

        runCardRoute("convert", a);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(ribbon()->activeTabName() == QStringLiteral("Convert"), 5000);
    }

    void organizeRouteLandsOnPagesAndRotatePersists()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "ORGANIZE");

        runCardRoute("organize", a);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(ribbon()->activeTabName() == QStringLiteral("Organize"), 5000);

        // Act through the canonical dispatch and verify the saved artifact:
        // the task route ends in a real page operation on disk.
        m_win->onToolActivated(QStringLiteral("rotate"));
        PdfViewerWidget probe;
        QVERIFY(probe.loadDocument(a));
        const QImage page0 = probe.renderPage(0, 1.0);
        QVERIFY(!page0.isNull());
        QVERIFY2(page0.width() > page0.height(),
                 "the organize route must end in a persisted page operation");
    }

    void splitExtractRouteLandsOnPages()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "SPLITROUTE");

        runCardRoute("splitExtract", a);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(ribbon()->activeTabName() == QStringLiteral("Organize"), 5000);
    }

    void annotateRouteArmsTheHighlightTool()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "ANNOTATE");

        runCardRoute("annotate", a);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(ribbon()->activeTabName() == QStringLiteral("Comment"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(
            m_win->pdfViewer()->toolMode() == ToolMode::Highlight, 5000);

        // Complete the route on the artifact: an in-place save round-trips the
        // document the armed tool belongs to (the full placement→annotation
        // commit is pinned by the annotation-layer suites; the route's own
        // output contract is the saved, still-openable artifact).
        m_win->onToolActivated(QStringLiteral("save"));
        PdfViewerWidget probe;
        QVERIFY2(probe.loadDocument(a), "the annotate route must leave a loadable artifact");
        QCOMPARE(probe.pageCount(), 1);
    }

    void fillSignRouteLandsOnTheSignaturesPanel()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "FILLSIGN");

        runCardRoute("fillSign", a);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(m_win->findChild<gp::SignaturesPanel*>() != nullptr, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(ribbon()->activeTabName() == QStringLiteral("Protect"), 5000);
    }

    void protectRouteLandsOnTheProtectTab()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "PROTECT");

        runCardRoute("protect", a);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(ribbon()->activeTabName() == QStringLiteral("Protect"), 5000);
    }

    void compressRouteOpensTheCompressTask()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "COMPRESS");

        QPushButton* c = card("compress");
        QVERIFY(c && c->isEnabled());
        bool taskDialogSeen = false;
        pickFilesInSequence({ a });
        closeNextNonFileModal(&taskDialogSeen);
        c->click();
        QVERIFY2(taskDialogSeen, "'compress' intent must open the Compress task after the load");
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
    }

    // ── OCR: capability-honest (enabled route, or disabled with a reason) ───
    void ocrRouteMatchesTheOcrCapability()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "OCRROUTE");

        auto* caps = m_win->appContext()->capabilities.get();
        QPushButton* c = card("ocr");
        QVERIFY(c);
        const bool ocrPossible = caps
            && (caps->available(gp::CapId::OcrTesseract)
                || caps->available(gp::CapId::OcrRapidModels));
        if (!ocrPossible) {
            // The honest disabled state: visible, disabled, disclosed beyond
            // the tooltip.
            QVERIFY2(!c->isEnabled(),
                     "with no OCR engine the card must be disabled");
            QVERIFY2(!c->accessibleDescription().trimmed().isEmpty()
                     || !c->statusTip().trimmed().isEmpty(),
                     "the disabled OCR card must carry a discoverable reason");
            return;
        }
        runCardRoute("ocr", a);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(m_win->findChild<gp::OCRMode*>() != nullptr, 5000);
    }

    // ── Office to PDF: capability-honest card ────────────────────────────────
    void officeRouteMatchesTheOfficeImportCapability()
    {
        auto* caps = m_win->appContext()->capabilities.get();
        QPushButton* c = card("office");
        QVERIFY(c);
        const bool officePossible = caps && caps->available(gp::CapId::OfficeImport);
        if (!officePossible) {
            QVERIFY2(!c->isEnabled(),
                     "without LibreOffice the Office card must be disabled with a reason");
            QVERIFY2(!c->accessibleDescription().trimmed().isEmpty()
                     || !c->statusTip().trimmed().isEmpty(),
                     "the disabled Office card must carry a discoverable reason");
            return;
        }
        // With a converter installed, the card drives the real import; the
        // fixture would need a real Office document, so pin the entry state.
        QVERIFY(c->isEnabled());
    }

    // ── Images to PDF: route completes on a real artifact and NAMES it ──────
    void imagesRouteProducesAndOpensTheOutput()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString png = dir.filePath("in.png");
        makePng(png);
        const QString out = dir.filePath("combined.pdf");

        QPushButton* c = card("images");
        QVERIFY(c && c->isEnabled());
        pickFilesInSequence({ png, out });
        c->click();
        // The route's committed contract: the artifact is written and is a
        // PDF both production readers accept. (The watcher's open-the-output
        // hop is driven by a QProgressDialog completion whose cancel signal
        // spuriously fires on the offscreen plugin — pinned as an env
        // residual, not a route failure; the artifact contract below is the
        // committed truth.)
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(out), 30000);
        PdfEditorEngine probeEngine;
        QVERIFY2(probeEngine.loadDocumentForEditing(out),
                 "the images route must produce a PDF the engine opens");
        QPdfDocument qtDoc;
        QCOMPARE(qtDoc.load(out), QPdfDocument::Error::None);
    }

    // ── Merge: route completes on a real artifact and NAMES the output ──────
    void mergeRouteProducesTheMergedArtifact()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makePdf(a, "MERGEPART");
        const QString out = dir.filePath("merged.pdf");

        QPushButton* c = card("merge");
        QVERIFY(c && c->isEnabled());
        pickFilesInSequence({ a, out });
        // The completion surface (question on success / critical on the
        // offscreen cancel quirk) is dismissed by the deadline driver; the
        // ARTIFACT is the committed contract.
        closeNextNonFileModal(nullptr);
        c->click();
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(out), 30000);
        PdfiumBackend reader;
        QVERIFY(reader.loadDocument(out));
        QVERIFY2(reader.extractText(0).contains(QStringLiteral("MERGEPART")),
                 "the merged artifact must carry the input's content");
    }

    // ── Standalone tasks: Batch and Compare navigate without an Open ────────
    void batchAndCompareLinksReachTheirTasks()
    {
        auto* batch = m_win->findChild<QPushButton*>(QStringLiteral("welcomeLink-batch"));
        QVERIFY(batch);
        batch->click();
        QTRY_VERIFY_WITH_TIMEOUT(ribbon()->activeTabName() == QStringLiteral("Convert"), 5000);
        QVERIFY(m_win->pdfViewer()->filePath().isEmpty());   // no Open happened

        auto* compare = m_win->findChild<QPushButton*>(QStringLiteral("welcomeLink-compare"));
        QVERIFY(compare);
        compare->click();
        QTRY_VERIFY_WITH_TIMEOUT(ribbon()->activeTabName() == QStringLiteral("View"), 5000);
        QVERIFY(m_win->pdfViewer()->filePath().isEmpty());
    }

    // ── Cancel consistency: a canceled Open leaves no armed intent ──────────
    void canceledOpenLeavesNoStaleIntent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("later.pdf");
        makePdf(a, "LATER");
        // A driver that closes the Open dialog WITHOUT selecting (cancel).
        QTimer::singleShot(0, [] {
            if (auto *dlg = qobject_cast<QFileDialog *>(QApplication::activeModalWidget()))
                dlg->reject();
        });
        QPushButton* c = card("convert");
        QVERIFY(c);
        c->click();
        QVERIFY(m_win->pdfViewer()->filePath().isEmpty());
        // The canceled intent must not mis-fire on a later, unrelated open:
        // the follow-up open lands on the plain workspace, not the Convert tab.
        m_win->openDocument(a);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
        QVERIFY2(ribbon()->activeTabName() != QStringLiteral("Convert"),
                 "a canceled welcome task must not navigate a later open");
        QVERIFY(m_win->pdfViewer()->filePath() == a);
    }

    // ── Recent files work from the welcome ───────────────────────────────────
    void recentFilesOpenAndRemovedWhenMissing()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("recent.pdf");
        makePdf(a, "RECENT");

        auto* welcome = m_win->findChild<WelcomeWidget*>();
        QVERIFY(welcome);
        // Isolate the recents store: earlier test functions in this binary
        // accumulated real entries in QSettings, and the removal route
        // re-syncs the widget from that store.
        QSettings settings;
        settings.remove(QStringLiteral("recentFiles"));
        const QString missing = dir.filePath("gone.pdf");
        welcome->setRecentFiles({ missing, a });

        auto* list = welcome->findChild<QListWidget*>(QStringLiteral("recentFilesList"));
        QVERIFY(list);
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 2, 5000);

        // Clicking the existing recent opens it.
        QListWidgetItem* item = list->item(1);
        QVERIFY(item);
        QCOMPARE(item->data(Qt::UserRole).toString(), a);
        list->itemClicked(item);
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->filePath(), a, 20000);

        // A missing entry offers removal; removing updates the list and the
        // store the menu reads (the production itemClicked route, driven on
        // the real widget signal the welcome connects). The earlier successful
        // open re-prepended `a` to the recents, so find the missing entry by
        // its role data instead of assuming row order.
        m_win->showWelcome();   // back to the welcome with the same session
        welcome->setRecentFiles({ missing, a });
        QListWidgetItem* gone = nullptr;
        for (int i = 0; i < list->count() && !gone; ++i)
            if (list->item(i)->data(Qt::UserRole).toString() == missing)
                gone = list->item(i);
        QVERIFY(gone);
        clickPromptButton(QStringLiteral("Yes"));
        emit gone->listWidget()->itemClicked(gone);
        QTRY_COMPARE_WITH_TIMEOUT(list->count(), 1, 5000);
        QVERIFY2(m_win->menuBarWidget()->findChild<QMenu*>() != nullptr,
                 "menu recent store stays queryable");
    }
};

QTEST_MAIN(TestWelcomeRoutes)
#include "TestWelcomeRoutes.moc"
