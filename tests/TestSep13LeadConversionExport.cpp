// SPDX-License-Identifier: Apache-2.0
// SEP13 leads 2 + 13 + M7 — ConversionManager export path.
//
// PARITY-GLM-REVIEW-2026-09-13 leads exercised here:
//   lead 2 (exportToHtml ~358): el.fontName is concatenated RAW into the
//     style="...font-family: '%4';" attribute — a PDF font name containing
//     attribute metacharacters (' ; ") breaks out / injects CSS-HTML.
//   lead 13 (deriveColumns ~238): nearest-existing-anchor-within-tolerance
//     assignment misassigns runs of ragged (right-aligned) columns to
//     different spreadsheet columns; visible through CSV export.
//   M7 (clusterIntoRows ~193): the line-join tolerance qMax(1, 0.5*maxFont)
//     lets a large-font line swallow a small-font line ~half a big glyph
//     below it; visible through Text export.
//   XML probe (adjunct for the PPTX-writer low lead ~1199): documents what
//     QXmlStreamWriter does with writeAttribute AFTER writeEmptyElement and
//     proves attribute-value escaping.
//
// The lead probes assert the CORRECT contract and are expected to FAIL on
// candidate 83be3c2.
#include <QtTest/QtTest>
#include <QBuffer>
#include <QFile>
#include <QList>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "core/interfaces/IConversionEngine.h"
#include "engines/ConversionManager.h"

// TargetFormat is nested in IConversionEngine (global-namespace class).
using TargetFormat = IConversionEngine::TargetFormat;

namespace {

// Minimal PDF builder with PROGRAMMATIC xref offsets (PDFium parses this).
struct PdfObj { QByteArray body; };
QByteArray buildPdf(const QList<PdfObj>& objs) {
    QByteArray out = "%PDF-1.4\n";
    QList<qint64> offsets;
    for (int i = 0; i < objs.size(); ++i) {
        offsets.append(out.size());
        out += QByteArray::number(i + 1) + " 0 obj\n" + objs[i].body + "\nendobj\n";
    }
    const qint64 xrefAt = out.size();
    out += "xref\n0 " + QByteArray::number(objs.size() + 1) + "\n";
    out += "0000000000 65535 f \n";
    for (qint64 off : offsets)
        out += QByteArray::number(int(off)).rightJustified(10, '0') + " 00000 n \n";
    out += "trailer<</Size " + QByteArray::number(objs.size() + 1) + "/Root 1 0 R>>\n";
    out += "startxref\n" + QByteArray::number(int(xrefAt)) + "\n%%EOF\n";
    return out;
}

// One page, one Helvetica font, arbitrary content stream.
QByteArray onePagePdf(const QByteArray& contentStream, const QByteArray& baseFontName) {
    QList<PdfObj> objs;
    objs.append({ "<</Type/Catalog/Pages 2 0 R>>" });                                   // 1 catalog
    objs.append({ "<</Type/Pages/Kids[3 0 R]/Count 1>>" });                             // 2 pages
    objs.append({ "<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]"
                  "/Resources<</Font<</F1 4 0 R>>>>/Contents 5 0 R>>" });              // 3 page
    objs.append({ "<</Type/Font/Subtype/Type1/BaseFont/" + baseFontName +
                  "/Encoding/WinAnsiEncoding>>" });                                     // 4 font
    objs.append({ "<</Length " + QByteArray::number(contentStream.size()) + ">>\nstream\n" +
                  contentStream + "\nendstream" });                                     // 5 content
    return buildPdf(objs);
}

bool writeFile(const QString& path, const QByteArray& bytes) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const bool ok = f.write(bytes) == bytes.size();
    f.close();
    return ok;
}

QString readFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(f.readAll());
}

} // namespace

class TestSep13LeadConversionExport : public QObject {
    Q_OBJECT

private slots:
    // LEAD 2 CONFIRMATION (expected FAILURE on the candidate): a font name
    // carrying attribute metacharacters must not reach the HTML verbatim.
    void htmlExportEscapesFontName() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        // ';' ''' '"' are legal REGULAR characters inside a PDF name (only
        // delimiters ( ) < > [ ] { } / % and whitespace are forbidden), so a
        // real-world PDF can carry this BaseFont.
        const QByteArray evilFont = "EVIL;color:red'x\"y";
        const QByteArray pdf = onePagePdf("BT /F1 24 Tf 72 700 Td (HELLO) Tj ET", evilFont);
        const QString src = tmp.filePath("evilfont.pdf");
        QVERIFY(writeFile(src, pdf));

        const QString out = tmp.filePath("evilfont.html");
        ConversionManager conv;
        QVERIFY2(conv.convertTo(src, out, TargetFormat::Html),
                 "HTML export must succeed on the crafted PDF (PDFium parses it)");
        const QString html = readFile(out);
        QVERIFY2(!html.isEmpty(), "HTML output must not be empty");

        qInfo() << "font-family fragment present:"
                << (html.contains("font-family") ? "yes" : "no");
        const int famIdx = html.indexOf("font-family");
        if (famIdx >= 0)
            qInfo() << "style around font-family:" << html.mid(famIdx, 60);

