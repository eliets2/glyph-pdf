// SPDX-License-Identifier: Apache-2.0
// Audit 9.5 P0 regression test: PPTX overlay text must carry the intended
// ~1% alpha (<a:alpha val="1000"/>) so it is selectable but visually
// invisible over the slide image — previously it rendered solid black.
//
// SEP13-PPTX (refute-or-close, PARITY-GLM-REVIEW-2026-09-13 lead ~1199):
// the review flagged the PPTX writer's text/fontName emission for
// completeness, suspecting quotes/`;`/`<` could break the slide XML. The
// writer emits through QXmlStreamWriter (writeCharacters / writeAttribute),
// which escapes — these tests PROVE it: hostile text (quotes, semicolons,
// angle brackets, ampersands) and a hostile base-font name round-trip
// through a re-parse of the saved artifact byte-for-byte. The probe string
// below would be UNPARSEABLE XML if any of them leaked unescaped.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <QXmlStreamReader>
#include <zip.h>
#include "engines/ConversionManager.h"

class TestPptxOverlayAlpha : public QObject {
    Q_OBJECT
private slots:
    void slideXmlCarriesAlpha();
    void slideTextRoundTripsHostileCharacters();
    void slideTypefaceAttributeRoundTripsHostileName();
private:
    static QString createMinimalPdf(const QString& dir, const QString& name);
    // Hand-built (byte-exact Tj strings, unembedded WinAnsi Helvetica — same
    // idiom as TestExportPathBadge) so extraction delivers the exact bytes.
    static QString createSpecialCharsPdf(const QString& dir, const QString& name,
                                         const QString& text,
                                         const QByteArray& baseFont,
                                         const QString& secondText);
    struct SlideParts {
        bool ok = false;
        QString err;
        QStringList runTexts;      // every <a:t> content
        QStringList typefaces;     // every <a:latin typeface="..."> value
    };
    static SlideParts parseSlide1(const QString& pptxPath);
};

QString TestPptxOverlayAlpha::createMinimalPdf(const QString& dir, const QString& name) {
    // A text-bearing page so the PPTX exporter emits overlay text shapes.
    const QString path = dir + "/" + name;
    QPdfWriter w(path);
    w.setPageSize(QPageSize(QPageSize::A4));
    QPainter p(&w);
    p.drawText(100, 100, QStringLiteral("Overlay probe text"));
    p.end();
    return path;
}

QString TestPptxOverlayAlpha::createSpecialCharsPdf(const QString& dir,
                                                    const QString& name,
                                                    const QString& text,
                                                    const QByteArray& baseFont,
                                                    const QString& secondText)
{
    QByteArray lit = text.toLatin1();
    lit.replace('\\', "\\\\").replace('(', "\\(").replace(')', "\\)");
    QByteArray lit2 = secondText.toLatin1();
    lit2.replace('\\', "\\\\").replace('(', "\\(").replace(')', "\\)");
    const QByteArray content =
        "BT /F1 12 Tf 72 720 Td (" + lit + ") Tj ET\n"
        "BT /F2 12 Tf 72 700 Td (" + lit2 + ") Tj ET\n";
    const QByteArray objects[] = {
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n",
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n",
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
        "/Resources<</Font<</F1 5 0 R/F2 6 0 R>>>>>>endobj\n",
        "4 0 obj<</Length " + QByteArray::number(content.size()) + ">>stream\n"
            + content + "endstream endobj\n",
        "5 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica"
        "/Encoding/WinAnsiEncoding>>endobj\n",
        // Hostile base-font name: quote and semicolon are REGULAR characters
        // in a PDF name object, so this is a legal /BaseFont that becomes the
        // extracted fontName — and the writer's typeface ATTRIBUTE input.
        "6 0 obj<</Type/Font/Subtype/Type1/BaseFont/" + baseFont +
            "/Encoding/WinAnsiEncoding>>endobj\n",
    };
    QByteArray pdf = "%PDF-1.4\n";
    QList<qint64> offsets;
    for (const QByteArray& obj : objects) {
        offsets.append(pdf.size());
        pdf += obj;
    }
    const qint64 xrefOffset = pdf.size();
    pdf += "xref\n0 7\n0000000000 65535 f \n";
    for (qint64 off : offsets) {
        pdf += QByteArray::number(static_cast<qulonglong>(off)).rightJustified(10, '0')
               + " 00000 n \n";
    }
    pdf += "trailer<</Size 7/Root 1 0 R>>\nstartxref\n"
           + QByteArray::number(xrefOffset) + "\n%%EOF\n";

    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(pdf);
    return path;
}

