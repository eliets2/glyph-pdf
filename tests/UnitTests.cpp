#include <QtTest>
#include "engines/OcrEngine.h"
#include "engines/FormManager.h"

class UnitTests : public QObject
{
    Q_OBJECT

private slots:
    void testOcrEngineReturnsEmptyBeforeInitialize()
    {
        OcrEngine engine;
        QImage image(16, 16, QImage::Format_Grayscale8);
        image.fill(Qt::white);

        QVERIFY(engine.processImage(image).isEmpty());
        QVERIFY(engine.getRawText(image).isEmpty());
    }

    void testFormManagerStubs()
    {
        FormManager manager;
        // In the absence of a real PDF with forms in the test suite, we just verify it doesn't crash on invalid paths.
        QVERIFY(manager.extractFormFields("invalid_path.pdf") == false);
        QVERIFY(manager.hasXfaForms("invalid_path.pdf") == false);
    }
};

QTEST_GUILESS_MAIN(UnitTests)
#include "UnitTests.moc"
