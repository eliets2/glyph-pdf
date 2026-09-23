// SPDX-License-Identifier: Apache-2.0
// T2-9: auto-bookmarks from text styles.
//
// Pins the full honest pipeline:
//   * line grouping + the disclosed heuristics (font size vs body median,
//     bold styling, TOC dot-leader pattern, repeated-header filter),
//   * tree nesting from detected levels,
//   * the outline WRITE seam (IOutlineEditor): a real committed write,
//     read back from the SAVED file with titles AND page targets intact,
//   * the preview dialog over a real fixture: candidates listed, titles
//     editable, rows uncheckable — and the confirmed selection is exactly
//     what gets written,
//   * undoability: SetOutlineCommand restores the previous outline (and the
//     re-push re-applies), all verified by reading the file back.

#include <QtTest>
#include <QUndoStack>
#include <QTableWidget>
#include <QLabel>
#include <QTimer>
#include <QApplication>
#include <podofo/podofo.h>

#include "engines/HeadingOutlineDetector.h"
#include "engines/PdfEditorEngine.h"
#include "commands/SetOutlineCommand.h"
#include "ui/AutoBookmarkDialog.h"
#include "mocks/MockPdfEditorEngine.h"
#include "GpMainWindow.h"
#include "app/Bootstrapper.h"
#include "shell/controllers/EditController.h"

#ifdef DrawText
#undef DrawText
#endif

namespace {

void drawLine(PoDoFo::PdfPage& page, PoDoFo::PdfDocument& doc,
              const char* text, double size, double y) {
    PoDoFo::PdfPainter painter;
    painter.SetCanvas(page);
    auto& font = doc.GetFonts().GetStandard14Font(
        size > 12.0 ? PoDoFo::PdfStandard14FontType::HelveticaBold
                    : PoDoFo::PdfStandard14FontType::Helvetica);
    painter.TextState.SetFont(font, size);
    painter.DrawText(text, 60, y);
    painter.FinishDrawing();
}

// Two pages: body-size (10pt) text plus one 18pt heading per page.
QString createTwoPageDoc(const QTemporaryDir& tmpDir, const QString& name) {
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        {
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            drawLine(page, doc, "GlyphPDF quarterly report", 10.0, 780);
            drawLine(page, doc, "Chapter One", 18.0, 740);
            drawLine(page, doc, "Body text continues here with plain prose.", 10.0, 700);
        }
        {
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            drawLine(page, doc, "Chapter Two", 18.0, 760);
            drawLine(page, doc, "More body text on the second page.", 10.0, 720);
        }
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception& e) {
        qWarning("fixture failed: %s", e.what());
        return {};
    }
    return path;
}

int countEntries(const QList<OutlineEntry>& entries) {
    int n = entries.size();
    for (const auto& e : entries) n += countEntries(e.children);
    return n;
}

// Recursive equality (OutlineEntry has no operator==; Qt containers compare
// element-wise when the element type does).
bool sameOutline(const QList<OutlineEntry>& a, const QList<OutlineEntry>& b) {
    if (a.size() != b.size()) return false;
    for (int i = 0; i < a.size(); ++i) {
        if (a[i].title != b[i].title || a[i].targetPage != b[i].targetPage
            || !sameOutline(a[i].children, b[i].children))
            return false;
    }
    return true;
}

} // namespace

