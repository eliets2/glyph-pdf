// SPDX-License-Identifier: Apache-2.0
// Audit 9.6 P0 regression test: fillForm must REPORT requested values that it
// could not apply (unknown field names, Radio/PushButton targets) through the
// unsupportedFields out-parameter instead of silently dropping them.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "engines/FormManager.h"

class TestFillFormNoOp : public QObject {
    Q_OBJECT
private slots:
    void unknownFieldNameIsReported();
private:
    static QString createTestPdf(const QString& dir, const QString& name);
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
QTEST_MAIN(TestFillFormNoOp)
#include "TestFillFormNoOp.moc"
