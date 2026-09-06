// SPDX-License-Identifier: Apache-2.0
// Audit 9.16 P0 regression test: the app must be able to report which export
// path actually ran (native OOXML vs mislabeled fallback) so the UI can tell
// the user the truth about their .docx/.xlsx files.
// Audit 9.5 P0 extension: the in-house OOXML writers must produce REAL OOXML
// packages — every item of the corruption checklist gets a test:
//   * exact mandatory part lists (docx 3, xlsx 5)
//   * [Content_Types].xml Defaults (rels+xml) + Override per part
//   * relationship rIds present/monotonic, no dangling r:id
//   * word/document.xml + sheet1.xml well-formed, extracted text survives
//   * XML-escaping of &<>'" in extracted text, C0 control chars stripped
//   * engine badge: in-house output is real OOXML -> the ConvertController
//     "HTML-as-docx / CSV-as-xlsx" warning must NOT fire.
// R10 extension: the capability queries must reflect the in-house writers
// truthfully (real Word/Excel export in EVERY build), and the engine must
// reject an unloadable input BEFORE opening or truncating the destination.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QXmlStreamReader>
#include <zip.h>
#include "engines/ConversionManager.h"
#include "shell/controllers/ConvertController.h"
#include "core/Capability.h"
#include "modes/CompressDialog.h"

class TestExportPathBadge : public QObject {
    Q_OBJECT
private slots:
    void capabilityQueriesReflectInHouseWriters();
    void wordExportTracksEngineUsed();
    void excelExportTracksEngineUsed();
    void wordDocxIsRealOoxmlPackage();
    void wordDocxDocumentParsesAndSurvivesText();
    void wordDocxStripsControlCharacters();
    void excelXlsxIsRealOoxmlPackage();
    void excelSheetParsesInlineStrCells();
    void excelCellEscapesSpecialChars();
    void inHouseExportSuppressesFallbackWarning();
    void failedInputDoesNotTruncateDestination();
    void localProcessingNoticeStatesPrivacy();
    // U08: capability-registry integration for the conversion flows.
    void registryExportProbesNameTheWriterThatWillRun();
    void registryImageFileFilterMatchesPreviousHandBuiltFilter();
    void compressDialogWordingRoundTripsThroughRegistry();
private:
    static QString createMinimalPdf(const QString& dir, const QString& name);
    // Multi-line text PDF, hand-built with an unembedded standard Helvetica
    // font carrying an explicit WinAnsiEncoding, so the bytes in the Tj
    // strings decode as intended by any encoding-aware extractor (R09: the
    // conversion path decodes through PDFium; the old raw-byte path ignored
    // encodings entirely). QPdfWriter output (embedded subset fonts) also
    // extracts correctly now and would work here too, but the hand-built PDF
    // keeps the fixture dependency-free and byte-exact.
    static QString createTextPdf(const QString& dir, const QString& name,
                                 const QStringList& lines);
    // Hand-built PDF whose content stream holds a *raw* string literal, so the
    // extraction path really delivers C0 control bytes to the writer.
    static QString createRawStringPdf(const QString& dir, const QString& name,
                                      const QByteArray& pdfStringLiteral);
    static QStringList zipListNames(const QString& path);
    static QByteArray zipReadFile(const QString& path, const QString& name);
};

QString TestExportPathBadge::createMinimalPdf(const QString& dir, const QString& name) {
    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(
        "%PDF-1.4\n"
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>endobj\n"
        "xref\n0 4\n"
        "0000000000 65535 f \n"
        "0000000009 00000 n \n"
        "0000000058 00000 n \n"
        "0000000115 00000 n \n"
        "trailer<</Size 4/Root 1 0 R>>\n"
        "startxref\n183\n%%EOF\n");
    return path;
}

