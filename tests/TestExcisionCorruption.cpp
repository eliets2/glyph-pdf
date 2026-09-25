// SPDX-License-Identifier: Apache-2.0
// E-1 regression (evidence ledger 2026-09-05, "Newly discovered engine defect"):
// redacting ONE line corrupted a byte-ADJACENT same-stream Tj — observed
// 'PUBLIC_KEEP_TEXT' -> 'PUBLIC_KEEP_XEXX' (both 'T' glyph bytes 0x16 became
// 0x17) with the keep line ~150pt away in the SAME content stream, while a line
// on a SECOND page survived (its stream was never re-emitted). Every prior
// fixture was single-line (or kept the keep line on a second page) and could
// not see the corruption.
//
// These tests pin the excision contract at BOTH levels:
//  (a) extraction level — the secret is gone per independent Pdfium extraction
//      while the same-page neighbor line survives byte-exact;
//  (b) content-stream level — the neighbor Tj's bytes in the decoded page
//      stream are IDENTICAL before/after excision (hex-level assert that no
//      unexpected byte mutation occurs around the keep line). PoDoFo's painter
//      writes glyph-encoded hex strings (not ASCII), so the operators are
//      located by their Td geometry, not by an ASCII needle.
#include <cmath>
#include <cstring>
#include <QtTest/QtTest>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <podofo/podofo.h>

#include "engines/PdfEditorEngine.h"
#include "engines/pdfium/PdfiumBackend.h"

// Windows headers (transitively included via the pdfium/OpenSSL headers) define
// `#define DrawText DrawTextW`, which would rewrite the PoDoFo painter calls below,
// and `#define GetObject GetObjectW`, which would rewrite the PdfElement accessor.
#ifdef DrawText
#undef DrawText
#endif
#ifdef GetObject
#undef GetObject
#endif

class TestExcisionCorruption : public QObject {
    Q_OBJECT

private slots:
    // The exact E-1 geometry: secret line + keep line ~150pt apart in the SAME
    // content stream; only the secret is redacted; the keep line must survive
    // byte-exact.
    void samePageNeighborLineSurvivesByteExact();

    // Multi-line, multi-op stream: two secrets interleaved with two keep lines
    // (4 text-showing ops in one stream); both secrets redacted, both keeps
    // must survive byte-exact.
    void multiLineMultiOpStreamKeepsNeighborsByteExact();

    // G2 (audit REDACTION-RESEARCH-2026-09-21 §2.4): a page whose secret is
    // drawn ONLY inside a tiling pattern (/Pattern resources painted by
    // scn + fill — no Do) must ABORT redaction honestly — the canvas walk
    // cannot reach pattern streams, so a black box would paint over live data
    // (silent w/o proof). A text-free pattern must NOT refuse (the guard is
    // scoped to pattern streams carrying text operators).
    void patternDrawnSecretAbortsAndPatternFreeControlProceeds();

private:
    // Page 1: "Secret a@b.com" at (50,700) + "PUBLIC_KEEP_TEXT" at (50,550)
    // (150pt below, same stream). Page 2: "PAGE2_KEEP_TEXT" — the second-page
    // survival the U05 fixture could already pin honestly.
    static QString createTwoLinePdf(const QTemporaryDir& tmpDir, const QString& name);
    // One page: secret / keep / secret / keep alternating at 50pt steps.
    static QString createMultiLinePdf(const QTemporaryDir& tmpDir, const QString& name);

    // One page whose content paints a tiling pattern over the whole page
    // ("/Pattern cs /P1 scn ... re f"). With \p withText the pattern stream
    // carries the secret as a glyph-carrying text operator; otherwise it is
    // text-free (positive control).
    static QString createPatternPdf(const QTemporaryDir& tmpDir, const QString& name,
                                    bool withText);

    static QByteArray decodedPageContent(const QString& pdf, int pageIndex);

