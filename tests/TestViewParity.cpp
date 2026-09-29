// SPDX-License-Identifier: Apache-2.0
// TestViewParity: characterization suite for every VIEW capability of the
// pre-redesign UI.
//
// Why it exists: the owner's requirement for the UI redesign (2026-09-27) is
// "no regression, notably the multiple types of views on the original UI".
// See 06-MERGE-PLAN-FINAL.md §4.14.
//
// How to use it during the redesign:
//   * It was written against the OLD shell (MenuBar / Ribbon / ModeStrip /
//     ScreenNav / Sidebar) and passes there. That run is the baseline.
//   * The assertions describe user-visible behaviour through stable seams:
//     the command ids (ToolId), the viewer's public state, the app-wide theme
//     and layout direction, QSettings, the window state and the task registry
//     (TaskNav).
//   * ONLY the functions in the "SHELL SEAM" block below may change when the
//     new shell (CommandRegistry, document bar, rails, panes) replaces the old
//     one. The assertions in the test slots must NOT change. If one has to,
//     that is a behaviour change: justify it in the phase handoff.
//   * Two current behaviours are documented but deliberately NOT pinned,
//     because the redesign should improve them:
//       - leaving Full Screen always lands in Continuous and maximizes the
//         window, instead of restoring the previous layout and window state;
//       - Presentation leaves automatically after the last page.
//   * One current DEFECT was recorded with QEXPECT_FAIL
//     (zoomInAfterFitLeavesFitMode). FIXED (PROGRAM-CONSOLIDATION-2026-09-25
//     §1.4, 06 §4.8): an explicit zoom now leaves the fit mode, the QEXPECT_FAIL
//     marker was removed and the assertion is a permanent regression lock.
#include <QtTest/QtTest>
#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPdfView>
#include <QPdfWriter>
#include <QScrollArea>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/ToolId.h"
#include "modes/AIChatPanel.h"
#include "modes/ModeController.h"
#include "shell/RibbonModel.h"
#include "shell/Sidebar.h"
#include "shell/TaskNav.h"
#include "shell/ToolRegistry.h"
#include "core/interfaces/IToolController.h"
#include "ui/PdfViewerWidget.h"
#include "util/GpTheme.h"

using gp::MainWindow;

namespace {

void makeMultiPagePdf(const QString &path, int pages)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&w);
    for (int i = 0; i < pages; ++i) {
        if (i > 0) w.newPage();
        p.drawText(100, 100, QStringLiteral("VIEW-PARITY PAGE %1").arg(i + 1));
    }
    p.end();
}

// Record whether a modal dialog appears inside the current nested event loop,
// and close it. Bounded retry budget; no sleeps (same pattern as
// TestCommandBinding).
void observeNextModal(bool *seen, int turnsLeft = 200)
{
    QTimer::singleShot(0, [seen, turnsLeft] {
        if (QWidget *w = QApplication::activeModalWidget()) {
            if (seen) *seen = true;
            w->close();
            return;
        }
        if (turnsLeft > 0) observeNextModal(seen, turnsLeft - 1);
    });
}

} // namespace

class TestViewParity : public QObject {
    Q_OBJECT

    std::unique_ptr<MainWindow> m_win;
    QTemporaryDir m_dir;
    QString m_pdf;
    gp::Theme::Mode m_initialTheme = gp::Theme::Dark;
    Qt::LayoutDirection m_initialDirection = Qt::LeftToRight;

    // ======================= SHELL SEAM ===================================
    // The ONLY code in this file that may change when the new shell lands.
    // Each helper names the user-level concept it observes. Keep the concept
    // and re-implement the access.

    /// Run a command exactly as the ribbon, menus and ScreenNav do. Today that
    /// is MainWindow::onToolActivated: U02 task-entry routing (OCR / Compare /
    /// Compress / Watermark open their task) plus the visible-state sync. It is
    /// NOT ToolRegistry::activate, which skips the routing. The redesign:
    /// CommandRegistry dispatch with the same ids and the same routing rule.
    void trigger(ToolId id) { m_win->onToolActivated(toolIdToString(id)); QCoreApplication::processEvents(); }