class TestAutoBookmarks : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
    }

    // ── grouping + heuristics (pure, deterministic) ──────────────────────

    void groupRunsMergesSameBaseline() {
        QList<PdfiumBackend::TextRun> runs;
        PdfiumBackend::TextRun a;
        a.text = QStringLiteral("Chapter");
        a.rect = QRectF(60, 100, 60, 10);
        a.fontSize = 18;
        a.fontName = QStringLiteral("Arial-BoldMT");
        runs.append(a);
        PdfiumBackend::TextRun b;
        b.text = QStringLiteral("One");
        b.rect = QRectF(125, 100, 30, 10);
        b.fontSize = 18;
        b.fontName = QStringLiteral("Arial-BoldMT");
        runs.append(b);

        const auto lines = HeadingOutlineDetector::groupRuns(runs);
        QCOMPARE(lines.size(), 1);
        QCOMPARE(lines.first().text, QStringLiteral("Chapter One"));
        QVERIFY(lines.first().bold);
        QCOMPARE(lines.first().maxSize, 18.0);
    }

    void detectFindsSizeBoldTocAndFiltersRepeats() {
        QList<QList<HeadingOutlineDetector::TextLine>> pages;
        pages.append({
            { QStringLiteral("GlyphPDF quarterly report"), QRectF(0, 0, 300, 10), 10.0, false },
            { QStringLiteral("Chapter One"),               QRectF(0, 40, 200, 18), 18.0, true },
            { QStringLiteral("Body prose with words."),    QRectF(0, 80, 300, 10), 10.0, false },
        });
        pages.append({
            { QStringLiteral("Company Report"),            QRectF(0, 0, 200, 10), 10.0, false },
            { QStringLiteral("Chapter Two"),               QRectF(0, 40, 200, 18), 18.0, true },
            { QStringLiteral("Introduction . . . . . 2"),  QRectF(0, 80, 300, 10), 10.0, false },
        });
        pages.append({
            { QStringLiteral("Company Report"),            QRectF(0, 0, 200, 10), 10.0, false },
            { QStringLiteral("Bold low heading"),          QRectF(0, 40, 200, 11), 11.0, true },
            { QStringLiteral("Small body prose line."),    QRectF(0, 80, 300, 10), 10.0, false },
        });

        const auto out = HeadingOutlineDetector::detect(pages);

        QList<QString> titles;
        for (const auto& c : out) titles << c.title;
        QVERIFY2(titles.contains(QStringLiteral("Chapter One")), qPrintable(titles.join('|')));
        QVERIFY2(titles.contains(QStringLiteral("Chapter Two")), qPrintable(titles.join('|')));
        QVERIFY2(titles.contains(QStringLiteral("Introduction")),
                 "The TOC dot-leader line must yield a candidate titled 'Introduction'");
        QVERIFY2(titles.contains(QStringLiteral("Bold low heading")),
                 "Bold body-size text must be detected");
        QVERIFY2(!titles.contains(QStringLiteral("Company Report")),
                 "Repeated running headers must be filtered out");
        QVERIFY2(!titles.contains(QStringLiteral("Small body prose line.")),
                 "Plain body text must not become a bookmark");

        for (const auto& c : out) {
            if (c.title == QStringLiteral("Chapter One")) {
                QCOMPARE(c.level, 1);          // 18pt ≈ 1.8× body 10pt
                QCOMPARE(c.pageIndex, 0);
                QVERIFY(!c.fromToc);
            }
            if (c.title == QStringLiteral("Introduction")) {
                QVERIFY(c.fromToc);
                QCOMPARE(c.tocTargetPage, 1);  // "2" → 0-based page 1
            }
            if (c.title == QStringLiteral("Bold low heading"))
                QCOMPARE(c.level, 3);          // 11/10 → weakest tier
        }
    }

    void buildOutlineTreeNestsLevels() {
        QList<HeadingOutlineDetector::Candidate> in = {
            { QStringLiteral("Part I"),   0, 1, 18, false, -1 },
            { QStringLiteral("Sec 1.1"),  0, 2, 14, false, -1 },
            { QStringLiteral("Sec 1.2"),  1, 2, 14, false, -1 },
            { QStringLiteral("Part II"),  1, 1, 18, false, -1 },
        };
        const auto tree = HeadingOutlineDetector::buildOutlineTree(in);
        QCOMPARE(tree.size(), 2);
        QCOMPARE(tree[0].title, QStringLiteral("Part I"));
        QCOMPARE(tree[0].children.size(), 2);
        QCOMPARE(tree[0].children[1].title, QStringLiteral("Sec 1.2"));
        QCOMPARE(tree[1].title, QStringLiteral("Part II"));
        QCOMPARE(tree[1].children.size(), 0);
    }

    // ── the outline write seam: committed, read back from disk ───────────

    void outlineWriteRoundTripsThroughSavedFile() {
        const QString path = createTwoPageDoc(m_tmpDir, QStringLiteral("outline.pdf"));
        QVERIFY2(!path.isEmpty(), "fixture creation failed");

        PdfEditorEngine engine;
        // The engine contract is load-then-mutate (production always has the
        // document open); getOutline reads the committed file via a throwaway
        // doc (E-1 COW), so it reflects disk either way.
        QVERIFY2(engine.loadDocumentForEditing(path), "engine load failed");
        // No outline yet on a fresh document.
        QVERIFY(engine.getOutline(path).isEmpty());

        QList<OutlineEntry> entries;
        OutlineEntry one;
        one.title = QStringLiteral("Chapter One");
        one.targetPage = 0;
        OutlineEntry child;
        child.title = QStringLiteral("Section");
        child.targetPage = 1;
        one.children.append(child);
        OutlineEntry two;
        two.title = QStringLiteral("Chapter Two");
        two.targetPage = 1;
        entries.append(one);
        entries.append(two);

        QVERIFY2(engine.replaceOutline(path, entries),
                 "replaceOutline must commit in one write");
        const QList<OutlineEntry> readBack = engine.getOutline(path);
        QVERIFY2(sameOutline(readBack, entries),
                 "titles and page targets must survive the save/reload roundtrip");
        QCOMPARE(countEntries(readBack), 3);
    }

    void outlineWriteRejectsOutOfRangeTargets() {
        const QString path = createTwoPageDoc(m_tmpDir, QStringLiteral("outline_bad.pdf"));
        QVERIFY2(!path.isEmpty(), "fixture creation failed");
        PdfEditorEngine engine;
        // Load for real — an unloaded engine would refuse for the wrong
        // reason (no backend) and make the refusal assertion vacuous.
        QVERIFY2(engine.loadDocumentForEditing(path), "engine load failed");

        OutlineEntry bad;
        bad.title = QStringLiteral("Beyond the end");
        bad.targetPage = 99;
        QVERIFY2(!engine.replaceOutline(path, { bad }),
                 "out-of-range targets must refuse the whole write");
        QVERIFY2(engine.getOutline(path).isEmpty(),
                 "a refused write must leave the document unchanged");
    }

    void outlineCommitIsUndoable() {
        const QString path = createTwoPageDoc(m_tmpDir, QStringLiteral("undo.pdf"));
        QVERIFY2(!path.isEmpty(), "fixture creation failed");
        PdfEditorEngine engine;
        QVERIFY2(engine.loadDocumentForEditing(path), "engine load failed");
        const QList<OutlineEntry> previous = engine.getOutline(path);   // empty

        QList<OutlineEntry> entries;
        OutlineEntry one;
        one.title = QStringLiteral("Chapter One");
        one.targetPage = 0;
        entries.append(one);

        QVERIFY(engine.replaceOutline(path, entries));

        QUndoStack stack;
        stack.push(new SetOutlineCommand(&engine, nullptr, nullptr, path,
                                         previous, entries));
        // Push re-applies the same tree (idempotent), file still holds it.
        QVERIFY(sameOutline(engine.getOutline(path), entries));

        // Undo restores the pre-change outline — verified from disk.
        stack.undo();
        QVERIFY2(engine.getOutline(path).isEmpty(),
                 "undo must restore the previous (empty) outline");

        // And redo re-applies the auto-bookmarks.
        stack.redo();
        QVERIFY(sameOutline(engine.getOutline(path), entries));
    }

    // ── packa-F3: one user action = ONE committed outline write ───────────
    //
    // Base defect (re-confirmed at tip 586d6e4): runAutoBookmarks committed
    // the outline directly AND pushed SetOutlineCommand whose initial redo
    // wrote the same outline again — two full path-based saves per run. The
    // fix gives the producer single-writer ownership: the command's first
    // redo only reloads. Driven through the REAL controller + window with a
    // counting engine, so the write count is the committed artifact count.

    void autoBookmarkFirstApplyWritesExactlyOnce() {
        const QString path = createTwoPageDoc(
            m_tmpDir, QStringLiteral("single_write.pdf"));
        QVERIFY2(!path.isEmpty(), "fixture creation failed");

        AppContext ctx = Bootstrapper::createContext();
        auto mock = std::make_shared<MockPdfEditorEngine>();
        mock->m_loaded = true;
        mock->m_outline = {};   // snapshot the controller will read: empty
        ctx.pdfEditor = mock;   // inject BEFORE the window copies the context
        gp::MainWindow win(ctx);
        win.show();
        win.openDocument(path);
        QTRY_COMPARE_WITH_TIMEOUT(win.pdfViewer()->pageCount(), 2, 20000);
        auto* edit = win.findChild<gp::EditController*>();
        QVERIFY2(edit, "the window must own the canonical EditController");

        // Accept the preview dialog deterministically: queued pick inside
        // the modal's nested loop (no sleeps). Rows default to checked.
        QTimer::singleShot(0, [] {
            if (auto* dlg = qobject_cast<AutoBookmarkDialog*>(
                    QApplication::activeModalWidget())) {
                QMetaObject::invokeMethod(dlg, "accept");
            }
        });
        edit->runAutoBookmarks();

        QVERIFY2(mock->m_lastOutline.size() >= 1,
                 "the confirmed candidates must reach the engine");
        QCOMPARE(mock->m_outlineWrites, 1);   // packa-F3: exactly ONE write

        // Undo restores the previous (empty) outline — one more write.
        ctx.undoStack->undo();
        QCOMPARE(mock->m_outlineWrites, 2);
        QCOMPARE(mock->m_lastOutline.size(), 0);

        // Redo re-applies — third write, entries back.
        ctx.undoStack->redo();
        QCOMPARE(mock->m_outlineWrites, 3);
        QVERIFY(mock->m_lastOutline.size() >= 1);
    }

    // packa-F3 failure path: a failed first write must leave NO history
    // entry (no false "Create auto-bookmarks" undo step for a write that
    // never landed) and exactly one attempted write.
    void autoBookmarkFailedWriteCreatesNoHistoryEntry() {
        const QString path = createTwoPageDoc(
            m_tmpDir, QStringLiteral("failed_write.pdf"));
        QVERIFY2(!path.isEmpty(), "fixture creation failed");

        AppContext ctx = Bootstrapper::createContext();
        auto mock = std::make_shared<MockPdfEditorEngine>();
        mock->m_loaded = true;
        mock->m_outlineFails = true;   // the committed write fails
        ctx.pdfEditor = mock;
        gp::MainWindow win(ctx);
        win.show();
        win.openDocument(path);
        QTRY_COMPARE_WITH_TIMEOUT(win.pdfViewer()->pageCount(), 2, 20000);
        auto* edit = win.findChild<gp::EditController*>();
        QVERIFY(edit);
        QCOMPARE(ctx.undoStack->count(), 0);

        QTimer::singleShot(0, [] {
            if (auto* dlg = qobject_cast<AutoBookmarkDialog*>(
                    QApplication::activeModalWidget())) {
                QMetaObject::invokeMethod(dlg, "accept");
            }
        });
        edit->runAutoBookmarks();

        QCOMPARE(mock->m_outlineWrites, 1);   // one attempted, refused write
        QCOMPARE(ctx.undoStack->count(), 0);  // no false history entry
    }

    // ── the preview dialog over a real fixture ────────────────────────────

    void dialogPreviewsAndCommitsEditedSelection() {
        const QString path = createTwoPageDoc(m_tmpDir, QStringLiteral("dialog.pdf"));
        QVERIFY2(!path.isEmpty(), "fixture creation failed");

        AutoBookmarkDialog dialog(path);
        auto* table = dialog.findChild<QTableWidget*>(QStringLiteral("abTable"));
        QVERIFY(table);
        QVERIFY2(table->rowCount() >= 2,
                 qPrintable(QString("expected >= 2 heading candidates, got %1")
                                .arg(table->rowCount())));

        // Titles are editable: retitle the first candidate.
        table->item(0, 1)->setText(QStringLiteral("Retitled Chapter"));
        // Uncheck the second candidate.
        table->item(1, 0)->setCheckState(Qt::Unchecked);

        const auto accepted = dialog.acceptedCandidates();
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(accepted.first().title, QStringLiteral("Retitled Chapter"));

        const auto tree = dialog.buildTree();
        QCOMPARE(tree.size(), 1);
        QCOMPARE(tree.first().title, QStringLiteral("Retitled Chapter"));

        // The honesty disclosure label is part of the dialog.
        QVERIFY(dialog.findChild<QLabel*>(QStringLiteral("abDisclosure")) != nullptr);
        QVERIFY2(dialog.candidateCountText().contains(QStringLiteral("candidate")),
                 qPrintable(dialog.candidateCountText()));

        // The CONFIRMED selection is exactly what gets written — commit it
        // and read it back from the saved file.
        PdfEditorEngine engine;
        QVERIFY2(engine.loadDocumentForEditing(path), "engine load failed");
        QVERIFY(engine.replaceOutline(path, tree));
        const QList<OutlineEntry> readBack = engine.getOutline(path);
        QCOMPARE(readBack.size(), 1);
        QCOMPARE(readBack.first().title, QStringLiteral("Retitled Chapter"));
        QCOMPARE(readBack.first().targetPage, accepted.first().pageIndex);
    }
};

QTEST_MAIN(TestAutoBookmarks)
#include "TestAutoBookmarks.moc"
