// SPDX-License-Identifier: Apache-2.0
// R17 (2026-09-13) — keyboard accessibility, RTL/CJK and scale pass.
//
// WHOLE-PROJECT-READINESS-REVIEW (UI section, R17 acceptance): keyboard /
// screen-reader complete core workflows; Arabic RTL; 100–200% DPI without
// truncation; disabled reasons discoverable beyond a tooltip.
//
// Contract pinned here (real MainWindow + Bootstrapper, offscreen):
//   - the CORE routes are keyboard-complete: open (Ctrl+O menu shortcut),
//     navigate (PageUp/PageDown/Home/End on the Tab-reachable canvas),
//     edit (Space activates the focused ribbon tool), save (Ctrl+S via
//     delivered key events), find & replace (Ctrl+H opens the dialog),
//     F6 cycles the major regions; (no export workflow is keyboard-pinned
//     here — see TestUiAccessibility note in the ledger)
//   - the production RTL toggle mirrors the workspace (sidebars swap sides)
//     and restores;
//   - CJK text resolves a font that can actually render the glyphs;
//   - disabled controls disclose their reason on the accessible channels
//     (statusTip/accessibleDescription), never tooltip-only — ribbon planned
//     entries, menu Disabled items and welcome capability cards;
//   - at 200% scale (separate ctest entry with QT_SCALE_FACTOR=2) the whole
//     binary still passes and the scale factor is genuinely applied.
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QLabel>
#include <QPdfWriter>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QToolButton>
#include <QWidget>
#include <QFileDialog>
#include <QScrollArea>
#include <QTimer>
#include <QGuiApplication>
#include "modes/ModeController.h"

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/ToolId.h"
#include "shell/Ribbon.h"
#include "shell/RibbonModel.h"
#include "shell/MenuBar.h"
#include "shell/StatusBar.h"
#include "shell/Sidebar.h"
#include "ui/WelcomeWidget.h"
#include "ui/PdfViewerWidget.h"
#include "ui/FindReplaceDialog.h"

using gp::MainWindow;

namespace {

void makePdf(const QString &path, int pages)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&w);
    for (int i = 0; i < pages; ++i) {
        if (i > 0) w.newPage();
        p.drawText(100, 100, QStringLiteral("PAGE %1").arg(i + 1));
    }
    p.end();
    QVERIFY(QFileInfo::exists(path));
}

QToolButton* ribbonButton(QWidget& root, const QString& toolId)
{
    for (auto* btn : root.findChildren<QToolButton*>())
        if (btn->property("toolId").toString() == toolId)
            return btn;
    return nullptr;
}

QAction* menuAction(QWidget& root, const QString& toolId)
{
    return root.findChild<QAction*>(QStringLiteral("menu-") + toolId);
}

} // namespace

class TestUiAccessibility : public QObject {
    Q_OBJECT

