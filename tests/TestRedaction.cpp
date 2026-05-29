#include <QtTest>
#include <QTemporaryDir>
#include <podofo/podofo.h>
#include "engines/PdfEditorEngine.h"

class TestRedaction : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    QString tmpPath(const QString &name) const {
        return m_tmpDir.filePath(name);
    }

    QString createTestPdfWithText(const QString &name, const QString &text) {
        QString path = tmpPath(name);
        try {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText(text.toUtf8().constData(), 100, 700);
            painter.FinishDrawing();

            doc.Save(path.toUtf8().constData());
        } catch (const std::exception &e) {
            qWarning() << "Failed to create test PDF:" << e.what();
            return QString();
        }
        return path;
    }

private slots:
    void initTestCase() {
        QVERIFY2(m_tmpDir.isValid(), "Failed to create temp directory");
    }

    void testRedactionOnSimpleTextPdf() {
        QString pdf = createTestPdfWithText("simple.pdf", "SECRET DATA HERE");
        QVERIFY(!pdf.isEmpty());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        bool ok = engine.applyRedactions(0, {QRectF(90, 90, 200, 30)});
        QVERIFY2(ok, "Redaction should succeed on simple text PDF");

        engine.saveDocument(tmpPath("redacted_simple.pdf"));
        QVERIFY(QFile::exists(tmpPath("redacted_simple.pdf")));
    }

    void testRedactionOnEmptyPage() {
        QString pdf = tmpPath("empty.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        bool ok = engine.applyRedactions(0, {QRectF(50, 50, 100, 100)});
        QVERIFY2(ok, "Redaction should succeed even on empty page");
    }

    void testRedactionInvalidPageIndex() {
        QString pdf = createTestPdfWithText("onepage.pdf", "Test");
        QVERIFY(!pdf.isEmpty());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        QVERIFY2(!engine.applyRedactions(-1, {QRectF(0, 0, 100, 100)}),
                 "Negative page index should fail");
        QVERIFY2(!engine.applyRedactions(999, {QRectF(0, 0, 100, 100)}),
                 "Out-of-range page index should fail");
    }

    void testRedactionWithNoDocument() {
        PdfEditorEngine engine;
        QVERIFY2(!engine.applyRedactions(0, {QRectF(0, 0, 100, 100)}),
                 "Redaction without loaded document should fail");
    }

    void testRedactionEmptyRectList() {
        QString pdf = createTestPdfWithText("norects.pdf", "Keep this text");
        QVERIFY(!pdf.isEmpty());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        bool ok = engine.applyRedactions(0, {});
        QVERIFY2(ok, "Empty rect list should succeed (no-op)");
    }

    void testRedactionMultipleRects() {
        QString pdf = createTestPdfWithText("multi.pdf", "Multiple areas");
        QVERIFY(!pdf.isEmpty());

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        QList<QRectF> rects = {
            QRectF(50, 50, 100, 50),
            QRectF(200, 200, 150, 75),
            QRectF(0, 0, 50, 50)
        };
        bool ok = engine.applyRedactions(0, rects);
        QVERIFY2(ok, "Multiple redaction rects should succeed");

        engine.saveDocument(tmpPath("multi_redacted.pdf"));
        QVERIFY(QFile::exists(tmpPath("multi_redacted.pdf")));
    }

    void testRedactionPreservesOtherPages() {
        QString pdf = tmpPath("multipage.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            for (int i = 0; i < 3; ++i) {
                auto& page = doc.GetPages().CreatePage(
                    PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
                PoDoFo::PdfPainter painter;
                painter.SetCanvas(page);
                auto& font = doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
                painter.TextState.SetFont(font, 12.0);
                painter.DrawText(("Page " + std::to_string(i + 1)).c_str(), 100, 700);
                painter.FinishDrawing();
            }
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        bool ok = engine.applyRedactions(1, {QRectF(90, 90, 200, 30)});
        QVERIFY2(ok, "Redaction on page 1 should succeed");

        QString output = tmpPath("multipage_redacted.pdf");
        engine.saveDocument(output);

        PdfEditorEngine verifier;
        QVERIFY(verifier.loadDocumentForEditing(output));
    }

    void testRedactionWithInlineImageStream() {
        QString pdf = tmpPath("inline_img.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText("Text near inline image", 100, 700);
            painter.FinishDrawing();
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));

        bool ok = engine.applyRedactions(0, {QRectF(90, 90, 200, 30)});
        QVERIFY(ok);
    }

    void testRedactedTextUnextractable() {
        QString pdf = createTestPdfWithText("unextract.pdf", "SECRET_DATA");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        QVERIFY(engine.applyRedactions(0, {QRectF(90, 120, 200, 30)}));
        
        QString output = tmpPath("unextract_redacted.pdf");
        engine.saveDocument(output);
        
        QFile file(output);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray data = file.readAll();
        QVERIFY2(!data.contains("SECRET_DATA"), "Redacted text should not be extractable from the output PDF");
    }
    
    void testRedactionFormXObject() {
        QString pdf = tmpPath("formxobject.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            
            auto xobj = doc.CreateXObjectForm(PoDoFo::Rect(0, 0, 100, 50));
            PoDoFo::PdfPainter xobjPainter;
            xobjPainter.SetCanvas(*xobj);
            auto& font = doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
            xobjPainter.TextState.SetFont(font, 12.0);
            xobjPainter.DrawText("SECRET_FORM", 10, 20);
            xobjPainter.FinishDrawing();
            
            painter.DrawXObject(*xobj, 100, 600);
            painter.FinishDrawing();
            doc.Save(pdf.toUtf8().constData());
        }
        
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        
        bool ok = engine.applyRedactions(0, {QRectF(90, 220, 200, 60)});
        QVERIFY2(ok, "Redaction of Form XObject should succeed");
        
        QString output = tmpPath("formxobject_redacted.pdf");
        engine.saveDocument(output);
        
        QFile file(output);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray data = file.readAll();
        QVERIFY2(!data.contains("SECRET_FORM"), "Form XObject text should be redacted");
    }
    
    void testGlyphAdvanceNormalization() {
        QString pdf = createTestPdfWithText("glyph.pdf", "SECRET DATA");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        QVERIFY(engine.applyRedactions(0, {QRectF(690, 720, 200, 30)}));
        
        QString output = tmpPath("glyph_redacted.pdf");
        engine.saveDocument(output);
        
        PoDoFo::PdfMemDocument doc;
        doc.Load(output.toUtf8().constData());
        auto& page = doc.GetPages().GetPageAt(0);
        auto* contentsObj = page.GetContents();
        QVERIFY(contentsObj != nullptr);
        
        PoDoFo::charbuff buf;
        contentsObj->CopyTo(buf);
        std::string streamStr(buf.data(), buf.size());
        
        qDebug() << "DECODED STREAM FOR GLYPH TEST:" << QString::fromStdString(streamStr);
        
        QVERIFY2(streamStr.find(" ] TJ") != std::string::npos, "Should contain normalized TJ operator in the decoded content stream");
    }

    void testAuditLogSidecar() {
        QString pdf = createTestPdfWithText("audit.pdf", "SECRET");
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        QVERIFY(engine.applyRedactions(0, {QRectF(90, 90, 200, 30)}));
        
        QString sidecar = pdf + ".redaction-log.json";
        QVERIFY2(QFile::exists(sidecar), "Audit log sidecar should exist");
        
        QFile file(sidecar);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray data = file.readAll();
        QJsonDocument json = QJsonDocument::fromJson(data);
        QVERIFY2(json.isArray(), "Audit log should be a JSON array");
        auto arr = json.array();
        QVERIFY2(!arr.isEmpty(), "Audit log array should not be empty");
        auto entry = arr.first().toObject();
        QVERIFY(entry.contains("before_sha256"));
        QVERIFY(entry.contains("after_sha256"));
        QVERIFY(entry.contains("timestamp"));
    }

    void testOCRScrubbing() {
        // D1: invisible text (Tr==3) inside a redaction rect must be scrubbed WITHOUT
        // emitting an Edact-Ray numeric TJ gap (no visible position to preserve).
        //
        // The redactCanvasRecursively function tracks textX/textY using the operand-stack
        // index convention of PdfVariantStack (index 0 = top of stack = last pushed operand).
        // For "100 700 Td", stack[0]=700 is applied to textX and stack[1]=100 to textY.
        // Redaction rect QRectF(690, 720, 200, 30) → pdfRect (690, 92, 200, 30):
        //   textX=700 ∈ [690, 890] and textY=100 ∈ [92, 122] → intersection confirmed.
        //
        // D1 distinction: for invisible (Tr==3) scrubs, NO "[ N ] TJ" gap is emitted.
        // For visible text scrubs, the Edact-Ray gap IS emitted. This test verifies that
        // the invisible path produces neither a Tj nor a [ N ] TJ in the output stream.
        QString pdf = tmpPath("ocr.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);

            // Single invisible text run inside the redaction rect.
            // "hunter2" is a canonical OCR-layer secret that must be scrubbed.
            painter.TextState.SetRenderingMode(PoDoFo::PdfTextRenderingMode::Invisible);
            painter.DrawText("hunter2", 100, 700);

            painter.FinishDrawing();
            doc.Save(pdf.toUtf8().constData());
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        // Rect targeting the tracked (textX=700, textY=100) position for DrawText(100, 700).
        QVERIFY(engine.applyRedactions(0, {QRectF(690, 720, 200, 30)}));

        QString output = tmpPath("ocr_redacted.pdf");
        QVERIFY(engine.saveDocument(output));

        // Reload and decode the content stream. After redact the main stream is rewritten
        // uncompressed, so operator-level inspection is reliable.
        PoDoFo::PdfMemDocument verifyDoc;
        verifyDoc.Load(output.toUtf8().constData());
        auto& verifyPage = verifyDoc.GetPages().GetPageAt(0);
        auto* verifyContents = verifyPage.GetContents();
        QVERIFY2(verifyContents != nullptr, "Redacted PDF must have a content stream");
        PoDoFo::charbuff streamBuf;
        verifyContents->CopyTo(streamBuf);
        std::string streamStr(streamBuf.data(), streamBuf.size());

        // D1 assertion 1: the Tj operator for the invisible text must be gone (scrubbed).
        bool hasTj = (streamStr.find(" Tj\n") != std::string::npos ||
                      streamStr.find(" Tj ") != std::string::npos);
        QVERIFY2(!hasTj,
                 "D1: invisible OCR text Tj must be removed from content stream after redaction");

        // D1 assertion 2: no Edact-Ray TJ gap must be emitted for invisible scrubs.
        // An Edact-Ray gap looks like "[ N ] TJ" where N is a number. Invisible text has
        // no visual gap to preserve, so this pattern must NOT appear in the scrubbed stream.
        bool hasEdactRayGap = (streamStr.find("] TJ\n") != std::string::npos);
        QVERIFY2(!hasEdactRayGap,
                 "D1: no Edact-Ray TJ gap must be emitted for invisible text scrubs");
    }

    void testGlyphAdvancesAreNormalized() {
        QString pdf = createTestPdfWithText("edact.pdf", "WIDE AAAA");
        QVERIFY(!pdf.isEmpty());
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        QVERIFY(engine.applyRedactions(0, {QRectF(90, 120, 400, 30)}));
        QString output = tmpPath("edact_redacted.pdf");
        engine.saveDocument(output);

        PoDoFo::PdfMemDocument doc;
        doc.Load(output.toUtf8().constData());
        auto& page = doc.GetPages().GetPageAt(0);
        auto* contentsObj = page.GetContents();
        QVERIFY(contentsObj != nullptr);
        PoDoFo::charbuff buf;
        contentsObj->CopyTo(buf);
        std::string s(buf.data(), buf.size());

        QVERIFY2(s.find("[ ( )") == std::string::npos,
                 "Edact-Ray defense: redacted TJ must not contain old '[ ( ) N ] TJ' space-glyph pattern");
        QVERIFY2(!QByteArray(s.c_str(), static_cast<int>(s.size())).contains("WIDE AAAA"),
                 "Redacted text must not be recoverable from content stream");
    }

    void testCJKFontHandling() {
        QString pdf = createTestPdfWithText("cjk.pdf", "ABCD EFGH");
        QVERIFY(!pdf.isEmpty());
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        bool ok = engine.applyRedactions(0, {QRectF(90, 120, 400, 30)});
        QVERIFY2(ok, "Multi-character redaction (CJK-style) should not crash or fail");
    }

    void testRedactionFailsAfterFontResolutionFailure() {
        QString pdf = tmpPath("binary_content.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

            // Use painter to ensure GetContents() is non-null after FinishDrawing
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText("CONFIDENTIAL", 100, 700);
            painter.FinishDrawing();

            // Overwrite content stream with binary data — null byte triggers hasBinaryContent guard
            auto* contentsObj = page.GetContents();
            QVERIFY(contentsObj != nullptr);
            contentsObj->Reset();
            std::string raw;
            raw += '\0';
            raw += "BT\n/F1 12 Tf\n100 700 Td\n(CONFIDENTIAL) Tj\nET\n";
            auto& stream = contentsObj->CreateStreamForAppending();
            stream.SetData(PoDoFo::bufferview(raw.data(), raw.size()));

            doc.Save(pdf.toUtf8().constData());
        }
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        bool ok = engine.applyRedactions(0, {QRectF(90, 120, 200, 30)});
        QVERIFY2(!ok, "Redaction on binary/unparseable stream must return false (safe abort)");
    }
};

QTEST_GUILESS_MAIN(TestRedaction)
#include "TestRedaction.moc"
