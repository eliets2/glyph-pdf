// SPDX-License-Identifier: Apache-2.0
// R09 / F07 regression test: ConversionManager's shared Word/Excel/CSV/Text
// extractor used to interpret Tj/TJ string bytes DIRECTLY as text, so a
// subset-font fixture exported glyph/control codes while the existing PDFium
// backend extracted the same page correctly. These fixtures pin that the
// CONVERSION path produces the same decoded Unicode PDFium produces:
//   * a hand-built subset-font PDF whose /Differences maps ASCII codes to
//     non-ASCII glyph names (code 'A' -> egrave, 'B' -> eacute);
//   * a standard-14 Helvetica with accented Latin written as octal escapes
//     in the Tj string (WinAnsiEncoding);
//   * multiline ordering (top-to-bottom) and run-to-run determinism;
//   * an image-only PDF -> honest empty result (no fabricated text);
//   * CSV quoting/escaping and HTML escaping of the decoded text.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <zip.h>
#include "engines/ConversionManager.h"
#include "engines/pdfium/PdfiumBackend.h"

class TestConversionExtraction : public QObject {
    Q_OBJECT
private slots:
    // The premise: the existing backend boundary already decodes these fonts.
    void backendPlainExtractDecodesSubsetEncoding();
    void backendPlainExtractDecodesWinAnsiAccents();
    // The complaint: the CONVERSION extractor must produce the same text.
    void subsetFontExportsDecodedUnicodeNotGlyphCodes();
    void accentedLatinOctalEscapesDecodeInConversion();
    void multilineOrderIsTopToBottomAndDeterministic();
    void imageOnlyPdfYieldsHonestEmptyResult();
    void csvQuotesAndCommasAreEscaped();
    void htmlAccentsDecodeAndMarkupEscapes();

private:
    // Subset-font fixture: unembedded Type1 whose /Differences encoding maps
    // the ASCII codes 'A'/'B' to the glyph names egrave/eacute. A decoder that
    // treats the Tj bytes as text sees "AB"; a real PDF decoder sees "èé".
    static QString createSubsetPdf(const QString& dir, const QString& name);
    // Standard-14 Helvetica with /Encoding/WinAnsiEncoding and accented Latin
    // written as octal escapes (\351 = 0xE9 = é in WinAnsi).
    static QString createAccentedPdf(const QString& dir, const QString& name);
    // Three lines: two separate BT/ET blocks (absolute Td) plus a relative
    // "0 -20 Td" move inside the second block.
    static QString createMultilinePdf(const QString& dir, const QString& name);
    // A page whose only content is a 1x1 uncompressed image XObject — no text
    // operators at all.
    static QString createImageOnlyPdf(const QString& dir, const QString& name);
    // One-line text PDF with arbitrary Latin-1-safe content.
    static QString createTextPdf(const QString& dir, const QString& name,
                                 const QStringList& lines);
    static QByteArray readFile(const QString& path);
};

