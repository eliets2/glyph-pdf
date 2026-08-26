// SPDX-License-Identifier: Apache-2.0
// Audit 9.4 P0 regression test: the interactive OCR Accept flow must be able
// to assemble the per-page payload for exportMrcPdfA (the searchable-layer
// writer) — previously Accept was a silent no-op.
#include <QtTest/QtTest>
#include "shell/controllers/EditController.h"
#include "engines/ocr/OcrPipeline.h"

class TestOcrAcceptSeam : public QObject {
    Q_OBJECT
private slots:
    void buildsPayloadFromWords();
};
void TestOcrAcceptSeam::buildsPayloadFromWords() {
    MergedOcrWord w;
    w.text = QStringLiteral("invoice");
    w.boundingBox = QRectF(10, 10, 50, 12);
    w.confidence = 92;
    w.sourceEngine = QStringLiteral("ROVER");

    PageOcrResult r = gp::EditController::buildPageOcrResult(2, {w});
    QCOMPARE(r.pageIndex, 2);
    QCOMPARE(r.words.size(), 1);
    QCOMPARE(r.words.first().text, QStringLiteral("invoice"));
    QVERIFY(r.success);

    // Empty words → success=false (nothing searchable to write).
    PageOcrResult empty = gp::EditController::buildPageOcrResult(0, {});
    QVERIFY(!empty.success);
}
QTEST_MAIN(TestOcrAcceptSeam)
#include "TestOcrAcceptSeam.moc"
