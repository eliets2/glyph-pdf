// SPDX-License-Identifier: Apache-2.0
// Audit 9.6 P0 regression test: fillForm must REPORT requested values that it
// could not apply (unknown field names, Radio/PushButton targets) through the
// unsupportedFields out-parameter instead of silently dropping them.
//
// PARITY-SCORECARD-2026-09-30 §4 row 8 pins: importFormData's FDF/CSV parsers
// are attacker-reachable input. Every malformed-hostile-input pin below
// asserts REFUSAL (import fails, no output file is written — fail-closed,
// never a half-import) or exact round-trip fidelity for hostile-but-legal
// values (embedded delimiters/newlines/parens). A refusal pin that passed
// against the old regex/split parser would prove nothing, so each refusal
// case is chosen so the OLD parser returned true (silent acceptance or
// silent corruption).
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "engines/FormManager.h"

class TestFillFormNoOp : public QObject {
    Q_OBJECT
private slots:
    void unknownFieldNameIsReported();

    // ── Row 8: malformed-input refusal pins (fail-closed, no half-import) ──
    void truncatedFdfIsRefused();
    void unterminatedFdfStringIsRefused();
    void overLargeFieldCountIsRefused();
    void overLargeValueIsRefused();
    void oversizedInputFileIsRefused();
    void nonUtf8BytesAreRefused();
    void emptyAndWhitespaceOnlyInputIsRefused();
    void fdfWithoutFieldsArrayIsRefused();
    void malformedCsvRecordIsRefusedNotHalfImported();

    // ── Row 8: hostile-but-legal values must survive byte-exact ────────────
    void csvEmbeddedDelimitersAndNewlineRoundTrip();
    void fdfEmbeddedParensBackslashNewlineRoundTrip();
    void validFdfRoundTrip();
    void validCsvRoundTrip();

private:
    static QString createTestPdf(const QString& dir, const QString& name);
    // A one-text-field form PDF ("known") — the minimum target for a real
    // import (fillForm runs after parsing succeeds).
    static QString createFormPdf(const QString& dir, const QString& name);
    static bool writeBytes(const QString& path, const QByteArray& bytes);
    static QByteArray readNormalized(const QString& path);  // \r\n -> \n
};

QString TestFillFormNoOp::createTestPdf(const QString& dir, const QString& name) {
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

QString TestFillFormNoOp::createFormPdf(const QString& dir, const QString& name) {
    const QString pdf = createTestPdf(dir, name);
    if (pdf.isEmpty()) return {};
    FormManager fm;
    if (!fm.addTextField(pdf, 0, QRectF(72, 72, 144, 24), QStringLiteral("known"), pdf))
        return {};
    return pdf;
}

bool TestFillFormNoOp::writeBytes(const QString& path, const QByteArray& bytes) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;  // raw bytes, no Text translation
    return f.write(bytes) == bytes.size();
}

QByteArray TestFillFormNoOp::readNormalized(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QByteArray b = f.readAll();
    b.replace("\r\n", "\n");  // export writes in Text mode (CRLF on Windows)
    return b;
}

void TestFillFormNoOp::unknownFieldNameIsReported() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createTestPdf(tmp.path(), "noop.pdf");
    QVERIFY(!pdf.isEmpty());
    FormManager fm;
    QVERIFY(fm.addTextField(pdf, 0, QRectF(72, 72, 144, 24), QStringLiteral("known"), pdf));

    QVariantMap data;
    data[QStringLiteral("known")] = QStringLiteral("v");
    data[QStringLiteral("radio_group_that_does_not_exist")] = QStringLiteral("1");
    QStringList unsupported;
    QVERIFY(fm.fillForm(pdf, data, pdf, /*lockFields=*/false, &unsupported));
    QVERIFY2(unsupported.contains(QStringLiteral("radio_group_that_does_not_exist")),
             "a requested value that could not be applied must be reported, not silently dropped");
    QVERIFY(!unsupported.contains(QStringLiteral("known")));
}

// ── Row 8 refusal pins ─────────────────────────────────────────────────────

