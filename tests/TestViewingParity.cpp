// SPDX-License-Identifier: Apache-2.0
#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <podofo/podofo.h>

#include "ui/PdfViewerWidget.h"

class TestViewingParity : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void testRotationRotatesActualBitmap();

private:
    QTemporaryDir m_tempDir;
    QString m_pdfPath;
};

void TestViewingParity::initTestCase()
{
    // TEMP DIAGNOSTIC: mirror qDebug/qWarning to a file, this environment
    // doesn't surface QTest console output through the available capture
    // tools. Remove once the failure below is understood.
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& msg) {
        QFile f(QStringLiteral("C:/Users/User/AppData/Local/Temp/viewing_debug.log"));
        if (f.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream ts(&f);
            ts << msg << "\n";
        }
    });
    qDebug() << "[init] start, tempDir valid=" << m_tempDir.isValid();
    QVERIFY(m_tempDir.isValid());
    m_pdfPath = m_tempDir.filePath(QStringLiteral("viewing_parity_fixture.pdf"));
    qDebug() << "[init] pdfPath=" << m_pdfPath;

    try {
        PoDoFo::PdfMemDocument doc;
        auto &page0 = doc.GetPages().CreatePage(PoDoFo::Rect(0, 0, 200, 300));
        doc.GetPages().CreatePage(PoDoFo::Rect(0, 0, 200, 300));

        {
            PoDoFo::PdfPainter painter;
            painter.SetCanvas(page0);
            painter.GraphicsState.SetNonStrokingColor(PoDoFo::PdfColor(1.0, 0.0, 0.0));
            painter.DrawRectangle(0, 250, 50, 50, PoDoFo::PdfPathDrawMode::Fill);
            painter.FinishDrawing();
        }

        doc.Save(m_pdfPath.toStdString());
        qDebug() << "[init] saved OK";
    } catch (const std::exception& e) {
        qDebug() << "[init] EXCEPTION:" << e.what();
        QFAIL("PDF fixture creation threw");
    }
}

void TestViewingParity::testRotationRotatesActualBitmap()
{
    qDebug() << "[test] constructing viewer";
    PdfViewerWidget viewer;
    qDebug() << "[test] loading doc";
    const bool loaded = viewer.loadDocument(m_pdfPath);
    qDebug() << "[test] loaded=" << loaded;
    QVERIFY(loaded);
    qDebug() << "[test] pageCount=" << viewer.pageCount();
    QCOMPARE(viewer.pageCount(), 2);

    qDebug() << "[test] rendering upright";
    const QImage upright = viewer.renderPage(0, 1.0);
    qDebug() << "[upright] isNull=" << upright.isNull() << "size=" << upright.size();
    QVERIFY(!upright.isNull());
    QCOMPARE(upright.width(), 200);
    QCOMPARE(upright.height(), 300);

    auto isReddish = [](QRgb px) {
        return qRed(px) > 150 && qGreen(px) < 100 && qBlue(px) < 100;
    };
    auto isWhitish = [](QRgb px) {
        return qRed(px) > 200 && qGreen(px) > 200 && qBlue(px) > 200;
    };

    qDebug() << "[upright] px(10,10)=" << QString::number(upright.pixel(10,10), 16)
             << "px(190,290)=" << QString::number(upright.pixel(190,290), 16)
             << "px(10,290)=" << QString::number(upright.pixel(10,290), 16)
             << "px(190,10)=" << QString::number(upright.pixel(190,10), 16);
    QVERIFY2(isReddish(upright.pixel(10, 10)), "expected red top-left in unrotated render");
    QVERIFY2(isWhitish(upright.pixel(190, 290)), "expected white bottom-right in unrotated render");

    viewer.rotateClockwise();
    const QImage rot90 = viewer.renderPage(0, 1.0);
    qDebug() << "[rot90] isNull=" << rot90.isNull() << "size=" << rot90.size();
    QVERIFY(!rot90.isNull());
    QCOMPARE(rot90.width(), 300);
    QCOMPARE(rot90.height(), 200);

    viewer.rotateClockwise();
    const QImage rot180 = viewer.renderPage(0, 1.0);
    qDebug() << "[rot180] isNull=" << rot180.isNull() << "size=" << rot180.size()
             << "px(10,10)=" << (rot180.isNull() ? QString() : QString::number(rot180.pixel(10,10), 16))
             << "px(190,290)=" << (rot180.isNull() ? QString() : QString::number(rot180.pixel(190,290), 16));
    QVERIFY(!rot180.isNull());
    QCOMPARE(rot180.width(), 200);
    QCOMPARE(rot180.height(), 300);
    QVERIFY2(isWhitish(rot180.pixel(10, 10)), "180-degree rotation must move red away from top-left");
    QVERIFY2(isReddish(rot180.pixel(190, 290)), "180-degree rotation must move red to bottom-right");

    viewer.rotateClockwise();
    const QImage rot270 = viewer.renderPage(0, 1.0);
    qDebug() << "[rot270] isNull=" << rot270.isNull() << "size=" << rot270.size();
    QVERIFY(!rot270.isNull());
    QCOMPARE(rot270.width(), 300);
    QCOMPARE(rot270.height(), 200);

    viewer.rotateClockwise();
    const QImage rot360 = viewer.renderPage(0, 1.0);
    qDebug() << "[rot360] isNull=" << rot360.isNull() << "size=" << rot360.size();
    QVERIFY(!rot360.isNull());
    QCOMPARE(rot360.width(), 200);
    QCOMPARE(rot360.height(), 300);
    QVERIFY(isReddish(rot360.pixel(10, 10)));
    QVERIFY(isWhitish(rot360.pixel(190, 290)));
}

QTEST_MAIN(TestViewingParity)
#include "TestViewingParity.moc"