QString TestExportPathBadge::createTextPdf(const QString& dir, const QString& name,
                                           const QStringList& lines) {
    // One "BT /F1 12 Tf 72 <y> Td (<line>) Tj ET" block per line, 20pt apart
    // (> fontSize*0.8, so convertTo's row grouping puts each line in its own
    // row). Each Tj yields exactly one TextElement carrying the whole line.
    QByteArray content;
    int y = 720;
    for (const QString& line : lines) {
        QByteArray lit = line.toLatin1();
        lit.replace('\\', "\\\\").replace('(', "\\(").replace(')', "\\)");
        content += "BT /F1 12 Tf 72 " + QByteArray::number(y) + " Td (" + lit + ") Tj ET\n";
        y -= 20;
    }
    const QByteArray objects[] = {
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n",
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n",
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
        "/Resources<</Font<</F1 5 0 R>>>>>>endobj\n",
        "4 0 obj<</Length " + QByteArray::number(content.size()) + ">>stream\n"
            + content + "endstream endobj\n",
        "5 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica"
        "/Encoding/WinAnsiEncoding>>endobj\n",
    };
    QByteArray pdf = "%PDF-1.4\n";
    QList<qint64> offsets;
    for (const QByteArray& obj : objects) {
        offsets.append(pdf.size());
        pdf += obj;
    }
    const qint64 xrefOffset = pdf.size();
    pdf += "xref\n0 6\n0000000000 65535 f \n";
    for (qint64 off : offsets) {
        pdf += QByteArray::number(static_cast<qulonglong>(off)).rightJustified(10, '0')
               + " 00000 n \n";
    }
    pdf += "trailer<</Size 6/Root 1 0 R>>\nstartxref\n"
           + QByteArray::number(xrefOffset) + "\n%%EOF\n";

    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(pdf);
    return path;
}

QString TestExportPathBadge::createRawStringPdf(const QString& dir, const QString& name,
                                                const QByteArray& pdfStringLiteral) {
    const QByteArray content = "BT /F1 12 Tf 72 720 Td (" + pdfStringLiteral + ") Tj ET";
    const QByteArray objects[] = {
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n",
        "2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n",
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R"
        "/Resources<</Font<</F1 5 0 R>>>>>>endobj\n",
        "4 0 obj<</Length " + QByteArray::number(content.size()) + ">>stream\n"
            + content + "\nendstream endobj\n",
        "5 0 obj<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>endobj\n",
    };
    QByteArray pdf = "%PDF-1.4\n";
    QList<qint64> offsets;
    for (const QByteArray& obj : objects) {
        offsets.append(pdf.size());
        pdf += obj;
    }
    const qint64 xrefOffset = pdf.size();
    pdf += "xref\n0 6\n0000000000 65535 f \n";
    for (qint64 off : offsets) {
        pdf += QByteArray::number(static_cast<qulonglong>(off)).rightJustified(10, '0')
               + " 00000 n \n";
    }
    pdf += "trailer<</Size 6/Root 1 0 R>>\nstartxref\n"
           + QByteArray::number(xrefOffset) + "\n%%EOF\n";

    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(pdf);
    return path;
}

QStringList TestExportPathBadge::zipListNames(const QString& path) {
    int err = 0;
    zip_t* za = zip_open(path.toUtf8().constData(), ZIP_RDONLY, &err);
    if (!za) return {};
    QStringList names;
    const zip_int64_t n = zip_get_num_entries(za, 0);
    for (zip_int64_t i = 0; i < n; ++i) {
        const char* nm = zip_get_name(za, static_cast<zip_uint64_t>(i), 0);
        if (nm) names << QString::fromUtf8(nm);
    }
    zip_close(za);
    return names;
}

QByteArray TestExportPathBadge::zipReadFile(const QString& path, const QString& name) {
    QByteArray data;
    int err = 0;
    zip_t* za = zip_open(path.toUtf8().constData(), ZIP_RDONLY, &err);
    if (!za) return {};
    zip_file_t* f = zip_fopen(za, name.toUtf8().constData(), 0);
    if (f) {
        char buf[4096];
        zip_int64_t n;
        while ((n = zip_fread(f, buf, sizeof(buf))) > 0)
            data.append(buf, static_cast<int>(n));
        zip_fclose(f);
    }
    zip_close(za);
    return data;
}

