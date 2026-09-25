// SPDX-License-Identifier: Apache-2.0
// SEP13 lead 11 — OCRMode::onRejectResults / onReOcrRegion lack the
// ReviewState guard that onAcceptResults has.
//
// PARITY-GLM-REVIEW-2026-09-13 leads (OCRMode.cpp ~617 / ~707):
//   onAcceptResults():  `if (m_reviewState != ReviewState::ReviewReady) return;`
//   onRejectResults():  NO guard — clears words/session/canvas and emits
//                       reviewRejected() from ANY state (Running, Saving, …).
//   onReOcrRegion():    NO guard — emits reOcrRegionRequested() unconditionally,
//                       so a second OCR run can be requested while one is
//                       in flight or while a save is being committed.
// Both are reachable outside the disabled-button path (context menu builds the
// actions in every state).
//
// The two lead probes assert the CORRECT contract (mirroring the accept guard)
// and are expected to FAIL on candidate 83be3c2. The accept control probe must
// PASS — it pins the guard the other two paths are missing.
#include <QtTest/QtTest>
#include <QSignalSpy>

#include "modes/OCRMode.h"
#include "engines/ocr/OcrPipeline.h"

using namespace gp;

namespace {

QList<MergedOcrWord> makeWords() {
    QList<MergedOcrWord> words;
    MergedOcrWord w;
    w.text = QStringLiteral("INVOICE");
    w.boundingBox = QRectF(10, 10, 80, 14);
    w.confidence = 95;
    w.sourceEngine = QStringLiteral("Tesseract");
    words.append(w);
    MergedOcrWord w2 = w;
    w2.text = QStringLiteral("PAID");
    w2.boundingBox = QRectF(10, 30, 60, 14);
    words.append(w2);
    return words;
}

} // namespace

class TestSep13LeadOcrGuards : public QObject {
    Q_OBJECT

private slots:
    // Control pin (must PASS): accept outside ReviewReady is ignored — this is
    // exactly the guard the reject/re-OCR paths are missing.
    void acceptOutsideReviewReadyIsIgnored() {
        OCRMode panel;   // Idle
        QSignalSpy acceptSpy(&panel, &OCRMode::reviewAccepted);
        panel.onAcceptResults();
        QCOMPARE(int(panel.reviewState()), int(OCRMode::ReviewState::Idle));
        QCOMPARE(acceptSpy.count(), 0);
    }

    // LEAD 11a CONFIRMATION (expected FAILURE on the candidate): reject while
    // a save is in flight (Saving) must be a no-op like accept — instead it
    // wipes the review words and emits reviewRejected() mid-save.
    void rejectDuringSavingMustBeIgnored() {
        OCRMode panel;
        panel.setOcrResults(makeWords());
        QCOMPARE(int(panel.reviewState()), int(OCRMode::ReviewState::ReviewReady));

        panel.onAcceptResults();   // R07: moves to Saving before the save dialog
        QCOMPARE(int(panel.reviewState()), int(OCRMode::ReviewState::Saving));

        QSignalSpy rejectSpy(&panel, &OCRMode::reviewRejected);
        panel.onRejectResults();

        // CORRECT contract: reject outside ReviewReady is a no-op (state stays
        // Saving; the in-flight save decides the terminal transition).
        QVERIFY2(panel.reviewState() == OCRMode::ReviewState::Saving,
                 QStringLiteral("SEP13 lead 11 CONFIRMED: onRejectResults ran during Saving — "
                 "state jumped to %1 instead of staying Saving")
                     .arg(int(panel.reviewState())).toUtf8().constData());
        QVERIFY2(rejectSpy.count() == 0,
                 "SEP13 lead 11 CONFIRMED: reviewRejected emitted while a save was "
                 "in flight (host drops pending save state)");
    }

    // LEAD 11b CONFIRMATION (expected FAILURE on the candidate): re-OCR with
    // nothing reviewable/in-flight must be ignored — instead it emits
    // reOcrRegionRequested unconditionally (re-entrancy: a second OCR run can
    // be requested while another is Running/Saving).
    void reOcrOutsideReviewReadyMustBeIgnored() {
        OCRMode panel;   // Idle — nothing has been recognized yet
        QSignalSpy reOcrSpy(&panel, &OCRMode::reOcrRegionRequested);
        // onReOcrRegion is a PRIVATE slot (the image-pane context menu is its
        // only UI entry) — invoke it through the moc exactly as the action does.
        QVERIFY(QMetaObject::invokeMethod(&panel, "onReOcrRegion", Qt::DirectConnection));
        QVERIFY2(reOcrSpy.count() == 0,
                 "SEP13 lead 11 CONFIRMED: reOcrRegionRequested emitted from Idle — "
                 "the ReviewState guard onAcceptResults has is missing here");
    }
};

#include "TestSep13LeadOcrGuards.moc"
QTEST_MAIN(TestSep13LeadOcrGuards)
