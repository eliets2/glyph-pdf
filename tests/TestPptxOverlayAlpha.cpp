// SPDX-License-Identifier: Apache-2.0
// Audit 9.5 P0 regression test: PPTX overlay text must carry the intended
// ~1% alpha (<a:alpha val="1000"/>) so it is selectable but visually
// invisible over the slide image — previously it rendered solid black.
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QPdfWriter>
#include <QPainter>
#include <QPageSize>
#include <zip.h>
#include "engines/ConversionManager.h"

class TestPptxOverlayAlpha : public QObject {
    Q_OBJECT
private slots:
    void slideXmlCarriesAlpha();
private:
    static QString createMinimalPdf(const QString& dir, const QString& name);
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
QTEST_MAIN(TestPptxOverlayAlpha)
#include "TestPptxOverlayAlpha.moc"
