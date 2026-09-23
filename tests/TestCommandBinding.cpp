// SPDX-License-Identifier: Apache-2.0
// R15 (2026-09-13) — canonical command binding and honest feature discovery.
//
// WHOLE-PRODUCT-AND-PLAN-REVIEW PP06 / UI-BUTTON-REVIEW UI02/UI04: the ribbon
// declared 144 entries and HID 51 of them; the visible stand-alone buttons
// derived enablement independently of the registry (review probe: after the
// session went read-only, Edit buttons stayed enabled while the canonical
// actions were disabled); the layer panel offered a visibility toggle it did
// not perform (PP04).
//
// Contract pinned here (all on the REAL MainWindow + Bootstrapper, offscreen):
//   - every sampled ribbon button's enabled state EQUALS the canonical
//     registry action's enabled state — before, during and after a read-only
//     session armed through the production expired-document open (ARC07 path);
//   - the same predicate drives the menu item with the same toolId (bound via
//     enabledChanged), so menu/ribbon/shortcut are three views of one command;
//   - planned entries are VISIBLE, disabled, and carry a truthful reason plus
//     a supported alternative on statusTip/tooltip/accessibleDescription —
//     never tooltip-only, never hidden;
//   - promoted entries drive their real routes: measure lands on the Measure
//     task panel, thumbs on the Pages pane, batchConv raises the Batch task;
//   - sampled ENABLED actions produce real effects (rotate persists on disk,
//     stamp arms the placement tool, compress opens its dialog);
//   - the layers panel no longer offers a lying checkbox (PP04): items are
//     plain identification entries and a visible notice states the limitation.
//
// Every surface introduced by R15 is resolved at RUNTIME (objectName/property
// lookups, QMetaObject invokes on Q_INVOKABLE seams) so this suite COMPILES
// against the pre-R15 baseline — where the new anchors fail exactly at the
// missing binding — for scoped-stash revert verification.
#include <QtTest/QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QTimer>
#include <QPdfWriter>
#include <QPainter>
#include <QToolButton>
#include <QLabel>
#include <QAction>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDialog>

#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "core/AppContext.h"
#include "core/ToolId.h"
#include "shell/Ribbon.h"
#include "shell/RibbonModel.h"
#include "shell/ToolRegistry.h"
#include "shell/MenuBar.h"
#include "shell/Sidebar.h"
#include "engines/DocumentSession.h"
#include "engines/PdfEditorEngine.h"
#include "modes/MeasureMode.h"
#include "ui/PdfViewerWidget.h"

using gp::MainWindow;

namespace {

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

// Arm read-only through the REAL production entry (expired-document open),
// exactly like TestReadOnlyGate: a throw-away engine writes an expired
// §9.11 date into a fresh copy.
void makeExpiredCopy(const QString &src, const QString &dest)
{
    PdfEditorEngine writer;
    const QString normalized = dest + QStringLiteral(".norm.pdf");
    QVERIFY(writer.loadDocumentForEditing(src));
    QVERIFY(writer.saveDocument(normalized));
    QVERIFY2(writer.setExpiryDate(normalized, QDate::currentDate().addDays(-1), dest),
             "setExpiryDate must produce the expired fixture copy");
    QVERIFY(QFileInfo::exists(dest));
    QVERIFY(PdfEditorEngine::readExpiryDate(dest).isValid());
}

void scheduleModalDismiss(int turnsLeft = 100)
{
    QTimer::singleShot(0, [turnsLeft] {
        if (QWidget *w = QApplication::activeModalWidget()) {
            w->close();
            return;
        }
        if (turnsLeft > 0) scheduleModalDismiss(turnsLeft - 1);
    });
}

// Deterministically observe the next modal dialog (records the sighting and
// closes it inside the nested event loop). Bounded retry budget; no sleeps.
void observeNextModal(bool* seen, int turnsLeft)
{
    QTimer::singleShot(0, [seen, turnsLeft] {
        if (QWidget *w = QApplication::activeModalWidget()) {
            if (seen) *seen = true;
            w->close();
            return;
        }
        if (turnsLeft > 0) observeNextModal(seen, turnsLeft - 1);
        else qWarning() << "observeNextModal: no modal appeared within the turn budget";
    });
}

struct PageGeometry { int count = 0; bool landscape0 = false; };

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

class TestCommandBinding : public QObject {
    Q_OBJECT

    std::unique_ptr<MainWindow> m_win;

