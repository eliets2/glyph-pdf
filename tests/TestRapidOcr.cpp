// TestRapidOcr — real PP-OCRv5 inference verification (M5-P1 D6).
//
// Proves RapidOcrEngine is no longer a Mock: loads the genuine PP-OCRv5 mobile
// ONNX models, runs the full det → cls → perspective → SVTR → CTC pipeline on a
// rendered known-text image, and asserts the decoded text is real.

#include <QtTest>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QString>

#include "engines/ocr/RapidOcrEngine.h"
#include "core/OcrTypes.h"

#ifndef GLYPH_OCR_MODELS_DIR
#define GLYPH_OCR_MODELS_DIR ""
#endif

namespace {
QImage renderText(const QString &text, int w = 520, int h = 140)
{
    QImage img(w, h, QImage::Format_RGB888);
    img.fill(Qt::white);
    QPainter p(&img);
    QFont f(QStringLiteral("Arial"), 44);
    f.setBold(true);
    p.setFont(f);
    p.setPen(Qt::black);
    p.drawText(img.rect(), Qt::AlignCenter, text);
    p.end();
    return img;
}
} // namespace

class TestRapidOcr : public QObject
{
    Q_OBJECT
private slots:
    // Contract: the engine must declare itself real, not a Mock.
    void testNotMock()
    {
        RapidOcrEngine eng;
        QVERIFY2(!eng.isMockImplementation(),
                 "RapidOcrEngine must report isMockImplementation()==false");
    }

    // The genuine PP-OCRv5 models must load under the bundled ORT 1.17.3.
    void testInitializeLoadsModels()
    {
        // models/ is not in the repository — CI runners legitimately lack it.
        if (!QFile::exists(QStringLiteral(GLYPH_OCR_MODELS_DIR "/PP-OCRv5_mobile_det_infer.onnx")))
            QSKIP("PP-OCRv5 models not present in this environment");
        RapidOcrEngine eng;
        const bool ok = eng.initialize(QStringLiteral("eng"),
                                       QStringLiteral(GLYPH_OCR_MODELS_DIR));
        QVERIFY2(ok, "RapidOcrEngine.initialize() should load the PP-OCRv5 models + dictionary");
    }

    // The real bar: recognize printed text from a rendered image.
    void testRecognizePrintedEnglish()
    {
        RapidOcrEngine eng;
        if (!eng.initialize(QStringLiteral("eng"), QStringLiteral(GLYPH_OCR_MODELS_DIR)))
            QSKIP("PP-OCRv5 models not available in this environment");

        const QImage img = renderText(QStringLiteral("Hello World"));
        const QString got = eng.getRawText(img).toLower().simplified();
        qDebug() << "RapidOCR recognized:" << got;

        QVERIFY2(!got.isEmpty(), "RapidOCR returned empty text on a clear printed image");
        QVERIFY2(got.contains(QStringLiteral("hello")) || got.contains(QStringLiteral("world")),
                 qPrintable(QStringLiteral("Expected 'hello'/'world', got: '%1'").arg(got)));
    }

    // Digits — a second independent recognition case.
    void testRecognizeDigits()
    {
        RapidOcrEngine eng;
        if (!eng.initialize(QStringLiteral("eng"), QStringLiteral(GLYPH_OCR_MODELS_DIR)))
            QSKIP("PP-OCRv5 models not available in this environment");

        const QImage img = renderText(QStringLiteral("2026"));
        const QString got = eng.getRawText(img).simplified();
        qDebug() << "RapidOCR digits:" << got;
        QVERIFY2(got.contains(QStringLiteral("2026")),
                 qPrintable(QStringLiteral("Expected '2026', got: '%1'").arg(got)));
    }
};

QTEST_MAIN(TestRapidOcr)
#include "TestRapidOcr.moc"