    // The full string-showing operator ("<...> Tj" or "(...) Tj") in the decoded
    // stream that follows a Td whose y coordinate matches \p y (the glyph-encoded
    // payloads make an ASCII search impossible). Empty if none.
    static QByteArray stringOpAtY(const QByteArray& stream, double y);
    static QString hexDump(const QByteArray& bytes);
};

QString TestExcisionCorruption::createTwoLinePdf(const QTemporaryDir& tmpDir,
                                                 const QString& name) {
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        painter.TextState.SetFont(font, 12.0);
        painter.DrawText("Secret a@b.com", 50, 700);
        // E-1 geometry: the keep line lives ~150pt below the secret in the SAME
        // content stream (the pre-E-1 workaround pushed it to a second page).
        painter.DrawText("PUBLIC_KEEP_TEXT", 50, 550);
        painter.FinishDrawing();

        auto& page2 = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter2;
        painter2.SetCanvas(page2);
        painter2.TextState.SetFont(font, 12.0);
        painter2.DrawText("PAGE2_KEEP_TEXT", 50, 700);
        painter2.FinishDrawing();

        doc.Save(path.toUtf8().constData());
    } catch (const std::exception&) {
        return {};
    }
    return path;
}

QString TestExcisionCorruption::createMultiLinePdf(const QTemporaryDir& tmpDir,
                                                   const QString& name) {
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        PoDoFo::PdfPainter painter;
        painter.SetCanvas(page);
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);
        painter.TextState.SetFont(font, 12.0);
        // Same charset as the two-line fixture (draw order: secret, keep,
        // secret, keep) so the keep lines' glyph-encoded bytes land in the same
        // byte range the E-1 evaluation round-trip corrupts — the multi-op
        // stream must not get a weaker guarantee than the two-line one.
        painter.DrawText("Secret a@b.com", 50, 700);
        painter.DrawText("PUBLIC_KEEP_TEXT", 50, 650);
        painter.DrawText("Secret c@d.org", 50, 600);
        painter.DrawText("PUBLIC_KEEP_TEXT", 50, 550);
        painter.FinishDrawing();
        doc.Save(path.toUtf8().constData());
    } catch (const std::exception&) {
        return {};
    }
    return path;
}