        // CORRECT contract: the raw name must not survive into the attribute.
        QVERIFY2(!html.contains(QLatin1String("color:red"), Qt::CaseInsensitive),
                 "SEP13 lead 2 CONFIRMED: exportToHtml concatenated the raw PDF "
                 "font name into style=\"...\" — attribute injection "
                 "(semicolon/quote metacharacters survive unescaped)");
    }

    // LEAD 13 CONFIRMATION (expected FAILURE on the candidate): two runs that
    // visually share ONE right-aligned column ("Total" / "5") land in
    // DIFFERENT spreadsheet columns because the second run's ragged x-start
    // is beyond the nearest-anchor tolerance and becomes a NEW anchor.
    void csvKeepsRaggedRightAlignedColumnInOneColumn() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QByteArray pdf = onePagePdf(
            "BT /F1 12 Tf 260 700 Td (Total) Tj ET\n"
            "BT /F1 12 Tf 295 660 Td (5) Tj ET\n",
            "Helvetica");
        const QString src = tmp.filePath("ragged.pdf");
        QVERIFY(writeFile(src, pdf));

        const QString out = tmp.filePath("ragged.csv");
        ConversionManager conv;
        QVERIFY(conv.convertTo(src, out, TargetFormat::Csv));
        const QStringList lines = readFile(out).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QVERIFY2(lines.size() >= 2, "expected two extracted rows");
        qInfo() << "CSV row1:" << lines.at(0) << "| row2:" << lines.at(1);

        // CORRECT contract: "5" is visually in the SAME (only) column as
        // "Total" — row 2 must be a single cell, not ,"5" with an empty
        // leading cell invented by the geometry anchor mismatch.
        QVERIFY2(lines.at(1) == QStringLiteral("\"5\""),
                 QStringLiteral("SEP13 lead 13 CONFIRMED: deriveColumns split one visual column "
                                "into two spreadsheet columns (row2 = %1, expected \"5\")")
                     .arg(lines.at(1)).toUtf8().constData());
    }

    // M7 CONFIRMATION (expected FAILURE on the candidate): an 8pt line 10pt
    // below a 24pt line merges INTO the big line (tolerance 0.5*24 = 12pt).
    void textExportKeepsDistinctLinesSeparate() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QByteArray pdf = onePagePdf(
            "BT /F1 24 Tf 72 700 Td (BOLDHEAD) Tj ET\n"
            "BT /F1 8 Tf 72 690 Td (tiny detail) Tj ET\n",
            "Helvetica");
        const QString src = tmp.filePath("twosizes.pdf");
        QVERIFY(writeFile(src, pdf));


        const QString out = tmp.filePath("twosizes.txt");
        ConversionManager conv;
        QVERIFY(conv.convertTo(src, out, TargetFormat::Text));
        const QStringList lines = readFile(out).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        qInfo() << "text lines:" << lines;

        // CORRECT contract: two visually distinct lines → two rows. The defect
        // signature is the 8pt text MERGING INTO the 24pt row (one output row
        // carrying both texts), regardless of trailing blank lines.
        const QStringList nonEmpty = lines.filter(QRegularExpression("\\S"));
        QVERIFY2(nonEmpty.size() >= 2,
                 QStringLiteral("SEP13 M7 CONFIRMED: clusterIntoRows merged the 8pt line into "
                                "the 24pt line (half-max-font tolerance; %1 non-empty "
                                "line(s): %2)")
                     .arg(nonEmpty.size()).arg(nonEmpty.join(QLatin1Char('|'))).toUtf8().constData());
        for (const QString& l : nonEmpty) {
            QVERIFY2(!(l.contains(QLatin1String("BOLDHEAD"))
                       && l.contains(QLatin1String("tiny detail"))),
                     QStringLiteral("SEP13 M7 CONFIRMED: one output row carries BOTH the "
                                    "24pt and the 8pt line (\"%1\") — the half-max-font "
                                    "join tolerance swallowed the small line")
                         .arg(l.trimmed()).toUtf8().constData());
        }
    }

    // XML probe (PPTX-writer low lead adjunct): what does writeAttribute AFTER
    // writeEmptyElement produce, and are attribute values escaped?
    void xmlStreamWriterEmptyElementAttributeProbe() {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        QXmlStreamWriter xml(&buffer);
        xml.writeStartDocument();
        // Declare the real DrawingML namespace exactly as the PPTX package
        // does (the standalone probe XML must be well-formed for round-trip).
        xml.writeNamespace("http://schemas.openxmlformats.org/drawingml/2006/main", "a");
        xml.writeStartElement("a:rPr");
        xml.writeEmptyElement("a:latin");
        xml.writeAttribute("typeface", "Evil;color:red'&<X");
        xml.writeEndElement();
        xml.writeEndDocument();
        buffer.close();
        qInfo() << "produced bytes:" << bytes;

        // Escaping must hold regardless of placement (this REFUTES the
        // escaping half of the PPTX low lead if it passes).
        QVERIFY2(bytes.contains("&amp;") && !bytes.contains(QLatin1String("'&<X")),
                 "attribute values must be XML-escaped by QXmlStreamWriter");

        // Round-trip: the typeface must be recoverable as an ATTRIBUTE of
        // a:latin (this validates the writeEmptyElement+writeAttribute
        // pattern the PPTX writer uses).
        QBuffer in(&bytes);
        in.open(QIODevice::ReadOnly);
        QXmlStreamReader reader(&in);
        bool latinFound = false;
        QString typeface;
        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.isStartElement() && reader.name() == QLatin1String("latin")) {
                latinFound = true;
                typeface = reader.attributes().value("typeface").toString();
                break;
            }
        }
        qInfo() << "round-trip: latinFound =" << latinFound << "typeface =" << typeface;
        QVERIFY2(!reader.hasError(), "probe XML must stay well-formed");
        QVERIFY2(latinFound && typeface == QLatin1String("Evil;color:red'&<X"),
                 "writeAttribute after writeEmptyElement must land ON a:latin");
    }
};

#include "TestSep13LeadConversionExport.moc"
QTEST_MAIN(TestSep13LeadConversionExport)