TestPptxOverlayAlpha::SlideParts TestPptxOverlayAlpha::parseSlide1(const QString& pptxPath)
{
    SlideParts parts;
    int err = 0;
    zip_t* za = zip_open(pptxPath.toUtf8().constData(), ZIP_RDONLY, &err);
    if (!za) { parts.err = "cannot open the pptx as a zip"; return parts; }
    zip_file_t* f = zip_fopen(za, "ppt/slides/slide1.xml", 0);
    if (!f) { zip_close(za); parts.err = "slide1.xml missing"; return parts; }
    QByteArray xml;
    char buf[4096];
    zip_int64_t n;
    while ((n = zip_fread(f, buf, sizeof(buf))) > 0)
        xml.append(buf, static_cast<int>(n));
    zip_fclose(f);
    zip_close(za);

    QXmlStreamReader r(xml);
    bool inT = false;
    while (!r.atEnd()) {
        const auto tok = r.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            if (r.name() == QStringLiteral("t"))
                inT = true;
            else if (r.name() == QStringLiteral("latin"))
                parts.typefaces.append(
                    r.attributes().value("typeface").toString());
        } else if (tok == QXmlStreamReader::EndElement) {
            if (r.name() == QStringLiteral("t")) inT = false;
        } else if (tok == QXmlStreamReader::Characters && inT) {
            parts.runTexts.append(r.text().toString());
        }
    }
    if (r.hasError())
        parts.err = r.errorString();
    else
        parts.ok = true;
    return parts;
}

void TestPptxOverlayAlpha::slideXmlCarriesAlpha() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createMinimalPdf(tmp.path(), "in.pdf");
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.pptx");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::PowerPoint));

    // Open the generated package and inspect slide1.xml.
    int err = 0;
    zip_t* za = zip_open(out.toUtf8().constData(), ZIP_RDONLY, &err);
    QVERIFY2(za, "generated .pptx must open as a zip archive");
    zip_file_t* f = zip_fopen(za, "ppt/slides/slide1.xml", 0);
    QVERIFY2(f, "slide1.xml must exist in the pptx");
    QByteArray xml;
    char buf[4096];
    zip_int64_t n;
    while ((n = zip_fread(f, buf, sizeof(buf))) > 0)
        xml.append(buf, static_cast<int>(n));
    zip_fclose(f);
    zip_close(za);

    QVERIFY2(xml.contains("a:alpha"), "overlay run must declare an alpha element");
    QVERIFY2(xml.contains("val=\"1000\""), "overlay alpha must be ~1% (1000)");
}

// SEP13-PPTX: the exact probe string carries every character the review
// flagged. writeCharacters must escape them so the slide re-parses and the
// text survives verbatim.
void TestPptxOverlayAlpha::slideTextRoundTripsHostileCharacters() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString probe =
        QStringLiteral("Quote\"Apos;Semi<Angle&Amp>");
    const QString pdf = createSpecialCharsPdf(
        tmp.path(), "in.pdf", probe,
        QByteArrayLiteral("Helvetica"), QStringLiteral("Second"));
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.pptx");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::PowerPoint));

    const SlideParts parts = parseSlide1(out);
    QVERIFY2(parts.ok, qPrintable(QStringLiteral(
        "slide1.xml must re-parse cleanly with hostile text in it: %1")
            .arg(parts.err)));
    const QString joined = parts.runTexts.join(QChar(u' '));
    QVERIFY2(parts.runTexts.contains(probe),
             qPrintable(QStringLiteral(
                 "the hostile probe string must survive verbatim; runs=[%1]")
                     .arg(joined)));
}

// SEP13-PPTX (fontName path): the hostile /BaseFont becomes the extracted
// fontName written into <a:latin typeface="..."> — writeAttribute must
// escape it so the slide re-parses and the name survives verbatim.
void TestPptxOverlayAlpha::slideTypefaceAttributeRoundTripsHostileName() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createSpecialCharsPdf(
        tmp.path(), "in.pdf", QStringLiteral("First"),
        QByteArrayLiteral("Evil\"Quote;Name"), QStringLiteral("Second"));
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.pptx");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::PowerPoint));

    const SlideParts parts = parseSlide1(out);
    QVERIFY2(parts.ok, qPrintable(QStringLiteral(
        "slide1.xml must re-parse cleanly with a hostile font name in it: %1")
            .arg(parts.err)));
    QVERIFY2(parts.typefaces.contains(QStringLiteral("Evil\"Quote;Name")),
             qPrintable(QStringLiteral(
                 "the hostile typeface must survive as an attribute value; "
                 "typefaces=[%1]").arg(parts.typefaces.join(QStringLiteral(", ")))));
}

QTEST_MAIN(TestPptxOverlayAlpha)
#include "TestPptxOverlayAlpha.moc"
