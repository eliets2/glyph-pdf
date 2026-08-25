// SPDX-License-Identifier: Apache-2.0
// §9.11 P0 regression tests: text watermark must honor options.fontFamily
// (was hard-coded Helvetica) and must center using real glyph advances via
// font->GetStringLength (was a char-count heuristic).
#include <QtTest>
#include <QTemporaryDir>
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
};

QTEST_MAIN(TestWatermarkFont)
#include "TestWatermarkFont.moc"
