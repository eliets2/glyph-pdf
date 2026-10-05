// SPDX-License-Identifier: Apache-2.0
// TestComposeMode — §9.17/§9.18 side-by-side visual composition.
//
// What this suite pins:
//   1. Mode registration end-to-end: ToolId::Compose, the TaskNav table,
//      RibbonModel (Organize ▸ Document), commands.json, ScreenNav,
//      HomeController dispatch and ModeController lazy creation.
//   2. Two-document open — any two PDFs, or the same PDF twice; refusals
//      leave the previous state untouched.
//   3. Page pick → insert: order + position correctness through a REAL
//      applyTransfers (source pages land at the chosen position, in pick
//      order), and the source file is never modified.
//   4. Image inventory → pick → placement: the /XObject /Image inventory
//      carries dimensions + filters; a picked image lands at the pinned
//      geometry (fittedRect — aspect preserved, never stretched).
//   5. Transactional save: a commit fault refuses with the target
//      byte-identical and no history step; a retry after the fault succeeds.
//   6. Undo/redo semantics: one checked-history step per apply; undo restores
//      the page structure exactly; redo re-applies; empty history refuses.
//   7. Mismatched page sizes carry an honest scaling disclosure — both sizes
//      named, "never stretched" stated; equal sizes carry none.
//   8. A11y (the r4-ux discipline): every thumbnail/canvas surface names its
//      document role (source/target), file, and page numbers.
#include <QtTest>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QToolButton>

#include "modes/ComposeMode.h"
#include "modes/ModeController.h"
#include "shell/TaskNav.h"
#include "shell/RibbonModel.h"
#include "shell/ScreenNav.h"
#include "shell/controllers/HomeController.h"
#include "core/AppContext.h"
#include "core/ToolId.h"
#include "core/CommandRegistry.h"
#include "core/interfaces/IPdfEditorEngine.h"
#include "core/interfaces/IPdfRenderer.h"
#include "engines/PdfEditorEngine.h"
#include "engines/SafeSave.h"
#include "engines/BackendRouter.h"
#include "engines/ImageExtractEngine.h"

#include <QPdfWriter>
#include <QPageSize>

using namespace gp;

namespace {

// Hand-built N-page text PDF (TestCompareEntry's createPagePdf idiom) with a
// parameterized page size so mismatched-size fixtures are real. `pageSize`
// is in points (Letter 612×792, A4 595×842).
QString createPagePdf(const QString& path, const QStringList& pageTexts,
                      const QSizeF& pageSize = QSizeF(612, 792))
{
    const int n = pageTexts.size();
    QByteArray pdf = "%PDF-1.4\n";
    QList<qint64> offsets;
    offsets.append(pdf.size());
    pdf += "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n";
    offsets.append(pdf.size());
    QByteArray kids;
    for (int k = 0; k < n; ++k)
        kids += QByteArray::number(3 + 2 * k) + " 0 R ";
    pdf += "2 0 obj<</Type/Pages/Kids[" + kids + "]/Count "
           + QByteArray::number(n) + ">>endobj\n";
    for (int k = 0; k < n; ++k) {
        const int pageNo = 3 + 2 * k;
        const int contNo = 4 + 2 * k;
        const QString line = pageTexts.at(k);
        QByteArray content;
        if (!line.isEmpty()) {
            QByteArray lit = line.toLatin1();
            lit.replace('\\', "\\\\").replace('(', "\\(").replace(')', "\\)");
            content = "BT /F1 12 Tf 72 720 Td (" + lit + ") Tj ET\n";
        }
        offsets.append(pdf.size());
        pdf += QByteArray::number(pageNo)
             + " 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 "
             + QByteArray::number(pageSize.width()) + ' '
             + QByteArray::number(pageSize.height()) + "]/Contents "
             + QByteArray::number(contNo)
             + " 0 R/Resources<</Font<</F1 " + QByteArray::number(3 + 2 * n)
             + " 0 R>>>>>>endobj\n";
        offsets.append(pdf.size());
        pdf += QByteArray::number(contNo) + " 0 obj<</Length "
             + QByteArray::number(content.size()) + ">>stream\n"
             + content + "endstream endobj\n";
    }
    offsets.append(pdf.size());
    pdf += QByteArray::number(3 + 2 * n)
         + " 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>endobj\n";
    const qint64 xrefOffset = pdf.size();
    const int objCount = 4 + 2 * n;
    pdf += "xref\n0 " + QByteArray::number(objCount) + "\n0000000000 65535 f \n";
    for (qint64 off : offsets)
        pdf += QByteArray::number(static_cast<qulonglong>(off))
                   .rightJustified(10, '0')
               + " 00000 n \n";
    pdf += "trailer<</Size " + QByteArray::number(objCount)
           + "/Root 1 0 R>>\nstartxref\n" + QByteArray::number(xrefOffset)
           + "\n%%EOF\n";

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(pdf);
    return path;
}

// A one-page PDF carrying a real embedded image (QPdfWriter embeds the
// pixmap through the image filters the inventory must report).
QString createImagePdf(const QString& path, const QImage& image)
{
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    w.setResolution(72);
    QPainter p(&w);
    p.drawImage(QRect(50, 50, 300, 160), image);
    p.drawText(100, 700, QStringLiteral("carrier page"));
    p.end();
    return path;
}

QByteArray sha256OfFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QByteArray();
    return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha256);
}

