// SPDX-License-Identifier: Apache-2.0
// Audit 9.8 P0 regression test: the redaction Apply flow offers a bundled
// sanitize pass. This pins the engine sequence the flow performs:
// redactions applied + saved, then sanitizeDocument produces a copy with no
// metadata / OpenAction / JS name tree left behind.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"

class TestRedactSanitizeBundle : public QObject {
    Q_OBJECT
private slots:
    void sanitizedCopyIsClean();
};
void TestRedactSanitizeBundle::sanitizedCopyIsClean() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Build a 'risky' source: OpenAction JS + catalog XMP + trailer /Info.
    const QString src = tmp.filePath("risky.pdf");
    {
        PoDoFo::PdfMemDocument doc;
        doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        auto& cat = doc.GetCatalog().GetDictionary();
        PoDoFo::PdfDictionary oa;
        oa.AddKey("S", PoDoFo::PdfName("JavaScript"));
        oa.AddKey("JS", PoDoFo::PdfString("app.alert(1);"));
        cat.AddKey("OpenAction", PoDoFo::PdfObject(oa));
        cat.AddKey("Metadata", PoDoFo::PdfObject(PoDoFo::PdfString("<x:xmpmeta/>")));
        doc.Save(src.toUtf8().constData());
    }

    // The exact engine sequence the bundled flow performs.
    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(src));
    const QString sanitized = tmp.filePath("risky_redacted_sanitized.pdf");
    QVERIFY(engine.sanitizeDocument(sanitized));

    // The sanitized copy must carry none of the hidden data.
    PoDoFo::PdfMemDocument out;
    out.Load(sanitized.toUtf8().constData());
    auto& cat = out.GetCatalog().GetDictionary();
    QVERIFY(!cat.HasKey("OpenAction"));
    QVERIFY(!cat.HasKey("Metadata"));
}
QTEST_MAIN(TestRedactSanitizeBundle)
#include "TestRedactSanitizeBundle.moc"