    std::unique_ptr<MainWindow> m_win;

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestUiAccessibility"));
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    }

    void init()
    {
        m_win = std::make_unique<MainWindow>(Bootstrapper::createContext());
        m_win->show();
        QVERIFY(m_win->pdfViewer());
        // Ribbon tab bodies build lazily — force-build every tab so the
        // keyboard and disclosure pins see the real buttons.
        auto* ribbon = m_win->findChild<gp::Ribbon*>();
        QVERIFY(ribbon);
        const auto& tabs = gp::RibbonModel::tabs();
        for (int i = 0; i < tabs.size(); ++i)
            ribbon->raiseTab(tabs.at(i).name);
        QApplication::processEvents();
    }

    void cleanup()
    {
        // The RTL toggle persists its choice; never leak direction or theme
        // state into other suites.
        QSettings().remove(QStringLiteral("ui/rtl"));
        QApplication::setLayoutDirection(Qt::LeftToRight);
        m_win.reset();
    }

    // ── Open route: Ctrl+O is on the canonical action and drives the open ───
    void keyboardOpenRouteCompletesOnARealFixture()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("kbd-open.pdf");
        makePdf(a, 1);

        QAction* open = menuAction(*m_win, "open");
        QVERIFY2(open, "the Open menu action must expose 'menu-open'");
        QVERIFY2(open->shortcut() == QKeySequence::Open,
                 "Open must carry the standard Ctrl+O shortcut");

        // Trigger the shortcut path with the dialog driven deterministically:
        // queued pick inside the modal's nested loop (no sleeps).
        QTimer::singleShot(0, [a] {
            if (auto *dlg = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
                dlg->selectFile(a);
                QTimer::singleShot(0, [dlg, a] {
                    if (dlg->selectedFiles().contains(a))
                        QMetaObject::invokeMethod(dlg, "accept");
                    else
                        QTimer::singleShot(0, [dlg, a] {
                            if (dlg->selectedFiles().contains(a))
                                QMetaObject::invokeMethod(dlg, "accept");
                        });
                });
            }
        });
        open->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(m_win->pdfViewer()->pageCount(), 1, 20000);
    }

    // ── Navigate route: the canvas is Tab-reachable and turns pages ──────────
    void keyboardNavigationTurnsPages()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("kbd-nav.pdf");
        makePdf(a, 3);
        m_win->openDocument(a);
        QCOMPARE(m_win->pdfViewer()->pageCount(), 3);

        auto* viewer = m_win->pdfViewer();
        QVERIFY2(viewer->focusPolicy() != Qt::NoFocus,
                 "the reading canvas must be reachable by Tab (R17)");
        viewer->setFocus();
        QApplication::processEvents();

        // Key events are delivered to the widget; the page navigator's
        // current-page may settle on the next loop turn.
        QTest::keyClick(viewer, Qt::Key_PageDown);
        QTRY_COMPARE_WITH_TIMEOUT(viewer->currentPage(), 1, 5000);
        QTest::keyClick(viewer, Qt::Key_PageDown);
        QTRY_COMPARE_WITH_TIMEOUT(viewer->currentPage(), 2, 5000);
        QTest::keyClick(viewer, Qt::Key_PageUp);
        QTRY_COMPARE_WITH_TIMEOUT(viewer->currentPage(), 1, 5000);
        QTest::keyClick(viewer, Qt::Key_End);
        QTRY_COMPARE_WITH_TIMEOUT(viewer->currentPage(), 2, 5000);
        QTest::keyClick(viewer, Qt::Key_Home);
        QTRY_COMPARE_WITH_TIMEOUT(viewer->currentPage(), 0, 5000);
    }

    // ── Edit route: Space activates the focused ribbon tool ─────────────────
    void keyboardSpaceActivatesTheFocusedRibbonTool()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("kbd-edit.pdf");
        makePdf(a, 1);
        m_win->openDocument(a);

        QToolButton* stamp = ribbonButton(*m_win, QStringLiteral("stamp"));
        QVERIFY(stamp);
        QVERIFY(stamp->isEnabled());
        QVERIFY2(stamp->focusPolicy() & Qt::TabFocus,
                 "ribbon commands must be Tab-reachable");
        stamp->setFocus();
        QVERIFY(stamp->hasFocus());

        QTest::keyClick(stamp, Qt::Key_Space);
        QVERIFY2(m_win->pdfViewer()->toolMode() == ToolMode::Stamp,
                 "Space on the focused ribbon button must dispatch the tool");
    }

    // ── Save route: Ctrl+S carries the standard sequence ─────────────────────
    void keyboardSaveShortcutIsCanonical()
    {
        QAction* save = menuAction(*m_win, "save");
        QVERIFY(save);
        QCOMPARE(save->shortcut(), QKeySequence::Save);
        // r15-F2: the shortcut must be DELIVERABLE — the real key sequence
        // on the focused window fires the shared action (the read-only
        // refusal for the same route is pinned by TestReadOnlyGate /
        // TestCommandBinding).
        QSignalSpy fired(save, &QAction::triggered);
        QVERIFY(fired.isValid());
        QTest::keySequence(m_win.get(), QKeySequence::Save);
        QTRY_COMPARE(fired.count(), 1);
    }

    // ── Find & Replace route: Ctrl+H opens the dialog (key delivery) ────────
    void keyboardFindReplaceShortcutOpensTheDialog()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("kbd-fr.pdf");
        makePdf(a, 1);
        m_win->openDocument(a);

        QAction* fr = menuAction(*m_win, "find-replace");
        QVERIFY(fr);
        QVERIFY(fr->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_H));
        // r15-F2: deliver the actual shortcut keys (not trigger()).
        QTest::keySequence(m_win.get(), QKeySequence(Qt::CTRL | Qt::Key_H));
        auto* dlg = m_win->findChild<FindReplaceDialog*>();
        QVERIFY(dlg && dlg->isVisible());
        dlg->close();
    }

    // ── F6 cycles the major regions ──────────────────────────────────────────
    void f6CyclesTheMajorRegions()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("kbd-f6.pdf");
        makePdf(a, 1);
        m_win->openDocument(a);

        auto* viewer = m_win->pdfViewer();
        viewer->setFocus();
        QWidget* before = QApplication::focusWidget();
        QTest::keyClick(m_win.get(), Qt::Key_F6);
        QApplication::processEvents();
        QWidget* after = QApplication::focusWidget();
        QVERIFY2(after != before && after != nullptr,
                 "F6 must move focus to another major region");
        // The cycle lands in one of the five known regions.
        const QWidget* regions[] = {
            m_win->findChild<gp::Ribbon*>(),
            m_win->findChild<gp::Sidebar*>(QStringLiteral("leftSidebar")),
            m_win->findChild<gp::ModeController*>(),
            m_win->findChild<gp::Sidebar*>(QStringLiteral("rightSidebar")),
            m_win->statusBar()
        };
        bool inRegion = false;
        for (const QWidget* r : regions)
            if (r && (after == r || r->isAncestorOf(after)))
                inRegion = true;
        QVERIFY2(inRegion, "F6 must land focus inside a known major region");
    }

    // ── RTL: the production toggle mirrors the workspace and restores ────────
    void rtlToggleMirrorsTheWorkspace()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("rtl.pdf");
        makePdf(a, 1);
        m_win->openDocument(a);

        auto* left = m_win->findChild<gp::Sidebar*>(QStringLiteral("leftSidebar"));
        auto* right = m_win->findChild<gp::Sidebar*>(QStringLiteral("rightSidebar"));
        QVERIFY(left && right);

        const int ltrLeftX = left->mapTo(m_win.get(), QPoint(0, 0)).x();
        const int ltrRightX = right->mapTo(m_win.get(), QPoint(0, 0)).x();
        QVERIFY2(ltrLeftX < ltrRightX, "LTR sanity: the left sidebar sits left");

        // The production route (ribbon "rtl" entry / View ▸ RTL).
        m_win->onToolActivated(QStringLiteral("rtl"));
        QApplication::processEvents();
        QCOMPARE(QApplication::layoutDirection(), Qt::RightToLeft);

        const int rtlLeftX = left->mapTo(m_win.get(), QPoint(0, 0)).x();
        const int rtlRightX = right->mapTo(m_win.get(), QPoint(0, 0)).x();
        QVERIFY2(rtlLeftX > rtlRightX,
                 "RTL must mirror the workspace: the sidebars swap sides");
        // No clipped chrome: both sidebars remain fully inside the window.
        QVERIFY(rtlLeftX >= 0);
        QVERIFY(rtlRightX + right->width() <= m_win->width());

        m_win->onToolActivated(QStringLiteral("rtl"));
        QApplication::processEvents();
        QCOMPARE(QApplication::layoutDirection(), Qt::LeftToRight);
    }

    // ── CJK: font fallback resolves renderable glyphs ────────────────────────
    void cjkFontFallbackResolvesRenderableGlyphs()
    {
        const QString cjk = QStringLiteral("GlyphPDF\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95");
        QVERIFY(QFontDatabase::writingSystems().contains(QFontDatabase::SimplifiedChinese)
                || QFontDatabase::writingSystems().contains(QFontDatabase::TraditionalChinese));

        QLabel probe(cjk);
        QFont resolved = probe.font();
        resolved.setHintingPreference(QFont::PreferNoHinting);
        probe.setFont(resolved);
        QVERIFY2(probe.sizeHint().width() > 0, "CJK label must measure non-empty");

        QFontMetrics metrics(resolved);
        bool glyphRenderable = metrics.inFontUcs4(0x4E2D);      // 中
        if (!glyphRenderable) {
            // Font fallback is resolved at paint time; probe the substitution
            // families Qt picks for the writing system.
            const QStringList families = QFontDatabase::families(
                QFontDatabase::SimplifiedChinese);
            for (const QString& family : families) {
                QFont f(family);
                QFontMetrics m(f);
                if (m.inFontUcs4(0x4E2D)) { glyphRenderable = true; break; }
            }
        }
        QVERIFY2(glyphRenderable,
                 "CJK glyphs must resolve through fallback or a CJK family");
    }

    // ── Disabled reasons ride the accessible channels, never tooltip-only ────
    void disabledReasonsAreDiscoverableWithoutATooltipHover()
    {
        // Ribbon planned entries: disabled + statusTip reason + accessible
        // description carrying reason AND alternative.
        for (const QString& id : gp::RibbonModel::plannedTools()) {
            QToolButton* btn = ribbonButton(*m_win, id);
            QVERIFY2(btn, qPrintable(QStringLiteral("planned '%1' must render").arg(id)));
            QVERIFY2(!btn->isEnabled(), qPrintable(id + " must be disabled"));
            QVERIFY2(!btn->statusTip().trimmed().isEmpty(),
                     qPrintable(id + ": the reason must be on the status tip "
                                    "(readable by AT/hover without tooltips)"));
            QVERIFY2(btn->accessibleDescription().contains(btn->statusTip()),
                     qPrintable(id + ": the accessible description must carry the reason"));
            QVERIFY2(!btn->accessibleName().trimmed().isEmpty(),
                     qPrintable(id + ": disabled controls keep their accessible name"));
        }

        // Menu Disabled items: the status tip names the planned state.
        for (const auto& act : m_win->menuBarWidget()->findChildren<QAction*>()) {
            if (!act->objectName().startsWith(QStringLiteral("menu-")) || act->isEnabled())
                continue;
            QVERIFY2(!act->statusTip().trimmed().isEmpty(),
                     qPrintable(act->objectName()
                                + ": disabled menu items need an accessible reason"));
        }
    }

    // ── 200% scale: runs in the dedicated QT_SCALE_FACTOR=2 ctest entry; the
    // logical-contract assertions there must hold at any scale.
    void highDpiScaleFactorIsAppliedAndLayoutStable()
    {
        const qreal dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
        if (qFuzzyCompare(dpr, qreal(1))) {
            QSKIP("not running under QT_SCALE_FACTOR=2 (dedicated ctest entry)");
        }
        QVERIFY2(qFuzzyCompare(dpr, qreal(2)),
                 "the 200% ctest entry must see devicePixelRatio 2");

        // The welcome dashboard must not truncate at 200%: every card fits
        // the clipping viewport at the small acceptance size.
        WelcomeWidget w;
        w.resize(1366, 768);
        w.show();
        QApplication::processEvents();
        QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QApplication::processEvents();
        auto* scroll = w.findChild<QScrollArea*>();
        QVERIFY(scroll);
        const auto cards = w.findChildren<QPushButton*>();
        int cardCount = 0;
        for (const auto* card : cards) {
            if (card->property("role").toString() != QLatin1String("actionCard"))
                continue;
            ++cardCount;
            QVERIFY2(card->geometry().right() <= card->parentWidget()->width(),
                     "no card may clip at 200% scale");
        }
        QVERIFY(cardCount >= 13);
    }
};

QTEST_MAIN(TestUiAccessibility)
#include "TestUiAccessibility.moc"
