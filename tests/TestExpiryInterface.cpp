// SPDX-License-Identifier: Apache-2.0
// §9.11 regression test: setExpiryDate must be reachable through the
// IPdfEditorEngine interface (IPdfDocumentIO role) — SecurityController used to
// reach it via dynamic_cast<PdfEditorEngine*>, a layer violation that also made
// the feature untestable with a mock engine.
#include <QtTest/QtTest>
#include "core/interfaces/IPdfEditorEngine.h"
#include "mocks/MockPdfEditorEngine.h"
#include "engines/PdfEditorEngine.h"

#include <QFile>
#include <QTemporaryDir>

class TestExpiryInterface : public QObject {
    Q_OBJECT
private slots:
    // ── Interface-level: the mock stands in for any engine ──
    void callableThroughInterfacePointer();
    void rejectsInvalidDate();
    void requiresLoadedDocument();
    // ── Engine-level: the real XMP marker round-trips ──
    void writesAndReadsBackExpiryMarker();
};
void TestExpiryInterface::callableThroughInterfacePointer() {
    MockPdfEditorEngine mock;
    mock.m_loaded = true;
    // Deliberately hold the engine through the interface only.
    IPdfEditorEngine* engine = &mock;
    const QDate d(2026, 4, 1);
    QVERIFY(engine->setExpiryDate(QStringLiteral("in.pdf"), d, QStringLiteral("in.pdf")));
    QCOMPARE(mock.m_expiryCalls, 1);
    QCOMPARE(mock.m_lastExpiryDate, d);
    QCOMPARE(mock.m_lastExpiryPath, QStringLiteral("in.pdf"));
    QCOMPARE(mock.m_lastExpiryOut, QStringLiteral("in.pdf"));
}

void TestExpiryInterface::rejectsInvalidDate() {
    MockPdfEditorEngine mock;
    mock.m_loaded = true;
    IPdfEditorEngine* engine = &mock;
    QVERIFY(!engine->setExpiryDate(QStringLiteral("in.pdf"), QDate(), QStringLiteral("in.pdf")));
    QCOMPARE(mock.m_expiryCalls, 1); // reached the engine; engine refused
}

void TestExpiryInterface::requiresLoadedDocument() {
    MockPdfEditorEngine mock; // m_loaded == false
    IPdfEditorEngine* engine = &mock;
    QVERIFY(!engine->setExpiryDate(QStringLiteral("in.pdf"), QDate(2026, 4, 1),
                                   QStringLiteral("in.pdf")));
}

void TestExpiryInterface::writesAndReadsBackExpiryMarker() {
    // Minimal single-page PDF (same fixture pattern as TestIntegration).
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString in = dir.filePath(QStringLiteral("in.pdf"));
    QFile f(in);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(
        "%PDF-1.4\n"
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
        "xref\n0 4\n"
        "0000000000 65535 f \n"
        "0000000009 00000 n \n"
        "0000000058 00000 n \n"
        "0000000115 00000 n \n"
        "trailer<</Size 4/Root 1 0 R>>\n"
        "startxref\n183\n%%EOF\n");
    f.close();

    PdfEditorEngine engine;
    const QDate d(2026, 4, 1);
    // In-place write, exactly as SecurityController::setExpiryDocument does.
    QVERIFY(engine.setExpiryDate(in, d, in));
    QCOMPARE(PdfEditorEngine::readExpiryDate(in), d);
}

QTEST_GUILESS_MAIN(TestExpiryInterface)
#include "TestExpiryInterface.moc"
