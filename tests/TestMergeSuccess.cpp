// SPDX-License-Identifier: Apache-2.0
// Audit 9.9 P0 regression test: PdfViewerWidget::mergeDocuments must return
// false when the engine fails (bad inputs) and true with a real output file on
// success, so the UI can surface real merge failure instead of always
// reporting "Successfully merged".
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "ui/PdfViewerWidget.h"
class TestMergeSuccess : public QObject {
    Q_OBJECT
private slots:
    void mergeReturnsFalseOnBadInputs();
    void mergeReturnsTrueAndWritesOutput();
private:
    static QString createMinimalPdf(const QString& dir, const QString& name);
};
QString TestMergeSuccess::createMinimalPdf(const QString& dir, const QString& name) {
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
    f.close();
    return path;
}
void TestMergeSuccess::mergeReturnsFalseOnBadInputs() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString out = tmp.path() + "/merged_bad.pdf";
    QStringList badInputs;
    badInputs << tmp.path() + "/missing1.pdf" << tmp.path() + "/missing2.pdf";
    const bool ok = PdfViewerWidget::mergeDocuments(badInputs, out);
    QVERIFY(!ok);
    QVERIFY(!QFile::exists(out));
}
void TestMergeSuccess::mergeReturnsTrueAndWritesOutput() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString a = createMinimalPdf(tmp.path(), "a.pdf");
    const QString b = createMinimalPdf(tmp.path(), "b.pdf");
    QVERIFY(!a.isEmpty());
    QVERIFY(!b.isEmpty());
    const QString out = tmp.path() + "/merged_ok.pdf";
    const bool ok = PdfViewerWidget::mergeDocuments({a, b}, out);
    QVERIFY(ok);
    QVERIFY(QFile::exists(out));
}
QTEST_MAIN(TestMergeSuccess)
#include "TestMergeSuccess.moc"