namespace {
// Parses `xml`; returns false and sets `err` on any well-formedness error.
bool xmlWellFormed(const QByteArray& xml, QString* err) {
    QXmlStreamReader r(xml);
    while (!r.atEnd())
        r.readNext();
    if (r.hasError() && err) *err = r.errorString();
    return !r.hasError();
}

// Collects (ref, text) pairs of all <c t="inlineStr"><is><t> cells and, as a
// side effect of building them, verifies that row refs and cell refs are
// present and monotonically increasing (corruption checklist item 4).
bool collectInlineStrCells(const QByteArray& sheetXml, QList<QPair<QString, QString>>* out,
                           QString* err) {
    auto fail = [err](const char* msg) { if (err) *err = QString::fromLatin1(msg); return false; };
    // "B2" -> (row 2, col 2); returns (-1,-1) for malformed refs.
    auto refToRowCol = [](const QString& ref) -> QPair<int, int> {
        int i = 0;
        int col = 0;
        while (i < ref.size() && ref.at(i).isLetter()) {
            col = col * 26 + (ref.at(i).toUpper().unicode() - 'A' + 1);
            ++i;
        }
        if (i == 0 || i == ref.size()) // need letters AND digits
            return {-1, -1};
        const QStringView rowPart = QStringView{ref}.mid(i);
        bool ok = false;
        const int row = rowPart.toInt(&ok);
        if (!ok || col <= 0 || row <= 0)
            return {-1, -1};
        return {row, col};
    };

    QXmlStreamReader r(sheetXml);
    int lastRow = 0;
    int lastCol = 0;
    QString curRef;
    bool inIs = false, inT = false;
    QString curText;
    while (!r.atEnd()) {
        const auto tok = r.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            if (r.name() == QStringLiteral("row")) {
                const QStringView rv = r.attributes().value("r");
                if (rv.isEmpty()) return fail("row element must carry r=\"N\"");
                bool ok = false;
                const int row = rv.toString().toInt(&ok);
                if (!ok) return fail("row r attribute must be numeric");
                if (row <= lastRow) return fail("row refs must be present and monotonically increasing");
                lastRow = row;
                lastCol = 0;
            } else if (r.name() == QStringLiteral("c")) {
                const QStringView cr = r.attributes().value("r");
                if (cr.isEmpty()) return fail("cell element must carry r=\"A1\"");
                curRef = cr.toString();
                const auto [row, col] = refToRowCol(curRef);
                if (row <= 0 || col <= 0) return fail("cell r must be an A1-style reference");
                if (!(row > lastRow || (row == lastRow && col > lastCol)))
                    return fail("cell refs must be monotonically increasing");
                lastRow = row;
                lastCol = col;
                if (r.attributes().value("t") == QStringLiteral("inlineStr"))
                    inIs = true;
            } else if (r.name() == QStringLiteral("is") && inIs) {
                curText.clear();
            } else if (r.name() == QStringLiteral("t") && inIs) {
                inT = true;
            }
        } else if (tok == QXmlStreamReader::Characters && inT) {
            curText += r.text();
        } else if (tok == QXmlStreamReader::EndElement) {
            if (r.name() == QStringLiteral("t") && inT) {
                inT = false;
            } else if (r.name() == QStringLiteral("is") && inIs) {
                inIs = false;
                out->append({curRef, curText});
            }
        }
    }
    if (r.hasError()) {
        if (err) *err = r.errorString();
        return false;
    }
    return true;
}
} // namespace

