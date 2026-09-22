// SPDX-License-Identifier: Apache-2.0
// §9.11 P0 regression tests: text watermark must honor options.fontFamily
// (was hard-coded Helvetica) and must center using real glyph advances via
// font->GetStringLength (was a char-count heuristic).
#include <QtTest>
#include <QTemporaryDir>
#include <QImage>
#include <podofo/podofo.h>
#include <sstream>
#include <string>

#include "engines/podofo/PoDoFoBackend.h"

class TestWatermarkFont : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    // Minimal single-page base PDF (mirrors TestAnnotationDjot::createBasePdf).
    QString createBasePdf(const QString& name)
    {
        const QString path = m_tmpDir.filePath(name);
        try {
            PoDoFo::PdfMemDocument doc;
            auto& page = doc.GetPages().CreatePage(
                PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page);
            auto& font = doc.GetFonts().GetStandard14Font(
                PoDoFo::PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(font, 12.0);
            painter.DrawText("Base document.", 50, 700);
            painter.FinishDrawing();
            doc.Save(path.toUtf8().constData());
        } catch (const std::exception& e) {
            qWarning() << "createBasePdf failed:" << e.what();
            return {};
        }
        return path;
    }

    // Load a saved PDF and return page 0's decoded content stream bytes,
    // handling both single-stream and stream-array /Contents shapes.
    static void appendStreamBytes(PoDoFo::PdfMemDocument& doc,
                                  PoDoFo::PdfObject* obj, std::string& out)
    {
        if (!obj) return;
        if (obj->IsReference()) {
            obj = doc.GetObjects().GetObject(obj->GetReference());
            if (!obj) return;
        }
        if (obj->IsArray()) {
            for (auto& child : obj->GetArray())
                appendStreamBytes(doc, &child, out);
            return;
        }
        if (obj->HasStream()) {
            PoDoFo::charbuff buf;
            obj->GetStream()->CopyTo(buf);  // decoded (filters applied)
            out.append(buf.data(), buf.size());
        }
    }

    static std::string firstPageContent(const QString& path)
    {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto& page = doc.GetPages().GetPageAt(0);
        auto* contentsObj = page.GetContents();
        if (!contentsObj) return {};
        std::string out;
        appendStreamBytes(doc, &contentsObj->GetObject(), out);
        return out;
    }

    // S1-2 probe: the /ca /CA operands of the watermark ExtGState that page
    // 0's resources reference ("GS_WM" text, "GS_WMI" image), read from the
    // SAVED file. Missing keys read as -999 (assertion-visible sentinel).
    static QPair<double, double> extGStateOpacity(const QString& path, const char* key)
    {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto& page = doc.GetPages().GetPageAt(0);
        auto* resDict = page.GetDictionary().FindKey("Resources");
        if (!resDict) return {-999.0, -999.0};
        auto* gsDict = resDict->GetDictionary().FindKey("ExtGState");
        if (!gsDict) return {-999.0, -999.0};
        auto* gsRef = gsDict->GetDictionary().FindKey(key);
        if (!gsRef) return {-999.0, -999.0};
        PoDoFo::PdfObject* gsObj = gsRef;
        if (gsObj->IsReference())
            gsObj = doc.GetObjects().GetObject(gsObj->GetReference());
        if (!gsObj || !gsObj->IsDictionary()) return {-999.0, -999.0};
        const auto read = [gsObj](const char* k) -> double {
            const PoDoFo::PdfObject* v = gsObj->GetDictionary().FindKey(k);
            return (v && v->IsNumberOrReal()) ? v->GetReal() : -999.0;
        };
        return {read("ca"), read("CA")};
    }

    // BaseFont name of the /Resources /Font <key> entry on page 0.
    static QString resourceFontBaseName(const QString& path, const QByteArray& key)
    {
        PoDoFo::PdfMemDocument doc;
        doc.Load(path.toUtf8().constData());
        auto& page = doc.GetPages().GetPageAt(0);
        auto* resDict = page.GetDictionary().FindKey("Resources");
        if (!resDict) return {};
        auto* fontDict = resDict->GetDictionary().FindKey("Font");
        if (!fontDict) return {};
        auto* fontRef = fontDict->GetDictionary().FindKey(key.constData());
        if (!fontRef) return {};
        auto* baseFont = fontRef->GetDictionary().FindKey("BaseFont");
        if (!baseFont) return {};
        return QString::fromStdString(std::string(baseFont->GetName().GetString()));
    }

    // Apply a watermark and return the x operand of the "... Td" centering move.
    double watermarkTdX(const QString& text, const QString& family)
    {
        const QString base = createBasePdf("wm_c_" + text + ".pdf");
        const QString out  = m_tmpDir.filePath("wm_co_" + text + ".pdf");
        TextWatermarkOptions o;
        o.text = text; o.fontFamily = family; o.fontSize = 48;
        PoDoFoBackend backend;
        if (!backend.loadDocument(base) || !backend.addTextWatermark(o) || !backend.saveDocument(out))
            return 0.0;
        const std::string c = firstPageContent(out);
        // The watermark block is appended after the base content, so its centering
        // move is the LAST " Td" in the stream (rfind, not find — the base document
        // draws its own text with an earlier "x y Td").
        const auto tdp = c.rfind(" Td");
        if (tdp == std::string::npos) return 0.0;
        const auto nl = c.rfind('\n', tdp);
        std::istringstream is(c.substr(nl + 1, tdp - nl - 1));
        double x = 0.0; is >> x; return x;
    }

private slots:
    void initTestCase()
    {
        QVERIFY2(m_tmpDir.isValid(), "Temp directory creation failed");
    }

    // ── Font family is honored end-to-end ────────────────────────────────────
    void testCourierFamilyIsAppliedAndReferenced()
    {
        const QString base = createBasePdf("wm_courier_base.pdf");
        QVERIFY(!base.isEmpty());
        const QString out = m_tmpDir.filePath("wm_courier_out.pdf");

        TextWatermarkOptions opts;
        opts.text       = QStringLiteral("CONFIDENTIAL");
        opts.fontFamily = QStringLiteral("Courier");   // dialog offers this
        opts.fontSize   = 48;

        PoDoFoBackend backend;
        QVERIFY(backend.loadDocument(base));
        QVERIFY2(backend.addTextWatermark(opts), "addTextWatermark should succeed");
        QVERIFY(backend.saveDocument(out));

        // The registered page-font resource must be the requested family...
        QCOMPARE(resourceFontBaseName(out, "GS_WM_Font"), QStringLiteral("Courier"));

        // ...and the content stream must select it by that resource name.
        const std::string content = firstPageContent(out);
        QVERIFY2(content.find("/GS_WM_Font 48 Tf") != std::string::npos,
                 "content stream must reference /GS_WM_Font Tf");
        QVERIFY2(content.find("/Helvetica 48 Tf") == std::string::npos,
                 "hard-coded /Helvetica Tf must be gone");
    }

    // ── Empty family falls back to Helvetica (backward compatible) ──────────
    void testEmptyFamilyFallsBackToHelvetica()
    {
        const QString base = createBasePdf("wm_fallback_base.pdf");
        QVERIFY(!base.isEmpty());
        const QString out = m_tmpDir.filePath("wm_fallback_out.pdf");

        TextWatermarkOptions opts;
        opts.fontFamily.clear();   // legacy callers may pass empty

        PoDoFoBackend backend;
        QVERIFY(backend.loadDocument(base));
        QVERIFY(backend.addTextWatermark(opts));
        QVERIFY(backend.saveDocument(out));

        QCOMPARE(resourceFontBaseName(out, "GS_WM_Font"), QStringLiteral("Helvetica"));
        const std::string content = firstPageContent(out);
        QVERIFY(content.find("/GS_WM_Font") != std::string::npos);
    }

    // ── Centering offset derives from real font metrics, not the char count ──
    // The fix replaced the char-count heuristic (estimatedWidth = n*size*0.5)
    // with a real measured width via PdfFont::GetStringLength. We assert the
    // emitted centering offset is NOT the value that heuristic would produce —
    // the exact audit item ("replace the char-count centering heuristic with
    // real font metrics"). This is robust to which concrete font resolves the
    // family and to per-glyph metric variance.
    void testCenteringUsesRealFontMetrics()
    {
        const QString text = QStringLiteral("WWWWWWWW");   // 8 glyphs
        const int size = 48;
        const double x = watermarkTdX(text, QStringLiteral("Helvetica"));
        QVERIFY2(x < 0.0, "watermark must be left-centered (negative Td x-offset)");
        // Old heuristic: estimatedWidth = n*size*0.5, x = -estimatedWidth/2.
        const double heuristicX = -(text.size() * size * 0.5) / 2.0;
        QVERIFY2(qAbs(x - heuristicX) > 3.0,
                 qPrintable(QStringLiteral("centering must use real glyph metrics, not the char-count "
                                           "heuristic: got Td x=%1, the heuristic would emit %2")
                                .arg(x).arg(heuristicX)));
    }

    // ── S1-2 (SWEEP-BACKEND-2026-09-21): out-of-range opacity must never
    // reach the ExtGState. TextWatermarkOptions/ImageWatermarkOptions document
    // opacity as 0.0–1.0, but PoDoFoBackend wrote the value verbatim into
    // /ca /CA — a caller passing 7.0 (the batch-preset string-param class)
    // produced a spec-invalid ExtGState. The pin reads the operands back from
    // the SAVED file: both seams (text GS_WM, image GS_WMI) must clamp.
    void testOpacityClampedToUnitRangeAtSeam()
    {
        // 7.0 → clamped to 1.0 (text watermark seam).
        {
            const QString base = createBasePdf("wm_clamp_hi_base.pdf");
            QVERIFY(!base.isEmpty());
            const QString out = m_tmpDir.filePath("wm_clamp_hi_out.pdf");
            TextWatermarkOptions opts;
            opts.opacity = 7.0;
            PoDoFoBackend backend;
            QVERIFY(backend.loadDocument(base));
            QVERIFY(backend.addTextWatermark(opts));
            QVERIFY(backend.saveDocument(out));
            const auto op = extGStateOpacity(out, "GS_WM");
            QVERIFY2(op.first == 1.0 && op.second == 1.0,
                     qPrintable(QStringLiteral("S1-2: opacity 7.0 must clamp /ca /CA to 1.0; "
                                               "got ca=%1 CA=%2").arg(op.first).arg(op.second)));
        }

        // -2.5 → clamped to 0.0 (text watermark seam).
        {
            const QString base = createBasePdf("wm_clamp_lo_base.pdf");
            QVERIFY(!base.isEmpty());
            const QString out = m_tmpDir.filePath("wm_clamp_lo_out.pdf");
            TextWatermarkOptions opts;
            opts.opacity = -2.5;
            PoDoFoBackend backend;
            QVERIFY(backend.loadDocument(base));
            QVERIFY(backend.addTextWatermark(opts));
            QVERIFY(backend.saveDocument(out));
            const auto op = extGStateOpacity(out, "GS_WM");
            QVERIFY2(op.first == 0.0 && op.second == 0.0,
                     qPrintable(QStringLiteral("S1-2: opacity -2.5 must clamp /ca /CA to 0.0; "
                                               "got ca=%1 CA=%2").arg(op.first).arg(op.second)));
        }

        // Image watermark seam: 3.0 → clamped to 1.0.
        {
            const QString base = createBasePdf("wm_clamp_img_base.pdf");
            QVERIFY(!base.isEmpty());
            const QString img = m_tmpDir.filePath("wm_clamp_img.png");
            QImage pm(16, 16, QImage::Format_ARGB32);
            pm.fill(QColor(0, 0, 0, 128));
            QVERIFY(pm.save(img, "PNG"));
            const QString out = m_tmpDir.filePath("wm_clamp_img_out.pdf");
            ImageWatermarkOptions opts;
            opts.imagePath = img;
            opts.opacity = 3.0;
            PoDoFoBackend backend;
            QVERIFY(backend.loadDocument(base));
            QVERIFY2(backend.addImageWatermark(opts), "addImageWatermark should succeed");
            QVERIFY(backend.saveDocument(out));
            const auto op = extGStateOpacity(out, "GS_WMI");
            QVERIFY2(op.first == 1.0 && op.second == 1.0,
                     qPrintable(QStringLiteral("S1-2: image opacity 3.0 must clamp /ca /CA to 1.0; "
                                               "got ca=%1 CA=%2").arg(op.first).arg(op.second)));
        }

        // In-range values pass through untouched (no clamp-side drift).
        {
            const QString base = createBasePdf("wm_clamp_ok_base.pdf");
            QVERIFY(!base.isEmpty());
            const QString out = m_tmpDir.filePath("wm_clamp_ok_out.pdf");
            TextWatermarkOptions opts;
            opts.opacity = 0.3;
            PoDoFoBackend backend;
            QVERIFY(backend.loadDocument(base));
            QVERIFY(backend.addTextWatermark(opts));
            QVERIFY(backend.saveDocument(out));
            const auto op = extGStateOpacity(out, "GS_WM");
            QVERIFY2(qAbs(op.first - 0.3) < 1e-9 && qAbs(op.second - 0.3) < 1e-9,
                     qPrintable(QStringLiteral("S1-2: in-range opacity must pass through; "
                                               "got ca=%1 CA=%2").arg(op.first).arg(op.second)));
        }
    }
};

QTEST_MAIN(TestWatermarkFont)
#include "TestWatermarkFont.moc"