QString TestExcisionCorruption::createPatternPdf(const QTemporaryDir& tmpDir,
                                                 const QString& name, bool withText) {
    const QString path = tmpDir.filePath(name);
    try {
        PoDoFo::PdfMemDocument doc;
        auto& page = doc.GetPages().CreatePage(
            PoDoFo::PdfPage::CreateStandardPageSize(PoDoFo::PdfPageSize::A4));
        auto& font = doc.GetFonts().GetStandard14Font(
            PoDoFo::PdfStandard14FontType::Helvetica);

        // The tiling pattern: with withText its stream is the ONLY source of
        // the secret (glyph-carrying "x y Td (text) Tj"); without, text-free.
        const char* patternContent = withText
            ? "BT /F1 14 Tf 10 30 Td (PatternSecretOmega) Tj ET\n"
            : "0.2 0.4 0.8 rg 10 10 60 60 re f\n";
        auto& pattern = doc.GetObjects().CreateDictionaryObject();
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("Type"), PoDoFo::PdfName("Pattern"));
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("PatternType"), PoDoFo::PdfObject(int64_t(1)));
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("PaintType"), PoDoFo::PdfObject(int64_t(1)));
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("TilingType"), PoDoFo::PdfObject(int64_t(1)));
        PoDoFo::PdfArray bbox;
        bbox.Add(0.0); bbox.Add(0.0); bbox.Add(100.0); bbox.Add(100.0);
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("BBox"), bbox);
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("XStep"), PoDoFo::PdfObject(80.0));
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("YStep"), PoDoFo::PdfObject(80.0));
        auto& fontMap = doc.GetObjects().CreateDictionaryObject();
        fontMap.GetDictionary().AddKey(PoDoFo::PdfName("F1"),
                                       font.GetObject().GetIndirectReference());
        auto& patternRes = doc.GetObjects().CreateDictionaryObject();
        patternRes.GetDictionary().AddKey(PoDoFo::PdfName("Font"),
                                          fontMap.GetIndirectReference());
        pattern.GetDictionary().AddKey(PoDoFo::PdfName("Resources"),
                                       patternRes.GetIndirectReference());
        pattern.GetOrCreateStream().SetData(PoDoFo::bufferview(
            patternContent, std::strlen(patternContent)));

        // Page-level content painting the pattern (scn + fill — NO Do) and a
        // benign direct-text line; the page /Resources map the shared font.
        auto& patternMap = doc.GetObjects().CreateDictionaryObject();
        patternMap.GetDictionary().AddKey(PoDoFo::PdfName("P1"),
                                          pattern.GetIndirectReference());
        auto& pageRes = doc.GetObjects().CreateDictionaryObject();
        pageRes.GetDictionary().AddKey(PoDoFo::PdfName("Font"),
                                       fontMap.GetIndirectReference());
        pageRes.GetDictionary().AddKey(PoDoFo::PdfName("Pattern"),
                                       patternMap.GetIndirectReference());
        page.GetObject().GetDictionary().AddKey(PoDoFo::PdfName("Resources"),
                                                pageRes.GetIndirectReference());
        auto& content = doc.GetObjects().CreateDictionaryObject();
        const char* pageContent =
            "BT /F1 12 Tf 50 650 Td (PUBLIC_KEEP_TEXT) Tj ET\n"
            "/Pattern cs /P1 scn 0 0 595 842 re f\n";
        content.GetOrCreateStream().SetData(PoDoFo::bufferview(
            pageContent, std::strlen(pageContent)));
        page.GetObject().GetDictionary().AddKey(PoDoFo::PdfName("Contents"),
                                                content.GetIndirectReference());

        doc.Save(path.toUtf8().constData());
    } catch (const std::exception&) {
        return {};
    }
    return path;
}

QByteArray TestExcisionCorruption::decodedPageContent(const QString& pdf, int pageIndex) {    try {
        PoDoFo::PdfMemDocument doc;
        doc.Load(pdf.toUtf8().constData());
        auto& page = doc.GetPages().GetPageAt(pageIndex);
        auto* contentsObj = page.GetContents();
        if (!contentsObj) return {};
        PoDoFo::charbuff buf;
        contentsObj->CopyTo(buf);
        return QByteArray(buf.data(), static_cast<int>(buf.size()));
    } catch (const std::exception&) {
        return {};
    }
}

QByteArray TestExcisionCorruption::stringOpAtY(const QByteArray& stream, double y) {
    // "x y Td <whitespace> <string> Tj" — PoDoFo painter (and the engine's
    // re-emission) put each on its own line; tolerate arbitrary whitespace.
    static const QRegularExpression re(
        QStringLiteral("(-?[\\d.]+)\\s+(-?[\\d.]+)\\s+Td\\s*(<[^>]*>|\\((?:[^)\\\\]|\\\\.)*\\))\\s*(Tj|TJ)"));
    auto it = re.globalMatch(QString::fromLatin1(stream));
    while (it.hasNext()) {
        const auto m = it.next();
        const double yVal = m.captured(2).toDouble();
        if (std::abs(yVal - y) < 0.5) {
            return QByteArray(m.captured(3).toLatin1() + " " + m.captured(4).toLatin1());
        }
    }
    return {};
}

QString TestExcisionCorruption::hexDump(const QByteArray& bytes) {
    QString out;
    out.reserve(bytes.size() * 4);
    for (unsigned char c : bytes) {
        out += QStringLiteral("%1 ").arg(c, 2, 16, QLatin1Char('0'));
    }
    out += QLatin1String("| ascii: ");
    for (unsigned char c : bytes) {
        out += (c >= 0x20 && c < 0x7F) ? QChar::fromLatin1(static_cast<char>(c))
                                       : QLatin1Char('.');
    }
    return out;
}