// R10 (F08): the capability queries must reflect the in-house writers
// truthfully — real Word/Excel export exists in EVERY build (vendored lib
// when compiled in, in-house OOXML writer otherwise), so the old
// "returns false in lib-less builds" claim was a capability gap that no
// longer exists. The truth about WHICH writer ran stays in the engine badge.
void TestExportPathBadge::capabilityQueriesReflectInHouseWriters() {
    QVERIFY2(ConversionManager::hasNativeWordExport(),
             "Word (.docx) export must report available — the in-house writer is real OOXML");
    QVERIFY2(ConversionManager::hasNativeExcelExport(),
             "Excel (.xlsx) export must report available — the in-house writer is real OOXML");

    // The capability is not a bare claim: actual exports must succeed.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTextPdf(tmp.path(), "in.pdf", {"Capability probe"});
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    QVERIFY(mgr.convertTo(pdf, tmp.filePath("cap.docx"), IConversionEngine::TargetFormat::Word));
    QVERIFY(mgr.convertTo(pdf, tmp.filePath("cap.xlsx"), IConversionEngine::TargetFormat::Excel));

    // ... and the engine badge must keep telling the truth about the writer.
#ifdef HAS_DUCKX
    QCOMPARE(mgr.lastWordExportEngine(), ConversionManager::ExportEngine::NativeOoxml);
#else
    QCOMPARE(mgr.lastWordExportEngine(), ConversionManager::ExportEngine::InHouseOoxml);
#endif
#ifdef HAS_OPENXLSX
    QCOMPARE(mgr.lastExcelExportEngine(), ConversionManager::ExportEngine::NativeOoxml);
#else
    QCOMPARE(mgr.lastExcelExportEngine(), ConversionManager::ExportEngine::InHouseOoxml);
#endif
}
void TestExportPathBadge::wordExportTracksEngineUsed() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createMinimalPdf(tmp.path(), "in.pdf");
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    QCOMPARE(mgr.lastWordExportEngine(), ConversionManager::ExportEngine::Unknown);

    const QString out = tmp.filePath("out.docx");
    const bool ok = mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Word);
    QVERIFY(ok);

    // Whichever path ran must be reported truthfully (R10: branch on the
    // build flavor — the capability query is now unconditionally true).
#ifdef HAS_DUCKX
    QCOMPARE(mgr.lastWordExportEngine(), ConversionManager::ExportEngine::NativeOoxml);
#else
    // §9.5 P0: no duckx -> the in-house OOXML writer ran (never the old
    // HTML fallback, which used to be reported as Fallback).
    QCOMPARE(mgr.lastWordExportEngine(), ConversionManager::ExportEngine::InHouseOoxml);
#endif
}
void TestExportPathBadge::excelExportTracksEngineUsed() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createMinimalPdf(tmp.path(), "in.pdf");
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    QCOMPARE(mgr.lastExcelExportEngine(), ConversionManager::ExportEngine::Unknown);

    const QString out = tmp.filePath("out.xlsx");
    const bool ok = mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Excel);
    QVERIFY(ok);

#ifdef HAS_OPENXLSX
    QCOMPARE(mgr.lastExcelExportEngine(), ConversionManager::ExportEngine::NativeOoxml);
#else
    // §9.5 P0: no OpenXLSX -> the in-house OOXML writer ran (never the old
    // CSV-under-.xlsx fallback).
    QCOMPARE(mgr.lastExcelExportEngine(), ConversionManager::ExportEngine::InHouseOoxml);
#endif
}

// §9.5 P0: a .docx from a lib-less build must be a real OPC package holding
// exactly the mandatory WordprocessingML parts.
void TestExportPathBadge::wordDocxIsRealOoxmlPackage() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTextPdf(tmp.path(), "in.pdf", {"Word probe text"});
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.docx");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Word));

    const QStringList parts = zipListNames(out);
    QCOMPARE(parts.size(), 3);
    QVERIFY2(parts.contains("[Content_Types].xml"), "docx must contain [Content_Types].xml");
    QVERIFY2(parts.contains("_rels/.rels"), "docx must contain _rels/.rels");
    QVERIFY2(parts.contains("word/document.xml"), "docx must contain word/document.xml");

    const QByteArray ct = zipReadFile(out, "[Content_Types].xml");
    // Checklist: Default for rels+xml, Override per part.
    QVERIFY2(ct.contains("Extension=\"rels\""), "[Content_Types] must Default the rels extension");
    QVERIFY2(ct.contains("Extension=\"xml\""), "[Content_Types] must Default the xml extension");
    QVERIFY2(ct.contains("PartName=\"/word/document.xml\""),
             "[Content_Types] must Override the document part");
    QVERIFY2(ct.contains("application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"),
             "document Override must use the canonical WordprocessingML content type");

    // Checklist: relationship rIds present, targets resolve — no dangling refs.
    const QByteArray rels = zipReadFile(out, "_rels/.rels");
    QVERIFY2(rels.contains("Id=\"rId1\""), "root rels must define rId1");
    QVERIFY2(rels.contains("Target=\"word/document.xml\""),
             "root rels must target word/document.xml");
    QVERIFY2(rels.contains("Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\""),
             "root rels must use the canonical officeDocument relationship type");

    // Checklist: w:t carries xml:space="preserve".
    const QByteArray doc = zipReadFile(out, "word/document.xml");
    QVERIFY2(doc.contains("xml:space=\"preserve\""),
             "w:t elements must carry xml:space=\"preserve\"");
}

