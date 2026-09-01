// SPDX-License-Identifier: Apache-2.0
// Audit 9.8 P0 regression test: Mark All Occurrences actually places
// redaction marks for every regex match (via PatternRedactor geometry), and
// Mark Region activates the canvas drag-placement mode — the pills are no
// longer decorative toggles.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include <QFile>
#include <QLineEdit>
#include <QRadioButton>
#include <QTextStream>
#include <podofo/podofo.h>
#include "modes/RedactMode.h"
#include "core/AppContext.h"
#include "engines/PdfEditorEngine.h"
#include "ui/PdfViewerWidget.h"

class TestRedactMarkAll : public QObject {
    Q_OBJECT
private slots:
    void markAllPlacesMatchRects();
    void markRegionSetsRedactToolMode();
    // §9.8 F1: an explicit page-range LIST ("1, 3") must mark exactly those
    // pages — never the pages in between (they are irreversibly destroyed on
    // Apply), and an unparseable range must mark nothing at all.
    void rangeMarksExactlyTheListedPages();
    void invalidRangeMarksNothing();
private:
    static QString createPdfWithText(const QTemporaryDir& tmpDir,
                                     const QString& name, const QString& text);
    static QString createNPagePdfWithText(const QTemporaryDir& tmpDir,
                                          const QString& name, int pageCount,
                                          const QString& text);
};
QString TestRedactMarkAll::createPdfWithText(const QTemporaryDir& tmpDir,
                                             const QString& name, const QString& text) {
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        painter.TextState.SetFont(font, 12.0);
        painter.DrawText(text.toUtf8().constData(), 50, 700);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception&) {
        return {};
    }
    return path;
}

void TestRedactMarkAll::markAllPlacesMatchRects() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createPdfWithText(tmp, "emails.pdf",
                                          QStringLiteral("Contact a@b.com or c@d.org today"));
    QVERIFY(!pdf.isEmpty());

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    AppContext ctx;
    gp::RedactMode mode;
    mode.setAppContext(&ctx);
    mode.setViewer(&viewer);

    // Select the built-in Email pattern (combo index 0), whole-document scope.
    auto* combo = mode.findChild<QComboBox*>();
    QVERIFY(combo);
    combo->setCurrentIndex(0);
    QVERIFY(QMetaObject::invokeMethod(&mode, "onMarkAllOccurrences"));

    const auto annos = viewer.annotations();
    int redactCount = 0;
    for (const auto& a : annos)
        if (a.mode == ToolMode::Redact) ++redactCount;
    QFile diag(QStringLiteral("rma_diag.txt"));
    if (diag.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&diag);
        ts << "redactCount=" << redactCount << " total=" << annos.size() << "\n";
    }
    QVERIFY2(redactCount >= 2,
             qPrintable(QStringLiteral("expected >=2 redaction marks for two email addresses, got %1")
                            .arg(redactCount)));
}

void TestRedactMarkAll::markRegionSetsRedactToolMode() {
    PdfViewerWidget viewer;
    gp::RedactMode mode;
    mode.setViewer(&viewer);
    viewer.setToolMode(ToolMode::HandTool);
    QVERIFY(QMetaObject::invokeMethod(&mode, "onMarkRegion"));
    QCOMPARE(viewer.toolMode(), ToolMode::Redact);
}

QString TestRedactMarkAll::createNPagePdfWithText(const QTemporaryDir& tmpDir,
                                                  const QString& name, int pageCount,
                                                  const QString& text) {
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        for (int i = 0; i < pageCount; ++i) {
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText(text.toUtf8().constData(), 50, 700);
            painter.FinishDrawing();
        }
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception&) {
        return {};
    }
    return path;
}

namespace {
// Select the "Page range:" scope radio and return the range expression edit.
// Identified by its placeholder ("e.g. 1-3, 5, 7-9") — findChild<QLineEdit*>
// alone would grab the custom-regex edit created earlier in buildUi.
QLineEdit* selectRangeScope(gp::RedactMode& mode) {
    const auto radios = mode.findChildren<QRadioButton*>();
    for (QRadioButton* r : radios) {
        if (r->text().contains(QStringLiteral("Page range"))) {
            r->setChecked(true);
            break;
        }
    }
    const auto edits = mode.findChildren<QLineEdit*>();
    for (QLineEdit* e : edits)
        if (e->placeholderText().contains(QStringLiteral("1-3")))
            return e;
    return nullptr;
}
} // namespace

void TestRedactMarkAll::rangeMarksExactlyTheListedPages() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createNPagePdfWithText(tmp, "five_pages.pdf", 5,
                                               QStringLiteral("Secret a@b.com"));
    QVERIFY(!pdf.isEmpty());

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(pdf));
    AppContext ctx;
    // Non-owning: the engine is stack-allocated, the context only borrows it.
    ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(&engine, [](IPdfEditorEngine*){});

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    gp::RedactMode mode;
    mode.setAppContext(&ctx);
    mode.setViewer(&viewer);

    auto* combo = mode.findChild<QComboBox*>();
    QVERIFY(combo);
    combo->setCurrentIndex(0); // built-in Email pattern

    QLineEdit* rangeEdit = selectRangeScope(mode);
    QVERIFY2(rangeEdit && rangeEdit->isEnabled(), "range edit must be enabled by the Page range scope");
    rangeEdit->setText(QStringLiteral("1, 3")); // 1-based pages 1 and 3 → indices {0, 2}

    QVERIFY(QMetaObject::invokeMethod(&mode, "onMarkAllOccurrences"));

    int marksOn[5] = {0, 0, 0, 0, 0};
    for (const auto& a : viewer.annotations())
        if (a.mode == ToolMode::Redact && a.pageIndex >= 0 && a.pageIndex < 5)
            ++marksOn[a.pageIndex];
    QVERIFY2(marksOn[0] >= 1, "page 1 was explicitly selected and must be marked");
    QVERIFY2(marksOn[2] >= 1, "page 3 was explicitly selected and must be marked");
    QVERIFY2(marksOn[1] == 0 && marksOn[3] == 0 && marksOn[4] == 0,
             qPrintable(QStringLiteral("pages 2/4/5 were NOT selected — marking them would "
                                      "destroy unselected content on Apply (got %1,%2,%3)")
                            .arg(marksOn[1]).arg(marksOn[3]).arg(marksOn[4])));
}

void TestRedactMarkAll::invalidRangeMarksNothing() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createNPagePdfWithText(tmp, "three_pages.pdf", 3,
                                               QStringLiteral("Secret a@b.com"));
    QVERIFY(!pdf.isEmpty());

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(pdf));
    AppContext ctx;
    ctx.pdfEditor = std::shared_ptr<IPdfEditorEngine>(&engine, [](IPdfEditorEngine*){});

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    gp::RedactMode mode;
    mode.setAppContext(&ctx);
    mode.setViewer(&viewer);

    auto* combo = mode.findChild<QComboBox*>();
    QVERIFY(combo);
    combo->setCurrentIndex(0);

    QLineEdit* rangeEdit = selectRangeScope(mode);
    QVERIFY(rangeEdit);
    rangeEdit->setText(QStringLiteral("abc")); // unparseable → invalid sentinel

    QVERIFY(QMetaObject::invokeMethod(&mode, "onMarkAllOccurrences"));
    for (const auto& a : viewer.annotations())
        QVERIFY2(a.mode != ToolMode::Redact,
                 "an invalid range must mark nothing — falling through to 'all pages' "
                 "silently selects every page for irreversible destruction");
}
QTEST_MAIN(TestRedactMarkAll)
#include "TestRedactMarkAll.moc"