void TestExcisionCorruption::samePageNeighborLineSurvivesByteExact() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTwoLinePdf(tmp, QStringLiteral("two_line.pdf"));
    QVERIFY2(!pdf.isEmpty(), "two-line fixture failed");

    const QByteArray preStream = decodedPageContent(pdf, 0);
    QVERIFY2(!preStream.isEmpty(), "could not read the fixture's page-1 content stream");
    qWarning() << "E1 PRE-REDACTION page-1 stream:" << preStream;
    const QByteArray preKeep = stringOpAtY(preStream, 550);
    QVERIFY2(!preKeep.isEmpty(),
             qPrintable(QStringLiteral("keep-line Tj not found in the pre stream\n%1")
                            .arg(hexDump(preStream))));
    const QByteArray preSecret = stringOpAtY(preStream, 700);
    QVERIFY2(!preSecret.isEmpty(), "secret-line Tj not found in the pre stream");

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(pdf));
    // Viewer top-down rect convention (the engine converts with
    // pageHeight - y - height): covers the secret drawn at PDF (50,700)
    // (top-down ~142). The keep line at (50,550) is ~150pt below in the SAME
    // content stream and must not be touched.
    QVERIFY2(engine.applyRedactions(0, { QRectF(40, 130, 300, 30) }),
             "engine applyRedactions must succeed");
    const QString out = tmp.filePath(QStringLiteral("two_line_redacted.pdf"));
    QVERIFY2(engine.saveDocument(out), "saveDocument must succeed");

    // Dump the post stream FIRST — the byte evidence must survive any later
    // assertion failure.
    const QByteArray postStream = decodedPageContent(out, 0);
    QVERIFY2(!postStream.isEmpty(), "could not read the output's page-1 content stream");
    qWarning() << "E1 POST-REDACTION page-1 stream:" << postStream;

    // --- Extraction level, independent extractor (Pdfium) ---
    PdfiumBackend pdfium;
    QVERIFY(pdfium.loadDocument(out));
    const QString p0 = pdfium.extractText(0);
    QVERIFY2(!p0.contains(QStringLiteral("a@b.com")),
             qPrintable(QStringLiteral("secret survived the redaction (page 1): %1").arg(p0)));
    QVERIFY2(p0.contains(QStringLiteral("PUBLIC_KEEP_TEXT")),
             qPrintable(QStringLiteral("E-1: same-page keep line corrupted or lost: %1").arg(p0)));
    const QString p1 = pdfium.extractText(1);
    QVERIFY2(p1.contains(QStringLiteral("PAGE2_KEEP_TEXT")),
             qPrintable(QStringLiteral("second-page keep line lost: %1").arg(p1)));

    // --- Content-stream level: hex-level byte assertions ---
    // The secret's operator bytes must not survive (the redacted Tj becomes a
    // numeric TJ gap — Edact-Ray defense).
    QVERIFY2(postStream.indexOf(preSecret) < 0,
             qPrintable(QStringLiteral("secret Tj bytes survive in the output stream\n%1")
                            .arg(hexDump(postStream))));
    // The neighbor Tj's operator bytes must be byte-identical — no unexpected
    // mutation anywhere in the keep line.
    const QByteArray postKeep = stringOpAtY(postStream, 550);
    QVERIFY2(!postKeep.isEmpty(),
             qPrintable(QStringLiteral("E-1: keep-line Tj lost/mutated in the output stream\n%1")
                            .arg(hexDump(postStream))));
    QVERIFY2(preKeep == postKeep,
             qPrintable(QStringLiteral("E-1: bytes around the keep line mutated\nPRE : %1\nPOST: %2")
                            .arg(hexDump(preKeep), hexDump(postKeep))));
}

