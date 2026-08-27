// SPDX-License-Identifier: Apache-2.0
// §9.12 P0 regression test: batch OCR must surface OcrPipeline's confidence
// data — low-confidence words must be flagged for review in the batch log /
// error log instead of a bare pass/fail with zero visibility into which
// outputs need review. The flagging rule lives in
// BatchMode::lowConfidenceNote(), the pure seam the worker consumes.
#include <QtTest/QtTest>
#include "modes/BatchMode.h"
#include "engines/ocr/OcrPipeline.h"

using gp::BatchMode;
using ::PageOcrResult;
using ::MergedOcrWord;

static MergedOcrWord word(const QString& text, int confidence) {
    MergedOcrWord w;
    w.text = text;
    w.confidence = confidence;
    return w;
}

class TestBatchOcrConfidence : public QObject {
    Q_OBJECT
private slots:
    void noNoteWhenAllConfident();
    void noteCountsLowConfidenceWords();
    void noteListsAffectedPages();
    void noteIsOneBased();
    void emptyResultsProduceNoNote();
};

void TestBatchOcrConfidence::noNoteWhenAllConfident() {
    PageOcrResult pr;
    pr.pageIndex = 0;
    pr.words = { word(QStringLiteral("invoice"), 95), word(QStringLiteral("total"), 88) };
    QVERIFY(BatchMode::lowConfidenceNote({pr}).isEmpty());
}

void TestBatchOcrConfidence::noteCountsLowConfidenceWords() {
    PageOcrResult pr;
    pr.pageIndex = 0;
    pr.words = { word(QStringLiteral("invoi"), 42), word(QStringLiteral("ce"), 55),
                 word(QStringLiteral("total"), 90) };
    const QString note = BatchMode::lowConfidenceNote({pr});
    QVERIFY(!note.isEmpty());
    QVERIFY2(note.contains(QStringLiteral("2")), qPrintable(note));
    QVERIFY(note.contains(QStringLiteral("review"), Qt::CaseInsensitive));
}

void TestBatchOcrConfidence::noteListsAffectedPages() {
    PageOcrResult p0; p0.pageIndex = 0;
    p0.words = { word(QStringLiteral("blur"), 30) };
    PageOcrResult p2; p2.pageIndex = 2;
    p2.words = { word(QStringLiteral("smudge"), 10), word(QStringLiteral("x"), 20) };
    PageOcrResult p1; p1.pageIndex = 1;   // clean page — must not appear
    p1.words = { word(QStringLiteral("fine"), 99) };
    const QString note = BatchMode::lowConfidenceNote({p0, p1, p2});
    QVERIFY(note.contains(QStringLiteral("1")));
    QVERIFY(note.contains(QStringLiteral("3")));
    // Page 2 (index 1) is clean; the note must not claim it needs review.
    QVERIFY(!note.contains(QStringLiteral("2")));
}

void TestBatchOcrConfidence::noteIsOneBased() {
    PageOcrResult pr;
    pr.pageIndex = 4;   // 0-based index 4 → user-facing page 5
    pr.words = { word(QStringLiteral("low"), 15) };
    const QString note = BatchMode::lowConfidenceNote({pr});
    QVERIFY(note.contains(QStringLiteral("5")));
}

void TestBatchOcrConfidence::emptyResultsProduceNoNote() {
    QVERIFY(BatchMode::lowConfidenceNote({}).isEmpty());
    PageOcrResult pr;
    pr.pageIndex = 0;   // page with no recognised words at all
    QVERIFY(BatchMode::lowConfidenceNote({pr}).isEmpty());
}

QTEST_MAIN(TestBatchOcrConfidence)
#include "TestBatchOcrConfidence.moc"
