// SPDX-License-Identifier: Apache-2.0
// Genuine-win regression test: ConversionManager's content-stream text
// extractor mis-read LIFO PdfVariantStack operands (Td/Tf/Tm), placing text
// at mirrored coordinates in PPTX/HTML/CSV exports. This pins correct
// operand order via the generated PPTX geometry.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <zip.h>
#include "engines/ConversionManager.h"

class TestTextExtractionCoords : public QObject {
    Q_OBJECT
private slots:
    void pptxOverlayUsesCorrectY();
};
void TestTextExtractionCoords::pptxOverlayUsesCorrectY() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString pdf = tmp.filePath("coords.pdf");
    {
        QFile f(pdf);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QByteArray out = "%PDF-1.4\n";
        QList<qint64> off;
        auto addObj = [&](const QByteArray& body) {
            off.append(out.size());
            out += QByteArray::number(off.size()) + " 0 obj\n" + body + "\nendobj\n";
        };
        addObj("<</Type/Catalog/Pages 2 0 R>>");
        addObj("<</Type/Pages/Kids[3 0 R]/Count 1>>");
        addObj("<</Type/Page/Parent 2 0 R/MediaBox[0 0 612 792]/Contents 4 0 R/Resources<</Font<</F1 5 0 R>>>>>>");
        const QByteArray stream = "BT /F1 12 Tf 100 700 Td (Hi) Tj ET\n";
        addObj("<</Length " + QByteArray::number(stream.size()) + ">>stream\n" + stream + "endstream");
        addObj("<</Type/Font/Subtype/Type1/BaseFont/Helvetica>>");
        const qint64 xrefPos = out.size();
        out += QString("xref\n0 %1\n").arg(off.size() + 1).toLatin1();
        out += "0000000000 65535 f \n";
        for (qint64 o : off)
            out += QString("%1 00000 n \n").arg(o, 10, 10, QChar('0')).toLatin1();
        out += QString("trailer<</Size %1/Root 1 0 R>>\nstartxref\n%2\n%%EOF\n")
                  .arg(off.size() + 1).arg(xrefPos).toLatin1();
        f.write(out);
    }

    ConversionManager mgr;
    const QString out = tmp.filePath("out.pptx");
    QVERIFY(mgr.convertTo(pdf, out, IConversionEngine::TargetFormat::PowerPoint));

    int err = 0;
    zip_t* za = zip_open(out.toUtf8().constData(), ZIP_RDONLY, &err);
    QVERIFY2(za, "pptx must open");
    zip_file_t* f = zip_fopen(za, "ppt/slides/slide1.xml", 0);
    QVERIFY2(f, "slide1.xml must exist");
    QByteArray xml;
    char buf[4096];
    zip_int64_t n;
    while ((n = zip_fread(f, buf, sizeof(buf))) > 0)
        xml.append(buf, static_cast<int>(n));
    zip_fclose(f);
    zip_close(za);

    QFile diag(QStringLiteral("tc_diag.txt"));
    if (diag.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&diag);
        ts << "size=" << xml.size() << " hasHi=" << xml.contains("Hi")
           << " head=" << QString::fromUtf8(xml.left(200)) << "\n";
    }

    // Correct Y flip: (792 - 700 - 12)pt * 12700 EMU/pt = 1016000.
    // The old LIFO bug read tx=700 as X and ty=100 as Y, producing a
    // different geometry entirely.
    QVERIFY2(xml.contains("y=\"1016000\""), qPrintable(
        QStringLiteral("expected overlay y=1016000; slide xml:\n%1").arg(QString::fromUtf8(xml))));
}
QTEST_MAIN(TestTextExtractionCoords)
#include "TestTextExtractionCoords.moc"