    QToolButton* ribbonButton(const QString& toolId) const
    {
        for (auto* btn : m_win->findChildren<QToolButton*>())
            if (btn->property("toolId").toString() == toolId)
                return btn;
        return nullptr;
    }

    // Build every lazy ribbon tab body so all buttons exist.
    void buildAllRibbonTabs()
    {
        auto* ribbon = m_win->findChild<gp::Ribbon*>();
        QVERIFY(ribbon);
        const auto& tabs = gp::RibbonModel::tabs();
        for (int i = 0; i < tabs.size(); ++i)
            ribbon->raiseTab(tabs.at(i).name);
        QApplication::processEvents();
    }

    gp::ToolRegistry* registry() const
    {
        return m_win->findChild<gp::ToolRegistry*>();
    }

    // The canonical enablement for a tool-id string (same resolution the
    // ribbon binding uses).
    bool canonicalEnabled(const QString& toolId, bool* ok = nullptr) const
    {
        if (ok) *ok = false;
        auto* reg = registry();
        if (!reg) return false;
        const auto id = toolIdFromString(toolId);
        if (!id.has_value()) return false;
        QAction* action = reg->actionFor(id.value());
        if (!action) return false;
        if (ok) *ok = true;
        return action->isEnabled();
    }

    QAction* menuAction(const QString& toolId) const
    {
        return m_win->menuBarWidget()
            ->findChild<QAction*>(QStringLiteral("menu-") + toolId);
    }

    gp::Sidebar* leftSidebar() const
    {
        return m_win->findChild<gp::Sidebar*>(QStringLiteral("leftSidebar"));
    }