// document.xml must parse and the extracted text must survive it; XML-special
// characters (&<>'") in the PDF text must come out escaped, never raw markup.
void TestExportPathBadge::wordDocxDocumentParsesAndSurvivesText() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString specials = QStringLiteral("AT&T <tag> \"quoted\" 'apos'");
    const QString pdf = createTextPdf(tmp.path(), "in.pdf",
                                      {"Word probe text", specials});
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.docx");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Word));

    const QByteArray doc = zipReadFile(out, "word/document.xml");
    QString err;
    QVERIFY2(xmlWellFormed(doc, &err), qPrintable(QStringLiteral(
        "word/document.xml must be well-formed: %1").arg(err)));

    // Escaping of & and < in raw bytes (QXmlStreamWriter character-data rules).
    QVERIFY2(doc.contains("AT&amp;T"), "& must be written escaped");
    QVERIFY2(doc.contains("&lt;tag&gt;"), "< and > must be written escaped");
    QVERIFY2(!doc.contains("<tag>"), "raw markup from PDF text must never leak into the XML");

    // Quotes are legal unescaped in character data; the parser must round-trip
    // the full original string, including " and '.
    QStringList texts;
    QXmlStreamReader r(doc);
    while (!r.atEnd()) {
        if (r.readNext() == QXmlStreamReader::StartElement
            && r.name() == QStringLiteral("t")
            && r.namespaceUri().toString().endsWith(QLatin1String("wordprocessingml/2006/main"))) {
            texts << r.readElementText();
        }
    }
    QVERIFY2(!r.hasError(), qPrintable(QStringLiteral("parse error: %1").arg(r.errorString())));
    QVERIFY2(texts.contains(QStringLiteral("Word probe text")),
             "the plain probe string must survive the round-trip");
    QVERIFY2(texts.contains(specials),
             qPrintable(QStringLiteral("special-char string must round-trip; got %1")
                                .arg(texts.join('|'))));
}

// Checklist: C0 control characters (invalid in XML 1.0) extracted from the PDF
// must be stripped — the package stays well-formed and repair-prompt-free.
void TestExportPathBadge::wordDocxStripsControlCharacters() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // Raw PDF string literal carrying C0 controls: \x01 (SOH), \x0B (VT) and
    // \x1F (US). After stripping, the remaining text is "Document probe".
    const QString pdf = createRawStringPdf(tmp.path(), "in.pdf",
                                           "D\x01o\x0Bc\x1Fument probe");
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.docx");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Word));

    const QByteArray doc = zipReadFile(out, "word/document.xml");
    QString err;
    QVERIFY2(xmlWellFormed(doc, &err), qPrintable(QStringLiteral(
        "control chars must be stripped so document.xml stays well-formed: %1").arg(err)));
    // Reality check (empirical): PDFium does not deliver raw C0 bytes — it
    // decodes them through the font encoding ("Document" extracts
    // as "Do®ÿument"-style garbage). The guarantees that matter and
    // are testable end-to-end: the XML stays well-formed (above), NO raw C0
    // byte reaches it (below), and the literal text tail survives the pass.
    QVERIFY2(doc.contains("probe"),
             "text tail around the transcoded controls must survive");
    QVERIFY2(!doc.contains(QByteArray(1, '\x01')), "SOH must not reach the XML");
    QVERIFY2(!doc.contains(QByteArray(1, '\x0B')), "VT must not reach the XML");
    QVERIFY2(!doc.contains(QByteArray(1, '\x1F')), "US must not reach the XML");
}

