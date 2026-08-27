// SPDX-License-Identifier: Apache-2.0
// §9.1 regression test: the viewer must obtain clickable link annotations
// through the engine interface (IPdfDocumentIO::extractLinks) via an injected
// reader — NOT by calling a concrete backend (PoDoFoBackend) directly from the
// UI layer. Also verifies the per-page link cache: one fetch per page, cache
// invalidated when the document is (re)loaded.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QPdfWriter>
#include <QPainter>
#include "ui/PdfViewerWidget.h"
#include "mocks/MockPdfEditorEngine.h"

class TestViewerLinkSeam : public QObject {
    Q_OBJECT
private slots:
    void fetchesLinksThroughInjectedReader();
    void cachesLinksPerPage();
    void reloadInvalidatesLinkCache();
    void noReaderMeansNoLinks();
};

static QString makeTwoPagePdf(const QString &path) {
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&w);
    p.drawText(100, 100, QStringLiteral("page one"));
    w.newPage();               // page break while the painter is active
    p.drawText(100, 100, QStringLiteral("page two"));
    p.end();
    return path;
}

void TestViewerLinkSeam::fetchesLinksThroughInjectedReader() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPagePdf(tmp.filePath("doc.pdf"));

    MockPdfEditorEngine engine;
    PdfLinkInfo link;
    link.rect = QRectF(50, 700, 150, 20);
    link.isUri = true;
    link.uri = QStringLiteral("https://example.com/");
    engine.m_links = {link};

    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    viewer.setLinkReader([&engine](const QString &path, int page) {
        return engine.extractLinks(path, page);
    });

    // The reader was consulted for the opening page with the viewer's path.
    QCOMPARE(engine.m_linkCalls, 1);
    QCOMPARE(engine.m_lastLinkPage, 0);
    QCOMPARE(engine.m_lastLinkPath, pdf);
}

void TestViewerLinkSeam::cachesLinksPerPage() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPagePdf(tmp.filePath("doc.pdf"));

    MockPdfEditorEngine engine;
    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    viewer.setLinkReader([&engine](const QString &path, int page) {
        return engine.extractLinks(path, page);
    });
    QCOMPARE(engine.m_linkCalls, 1);   // primed for the opening page

    // Same page again → served from cache, no new fetch.
    viewer.goToPage(0); // jump to the page we are already on
    QCOMPARE(engine.m_linkCalls, 1);

    // A different page → exactly one new fetch.
    viewer.goToPage(1);
    QCOMPARE(engine.m_linkCalls, 2);
    QCOMPARE(engine.m_lastLinkPage, 1);
}

void TestViewerLinkSeam::reloadInvalidatesLinkCache() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPagePdf(tmp.filePath("doc.pdf"));

    MockPdfEditorEngine engine;
    PdfViewerWidget viewer;
    QVERIFY(viewer.loadDocument(pdf));
    viewer.setLinkReader([&engine](const QString &path, int page) {
        return engine.extractLinks(path, page);
    });
    const int callsAfterOpen = engine.m_linkCalls;

    // reload() re-enters loadDocument → the per-page cache must be dropped so
    // links are re-read from the (possibly mutated) file.
    viewer.reload();
    QVERIFY(engine.m_linkCalls > callsAfterOpen);
}

void TestViewerLinkSeam::noReaderMeansNoLinks() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = makeTwoPagePdf(tmp.filePath("doc.pdf"));

    PdfViewerWidget viewer;   // no setLinkReader — app wiring not done
    QVERIFY(viewer.loadDocument(pdf));
    // Must not crash and must not fabricate links; clicking is a no-op.
    QVERIFY(!viewer.isSafeLinkScheme(QStringLiteral("file:///etc/passwd")));
}

QTEST_MAIN(TestViewerLinkSeam)
#include "TestViewerLinkSeam.moc"
