// SPDX-License-Identifier: Apache-2.0
// Audit 9.1 P0 regression test: link annotations must be extracted with their
// click regions and targets (URI + internal GoTo) so the viewer can navigate
// on click — previously no code path for links existed at all.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "engines/podofo/PoDoFoBackend.h"

class TestHyperlinkNavigation : public QObject {
    Q_OBJECT
private slots:
    void extractsUriAndGoToLinks();
};
void TestHyperlinkNavigation::extractsUriAndGoToLinks() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("links.pdf");

    {
        PoDoFo::PdfMemDocument doc;
        auto& p0 = doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        auto& annos = p0.GetAnnotations();

        // URI link.
        auto& uriAnnot = annos.CreateAnnot(PoDoFo::PdfAnnotationType::Link,
                                           PoDoFo::Rect(50, 700, 200, 20));
        PoDoFo::PdfDictionary uriAction;
        uriAction.AddKey("S", PoDoFo::PdfName("URI"));
        uriAction.AddKey("URI", PoDoFo::PdfString("https://example.com/"));
        uriAnnot.GetObject().GetDictionary().AddKey("A", PoDoFo::PdfObject(uriAction));

        // Internal GoTo link → page 1 (index 1), direct /Dest array.
        auto& gotoAnnot = annos.CreateAnnot(PoDoFo::PdfAnnotationType::Link,
                                            PoDoFo::Rect(50, 600, 150, 20));
        PoDoFo::PdfArray dest;
        dest.Add(doc.GetPages().GetPageAt(1).GetObject().GetIndirectReference());
        dest.Add(PoDoFo::PdfName("XYZ"));
        gotoAnnot.GetObject().GetDictionary().AddKey("Dest", PoDoFo::PdfObject(dest));

        doc.Save(pdf.toUtf8().constData());
    }

    const QList<PdfLinkInfo> links = PoDoFoBackend::extractLinks(pdf, 0);
    QCOMPARE(links.size(), 2);

    bool sawUri = false, sawGoTo = false;
    for (const auto& l : links) {
        if (l.isUri) {
            sawUri = true;
            QCOMPARE(l.uri, QStringLiteral("https://example.com/"));
        } else if (l.targetPage == 1) {
            sawGoTo = true;
        }
        QVERIFY(l.rect.isValid());
    }
    QVERIFY(sawUri);
    QVERIFY(sawGoTo);
}
QTEST_MAIN(TestHyperlinkNavigation)
#include "TestHyperlinkNavigation.moc"