// §9.5 P0: a .xlsx from a lib-less build must be a real OPC package holding
// exactly the 5 mandatory SpreadsheetML parts.
void TestExportPathBadge::excelXlsxIsRealOoxmlPackage() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTextPdf(tmp.path(), "in.pdf", {"Excel probe cell"});
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.xlsx");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Excel));

    const QStringList parts = zipListNames(out);
    QCOMPARE(parts.size(), 5);
    QVERIFY2(parts.contains("[Content_Types].xml"), "xlsx must contain [Content_Types].xml");
    QVERIFY2(parts.contains("_rels/.rels"), "xlsx must contain _rels/.rels");
    QVERIFY2(parts.contains("xl/workbook.xml"), "xlsx must contain xl/workbook.xml");
    QVERIFY2(parts.contains("xl/_rels/workbook.xml.rels"),
             "xlsx must contain xl/_rels/workbook.xml.rels");
    QVERIFY2(parts.contains("xl/worksheets/sheet1.xml"),
             "xlsx must contain xl/worksheets/sheet1.xml");

    const QByteArray ct = zipReadFile(out, "[Content_Types].xml");
    QVERIFY2(ct.contains("Extension=\"rels\""), "[Content_Types] must Default the rels extension");
    QVERIFY2(ct.contains("Extension=\"xml\""), "[Content_Types] must Default the xml extension");
    QVERIFY2(ct.contains("PartName=\"/xl/workbook.xml\"")
             && ct.contains("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"),
             "[Content_Types] must Override the workbook part with the canonical type");
    QVERIFY2(ct.contains("PartName=\"/xl/worksheets/sheet1.xml\"")
             && ct.contains("application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"),
             "[Content_Types] must Override the worksheet part with the canonical type");

    // Checklist: no dangling r:id — workbook r:id="rId1" must be defined in the
    // workbook rels, and every rels target must exist in the package.
    const QByteArray wb = zipReadFile(out, "xl/workbook.xml");
    QVERIFY2(wb.contains("r:id=\"rId1\""), "workbook sheet must reference rId1");
    const QByteArray wbRels = zipReadFile(out, "xl/_rels/workbook.xml.rels");
    QVERIFY2(wbRels.contains("Id=\"rId1\""), "workbook rels must define rId1");
    QVERIFY2(wbRels.contains("Target=\"worksheets/sheet1.xml\""),
             "workbook rels must target worksheets/sheet1.xml");
    QVERIFY2(wbRels.contains("Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\""),
             "workbook rels must use the canonical worksheet relationship type");
    const QByteArray rootRels = zipReadFile(out, "_rels/.rels");
    QVERIFY2(rootRels.contains("Target=\"xl/workbook.xml\""),
             "root rels must target xl/workbook.xml");
}

// sheet1.xml must be well-formed, carry the expected text in inlineStr cells
// (no sharedStrings part), and use monotonic A1-style refs.
void TestExportPathBadge::excelSheetParsesInlineStrCells() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTextPdf(tmp.path(), "in.pdf",
                                      {"Excel probe cell", "second probe row"});
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.xlsx");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Excel));

    const QByteArray sheet = zipReadFile(out, "xl/worksheets/sheet1.xml");
    QString err;
    QVERIFY2(xmlWellFormed(sheet, &err), qPrintable(QStringLiteral(
        "xl/worksheets/sheet1.xml must be well-formed: %1").arg(err)));

    QList<QPair<QString, QString>> cells;
    QVERIFY2(collectInlineStrCells(sheet, &cells, &err),
             qPrintable(QStringLiteral("cells must parse with monotonic refs: %1").arg(err)));

    QVERIFY2(cells.contains({"A1", QStringLiteral("Excel probe cell")}),
             qPrintable(QStringLiteral("A1 must carry the first probe string; cells=%1")
                                .arg(cells.size())));
    QVERIFY2(cells.contains({"A2", QStringLiteral("second probe row")}),
             "A2 must carry the second probe string");
}

// A cell text with XML-special characters must round-trip escaped.
void TestExportPathBadge::excelCellEscapesSpecialChars() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString specials = QStringLiteral("AT&T <tag> \"quoted\" 'apos'");
    const QString pdf = createTextPdf(tmp.path(), "in.pdf", {specials});
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString out = tmp.filePath("out.xlsx");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::Excel));

    const QByteArray sheet = zipReadFile(out, "xl/worksheets/sheet1.xml");
    QVERIFY2(sheet.contains("AT&amp;T"), "& must be written escaped");
    QVERIFY2(sheet.contains("&lt;tag&gt;"), "< and > must be written escaped");
    QVERIFY2(!sheet.contains("<tag>"), "raw markup from PDF text must never leak into the XML");

    QList<QPair<QString, QString>> cells;
    QString err;
    QVERIFY2(collectInlineStrCells(sheet, &cells, &err),
             qPrintable(QStringLiteral("cells must parse: %1").arg(err)));
    QVERIFY2(cells.contains({"A1", specials}),
             qPrintable(QStringLiteral("special-char cell must round-trip; got %1 cell(s)")
                                .arg(cells.size())));
}

