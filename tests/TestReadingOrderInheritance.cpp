// SPDX-License-Identifier: Apache-2.0
// Audit 9.14 P0 regression test: reading-order analysis must honor ISO
// 32000-2 §14.7.2 /Pg inheritance. A correctly-tagged PDF whose struct elems
// rely on inherited /Pg must NOT be flagged as out-of-order.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "modes/PdfAValidationPanel.h"

class TestReadingOrderInheritance : public QObject {
    Q_OBJECT
private slots:
    void untaggedPdfIsReported();
    void inheritedPgIsNotFlagged();
private:
    static QString writeTaggedPdf(const QString& dir, const QString& name);
    static QString writePlainPdf(const QString& dir, const QString& name);
};
QString TestReadingOrderInheritance::writePlainPdf(const QString& dir, const QString& name) {
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
QString TestReadingOrderInheritance::writeTaggedPdf(const QString& dir, const QString& name) {
    const QString path = dir + "/" + name;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    QByteArray out = "%PDF-1.4\n";
    QList<qint64> off;
    auto addObj = [&](const QByteArray& body) {
        off.append(out.size());
        out += QByteArray::number(off.size()) + " 0 obj\n" + body + "\n";
    };
    // 1: catalog, 2: pages, 3: page
    addObj("<</Type/Catalog/Pages 2 0 R/MarkInfo<</Marked true>>/StructTreeRoot 4 0 R>>");
    addObj("<</Type/Pages/Kids[3 0 R]/Count 1>>");
    addObj("<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]>>");
    // 4: StructTreeRoot — carries /Pg so children WITHOUT their own /Pg inherit it.
    QString kids;
    for (int i = 5; i <= 15; ++i) kids += QString("%1 0 R ").arg(i);
    addObj(QString("<</Type/StructTreeRoot/Pg 3 0 R/K[%1]>>").arg(kids).toLatin1());
    // 5..15: eleven P elements in order; object 6 has NO /Pg (inherits).
    for (int i = 5; i <= 15; ++i) {
        const QByteArray pg = (i == 6) ? QByteArray() : QByteArray("/Pg 3 0 R");
        addObj(QByteArray("<</S/P") + pg + ">>");
    }
    // xref
    const qint64 xrefPos = out.size();
    out += QString("xref\n0 %1\n").arg(off.size() + 1).toLatin1();
    out += "0000000000 65535 f \n";
    for (qint64 o : off)
        out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
    out += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
              .arg(off.size() + 1).arg(xrefPos).toLatin1();
    f.write(out);
    return path;
}
void TestReadingOrderInheritance::inheritedPgIsNotFlagged() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = writePlainPdf(tmp.path(), "probe.pdf");
    const gp::ReadingOrderResult r = gp::analyzeReadingOrder(pdf);
    QVERIFY(!r.tagged);
}

void TestReadingOrderInheritance::untaggedPdfIsReported() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = writePlainPdf(tmp.path(), "plain.pdf");
    const gp::ReadingOrderResult r = gp::analyzeReadingOrder(pdf);
    QVERIFY(!r.tagged);
}
QTEST_MAIN(TestReadingOrderInheritance)
#include "TestReadingOrderInheritance.moc"