// FDF cut off at EOF in the middle of a /V literal string: the old regex
// simply matched nothing / truncated and the import "succeeded". The
// hardened parser must refuse (truncated input, not a partial import).
void TestFillFormNoOp::truncatedFdfIsRefused() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    const QString data = tmp.path() + "/cut.fdf";
    const QString out = tmp.path() + "/out.pdf";
    QVERIFY(writeBytes(data,
        "%FDF-1.2\n1 0 obj\n<< /FDF << /Fields [\n<< /T (known) /V (va"));
    FormManager fm;
    QStringList unsupported;
    QVERIFY2(!fm.importFormData(pdf, data, out, &unsupported),
             "a truncated FDF must be refused, not silently imported");
    QVERIFY2(!QFileInfo::exists(out),
             "a refused import must not write an output file (fail-closed)");
}

// Literal string with an unbalanced nesting paren, EOF before the close:
// exercises depth tracking (the old `(.*?)\)` regex stopped at the first
// ')' and produced a corrupted, silently-truncated value).
void TestFillFormNoOp::unterminatedFdfStringIsRefused() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    const QString data = tmp.path() + "/unterm.fdf";
    const QString out = tmp.path() + "/out.pdf";
    QVERIFY(writeBytes(data,
        "%FDF-1.2\n1 0 obj\n<< /FDF << /Fields [\n<< /T (known) /V (a(b"));
    FormManager fm;
    QVERIFY2(!fm.importFormData(pdf, data, out, nullptr),
             "an FDF string open at EOF must be refused");
    QVERIFY2(!QFileInfo::exists(out),
             "a refused import must not write an output file (fail-closed)");
}

// Field-count cap: 10,001 field entries must be refused outright — the old
// regex imported all of them (and fillForm then churned through every one).
void TestFillFormNoOp::overLargeFieldCountIsRefused() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    const QString data = tmp.path() + "/many.fdf";
    const QString out = tmp.path() + "/out.pdf";
    QByteArray fdf = "%FDF-1.2\n1 0 obj\n<< /FDF << /Fields [\n";
    for (int i = 0; i <= 10000; ++i)
        fdf += QString("<< /T (f%1) /V (v) >> ").arg(i).toUtf8();
    fdf += "] >> >>\nendobj\ntrailer << /Root 1 0 R >>\n%%EOF\n";
    QVERIFY(writeBytes(data, fdf));
    FormManager fm;
    QVERIFY2(!fm.importFormData(pdf, data, out, nullptr),
             "an FDF over the field-count cap must be refused");
    QVERIFY2(!QFileInfo::exists(out),
             "a refused import must not write an output file (fail-closed)");
}

// Value-size cap: a single 1 MiB+ string must be refused, not imported.
void TestFillFormNoOp::overLargeValueIsRefused() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    const QString data = tmp.path() + "/big.fdf";
    const QString out = tmp.path() + "/out.pdf";
    QByteArray fdf = "%FDF-1.2\n1 0 obj\n<< /FDF << /Fields [\n";
    fdf += "<< /T (known) /V (";
    fdf += QByteArray(1024 * 1024 + 1, 'a');
    fdf += ") >>\n] >> >>\nendobj\ntrailer << /Root 1 0 R >>\n%%EOF\n";
    QVERIFY(writeBytes(data, fdf));
    FormManager fm;
    QVERIFY2(!fm.importFormData(pdf, data, out, nullptr),
             "a value over the per-string cap must be refused");
    QVERIFY2(!QFileInfo::exists(out),
             "a refused import must not write an output file (fail-closed)");
}

// Input-file cap: a >16 MiB data file is refused at open — the old code
// read it all and ran the regex across every byte.
void TestFillFormNoOp::oversizedInputFileIsRefused() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    const QString data = tmp.path() + "/huge.fdf";
    const QString out = tmp.path() + "/out.pdf";
    QByteArray big = "%FDF-1.2\n";
    big += QByteArray(16 * 1024 * 1024 + 1, 'x');  // no '<<' in padding: old regex scans it all, matches none
    QVERIFY(writeBytes(data, big));
    FormManager fm;
    QVERIFY2(!fm.importFormData(pdf, data, out, nullptr),
             "a data file over the input-size cap must be refused");
    QVERIFY2(!QFileInfo::exists(out),
             "a refused import must not write an output file (fail-closed)");
}