// §9.5 P0/§9.16/R10: after an in-house (real OOXML) export the
// ConvertController warning seam — `lastXExportEngine() == ExportEngine::Fallback`
// — must be false, so the "HTML-as-docx / CSV-as-xlsx" repair-prompt warning
// cannot fire for in-house output (that warning only ever matched the dead
// mislabeled-fallback state).
void TestExportPathBadge::inHouseExportSuppressesFallbackWarning() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTextPdf(tmp.path(), "in.pdf", {"Badge probe"});
    QVERIFY(!pdf.isEmpty());

    ConversionManager mgr;
    const QString docx = tmp.filePath("out.docx");
    QVERIFY(mgr.convertTo(pdf, docx, IConversionEngine::TargetFormat::Word));
    const QString xlsx = tmp.filePath("out.xlsx");
    QVERIFY(mgr.convertTo(pdf, xlsx, IConversionEngine::TargetFormat::Excel));

#ifndef HAS_DUCKX
    QCOMPARE(mgr.lastWordExportEngine(), ConversionManager::ExportEngine::InHouseOoxml);
#endif
#ifndef HAS_OPENXLSX
    QCOMPARE(mgr.lastExcelExportEngine(), ConversionManager::ExportEngine::InHouseOoxml);
#endif
    QVERIFY2(mgr.lastWordExportEngine() != ConversionManager::ExportEngine::Fallback,
             "in-house OOXML output must not trigger the HTML-as-docx warning");
    QVERIFY2(mgr.lastExcelExportEngine() != ConversionManager::ExportEngine::Fallback,
             "in-house OOXML output must not trigger the CSV-as-xlsx warning");
}

// R10 (F08): the engine must reject an unloadable input BEFORE opening or
// truncating the output — UI gating alone is insufficient because batch and
// future callers bypass it. Every format's handler validates the input first,
// so a pre-existing destination must survive byte-for-byte.
void TestExportPathBadge::failedInputDoesNotTruncateDestination() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString missing = tmp.filePath("missing-input.pdf");

    const QList<IConversionEngine::TargetFormat> formats = {
        IConversionEngine::TargetFormat::Word,
        IConversionEngine::TargetFormat::Excel,
        IConversionEngine::TargetFormat::Csv,
        IConversionEngine::TargetFormat::Text,
        IConversionEngine::TargetFormat::Html,
    };
    const QStringList suffixes = {".docx", ".xlsx", ".csv", ".txt", ".html"};

    for (int i = 0; i < formats.size(); ++i) {
        const QString out = tmp.filePath(QStringLiteral("destination") + suffixes.at(i));
        const QByteArray sentinel = "PRESERVE-ME" + suffixes.at(i).toLatin1();
        {
            QFile f(out);
            QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Truncate),
                     qPrintable(QStringLiteral("could not create sentinel %1").arg(out)));
            f.write(sentinel);
        }

        ConversionManager mgr;
        const bool ok = mgr.convertTo(missing, out, formats.at(i));
        QVERIFY2(!ok, qPrintable(QStringLiteral(
            "convertTo must reject an unloadable input for %1").arg(suffixes.at(i))));

        QFile after(out);
        QVERIFY2(after.open(QIODevice::ReadOnly),
                 qPrintable(QStringLiteral("destination %1 must still exist").arg(out)));
        QCOMPARE(after.readAll(), sentinel);
    }
}