void TestExcisionCorruption::multiLineMultiOpStreamKeepsNeighborsByteExact() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createMultiLinePdf(tmp, QStringLiteral("multi_line.pdf"));
    QVERIFY2(!pdf.isEmpty(), "multi-line fixture failed");

    const QByteArray preStream = decodedPageContent(pdf, 0);
    QVERIFY2(!preStream.isEmpty(), "could not read the fixture's content stream");

    PdfEditorEngine engine;
    QVERIFY(engine.loadDocumentForEditing(pdf));
    // Two rects: cover SECRET_ONE at (50,700) and SECRET_TWO at (50,600);
    // KEEP_ONE_TEXT (650) and KEEP_TWO_TEXT (550) sit between/below them in
    // the same multi-op stream.
    QVERIFY2(engine.applyRedactions(0, { QRectF(40, 130, 300, 30),
                                         QRectF(40, 230, 300, 30) }),
             "engine applyRedactions must succeed");
    const QString out = tmp.filePath(QStringLiteral("multi_line_redacted.pdf"));
    QVERIFY2(engine.saveDocument(out), "saveDocument must succeed");

    PdfiumBackend pdfium;
    QVERIFY(pdfium.loadDocument(out));
    const QString p0 = pdfium.extractText(0);
    QVERIFY2(!p0.contains(QStringLiteral("a@b.com")) && !p0.contains(QStringLiteral("c@d.org")),
             qPrintable(QStringLiteral("secrets survived the redaction: %1").arg(p0)));
    QVERIFY2(p0.count(QStringLiteral("PUBLIC_KEEP_TEXT")) == 2,
             qPrintable(QStringLiteral("E-1: the two keep lines must both survive intact "
                                      "(found %1 of 2): %2")
                            .arg(p0.count(QStringLiteral("PUBLIC_KEEP_TEXT"))).arg(p0)));

    // Byte level: both keep Tj operators identical before/after; both secret
    // Tj operators gone.
    const QByteArray postStream = decodedPageContent(out, 0);
    QVERIFY2(!postStream.isEmpty(), "could not read the output's content stream");
    for (double keepY : { 650.0, 550.0 }) {
        const QByteArray preOp = stringOpAtY(preStream, keepY);
        QVERIFY2(!preOp.isEmpty(), "keep Tj not found in the pre stream");
        const QByteArray postOp = stringOpAtY(postStream, keepY);
        QVERIFY2(!postOp.isEmpty(),
                 qPrintable(QStringLiteral("E-1: keep Tj at y=%1 lost/mutated in the output stream")
                                .arg(keepY)));
        QVERIFY2(preOp == postOp,
                 qPrintable(QStringLiteral("E-1: bytes around the keep line at y=%1 mutated\n"
                                           "PRE : %2\nPOST: %3")
                                .arg(keepY).arg(hexDump(preOp), hexDump(postOp))));
    }
    for (double secretY : { 700.0, 600.0 }) {
        const QByteArray preOp = stringOpAtY(preStream, secretY);
        QVERIFY2(!preOp.isEmpty(), "secret Tj not found in the pre stream");
        QVERIFY2(postStream.indexOf(preOp) < 0,
                 qPrintable(QStringLiteral("secret Tj bytes at y=%1 survive in the output stream")
                                .arg(secretY)));
    }
}