    /// Navigate to a task screen by its FROZEN TaskNav id (today:
    /// MainWindow::activateScreen, as the ScreenNav buttons do).
    void enterTask(const QString &id) { m_win->activateScreen(id); QCoreApplication::processEvents(); }

    /// Which task screen the center shows ("" = the standard reading canvas).
    QString activeTask() const
    {
        auto *modes = m_win->findChild<gp::ModeController *>();
        return modes ? modes->currentScreen() : QStringLiteral("<no-mode-controller>");
    }

    /// The visible pane on a side ("left" hosts pages/bookmarks/comments,
    /// "right" hosts layers in the old shell). The redesign moves panes to
    /// rails, so re-implement this as "which navigation or task pane view
    /// is showing".
    QString activePane(const char *side) const
    {
        auto *pane = m_win->findChild<gp::Sidebar *>(QLatin1String(side) + QLatin1String("Sidebar"));
        if (!pane) return QStringLiteral("<no-sidebar>");
        QString out;
        QMetaObject::invokeMethod(pane, "activePane", Q_RETURN_ARG(QString, out));
        return out;
    }

    /// Is the regular chrome (command surfaces and side panes) on screen?
    bool chromeVisible() const
    {
        auto *left = m_win->findChild<gp::Sidebar *>(QStringLiteral("leftSidebar"));
        return left && left->isVisible();
    }

    /// The auto-advance timer of Presentation mode (today: ViewController's
    /// QTimer child).
    QTimer *presentationTimer() const
    {
        auto *ctrl = m_win->toolRegistry()->controllerFor(ToolId::Presentation);
        auto *obj = dynamic_cast<QObject *>(ctrl);
        return obj ? obj->findChild<QTimer *>() : nullptr;
    }

    /// The honest disclosure of a planned, not-yet-built command (today:
    /// RibbonModel's planned table; the redesign: commands.json `off`).
    QPair<QString, QString> plannedDisclosure(const QString &id) const
    {
        const auto *spec = gp::RibbonModel::plannedSpecFor(id);
        return spec ? qMakePair(spec->reason, spec->alternative) : qMakePair(QString(), QString());
    }
    // ===================== END SHELL SEAM =================================

    PdfViewerWidget *viewer() const { return m_win->pdfViewer(); }
    QPdfView *pageView() const { return viewer()->findChild<QPdfView *>(QStringLiteral("pdfView")); }
    QScrollArea *spreadView() const { return viewer()->findChild<QScrollArea *>(QStringLiteral("twoPageScrollArea")); }