// Shared hand-built-PDF writer: `objects` are the numbered object bodies
// (object 1..N); xref offsets are computed from the actual bytes.
static QString writePdf(const QString& path, const QList<QByteArray>& objects) {
    QByteArray pdf = "%PDF-1.4\n";
    QList<qint64> offsets;
    for (const QByteArray& obj : objects) {
        offsets.append(pdf.size());
        pdf += obj;
    }
    const qint64 xrefOffset = pdf.size();
    pdf += "xref\n0 " + QByteArray::number(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (qint64 off : offsets) {
        pdf += QByteArray::number(static_cast<qulonglong>(off)).rightJustified(10, '0')
               + " 00000 n \n";
    }
    pdf += "trailer<</Size " + QByteArray::number(objects.size() + 1)
           + "/Root 1 0 R>>\nstartxref\n" + QByteArray::number(xrefOffset) + "\n%%EOF\n";
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(pdf);
    return path;
}

static QByteArray contentStreamObj(const QByteArray& content) {
    return "4 0 obj<</Length " + QByteArray::number(content.size())
           + ">>stream\n" + content + "endstream endobj\n";
}

QString TestConversionExtraction::createSubsetPdf(const QString& dir, const QString& name) {
    QList<QByteArray> objects = {
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n",
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n",
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
        "/Resources<</Font<</F1 5 0 R>>>>>>endobj\n",
        contentStreamObj("BT /F1 12 Tf 72 700 Td (AB) Tj ET\n"),
        // Subset-style font: custom base name (subset prefix), no font file,
        // /Differences remapping ASCII codes to non-ASCII glyph names.
        "5 0 obj<</Type/Font/Subtype/Type1/BaseFont/AAAAAA+FakeSubset"
        "/Encoding<</Type/Encoding/BaseEncoding/WinAnsiEncoding"
        "/Differences[65/egrave 66/eacute]>>"
        "/FirstChar 65/LastChar 66/Widths[500 500]>>endobj\n",
    };
    return writePdf(dir + "/" + name, objects);
}

QString TestConversionExtraction::createAccentedPdf(const QString& dir, const QString& name) {
    QList<QByteArray> objects = {
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n",
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n",
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
        "/Resources<</Font<</F1 5 0 R>>>>>>endobj\n",
        // \351 = 0xE9 = eacute in WinAnsiEncoding: "café résumé".
        contentStreamObj("BT /F1 12 Tf 72 700 Td (caf\\351 r\\351sum\\351) Tj ET\n"),
        "5 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica"
        "/Encoding/WinAnsiEncoding>>endobj\n",
    };
    return writePdf(dir + "/" + name, objects);
}

QString TestConversionExtraction::createMultilinePdf(const QString& dir, const QString& name) {
    QList<QByteArray> objects = {
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n",
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n",
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
        "/Resources<</Font<</F1 5 0 R>>>>>>endobj\n",
        // Line 1 in its own BT/ET block; lines 2 and 3 share a block where
        // line 3 is reached with a RELATIVE Td move (ISO 32000-2 §9.4.1: BT
        // resets the text line matrix; Td offsets are relative to it).
        contentStreamObj(
            "BT /F1 12 Tf 72 720 Td (alpha line) Tj ET\n"
            "BT /F1 12 Tf 72 700 Td (beta line) Tj 0 -20 Td (gamma line) Tj ET\n"),
        "5 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>endobj\n",
    };
    return writePdf(dir + "/" + name, objects);
}

QString TestConversionExtraction::createImageOnlyPdf(const QString& dir, const QString& name) {
    QList<QByteArray> objects = {
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n",
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n",
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
        "/Resources<</XObject<</Im0 5 0 R>>>>>>endobj\n",
        contentStreamObj("q 100 0 0 100 72 72 cm /Im0 Do Q\n"),
        // 1x1 white RGB image, uncompressed.
        "5 0 obj<</Type/XObject/Subtype/Image/Width 1/Height 1"
        "/BitsPerComponent 8/ColorSpace/DeviceRGB/Length 3>>stream\n"
        "\xFF\xFF\xFF endstream endobj\n",
    };
    return writePdf(dir + "/" + name, objects);
}

QString TestConversionExtraction::createTextPdf(const QString& dir, const QString& name,
                                                const QStringList& lines) {
    QByteArray content;
    int y = 720;
    for (const QString& line : lines) {
        QByteArray lit = line.toLatin1();
        lit.replace('\\', "\\\\").replace('(', "\\(").replace(')', "\\)");
        content += "BT /F1 12 Tf 72 " + QByteArray::number(y) + " Td (" + lit + ") Tj ET\n";
        y -= 20;
    }
    QList<QByteArray> objects = {
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n",
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n",
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
        "/Resources<</Font<</F1 5 0 R>>>>>>endobj\n",
        contentStreamObj(content),
        // WinAnsiEncoding so bytes above 0x7F in the Tj strings decode as
        // intended (0xE9 = é); StandardEncoding (the no-Encoding default)
        // would map them differently — PDFium honors the real encoding.
        "5 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica"
        "/Encoding/WinAnsiEncoding>>endobj\n",
    };
    return writePdf(dir + "/" + name, objects);
}

QByteArray TestConversionExtraction::readFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

// The premise of R09: the existing PDFium backend boundary already extracts
// the subset-encoded text correctly (plan evidence: it got "Shared first page"
// right while the converter exported glyph codes).
void TestConversionExtraction::backendPlainExtractDecodesSubsetEncoding() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createSubsetPdf(tmp.path(), "subset.pdf");
    QVERIFY(!pdf.isEmpty());

    PdfiumBackend backend;
    QVERIFY(backend.loadDocument(pdf));
    const QString text = backend.extractText(0).trimmed();
    QVERIFY2(text.contains(QChar(0x00E8)) && text.contains(QChar(0x00E9)),
             qPrintable(QStringLiteral("PDFium must decode /Differences codes 65/66 to "
                                      "egrave/eacute; got %1").arg(text)));
    QVERIFY2(!text.contains(QLatin1String("AB")),
             qPrintable(QStringLiteral("the glyph codes 'AB' must not leak; got %1").arg(text)));
}

