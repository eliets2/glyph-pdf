// SPDX-License-Identifier: Apache-2.0
// Audit 9.8 P0 (DEFECT 1) regression test: RedactMode's "Apply All Redactions"
// must burn in EVERY placed mark (Mark Region / Mark All Occurrences) via the
// mark-based path — NOT re-run the regex and ignore the placed regions.
//
// This pins the authoritative shared boundary (engine->applyMarkRedactions):
// a ToolMode::Redact annotation placed over a secret must excise that content
// from the saved document, exactly as SecurityController's mark-based flow does.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"
#include "core/AnnotationTypes.h"

class TestRedactApplyMarks : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void markRegionContentIsExcised();
    void noMarksReturnsFalse();
    void nonRedactMarksAreIgnored();
private:
    QTemporaryDir m_tmpDir;
    QString createPdfWithText(const QString& name, const QString& text);
};

void TestRedactApplyMarks::initTestCase() {
    QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
}

QString TestRedactApplyMarks::createPdfWithText(const QString& name, const QString& text) {
    const QString path = m_tmpDir.filePath(name);
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
    } catch (const std::exception& e) {
        qWarning() << "createPdfWithText failed:" << e.what();
        return {};
    }
    return path;
}

// The core DEFECT-1 guarantee: a drag-placed region mark (ToolMode::Redact)
// over a secret must excise that secret from the decoded content stream.
void TestRedactApplyMarks::markRegionContentIsExcised() {
    const QString pdf = createPdfWithText("region.pdf", QStringLiteral("SECRET_REGION_DATA"));
    QVERIFY2(!pdf.isEmpty(), "PDF creation failed");

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(pdf));

    // A region mark placed by Mark Region / Mark All Occurrences.
    AnnotationItem mark;
    mark.mode = ToolMode::Redact;
    mark.pageIndex = 0;
    mark.rect = QRectF(40, 690, 300, 30); // covers the drawn text at (50,700)

    QVERIFY2(engine.applyMarkRedactions({mark}),
             qPrintable(engine.lastError().userMessage));

    const QString out = m_tmpDir.filePath("region_redacted.pdf");
    QVERIFY(engine.saveDocument(out));

    // Reload and confirm the secret is gone from ALL decoded streams.
    PoDoFo::PdfMemDocument verifyDoc;
    verifyDoc.Load(out.toUtf8().constData());
    const QByteArray needle("SECRET_REGION_DATA");
    bool leaked = false;
    for (int pi = 0; pi < (int)verifyDoc.GetPages().GetCount(); ++pi) {
        auto& page = verifyDoc.GetPages().GetPageAt(pi);
        auto* co = page.GetContents();
        if (!co) continue;
        PoDoFo::charbuff buf;
        co->CopyTo(buf);
        if (QByteArray(buf.data(), (int)buf.size()).contains(needle))
            leaked = true;
    }
    QVERIFY2(!leaked, "Content under a placed redaction region must be excised");
}

void TestRedactApplyMarks::noMarksReturnsFalse() {
    const QString pdf = createPdfWithText("nomarks.pdf", QStringLiteral("Keep me"));
    QVERIFY(!pdf.isEmpty());
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(pdf));
    QVERIFY2(!engine.applyMarkRedactions({}),
             "applyMarkRedactions with no marks must report failure (no-op is a lie)");
}

void TestRedactApplyMarks::nonRedactMarksAreIgnored() {
    const QString pdf = createPdfWithText("nonredact.pdf", QStringLiteral("Keep me"));
    QVERIFY(!pdf.isEmpty());
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(pdf));
    AnnotationItem highlight;
    highlight.mode = ToolMode::Highlight;
    highlight.pageIndex = 0;
    highlight.rect = QRectF(40, 690, 120, 40);
    QVERIFY2(!engine.applyMarkRedactions({highlight}),
             "Non-redact marks must not count as redaction marks");
}

QTEST_MAIN(TestRedactApplyMarks)
#include "TestRedactApplyMarks.moc"