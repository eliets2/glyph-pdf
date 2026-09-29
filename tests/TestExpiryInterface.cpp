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
    // PoDoFo stamps /ModDate on every Save and, when the date changed,
    // re-serializes its XMP packet over the /Metadata stream. A document last
    // saved in an earlier second — every real in-place use — must keep the
    // marker; it used to survive only when both saves shared one wall-clock
    // second (the TestReadOnlyGate / TestCommandBinding load flake).
    void markerSurvivesTheSaveTimeModDateRefresh();
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

void TestExpiryInterface::markerSurvivesTheSaveTimeModDateRefresh() {
    // Single-page PDF whose /Info carries an old /ModDate, so the save inside
    // setExpiryDate always takes PoDoFo's "ModDate changed → re-sync XMP" path.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString in = dir.filePath(QStringLiteral("dated.pdf"));
    const QList<QByteArray> objects = {
        "<</Type/Catalog/Pages 2 0 R>>",
        "<</Type/Pages/Kids[3 0 R]/Count 1>>",
        "<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>",
        "<</Producer(synthetic)/ModDate(D:20200101000000Z)>>",
    };
    QByteArray pdf = "%PDF-1.4\n";
    QList<qint64> offsets;
    for (int i = 0; i < objects.size(); ++i) {
        offsets.append(pdf.size());
        pdf += QByteArray::number(i + 1) + " 0 obj" + objects.at(i) + "endobj\n";
    }
    const qint64 xref = pdf.size();
    pdf += "xref\n0 " + QByteArray::number(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (qint64 off : offsets)
        pdf += QByteArray::number(off).rightJustified(10, '0') + " 00000 n \n";
    pdf += "trailer<</Size " + QByteArray::number(objects.size() + 1)
         + "/Root 1 0 R/Info 4 0 R>>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    QFile f(in);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(pdf);
    f.close();

    PdfEditorEngine engine;
    const QDate d(2026, 4, 1);
    // In place, exactly as SecurityController::setExpiryDocument does.
    QVERIFY(engine.setExpiryDate(in, d, in));
    QCOMPARE(PdfEditorEngine::readExpiryDate(in), d);
    // A second in-place write — the source now carries an XMP packet with the
    // marker — replaces the marker, and the document stays intact whatever
    // the outcome: PoDoFo reads objects on demand from the open source, so the
    // old direct Save() onto that same path wrote over bytes it still read.
    const QDate later(2027, 1, 15);
    const bool second = engine.setExpiryDate(in, later, in);
    PdfEditorEngine reopen;
    QVERIFY2(reopen.loadDocumentForEditing(in), "the document must survive the in-place write intact");
    QVERIFY(second);
    QCOMPARE(PdfEditorEngine::readExpiryDate(in), later);
}

QTEST_GUILESS_MAIN(TestExpiryInterface)
#include "TestExpiryInterface.moc"