    QString activePane(gp::Sidebar* pane) const
    {
        QString out;
        QMetaObject::invokeMethod(pane, "activePane", Q_RETURN_ARG(QString, out));
        return out;
    }

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("GlyphPDFTests"));
        QCoreApplication::setApplicationName(QStringLiteral("TestCommandBinding"));
    }

    void init()
    {
        m_win = std::make_unique<MainWindow>(Bootstrapper::createContext());
        QVERIFY(m_win->pdfViewer());
        QVERIFY(m_win->appContext()->pdfEditor);
        QVERIFY(m_win->appContext()->document);
        QVERIFY(registry());
        buildAllRibbonTabs();
    }

    void cleanup()
    {
        m_win.reset();
    }

    // ── R15 (a): ribbon enablement IS the registry predicate (UI02) ─────────
    void ribbonButtonsMirrorRegistryEnablementAcrossReadOnly()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makeMultiPagePdf(a, { "P1", "P2" });

        m_win->openDocument(a);
        QVERIFY(m_win->pdfViewer()->pageCount() == 2);

        // Sample across mutation and viewing/entry tools.
        const QStringList sample = {
            "save", "rotate", "deletePage", "insertPage", "highlight",
            "print", "search", "undo", "stamp", "compress", "ocr", "select"
        };
        for (const QString& id : sample) {
            QToolButton* btn = ribbonButton(id);
            QVERIFY2(btn, qPrintable(QStringLiteral("ribbon button '%1' must exist").arg(id)));
            bool ok = false;
            const bool canon = canonicalEnabled(id, &ok);
            QVERIFY2(ok, qPrintable(QStringLiteral("'%1' must resolve to a canonical action").arg(id)));
            QVERIFY2(btn->isEnabled() == canon,
                      qPrintable(QStringLiteral("'%1' button must equal the registry predicate").arg(id)));
        }

        // Arm read-only through the production expired-document open.
        const QString expired = dir.filePath("expired.pdf");
        makeExpiredCopy(a, expired);
        scheduleModalDismiss();
        m_win->openDocument(expired);
        QVERIFY(m_win->pdfViewer()->isReadOnly());

        for (const QString& id : sample) {
            QToolButton* btn = ribbonButton(id);
            bool ok = false;
            const bool canon = canonicalEnabled(id, &ok);
            QVERIFY(ok);
            QVERIFY2(btn->isEnabled() == canon,
                      qPrintable(QStringLiteral("'%1' button must follow the registry after read-only").arg(id)));
        }
        // Spot-check the honest states themselves (mutation disabled,
        // viewing/entry available).
        QVERIFY2(!ribbonButton("save")->isEnabled(), "save must disable in read-only");
        QVERIFY2(!ribbonButton("rotate")->isEnabled(), "rotate must disable in read-only");
        QVERIFY2(ribbonButton("search")->isEnabled(), "find stays available in read-only");

        // Re-enable: the same buttons become eligible again.
        QVERIFY2(QMetaObject::invokeMethod(m_win->appContext()->document.get(),
                                           "setReadOnly", Q_ARG(bool, false)),
                 "DocumentSession::setReadOnly(bool) must be invokable");
        QTRY_VERIFY_WITH_TIMEOUT(ribbonButton("save")->isEnabled(), 5000);
        QVERIFY2(ribbonButton("rotate")->isEnabled(), "rotate must re-enable");
    }

    // ── R15 (a): the menu shares the same one predicate ─────────────────────
    void menuItemsShareTheRegistryPredicate()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makeMultiPagePdf(a, { "P1" });

        QAction* menuSave = menuAction("save");
        QAction* menuRotate = menuAction("rotate-cw");
        QVERIFY2(menuSave && menuRotate,
                 "registry-dispatched menu items must expose objectName 'menu-<toolId>' (R15)");
        QVERIFY2(menuSave->shortcut() == QKeySequence::Save,
                 "the shortcut lives on the same action whose state is mirrored");

        m_win->openDocument(a);
        bool ok = false;
        const bool saveCanon = canonicalEnabled("save", &ok);
        QVERIFY(ok);
        QVERIFY(menuSave->isEnabled());
        QCOMPARE(menuSave->isEnabled(), saveCanon);

        const QString expired = dir.filePath("expired.pdf");
        makeExpiredCopy(a, expired);
        scheduleModalDismiss();
        m_win->openDocument(expired);

        QVERIFY2(!menuSave->isEnabled(), "menu Save must disable in read-only");
        QVERIFY2(!menuRotate->isEnabled(), "menu Rotate must disable in read-only");
        QCOMPARE(menuSave->isEnabled(), canonicalEnabled("save"));
        QCOMPARE(ribbonButton("save")->isEnabled(), menuSave->isEnabled());
    }

    // ── R15 (b): planned entries are visible, honest and inert ──────────────
    void plannedEntriesRenderVisibleDisabledAndDisclosed()
    {
        // Every declared ribbon entry renders (PP06: a hidden row is a secret).
        int modelCount = 0;
        for (const auto& tab : gp::RibbonModel::tabs())
            for (const auto& grp : tab.groups)
                modelCount += grp.tools.size();

        QSet<QString> rendered;
        for (auto* btn : m_win->findChildren<QToolButton*>()) {
            const QString id = btn->property("toolId").toString();
            if (!id.isEmpty()) rendered.insert(id);
        }
        QVERIFY2(rendered.size() == modelCount,
                 "every declared ribbon entry must render — planned visible-disabled, none hidden");

        // Every planned entry is disabled and disclosed on all channels.
        const QSet<QString>& planned = gp::RibbonModel::plannedTools();
        QVERIFY(!planned.isEmpty());
        for (const QString& id : planned) {
            QToolButton* btn = ribbonButton(id);
            QVERIFY2(btn, qPrintable(QStringLiteral("planned entry '%1' must render visibly").arg(id)));
            QVERIFY2(!btn->isEnabled(),
                     qPrintable(QStringLiteral("planned entry '%1' must be disabled").arg(id)));
            const QString reason = btn->statusTip();
            QVERIFY2(!reason.trimmed().isEmpty(),
                     qPrintable(QStringLiteral("planned entry '%1' must carry a status-tip reason").arg(id)));
            QVERIFY2(!reason.contains(QStringLiteral("Planned for a future release")),
                     qPrintable(QStringLiteral("planned entry '%1' reason must be truthful, not boilerplate").arg(id)));
            QVERIFY2(btn->toolTip().contains(reason),
                     qPrintable(QStringLiteral("planned entry '%1' tooltip must contain the reason").arg(id)));
            QVERIFY2(btn->accessibleDescription().contains(reason),
                     qPrintable(QStringLiteral("planned entry '%1' reason must be accessible beyond the tooltip").arg(id)));
        }
    }

    // ── R15 (b/c): promoted entries drive their real routes ─────────────────
    void measureEntryLandsOnTheMeasureTaskPanel()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makeMultiPagePdf(a, { "P1", "P2" });
        m_win->openDocument(a);

        // measure → the T1 Measure task panel (real route; the pre-R15
        // ribbon treated "measure" as planned and the string resolved to
        // nothing — the promoted route is the fix under test).
        m_win->onToolActivated(QStringLiteral("measure"));
        QVERIFY2(m_win->findChild<gp::MeasureMode*>(),
                 "'measure' must land on the Measure task panel");
    }

    // Split the pane pins into their own test for clarity.
    void paneEntriesSwitchTheRealSidebarPanes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makeMultiPagePdf(a, { "P1" });
        m_win->openDocument(a);

        gp::Sidebar* left = leftSidebar();
        QVERIFY(left);
        auto* right = m_win->findChild<gp::Sidebar*>(QStringLiteral("rightSidebar"));
        QVERIFY(right);

        m_win->onToolActivated(QStringLiteral("thumbs"));
        QVERIFY2(activePane(left) == QStringLiteral("pages"),
                 "'thumbs' must open the Pages pane (R15 route)");

        m_win->onToolActivated(QStringLiteral("bookmarks"));
        QCOMPARE(activePane(left), QStringLiteral("bookmarks"));

        m_win->onToolActivated(QStringLiteral("comments"));
        QCOMPARE(activePane(left), QStringLiteral("comments"));

        m_win->onToolActivated(QStringLiteral("layers"));
        QCOMPARE(activePane(right), QStringLiteral("layers"));
    }

    void batchEntryRaisesTheBatchTask()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makeMultiPagePdf(a, { "P1" });
        m_win->openDocument(a);

        m_win->onToolActivated(QStringLiteral("batchConv"));
        // TaskStateSync raises the task's ribbon tab (Convert) — the visible
        // writer's channel, not a parallel state.
        auto* ribbon = m_win->findChild<gp::Ribbon*>();
        QTRY_VERIFY_WITH_TIMEOUT(ribbon->activeTabName() == QStringLiteral("Convert"), 5000);
    }

    void findReplaceEntriesOpenTheReplaceDialog()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makeMultiPagePdf(a, { "P1", "REPLACEME" });
        m_win->openDocument(a);

        m_win->onToolActivated(QStringLiteral("findRep"));
        auto* dialog = m_win->findChild<QWidget*>(QStringLiteral("findReplaceDialog"));
        QVERIFY2(dialog && dialog->isVisible(),
                 "'findRep' must open the T2-2 Find & Replace dialog");
        m_win->onToolActivated(QStringLiteral("regex"));
        QVERIFY(m_win->findChild<QWidget*>(QStringLiteral("findReplaceDialog"))->isVisible());
        dialog->close();
    }

    // ── R15: sampled enabled actions produce real effects or honest refusals ─
    void sampledEnabledActionsHaveRealEffects()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makeMultiPagePdf(a, { "P1", "P2" });
        m_win->openDocument(a);

        // rotate → persists 90° in the displayed file on disk.
        m_win->onToolActivated(QStringLiteral("rotate"));
        {
            const PageGeometry g = diskGeometry(a);
            QVERIFY2(g.landscape0, "the ribbon rotate route must persist on disk");
        }

        // stamp → the placement tool arms (T2-6 canonical wiring, PP-level
        // "no-op" honesty check).
        m_win->onToolActivated(QStringLiteral("stamp"));
        QCOMPARE(m_win->pdfViewer()->toolMode(), ToolMode::Stamp);
        m_win->pdfViewer()->setToolMode(ToolMode::HandTool);

        // compress → the real task dialog opens (entry route). The dialog is
        // modal and exec() blocks, so a queued driver OBSERVES it (flag) and
        // closes it inside the nested loop; the assert runs after the call
        // returns (deterministic barrier, no sleeps).
        bool dialogSeen = false;
        observeNextModal(&dialogSeen, 100);
        m_win->onToolActivated(QStringLiteral("compress"));
        QVERIFY2(dialogSeen, "'compress' must open the Compress task dialog");
    }

    // ── R15 (c): the layers panel no longer offers a lying checkbox ─────────
    void layersPanelIsIdentificationOnlyWithVisibleNotice()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath("a.pdf");
        makeMultiPagePdf(a, { "P1" });
        m_win->openDocument(a);   // pageChanged triggers the layers refresh

        auto* layersList = m_win->findChild<QListWidget*>(QStringLiteral("layersListWidget"));
        QVERIFY2(layersList,
                 "the layers list must expose objectName 'layersListWidget' (R15)");
        for (int i = 0; i < layersList->count(); ++i) {
            QVERIFY2(!(layersList->item(i)->flags() & Qt::ItemIsUserCheckable),
                     "PP04: layer rows must not offer a visibility checkbox the engine cannot honor");
        }

        auto* note = m_win->findChild<QLabel*>(QStringLiteral("layersVisibilityNote"));
        QVERIFY2(note, "the layers pane must carry the visible limitation notice");
        QVERIFY2(!note->text().trimmed().isEmpty(), "the notice must have content");
    }
};

QTEST_MAIN(TestCommandBinding)
#include "TestCommandBinding.moc"