// Invalid UTF-8 bytes in the data file: the old QTextStream decode silently
// produced U+FFFD replacement characters (mojibake import). Must refuse.
void TestFillFormNoOp::nonUtf8BytesAreRefused() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    const QString data = tmp.path() + "/latin1.fdf";
    const QString out = tmp.path() + "/out.pdf";
    QByteArray raw = "%FDF-1.2\n1 0 obj\n<< /FDF << /Fields [\n";
    raw += "<< /T (known) /V (caf\xe9) >>\n";  // Latin-1 é, invalid UTF-8
    raw += "] >> >>\nendobj\ntrailer << /Root 1 0 R >>\n%%EOF\n";
    QVERIFY(writeBytes(data, raw));
    FormManager fm;
    QVERIFY2(!fm.importFormData(pdf, data, out, nullptr),
             "a data file that is not valid UTF-8 must be refused");
    QVERIFY2(!QFileInfo::exists(out),
             "a refused import must not write an output file (fail-closed)");
}

// Empty / whitespace-only / header-only inputs contain no form data: refuse
// honestly instead of rewriting the document with zero changes.
void TestFillFormNoOp::emptyAndWhitespaceOnlyInputIsRefused() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    FormManager fm;
    const QString out = tmp.path() + "/out.pdf";

    const QString empty = tmp.path() + "/empty.csv";
    QVERIFY(writeBytes(empty, QByteArray()));
    QVERIFY2(!fm.importFormData(pdf, empty, out, nullptr),
             "an empty data file must be refused");
    QVERIFY2(!QFileInfo::exists(out), "no output for a refused import");

    const QString blank = tmp.path() + "/blank.csv";
    QVERIFY(writeBytes(blank, " \n\t \n"));
    QVERIFY2(!fm.importFormData(pdf, blank, out, nullptr),
             "a whitespace-only data file must be refused");
    QVERIFY2(!QFileInfo::exists(out), "no output for a refused import");

    const QString headerOnly = tmp.path() + "/header.csv";
    QVERIFY(writeBytes(headerOnly, "FieldName,FieldValue\n"));
    QVERIFY2(!fm.importFormData(pdf, headerOnly, out, nullptr),
             "a header-only CSV holds no data and must be refused");
    QVERIFY2(!QFileInfo::exists(out), "no output for a refused import");
}

// "%FDF" header but no /Fields array at all: not a form-data file — refuse.
void TestFillFormNoOp::fdfWithoutFieldsArrayIsRefused() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    const QString data = tmp.path() + "/nofields.fdf";
    const QString out = tmp.path() + "/out.pdf";
    QVERIFY(writeBytes(data, "%FDF-1.2\ngarbage with no fields array\n"));
    FormManager fm;
    QVERIFY2(!fm.importFormData(pdf, data, out, nullptr),
             "an FDF without /Fields must be refused");
    QVERIFY2(!QFileInfo::exists(out),
             "a refused import must not write an output file (fail-closed)");
}

// THE half-import pin: rows 1-2 are valid, row 3 is malformed (one column).
// The old split() silently dropped the bad row and imported the rest — a
// partial import read as success. Must refuse the WHOLE file.
void TestFillFormNoOp::malformedCsvRecordIsRefusedNotHalfImported() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    const QString data = tmp.path() + "/broken.csv";
    const QString out = tmp.path() + "/out.pdf";
    QVERIFY(writeBytes(data,
        "FieldName,FieldValue\n"
        "\"known\",\"good\"\n"
        "\"nocomma\"\n"));
    FormManager fm;
    QStringList unsupported;
    QVERIFY2(!fm.importFormData(pdf, data, out, &unsupported),
             "a CSV with a malformed record must be refused entirely, not half-imported");
    QVERIFY2(!QFileInfo::exists(out),
             "a refused import must not write an output file (fail-closed)");
}

// ── Row 8 fidelity pins: hostile-but-legal values survive byte-exact ──────