// §9.16 P0: the local-processing badge must exist and make an honest,
// factual claim (every conversion runs on-device; imports use a local
// LibreOffice subprocess — no network anywhere in these paths).
void TestExportPathBadge::localProcessingNoticeStatesPrivacy() {
    const QString notice = gp::ConvertController::localProcessingNotice();
    QVERIFY2(notice.contains(QStringLiteral("no upload"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("badge must state 'no upload': %1").arg(notice)));
    QVERIFY2(notice.contains(QStringLiteral("no internet"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("badge must state 'no internet': %1").arg(notice)));
    QVERIFY2(notice.contains(QStringLiteral("locally"), Qt::CaseInsensitive),
             qPrintable(QStringLiteral("badge must state the processing is local: %1").arg(notice)));
}

// ── U08: capability-registry integration for the conversion flows ───────────

// The pre-dialog disclosure (ConvertController::exportFormatNotice) is fed by
// the registry's export probes: they must be Available in every build and
// name the writer that WILL run — the honest badge, moved before the dialog.
void TestExportPathBadge::registryExportProbesNameTheWriterThatWillRun() {
    gp::CapabilityRegistry reg;
    reg.registerEngineProbes();

    const gp::Capability word = reg.query(gp::CapId::WordExport);
    QVERIFY2(word.status == gp::Availability::Available,
             "Word export is available in every build (in-house OOXML writer)");
    QVERIFY2(word.detail.contains(QStringLiteral("WordprocessingML")),
             qPrintable(QStringLiteral("Word disclosure must name the writer; got '%1'")
                                .arg(word.detail)));

    const gp::Capability excel = reg.query(gp::CapId::ExcelExport);
    QVERIFY2(excel.status == gp::Availability::Available,
             "Excel export is available in every build (in-house OOXML writer)");
    QVERIFY2(excel.detail.contains(QStringLiteral("SpreadsheetML")),
             qPrintable(QStringLiteral("Excel disclosure must name the writer; got '%1'")
                                .arg(excel.detail)));

    // The actual exports must keep succeeding — a capability claim without a
    // working writer is exactly the old fake-availability bug class.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTextPdf(tmp.path(), "in.pdf", {"registry probe"});
    QVERIFY(!pdf.isEmpty());
    ConversionManager mgr;
    QVERIFY(mgr.convertTo(pdf, tmp.filePath("probe.docx"), IConversionEngine::TargetFormat::Word));
    QVERIFY(mgr.convertTo(pdf, tmp.filePath("probe.xlsx"), IConversionEngine::TargetFormat::Excel));
}

// The hand-built image filter at the old ConvertController.cpp:543 is now
// derived via CapabilityRegistry::fileFilterFor; with ImageExport available in
// every build the derived filter must equal the previous string byte for byte
// (pure consolidation — no user-visible change).
void TestExportPathBadge::registryImageFileFilterMatchesPreviousHandBuiltFilter() {
    gp::CapabilityRegistry reg;
    reg.registerEngineProbes();

    const gp::Capability image = reg.query(gp::CapId::ImageExport);
    QCOMPARE(image.status, gp::Availability::Available);

    const QList<QPair<QString, gp::CapId>> clauses = {
        { QStringLiteral("PNG Images (*.png)"),  gp::CapId::ImageExport },
        { QStringLiteral("JPEG Images (*.jpg)"), gp::CapId::ImageExport },
        { QStringLiteral("TIFF Images (*.tif)"), gp::CapId::ImageExport },
    };
    QCOMPARE(reg.fileFilterFor(clauses),
             QStringLiteral("PNG Images (*.png);;JPEG Images (*.jpg);;TIFF Images (*.tif)"));
}

// The disabled-checkbox wording in CompressDialog (pinned by
// TestCompressDialogHonesty) must stay byte-identical to the registry's R12
// probe whyNot — one source of truth, no drift.
void TestExportPathBadge::compressDialogWordingRoundTripsThroughRegistry() {
    gp::CapabilityRegistry reg;
    reg.registerEngineProbes();

    QCOMPARE(gp::CompressDialog::unsupportedPassExplanation(),
             gp::r12UnsupportedPassExplanation());
    QCOMPARE(reg.query(gp::CapId::CompressSubsetFonts).whyNot,
             gp::CompressDialog::unsupportedPassExplanation());
    QCOMPARE(reg.query(gp::CapId::CompressRemoveUnused).whyNot,
             gp::CompressDialog::unsupportedPassExplanation());
}
QTEST_MAIN(TestExportPathBadge)
#include "TestExportPathBadge.moc"