void TestConversionExtraction::backendPlainExtractDecodesWinAnsiAccents() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createAccentedPdf(tmp.path(), "accented.pdf");
    QVERIFY(!pdf.isEmpty());

    PdfiumBackend backend;
    QVERIFY(backend.loadDocument(pdf));
    const QString text = backend.extractText(0).trimmed();
    QVERIFY2(text.contains(QStringLiteral("caf") + QChar(0x00E9)),
             qPrintable(QStringLiteral("PDFium must decode the WinAnsi octal escapes; "
                                      "got %1").arg(text)));
}

// F07 core: convertTo's text export must carry the DECODED Unicode, not the
// raw Tj bytes of the subset font.
void TestConversionExtraction::subsetFontExportsDecodedUnicodeNotGlyphCodes() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createSubsetPdf(tmp.path(), "subset.pdf");
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.txt");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Text));

    const QString text = QString::fromUtf8(readFile(out));
    QVERIFY2(text.contains(QChar(0x00E8)) && text.contains(QChar(0x00E9)),
             qPrintable(QStringLiteral("conversion must export decoded egrave/eacute; "
                                      "got %1").arg(text)));
    QVERIFY2(!text.contains(QLatin1String("AB")),
             qPrintable(QStringLiteral("raw subset glyph codes 'AB' must not be exported; "
                                      "got %1").arg(text)));
}

void TestConversionExtraction::accentedLatinOctalEscapesDecodeInConversion() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createAccentedPdf(tmp.path(), "accented.pdf");
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.txt");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Text));

    const QString text = QString::fromUtf8(readFile(out));
    QVERIFY2(text.contains(QStringLiteral("caf") + QChar(0x00E9)),
             qPrintable(QStringLiteral("WinAnsi octal escapes must decode in conversion; "
                                      "got %1").arg(text)));
    QVERIFY2(!text.contains(QChar(0xFFFD)),
             qPrintable(QStringLiteral("no replacement chars may remain (raw-byte "
                                      "interpretation produced them); got %1").arg(text)));
}

// Row grouping (452bfa2 behavior kept): lines come out top-to-bottom, runs
// inside a block follow content order, and the whole pipeline is
// deterministic (two conversions produce identical bytes).
void TestConversionExtraction::multilineOrderIsTopToBottomAndDeterministic() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createMultilinePdf(tmp.path(), "multi.pdf");
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out1 = tmp.filePath("out1.txt");
    const QString out2 = tmp.filePath("out2.txt");
    QVERIFY(mgr.convertTo(pdf, out1, IConversionEngine::TargetFormat::Text));
    QVERIFY(mgr.convertTo(pdf, out2, IConversionEngine::TargetFormat::Text));

    const QStringList lines1 = QString::fromUtf8(readFile(out1)).split(QLatin1Char('\n'),
        Qt::SkipEmptyParts);
    QStringList content;
    for (const QString& l : lines1) {
        const QString t = l.trimmed();
        if (!t.isEmpty()) content << t;
    }
    QCOMPARE(content.size(), 3);
    QCOMPARE(content.at(0), QStringLiteral("alpha line"));
    QCOMPARE(content.at(1), QStringLiteral("beta line"));
    QCOMPARE(content.at(2), QStringLiteral("gamma line"));

    // Deterministic: identical bytes on a repeat run.
    QCOMPARE(readFile(out1), readFile(out2));
}