    void openSample()
    {
        m_win->openDocument(m_pdf);
        QTRY_VERIFY_WITH_TIMEOUT(viewer()->isLoaded(), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(viewer()->pageCount(), 5, 10000);
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestViewParity"));
        QVERIFY(m_dir.isValid());
        m_pdf = m_dir.filePath(QStringLiteral("parity.pdf"));
        makeMultiPagePdf(m_pdf, 5);
        QVERIFY(QFileInfo::exists(m_pdf));
        m_initialTheme = gp::Theme::current();
        m_initialDirection = QApplication::layoutDirection();
    }

    void init()
    {
        QSettings().remove(QStringLiteral("ui/rtl"));
        m_win = std::make_unique<MainWindow>(Bootstrapper::createContext());
        m_win->resize(1600, 1000);
        m_win->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_win.get()));
        QVERIFY(viewer());
    }

    void cleanup()
    {
        m_win.reset();
        gp::Theme::setMode(m_initialTheme);
        QApplication::setLayoutDirection(m_initialDirection);
        QSettings().remove(QStringLiteral("ui/rtl"));
    }

    // ── Zoom set: Zoom In / Zoom Out / Actual Size / Fit Width / Fit Page ──
    void zoomCommands()
    {
        openSample();
        trigger(ToolId::ActualSize);
        QVERIFY(qFuzzyCompare(viewer()->zoomLevel(), 1.0));
        QCOMPARE(pageView()->zoomMode(), QPdfView::ZoomMode::Custom);

        trigger(ToolId::ZoomIn);
        const qreal zoomedIn = viewer()->zoomLevel();
        QVERIFY2(zoomedIn > 1.0, "Zoom In must enlarge from 100 %");
        QCOMPARE(pageView()->zoomFactor(), zoomedIn);

        trigger(ToolId::ZoomOut);
        QVERIFY2(viewer()->zoomLevel() < zoomedIn, "Zoom Out must shrink");

        trigger(ToolId::FitWidth);
        QCOMPARE(pageView()->zoomMode(), QPdfView::ZoomMode::FitToWidth);
        trigger(ToolId::FitPage);
        QCOMPARE(pageView()->zoomMode(), QPdfView::ZoomMode::FitInView);

        trigger(ToolId::ActualSize);   // leaving a fit mode through a manual zoom
        QCOMPARE(pageView()->zoomMode(), QPdfView::ZoomMode::Custom);
        QVERIFY(qFuzzyCompare(viewer()->zoomLevel(), 1.0));
    }

    // Was a known DEFECT at the redesign base: Zoom In / Zoom Out after Fit
    // Width or Fit Page changed the stored factor but left QPdfView in the fit
    // mode, so the page did not visibly zoom. FIXED per 06 §4.8 ("any manual
    // zoom switches to Fixed") — the QEXPECT_FAIL was removed when
    // PROGRAM-CONSOLIDATION-2026-09-25 §1.4 landed; this is the regression
    // lock now.
    void zoomInAfterFitLeavesFitMode()
    {
        openSample();
        trigger(ToolId::FitWidth);
        QCOMPARE(pageView()->zoomMode(), QPdfView::ZoomMode::FitToWidth);
        trigger(ToolId::ZoomIn);
        QCOMPARE(pageView()->zoomMode(), QPdfView::ZoomMode::Custom);
    }

    // ── Page layouts: Single Page / Continuous / Two-Page ──
    void pageLayoutCommands()
    {
        openSample();
        trigger(ToolId::SinglePage);
        QCOMPARE(pageView()->pageMode(), QPdfView::PageMode::SinglePage);
        QVERIFY(pageView()->isVisible());
        QVERIFY(!spreadView()->isVisible());

        trigger(ToolId::Continuous);
        QCOMPARE(pageView()->pageMode(), QPdfView::PageMode::MultiPage);
        QVERIFY(pageView()->isVisible());

        trigger(ToolId::TwoPage);
        QVERIFY2(spreadView()->isVisible(), "Two-Page shows the side-by-side spread");
        QVERIFY(!pageView()->isVisible());
        auto *left = viewer()->findChild<QLabel *>(QStringLiteral("twoPageLeftLabel"));
        auto *right = viewer()->findChild<QLabel *>(QStringLiteral("twoPageRightLabel"));
        QVERIFY(left && right);
        QTRY_VERIFY_WITH_TIMEOUT(!left->pixmap().isNull() && !right->pixmap().isNull(), 5000);

        trigger(ToolId::SinglePage);   // any layout command leaves the spread
        QVERIFY(!spreadView()->isVisible());
        QCOMPARE(pageView()->pageMode(), QPdfView::PageMode::SinglePage);
    }

    // ── Full Screen: enters and exits; chrome hides and comes back ──
    void fullScreenEntersAndExits()
    {
        openSample();
        QVERIFY(chromeVisible());
        trigger(ToolId::Fullscreen);
        QTRY_VERIFY_WITH_TIMEOUT(m_win->isFullScreen(), 3000);
        QVERIFY2(!chromeVisible(), "Full Screen hides the command and pane chrome");
        QCOMPARE(pageView()->pageMode(), QPdfView::PageMode::SinglePage);
        QCOMPARE(pageView()->zoomMode(), QPdfView::ZoomMode::FitInView);

        trigger(ToolId::Fullscreen);
        QTRY_VERIFY_WITH_TIMEOUT(!m_win->isFullScreen(), 3000);
        QVERIFY2(chromeVisible(), "leaving Full Screen restores the chrome");
        // Documented, deliberately NOT pinned: today leaving Full Screen always
        // lands in Continuous (MultiPage) and maximizes the window. The
        // redesign should restore the previous layout and window state.
        QVERIFY(pageView()->isVisible() || spreadView()->isVisible());
    }

    // ── Presentation: full screen, single page, auto-advance, keys, Esc ──
    void presentationModeAdvancesAndExits()
    {
        openSample();
        viewer()->goToPage(0);
        trigger(ToolId::Presentation);
        QTRY_VERIFY_WITH_TIMEOUT(m_win->isFullScreen(), 3000);
        QCOMPARE(pageView()->pageMode(), QPdfView::PageMode::SinglePage);

        QTimer *timer = presentationTimer();
        QVERIFY2(timer && timer->isActive(), "Presentation auto-advances on a timer");
        QCOMPARE(timer->interval(), 5000);

        // Keyboard: Right = next, Left = previous (handled app-wide).
        const int start = viewer()->currentPage();
        QTest::keyClick(m_win.get(), Qt::Key_Right);
        QTRY_COMPARE_WITH_TIMEOUT(viewer()->currentPage(), start + 1, 3000);
        QTest::keyClick(m_win.get(), Qt::Key_Left);
        QTRY_COMPARE_WITH_TIMEOUT(viewer()->currentPage(), start, 3000);

        // Auto-advance, compressed in time: the same timer, a shorter interval.
        timer->setInterval(20);
        QTRY_VERIFY_WITH_TIMEOUT(viewer()->currentPage() > start, 3000);
        timer->setInterval(5000);

        // Esc leaves Presentation and Full Screen.
        QTest::keyClick(m_win.get(), Qt::Key_Escape);
        QTRY_VERIFY_WITH_TIMEOUT(!m_win->isFullScreen(), 3000);
        QVERIFY(!timer->isActive());
        QVERIFY(chromeVisible());
        // Documented, deliberately NOT pinned: Presentation also leaves by
        // itself after the last page.
    }

    // ── Theme: the View "Dark Mode" command cycles Dark → Light → HC → Dark ──
    void themeCommandCyclesAllThemes()
    {
        openSample();
        gp::Theme::setMode(gp::Theme::Dark);
        const QList<gp::Theme::Mode> expected{ gp::Theme::Light, gp::Theme::HighContrast, gp::Theme::Dark };
        QString previousSheet = qApp->styleSheet();
        for (const auto mode : expected) {
            trigger(ToolId::DarkMode);
            QCOMPARE(gp::Theme::current(), mode);
            QVERIFY2(!qApp->styleSheet().isEmpty(), "every theme applies a stylesheet");
            QVERIFY2(qApp->styleSheet() != previousSheet, "each theme applies a different stylesheet");
            previousSheet = qApp->styleSheet();
        }
    }

    void themeCommandWorksWithoutADocument()
    {
        QVERIFY(!viewer()->isLoaded());
        const auto before = gp::Theme::current();
        trigger(ToolId::DarkMode);
        QVERIFY2(gp::Theme::current() != before, "the theme command must work with no document open");
    }

    // ── Reading filters through the commands: Eye Care and Night Mode are exclusive ──
    void readingFiltersThroughCommandsAreExclusive()
    {
        openSample();
        QVERIFY(!viewer()->isEyeCareMode());
        QVERIFY(!viewer()->isNightMode());

        trigger(ToolId::EyeCare);
        QVERIFY(viewer()->isEyeCareMode());
        trigger(ToolId::NightMode);
        QVERIFY(viewer()->isNightMode());
        QVERIFY2(!viewer()->isEyeCareMode(), "Night Mode turns Eye Care off");
        trigger(ToolId::EyeCare);
        QVERIFY(viewer()->isEyeCareMode());
        QVERIFY2(!viewer()->isNightMode(), "Eye Care turns Night Mode off");
        trigger(ToolId::EyeCare);
        QVERIFY(!viewer()->isEyeCareMode());
        QVERIFY(!viewer()->isNightMode());
    }

    // ── RTL: app-wide layout direction, persisted, works without a document ──
    void rtlCommandFlipsAndPersistsLayoutDirection()
    {
        QApplication::setLayoutDirection(Qt::LeftToRight);
        trigger(ToolId::RTL);
        QCOMPARE(QApplication::layoutDirection(), Qt::RightToLeft);
        QCOMPARE(m_win->layoutDirection(), Qt::RightToLeft);
        QVERIFY2(QSettings().value(QStringLiteral("ui/rtl")).toBool(), "the choice persists (QSettings ui/rtl)");
        trigger(ToolId::RTL);
        QCOMPARE(QApplication::layoutDirection(), Qt::LeftToRight);
        QVERIFY(!QSettings().value(QStringLiteral("ui/rtl")).toBool());
    }

    // ── Panes: Thumbnails / Bookmarks / Comments / Layers ──
    void paneCommandsOpenTheirPanes()
    {
        openSample();
        trigger(ToolId::PaneBookmarks);
        QCOMPARE(activePane("left"), QStringLiteral("bookmarks"));
        trigger(ToolId::PaneComments);
        QCOMPARE(activePane("left"), QStringLiteral("comments"));
        trigger(ToolId::PanePages);
        QCOMPARE(activePane("left"), QStringLiteral("pages"));
        trigger(ToolId::PaneLayers);
        QCOMPARE(activePane("right"), QStringLiteral("layers"));
    }

    // ── View ▸ Window: Compare works; Split and New window are honest planned entries ──
    void windowGroupCompareRoutesAndPlannedEntriesExplainThemselves()
    {
        openSample();
        trigger(ToolId::Compare);
        QCOMPARE(activeTask(), QStringLiteral("compare"));
        enterTask(QString());
        QCOMPARE(activeTask(), QString());

        for (const QString &id : { QStringLiteral("splitWin"), QStringLiteral("newWin") }) {
            const auto disclosure = plannedDisclosure(id);
            QVERIFY2(!disclosure.first.isEmpty(), qPrintable(id + QStringLiteral(": a planned view command must say why it is unavailable")));
            QVERIFY2(!disclosure.second.isEmpty(), qPrintable(id + QStringLiteral(": ... and offer a supported alternative")));
        }
    }

    // ── The 13 task screens of the old navigation: each one is reachable ──
    void everyTaskScreenIsReachable()
    {
        openSample();
        int checked = 0;
        for (const gp::TaskSpec &task : gp::TaskNav::tasks()) {
            const QString id = QString::fromLatin1(task.id);
            if (task.kind == gp::TaskKind::Standard) continue;
            ++checked;
            switch (task.kind) {
            case gp::TaskKind::Workspace:
            case gp::TaskKind::Panel:
                enterTask(id);
                QVERIFY2(activeTask() == id, qPrintable(QStringLiteral("task screen not reached: ") + id));
                enterTask(QString());
                QCOMPARE(activeTask(), QString());
                break;
            case gp::TaskKind::Dialog: {
                bool seen = false;
                observeNextModal(&seen);
                enterTask(id);
                QVERIFY2(seen, qPrintable(QStringLiteral("task dialog did not open: ") + id));
                QCOMPARE(activeTask(), QString());   // dialogs snap back to the reading canvas
                break;
            }
            case gp::TaskKind::Toggle: {
                enterTask(id);
                auto *panel = m_win->findChild<gp::AIChatPanel *>();
                QVERIFY2(panel && panel->isVisible(), qPrintable(QStringLiteral("toggle task did not show its panel: ") + id));
                enterTask(id);
                QVERIFY2(!panel->isVisible(), qPrintable(QStringLiteral("toggle task did not hide its panel: ") + id));
                break;
            }
            case gp::TaskKind::Standard:
                break;
            }
        }
        QCOMPARE(checked, 13);   // OCR Verify … Watermark; the old ScreenNav minus "Standard"
    }

    // ── Entry commands that ARE routes (ribbon OCR, Compare, Compress, Watermark …) land on their task ──
    void taskEntryCommandsRouteToTheirTask()
    {
        openSample();
        for (const gp::TaskSpec &task : gp::TaskNav::tasks()) {
            if (!task.entryIsRoute || task.entryTool == ToolId::COUNT) continue;
            const QString id = QString::fromLatin1(task.id);
            if (task.kind == gp::TaskKind::Dialog) {
                bool seen = false;
                observeNextModal(&seen);
                trigger(task.entryTool);
                QVERIFY2(seen, qPrintable(QStringLiteral("entry command did not open its task dialog: ") + id));
            } else {
                trigger(task.entryTool);
                QVERIFY2(activeTask() == id, qPrintable(QStringLiteral("entry command did not route to its task: ") + id));
                enterTask(QString());
            }
        }
    }
};

QTEST_MAIN(TestViewParity)
#include "TestViewParity.moc"
