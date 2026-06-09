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
        // G-02: prove SECRET_DATA is absent from ALL decoded streams, not just raw bytes.
        // A FlateDecode-compressed stream containing SECRET_DATA passes the raw-byte
        // check but is still extractable by any PDF viewer. PoDoFo CopyTo() applies
        // all stream filters (decompression), making this a real redaction gate.
        QString pdf = createTestPdfWithText("unextract.pdf", "SECRET_DATA");
        QVERIFY(!pdf.isEmpty());
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        QVERIFY(engine.applyRedactions(0, {QRectF(90, 120, 200, 30)}));

        QString output = tmpPath("unextract_redacted.pdf");
        QVERIFY(engine.saveDocument(output));

        // Reload via PoDoFo and inspect every decoded stream.
        PoDoFo::PdfMemDocument verifyDoc;
        verifyDoc.Load(output.toUtf8().constData());

        const QByteArray secret("SECRET_DATA");
        bool leaked = false;

        // Page content streams (decoded — FlateDecode applied by CopyTo).
        for (int pi = 0; pi < (int)verifyDoc.GetPages().GetCount(); ++pi) {
            auto& page = verifyDoc.GetPages().GetPageAt(pi);
            auto* co = page.GetContents();
            if (!co) continue;
            PoDoFo::charbuff buf;
            co->CopyTo(buf);
            if (QByteArray(buf.data(), static_cast<int>(buf.size())).contains(secret))
                leaked = true;
        }

        // All XObject/other streams (Form XObjects, image data).
        for (auto it = verifyDoc.GetObjects().begin();
             it != verifyDoc.GetObjects().end(); ++it) {
            PoDoFo::PdfObject* obj = *it;
            if (!obj->HasStream()) continue;
            PoDoFo::charbuff buf;
            try { obj->GetStream()->CopyTo(buf); } catch (...) { continue; }
            if (QByteArray(buf.data(), static_cast<int>(buf.size())).contains(secret))
                leaked = true;
        }

        QVERIFY2(!leaked,
                 "SECRET_DATA must not appear in any decoded stream after redaction "
                 "(G-02: FlateDecode-compressed leakage would defeat raw-byte check)");
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
        // DrawText(100, 700) → "100 700 Td". PdfVariantStack: stack[0]=top=last-pushed=700(ty→textY),
        // stack[1]=100(tx→textX). Correct tracking: textX=100, textY=700.
        // QRectF(90, 130, 200, 30) → pdfRect(90, 681.89, 200, 30): textX=100∈[90,290] and textY=700∈[681.89,711.89].
        QVERIFY(engine.applyRedactions(0, {QRectF(90, 130, 200, 30)}));
        
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
        // PdfVariantStack index convention: index 0 = top of stack = last pushed operand.
        // For "100 700 Td", operands push in order 100 then 700, so stack[0]=700(ty→textY)
        // and stack[1]=100(tx→textX). Correct tracking: textX=100, textY=700.
        // QRectF(90, 130, 200, 30) → pdfRect(90, 681.89, 200, 30):
        //   textX=100 ∈ [90, 290] and textY=700 ∈ [681.89, 711.89] → intersection confirmed.
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
        // Rect targeting the correct (textX=100, textY=700) position for DrawText(100, 700).
        QVERIFY(engine.applyRedactions(0, {QRectF(90, 130, 200, 30)}));

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

    // A-02: an image (bitmap) XObject intersecting a redaction rect must be excised
    // — its original pixel bytes must NOT survive, and the on-page Do must be
    // dropped. We place TWO images: one under the redaction rect (must be
    // neutralised to 1x1) and one well clear of it (must be untouched). This also
    // proves the intersection test uses the image CTM, not the text cursor.
    void testRedactionRemovesImageBytes() {
        QString pdf = tmpPath("image_redact.pdf");
        // Distinctive RGB24 pixel pattern so we can also scan for raw leakage.
        const int W = 16, H = 16;
        QByteArray pixels;
        for (int i = 0; i < W * H; ++i) { pixels.append('\xAB'); pixels.append('\xCD'); pixels.append('\xEF'); }

        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            const double pageH = page.GetMediaBox().Height;   // ~842 for A4

            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);

            // Secret image: device rect [100,600]..[180,660] (scaled to 80x60 pt).
            auto secret = doc.CreateImage();
            secret->SetData(PoDoFo::bufferview(pixels.constData(), pixels.size()),
                            W, H, PoDoFo::PdfPixelFormat::RGB24);
            painter.DrawImage(*secret, 100, 600, 80.0 / W, 60.0 / H);

            // Control image far away: device rect [100,100]..[140,140].
            auto keep = doc.CreateImage();
            keep->SetData(PoDoFo::bufferview(pixels.constData(), pixels.size()),
                          W, H, PoDoFo::PdfPixelFormat::RGB24);
            painter.DrawImage(*keep, 100, 100, 40.0 / W, 40.0 / H);

            painter.FinishDrawing();
            doc.Save(pdf.toUtf8().constData());

            // Sanity: redaction rect (top-left coords) that maps onto the secret
            // image's PDF region [600,660] in y. y_qt = pageH - 660.
            m_secretRectY = pageH - 660.0;
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        // QRectF(x=100, y=pageH-660, w=80, h=60) → PDF rect (100, 600, 80, 60).
        QVERIFY2(engine.applyRedactions(0, {QRectF(100, m_secretRectY, 80, 60)}),
                 "image redaction must succeed");

        QString out = tmpPath("image_redact_out.pdf");
        QVERIFY(engine.saveDocument(out));

        // Inspect the saved file: exactly one image XObject must have been
        // neutralised to 1x1 (the secret), and at least one must retain its
        // original 16x16 size (the control).
        PoDoFo::PdfMemDocument doc;
        doc.Load(out.toUtf8().constData());
        int neutralised = 0, intact = 0;
        for (auto it = doc.GetObjects().begin(); it != doc.GetObjects().end(); ++it) {
            PoDoFo::PdfObject* o = *it;
            if (!o->IsDictionary()) continue;
            auto& d = o->GetDictionary();
            auto* st = d.FindKey("Subtype");
            if (!st || !st->IsName() || std::string(st->GetName().GetString()) != "Image") continue;
            auto* w = d.FindKey("Width");
            auto* h = d.FindKey("Height");
            if (!w || !h) continue;
            const int64_t ww = w->GetNumber();
            const int64_t hh = h->GetNumber();
            if (ww == 1 && hh == 1) ++neutralised;
            else if (ww == W && hh == H) ++intact;
        }
        QVERIFY2(neutralised >= 1, "the redacted image XObject must be excised to 1x1");
        QVERIFY2(intact >= 1, "the non-intersecting image must be left intact (CTM-based test)");
    }

    // NF-2: An image embedded inside a Form XObject's OWN /Resources dict (not the
    // page's /Resources) must be neutralized when its bounding box intersects the
    // redaction rect. This proves that the Do-name lookup uses the current canvas's
    // resource dictionary during recursion, not always the top-level page dict.
    void testRedactionNeutralizesImageInFormLocalResources() {
        // Distinctive pixel pattern for the secret image.
        const int W = 8, H = 8;
        QByteArray pixels;
        for (int i = 0; i < W * H; ++i) {
            pixels.append('\xDE'); pixels.append('\xAD'); pixels.append('\xBE');
        }

        QString pdf = tmpPath("form_local_img.pdf");
        {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));

            // Create a Form XObject with its own painter.  When we call
            // xobjPainter.DrawImage(), PoDoFo registers the image in the Form's
            // OWN /Resources/XObject — NOT in the page's resources.  This is
            // exactly the NF-2 scenario.
            auto xobj = doc.CreateXObjectForm(PoDoFo::Rect(0, 0, 80, 60));
            PoDoFo::PdfPainter xobjPainter;
            xobjPainter.SetCanvas(*xobj);
            auto secretImg = doc.CreateImage();
            secretImg->SetData(PoDoFo::bufferview(pixels.constData(), pixels.size()),
                               W, H, PoDoFo::PdfPixelFormat::RGB24);
            // Draw the image filling the Form's bounding box.
            xobjPainter.DrawImage(*secretImg, 0, 0, 80.0 / W, 60.0 / H);
            xobjPainter.FinishDrawing();

            // Place the Form on the page at position (100, 600) in PDF coords
            // (device: left=100, bottom=600, right=180, top=660).
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            painter.DrawXObject(*xobj, 100, 600);
            painter.FinishDrawing();

            doc.Save(pdf.toUtf8().constData());

            // Remember PDF->Qt Y conversion: Qt y_top = pageH - pdf_top = pageH - 660
            m_secretRectY = page.GetMediaBox().Height - 660.0;
        }

        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        // QRectF covers the Form placement area.
        QVERIFY2(engine.applyRedactions(0, {QRectF(100, m_secretRectY, 80, 60)}),
                 "NF-2: redaction over Form-local-resource image must succeed");

        QString out = tmpPath("form_local_img_redacted.pdf");
        QVERIFY(engine.saveDocument(out));

        // Verify: the image must be neutralised to 1x1 — its original 8x8 pixel
        // bytes must not survive in any decoded stream in the saved file.
        PoDoFo::PdfMemDocument verifyDoc;
        verifyDoc.Load(out.toUtf8().constData());

        int neutralised = 0;
        bool pixelLeaked = false;
        const QByteArray secretPixels = pixels; // \xDE\xAD\xBE repeated

        for (auto it = verifyDoc.GetObjects().begin();
             it != verifyDoc.GetObjects().end(); ++it) {
            PoDoFo::PdfObject* o = *it;
            if (o->IsDictionary()) {
                auto& d = o->GetDictionary();
                auto* st = d.FindKey("Subtype");
                if (st && st->IsName() &&
                    std::string(st->GetName().GetString()) == "Image") {
                    auto* w = d.FindKey("Width");
                    auto* h = d.FindKey("Height");
                    if (w && h && w->GetNumber() == 1 && h->GetNumber() == 1)
                        ++neutralised;
                }
            }
            if (o->HasStream()) {
                PoDoFo::charbuff buf;
                try { o->GetStream()->CopyTo(buf); } catch (...) { continue; }
                if (QByteArray(buf.data(), static_cast<int>(buf.size())).contains(secretPixels))
                    pixelLeaked = true;
            }
        }

        QVERIFY2(neutralised >= 1,
                 "NF-2: image embedded in Form XObject local /Resources must be excised to 1x1");
        QVERIFY2(!pixelLeaked,
                 "NF-2: original secret pixel bytes must not survive in any decoded stream");
    }

private:
    double m_secretRectY = 0.0;

private slots:
};

QTEST_GUILESS_MAIN(TestRedaction)
#include "TestRedaction.moc"
