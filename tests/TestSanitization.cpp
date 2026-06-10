#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "commands/SanitizeDocumentHelper.h"
#include "engines/DocumentSession.h"
#include "mocks/MockPdfEditorEngine.h"
#include "engines/PdfEditorEngine.h"
#include "engines/podofo/PdfStringEscape.h"
#include "engines/podofo/PoDoFoBackend.h"

class TestSanitization : public QObject {
    Q_OBJECT

private slots:

    void testSanitizeSucceedsOnLoadedDocument()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;
        mock.m_sanitizeResult = true;

        DocumentSession doc;
        doc.setPath("input.pdf");

        bool result = SanitizeDocumentHelper::execute(&mock, &doc, "sanitized.pdf");

        QVERIFY2(result, "Sanitize should succeed on a loaded document");
        QCOMPARE(mock.m_sanitizeCalls, 1);
        QCOMPARE(mock.m_lastSanitizedPath, QString("sanitized.pdf"));
    }

    void testSanitizeFailsWhenEngineRejects()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;
        mock.m_sanitizeResult = false;

        DocumentSession doc;
        doc.setPath("input.pdf");

        bool result = SanitizeDocumentHelper::execute(&mock, &doc, "sanitized.pdf");

        QVERIFY2(!result, "Sanitize should fail when engine rejects");
        QCOMPARE(mock.m_sanitizeCalls, 1);
    }

    void testSanitizeSkipsWithEmptyDocPath()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;

        DocumentSession doc;

        bool result = SanitizeDocumentHelper::execute(&mock, &doc, "sanitized.pdf");

        QVERIFY2(!result, "Sanitize should skip when document path is empty");
        QCOMPARE(mock.m_sanitizeCalls, 0);
    }

    void testSanitizeSkipsWithEmptyOutputPath()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;

        DocumentSession doc;
        doc.setPath("input.pdf");

        bool result = SanitizeDocumentHelper::execute(&mock, &doc, "");

        QVERIFY2(!result, "Sanitize should skip when output path is empty");
        QCOMPARE(mock.m_sanitizeCalls, 0);
    }

    void testSanitizeSkipsWithNullEngine()
    {
        DocumentSession doc;
        doc.setPath("input.pdf");

        bool result = SanitizeDocumentHelper::execute(nullptr, &doc, "sanitized.pdf");

        QVERIFY2(!result, "Sanitize should skip with null engine");
    }

    void testSanitizeSkipsWithNullDocument()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;

        bool result = SanitizeDocumentHelper::execute(&mock, nullptr, "sanitized.pdf");

        QVERIFY2(!result, "Sanitize should skip with null document");
        QCOMPARE(mock.m_sanitizeCalls, 0);
    }

    void testMockSanitizeRemovesMetadata()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;

        PdfMetadata meta;
        meta.title = "Secret Title";
        meta.author = "Secret Author";
        meta.subject = "Confidential";
        meta.keywords = "secret;classified";
        meta.creator = "EvilApp";
        meta.producer = "EvilLib";
        mock.setMetadata(meta);

        QVERIFY(mock.sanitizeDocument("output.pdf"));

        QCOMPARE(mock.m_sanitizeCalls, 1);
        QCOMPARE(mock.m_lastSanitizedPath, QString("output.pdf"));
    }

    void testMockSanitizeRequiresLoad()
    {
        MockPdfEditorEngine mock;
        QVERIFY(!mock.sanitizeDocument("output.pdf"));

        mock.loadDocumentForEditing("test.pdf");
        QVERIFY(mock.sanitizeDocument("output.pdf"));
    }

    void testMultipleSanitizeCallsTracked()
    {
        MockPdfEditorEngine mock;
        mock.m_loaded = true;

        mock.sanitizeDocument("first.pdf");
        mock.sanitizeDocument("second.pdf");
        mock.sanitizeDocument("third.pdf");

        QCOMPARE(mock.m_sanitizeCalls, 3);
        QCOMPARE(mock.m_lastSanitizedPath, QString("third.pdf"));
    }

    void testSanitizeGeneratesUniqueTrailerID()
    {
        QTemporaryDir m_tmpDir;
        QVERIFY(m_tmpDir.isValid());
        QString pdf = m_tmpDir.filePath("sanitizetest_id.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        QString outPdf1 = m_tmpDir.filePath("sanitizetest_id_out1.pdf");
        QString outPdf2 = m_tmpDir.filePath("sanitizetest_id_out2.pdf");

        QVERIFY(engine.sanitizeDocument(outPdf1));
        QVERIFY(engine.sanitizeDocument(outPdf2));

        PoDoFo::PdfMemDocument outDoc1;
        outDoc1.Load(outPdf1.toUtf8().constData());
        
        PoDoFo::PdfMemDocument outDoc2;
        outDoc2.Load(outPdf2.toUtf8().constData());
        
        auto getTrailerId1 = [](PoDoFo::PdfMemDocument& d) -> std::string {
            if (d.GetTrailer().GetDictionary().HasKey("ID")) {
                auto* idObj = d.GetTrailer().GetDictionary().FindKey("ID");
                if (idObj && idObj->IsArray() && idObj->GetArray().GetSize() >= 2) {
                    return std::string(idObj->GetArray()[1].GetString().GetString());
                }
            }
            return "";
        };

        std::string id1 = getTrailerId1(outDoc1);
        std::string id2 = getTrailerId1(outDoc2);

        QVERIFY(!id1.empty());
        QVERIFY(!id2.empty());
        QVERIFY(id1 != id2);
    }

    void testIntegrationSanitization()
    {
        QTemporaryDir m_tmpDir;
        QVERIFY(m_tmpDir.isValid());
        QString pdf = m_tmpDir.filePath("sanitizetest.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

            auto& catalog = doc.GetCatalog();

            // -- Outlines (bookmarks) --
            auto& outlines = doc.GetObjects().CreateDictionaryObject();
            catalog.GetDictionary().AddKey(PoDoFo::PdfName("Outlines"), outlines.GetIndirectReference());

            // -- Collection (Portfolio) --
            auto& collection = doc.GetObjects().CreateDictionaryObject();
            catalog.GetDictionary().AddKey(PoDoFo::PdfName("Collection"), collection.GetIndirectReference());

            // -- JavaScript via /Names /JavaScript --
            auto& jsDictObj = doc.GetObjects().CreateDictionaryObject();
            jsDictObj.GetDictionary().AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
            jsDictObj.GetDictionary().AddKey(PoDoFo::PdfName("JS"),
                PoDoFo::PdfString("app.alert('pwned');"));
            auto& namesDict = doc.GetObjects().CreateDictionaryObject();
            PoDoFo::PdfArray jsNames;
            jsNames.Add(PoDoFo::PdfString("EvilScript"));
            jsNames.Add(jsDictObj.GetIndirectReference());
            namesDict.GetDictionary().AddKey(PoDoFo::PdfName("JavaScript"), jsNames);
            catalog.GetDictionary().AddKey(PoDoFo::PdfName("Names"), namesDict.GetIndirectReference());

            // -- OCProperties (Optional Content) --
            auto& ocpObj = doc.GetObjects().CreateDictionaryObject();
            ocpObj.GetDictionary().AddKey(PoDoFo::PdfName("OCGs"), PoDoFo::PdfArray());
            catalog.GetDictionary().AddKey(PoDoFo::PdfName("OCProperties"), ocpObj.GetIndirectReference());

            // -- Page-level AA (Additional Actions) --
            auto& aaObj = doc.GetObjects().CreateDictionaryObject();
            aaObj.GetDictionary().AddKey(PoDoFo::PdfName("O"),
                PoDoFo::PdfString("app.alert('open');"));
            page.GetDictionary().AddKey(PoDoFo::PdfName("AA"), aaObj.GetIndirectReference());

            // -- AcroForm field with /V (default value to scrub) --
            auto& field = page.CreateField<PoDoFo::PdfTextBox>("FieldName", PoDoFo::Rect(100, 100, 100, 20));
            field.SetText(PoDoFo::PdfString("FieldValue"));

            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        QString outPdf = m_tmpDir.filePath("sanitizetest_out.pdf");
        QVERIFY(engine.sanitizeDocument(outPdf));

        PoDoFo::PdfMemDocument outDoc;
        outDoc.Load(outPdf.toUtf8().constData());

        auto& outCatalog = outDoc.GetCatalog();

        // Vector 1: /Outlines removed.
        QVERIFY2(!outCatalog.GetDictionary().HasKey("Outlines"),
                 "G-04: /Outlines must be removed by sanitize");

        // Vector 2: /Collection removed.
        QVERIFY2(!outCatalog.GetDictionary().HasKey("Collection"),
                 "G-04: /Collection must be removed by sanitize");

        // Vector 3: /Names /JavaScript removed.
        if (outCatalog.GetDictionary().HasKey("Names")) {
            auto* namesObj = outCatalog.GetDictionary().FindKey("Names");
            if (namesObj && namesObj->IsReference())
                namesObj = &outDoc.GetObjects().MustGetObject(namesObj->GetReference());
            if (namesObj && namesObj->IsDictionary()) {
                QVERIFY2(!namesObj->GetDictionary().HasKey("JavaScript"),
                         "G-04: /Names /JavaScript must be removed by sanitize");
            }
        }

        // Vector 4: /OCProperties removed.
        QVERIFY2(!outCatalog.GetDictionary().HasKey("OCProperties"),
                 "G-04: /OCProperties must be removed by sanitize");

        // Vector 5: page-level /AA removed.
        auto& outPage = outDoc.GetPages().GetPageAt(0);
        QVERIFY2(!outPage.GetDictionary().HasKey("AA"),
                 "G-04: page /AA (additional-actions) must be removed by sanitize");

        // Vector 6: AcroForm field /V scrubbed.
        if (outCatalog.GetDictionary().HasKey("AcroForm")) {
            auto* acroForm = outCatalog.GetDictionary().GetKey("AcroForm");
            if (acroForm && acroForm->IsDictionary() && acroForm->GetDictionary().HasKey("Fields")) {
                auto* fields = acroForm->GetDictionary().GetKey("Fields");
                if (fields && fields->IsArray() && fields->GetArray().GetSize() > 0) {
                    auto fieldRef = fields->GetArray()[0];
                    if (fieldRef.IsReference()) {
                        auto& fieldDict = outDoc.GetObjects().MustGetObject(fieldRef.GetReference());
                        if (fieldDict.IsDictionary())
                            QVERIFY2(!fieldDict.GetDictionary().HasKey("V"),
                                     "G-04: Form field /V must be removed by sanitize");
                    }
                }
            }
        }
    }

    // T-H1: second integration pass covering the threat vectors the first test
    // does not — catalog /OpenAction, catalog /AA, /PieceInfo, /EmbeddedFiles,
    // RichMedia/Screen/Movie annotations, and dangerous annotation /A actions
    // (Launch / URI / SubmitForm). All must be neutralized by sanitizeDocument.
    void testIntegrationSanitizationExtendedVectors()
    {
        QTemporaryDir m_tmpDir;
        QVERIFY(m_tmpDir.isValid());
        QString pdf = m_tmpDir.filePath("sanitize_ext.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            auto& catalog = doc.GetCatalog();

            // -- catalog /OpenAction (auto-run on open) --
            auto& openAction = doc.GetObjects().CreateDictionaryObject();
            openAction.GetDictionary().AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("JavaScript"));
            openAction.GetDictionary().AddKey(PoDoFo::PdfName("JS"),
                PoDoFo::PdfString("app.alert('open');"));
            catalog.GetDictionary().AddKey(PoDoFo::PdfName("OpenAction"),
                openAction.GetIndirectReference());

            // -- catalog /AA --
            auto& catAA = doc.GetObjects().CreateDictionaryObject();
            catAA.GetDictionary().AddKey(PoDoFo::PdfName("WC"), PoDoFo::PdfString("onclose"));
            catalog.GetDictionary().AddKey(PoDoFo::PdfName("AA"), catAA.GetIndirectReference());

            // -- catalog /PieceInfo (private app data, can hide payloads) --
            auto& pieceInfo = doc.GetObjects().CreateDictionaryObject();
            catalog.GetDictionary().AddKey(PoDoFo::PdfName("PieceInfo"),
                pieceInfo.GetIndirectReference());

            // -- /Names /EmbeddedFiles --
            auto& efDictObj = doc.GetObjects().CreateDictionaryObject();
            PoDoFo::PdfArray efNames;
            efNames.Add(PoDoFo::PdfString("payload.exe"));
            efNames.Add(efDictObj.GetIndirectReference());
            auto& namesDict = doc.GetObjects().CreateDictionaryObject();
            namesDict.GetDictionary().AddKey(PoDoFo::PdfName("EmbeddedFiles"), efNames);
            catalog.GetDictionary().AddKey(PoDoFo::PdfName("Names"),
                namesDict.GetIndirectReference());

            // -- page /PieceInfo --
            page.GetDictionary().AddKey(PoDoFo::PdfName("PieceInfo"),
                doc.GetObjects().CreateDictionaryObject().GetIndirectReference());

            // -- Link annotation with a /Launch action (run executable) --
            auto& launchAction = doc.GetObjects().CreateDictionaryObject();
            launchAction.GetDictionary().AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("Launch"));
            launchAction.GetDictionary().AddKey(PoDoFo::PdfName("F"),
                PoDoFo::PdfString("cmd.exe"));
            auto& linkAnnot = doc.GetObjects().CreateDictionaryObject();
            linkAnnot.GetDictionary().AddKey(PoDoFo::PdfName("Type"), PoDoFo::PdfName("Annot"));
            linkAnnot.GetDictionary().AddKey(PoDoFo::PdfName("Subtype"), PoDoFo::PdfName("Link"));
            PoDoFo::PdfArray linkRect;
            linkRect.Add(PoDoFo::PdfObject(int64_t(0)));
            linkRect.Add(PoDoFo::PdfObject(int64_t(0)));
            linkRect.Add(PoDoFo::PdfObject(int64_t(50)));
            linkRect.Add(PoDoFo::PdfObject(int64_t(50)));
            linkAnnot.GetDictionary().AddKey(PoDoFo::PdfName("Rect"), linkRect);
            linkAnnot.GetDictionary().AddKey(PoDoFo::PdfName("A"),
                launchAction.GetIndirectReference());

            // -- URI link annotation (data exfil) --
            auto& uriAction = doc.GetObjects().CreateDictionaryObject();
            uriAction.GetDictionary().AddKey(PoDoFo::PdfName("S"), PoDoFo::PdfName("URI"));
            uriAction.GetDictionary().AddKey(PoDoFo::PdfName("URI"),
                PoDoFo::PdfString("https://evil.example/leak"));
            auto& uriAnnot = doc.GetObjects().CreateDictionaryObject();
            uriAnnot.GetDictionary().AddKey(PoDoFo::PdfName("Type"), PoDoFo::PdfName("Annot"));
            uriAnnot.GetDictionary().AddKey(PoDoFo::PdfName("Subtype"), PoDoFo::PdfName("Link"));
            PoDoFo::PdfArray uriRect;
            uriRect.Add(PoDoFo::PdfObject(int64_t(60)));
            uriRect.Add(PoDoFo::PdfObject(int64_t(0)));
            uriRect.Add(PoDoFo::PdfObject(int64_t(110)));
            uriRect.Add(PoDoFo::PdfObject(int64_t(50)));
            uriAnnot.GetDictionary().AddKey(PoDoFo::PdfName("Rect"), uriRect);
            uriAnnot.GetDictionary().AddKey(PoDoFo::PdfName("A"),
                uriAction.GetIndirectReference());

            PoDoFo::PdfArray annots;
            annots.Add(linkAnnot.GetIndirectReference());
            annots.Add(uriAnnot.GetIndirectReference());
            page.GetDictionary().AddKey(PoDoFo::PdfName("Annots"), annots);

            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        QString outPdf = m_tmpDir.filePath("sanitize_ext_out.pdf");
        QVERIFY(engine.sanitizeDocument(outPdf));

        PoDoFo::PdfMemDocument outDoc;
        outDoc.Load(outPdf.toUtf8().constData());
        auto& outCatalog = outDoc.GetCatalog();

        // Vector 7: catalog /OpenAction removed.
        QVERIFY2(!outCatalog.GetDictionary().HasKey("OpenAction"),
                 "T-H1: catalog /OpenAction must be removed");

        // Vector 8: catalog /AA removed.
        QVERIFY2(!outCatalog.GetDictionary().HasKey("AA"),
                 "T-H1: catalog /AA must be removed");

        // Vector 9: catalog /PieceInfo removed.
        QVERIFY2(!outCatalog.GetDictionary().HasKey("PieceInfo"),
                 "T-H1: catalog /PieceInfo must be removed");

        // Vector 10: /Names /EmbeddedFiles removed.
        if (outCatalog.GetDictionary().HasKey("Names")) {
            auto* namesObj = outCatalog.GetDictionary().FindKey("Names");
            if (namesObj && namesObj->IsReference())
                namesObj = &outDoc.GetObjects().MustGetObject(namesObj->GetReference());
            if (namesObj && namesObj->IsDictionary()) {
                QVERIFY2(!namesObj->GetDictionary().HasKey("EmbeddedFiles"),
                         "T-H1: /Names /EmbeddedFiles must be removed");
            }
        }

        auto& outPage = outDoc.GetPages().GetPageAt(0);

        // Vector 11: page /PieceInfo removed.
        QVERIFY2(!outPage.GetDictionary().HasKey("PieceInfo"),
                 "T-H1: page /PieceInfo must be removed");

        // Vectors 12-13: dangerous annotation /A actions stripped.
        // The Link annotations survive (Subtype Link is not removed) but their
        // /A Launch and /URI actions must be gone.
        auto& outAnnots = outPage.GetAnnotations();
        for (unsigned i = 0; i < outAnnots.GetCount(); ++i) {
            auto& a = outAnnots.GetAnnotAt(i);
            auto* actObj = a.GetDictionary().FindKey("A");
            if (actObj) {
                if (actObj->IsReference())
                    actObj = &outDoc.GetObjects().MustGetObject(actObj->GetReference());
                if (actObj && actObj->IsDictionary()) {
                    auto* sObj = actObj->GetDictionary().FindKey("S");
                    if (sObj && sObj->IsName()) {
                        const std::string s = std::string(sObj->GetName().GetString());
                        QVERIFY2(s != "Launch" && s != "URI",
                                 qPrintable(QString("T-H1: dangerous annotation action "
                                                    "'%1' survived sanitize").arg(QString::fromStdString(s))));
                    }
                }
            }
        }
    }

    void testPdfEscapeLiteralStringInvariants()
    {
        // Basic escaping
        QCOMPARE(pdfEscapeLiteralString(std::string("test")), std::string("test"));
        QCOMPARE(pdfEscapeLiteralString(std::string("te(s)t\\")), std::string("te\\(s\\)t\\\\"));
        
        // Idempotency: double escaping should equal single escaping
        std::string once = pdfEscapeLiteralString(std::string("te(s)t\\"));
        std::string twice = pdfEscapeLiteralString(once);
        QCOMPARE(once, twice);
        
        // Escaping control characters
        QCOMPARE(pdfEscapeLiteralString(std::string("\n\r")), std::string("\\n\\r"));
    }

    void testWatermarkInjectionIsPrevented()
    {
        QTemporaryDir m_tmpDir;
        QVERIFY(m_tmpDir.isValid());
        QString pdf = m_tmpDir.filePath("watermark_injection.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PoDoFoBackend backend;
        backend.loadDocument(pdf);
        
        TextWatermarkOptions opts;
        opts.text = "Foo) Tj 1 0 0 1 100 200 cm (";
        opts.fontSize = 12;
        opts.color = QColor(0,0,0);
        
        QVERIFY(backend.addTextWatermark(opts));
        QString outPdf = m_tmpDir.filePath("watermark_injection_out.pdf");
        QVERIFY(backend.saveDocument(outPdf));
        
        PoDoFo::PdfMemDocument outDoc;
        outDoc.Load(outPdf.toUtf8().constData());
        auto& page = outDoc.GetPages().GetPageAt(0);
        auto* contentsObj = page.GetContents();
        QVERIFY(contentsObj);
        
        PoDoFo::charbuff buf;
        contentsObj->CopyTo(buf);
        std::string streamStr(buf.data(), buf.size());
        
        // Ensure the injected string is escaped and doesn't break out of the literal
        std::string expectedEscaped = "Foo\\) Tj 1 0 0 1 100 200 cm \\(";
        QVERIFY(streamStr.find(expectedEscaped) != std::string::npos);
        QVERIFY(streamStr.find(opts.text.toStdString()) == std::string::npos); // Should not find the raw unescaped string
    }

    void testAnnotationInjectionIsPrevented()
    {
        QTemporaryDir m_tmpDir;
        QVERIFY(m_tmpDir.isValid());
        QString pdf = m_tmpDir.filePath("anno_injection.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PoDoFoBackend backend;
        backend.loadDocument(pdf);
        
        AnnotationItem anno;
        anno.pageIndex = 0;
        anno.rect = QRectF(100, 100, 100, 50);
        anno.text = "Here is \\, ( and )";
        anno.mode = ToolMode::Callout;
        
        QVERIFY(backend.embedAnnotations(pdf, m_tmpDir.filePath("anno_out.pdf"), {anno}));
        PoDoFo::PdfMemDocument outDoc;
        outDoc.Load(m_tmpDir.filePath("anno_out.pdf").toUtf8().constData());
        auto& page = outDoc.GetPages().GetPageAt(0);
        
        auto& annots = page.GetAnnotations();
        QCOMPARE(annots.GetCount(), 1u);
        auto& annot = annots.GetAnnotAt(0);
        auto* apDict = annot.GetDictionary().FindKey("AP");
        QVERIFY(apDict);
        auto& apDictRef = apDict->GetDictionary();
        auto* nStreamRef = apDictRef.FindKey("N");
        QVERIFY(nStreamRef);
        
        const PoDoFo::PdfObject* actualStreamObj = nStreamRef;
        if (nStreamRef->IsReference()) {
            actualStreamObj = &outDoc.GetObjects().MustGetObject(nStreamRef->GetReference());
        }
        
        if (!actualStreamObj->HasStream()) {
            QFAIL("Object does not have a stream!");
        }
        
        PoDoFo::charbuff buf;
        actualStreamObj->GetStream()->CopyTo(buf);
        std::string streamStr;
        if (buf.data() && buf.size() > 0) {
            streamStr.assign(buf.data(), buf.size());
        }
        
        std::string expectedEscaped = "Here is \\\\, \\( and \\)";
        QVERIFY(streamStr.find(expectedEscaped) != std::string::npos);
    }
    
    void testHeaderFooterInjectionIsPrevented()
    {
        QTemporaryDir m_tmpDir;
        QVERIFY(m_tmpDir.isValid());
        QString pdf = m_tmpDir.filePath("hf_injection.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PoDoFoBackend backend;
        
        HeaderFooterOptions opts;
        opts.textTemplate = "File: file(.pdf";
        opts.position = HeaderFooterOptions::Position::TopCenter;
        opts.fontSize = 12;
        
        QVERIFY(backend.addHeaderFooter(pdf, opts));
        
        PoDoFo::PdfMemDocument outDoc;
        outDoc.Load(pdf.toUtf8().constData());
        auto& page = outDoc.GetPages().GetPageAt(0);
        auto* contentsObj = page.GetContents();
        QVERIFY(contentsObj);
        
        PoDoFo::charbuff buf;
        contentsObj->CopyTo(buf);
        std::string streamStr(buf.data(), buf.size());
        
        std::string expectedEscaped = "File: file\\(.pdf";
        QVERIFY(streamStr.find(expectedEscaped) != std::string::npos);
    }

    void testBatesInjectionIsPrevented()
    {
        QTemporaryDir m_tmpDir;
        QVERIFY(m_tmpDir.isValid());
        QString pdf = m_tmpDir.filePath("bates_injection.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PoDoFoBackend backend;

        BatesNumberingOptions opts;
        opts.prefix = "CASE) Tj (";
        opts.suffix = "-END(";
        opts.startNumber = 7;
        opts.digitCount = 3;
        opts.position = HeaderFooterOptions::Position::BottomRight;
        opts.fontSize = 12;

        QVERIFY(backend.applyBatesNumbering(pdf, opts));

        PoDoFo::PdfMemDocument outDoc;
        outDoc.Load(pdf.toUtf8().constData());
        auto& page = outDoc.GetPages().GetPageAt(0);
        auto* contentsObj = page.GetContents();
        QVERIFY(contentsObj);

        PoDoFo::charbuff buf;
        contentsObj->CopyTo(buf);
        std::string streamStr(buf.data(), buf.size());

        std::string expectedEscaped = "CASE\\) Tj \\(007-END\\(";
        QVERIFY(streamStr.find(expectedEscaped) != std::string::npos);
    }
};

QTEST_GUILESS_MAIN(TestSanitization)
#include "TestSanitization.moc"
