// SPDX-License-Identifier: Apache-2.0
// Audit 9.8 P0 regression test: Mark All Occurrences actually places
// redaction marks for every regex match (via PatternRedactor geometry), and
// Mark Region activates the canvas drag-placement mode — the pills are no
// longer decorative toggles.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QComboBox>
#include <QFile>
#include <QTextStream>
#include <podofo/podofo.h>
#include "modes/RedactMode.h"
#include "core/AppContext.h"
#include "ui/PdfViewerWidget.h"

class TestRedactMarkAll : public QObject {
    Q_OBJECT
private slots:
    void markAllPlacesMatchRects();
    void markRegionSetsRedactToolMode();
private:
    static QString createPdfWithText(const QTemporaryDir& tmpDir,
                                     const QString& name, const QString& text);
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
QTEST_MAIN(TestRedactMarkAll)
#include "TestRedactMarkAll.moc"