// An image-only PDF has no text layer: the conversion must yield an honest
// empty result — never fabricated text.
void TestConversionExtraction::imageOnlyPdfYieldsHonestEmptyResult() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createImageOnlyPdf(tmp.path(), "imageonly.pdf");
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;

    // Text export: succeeds but carries no text content (page separators only).
    const QString outTxt = tmp.filePath("out.txt");
    QVERIFY(mgr.convertTo(pdf, outTxt, IConversionEngine::TargetFormat::Text));
    const QString txt = QString::fromUtf8(readFile(outTxt));
    QVERIFY2(txt.trimmed().isEmpty(),
             qPrintable(QStringLiteral("text export of an image-only PDF must be empty; "
                                      "got %1").arg(txt)));

    // CSV export: no fabricated cells — either no file or whitespace only.
    const QString outCsv = tmp.filePath("out.csv");
    const bool csvOk = mgr.convertTo(pdf, outCsv, IConversionEngine::TargetFormat::Csv);
    const QByteArray csv = readFile(outCsv);
    QVERIFY2((!csvOk && csv.isEmpty()) || csv.trimmed().isEmpty(),
             qPrintable(QStringLiteral("csv export of an image-only PDF must stay empty; "
                                      "got %1").arg(csv)));

    // Word export: the OOXML document body must not contain fabricated runs.
    const QString outDocx = tmp.filePath("out.docx");
    QVERIFY(mgr.convertTo(pdf, outDocx, IConversionEngine::TargetFormat::Word));
    int err = 0;
    zip_t* za = zip_open(outDocx.toUtf8().constData(), ZIP_RDONLY, &err);
    QVERIFY2(za, "docx must open as a zip archive");
    zip_file_t* f = zip_fopen(za, "word/document.xml", 0);
    QVERIFY2(f, "word/document.xml must exist");
    QByteArray doc;
    char buf[4096];
    zip_int64_t n;
    while ((n = zip_fread(f, buf, sizeof(buf))) > 0)
        doc.append(buf, static_cast<int>(n));
    zip_fclose(f);
    zip_close(za);
    QVERIFY2(!doc.contains("<w:t"), "no fabricated w:t runs for an image-only PDF");
}

// CSV quoting/escaping of the (decoded) text: commas force quoting, embedded
// double quotes are doubled, accented text round-trips as UTF-8.
void TestConversionExtraction::csvQuotesAndCommasAreEscaped() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QStringList lines = {
        QStringLiteral("caf") + QChar(0x00E9) + QStringLiteral(" review"),
        QStringLiteral("He said \"hi\", ok"),
    };
    const QString pdf = createTextPdf(tmp.path(), "csv.pdf", lines);
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.csv");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Csv));

    const QByteArray csv = readFile(out);
    QVERIFY2(csv.contains("He said \"\"hi\"\", ok"),
             qPrintable(QStringLiteral("embedded quotes must be doubled and the cell "
                                      "quoted; got %1").arg(QString::fromUtf8(csv))));
    QVERIFY2(csv.contains(QByteArray("caf\xC3\xA9 review")),
             qPrintable(QStringLiteral("accented text must round-trip as UTF-8; got %1")
                                .arg(QString::fromUtf8(csv))));
    // The comma line must be a single quoted cell, not split into two.
    QVERIFY2(csv.contains("\"He said \"\"hi\"\", ok\""),
             "comma-bearing text must stay one quoted cell");
}

// HTML export: decoded accents as UTF-8, XML-escaped markup from the text.
void TestConversionExtraction::htmlAccentsDecodeAndMarkupEscapes() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QStringList lines = {
        QStringLiteral("caf") + QChar(0x00E9) + QStringLiteral(" <b>bold</b> & more"),
    };
    const QString pdf = createTextPdf(tmp.path(), "html.pdf", lines);
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.html");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Html));

    const QByteArray html = readFile(out);
    QVERIFY2(html.contains(QByteArray("caf\xC3\xA9")),
             qPrintable(QStringLiteral("accents must be decoded to UTF-8; got %1")
                                .arg(QString::fromUtf8(html))));
    QVERIFY2(html.contains("&amp;"), "& must be escaped");
    QVERIFY2(html.contains("&lt;b&gt;"), "<b> must be escaped, never emitted as markup");
}
QTEST_MAIN(TestConversionExtraction)
#include "TestConversionExtraction.moc"