QListWidget* gridOf(QWidget& mode, ComposeSide side)
{
    return mode.findChild<QListWidget*>(side == ComposeSide::Source
        ? QStringLiteral("composeSourceGrid") : QStringLiteral("composeTargetGrid"));
}

QListWidget* imagesOf(QWidget& mode, ComposeSide side)
{
    return mode.findChild<QListWidget*>(side == ComposeSide::Source
        ? QStringLiteral("composeSourceImages") : QStringLiteral("composeTargetImages"));
}

QStringList pageTexts(const QString& path, int count)
{
    QStringList out;
    auto renderer = BackendRouter::rendererFor(path);
    if (!renderer) return out;
    for (int i = 0; i < count; ++i)
        out << renderer->extractText(i);
    return out;
}

} // namespace

class TestComposeMode : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString path(const QString& name) const { return m_dir.filePath(name); }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    // ── 1. Registration end-to-end ────────────────────────────────────────

    void composeIsRegisteredEndToEnd()
    {
        // ToolId: canonical string + alias round-trip.
        QCOMPARE(toolIdToString(ToolId::Compose), QStringLiteral("compose"));
        QCOMPARE(toolIdFromString(QStringLiteral("compose")).value_or(ToolId::COUNT),
                 ToolId::Compose);
        QCOMPARE(toolIdFromString(QStringLiteral("composeMode")).value_or(ToolId::COUNT),
                 ToolId::Compose);

        // TaskNav: the ONE navigation registry carries the workspace task,
        // and the entry tool round-trips to its own screen.
        const TaskSpec* spec = TaskNav::forScreen(QStringLiteral("compose"));
        QVERIFY2(spec, "TaskNav must carry a 'compose' task");
        QCOMPARE(spec->kind, TaskKind::Workspace);
        QCOMPARE(spec->entryTool, ToolId::Compose);
        QCOMPARE(TaskNav::title(QStringLiteral("compose")), QStringLiteral("Compose"));
        QCOMPARE(TaskNav::screenForTool(ToolId::Compose), QStringLiteral("compose"));
        QVERIFY(TaskNav::isEntryRoute(ToolId::Compose));
        QVERIFY(TaskNav::isKnownScreen(QStringLiteral("compose")));

        // RibbonModel: the Organize ▸ Document entry is wired (never planned).
        QVERIFY(!RibbonModel::plannedTools().contains(QStringLiteral("compose")));
        bool inOrganize = false;
        for (const auto& tab : RibbonModel::tabs()) {
            for (const auto& grp : tab.groups) {
                if (tab.name != QStringLiteral("Organize")) continue;
                for (const auto& tool : grp.tools)
                    if (tool.id == QStringLiteral("compose")) inOrganize = true;
            }
        }
        QVERIFY2(inOrganize, "ribbon Organize tab must carry the compose entry");

        // commands.json: the canonical command identity resolves — the id IS
        // the ToolId string (no second id space).
        const CommandRegistry& registry = CommandRegistry::instance();
        const CommandSpec* cmd = registry.find(QStringLiteral("compose"));
        QVERIFY2(cmd, "commands.json must carry the compose command");
        QVERIFY2(!cmd->label.isEmpty(), "the compose command must carry a label");
        QVERIFY2(!cmd->description.isEmpty(),
                 "the compose command must carry a description");
        QCOMPARE(registry.find(ToolId::Compose), cmd);

        // ScreenNav: the task appears as a screen button.
        ScreenNav nav;
        QVERIFY2(nav.findChild<QToolButton*>(QStringLiteral("screenNav_compose")),
                 "ScreenNav must expose the compose screen button");

        // HomeController dispatch + ModeController lazy creation.
        AppContext ctx;
        HomeController home(&ctx, nullptr);
        QVERIFY(home.handledTools().contains(ToolId::Compose));
        ModeController modes;
        QSignalSpy spy(&modes, &ModeController::screenChanged);
        modes.setScreen(QStringLiteral("compose"));
        QCOMPARE(spy.count(), 1);
        auto* compose = qobject_cast<ComposeMode*>(modes.currentWidget());
        QVERIFY2(compose, "setScreen(\"compose\") must surface a ComposeMode widget");
        QCOMPARE(modes.currentScreen(), QStringLiteral("compose"));
    }

    // ── 2. Two-document open ──────────────────────────────────────────────

    void twoDocumentOpen()
    {
        const QString src = createPagePdf(path("open_src.pdf"), {"S1", "S2"});
        const QString tgt = createPagePdf(path("open_tgt.pdf"), {"T1", "T2", "T3"});
        QVERIFY(!src.isEmpty() && !tgt.isEmpty());

        ComposeMode mode;
        QVERIFY(mode.sourcePath().isEmpty());
        QVERIFY(mode.setDocuments(src, tgt));
        QCOMPARE(mode.sourcePath(), src);
        QCOMPARE(mode.targetPath(), tgt);
        QCOMPARE(mode.pageCount(ComposeSide::Source), 2);
        QCOMPARE(mode.pageCount(ComposeSide::Target), 3);
        QCOMPARE(gridOf(mode, ComposeSide::Source)->count(), 2);
        QCOMPARE(gridOf(mode, ComposeSide::Target)->count(), 3);

        // The same PDF twice is a legal compose session.
        QVERIFY(mode.setDocuments(tgt, tgt));
        QCOMPARE(mode.pageCount(ComposeSide::Source), 3);
        QCOMPARE(mode.pageCount(ComposeSide::Target), 3);

        // The files label discloses both names AND both page counts
        // (the CompareMode cmpFilesLabel discipline) for the OPEN session.
        auto* files = mode.findChild<QLabel*>(QStringLiteral("composeFilesLabel"));
        QVERIFY(files);
        QVERIFY2(mode.setDocuments(src, tgt), "reopen (src, tgt) for the label check");
        const QString label = files->text();
        QVERIFY2(label.contains(QStringLiteral("open_src.pdf"))
                 && label.contains(QStringLiteral("open_tgt.pdf")),
                 qPrintable(QStringLiteral("files label: %1").arg(label)));
        QVERIFY2(label.contains(QStringLiteral("(2 pp)"))
                 && label.contains(QStringLiteral("(3 pp)")),
                 qPrintable(QStringLiteral("files label page counts: %1").arg(label)));

        // A missing document refuses and keeps the PREVIOUS session —
        // panes, counts and grids all still describe (src, tgt).
        QVERIFY(!mode.setDocuments(src, path("never_there.pdf")));
        QCOMPARE(mode.sourcePath(), src);
        QCOMPARE(mode.targetPath(), tgt);
        QCOMPARE(mode.pageCount(ComposeSide::Source), 2);
        QCOMPARE(mode.pageCount(ComposeSide::Target), 3);
        QCOMPARE(gridOf(mode, ComposeSide::Source)->count(), 2);
        QCOMPARE(gridOf(mode, ComposeSide::Target)->count(), 3);
    }

    // ── 3. Insertion seam + pick order ────────────────────────────────────

    void insertionIndexSeamAndPickOrder()
    {
        // "after page N": N = -1 lands before the first page; k climbs.
        QCOMPARE(ComposeMode::insertionIndexFor(-1, 0), 0);
        QCOMPARE(ComposeMode::insertionIndexFor(-1, 2), 2);
        QCOMPARE(ComposeMode::insertionIndexFor(0, 0), 1);   // after page 1
        QCOMPARE(ComposeMode::insertionIndexFor(0, 1), 2);
        QCOMPARE(ComposeMode::insertionIndexFor(2, 0), 3);   // after page 3
        QCOMPARE(ComposeMode::insertionIndexFor(2, 3), 6);

        ComposeMode mode;
        const QString src = createPagePdf(path("pick_src.pdf"), {"S1", "S2", "S3"});
        const QString tgt = createPagePdf(path("pick_tgt.pdf"), {"T1"});
        QVERIFY(mode.setDocuments(src, tgt));

        mode.addPagePick(ComposeSide::Source, 1);
        mode.addPagePick(ComposeSide::Source, 2);
        mode.addPagePick(ComposeSide::Source, 1);   // duplicate pick collapses
        QCOMPARE(mode.pagePicks(ComposeSide::Source), QList<int>({1, 2}));
        QCOMPARE(mode.pagePicks(ComposeSide::Target), QList<int>());

        mode.removePagePick(ComposeSide::Source, 1);
        QCOMPARE(mode.pagePicks(ComposeSide::Source), QList<int>({2}));
        mode.addPagePick(ComposeSide::Source, 1);
        mode.clearPicks();
        QVERIFY(mode.pagePicks(ComposeSide::Source).isEmpty());

        // Positions are per destination.
        mode.setInsertAfterPage(ComposeSide::Target, 0);
        mode.setInsertAfterPage(ComposeSide::Source, 1);
        QCOMPARE(mode.insertAfterPage(ComposeSide::Target), 0);
        QCOMPARE(mode.insertAfterPage(ComposeSide::Source), 1);
    }

    // ── 4. Page pick → insert: order + position correctness ──────────────

    void pagePickInsertOrderAndPosition()
    {
        const QString src = createPagePdf(path("ins_src.pdf"), {"S1", "S2"});
        const QString tgt = createPagePdf(path("ins_tgt.pdf"), {"T1", "T2", "T3"});
        QVERIFY(!src.isEmpty() && !tgt.isEmpty());
        const QByteArray srcHash = sha256OfFile(src);

        ComposeMode mode;
        QVERIFY(mode.setDocuments(src, tgt));
        QCOMPARE(mode.historyCount(ComposeSide::Target), 0);

        // Pick source pages 2 then 1 (pick order preserved, NOT page order),
        // insert after target page 1 → T1, S2, S1, T2, T3.
        QCOMPARE(gridOf(mode, ComposeSide::Source)->count(), 2);
        gridOf(mode, ComposeSide::Source)->item(1)->setCheckState(Qt::Checked);
        gridOf(mode, ComposeSide::Source)->item(0)->setCheckState(Qt::Checked);
        QCOMPARE(mode.pagePicks(ComposeSide::Source), QList<int>({1, 0}));
        mode.setInsertAfterPage(ComposeSide::Target, 0);

        QString why;
        QVERIFY2(mode.applyTransfers(&why), qPrintable(why));
        QCOMPARE(mode.historyCount(ComposeSide::Target), 1);
        QCOMPARE(mode.pageCount(ComposeSide::Target), 5);

        // Position + order, proven by the text each landed page carries.
        const QStringList texts = pageTexts(tgt, 5);
        QCOMPARE(texts.size(), 5);
        QVERIFY2(texts.at(0).contains(QStringLiteral("T1")),
                 qPrintable(QStringLiteral("page 1: %1").arg(texts.at(0))));
        QVERIFY2(texts.at(1).contains(QStringLiteral("S2")),
                 qPrintable(QStringLiteral("page 2: %1").arg(texts.at(1))));
        QVERIFY2(texts.at(2).contains(QStringLiteral("S1")),
                 qPrintable(QStringLiteral("page 3: %1").arg(texts.at(2))));
        QVERIFY2(texts.at(3).contains(QStringLiteral("T2")),
                 qPrintable(QStringLiteral("page 4: %1").arg(texts.at(3))));
        QVERIFY2(texts.at(4).contains(QStringLiteral("T3")),
                 qPrintable(QStringLiteral("page 5: %1").arg(texts.at(4))));

        // The source is never modified by a transfer aimed at the target.
        QCOMPARE(sha256OfFile(src), srcHash);
        QCOMPARE(mode.pageCount(ComposeSide::Source), 2);

        // Picks consumed; the target pane grid mirrors the composed document.
        QVERIFY(mode.pagePicks(ComposeSide::Source).isEmpty());
        QCOMPARE(gridOf(mode, ComposeSide::Target)->count(), 5);
    }

    // ── 5. Image inventory → pick → placement geometry pin ───────────────

    void imageInventoryPickPlacementGeometryPin()
    {
        QImage stamp(64, 40, QImage::Format_RGBA8888);
        stamp.fill(Qt::transparent);
        QPainter pt(&stamp);
        pt.fillRect(8, 8, 48, 24, QColor(200, 40, 40, 200));
        pt.end();

        const QString src = createImagePdf(path("img_src.pdf"), stamp);
        const QString tgt = createPagePdf(path("img_tgt.pdf"), {"T1", "T2", "T3"});
        QVERIFY(!src.isEmpty() && !tgt.isEmpty());

        // Inventory: dimensions + filters are reported for the embedded image.
        const QList<ComposeImageInfo> inventory =
            ComposeMode::imageInventory(src, 0);
        QVERIFY2(!inventory.isEmpty(), "the image page must inventory its images");
        bool found = false;
        for (const auto& info : inventory) {
            if (info.widthPx > 0 && info.heightPx > 0 && !info.filters.isEmpty()) {
                found = true;
                QVERIFY2(!info.pixels.isNull(),
                         "a decodable embedded image must carry decoded pixels");
                QVERIFY2(!info.xobjectName.isEmpty(),
                         "inventory entries must name their resource");
            }
        }
        QVERIFY2(found, "inventory entries must carry dimensions and filters");

        const ComposeImagePick pick0{
            0, inventory.first().xobjectName,
            QSize(inventory.first().widthPx, inventory.first().heightPx),
            inventory.first().filters};

        ComposeMode mode;
        QVERIFY(mode.setDocuments(src, tgt));
        // The pane image picker surfaces the inventory of the selected page.
        QVERIFY2(imagesOf(mode, ComposeSide::Source)->count() >= 1,
                 "the source image picker must list the page's images");

        // Image-only transfer: no page inserts, the image lands on the first
        // destination page after the (empty) inserted block — target page 2.
        mode.setInsertAfterPage(ComposeSide::Target, 0);
        const QRectF box(100, 100, 120, 80);   // PDF user space, bottom-left y
        mode.setImagePlacementBox(ComposeSide::Target, box);
        mode.addImagePick(ComposeSide::Source, pick0);

        QString why;
        QVERIFY2(mode.applyTransfers(&why), qPrintable(why));
        QCOMPARE(mode.historyCount(ComposeSide::Target), 1);
        QCOMPARE(mode.pageCount(ComposeSide::Target), 3);   // images add no pages

        // Geometry pin: the placement listImages reports on the placed page
        // carries the fitted rect (aspect preserved inside the box) and the
        // native pixel size of the picked object.
        const QRectF expected = ComposeMode::fittedRect(
            QSizeF(pick0.pixelSize), box);
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(tgt));
        const QList<PdfImageInfo> placements = engine.listImages(1);
        QVERIFY2(!placements.isEmpty(), "the target page must carry the placed image");
        bool pinned = false;
        for (const auto& place : placements) {
            if (qAbs(place.placement.width() - expected.width()) < 0.5
                && qAbs(place.placement.height() - expected.height()) < 0.5
                && qAbs(place.placement.left() - expected.left()) < 0.5
                && qAbs(place.placement.top() - expected.top()) < 0.5) {
                pinned = true;
                QCOMPARE(place.widthPx, pick0.pixelSize.width());
                QCOMPARE(place.heightPx, pick0.pixelSize.height());
            }
        }
        QVERIFY2(pinned,
                 qPrintable(QStringLiteral("no placement matches the fitted rect %1 %2 %3 %4")
                                .arg(expected.left()).arg(expected.top())
                                .arg(expected.width()).arg(expected.height())));

        // Honest placement disclosure: names the draw size and the
        // never-stretched contract.
        const QString disclosure =
            ComposeMode::imagePlacementDisclosure(QSizeF(pick0.pixelSize), box);
        QVERIFY2(disclosure.contains(QStringLiteral("aspect"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("image disclosure: %1").arg(disclosure)));
        QVERIFY2(disclosure.contains(QStringLiteral("never stretched"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("image disclosure: %1").arg(disclosure)));
    }

    // ── 6. Transactional save: candidate → commit, refusal is byte-exact ──

    void transactionalSaveRefusalIsByteExact()
    {
        const QString src = createPagePdf(path("tx_src.pdf"), {"S1"});
        const QString tgt = createPagePdf(path("tx_tgt.pdf"), {"T1", "T2"});
        QVERIFY(!src.isEmpty() && !tgt.isEmpty());
        const QByteArray before = sha256OfFile(tgt);

        ComposeMode mode;
        QVERIFY(mode.setDocuments(src, tgt));
        mode.addPagePick(ComposeSide::Source, 0);
        mode.setInsertAfterPage(ComposeSide::Target, 0);

        // Refusal: the commit step faults AFTER the candidate was built —
        // the destination must be byte-identical and NO history step pushed.
        SafeSave::setCommitFaultForTesting(SafeSave::CommitFaultForTesting::FailBeforeCommit);
        QString why;
        QVERIFY2(!mode.applyTransfers(&why),
                 "a faulted commit must refuse the apply");
        QVERIFY2(!why.isEmpty(), "a refusal must explain itself");
        QCOMPARE(sha256OfFile(tgt), before);
        QCOMPARE(mode.historyCount(ComposeSide::Target), 0);
        QCOMPARE(mode.pageCount(ComposeSide::Target), 2);
        SafeSave::setCommitFaultForTesting(SafeSave::CommitFaultForTesting::None);

        // Retry after the fault succeeds (the pick stays pending — retryable).
        QVERIFY2(mode.applyTransfers(&why), qPrintable(why));
        QCOMPARE(mode.historyCount(ComposeSide::Target), 1);
        QCOMPARE(mode.pageCount(ComposeSide::Target), 3);
        QVERIFY(sha256OfFile(tgt) != before);

        // The committed candidate is a complete, valid PDF — a fresh renderer
        // opens it and the inserted page carries the source text.
        const QStringList texts = pageTexts(tgt, 3);
        QCOMPARE(texts.size(), 3);
        QVERIFY(texts.at(1).contains(QStringLiteral("S1")));
    }

    // ── 7. Undo/redo semantics ────────────────────────────────────────────

    void undoRedoSemantics()
    {
        const QString src = createPagePdf(path("undo_src.pdf"), {"S1", "S2"});
        const QString tgt = createPagePdf(path("undo_tgt.pdf"), {"T1", "T2"});
        QVERIFY(!src.isEmpty() && !tgt.isEmpty());

        ComposeMode mode;
        QVERIFY(mode.setDocuments(src, tgt));
        // Empty history refuses honestly.
        QVERIFY(!mode.undoTransfers(ComposeSide::Target));
        QVERIFY(!mode.redoTransfers(ComposeSide::Target));

        mode.addPagePick(ComposeSide::Source, 1);
        mode.setInsertAfterPage(ComposeSide::Target, 0);
        QString why;
        QVERIFY2(mode.applyTransfers(&why), qPrintable(why));
        QCOMPARE(mode.pageCount(ComposeSide::Target), 3);   // 2 + 1 inserted

        // Undo: one checked step restores the page structure exactly.
        QVERIFY(mode.undoTransfers(ComposeSide::Target));
        QCOMPARE(mode.pageCount(ComposeSide::Target), 2);
        const QStringList texts = pageTexts(tgt, 2);
        QVERIFY(texts.at(0).contains(QStringLiteral("T1")));
        QVERIFY(texts.at(1).contains(QStringLiteral("T2")));

        // Redo: re-applies the same transfer.
        QVERIFY(mode.redoTransfers(ComposeSide::Target));
        QCOMPARE(mode.pageCount(ComposeSide::Target), 3);
        const QStringList after = pageTexts(tgt, 3);
        QVERIFY(after.at(1).contains(QStringLiteral("S2")));

        QVERIFY(mode.undoTransfers(ComposeSide::Target));
        QCOMPARE(mode.historyCount(ComposeSide::Target), 1);
    }

    // ── 8. Mismatched page sizes: honest scaling disclosure ──────────────

    void mismatchedSizeDisclosureIsHonest()
    {
        // Equal sizes disclose nothing.
        QVERIFY(ComposeMode::pageSizeDisclosure(QSizeF(612, 792), QSizeF(612, 792))
                    .isEmpty());
        // Tolerated rounding (0.5 pt) discloses nothing either.
        QVERIFY(ComposeMode::pageSizeDisclosure(QSizeF(612.0, 792.0),
                                                QSizeF(612.3, 792.3))
                    .isEmpty());

        // A real mismatch names BOTH sizes and the never-stretched contract.
        const QString d = ComposeMode::pageSizeDisclosure(QSizeF(612, 792),
                                                          QSizeF(595, 842));
        QVERIFY2(d.contains(QStringLiteral("612")), qPrintable(d));
        QVERIFY2(d.contains(QStringLiteral("842")), qPrintable(d));
        QVERIFY2(d.contains(QStringLiteral("595")), qPrintable(d));
        QVERIFY2(d.contains(QStringLiteral("never stretched"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("disclosure: %1").arg(d)));

        // The pending-transfer preview surfaces the disclosure for a real
        // Letter → A4 transfer before anything is applied.
        const QString src = createPagePdf(path("disc_src.pdf"), {"S1"},
                                          QSizeF(612, 792));   // Letter
        const QString tgt = createPagePdf(path("disc_tgt.pdf"), {"T1", "T2"},
                                          QSizeF(595, 842));   // A4
        ComposeMode mode;
        QVERIFY(mode.setDocuments(src, tgt));
        mode.addPagePick(ComposeSide::Source, 0);
        mode.setInsertAfterPage(ComposeSide::Target, 0);

        const QStringList pending = mode.pendingSummary();
        QVERIFY2(!pending.isEmpty(), "a pending pick must preview");
        QString disclosureRow;
        for (const QString& row : pending)
            if (row.contains(QStringLiteral("612"))) disclosureRow = row;
        QVERIFY2(!disclosureRow.isEmpty(),
                 qPrintable(QStringLiteral("pending rows: %1").arg(pending.join(" | "))));
        QVERIFY2(disclosureRow.contains(QStringLiteral("never stretched"),
                                        Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("preview row: %1").arg(disclosureRow)));

        // The composed-order preview shows the pending insertion at its
        // chosen position.
        const QString preview = mode.composedOrderPreview();
        QVERIFY2(preview.contains(QStringLiteral("[S1]")),
                 qPrintable(QStringLiteral("composed order: %1").arg(preview)));
        QVERIFY2(preview.indexOf(QStringLiteral("1")) >= 0
                 && preview.indexOf(QStringLiteral("[S1]"))
                        > preview.indexOf(QStringLiteral("1")),
                 qPrintable(QStringLiteral("composed order: %1").arg(preview)));
    }

    // ── 9. A11y: role + page numbers on every thumbnail surface ──────────

    void a11yNamesCarryRoleAndPageNumbers()
    {
        const QString src = createPagePdf(path("a11y_src.pdf"), {"S1", "S2", "S3"});
        const QString tgt = createPagePdf(path("a11y_tgt.pdf"), {"T1"});
        ComposeMode mode;
        QVERIFY(mode.setDocuments(src, tgt));

        // Pane grids name the document role, file and page count.
        auto* srcGrid = gridOf(mode, ComposeSide::Source);
        auto* tgtGrid = gridOf(mode, ComposeSide::Target);
        QVERIFY(srcGrid && tgtGrid);
        const QString srcName = srcGrid->accessibleName();
        QVERIFY2(srcName.contains(QStringLiteral("Source"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("source grid a11y name: %1").arg(srcName)));
        QVERIFY2(srcName.contains(QStringLiteral("a11y_src.pdf")), qPrintable(srcName));
        QVERIFY2(srcName.contains(QStringLiteral("3")), qPrintable(srcName));
        const QString tgtName = tgtGrid->accessibleName();
        QVERIFY2(tgtName.contains(QStringLiteral("Target"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("target grid a11y name: %1").arg(tgtName)));
        QVERIFY2(tgtName.contains(QStringLiteral("a11y_tgt.pdf")), qPrintable(tgtName));

        // Every thumbnail item discloses role + document + page position.
        const QVariant srcItemA11y =
            srcGrid->item(1)->data(Qt::AccessibleTextRole);
        QVERIFY2(srcItemA11y.isValid(),
                 "each page thumbnail must carry AccessibleTextRole");
        const QString srcItemText = srcItemA11y.toString();
        QVERIFY2(srcItemText.contains(QStringLiteral("Source"), Qt::CaseInsensitive),
                 qPrintable(srcItemText));
        QVERIFY2(srcItemText.contains(QStringLiteral("page 2 of 3")), qPrintable(srcItemText));
        QVERIFY2(srcItemText.contains(QStringLiteral("not picked")), qPrintable(srcItemText));

        // Picking through the real checkbox path updates the disclosure.
        srcGrid->item(1)->setCheckState(Qt::Checked);
        const QString pickedText = srcGrid->item(1)->data(Qt::AccessibleTextRole).toString();
        QVERIFY2(pickedText.contains(QStringLiteral("picked"))
                 && !pickedText.contains(QStringLiteral("not picked")),
                 qPrintable(QStringLiteral("picked a11y text: %1").arg(pickedText)));
        srcGrid->item(1)->setCheckState(Qt::Unchecked);
        QVERIFY2(srcGrid->item(1)->data(Qt::AccessibleTextRole).toString()
                     .contains(QStringLiteral("not picked")),
                 "unpicking must restore the not-picked disclosure");

        // The target grid mirrors the same discipline with its own role.
        const QVariant tgtItemA11y = tgtGrid->item(0)->data(Qt::AccessibleTextRole);
        QVERIFY2(tgtItemA11y.isValid(), "target thumbnails must carry AccessibleTextRole");
        QVERIFY2(tgtItemA11y.toString().contains(QStringLiteral("Target"), Qt::CaseInsensitive),
                 qPrintable(tgtItemA11y.toString()));
        QVERIFY2(tgtItemA11y.toString().contains(QStringLiteral("page 1 of 1")),
                 qPrintable(tgtItemA11y.toString()));

        // The image picker names the page it inventories.
        const QString imgName = imagesOf(mode, ComposeSide::Source)->accessibleName();
        QVERIFY2(imgName.contains(QStringLiteral("Source"), Qt::CaseInsensitive),
                 qPrintable(imgName));
        QVERIFY2(imgName.contains(QStringLiteral("page 1")), qPrintable(imgName));

        // The transfer list announces itself.
        auto* transfer = mode.findChild<QListWidget*>(QStringLiteral("composeTransferList"));
        QVERIFY(transfer);
        QVERIFY2(transfer->accessibleName().contains(QStringLiteral("pending"),
                                                     Qt::CaseInsensitive),
                 qPrintable(transfer->accessibleName()));
    }
};

QTEST_MAIN(TestComposeMode)
#include "TestComposeMode.moc"
