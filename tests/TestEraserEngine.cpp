// SPDX-License-Identifier: Apache-2.0
// Eraser on the real engine (Wave 1B port from feature/editing-parity
// 97172fbe). PdfEditorEngine::deleteObjectAt excises the content under the
// click and commits it. Judged by TEXT EXTRACTION of the saved file — a raw
// byte search would pass vacuously once content streams are compressed.
//   1. the text under the click no longer extracts; a line elsewhere survives;
//   2. an empty page is a harmless success, like redacting one
//      (TestRedaction::testRedactionOnEmptyPage);
//   3. a page index past the end fails and leaves the file untouched.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QPdfDocument>
#include <QPdfSelection>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"

namespace {

// A4 page, Helvetica 12: `first` at (100,700), `second` at (100,500).
QString makeTextPdf(const QString &dir, const QString &name, const char *first,
                    const char *second = nullptr)
{
    const QString path = dir + QLatin1Char('/') + name;
    PoDoFo::PdfMemDocument doc;
    auto &page = doc.GetPages().CreatePage(
        PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
    if (first) {
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        painter.TextState.SetFont(
            doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica), 12.0);
        painter.DrawText(first, 100, 700);
        if (second) painter.DrawText(second, 100, 500);
        painter.FinishDrawing();
    }
    doc.Save(path.toStdString());
    return path;
}

QString pageText(const QString &path)
{
    QPdfDocument pdf;
    if (pdf.load(path) != QPdfDocument::Error::None) return QStringLiteral("<unloadable>");
    return pdf.getAllText(0).text();
}

QByteArray fileBytes(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

} // namespace

class TestEraserEngine : public QObject {
    Q_OBJECT
private slots:
    void eraserRemovesTheTextUnderTheClick();
    void eraserOnAnEmptyPageIsAHarmlessSuccess();
    void eraserOnAMissingPageFailsAndChangesNothing();
};

void TestEraserEngine::eraserRemovesTheTextUnderTheClick()
{
    QTemporaryDir dir;
    const QString f = makeTextPdf(dir.path(), "erase.pdf", "SECRETERASE", "KEEPTHISLINE");
    const QString before = pageText(f);
    QVERIFY2(before.contains(QLatin1String("SECRETERASE")), qPrintable(before));

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    // Qt top-left user space: the (100,700) baseline sits at y = 842 - 700.
    QVERIFY(engine.deleteObjectAt(0, QPointF(150, 842 - 700 - 3)));

    const QString after = pageText(f);
    QVERIFY2(!after.contains(QLatin1String("SECRETERASE")),
             qPrintable(QStringLiteral("erased text still extracts: %1").arg(after)));
    QVERIFY2(after.contains(QLatin1String("KEEPTHISLINE")),
             qPrintable(QStringLiteral("a line far from the click must survive: %1").arg(after)));
}

void TestEraserEngine::eraserOnAnEmptyPageIsAHarmlessSuccess()
{
    QTemporaryDir dir;
    const QString f = makeTextPdf(dir.path(), "empty.pdf", nullptr);
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    QVERIFY(engine.deleteObjectAt(0, QPointF(150, 145)));
    QCOMPARE(pageText(f), QString());
}

void TestEraserEngine::eraserOnAMissingPageFailsAndChangesNothing()
{
    QTemporaryDir dir;
    const QString f = makeTextPdf(dir.path(), "onepage.pdf", "TEXT");
    const QByteArray before = fileBytes(f);
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(f));
    QVERIFY(!engine.deleteObjectAt(5, QPointF(150, 145)));
    QCOMPARE(fileBytes(f), before);
}

QTEST_MAIN(TestEraserEngine)
#include "TestEraserEngine.moc"
