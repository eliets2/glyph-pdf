// SPDX-License-Identifier: Apache-2.0
// Audit 9.13 P0 regression test: the Compress flow's "strip metadata" option
// must run the FULL sanitizeDocument() hidden-data scrub (OpenAction, /AA,
// embedded files, JS name tree, ...), not just remove /Info + catalog XMP.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <podofo/podofo.h>
#include "engines/podofo/PoDoFoBackend.h"

class TestCompressStripSanitize : public QObject {
    Q_OBJECT
private slots:
    void stripMetadataRemovesHiddenData();
private:
    static QString makeRiskyPdf(const QString& path);
};
QString TestCompressStripSanitize::makeRiskyPdf(const QString& path) {
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        Q_UNUSED(page);
        auto& cat = doc.GetCatalog().GetDictionary();
        // OpenAction: a dictionary action (JS) — must be scrubbed.
        PoDoFo::PdfDictionary oa;
        oa.AddKey("S", PoDoFo::PdfName("JavaScript"));
        oa.AddKey("JS", PoDoFo::PdfString("app.alert(1);"));
        cat.AddKey("OpenAction", PoDoFo::PdfObject(oa));
        // /Names with /JavaScript tree — must be scrubbed.
        PoDoFo::PdfDictionary jsNameTree;
        jsNameTree.AddKey("Names", PoDoFo::PdfArray());
        PoDoFo::PdfDictionary names;
        names.AddKey("JavaScript", PoDoFo::PdfObject(jsNameTree));
        cat.AddKey("Names", PoDoFo::PdfObject(names));
        doc.Save(path.toUtf8().constData());
        return path;
    } catch (...) {
        return {};
    }
}
void TestCompressStripSanitize::stripMetadataRemovesHiddenData() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString risky = makeRiskyPdf(tmp.filePath("risky.pdf"));
    QVERIFY2(!risky.isEmpty(), "makeRiskyPdf failed");

    PoDoFoBackend backend;
    QVERIFY2(backend.loadDocument(risky), "loadDocument(risky) failed");

    OptimizeOptions opts;
    opts.stripMetadata = true; // Compress dialog checkbox under test
    const QString out = tmp.filePath("optimized.pdf");
    const bool ok = backend.optimizeDocument(out, opts);
    QVERIFY2(ok, "optimizeDocument(stripMetadata=true) failed");

    // Reload the optimized file and verify the full scrub ran.
    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        auto& cat = doc.GetCatalog().GetDictionary();
        QVERIFY2(!cat.HasKey("OpenAction"), "OpenAction must be removed by stripMetadata");
        // The empty /Names container may remain (legal per spec); the dangerous
        // JavaScript NAME TREE inside it must be gone.
        QVERIFY2(!cat.HasKey("Names") || !cat.FindKey("Names")->GetDictionary().HasKey("JavaScript"),
                 "JavaScript name tree must be removed by stripMetadata");
        QVERIFY2(!cat.HasKey("Metadata"), "catalog XMP must be removed by stripMetadata");
        QVERIFY2(!doc.GetTrailer().GetDictionary().HasKey("Info"), "/Info must be removed by stripMetadata");
    } catch (const PoDoFo::PdfError& e) {
        QFAIL(qPrintable(QStringLiteral("failed to reload optimized pdf: %1")
                         .arg(QString::fromLatin1(e.what()))));
    }
}
QTEST_MAIN(TestCompressStripSanitize)
#include "TestCompressStripSanitize.moc"