// A quoted CSV cell containing commas, doubled quotes and a raw newline must
// land intact (RFC 4180). The old split('\n')/split("\",\"") parser
// truncated at the newline and emitted junk rows — silent corruption.
void TestFillFormNoOp::csvEmbeddedDelimitersAndNewlineRoundTrip() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    const QString data = tmp.path() + "/tricky.csv";
    const QString out = tmp.path() + "/out.pdf";
    const QString value = QStringLiteral("x,y \"q\" \nz");
    QVERIFY(writeBytes(data,
        QByteArray("FieldName,FieldValue\n\"known\",\"x,y \"\"q\"\" \nz\"\n")));
    FormManager fm;
    QStringList unsupported;
    QVERIFY2(fm.importFormData(pdf, data, out, &unsupported),
             "a legal quoted-CSV cell with embedded delimiters must import");
    QVERIFY(unsupported.isEmpty());
    QVERIFY(QFileInfo::exists(out));

    const QString exported = tmp.path() + "/back.csv";
    QVERIFY(fm.exportFormData(out, exported, "csv"));
    const QByteArray expected =
        "FieldName,FieldValue\n\"known\",\"x,y \"\"q\"\" \nz\"\n";
    QCOMPARE(readNormalized(exported), expected);
}

// An FDF value with escaped parens, backslash and \n escape must decode to
// the exact original. The old `(.*?)\)` regex stopped at the first ')',
// silently truncating the value.
void TestFillFormNoOp::fdfEmbeddedParensBackslashNewlineRoundTrip() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    const QString data = tmp.path() + "/parens.fdf";
    const QString out = tmp.path() + "/out.pdf";
    QVERIFY(writeBytes(data,
        "%FDF-1.2\n1 0 obj\n<< /FDF << /Fields [\n"
        "<< /T (known) /V (a\\(b\\)c\\\\d\\ne) >>\n"
        "] >> >>\nendobj\ntrailer << /Root 1 0 R >>\n%%EOF\n"));
    FormManager fm;
    QStringList unsupported;
    QVERIFY2(fm.importFormData(pdf, data, out, &unsupported),
             "a legal FDF value with escaped parens/backslash must import");
    QVERIFY(unsupported.isEmpty());
    QVERIFY(QFileInfo::exists(out));

    const QString exported = tmp.path() + "/back.csv";
    QVERIFY(fm.exportFormData(out, exported, "csv"));
    // The CSV export writer only doubles quotes — the decoded value keeps its
    // literal backslash and its raw newline inside the quoted cell.
    const QByteArray expected =
        "FieldName,FieldValue\n\"known\",\"a(b)c\\d\ne\"\n";
    QCOMPARE(readNormalized(exported), expected);
}

// Positive control: the app's own FDF export re-imports cleanly.
void TestFillFormNoOp::validFdfRoundTrip() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    FormManager fm;
    QVERIFY(fm.fillForm(pdf, QVariantMap{ { "known", QStringLiteral("hello") } }, pdf, false));

    const QString fdf = tmp.path() + "/export.fdf";
    QVERIFY(fm.exportFormData(pdf, fdf, "fdf"));
    const QString out = tmp.path() + "/out.pdf";
    QStringList unsupported;
    QVERIFY2(fm.importFormData(pdf, fdf, out, &unsupported),
             "the app's own FDF export must re-import");
    QVERIFY(unsupported.isEmpty());
    const QString back = tmp.path() + "/back.csv";
    QVERIFY(fm.exportFormData(out, back, "csv"));
    QCOMPARE(readNormalized(back), QByteArray("FieldName,FieldValue\n\"known\",\"hello\"\n"));
}

// Positive control: the app's own CSV export re-imports cleanly.
void TestFillFormNoOp::validCsvRoundTrip() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = createFormPdf(tmp.path(), "form.pdf");
    QVERIFY(!pdf.isEmpty());
    FormManager fm;
    QVERIFY(fm.fillForm(pdf, QVariantMap{ { "known", QStringLiteral("v1") } }, pdf, false));

    const QString csv = tmp.path() + "/export.csv";
    QVERIFY(fm.exportFormData(pdf, csv, "csv"));
    const QString out = tmp.path() + "/out.pdf";
    QStringList unsupported;
    QVERIFY2(fm.importFormData(pdf, csv, out, &unsupported),
             "the app's own CSV export must re-import");
    QVERIFY(unsupported.isEmpty());
    const QString back = tmp.path() + "/back.csv";
    QVERIFY(fm.exportFormData(out, back, "csv"));
    QCOMPARE(readNormalized(back), QByteArray("FieldName,FieldValue\n\"known\",\"v1\"\n"));
}

QTEST_MAIN(TestFillFormNoOp)
#include "TestFillFormNoOp.moc"