void TestExcisionCorruption::patternDrawnSecretAbortsAndPatternFreeControlProceeds() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // ── the abort direction: pattern-drawn secret must REFUSE, not black-box ──
    const QString pdf = createPatternPdf(tmp, QStringLiteral("pattern_secret.pdf"), true);
    QVERIFY2(!pdf.isEmpty(), "pattern-secret fixture failed");
    {   // Fixture evidence note: this PDFium's FPDFText extraction does not see
        // pattern streams, and the render probe over the synthetic pattern came
        // back unpainted — spec-compliant viewers DO paint tiling patterns, so
        // the black-box-over-live-data premise stands on the PDF spec (scn +
        // re f paints the pattern across the whole page), not on this PDFium's
        // synthetic-pattern handling. The pinned contract is structural: a
        // tiling pattern whose stream carries text operators must refuse.
        PdfiumBackend pdfium;
        QVERIFY(pdfium.loadDocument(pdf));
        const QImage rendered = pdfium.renderPage(0, 72);
        if (!rendered.isNull()) {
            const QColor c = rendered.pixelColor(30, 842 - 60);
            qWarning().noquote() << "G2 note: render probe at first pattern cell rgb"
                                 << c.red() << c.green() << c.blue();
        }
        const QString p0 = pdfium.extractText(0);
        qWarning().noquote() << "G2 note: FPDFText extraction sees"
                             << (p0.contains(QStringLiteral("PatternSecretOmega"))
                                     ? QStringLiteral("the pattern text")
                                     : QStringLiteral("NO pattern text (extraction "
                                                      "skips pattern streams)"));
    }
    {
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(pdf));
        QVERIFY2(!engine.applyRedactions(0, { QRectF(40, 100, 500, 600) }),
                 "G2: the pattern-text page must be REFUSED — a silent black box "
                 "over live pattern text is the data-loss defect");
        // Nothing was half-edited: the committed bytes still carry the pattern
        // fill op and the benign line in the page stream, and the PATTERN
        // stream (where the secret lives) is untouched.
        const QString out = tmp.filePath(QStringLiteral("pattern_secret_out.pdf"));
        QVERIFY2(engine.saveDocument(out), "saveDocument after refusal must succeed");
        const QByteArray postStream = decodedPageContent(out, 0);
        QVERIFY2(postStream.contains("/P1 scn") && postStream.contains("PUBLIC_KEEP_TEXT"),
                 "G2: the refusal must leave the page stream unmodified");
        {   // the pattern stream still holds the secret (no surgery ran)
            PoDoFo::PdfMemDocument d;
            d.Load(out.toUtf8().constData());
            auto& resDict = d.GetPages().GetPageAt(0).GetResources().GetObject()
                               .GetDictionary();
            auto* patternMap = resDict.FindKey(PoDoFo::PdfName("Pattern"));
            QVERIFY(patternMap != nullptr);
            if (patternMap->IsReference())
                patternMap = &d.GetObjects().MustGetObject(patternMap->GetReference());
            auto* p1 = patternMap->GetDictionary().FindKey(PoDoFo::PdfName("P1"));
            QVERIFY(p1 != nullptr);
            if (p1->IsReference())
                p1 = &d.GetObjects().MustGetObject(p1->GetReference());
            PoDoFo::charbuff buf;
            p1->GetStream()->CopyTo(buf);
            QVERIFY2(QByteArray(buf.data(), int(buf.size())).contains("PatternSecretOmega"),
                     "G2: the refusal must leave the pattern stream unmodified");
        }
    }

    // ── the positive control: a TEXT-FREE pattern must proceed ──────────────
    const QString controlPdf = createPatternPdf(tmp, QStringLiteral("pattern_free.pdf"), false);
    QVERIFY2(!controlPdf.isEmpty(), "pattern-free fixture failed");
    {
        PdfEditorEngine engine;
        QVERIFY(engine.loadDocumentForEditing(controlPdf));
        QVERIFY2(engine.applyRedactions(0, { QRectF(40, 100, 500, 600) }),
                 "G2 control: a text-free pattern must not refuse redaction");
        const QString out = tmp.filePath(QStringLiteral("pattern_free_out.pdf"));
        QVERIFY2(engine.saveDocument(out), "saveDocument must succeed");
        // The mark covers the direct-text line too, so the benign line is
        // legitimately excised; what matters is that surgery RAN (the numeric
        // TJ gap substitution) despite the text-free pattern being present.
        const QByteArray postStream = decodedPageContent(out, 0);
        QVERIFY2(!postStream.contains("(PUBLIC_KEEP_TEXT) Tj"),
                 "G2 control: the covered direct-text line must be excised");
        QVERIFY2(postStream.contains("/P1 scn"),
                 "G2 control: the text-free pattern fill must be untouched");
    }
}

QTEST_MAIN(TestExcisionCorruption)
#include "TestExcisionCorruption.moc"
